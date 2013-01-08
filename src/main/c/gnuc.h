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

// This file contains some hackery to make SDCC's code compile with GCC so it's easier to develop in eclipse.

#ifdef __GNUC__
#define __sfr char
#define __at(x)
#define at(x)


 __sfr  __at (INDF_ADDR)                    INDF;
 __sfr  __at (TMR0_ADDR)                    TMR0;
 __sfr  __at (PCL_ADDR)                     PCL;
 __sfr  __at (STATUS_ADDR)                  STATUS;
 __sfr  __at (FSR_ADDR)                     FSR;
 __sfr  __at (PORTA_ADDR)                   PORTA;

 __sfr  __at (PORTC_ADDR)                   PORTC;

 __sfr  __at (PCLATH_ADDR)                  PCLATH;
 __sfr  __at (INTCON_ADDR)                  INTCON;
 __sfr  __at (PIR1_ADDR)                    PIR1;

 __sfr  __at (TMR1L_ADDR)                   TMR1L;
 __sfr  __at (TMR1H_ADDR)                   TMR1H;
 __sfr  __at (T1CON_ADDR)                   T1CON;
 __sfr  __at (BAUDCTL_ADDR)                 BAUDCTL;
 __sfr  __at (SPBRGH_ADDR)                  SPBRGH;
 __sfr  __at (SPBRG_ADDR)                   SPBRG;
 __sfr  __at (RCREG_ADDR)                   RCREG;
 __sfr  __at (TXREG_ADDR)                   TXREG;
 __sfr  __at (TXSTA_ADDR)                   TXSTA;
 __sfr  __at (RCSTA_ADDR)                   RCSTA;
 __sfr  __at (WDTCON_ADDR)                  WDTCON;
 __sfr  __at (CMCON0_ADDR)                  CMCON0;
 __sfr  __at (CMCON1_ADDR)                  CMCON1;

 __sfr  __at (ADRESH_ADDR)                  ADRESH;
 __sfr  __at (ADCON0_ADDR)                  ADCON0;


 __sfr  __at (OPTION_REG_ADDR)              OPTION_REG;

 __sfr  __at (TRISA_ADDR)                   TRISA;
 __sfr  __at (TRISC_ADDR)                   TRISC;

 __sfr  __at (PIE1_ADDR)                    PIE1;

 __sfr  __at (PCON_ADDR)                    PCON;
 __sfr  __at (OSCCON_ADDR)                  OSCCON;
 __sfr  __at (OSCTUNE_ADDR)                 OSCTUNE;
 __sfr  __at (ANSEL_ADDR)                   ANSEL;

 __sfr  __at (WPU_ADDR)                     WPU;
 __sfr  __at (WPUA_ADDR)                    WPUA;
 __sfr  __at (IOC_ADDR)                     IOC;
 __sfr  __at (IOCA_ADDR)                    IOCA;
 __sfr  __at (EEDATH_ADDR)                  EEDATH;
 __sfr  __at (EEADRH_ADDR)                  EEADRH;
 __sfr  __at (VRCON_ADDR)                   VRCON;
 __sfr  __at (EEDAT_ADDR)                   EEDAT;
 __sfr  __at (EEDATA_ADDR)                  EEDATA;
 __sfr  __at (EEADR_ADDR)                   EEADR;
 __sfr  __at (EECON1_ADDR)                  EECON1;
 __sfr  __at (EECON2_ADDR)                  EECON2;
 __sfr  __at (ADRESL_ADDR)                  ADRESL;
__sfr  __at (ADCON1_ADDR)                  ADCON1;

#include "pic14/pic16f688.h"
volatile __PORTAbits_t __at(PORTA_ADDR) PORTAbits;
volatile __PORTCbits_t __at(PORTC_ADDR) PORTCbits;
volatile __ADCON0bits_t __at(ADCON0_ADDR) ADCON0bits;
volatile __EECON1bits_t __at(EECON1_ADDR) EECON1bits;
volatile __INTCONbits_t __at(INTCON_ADDR) INTCONbits;
volatile __PIR1bits_t __at(PIR1_ADDR) PIR1bits;
volatile __PIE1bits_t __at(PIE1_ADDR) PIE1bits;
volatile __BAUDCTLbits_t __at(BAUDCTL_ADDR) BAUDCTLbits;
volatile __RCSTAbits_t __at(RCSTA_ADDR) RCSTAbits;
volatile __T1CONbits_t __at(T1CON_ADDR) T1CONbits;
volatile __TXSTAbits_t __at(TXSTA_ADDR) TXSTAbits;

#endif
