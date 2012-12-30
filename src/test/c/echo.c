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

/* Define processor and include header file. */
#define __16f688
#include "pic14/pic16f688.h"
#include "util.h"

#define RX_BUF_SIZE 16

/* Setup chip configuration */
#ifdef SDCC
typedef unsigned int config;
config __at 0x2007 __CONFIG = _CP_OFF & _CPD_OFF & _BOD_OFF & _PWRTE_ON & _WDT_OFF & _INTRC_OSC_NOCLKOUT & _MCLRE_ON & _FCMEN_OFF & _IESO_OFF;
#endif

void main();
#ifdef SDCC
void interruptHandler(void) __interrupt 0;
#else
void interruptHandler(void);
#endif
void executeCommand(unsigned char rx);
void halt();


volatile char rxBuf[RX_BUF_SIZE];
volatile unsigned char rxStart = 0;
volatile unsigned char rxEnd = 0;

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
		TMR1IF = 0;
	}
	while (rxStart != rxEnd) {
		char rx = rxBuf[rxStart % RX_BUF_SIZE];
		rxStart++;
		tx(rx);
		red(20);
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
		char c;
		for (c = '0'; c < 'Z'; c++) {	
			tx(c);
			sleep(50);
		}
		tx('\r');
		tx('\n');
	}
}

void restoreLed() {
	;
}

