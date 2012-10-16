/**
 * \file crc.c
 * Functions and types for CRC checks.
 *
 * Generated on Mon Apr 19 00:38:26 2010,
 * by pycrc v0.7.5, http://www.tty1.net/pycrc/
 * using the configuration:
 *    Width        = 16
 *    Poly         = 0x1021
 *    XorIn        = 0xffff
 *    ReflectIn    = False
 *    XorOut       = 0x0000
 *    ReflectOut   = False
 *    Algorithm    = bit-by-bit-fast
 *****************************************************************************/
#include "crc.h"
#include <stdint.h>


/**
 * Update the crc value with new data.
 *
 * \param crc      The current crc value.
 * \param data     Pointer to a buffer of \a data_len bytes.
 * \param data_len Number of bytes in the \a data buffer.
 * \return         The updated crc value.
 *****************************************************************************/
crc_t crc_update(crc_t crc, const unsigned char c)
{
    unsigned int i;
	int bit;

	for (i = 0x80; i > 0; i >>= 1) {
		bit = crc & 0x8000;
		if (c & i) {
			bit = !bit;
		}
		crc <<= 1;
		if (bit) {
			crc ^= 0x1021;
		}
	}
	return crc & 0xffff;
}


