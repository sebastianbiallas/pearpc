/*
 *	PearPC
 *	sysethtun.cc
 *
 *	POSIX/UN*X-specific ethernet-tunnel access
 *
 *	Copyright (C) 2003 Stefan Weyergraf (stefan@weyergraf.de)
 *
 *	code taken from Mac-on-Linux 0.9.68:
 *	Copyright (C) 1999-2002 Samuel Rydh (samuel@ibrium.se)
 *
 *	darwin code taken from tinc 1.0.2:
 *	Copyright (C) 2001-2003 Ivo Timmermans <ivo@o2w.nl>,
 *	              2001-2003 Guus Sliepen <guus@sliepen.eu.org>
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <asm/types.h>
#include <sys/wait.h>

#include "system/sysethtun.h"
#include "tools/snprintf.h"


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tools/except.h"
#include "system/sysethtun.h"

/**
 *	Unix ethernet tunnel devices should work using a file descriptor
 */
class UnixEthTunDevice: public EthTunDevice {
protected:
	int	mFD;
public:

UnixEthTunDevice()
{
	// super-class constructor MUST set mFD!!!
	// this is yet another case where C++ constructors suck
	// (ohh lovely Borland Pascal, where are you?)
}

virtual	uint recvPacket(void *buf, uint size)
{
	ssize_t e = ::read(mFD, buf, size);
	if (e < 0) return 0;
	return e;
}

virtual	int waitRecvPacket()
{
	fd_set rfds;
	fd_set zerofds;
	FD_ZERO(&rfds);
	FD_ZERO(&zerofds);
	FD_SET(mFD, &rfds);

	int e = select(mFD+1, &rfds, &zerofds, &zerofds, NULL);
	if (e < 0) return errno;
	return 0;
}

virtual	uint sendPacket(void *buf, uint size)
{
	ssize_t e = ::write(mFD, buf, size);
	if (e < 0) return 0;
	return e;
}

}; // end of UnixEthTunDevice

// FIXME: How shall we configure networking??? This thing can only be a temporary solution
static int execIFConfigScript(const char *arg)
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

#if defined(HAVE_LINUX_TUN)
/*
 *	This is how it's done in Linux
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

class LinuxEthTunDevice: public UnixEthTunDevice {
public:
LinuxEthTunDevice(const char *netif_name)
: UnixEthTunDevice()
{
	/* allocate tun device */ 
	if ((mFD = ::open("/dev/net/tun", O_RDWR)) < 0) {
		throw new MsgException("Failed to open /dev/net/tun");
	}

	struct ifreq ifr;
	::memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	if (::strlen(netif_name)+1 > IFNAMSIZ) {
		throw new MsgfException("Interface name too long (%d > %d bytes)"
			" in '%s'\n", ::strlen(netif_name)+1, IFNAMSIZ, netif_name);
	}

	::strncpy(ifr.ifr_name, netif_name, IFNAMSIZ);
	if (::ioctl(mFD, TUNSETIFF, &ifr) < 0) {
		::close(mFD);
		throw new MsgException("TUNSETIFF failed.");
	}

	/* don't checksum */
	ioctl(mFD, TUNSETNOCSUM, 1);

	/* Configure device */
	if (execIFConfigScript("up")) {
		::close(mFD);
		throw new MsgException("error executing ifconfig.");
	}
}

virtual	uint getWriteFramePrefix()
{
	return 0;
}

}; // end of LinuxEthTunDevice

EthTunDevice *createEthernetTunnel()
{
	return new LinuxEthTunDevice("ppc" /* FIXME: hardcoding */);
}

#elif (defined(__APPLE__) && defined(__MACH__)) || defined (__FreeBSD__)
/*
	Interaction with Darin/Mac OS X "tun" device driver

	See http://chrisp.de/en/projects/tunnel.html,
	for source code and binaries of tunnel.kext :

	kextload /System/Library/Extensions/tunnel.kext
*/

#include <fcntl.h>

#ifdef __FreeBSD__
#define DEFAULT_DEVICE "/dev/tap0"
#else
#define DEFAULT_DEVICE "/dev/tun0"
#endif

class SimpleEthTunDevice: public UnixEthTunDevice {
public:
SimpleEthTunDevice()
: UnixEthTunDevice()
{
	/* allocate tun device */ 
	if ((mFD = ::open(DEFAULT_DEVICE, O_RDWR | O_NONBLOCK)) < 0) {
		throw new MsgException("Failed to open "DEFAULT_DEVICE"! Is tunnel.kext loaded?");
	}
}

virtual	uint getWriteFramePrefix()
{
	// Please document this! Why is it 14?
	return 14;
}

}; // end of SimpleEthTunDevice

EthTunDevice *createEthernetTunnel()
{
	return new SimpleEthTunDevice();
}

#else
/*
 *	System provides no ethernet tunnel
 */

EthTunDevice *createEthernetTunnel()
{
	throw new MsgException("Your system has no support for ethernet tunnels.");
}

#endif
