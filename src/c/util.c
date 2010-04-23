/*
    Copyright 2009 Tom Parker

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

/* Define processor and include header file. */
#define __16f688
#include "pic/pic16f688.h"
#include "util.h"

void tx(unsigned char c) {
	while (TXIF == 0) {
		// spin
	}
	TXREG = c;
}

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

#ifdef SEND_BINARY
void txBin10(unsigned short c) {
	unsigned char i;

	for (i = 0; i < 10; i++) {
		if ((c << i) & 0x0200) {
			tx(0x31);
		} else {
			tx(0x30);
		}
	}
}
#endif

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

/** Make and average ADC readings until the timer reaches ADC_TIME */
unsigned short adc(unsigned char con) {
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
		_asm
			nop
		_endasm;
	
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
	return result / i;
}
