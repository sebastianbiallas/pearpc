/*
 *	PearPC
 *	sysethtun.h
 *
 *	Abstraction for ethernet tunnel devices
 *
 *	Copyright (C) 2003,2004 Stefan Weyergraf (stefan@weyergraf.de)
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

#ifndef __SYSETHTUN_H__
#define __SYSETHTUN_H__

#include "system/types.h"

// FIXME: reentrancy, threading specs

class PacketDevice {
public:
	/**
	 *	Read a packet from the device into a buffer
	 *
	 *	@param buf pointer to a buffer containing the packet data
	 *	@param size size of the buffer
	 *	@returns number of bytes received and written into buf
	 */
	virtual	uint	recvPacket(void *buf, uint size) = 0;

	/**
	 *	Wait for a packet to be received
	 *
	 *	This function blocks the calling thread until a packet is received
	 *	or an error occurs. It should do this in a manner that consumes
	 *	little to low CPU time.
	 *	If the function succeeds the next call to recvPacket() is
	 *	"very likely" to succeed as well.
	 *
	 *	@returns 0 on success, non-zero in case of error (the result is
	 *	an errno. Ie. you can use it in strerror())
	 */
	virtual	int	waitRecvPacket() = 0;

	/**
	 *	Write a packet from a buffer into the device
	 *
	 *	@param buf pointer to a buffer containing the packet data
	 *	@param size size of the buffer
	 *	@returns number of bytes sent and written into buf
	 */
	virtual	uint	sendPacket(void *buf, uint size) = 0;
};

/**
 *	An ethernet tunnel is a virtual ethernet line connecting the operating
 *	system's networking stack with some other piece of software (as opposed
 *	to a physical ethernet cable, connecting two pieces of hardware).
 *
 *	This class represents one end-point of the tunnel, the aforementioned
 *	"piece of software". Both sides should be configured to have their own
 *	separate network addresses (e.g. MAC and IP addresses).
 *
 *	The packets sent and received are ethernet specific Ethernet-II
 *	frames with IEEE 802.3 MAC addresses, prefixed by a specific number of
 *	bytes (for use by the networking stack).
 */
class EthTunDevice: public PacketDevice {
public:
	/**
	 *	Get driver-specific ethernet write frame prefix.
	 *
	 *	When writing packets, a specific number of bytes in the packet
	 *	buffer must be dedicated to the system's networking stack. The
	 *	actual ethernet frame follows after this area.
	 *
	 *	@returns number of bytes in write frame prefix
	 */
	virtual	uint	getWriteFramePrefix() = 0;
	/**
	 *	Initialize the device.
	 *
	 *	@returns init status (0 = ok)
	 */
	virtual int		initDevice() = 0;
	/**
	 *	Uninitialize the device.
	 *
	 *	@returns shutdown status (0 = ok)
	 */
	virtual int		shutdownDevice() = 0;
};

/* system-dependent (implementation in $MYSYSTEM/ *.cc) */
extern EthTunDevice *createEthernetTunnel();

#endif /* __SYSETHTUN_H__ */
