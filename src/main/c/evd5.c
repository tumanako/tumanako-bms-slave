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
#include "pic14/pic16f688.h"
#include "util.h"
#include "evd5.h"
#include "crc.h"

//#define MAP_CURRENT_MATRIX
//#define RESISTOR_SHUNT
//#define HARD_SWITCHED_SHUNT
//#define HAS_TEMPERATURE_SENSOR

#define PROTOCOL_VERSION 4

#ifndef CELL_ID_LOW
#define CELL_ID_LOW 0
#define CELL_ID_HIGH 0

#define KELVIN_CONNECTION 0

#define REVISION_LOW 0
#define REVISION_HIGH 0

#define IS_CLEAN 0

#define PROGRAM_DATE_0 0
#define PROGRAM_DATE_1 0
#define PROGRAM_DATE_2 0
#define PROGRAM_DATE_3 0
#endif

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

// packet
// 2 character cell id
// 1 character sequence number
// 1 character command
// 2 character crc
// TODO replace with enum
#define WANT_START_OF_PACKET 0
#define STATE_WANT_CELL_ID_LOW 1
#define STATE_WANT_CELL_ID_HIGH 2
#define STATE_WANT_COMMAND 3
#define STATE_WANT_CRC_LOW 4
#define STATE_WANT_CRC_HIGH 5

#define vddOff() RC0 = 1
#define isVddOn() (RC0 == 0)

/* Setup chip configuration */
#ifdef SDCC
typedef unsigned int config;
config __at 0x2007 __CONFIG = _CP_OFF & _CPD_OFF & _BOD_OFF & _PWRTE_ON & _WDT_OFF & _INTRC_OSC_NOCLKOUT & _MCLRE_ON & _FCMEN_OFF & _IESO_OFF;
#endif

int main();
#ifdef SDCC
void interruptHandler(void) __interrupt 0;
#else
void interruptHandler(void);
#endif
void halt();

#ifdef MAP_CURRENT_MATRIX
void mapCurrentMatrix();
#endif

// the number of times we have seen overcurrent on the shunt
// TODO provide interface to get and clear this
unsigned char eventOverCurrent = 0;

// incremented each time the timer overflows (skips multiples of 32)
volatile unsigned char timerOverflow = 1;

volatile unsigned char command;
crc_t rxCRC;

// if we use local variables in main() the compiler optimises them away :(
unsigned short mainLoopIShunt;
unsigned short mainLoopTemperature;
unsigned short mainLoopVCell;
unsigned short mainLoopVShunt;

volatile unsigned short iShunt;
volatile unsigned short vCell;
volatile unsigned short vShunt;
volatile unsigned short temperature;
volatile unsigned short minCurrent = 0;
// lower numbers == less gain
volatile char gainPot = MAX_POT;
// lower numbers == less voltage
volatile char vShuntPot = MAX_POT;
volatile unsigned char hasRx = 0;
volatile unsigned char softwareAddressing = 1;
volatile unsigned char automatic = 1;

volatile unsigned char state = STATE_WANT_CELL_ID_HIGH;
unsigned char escape = 0;

// SDCC's pic14 port does not save the "stack" so we have to save it.
// Currently we are only saving the first 7 bytes because that's all we use
// TODO extract this into header and assembly files.
unsigned char stackSave[7];

// whether we have turned on an indicator LED
volatile unsigned char isLedIndicatorOn = 0;

void txBinStatus() {
	crc_t crc = crc_init();
	crc = txCrc(START_OF_PACKET, crc);
	crc = txEscapeCrc(CELL_ID_LOW, crc);
	crc = txEscapeCrc(CELL_ID_HIGH, crc);
	crc = txEscapeCrc(iShunt, crc);
	crc = txEscapeCrc(iShunt >> 8, crc);
	crc = txEscapeCrc(vCell, crc);
	crc = txEscapeCrc(vCell >> 8, crc);
	crc = txEscapeCrc(vShunt, crc);
	crc = txEscapeCrc(vShunt >> 8, crc);
	crc = txEscapeCrc(temperature, crc);
	crc = txEscapeCrc(temperature >> 8, crc);
	crc = txEscapeCrc(minCurrent, crc);
	crc = txEscapeCrc(minCurrent >> 8, crc);
	crc = txEscapeCrc(gainPot, crc);
	crc = txEscapeCrc(vShuntPot, crc);
	crc = txEscapeCrc(hasRx, crc);
	crc = txEscapeCrc(softwareAddressing, crc);
	crc = txEscapeCrc(automatic, crc);
	crc = crc_finalize(crc);
	txEscape(crc);
	txEscape(crc >> 8);
}

void txSummary() {
	crc_t crc = crc_init();
	crc = txCrc(START_OF_PACKET, crc);
	crc = txEscapeCrc(CELL_ID_LOW, crc);
	crc = txEscapeCrc(CELL_ID_HIGH, crc);
	crc = txEscapeCrc(iShunt, crc);
	crc = txEscapeCrc(iShunt >> 8, crc);
	crc = txEscapeCrc(vCell, crc);
	crc = txEscapeCrc(vCell >> 8, crc);
	crc = txEscapeCrc(temperature, crc);
	crc = txEscapeCrc(temperature >> 8, crc);
	crc = crc_finalize(crc);
	txEscape(crc);
	txEscape(crc >> 8);
}

void txVersion() {
	crc_t crc = crc_init();
	crc = txCrc(START_OF_PACKET, crc);
	crc = txEscapeCrc(CELL_ID_LOW, crc);
	crc = txEscapeCrc(CELL_ID_HIGH, crc);
	crc = txEscapeCrc(PROTOCOL_VERSION, crc);
	crc = txEscapeCrc(KELVIN_CONNECTION, crc);
#ifdef RESISTOR_SHUNT
	crc = txEscapeCrc(1, crc);
#else
	crc = txEscapeCrc(0, crc);
#endif
#ifdef HARD_SWITCHED_SHUNT
	crc = txEscapeCrc(1, crc);
#else
	crc = txEscapeCrc(0, crc);
#endif
#ifdef HAS_TEMPERATURE_SENSOR
	crc = txEscapeCrc(1, crc);
#else
	crc = txEscapeCrc(0, crc);
#endif
	crc = txEscapeCrc(REVISION_LOW, crc);
	crc = txEscapeCrc(REVISION_HIGH, crc);
	crc = txEscapeCrc(IS_CLEAN, crc);
	crc = txEscapeCrc(PROGRAM_DATE_0, crc);
	crc = txEscapeCrc(PROGRAM_DATE_1, crc);
	crc = txEscapeCrc(PROGRAM_DATE_2, crc);
	crc = txEscapeCrc(PROGRAM_DATE_3, crc);
	crc = crc_finalize(crc);
	txEscape(crc);
	txEscape(crc >> 8);
}

/**
 * Restore the LEDs to the correct state, useful after coming out of suspend.
 *
 * We can light either the red, green or neither LED.
 *
 * If we are shunting, turn on the red LED.
 * If we aren't shunting and the cell is full, turn on the green LED.
 * Otherwise turn off both LEDs.
 *
 * isLedIndicatorOn is designed to cause a dark LED to flash on. If this is true
 * then we invert the logic above.
 *
 */
void restoreLed() {
	if (minCurrent) {
		if (isLedIndicatorOn) {
			setGreen();
		} else {
			setRed();
		}
	} else if (vCell > FULL_VOLTAGE) {
		if (isLedIndicatorOn) {
			setRed();
		} else {
			setGreen();
		}
	} else {
		if (isLedIndicatorOn) {
			setGreen();
		} else {
			ledOff();
		}
	}
}

/** Turn on an LED, see @restoreLed() for how this works */
void setLedIndicator(unsigned char isOn) {
	isLedIndicatorOn = isOn;
	restoreLed();
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

void executeCommand(unsigned char rx) {
	setLedIndicator(1);
	switch (rx) {
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
		case 's' :
			txSummary();
			break;
		case '?' :
			txVersion();
			break;
		case '0' :
			minCurrent = 0;
			break;
		case '3' :
			minCurrent = 150;
			break;
		case '4' :
			minCurrent = 200;
			break;
		case '5' :
			minCurrent = 250;
			break;
		case '6' :
			minCurrent = 300;
			break;
		case '7' :
			minCurrent = 350;
			break;
		case '8' :
			minCurrent = 400;
			break;
		case '9' :
			minCurrent = 450;
			break;
		case '$' :
			automatic = !automatic;
			break;
	}
	setLedIndicator(0);
}

#ifdef SDCC
void interruptHandler(void) __interrupt 0 {
	// SDCC's pic14 port does not save the "stack" so we have to save it.
	// Currently we are only saving the first 7 bytes because that's all we use
	// TODO extract this into header and assembly files.
	__asm
		BANKSEL	STK00
		MOVF    STK00,W
		BANKSEL	_stackSave
		MOVWF	_stackSave

		BANKSEL	STK01
		MOVF    STK01,W
		BANKSEL	(_stackSave + 1)
		MOVWF	(_stackSave + 1)

		BANKSEL	STK02
		MOVF    STK02,W
		BANKSEL	(_stackSave + 2)
		MOVWF	(_stackSave + 2)

		BANKSEL	STK03
		MOVF    STK03,W
		BANKSEL	(_stackSave + 3)
		MOVWF	(_stackSave + 3)

		BANKSEL	STK04
		MOVF    STK04,W
		BANKSEL	(_stackSave + 4)
		MOVWF	(_stackSave + 4)

		BANKSEL	STK05
		MOVF    STK05,W
		BANKSEL	(_stackSave + 5)
		MOVWF	(_stackSave + 5)

		BANKSEL	STK06
		MOVF    STK06,W
		BANKSEL	(_stackSave + 6)
		MOVWF	(_stackSave + 6)
	__endasm;
#else
void interruptHandler(void) {
#endif
	if (TMR1IF) {
		timerOverflow++;
		TMR1IF = 0;
	}
	if (RCIF) {
		unsigned char rx = RCREG;
		hasRx = 1;
		if (rx == ESCAPE_CHARACTER && !escape) {
			escape = 1;
		} else if (rx == START_OF_PACKET && !escape) {
			rxCRC = crc_update(crc_init(), rx);
			state = STATE_WANT_CELL_ID_LOW;
		} else {
			escape = 0;
			switch (state) {
				case STATE_WANT_CELL_ID_LOW :
					if (rx == CELL_ID_LOW) {
						rxCRC = crc_update(rxCRC, rx);
						state = STATE_WANT_CELL_ID_HIGH;
					} else {
						state = WANT_START_OF_PACKET;
					}
					break;
				case STATE_WANT_CELL_ID_HIGH :
					if (rx == CELL_ID_HIGH) {
						rxCRC = crc_update(rxCRC, rx);
						state = STATE_WANT_COMMAND;
					} else {
						state = WANT_START_OF_PACKET;
					}
					break;
				case STATE_WANT_COMMAND :
					state = STATE_WANT_CRC_LOW;
					rxCRC = crc_update(rxCRC, rx);
					rxCRC = crc_finalize(rxCRC);
					command = rx;
					break;
				case STATE_WANT_CRC_LOW :
					if (rx == (unsigned char) (rxCRC & 0x00FF)) {
						state = STATE_WANT_CRC_HIGH;
					} else {
						state = WANT_START_OF_PACKET;
					}
					break;
				case STATE_WANT_CRC_HIGH :
					if (rx == rxCRC >> 8) {
						executeCommand(command);
					}
					state = WANT_START_OF_PACKET;
					break;
				default :
					state = WANT_START_OF_PACKET;
					break;
			}
			//tx(state);
			//crlf();
		}
	}
#ifdef SDCC
	// restore stack, see beginning of interrupt for saving
	// TODO extract this into header and assembly files.
	__asm
		BANKSEL	(_stackSave + 0)
		MOVF	(_stackSave + 0),W
		BANKSEL	STK00
		MOVWF	STK00

		BANKSEL	(_stackSave + 1)
		MOVF	(_stackSave + 1),W
		BANKSEL	STK01
		MOVWF	STK01

		BANKSEL	(_stackSave + 2)
		MOVF	(_stackSave + 2),W
		BANKSEL	STK02
		MOVWF	STK02

		BANKSEL	(_stackSave + 3)
		MOVF	(_stackSave + 3),W
		BANKSEL	STK03
		MOVWF	STK03

		BANKSEL	(_stackSave + 4)
		MOVF	(_stackSave + 4),W
		BANKSEL	STK04
		MOVWF	STK04

		BANKSEL	(_stackSave + 5)
		MOVF	(_stackSave + 5),W
		BANKSEL	STK05
		MOVWF	STK05

		BANKSEL	(_stackSave + 6)
		MOVF	(_stackSave + 6),W
		BANKSEL	STK06
		MOVWF	STK06
	__endasm;
#endif
}

unsigned short getTemperature() {
	unsigned short result = ~(adc(BIN(10010101)) / 1000) & 0x03FF;
	// TODO derive this equation!
	return (10700000l - 11145l * result) / 1000;
}

unsigned short getVCell() {
	unsigned long result = adc(BIN(10000101));
	// TODO derive this equation!
	return 1254400000l / result;
}

unsigned short getVShunt(unsigned short vCell) {
	unsigned long shunt = adc(BIN(10001101));
	// TODO derive this equation!
	return vCell * shunt / 1024 / 1000l;
}

unsigned short getIShunt() {
	unsigned long result = adc(BIN(11011101));
	// TODO derive this equation!
	return 1225l * result / 49l / 10240l;
}

void vddOn() {
	RC0 = 0;	// turn on FET Qx02
	sleep(10);  // give pots time to turn on
	setGainPot(GAIN_POT_OFF);
	setVShuntPot(V_SHUNT_POT_OFF);
}

void setIShunt(unsigned short targetShuntCurrent) {
#ifdef HARD_SWITCHED_SHUNT
	if (targetShuntCurrent == 0) {
		RA2 = 0;
	} else {
		RA2 = 1;
	}
#else
	short difference;
	// if we want zero current, park pots at ..._POT_OFF position
	if (targetShuntCurrent == 0) {
		if (gainPot != GAIN_POT_OFF) {
			setGainPot(GAIN_POT_OFF);
		}
		if (vShuntPot != V_SHUNT_POT_OFF) {
			setVShuntPot(V_SHUNT_POT_OFF);
		}
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
	}
#endif
#endif
}

void halt() {
	minCurrent = 0;
	setIShunt(0);
	red(100);
	green(100);
	ledOff();
	vddOff();
	TMR1IE = 0;			// disable timer interrupt so it doesn't wake us up
	WUE = 1;			// wake up if we receive something (recieve interrupt still enabled)
#ifdef SDCC
	__asm
		sleep
	__endasm;
#endif
	TMR1IE = 1;			// enable timer
	green(10);
	red(10);
	vddOn();
}

int main(void) {
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

	red(150);
	green(150);
	red(150);
	green(150);

	while(1) {
		if (!isVddOn()) {
			vddOn();
		}
		restoreLed();

		mainLoopIShunt = getIShunt();
		mainLoopTemperature = getTemperature();
		mainLoopVCell = getVCell();
		mainLoopVShunt = getVShunt(mainLoopVCell);

		disableInterrupts();
		iShunt = mainLoopIShunt;
		temperature = mainLoopTemperature;
		vCell = mainLoopVCell;
		vShunt = mainLoopVShunt;
		// turn on interrupts
		GIE = 1;

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
