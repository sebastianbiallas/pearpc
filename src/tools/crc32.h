/*
 * Function for computing CRC32 for the purpose of adding to Ethernet packets.
 *
 */

#ifndef _CRC32_H_
#define _CRC32_H_

#include <stddef.h>
#include "system/types.h"

uint32 ether_crc(size_t len, const byte *p);

#endif /* _CRC32_H_ */
