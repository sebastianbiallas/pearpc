/*
 *  TAP-Win32 -- A kernel driver to provide virtual tap device functionality
 *               on Windows.  Originally derived from the CIPE-Win32
 *               project by Damion K. Wilson, with extensive modifications by
 *               James Yonan.
 *
 *  All source code which derives from the CIPE-Win32 project is
 *  Copyright (C) Damion K. Wilson, 2003, and is released under the
 *  GPL version 2 (see below).
 *
 *  All other source code is Copyright (C) James Yonan, 2003,
 *  and is released under the GPL version 2 (see below).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TAP_CONSTANTS_DEFINED
#define TAP_CONSTANTS_DEFINED

//====================================================================
//                        Product and Version public settings
//====================================================================
#include <windows.h>
#include <Winioctl.h>
#define PRODUCT_STRING "TAP VPN Adapter."
#define TAP_SERVICE_NAME "TAP_Daemon"
#define TAP_DRIVER_NAME "TAP"

#define TAP_NDIS_MAJOR_VERSION 5
#define TAP_NDIS_MINOR_VERSION 0

// Minimum version number expected by userspace
#define TAP_WIN32_MIN_MAJOR 3
#define TAP_WIN32_MIN_MINOR 10

#ifdef CIPE_SERVICE_DEFINES

#ifndef ENABLE_RANDOM_MAC
# ifndef TAP_MAC_ROOT_ADDRESS
#   define TAP_MAC_ROOT_ADDRESS "8:0:58:0:0:1"
# endif
#endif

// milliseconds before ping timeout
#ifndef PING_TIMEOUT
#   define PING_TIMEOUT 15000 
#endif

// Number of packets before key exchange
#ifndef KEY_EXCHANGE_PACKETS
#   define KEY_EXCHANGE_PACKETS 10000 
#endif

// Ten minutes in seconds
#ifndef KEY_EXCHANGE_TIMEOUT
#   define KEY_EXCHANGE_TIMEOUT 600 
#endif

// Once every 100 packets
#ifndef STATISTICS_UPDATE_FREQUENCY
#   define STATISTICS_UPDATE_FREQUENCY 100 
#endif

// Make 0 if a "lazy" key exchange is desired
#ifndef KEY_EXCHANGE_EARLY
#   define KEY_EXCHANGE_EARLY 1
#endif

// Make 1 if we want to save dynamic keys
#ifndef KEY_REMEMBER_DYNAMIC
#   define KEY_REMEMBER_DYNAMIC 0
#endif

// Ten second Select() timeout
#ifndef DAEMON_SELECT_TIMEOUT
#   define DAEMON_SELECT_TIMEOUT 10000
#endif

typedef enum
   {
    NK_KEY_EXCHANGE = 2,
    NK_DATA = 0,
    NK_REQ = 1,
    NK_IND = 2,
    NK_ACK = 3,
    CT_DUMMY = 0x70,
    CT_DEBUG = 0x71,
    CT_PING = 0x72,
    CT_PONG = 0x73,
    CT_KILL = 0x74
   }
NK_Type;

#define UDP_DATAGRAM_BUFFER_SIZE 65536
typedef unsigned char UDPBUFFER [UDP_DATAGRAM_BUFFER_SIZE];

#define BLOWFISH_DATA_SIZE 8      // Eight bytes (64 bit) datum
#define BLOWFISH_KEY_LENGTH 16    // Keys are 16 bytes long

#define MIN(a,b) (a > b ? b : a)
#define MAX(a,b) (a < b ? b : a)

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY 0

#endif

//==========================================================
//                                   Windows 2000 OID's
//==========================================================
#ifndef OID_GEN_TRANSPORT_HEADER_OFFSET

#define	OID_GEN_SUPPORTED_GUIDS			0x00010117
#define	OID_GEN_NETWORK_LAYER_ADDRESSES		0x00010118  // Set only
#define OID_GEN_TRANSPORT_HEADER_OFFSET		0x00010119  // Set only

//
//	TCP/IP OIDs
//
#define	OID_TCP_TASK_OFFLOAD			0xFC010201
#define	OID_TCP_TASK_IPSEC_ADD_SA		0xFC010202
#define	OID_TCP_TASK_IPSEC_DELETE_SA		0xFC010203
#define OID_TCP_SAN_SUPPORT			0xFC010204

//
//	Defines for FFP
//
#define OID_FFP_SUPPORT				0xFC010210
#define	OID_FFP_FLUSH				0xFC010211
#define	OID_FFP_CONTROL				0xFC010212
#define	OID_FFP_PARAMS				0xFC010213
#define	OID_FFP_DATA				0xFC010214

#define	OID_FFP_DRIVER_STATS			0xFC020210
#define	OID_FFP_ADAPTER_STATS			0xFC020211

//
//	PnP and PM OIDs
//
#define	OID_PNP_CAPABILITIES			0xFD010100
#define	OID_PNP_SET_POWER			0xFD010101
#define	OID_PNP_QUERY_POWER			0xFD010102
#define OID_PNP_ADD_WAKE_UP_PATTERN		0xFD010103
#define OID_PNP_REMOVE_WAKE_UP_PATTERN		0xFD010104
#define	OID_PNP_WAKE_UP_PATTERN_LIST		0xFD010105
#define	OID_PNP_ENABLE_WAKE_UP			0xFD010106

#endif

//===============================================
// TAP IOCTLs
//===============================================
#define TAP_CONTROL_CODE(request,method) \
  CTL_CODE (FILE_DEVICE_PHYSICAL_NETCARD | 8000, request, method, FILE_ANY_ACCESS)

#define TAP_IOCTL_GET_LASTMAC           TAP_CONTROL_CODE (0, METHOD_BUFFERED)
#define TAP_IOCTL_GET_MAC               TAP_CONTROL_CODE (1, METHOD_BUFFERED)
#define TAP_IOCTL_SET_STATISTICS        TAP_CONTROL_CODE (2, METHOD_BUFFERED)
#define TAP_IOCTL_GET_VERSION           TAP_CONTROL_CODE (3, METHOD_BUFFERED)
#define TAP_IOCTL_GET_MTU               TAP_CONTROL_CODE (4, METHOD_BUFFERED)
#define TAP_IOCTL_GET_INFO              TAP_CONTROL_CODE (5, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT TAP_CONTROL_CODE (6, METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS      TAP_CONTROL_CODE (7, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_MASQ      TAP_CONTROL_CODE (8, METHOD_BUFFERED)

//=======================================================================
// Registry keys
//=======================================================================

#define NETCARD_REG_KEY_2000 "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

#define NETCARD_REG_KEY      "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards"

#define REG_SERVICE_KEY      "SYSTEM\\CurrentControlSet\\Services"

#define REG_CONTROL_NET      "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

//============================================================
// MAC address, Ethernet header, and ARP
//============================================================

#pragma pack(1)

#define IP_HEADER_SIZE 20

typedef unsigned char MACADDR [6];
typedef unsigned long IPADDR;

// Ethernet header
typedef struct
{
  MACADDR dest;               /* destination eth addr	*/
  MACADDR src;                /* source ether addr	*/

# define ETH_P_IP   0x0800    /* IPv4 protocol */
# define ETH_P_ARP  0x0806    /* ARP protocol */
  USHORT proto;               /* packet type ID field	*/
} ETH_HEADER, *PETH_HEADER;

typedef struct
   {
    MACADDR        m_MAC_Destination;        // Reverse these two
    MACADDR        m_MAC_Source;             // to answer ARP requests
    USHORT         m_Proto;                  // 0x0806

#   define MAC_ADDR_TYPE 0x0001
    USHORT         m_MAC_AddressType;        // 0x0001

    USHORT         m_PROTO_AddressType;      // 0x0800
    UCHAR          m_MAC_AddressSize;        // 0x06
    UCHAR          m_PROTO_AddressSize;      // 0x04

#   define ARP_REQUEST 0x0001
#   define ARP_REPLY   0x0002
    USHORT         m_ARP_Operation;              // 0x0001 for ARP request, 0x0002 for ARP reply

    MACADDR        m_ARP_MAC_Source;
    IPADDR         m_ARP_IP_Source;
    MACADDR        m_ARP_MAC_Destination;
    IPADDR         m_ARP_IP_Destination;
   }
ARP_PACKET, *PARP_PACKET;

#pragma pack()

#define COPY_MAC(dest, src) memcpy(dest, src, sizeof (MACADDR));

#define MAC_EQUAL(a,b) (memcmp (a, b, sizeof (MACADDR)) == 0)

//===========================================================
// Driver constants
//===========================================================

#define ETHERNET_HEADER_SIZE (sizeof (ETH_HEADER))
#define ETHERNET_PACKET_SIZE (1500 + ETHERNET_HEADER_SIZE)
#define DEFAULT_PACKET_LOOKAHEAD (ETHERNET_PACKET_SIZE - ETHERNET_HEADER_SIZE)

#define MINIMUM_MTU 576    // USE TCP Minimum MTU
#define MAXIMUM_MTU 65536  // IP maximum MTU

#define USERMODEDEVICEDIR "\\\\.\\"
#define SYSDEVICEDIR  "\\Device\\"
#define USERDEVICEDIR "\\??\\"
#define TAPSUFFIX     ".tap"

#define PACKET_QUEUE_SIZE   64
#define IRP_QUEUE_SIZE      16

#define TAP_LITTLE_ENDIAN

#endif
