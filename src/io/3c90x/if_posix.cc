/*
 *	PearPC
 *	if_posix.cc
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

#if defined(WIN32) || defined(__WIN32__) || !defined(HAVE_ETHERTAP)
#elif defined(__APPLE__) && defined(__MACH__)
#else

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

#include "if_posix.h"
#include "tools/snprintf.h"

#define perrorm(s)	{ printf("tun: %s: %d (%s)\n", s, errno, strerror(errno)); }
#define printm(s...)	{ printf(s); }

enum{ f_tap=1, f_sheep=2, f_tun=4, f_driver_id_mask=7 };

/************************************************************************/
/*	Misc								*/
/************************************************************************/

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
}

static void common_iface_open(enet_iface_t *is, char *drv_str, char *intf_name, int fd)
{
	ht_snprintf(is->drv_name, sizeof(is->drv_name), "%s-<%s>", drv_str, intf_name);
	strncpy(is->iface_name, intf_name, sizeof(is->iface_name)-1);
	is->iface_name[sizeof(is->iface_name)-1] = 0;
	is->fd = fd;
}

static void generic_close(enet_iface_t *is)
{
	close(is->fd);
	is->fd = -1;
}


/************************************************************************/
/*	TAP Packet Driver						*/
/************************************************************************/

#define TAP_PACKET_PAD			2		/* used by read & writes */

static int tap_open(enet_iface_t *is, char *intf_name, int *sigio_capable, byte mac[6])
{
	struct sockaddr_nl nladdr;
	int fd, tapnum = 0;
	char buf[16];

	if (intf_name) {
		if (sscanf(intf_name, "tap%d", &tapnum) == 1) {
			if (tapnum < 0 || tapnum > 15) {
				printf("Invalid tap device %s. Using tap0 instead\n", intf_name );
				intf_name = NULL;
				tapnum = 0;
			}
		} else {
			printm("Bad tapdevice interface '%s'\n", intf_name );
			printm("Using default tap device (tap0)\n");
			intf_name = NULL;
		}
	}
	if (!intf_name) {
		sprintf(buf, "tap0");
		intf_name = buf;
	}

	/* verify that the device is up and running */
	if (check_netdev(intf_name)) return 1;

	if ((fd = socket( PF_NETLINK, SOCK_RAW, NETLINK_TAPBASE+tapnum )) < 0) {
		perrorm("socket");
		printm("Does the kernel lack netlink support (CONFIG_NETLINK)?\n");
		return 1;
	}
	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_groups = ~0;
	nladdr.nl_pid = getpid();
	if (bind( fd, (struct sockaddr*)&nladdr, sizeof(nladdr) ) < 0) {
		perrorm("bind");
		close(fd);
		return 1;
	}

	memcpy(is->eth_addr, mac, sizeof mac);

	is->packet_pad = TAP_PACKET_PAD;
	*sigio_capable = 1;

	common_iface_open(is, "tap", intf_name, fd);
	return 0;
}

packet_driver_t tap_pd = {
	name:			"tap",
	packet_driver_id: 	f_tap,
	open: 			tap_open,
	close:			generic_close,
};

/************************************************************************/
/*	TUN/TAP Packet Driver						*/
/************************************************************************/

#define HAS_TUN
#ifdef HAS_TUN

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

static int tun_open(enet_iface_t *is, char *intf_name, int *sigio_capable, byte mac[6])
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

static void tun_close(enet_iface_t *is)
{
//	script_exec( get_filename_res(TUNSETUP_RES), is->iface_name, "down" );
	exec_ifconfig(is->iface_name, "down");

	close(is->fd);
	is->fd = -1;
}

packet_driver_t tun_pd = {
	name:			"tun",
	packet_driver_id: 	f_tun,
	open: 			tun_open,
	close:			tun_close,
};

#endif /* HAS_TUN */
#endif
