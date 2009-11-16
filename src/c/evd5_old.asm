
;**********************************************************************
;                                                                     
;    Filename:	    EVD5_v1a.asm                                           
;    Date:               3/21/09                                      
;    File Version:      0.10                                              
;                                                                     
;    Author:       Bob Simpson                                                   
;    Company:      Edrive Inc                                                  
;                                                                      
;                                                                     
;**********************************************************************
;                                                                     *
;    Files Required: P16F688.INC                                      *
;                                                                     *
;**********************************************************************


	list		p=16f688		; list directive to define processor
	#include	<P16F688.inc>		; processor specific variable definitions
	
	__CONFIG    _CP_OFF & _CPD_OFF & _BOD_OFF & _PWRTE_ON & _WDT_OFF & _INTRC_OSC_NOCLKOUT & _MCLRE_ON & _FCMEN_OFF & _IESO_OFF


;----------------------------------------------------------------------------
;Constants

SPBRG_VAL	EQU	.12		;set baud rate 2400 for 2Mhz clock
RX_BUF_LEN	EQU	.80		;length of receive buffer
TX_BUF_LEN	EQU	RX_BUF_LEN	;length of transmit buffer


;***** VARIABLE DEFINITIONS

w_temp		EQU	0x7D			; variable used for context saving
STATUS_temp	EQU	0x7E			; variable used for context saving
pclath_temp	EQU	0x7F			; variable used for context saving

RESULTHI	equ		0x20		;A/D results
RESULTLO	equ		0x30

		CBLOCK	0x040
		Flags			;byte to store indicator flags
		RxByteCount		;number of bytes received
		TxByteCount		;number of bytes to transmit
		TxPointer		;pointer to data in buffer
		delcount1		;delay count vars
		delcount2
		Vsource			;var that sets A/D input source
		icount			
		ENDC

		CBLOCK	0x050
		TxBuffer:TX_BUF_LEN	;buffer for data to transmit
		ENDC

		CBLOCK	0x160
		RxBuffer:RX_BUF_LEN	;buffer for data received
		ENDC

;----------------------------------------------------------------------------

Bank0	Macro
		bcf	STATUS, RP0
		bcf	STATUS, RP1
		endm
Bank1	Macro
		bsf	STATUS, RP0
		bcf	STATUS, RP1
		endm
;----------------------------------------------------------------------------

errorlevel -302

;**********************************************************************
	ORG		0x000			; processor reset vector
  	goto		start			; go to beginning of program


	ORG		0x004			; interrupt vector location
	movwf		w_temp			; save off current W register contents
	movf		STATUS,w		; move STATUS register into W register
	movwf		STATUS_temp		; save off contents of STATUS register
	movf		PCLATH,w		; move pclath register into W register
	movwf		pclath_temp		; save off contents of PCLATH register


; isr code can go here or be located as a call subroutine elsewhere

;	movf		pclath_temp,w		; retrieve copy of PCLATH register
;	movwf		PCLATH			; restore pre-isr PCLATH register contents	
;	movf		STATUS_temp,w		; retrieve copy of STATUS register
;	movwf		STATUS			; restore pre-isr STATUS register contents
;	swapf		w_temp,f
;	swapf		w_temp,w		; restore pre-isr W register contents
;	retfie					; return from interrupt


start


		BANKSEL	OSCCON

		movlw	b'01010000'		;set internal clock for 2MHz freq
		movwf	OSCCON

;----------------------------------------------------------------------------
;Set up serial port and buffers.

	BANKSEL	SPBRG			;select bank 1

		movlw	.12			;set baud rate
		movwf	SPBRG
		movlw	0x24		;enable transmission and high baud rate
		movwf	TXSTA
	
		movlw	0x90		;enable serial port and reception
		movwf	RCSTA
		clrf	TxByteCount	;clear transmit buffer data count
		clrf	RxByteCount	;clear receive buffer data count
		clrf	Flags		;clear all flags
	
;----------------------------------------------------------------------------

;		Port A setup:
;	RA0		Digital output	/CS1
;	RA1		Analog input	Vref source	1.22V
;	RA2		Digital output	/CS0
;	RA3		/MCLR/Vpp
;	RA4		Analog input	Vcell
;	RA5		Digital output	up/down control on digital pots
;  	  port A pins = xx543210
;    bit settings = 00011010 	;1=input, 0=output

		BANKSEL	PORTA
	;	movlw	b'11011010'
	;	movwf	PORTA		;write the port A settings
		CLRF	PORTA
		movlw	07h			;set to digital I/O and
		movwf	CMCON0		;turn off comparitors
		BANKSEL	ANSEL
		movlw	b'10101010'
		movwf	ANSEL
		movlw	b'11011010'
		movwf	TRISA

;		Port C setup:
;	RC0		Digital output	pwr control for Switched_Vdd
;	RC1		Analog input	Tcell
;	RC2		Digital output	LED indicator High=red low =Green (with opposite RA5 state)
;	RC3		Analog input	Ishunt (voltage across 0.25 ohms)
;	RC4		Digital output	TX
;	RC5		Digital input	RX
;  	  port C pins = xx543210
;    bit settings = 11101010

;		BANKSEL PORTC
;		movlw	b'11101010'
;		movwf	PORTC		;write the port C settings
;		BANKSEL	ANSEL
;		movlw	b'10101010'
;		movwf	ANSEL
		movlw	b'11101010'
		movwf	TRISC		


;		ANSEL register setup: (
; 	AN0	 output
;	AN1	 analog input AN1	Vreference
;	AN2	 output
;	AN3	 analog input AN3	Shunt voltage reference
; 	AN4	 output
;	AN5	 analog input AN5	Cell temp divider
;	AN6	 output
;	AN7	 analog input AN7	Ishunt (v across 0.1 ohm)
;  	     reg bits = 76543210
;    bit settings = 10101010 	
;______________________________________
;		ADCON1	A/D control register functions:
;	bits 4-6	set to b'100 for Fosc/4 (other bits set to 0)
;	 
		BANKSEL	ADCON1
		movlw	b'01000000'
		movwf	ADCON1		;sets clock source


;		ADCON0	A/D control register functions:
;	bit 0	ADON	1=ADC is enabled, 0=ADC disabled and no power used
;	bit 1	Go/Done STATUS bit	setting to 1 starts conv. (auto cleared)
;	bits 2-4	3 bit channel select 
;	bit 5	not used
;	bit 6	Vcfg	Voltage reference pin (vdd=0 or Vref=1)
;	bit 7	ADFM format type, select right justified = 1
;  	  port A pins = 76543210
;    bit settings = 10000101 for reading Vref  (1.22volt reference)
;    bit settings = 10001101 for reading Vdiv3 (for cell voltage
;    bit settings = 10010101 for reading Vntcr (for cell temp)

Vrefsrc		EQU		b'10000101'
Vcellsrc	EQU		b'10001101'
Vtempsrc	EQU		b'10010101'



;power up vref and res dividers
		BANKSEL	PORTC 
		bcf		PORTC,0		;turn on FET Qx01
;_____________________________________________________________________
;	Main code 
;
;		The Vcell result is calculated like this:
;			(1024*1.225V)/(1024-a/d_result)=Vcell
;_____________________________________________________________________

main

    	call	sred
		call	green
		call	shunt_init
		call	red
		call	sgreen

		BANKSEL	PORTC 
		bsf		PORTC,0		;turn off VDD_switch FET Qx01
		
	;check for incoming char

idle	    Bank0
		btfss	PIR1,5
		goto	idle	;check for a received char

		movf	RCREG, W	;get received data
		xorlw	0x63		;check for a "c" char (Vcell command)
		btfss	STATUS, Z	;check if same	
		goto	nextch1		;no, go check the next
		call	Vcell
	;	call	crlf
		call	sgreen
		goto	idle

nextch1	movf	RCREG, W	;get received data	
		xorlw	0x73		;check for an "s" (Vshunt command)
		btfss	STATUS, Z	;check if same
		goto	nextch2		;no, go check the next
		call	Vshunt
	;	call	crlf
		call	sred
		goto	idle		

nextch2	movf	RCREG, W	;get received data	
		xorlw	0x74		;check for a "t" (Vtemp command)
		btfss	STATUS, Z	;check if same
		goto	nextch3		;no, go check the next
		call	Vtemp
	;	call	crlf
		call 	sred
		goto	idle		

nextch3	movf	RCREG, W	;get received data	
		xorlw	0x75		;check for a "u" (inc Vshunt command)
		btfss	STATUS, Z	;check if same
		goto	nextch4		;no, go check the next
		call	incA
		call 	sgreen
		call	sgreen
		goto	idle

nextch4	movf	RCREG, W	;get received data	
		xorlw	0x64		;check for a "d" (dec Vshunt command)
		btfss	STATUS, Z	;check if same
		goto	nextch5		;no, go check the next
		call	decA
		call	sred
		goto	idle

nextch5	movf	RCREG, W	;get received data	
		xorlw	0x31		;check for a "1" (inc Vgain command)
		btfss	STATUS, Z	;check if same
		goto	nextch6		;no, go check the next
		call	incB
		call	sgreen
		goto	idle

nextch6	movf	RCREG, W	;get received data	
		xorlw	0x32		;check for a "2" (dec Vgain command)
		btfss	STATUS, Z	;check if same
		goto	nextch7		;no, go check the next
		call	decB
		call	sred
		call	sgreen
		goto	idle

nextch7	movf	RCREG, W	;get received data	
		xorlw	0x69		;check for a "i" (shunt current)
		btfss	STATUS, Z	;check if same
		goto	nextch8		;no, go check the next
		call	Ishunt
		call	sgreen
		goto	idle

nextch8	movf	RCREG, W	;get received data	
		xorlw	0x70		;check for a "p" (powerup Vdd rail)
		btfss	STATUS, Z	;check if same
		goto	nextch9		;no, go get another
		BANKSEL	PORTC 
		bcf		PORTC,0		;turn on FET Qx02
		call	red
		call	green
		call	shunt_init	;setup Shunt HW to keep mosfet off
		goto	idle

nextch9	movf	RCREG, W	;get received data	
		xorlw	0x6f		;check for a "o" (power off Vdd rail)
		btfss	STATUS, Z	;check if same
		goto	nextch10		;no, go get another
		BANKSEL	PORTC 
		bsf		PORTC,0		;turn off FET Qx02
		call	sred
		call	sgreen
		goto	idle

nextch10	movf	RCREG, W	;get received data	
			xorlw	0x67		;check for a "g" (flash LEDs command)
			btfss	STATUS, Z	;check if same
			goto	nextch11	;no, go get another
			call	green
			goto	idle

nextch11	movf	RCREG, W	;get received data	
			xorlw	0x72		;check for a "r" (flash LEDs command)
			btfss	STATUS, Z	;check if same
			goto	idle		;no, go get another
			call	red
			goto	idle

;__________________________________________________________________________		
; misc subroutines

decA	bsf		PORTA,5		;set U/D high for increment
		bcf		PORTA,2		;set /CS0 low (active)
		bcf		PORTA,5		;pulse low
		bsf		PORTA,5		;back to high
		bsf		PORTA,2		;release chip select CS0 (set high)
		bcf		PORTA,5		;back to low (to not leave the green led on)		
;		call	green
		return
			
incA	bcf		PORTA,5		;set U/D low for decrement
		bcf		PORTA,2		;set /CS0 low (active)
		bsf		PORTA,5		;pulse high
		bcf		PORTA,5		;back to low
		bsf		PORTA,2		;release chip select CS0 (set high)
;		call	red
		return

incB	bsf		PORTA,5		;set U/D high for increment
		bcf		PORTA,0		;set /CS1 low (active)
		bcf		PORTA,5		;pulse low
		bsf		PORTA,5		;back to high
		bsf		PORTA,0		;release chip select CS1 (set high)
		bcf		PORTA,5		;back to low (to not leave the green led on)
;		call	green
		return
			
decB	bcf		PORTA,5		;set U/D low for decrement
		bcf		PORTA,0		;set /CS1 low (active)
		bsf		PORTA,5		;pulse high
		bcf		PORTA,5		;back to low
		bsf		PORTA,0		;release chip select CS1 (set high)
;		call	red
		return



red 	bsf		PORTC,2		;turn on red LED
		bcf		PORTA,5
		Call 	delay
		bcf		PORTC,2		;turn it off now
		return

green	bsf		PORTA,5		;turn on green LED
		bcf		PORTC,2
		call 	delay
		bcf		PORTA,5		;turn off LED
		return	

sred	bsf		PORTC,2		;turn on red LED
		bcf		PORTA,5
		call 	sdelay
		bcf		PORTC,2		;turn off LED
		return	

sgreen	bsf		PORTA,5		;turn on green LED
		bcf		PORTC,2
		Call 	sdelay
		bcf		PORTA,5		;turn it off now
		return


	;___Shunt current_________
Ishunt	Bank0
		movlw	0x49
		movwf	TXREG		;send a char I
		call	txwait

		movlw	0x73
		movwf	TXREG		;send a char s
		call	txwait

		movlw	0x3D
		movwf	TXREG		;send a char =
		call	txwait
	
		BANKSEL	ADCON0
		movlw	b'10011101' 		;select I shunt (voltage, across Rx02
		movwf	ADCON0			
		nop
		nop

		bsf		ADCON0,1		;start conversion	
loopI	btfsc	ADCON0,1		; check if done?
		goto 	loopI 			;No, test again

		Bank0
	;	movf	ADRESH,W
		comf	ADRESH,0
		movwf	RESULTHI			;store upper 2 bits in mem

		Bank1
	;	movf	ADRESL,W
		comf	ADRESH,0
		Bank0
		movwf	RESULTLO			;store lower 8 bits in mem

		call	ADout		;go convert and send V ref voltage out

		return

	;___Cell voltage_________
Vcell	Bank0
		movlw	0x56
		movwf	TXREG		;send a char V
		call	txwait

		movlw	0x63
		movwf	TXREG		;send a char c
		call	txwait

		movlw	0x3D
		movwf	TXREG		;send a char =
		call	txwait
	
		BANKSEL	ADCON0
		movlw	b'10000101' 		;set to 1.2V ref
		movwf	ADCON0			
		nop

		bsf		ADCON0,1		;start conversion	
loopR	btfsc	ADCON0,1		; check if done?
		goto 	loopR 			;No, test again

		Bank0
		movf	ADRESH,W
		movwf	RESULTHI			;store upper 2 bits in mem

		Bank1
		movf	ADRESL,W
		Bank0
		movwf	RESULTLO			;store lower 8 bits in mem

		call	ADout		;go convert and send V ref voltage out

		return


	;___Shunt voltage value _________
Vshunt	Bank0
		movlw	0x56
		movwf	TXREG		;send a char V
		call	txwait

		movlw	0x73
		movwf	TXREG		;send a char s
		call	txwait

		movlw	0x3D
		movwf	TXREG		;send a char =
		call	txwait
	
		BANKSEL	ADCON0
		movlw	b'10001101' 		;set to AN3 input
		movwf	ADCON0			
		nop

		bsf		ADCON0,1		;start conversion	
loopV	btfsc	ADCON0,1		; check if done?
		goto 	loopV 			;No, test again

		Bank0
		movf	ADRESH,W
		movwf	RESULTHI			;store upper 2 bits in mem

		Bank1
		movf	ADRESL,W
		Bank0
		movwf	RESULTLO			;store lower 8 bits in mem

		call	ADout		;go convert and send V ref voltage out

		return

	;___V temp_________
Vtemp	Bank0	
		movlw	0x56
		movwf	TXREG		;send a char V
		call	txwait

		movlw	0x74
		movwf	TXREG		;send a char t
		call	txwait

		movlw	0x3D 
		movwf	TXREG		;send a char =
		call	txwait

		BANKSEL	ADCON0
		movlw	b'10010101' 		;set to Cell temp sensor
		movwf	ADCON0			
		nop

		bsf		ADCON0,1		;start conversion	
loopT	btfsc	ADCON0,1		; check if done?
		goto 	loopT 			;No, test again

		Bank0
		movf	ADRESH,W
		movwf	RESULTHI			;store upper 2 bits in mem

		Bank1
		movf	ADRESL,W
		Bank0
		movwf	RESULTLO			;store lower 8 bits in mem

		call	ADout		;go convert and send V ref voltage out

		return


;_________subroutines_________________________________________


crlf	movlw	0x0D
		movwf	TXREG		;send a carriage return
		call	txwait

		movlw	0x0A
		movwf	TXREG		;send a line feed
		call	txwait
	
		return




;___________________________________________________________________
; output binary value to UART

		;____bit 10:_____________________________
ADout:	Bank0
		BTFSS	RESULTHI,1
		goto	outputL10
		goto	outputH10

outputL10:				;output 10th bit of A/D result
		call	send0
		goto	next9
outputH10:
		call	send1		
		;____bit 9:_____________________________
next9	
		BTFSS	RESULTHI,0
		goto	outputL9
		goto	outputH9

outputL9:				;output 9th bit char of A/D result
		call	send0
		goto	next8
outputH9:
		call	send1
		;____bit 8:_____________________________
next8		
		BTFSS	RESULTLO,7
		goto	outputL8
		goto	outputH8

outputL8:				;output 8th bit char of A/D result
		call	send0
		goto	next7
outputH8:
		call	send1
		;____bit 7:_____________________________
next7		
		BTFSS	RESULTLO,6
		goto	outputL7
		goto	outputH7

outputL7:				;output 7th bit char of A/D result
		call	send0
		goto	next6
outputH7:
		call	send1
		;____bit 6:_____________________________
next6		
		BTFSS	RESULTLO,5
		goto	outputL6
		goto	outputH6

outputL6:				;output 6th bit char of A/D result
		call	send0
		goto	next5
outputH6:
		call	send1
		;____bit 5:_____________________________
next5		
		BTFSS	RESULTLO,4
		goto	outputL5
		goto	outputH5

outputL5:				;output 5th bit char of A/D result
		call	send0
		goto	next4
outputH5:
		call	send1
		;____bit 4:_____________________________
next4		
		BTFSS	RESULTLO,3
		goto	outputL4
		goto	outputH4

outputL4:				;output 4th bit char of A/D result
		call	send0
		goto	next3
outputH4:
		call	send1
		;____bit 3:_____________________________
next3		
		BTFSS	RESULTLO,2
		goto	outputL3
		goto	outputH3

outputL3:				;output 3rd bit char of A/D result
		call	send0
		goto	next2
outputH3:
		call	send1
		;____bit 2:_____________________________
next2		
		BTFSS	RESULTLO,1
		goto	outputL2
		goto	outputH2

outputL2:				;output 2nd bit char of A/D result
		call	send0
		goto	next1
outputH2:
		call	send1
		;____bit 1:_____________________________
next1		
		BTFSS	RESULTLO,0
		goto	outputL1
		goto	outputH1
outputL1:				;output 1st bit char of A/D result
		call	send0
		goto	next0
outputH1:
		call	send1

next0	movlw	0x0D
		movwf	TXREG		;send a carriage return
		call	txwait

		movlw	0x0A
		movwf	TXREG		;send a line feed
		call	txwait

	;   	call	delay

		return

;___________________________________________________________________
;	Init Shunt Hardware
;   This needs set so there is no drive to the shunt MOSFET


shunt_init
		movlw	0x40
		movwf	icount

iloop	Call	decA
		Call	incB

    	decfsz	icount
		goto	iloop
		
		return

;___________________________________________________________________



txwait:	
		BANKSEL	TXSTA
		BTFSS	TXSTA,1		;check if sill not empty
		goto 	txwait		;not empty yet 
		return				;now is empty


send1:
		movlw	0x30
		movwf	TXREG		;send out a 0 char
		call	txwait
		return
send0:
		movlw	0x31
		movwf	TXREG		;send out a 1 char
		call	txwait
		return
		
delay	;
		movlw	0xFF
		movwf	delcount2
bloop	movlw	0xAF
		movwf	delcount1

dloop	decfsz	delcount1
		goto	dloop
		
		decfsz	delcount2
		goto	bloop
		return

sdelay						;short delay
		movlw	0xFF
		movwf	delcount2
sloop	movlw	0x10
		movwf	delcount1

eloop	decfsz	delcount1
		goto	eloop
		
		decfsz	delcount2
		goto	sloop
		return

ErrTxBufOver:	
		clrf	RxByteCount			;reset received byte count
		bcf	 	Flags,0		;clear indicator for <CR> received
		return

;Currently not used
send_data:
		Bank0
		movlw	w_temp
		movwf	TXREG		;send a char R
		call	txwait

		movlw	0x0D
		movwf	TXREG		;send a carraige return
		call	txwait

		movlw	0x0A
		movwf	TXREG		;send a line feed
		call	txwait

		return
;________________________________________________________________________________

	ORG	0x2100				; data EEPROM location
	DE	1,2,3,4				; define first four EEPROM locations as 1, 2, 3, and 4




	END                       ; directive 'end of program'
