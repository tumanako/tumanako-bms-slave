/*
    Copyright 2009-2012 Tom Parker

    This file is part of the Tumanako EVD5 BMS.

    The Tumanako EVD5 BMS is free software: you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    The Tumanako EVD5 BMS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with the Tumanako EVD5 BMS.  If not, see 
    <http://www.gnu.org/licenses/>.
*/

#include "gnuc.h"

/* Define processor and include header file. */
#define __16f688
#include "pic/pic16f688.h"
#include "util.h"
#include "crc.h"

void txShort(unsigned short s) {
	unsigned char ones = s % 10;
	unsigned char tens = (s / 10) % 10;
	unsigned char hundreds = (s / 100) % 10;
	unsigned char thousands = (s / 1000) % 10;
	unsigned char tenthousands = (s / 10000) % 10;
	tx(0x30 + tenthousands);
	tx(0x30 + thousands);
	tx(0x30 + hundreds);
	tx(0x30 + tens);
	tx(0x30 + ones);
}

void txByte(unsigned char b) {
	unsigned char ones = b % 10;
	unsigned char tens = (b / 10) % 10;
	unsigned char hundreds = (b / 100) % 10;
	tx(0x30 + hundreds);
	tx(0x30 + tens);
	tx(0x30 + ones);
}

crc_t txCrc(unsigned char c, crc_t crc) {
	tx(c);
	return crc_update(crc, c);
}

crc_t txEscapeCrc(unsigned char c, crc_t crc) {
	// we inline txEscape here to avoid stack overflow
	if (c == ESCAPE_CHARACTER || c == START_OF_PACKET) {
		tx(ESCAPE_CHARACTER);
	}
	tx(c);
	return crc_update(crc, c);
}

short txEscape(unsigned char c) {
	if (c == ESCAPE_CHARACTER || c == START_OF_PACKET) {
		tx(ESCAPE_CHARACTER);
	}
	tx(c);
}

unsigned short sabs(short s) {
	if (s < 0) {
		return -1 * s;
	}
	return s;
}

void sleep(unsigned char time) {
	time = TMR1H + time;
	while (TMR1H != time) {
		// spin
	}
}

unsigned char readEEPROM(unsigned char address) {
	EEADRH = 0;
	EEADR = address;
	EEPGD = 0;
	RD = 1;
	return EEDAT;
}

void writeEEPROM(unsigned char address, unsigned char value) {
	EEADRH = 0;
	EEADR = address;
	EEDAT = value;
	EEPGD = 0;
	WREN = 1;
	GIE = 0;
	do {
		GIE = 0;
	} while (GIE != 0);
	EECON2 = 0x55;
	EECON2 = 0xAA;
	WR = 1;
	GIE = 1;
	while (WR == 1) {
		;
	}
	WREN = 0;
}

/**
 * Make and average ADC readings until the timer reaches ADC_TIME
 * @return 1000 times the actual result, in order to preserve precision
 */
unsigned long adc(unsigned char con) {
	unsigned long result = 0;
	unsigned short i = 0;
	unsigned char end = TMR1H + ADC_TIME;

	// we will use the timer to go to sleep on inactivity, we don't care if
	// there is some jitter there, so we can set the low byte to zero
	TMR1L = 0;
	while (end != TMR1H) {
		unsigned char singleResultHi;
		unsigned char singleResultLo;
		unsigned short singleResult;
		ADCON1 = BIN(10000000);
		ADCON0 = con;
	
		// TODO why do we have a NOP here?
#ifdef SDCC
		_asm
			nop
		_endasm;
#endif

		GO = 1;
		while (GO) {
			// spin
		}
	
		singleResultHi = ADRESH & 0x03;
		singleResultLo = ADRESL;
	
		singleResult = singleResultHi;
		singleResult = singleResult << 8;
		singleResult = singleResult | singleResultLo;

		// TODO check for overflow
		result += singleResult;
		i++;
	}
	// multiply by 1000 to preserve additional precision acquired by averaging
	// 1000 is way too much, but we're already forced to use long arithmetic so why not
	return result * 1000 / i;
}
