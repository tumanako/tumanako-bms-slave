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
#include "evd5.h"
#include "crc.h"

//#define MAP_CURRENT_MATRIX
#define RESISTOR_SHUNT 1

#define FULL_VOLTAGE 3600
#define SHUNT_CURRENT_HYSTERISIS 100
#define MAX_SHUNT_CURRENT 500
#define ABS_MAX_SHUNT_CURRENT 750

#define GAIN_POT_OFF 20
#define GAIN_POT_RESISTOR_ON 40
#define V_SHUNT_POT_OFF 20
#define V_SHUNT_POT_RESISTOR_ON 40
#define MAX_POT 63

#define RX_BUF_SIZE 16
// SDCC is little endian
#define EEPROM_CELL_ID_ADDRESS 0x10
//#define CELL_ID 1  //Only needs to be defined the first time the pic is programed

// packet
// 4 character start-of-packet string "helo"
// 2 character cell id
// 1 character sequence number
// 1 character command
// TODO replace with enum?
#define STATE_WANT_MAGIC_1 0
#define STATE_WANT_MAGIC_2 1
#define STATE_WANT_MAGIC_3 2
#define STATE_WANT_MAGIC_4 3
#define STATE_WANT_CELL_ID_HIGH 4
#define STATE_WANT_CELL_ID_LOW 5
#define STATE_WANT_SEQUENCE_NUMBER 6
#define STATE_WANT_COMMAND 7

/* Setup chip configuration */
#ifdef SDCC
typedef unsigned int config;
config at 0x2007 __CONFIG = _CP_OFF & _CPD_OFF & _BOD_OFF & _PWRTE_ON & _WDT_OFF & _INTRC_OSC_NOCLKOUT & _MCLRE_ON & _FCMEN_OFF & _IESO_OFF;
#endif

void main();
#ifdef SDCC
void interruptHandler(void) __interrupt 0;
#else
void interruptHandler(void);
#endif
void executeCommand(unsigned char rx);
void halt();

unsigned short getIShunt();
unsigned short getTemperature();
unsigned short getVCell();
unsigned short getVShunt();

void txBinStatus();

void vddOn();
void vddOff();
unsigned char isVddOn();
#ifdef MAP_CURRENT_MATRIX
void mapCurrentMatrix();
#endif

void setIShunt(unsigned short i);
void setVShuntPot(unsigned char c);
void setGainPot(unsigned char c);

void writeCellID(unsigned short serial);
void initCellMagic();

// the number of times we have seen overcurrent on the shunt
// TODO provide interface to get and clear this
unsigned char eventOverCurrent = 0;

volatile char rxBuf[RX_BUF_SIZE];
volatile unsigned char rxStart = 0;
volatile unsigned char rxEnd = 0;

// incremented each time the timer overflows (skips multiples of 32)
volatile unsigned char timerOverflow = 1;

#define CELL_MAGIC_ADDR			0x140
#define CELL_ID_ADDR			CELL_MAGIC_ADDR + 4
#define I_SHUNT_ADDR			CELL_MAGIC_ADDR + 6
#define V_CELL_ADDR			I_SHUNT_ADDR + 2
#define V_SHUNT_ADDR			V_CELL_ADDR + 2
#define TEMPERATURE_ADDR		V_SHUNT_ADDR + 2
#define MIN_CURRENT_ADDR		TEMPERATURE_ADDR + 2
#define SEQUENCE_NUMBER_ADDR		MIN_CURRENT_ADDR + 2
#define GAIN_POT_ADDR			SEQUENCE_NUMBER_ADDR + 1
#define V_SHUNT_POT_ADDR		GAIN_POT_ADDR + 1
#define HAS_RX_ADDR			V_SHUNT_POT_ADDR + 1
#define SOFTWARE_ADDRESSING_ADDR	HAS_RX_ADDR + 1
#define AUTOMATIC_ADDR			SOFTWARE_ADDRESSING_ADDR + 1
#define CRC_ADDR			AUTOMATIC_ADDR + 1

// magic string at start of packet (includes cellID)
char at (CELL_MAGIC_ADDR) cellMagic[4];
volatile unsigned short at (CELL_ID_ADDR) cellID;
volatile unsigned short at (I_SHUNT_ADDR) iShunt;
volatile unsigned short at (V_CELL_ADDR) vCell;
volatile unsigned short at (V_SHUNT_ADDR) vShunt;
volatile unsigned short at (TEMPERATURE_ADDR) temperature;
volatile unsigned short at (MIN_CURRENT_ADDR) minCurrent = 0;
volatile unsigned char at (SEQUENCE_NUMBER_ADDR) sequenceNumber;
// lower numbers == less gain
volatile char at (GAIN_POT_ADDR) gainPot = MAX_POT;
// lower numbers == less voltage
volatile char at (V_SHUNT_POT_ADDR) vShuntPot = MAX_POT;
volatile unsigned char at (HAS_RX_ADDR) hasRx = 0;
volatile unsigned char at (SOFTWARE_ADDRESSING_ADDR) softwareAddressing = 1;
volatile unsigned char at (AUTOMATIC_ADDR) automatic = 1;

volatile unsigned char state = STATE_WANT_MAGIC_1;

#ifdef SDCC
void interruptHandler(void) __interrupt 0 {
#else
void interruptHandler(void) {
#endif
	if (RCIF) {
		rxBuf[rxEnd % RX_BUF_SIZE] = RCREG;
		rxEnd++;
	}
	if (TMR1IF) {
		timerOverflow++;
		TMR1IF = 0;
	}
	while (rxStart != rxEnd) {
		char rx = rxBuf[rxStart % RX_BUF_SIZE];
		hasRx = 1;
		rxStart++;
		if (softwareAddressing) {
			switch (state) {
				case STATE_WANT_SEQUENCE_NUMBER :
					state = STATE_WANT_COMMAND;
					sequenceNumber = rx;
					break;
				case STATE_WANT_COMMAND :
					state = STATE_WANT_MAGIC_1;
					executeCommand(rx);
					break;
				default :
					if (rx == cellMagic[state]) {
						state++;
					} else {
						state = STATE_WANT_MAGIC_1;
					}
			}
			//txByte(state);
			//crlf();
		} else {
			executeCommand(rx);
		}
	}
}

void main(void) {
	OSCCON = BIN(01010000);		// set internal clock for 2MHz freq
	SPBRG = 12;			// set baud rate

	TX9 = 0;
	TXEN = 1;
	SYNC = 0;
	BRGH = 1;

	SPEN = 1;
	RX9 = 0;
	CREN = 1;

	RCIE = 1;
	GIE = 1;
	PEIE = 1;

	CMCON0 = 0x07;			// set to digital I/O and turn off comparitors
	ANSEL = BIN(10101010);

	PORTA = 0;
	PORTC = 0;
	TRISA = BIN(11011010);
	TRISC = BIN(11101010);
 
	ADCON1 = BIN(01000000);		// sets clock source

	RA2 = 1;			// release chip select on shunt voltage control
	RA0 = 1;			// release chip select on shunt gain control
	vddOff();

	// set up timer for ADC measurements
	T1CKPS1 = 1;			// divide by 4
	T1CKPS0 = 0;
	TMR1CS = 0;			// internal clcock source
	TMR1H = 0;			// set to zero
	TMR1L = 0;
	TMR1IF = 0;			// clear interrupt flag
	TMR1IE = 1;			// enable interrupt
	TMR1ON = 1;			// turn it on

#ifdef CELL_ID
	writeCellID(CELL_ID);
#endif
	initCellMagic();

	red(150);
	green(150);
	red(150);
	green(150);

	while(1) {
		if (!isVddOn()) {
			vddOn();
		}
		if (minCurrent) {
			red(10);
		} else {
			green(10);
		}
		{
			unsigned short localIShunt = getIShunt();
			unsigned short localTemperature = getTemperature();
			unsigned short localVCell = getVCell();
			unsigned short localVShunt = getVShunt();

			// turn off interrupts
			disableInterrupts();
			iShunt = localIShunt;
			temperature = localTemperature;
			vCell = localVCell;
			vShunt = localVShunt;
			// turn on interrupts
			GIE = 1;
		}
		if (timerOverflow % 32 == 0) {
			// increment timerOverflow so we don't drop back in here on the next loop and go to sleep
			timerOverflow++;
			if (hasRx == 0) {
				halt();
			}
			hasRx = 0;
			red(10);
		}
		if (OERR) {
			// recieve overflow
			SPEN = 0;
			SPEN = 1;
		}
		if (automatic) {
			setIShunt(minCurrent);
		}
	}
}

void executeCommand(unsigned char rx) {
	switch (rx) {
		case 'p' :
			vddOn();
			break;
		case 'o' :
			vddOff();
			break;
		case 'u' :
			setVShuntPot(vShuntPot + 1);
			break;
		case 'd' :
			setVShuntPot(vShuntPot - 1);
			break;
		case '1' :
			setGainPot(gainPot - 1);
			break;
		case '2' :
			setGainPot(gainPot + 1);
			break;
		case 'g' :
			green(200);
			break;
		case 'r' :
			red(200);
			break;
#ifdef MAP_CURRENT_MATRIX
		case 'm' :
			mapCurrentMatrix();
			break;
#endif
		case '/' :
			txBinStatus();
			break;
		case 'x' :
			halt();
			break;
		case '>' :
			if (minCurrent < MAX_SHUNT_CURRENT) {
				minCurrent += 50;
			}
			txShort(minCurrent);
			crlf();
			break;
		case '<' :
			if (minCurrent > 0) {
				minCurrent -= 50;
			}
			txShort(minCurrent);
			crlf();
			break;
		case '#' :
			softwareAddressing = 1 - softwareAddressing;
			txByte(softwareAddressing);
			crlf();
			break;
		case '$' :
			automatic = 1 - automatic;
			txByte(automatic);
			crlf();
			break;
	}
	green(1);
}

unsigned short getTemperature() {
	unsigned short result = ~adc(BIN(10010101)) & 0x03FF;
	return (10700000 - 11145l * result) / 1000;
}

unsigned short getVCell() {
	unsigned short result;
	result = adc(BIN(10000101));
	return 1254400l / result;
}

unsigned short getVShunt() {
	unsigned short shunt = adc(BIN(10001101));
	return (unsigned long) vCell * shunt / 1024;
}


unsigned short getIShunt() {
	unsigned short result = adc(BIN(11011101));
	return 1225l * 10l * result * 10l / 49l / 1024l;
}

void vddOn() {
	RC0 = 0;	// turn on FET Qx02
	setGainPot(GAIN_POT_OFF);
	setVShuntPot(V_SHUNT_POT_OFF);
}

void vddOff() {
	RC0 = 1;	// turn off FET Qx02
}

unsigned char isVddOn() {
	return RC0 == 0;
}

// todo find out how to pass a register and combine with setGainPot
void setVShuntPot(unsigned char c) {
	unsigned char i;
	if (c > MAX_POT) {
		c = MAX_POT;
	}
	// first drive to 0
	RA5 = 1;			// set U/D high for increment (decreses voltage)
	RA2 = 0;			// set /CS0 low (active)
	for (i = 64; i != 0; i--) {
		RA5 = 0;		// pulse low
		RA5 = 1;		// back to high
	}
	RA2 = 1;			// release chip select CS0 (set high)
	
	// now drive to desired value
	RA5 = 0;			// set U/D low for decrement (increases voltage)
	RA2 = 0;			// set /CS0 low (active)
	for (i = c; i != 0; i--) {
		RA5 = 1;		// pulse high
		RA5 = 0;		// back to low
	}
	RA2 = 1;			// release chip select CS0 (set high)
	vShuntPot = c;
	restoreLed();
}

void setGainPot(unsigned char c) {
	unsigned char i;
	if (c > MAX_POT) {
		c = MAX_POT;
	}
	// first drive to 0
	RA5 = 1;			// set U/D high for increment (decreses voltage)
	RA0 = 0;			// set /CS1 low (active)
	for (i = 64; i != 0; i--) {
		RA5 = 0;		// pulse low
		RA5 = 1;		// back to high
	}
	RA0 = 1;			// release chip select CS0 (set high)
	
	// now drive to desired value
	RA5 = 0;			// set U/D low for decrement (increases voltage)
	RA0 = 0;			// set /CS0 low (active)
	for (i = c; i != 0; i--) {
		RA5 = 1;		// pulse high
		RA5 = 0;		// back to low
	}
	RA0 = 1;			// release chip select CS0 (set high)
	gainPot = c;
	restoreLed();
}

void txBinStatus() {
	unsigned char *buf = (unsigned char *) &cellID;
	short crc = crc_init();
	int i;

	for (i = 0; i < EVD5_STATUS_LENGTH - 2; i++) {
		tx(*buf);
		crc = crc_update(crc, buf, 1);
		buf++;
	}
	crc = crc_finalize(crc);
	tx(crc);
	tx(crc >> 8);
}

void halt() {
	minCurrent = 0;
	red(100);
	green(100);
	vddOff();
	TMR1IE = 0;			// disable timer interrupt so it doesn't wake us up
	WUE = 1;			// wake up if we receive something (recieve interrupt still enabled)
#ifdef SDCC
	_asm
		sleep
	_endasm;
#endif
	TMR1IE = 1;			// enable timer
	green(10);
	red(10);
	vddOn();
}

void setIShunt(unsigned short targetShuntCurrent) {
	short difference;
	// if we want zero current, park pots at ..._POT_OFF position
	if (targetShuntCurrent == 0 && (gainPot != GAIN_POT_OFF || vShuntPot != V_SHUNT_POT_OFF)) {
		setGainPot(GAIN_POT_OFF);
		setVShuntPot(V_SHUNT_POT_OFF);
		return;
	}
#ifdef RESISTOR_SHUNT
	if (targetShuntCurrent != 0 && (gainPot != GAIN_POT_RESISTOR_ON || vShuntPot != V_SHUNT_POT_RESISTOR_ON)) {
		setGainPot(GAIN_POT_RESISTOR_ON);
		setVShuntPot(V_SHUNT_POT_RESISTOR_ON);
	}
#else
	// first do current limit
	if (iShunt > ABS_MAX_SHUNT_CURRENT) {
		setGainPot(GAIN_POT_OFF);
		setVShuntPot(GAIN_POT_OFF);
		if (eventOverCurrent < 255) {
			eventOverCurrent++;
		}
		return;
	}
	difference = iShunt - targetShuntCurrent;
	if (sabs(difference) < SHUNT_CURRENT_HYSTERISIS) {
		return;
	}
	if (difference < 0) {
		if (vShuntPot >= MAX_POT) {
			setVShuntPot(vShuntPot - 10);
			setGainPot(gainPot + 1);
		}
		setVShuntPot(vShuntPot + 1);
		red(2);
	} else {
		// TODO reduce gain
		setVShuntPot(vShuntPot - 1);
		green(2);
	}
#endif
}

void initCellMagic() {
	cellMagic[0] = 'h';
	cellMagic[1] = 'e';
	cellMagic[2] = 'l';
	cellMagic[3] = 'o';
	cellMagic[4] = readEEPROM(EEPROM_CELL_ID_ADDRESS);
	cellMagic[5] = readEEPROM(EEPROM_CELL_ID_ADDRESS + 1);
}

void restoreLed() {
	if (minCurrent) {
		setRed();
	} else if (vCell > FULL_VOLTAGE) {
		setGreen();
	}
}

#ifdef CELL_ID
void writeCellID(unsigned short cellID) {
	sleep(100);
	// SDCC is little endian, so write low byte first
	writeEEPROM(EEPROM_CELL_ID_ADDRESS, (unsigned char) cellID & 0x00FF);
	sleep(100);
	writeEEPROM(EEPROM_CELL_ID_ADDRESS + 1, (unsigned char) ((cellID & 0xFF00) >> 8));
	sleep(100);
}
#endif

#ifdef MAP_CURRENT_MATRIX
void mapCurrentMatrix() {
	unsigned char vShunt;
	unsigned char gain;
	unsigned short current;
	for (vShunt = V_SHUNT_POT_OFF; vShunt < 64; vShunt++) {
		setGainPot(0);
		setVShuntPot(vShunt);
		for (gain = GAIN_POT_OFF; gain < 64; gain++) {
			sleep(100);
			current = getIShunt();
			if (current > 0) {
				txBinStatus();
			}
			if (current > MAX_SHUNT_CURRENT) {
				break;
			}
			setGainPot(gain);
		}
	}
	setVShuntPot(V_SHUNT_POT_OFF);
	setGainPot(GAIN_POT_OFF);
}
#endif
