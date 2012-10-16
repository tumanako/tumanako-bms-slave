/**
 * \file crc.h
 * Functions and types for CRC checks.
 *
 * Generated on Mon Apr 19 00:38:21 2010,
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
#ifndef __CRC_H__
#define __CRC_H__

#include <stdint.h>
// gcc doesn't like SDCC's stdlib
#ifdef SDCC
#include <stdlib.h>
#else
typedef unsigned int size_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The definition of the used algorithm.
 *****************************************************************************/
#define CRC_ALGO_BIT_BY_BIT_FAST 1

/**
 * The type of the CRC values.
 *
 * This type must be big enough to contain at least 16 bits.
 *****************************************************************************/
typedef unsigned int crc_t;

/**
 * Calculate the initial crc value.
 *
 * \return     The initial crc value.
 *****************************************************************************/
#define crc_init()      (0xffff)

/**
 * Update the crc value with new data.
 *
 * \param crc      The current crc value.
 * \param c        The data to add to the crc.
 * \return         The updated crc value.
 *****************************************************************************/
crc_t crc_update(crc_t crc, const unsigned char c);

/**
 * Calculate the final crc value.
 *
 * \param crc  The current crc value.
 * \return     The final crc value.
 *****************************************************************************/
#define crc_finalize(crc)      (crc ^ 0x0000)


#ifdef __cplusplus
}           /* closing brace for extern "C" */
#endif

#endif      /* __CRC_H__ */
