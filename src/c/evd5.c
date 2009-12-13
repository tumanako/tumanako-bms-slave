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

//#define SEND_BINARY
#define SHUNT_CIRCUIT_FIX
#define MAP_CURRENT_MATRIX

#define SHUNT_START_VOLTAGE 3550
#define MAX_CELL_VOLTAGE 3600
#define SHUNT_CURRENT_HYSTERISIS 100
#define MAX_SHUNT_CURRENT 500
#define ABS_MAX_SHUNT_CURRENT 750

#define GAIN_POT_OFF 20
#define V_SHUNT_POT_OFF 20
#define MAX_POT 63

#define RX_BUF_SIZE 8
#define EEPROM_CELL_ID_LOW 0x0F
#define EEPROM_CELL_ID_HIGH 0x10

// packet
// 4 character start-of-packet string "helo"
// 3 character cell id
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
typedef unsigned int config;
config at 0x2007 __CONFIG = _CP_OFF & _CPD_OFF & _BOD_OFF & _PWRTE_ON & _WDT_OFF & _INTRC_OSC_NOCLKOUT & _MCLRE_ON & _FCMEN_OFF & _IESO_OFF;

void main();
void interruptHandler(void) __interrupt 0;
void executeCommand(unsigned char rx);
void halt();

unsigned short getVCell();
unsigned short getVShunt();

void txIShunt();
void txStatus();
void txTargetIShunt();
void txTemperature();
void txVCell();
void txVShunt();

void vddOn();
void vddOff();
unsigned char isVddOn();
#ifdef MAP_CURRENT_MATRIX
void mapCurrentMatrix();
#endif

unsigned short calculateTargetIShunt();
void setIShunt(unsigned short i);
void setVShuntPot(unsigned char c);
void setGainPot(unsigned char c);

void writeCellID(unsigned short serial);
void initCellMagic();

// lower numbers == less gain
char gainPot = MAX_POT;
// lower numbers == less voltage
char vShuntPot = MAX_POT;

// the number of times we have seen overcurrent on the shunt
// TODO provide interface to get and clear this
unsigned char eventOverCurrent = 0;
unsigned short minCurrent = 0;

// current status
unsigned short vCell;
unsigned short vShunt;

volatile char rxBuf[RX_BUF_SIZE];
volatile unsigned char rxStart = 0;
volatile unsigned char rxEnd = 0;

// incremented each time the timer overflows (skips multiples of 32)
volatile unsigned char timerOverflow = 1;

struct status_t {
	// true if we have received a character since the last time loopCounter overflowed
	unsigned char hasRx:1;
	// true if we are doing software addressing
	unsigned char softwareAddressing:1;
	// true if we are controlling the shunt current automatically
	unsigned char automatic:1;
};
struct status_t status;

// magic string at start of packet (includes cell ID)
char cellMagic[6];
unsigned char state = STATE_WANT_MAGIC_1;
unsigned char sequenceNumber;

void interruptHandler(void) __interrupt 0 {
	if (RCIF) {
		rxBuf[rxEnd % RX_BUF_SIZE] = RCREG;
		rxEnd++;
	}
	if (TMR1IF) {
		timerOverflow++;
		TMR1IF = 0;
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

	//writeCellID(0x3033);
	initCellMagic();

	status.hasRx = 0;
	status.softwareAddressing = 1;
	status.automatic = 1;

	red(150);
	green(150);
	red(150);
	green(150);

	while(1) {
		if (!isVddOn()) {
			vddOn();
		}
		vCell = getVCell();
		vShunt = getVShunt();
		if (timerOverflow % 32 == 0) {
			// increment timerOverflow so we don't drop back in here on the next loop and go to sleep
			timerOverflow++;
			if (status.hasRx == 0 && vCell < SHUNT_START_VOLTAGE) {
				halt();
			}
			status.hasRx = 0;
			red(10);
		}
		if (OERR) {
			// recieve overflow
			SPEN = 0;
			SPEN = 1;
		}
		while (rxStart != rxEnd) {
			char rx = rxBuf[rxStart % RX_BUF_SIZE];
			status.hasRx = 1;
			rxStart++;
			if (status.softwareAddressing) {
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
//				txByte(state);
//				crlf();
			} else {
				executeCommand(rx);
			}
		}
		if (status.automatic) {
			setIShunt(calculateTargetIShunt());
		}
	}
}

void executeCommand(unsigned char rx) {
	switch (rx) {
		case 'c' :
			txVCell();
			crlf();
			break;
		case 's' :
			txVShunt();
			crlf();
			break;
		case 'i' :
			txIShunt();
			crlf();
			break;
		case 't' :
			txTemperature();
			crlf();
			break;
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
		case 'q' :
			txTargetIShunt();
			crlf();
			break;
#ifdef MAP_CURRENT_MATRIX
		case 'm' :
			mapCurrentMatrix();
			break;
#endif
		case '?' :
			txStatus();
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
			status.softwareAddressing = 1 - status.softwareAddressing;
			txByte(status.softwareAddressing);
			crlf();
			break;
		case '$' :
			status.automatic = 1 - status.automatic;
			txByte(status.automatic);
			crlf();
			break;
	}
	green(1);
}

void txVCell() {
	tx('V');
	tx('c');
	tx('=');
#ifdef SEND_BINARY
	txBin10(~adc(BIN(10000101)));
#else
	txShort(vCell);
#endif
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

void txVShunt() {
	tx('V');
	tx('s');
	tx('=');
#ifdef SEND_BINARY
	{
		unsigned short shunt = adc(BIN(10001101));
		txBin10(~shunt & 0x03FF);
	}
#else
	txShort(vShunt);
#endif
}

unsigned short iShunt() {
	unsigned short result = adc(BIN(11011101));
#ifdef SHUNT_CIRCUIT_FIX
	return 1225l * 10l * result * 10l / 49l / 1024l;
#else
	// 7 is a constant obtained by measuring the actual current
	return (unsigned long) 1225 * 10 * result / 1024 / 7;
#endif
}

void txIShunt() {
	tx('I');
	tx('s');
	tx('=');
#ifdef SEND_BINARY
	txBin10(adc(BIN(10011101)));
#else
	txShort(iShunt());
#endif
}

void txTemperature() {
	// we take the compliment because the original calculation requires it
	// TODO work out new calculation that doesn't take the compliment
	unsigned short result = ~adc(BIN(10010101)) & 0x03FF;
	tx('V');
	tx('t');
	tx('=');
#ifdef SEND_BINARY
	txBin10(result);
#else
	txShort((10700000 - 11145l * result) / 1000);
#endif
}

void vddOn() {
	RC0 = 0;	// turn on FET Qx02
	setGainPot(0);
	setVShuntPot(0);
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
}


void txTargetIShunt() {
	tx('Q');
	tx('=');
	txShort(calculateTargetIShunt());
}

void txStatus() {
	txVCell();
	tx(' ');
	txVShunt();
	tx(' ');
	txIShunt();
	tx(' ');
	txTargetIShunt();
	tx(' ');
	txTemperature();
	tx(' ');
	tx('V');
	tx('g');
	tx('=');
	txByte(vShuntPot);
	tx(' ');
	tx('g');
	tx('=');
	txByte(gainPot);
	tx(' ');
	tx('m');
	tx('=');
	txShort(minCurrent);
	crlf();
}

void halt() {
	minCurrent = 0;
	red(100);
	green(100);
	vddOff();
	TMR1IE = 0;			// disable timer interrupt so it doesn't wake us up
	WUE = 1;			// wake up if we receive something (recieve interrupt still enabled)
	_asm
		sleep
	_endasm;
	TMR1IE = 1;			// enable timer
	green(10);
	red(10);
	vddOn();
}


unsigned short calculateTargetIShunt() {
	unsigned short cellVoltage = vCell;
	long difference;
	short result;
	if (cellVoltage < SHUNT_START_VOLTAGE) {
		return minCurrent;
	}
	if (cellVoltage > MAX_CELL_VOLTAGE) {
		return MAX_SHUNT_CURRENT;
	}
	difference = (long) cellVoltage - SHUNT_START_VOLTAGE;
	result = (long) difference * 1000l / (MAX_CELL_VOLTAGE - SHUNT_START_VOLTAGE) * MAX_SHUNT_CURRENT / 1000;
	if (result > MAX_SHUNT_CURRENT || result < 0) {
		tx('I');
		tx('t');
		tx('!');
		txShort(result);
		crlf();
		return 0;
	}
	if (result > minCurrent) {
		return result;
	} else {
		return minCurrent;
	}
}

void setIShunt(unsigned short targetShuntCurrent) {
	unsigned short shuntCurrent = iShunt();
	short difference;
	// if we want zero current, park pots at ..._POT_OFF position
	if (targetShuntCurrent == 0 && (gainPot != GAIN_POT_OFF || vShuntPot != V_SHUNT_POT_OFF)) {
		setGainPot(GAIN_POT_OFF);
		setVShuntPot(V_SHUNT_POT_OFF);
		return;
	}
	// first do current limit
	if (shuntCurrent > ABS_MAX_SHUNT_CURRENT) {
		setGainPot(GAIN_POT_OFF);
		setVShuntPot(GAIN_POT_OFF);
		if (eventOverCurrent < 255) {
			eventOverCurrent++;
		}
		return;
	}
	difference = shuntCurrent - targetShuntCurrent;
	if (abs(difference) < SHUNT_CURRENT_HYSTERISIS) {
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
}

void initCellMagic() {
	cellMagic[0] = 'h';
	cellMagic[1] = 'e';
	cellMagic[2] = 'l';
	cellMagic[3] = 'o';
	cellMagic[4] = readEEPROM(EEPROM_CELL_ID_HIGH);
	cellMagic[5] = readEEPROM(EEPROM_CELL_ID_LOW);
}

void writeCellID(unsigned short cellID) {
	writeEEPROM(EEPROM_CELL_ID_HIGH, (unsigned char) ((cellID & 0xFF00) >> 8));
	writeEEPROM(EEPROM_CELL_ID_LOW, (unsigned char) cellID & 0x00FF);
}

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
			current = iShunt();
			if (current > 0) {
				txStatus();
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
