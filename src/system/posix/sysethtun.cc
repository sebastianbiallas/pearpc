/*
 *	PearPC
 *	sysethtun.cc
 *
 *	POSIX-specific ethernet-tunnel access
 *
 *	Copyright (C) 2003 Stefan Weyergraf (stefan@weyergraf.de)
 *
 *	code taken from Mac-on-Linux 0.9.68:
 *	Copyright (C) 1999-2002 Samuel Rydh (samuel@ibrium.se)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_ETHERTAP) && !(defined(__APPLE__) && defined(__MACH__))

#include <errno.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <linux/netlink.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cerrno>

#include "system/sysethtun.h"
#include "tools/snprintf.h"

#define perrorm(s)	{ printf("tun: %s: %d (%s)\n", s, errno, strerror(errno)); }
#define printm(s...)	{ printf(s); }

/************************************************************************/
/*	Misc								*/
/************************************************************************/
/*
static int check_netdev(char *ifname)
{
	struct ifreq	ifr;
	int 		fd;
	int		ret=-1;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFFLAGS, &ifr ) < 0) {
		perrorm("SIOCGIFFLAGS");
		goto out;
	}
	if (!(ifr.ifr_flags & IFF_RUNNING)) {
		printm("---> The network interface '%s' is not configured!\n", ifname);
		goto out;
	}
	if ((ifr.ifr_flags & IFF_NOARP )) {
		printm("WARNING: Turning on ARP for device '%s'.\n", ifname);
		ifr.ifr_flags &= ~IFF_NOARP;
		if (ioctl( fd, SIOCSIFFLAGS, &ifr ) < 0) {
			perrorm("SIOCSIFFLAGS");
			goto out;
		}
	}
	ret = 0;
 out:
	close(fd);
	return ret;
}*/

static void common_iface_open(enet_iface_t *is, char *drv_str, char *intf_name, int fd)
{
	ht_snprintf(is->drv_name, sizeof(is->drv_name), "%s-<%s>", drv_str, intf_name);
	strncpy(is->iface_name, intf_name, sizeof(is->iface_name)-1);
	is->iface_name[sizeof(is->iface_name)-1] = 0;
	is->fd = fd;
}
/*
static void generic_close(enet_iface_t *is)
{
	close(is->fd);
	is->fd = -1;
}
*/
/************************************************************************/
/*	TUN/TAP Packet Driver						*/
/************************************************************************/

int exec_ifconfig(const char *intf_name, const char *arg)
{
	int pid = fork();
	if (pid == 0) {
		char *progname;
		if (strcmp(arg, "up") == 0) {
			progname = "scripts/ifppc_up.setuid";
		} else {
			progname = "scripts/ifppc_down.setuid";
		}
		printf("executing '%s' ...\n"
"********************************************************************************\n", progname);
		execl(progname, progname, 0);
		printf("couldn't exec '%s': %s\n", progname, strerror(errno));
		exit(1);
	} else if (pid != -1) {
		int status;
		waitpid(pid, &status, 0);
		if (!WIFEXITED(status)) {
			printf("program terminated abnormally...\n");
			return 1;
		}
		if (WEXITSTATUS(status)) {
			printf("program terminated with exit code %d\n", WEXITSTATUS(status));
			return 1;
		}
		return 0;
	}
	return 1;
}

static int tun_open(enet_iface_t *is, char *intf_name, int *sigio_capable, const byte *mac)
{
	struct ifreq ifr;
	int fd;

	/* allocate tun/tap device */ 
	if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
		perrorm("Failed to open /dev/net/tun");
		return 1;
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	strncpy(ifr.ifr_name, intf_name, IFNAMSIZ);
	if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
		perrorm("TUNSETIFF");
		goto out;
	}

	/* don't checksum */
	ioctl(fd, TUNSETNOCSUM, 1);

	/* Configure device */
//	script_exec(get_filename_res(TUNSETUP_RES), intf_name, "up");
	if (exec_ifconfig(intf_name, "up")) {
		printm("exec ifconfig\n");
		goto out;
	}

	/* set HW address */
	memcpy(is->eth_addr, mac, 6);

	/* finish... */
	is->packet_pad = 0;
	*sigio_capable = 1;
	common_iface_open(is, "tun", intf_name, fd);
	return 0;

out:
	close(fd);
	return 1;
}

static int tun_wait_receive(enet_iface_t *is)
{
	fd_set rfds;
	fd_set zerofds;
	FD_ZERO(&rfds);
	FD_ZERO(&zerofds);
	FD_SET(is->fd, &rfds);

	int e = select(is->fd+1, &rfds, &zerofds, &zerofds, NULL);
	if (e < 0) return errno;
	return 0;
}

static void tun_close(enet_iface_t *is)
{
//	script_exec( get_filename_res(TUNSETUP_RES), is->iface_name, "down" );
	exec_ifconfig(is->iface_name, "down");

	close(is->fd);
	is->fd = -1;
}

packet_driver_t g_sys_ethtun_pd = {
	name:			"tun",
	open: 			tun_open,
	close:			tun_close,
	wait_receive:		tun_wait_receive
};

#else

static int pdnull_open(enet_iface_t *is, char *intf_name, int *sigio_capable, const byte *mac)
{
	return ENOSYS;
}

packet_driver_t g_sys_ethtun_pd = {
	name:			"null",
	open: 			pdnull_open,
};

#endif
