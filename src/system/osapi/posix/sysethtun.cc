/*
 *	PearPC
 *	sysethtun.cc
 *
 *	POSIX/UN*X-specific ethernet-tunnel access
 *
 *	Copyright (C) 2003 Stefan Weyergraf
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
#include <sys/wait.h>

#include "system/sysethtun.h"
#include "tools/snprintf.h"
#include "tools/data.h"
#include "tools/str.h"

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
	// (ohh lovely Borland Pascal, where art thou?)
	mFD = -1;
}

virtual ~UnixEthTunDevice()
{
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
	if (mFD < 0)
		return ENODEV;
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

virtual const char *devicePath()
{
	return NULL;
}

virtual const char *upScript()
{
	return "scripts/ifppc_up.setuid";
}

virtual const char *downScript()
{
	return "scripts/ifppc_down.setuid";
}

// FIXME: How shall we configure networking??? This thing can only be a temporary solution
virtual int execIFConfigScript(const char *action, const char *interface)
{
//sleep(1000000);
	int pid = fork();
	if (pid < 0)
		printf("fork = %d, 0x%08x\n", pid, errno);
	if (pid == 0) {
		const char *progname;
		if (strcmp(action, "up") == 0) {
			progname = upScript();
		} else {
			progname = downScript();
		}
		setenv("PPC_INTERFACE", interface,0);
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

}; // end of UnixEthTunDevice

#if defined(HAVE_LINUX_TUN) || defined(HAVE_BEOS_TUN)
/*
 *	This is how it's done in Linux
 */

#include <sys/ioctl.h>
#include <sys/socket.h>

#ifdef HAVE_LINUX_TUN
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#else /* BeOS */
#include <net/if.h>
#include <linux/if_tun.h> // should be moved to net/ someday too...
#include <image.h>
#endif /* HAVE_LINUX_TUN */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

class LinuxLikeEthTunDevice: public UnixEthTunDevice {
String mIfName;
public:
LinuxLikeEthTunDevice(const char *netif_prefix)
: UnixEthTunDevice()
{
	// We will allocate the next available interface name 
	// First, trim passed interface name down to 6 letters.  To prevent
	// overflow attacks and shell attacks.
	String netif_buffer(netif_prefix);
	netif_buffer.crop(6);
	for (int i=0; i < netif_buffer.length(); i++) {
		char c = netif_buffer[i];
		// Delete all characters not suitable for below command
		if (!(c >= 'a' && c <= 'z')
		 && !(c >= 'A' && c <= 'Z')
		 && !(c >= '0' && c <= '9')
		 && (c != '_')) {
			netif_buffer[i] = '_';
		}
	}
	for (int counter = 0; counter < 10; counter++) {
		String command;
		command.assignFormat("ifconfig %y%d", &netif_buffer, counter);
		// FIXME: Is there anything else than command() that I can use here?  
		// Seems a fork&exec is a bit too much...
		int ret = system(command);
		if (WEXITSTATUS(ret)) { 
			mIfName.assignFormat("%y%d", &netif_buffer, counter);
			ht_printf("mIfName = %y\n", &mIfName);
			break;
		}
	}
	// FIXME:There is more than 10 interfaces taken at this time.  What should we do ?
	if (mIfName == (String)"") 
		throw new MsgfException("There are already 10 interfaces configured.  We don't support more than that currently.");
}


int initDevice()
{

	/* allocate tun device */ 
	if ((mFD = ::open(devicePath(), O_RDWR)) < 0) {
		throw new MsgfException("Failed to open %s", devicePath());
	}

	struct ifreq ifr;
	::memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	if (::strlen(mIfName)+1 > IFNAMSIZ) {
		throw new MsgfException("Interface name too long (%d > %d bytes)"
			" in '%y'\n", ::strlen(mIfName)+1, IFNAMSIZ, &mIfName);
	}

	::strncpy(ifr.ifr_name, mIfName, IFNAMSIZ);
	if (::ioctl(mFD, TUNSETIFF, &ifr) < 0) {
		::close(mFD);
		throw new MsgException("TUNSETIFF failed.");
	}

	/* don't checksum */
	::ioctl(mFD, TUNSETNOCSUM, 1);

	/* Configure device */
	if (execIFConfigScript("up", mIfName)) {
		::close(mFD);
		throw new MsgException("error executing ifconfig.");
	}
	return 0;
}

int shutdownDevice()
{
	/* tear down the device */
	execIFConfigScript("down", mIfName);
	::close(mFD);
	return 0;
}

virtual	uint getWriteFramePrefix()
{
	return 0;
}

virtual const char *devicePath()
{
	return "/dev/net/tun";
}

}; // end of LinuxLikeEthTunDevice

#ifdef HAVE_LINUX_TUN

class LinuxEthTunDevice: public LinuxLikeEthTunDevice {
public:
LinuxEthTunDevice(const char *netif_prefix)
: LinuxLikeEthTunDevice(netif_prefix)
{
}

}; // end of LinuxEthTunDevice

EthTunDevice *createEthernetTunnel()
{
	return new LinuxEthTunDevice("ppc" /* FIXME: hardcoding */);
}

#else /* BeOS */

class BeOSEthTunDevice: public LinuxLikeEthTunDevice {
public:
BeOSEthTunDevice(const char *netif_name)
: LinuxLikeEthTunDevice(netif_name)
{
}

virtual const char *devicePath()
{
	return TUN_DEVICE;
}

virtual const char *upScript()
{
	return "scripts/ifppc_up.beos";
}

virtual const char *downScript()
{
	return "scripts/ifppc_down.beos";
}


#if 1
// FIXME: How shall we configure networking??? This thing can only be a temporary solution
// XXX: BeOS doesn't seem to like forking from PearPC... out of swap !?
// what you get from playing dirty tricks. :)
virtual int execIFConfigScript(const char *arg)
{
	thread_id tid;
	status_t status;
	const char *progname;
	if (strcmp(arg, "up") == 0) {
		progname = upScript();
	} else {
		progname = downScript();
	}
	const char *sargv[] = { "/bin/sh", progname, NULL };
	int sargc = 2;
	printf("executing '%s' ...\n"
"********************************************************************************\n", progname);
	//execl(progname, progname, 0);
	tid = load_image(sargc, sargv, (const char **)environ);
	if ((tid < B_OK) || (resume_thread(tid) < B_OK)) {
		printf("couldn't exec '%s': %s\n", progname, strerror(tid));
		return 1;
	}
	sleep(1);
	wait_for_thread(tid, &status);
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
#endif
}; // end of BeOSEthTunDevice

EthTunDevice *createEthernetTunnel()
{
	return new BeOSEthTunDevice("ppc" /* FIXME: hardcoding */);
}

#endif /* HAVE_LINUX_TUN */

#elif (defined(__APPLE__) && defined(__MACH__)) || defined (__FreeBSD__)
/*
 *	This is how it's done in Darwin, Mac OS X or FreeBSD
 */

/*
	Interaction with Darwin/Mac OS X "tun" device driver

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
}

int initDevice()
{
	/* allocate tun device */ 
	if ((mFD = ::open(DEFAULT_DEVICE, O_RDWR | O_NONBLOCK)) < 0) {
		throw new MsgException("Failed to open "DEFAULT_DEVICE"! Is tunnel.kext loaded?");
	}
	return 0;
}

int shutdownDevice()
{
}

virtual	uint getWriteFramePrefix()
{
	return 0;
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
