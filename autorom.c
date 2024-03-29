/*-
 * Copyright (c) 2005-2020 Poul-Henning Kamp
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "rc3600.h"

/* From RCSL 52-AA894, Appendix A */
static uint16_t autorom_appendix_a[32] = {
			// ; 78.04.03 KNEH.
			// ; PROGRAM TO INITIALIZE CPU711 CONSOLE
			// ; INTERFACE AND TO WRITE CORRECT PARITY
			// ; IN ALL MEMORY LOCATIONS
			//		.LOC	0
/* 00000 */ 0060477,	//		READS	0	; READ SWITCHES<BAUD RATE
/* 00001 */ 0101220,	//		MOVZR	0,0	; AND NO. OF STOP BITS>
/* 00002 */ 0024033,	//		LDA	1,MODF
/* 00003 */ 0107000,	//		ADD	0,1
/* 00004 */ 0066011,	//		DOB	1,TTO	; SET MODE1
/* 00005 */ 0101300,	//		MOVS	0,0
/* 00006 */ 0024034,	//		LDA	1,C17
/* 00007 */ 0107620,	//		ANDZR	0,1
/* 00010 */ 0030035,	//		LDA	2,C8
/* 00011 */ 0133000,	//		ADD	1,2
/* 00012 */ 0025000,	//		LDA	1,0,2
/* 00013 */ 0030033,	//		LDA	2,MODE
/* 00014 */ 0125002,	//		MOV	1,1,SZC
/* 00015 */ 0125300,	//		MOVS	1,1
/* 00016 */ 0147300,	//		ADDS	2,1
/* 00017 */ 0066011,	//		DOB	1,TTO	; SET MODE2
/* 00020 */ 0024036,	//		LDA	1,COMN
/* 00021 */ 0067011,	//		DOC	1,TTO	; SET COMMAND
/* 00022 */ 0030035,	//		LDA	2,C40	; RESET MEM
/* 00023 */ 0051000,	//		STA	2,0,2
/* 00024 */ 0151404,	//		INC	2,2,SZR
/* 00025 */ 0000023,	//		JMP	.-2
/* 00026 */ 0062677,	//		IORST
/* 00027 */ 0007402,	// BAUDT:	007402		; 19200BPS,110BPS
/* 00030 */ 0002406,	//		2406		; 300BPS,600BPS
/* 00031 */ 0004012,	//		4012		; 1200BPS,2400BPS
/* 00032 */ 0006016,	//		6016		; 4800BPS,9600BPS
/* 00033 */ 0030116,	// MODE:	30166		; MODE2,MODE1
/* 00034 */ 0000017,	// C17:		17
/* 00035 */ 0000027,	// C8:		BAUDT
/* 00036 */ 0000047,	// COMN:	47		; COMMAND
	    0000035,	// C40=		C8
};

/* From RCSL 52-AA894, Appendix C */
static uint16_t autorom_appendix_c[32] = {
			// . LOC	0
/* 00000 */ 0060477,	// BEG:		READS	0	; READ SWITCHES.
/* 00001 */ 0101102,	//		MOVZL	0,0,SZC	; TEST THE STATE OF SWITCH0.
/* 00002 */ 0000011,	//		JMP	PATT
/* 00003 */ 0060110,	// ECHO:	NIOS	TTI
/* 00004 */ 0063610,	//		SKPDN	TTI
/* 00005 */ 0000004,	//		JMP	.-1
/* 00006 */ 0060610,	//		DIAC	0,TTI
/* 00007 */ 0004025,	//		JSR	OUT	; OUTPUT THE RECEIVED
/* 00010 */ 0000003,	//		JMP	ECHO	; CHARACTER

/* 00011 */ 0020031,	// PATT:	LDA	0,C80	; SET CHAR COUNTER.
/* 00012 */ 0040000,	//		STA	0,0
/* 00013 */ 0020034,	//		LDA	0,CSPACE
/* 00014 */ 0004025,	// LOOP:	JSR	OUT
/* 00015 */ 0101400,	//		INC	0,0
/* 00016 */ 0014000,	//		DSZ	0
/* 00017 */ 0000014,	//		JMP	LOOP
/* 00020 */ 0020033,	//		LDA	0,CCR
/* 00021 */ 0004025,	//		JSR	OUT	; OUTPUT NEW LINE
/* 00022 */ 0020032,	//		LDA	0,CNL
/* 00023 */ 0004025,	//		JSR	OUT	; OUTPUT CARRIAGE RETURN
/* 00024 */ 0000011,	//		JMP	PATT

/* 00025 */ 0061111,	// OUT:		DOAS	0,TTO	; ROUTINE TO OUTPUT
/* 00026 */ 0063611,	//		SKPDN	TTO	; A CHARACTER TO TTO
/* 00027 */ 0000026,	//		JMP	.-1
/* 00030 */ 0001400,	//		JMP	0,3

/* 00031 */ 0000120,	// C80:		120
/* 00032 */ 0000012,	// CNL:		12
/* 00033 */ 0000015,	// CCR:		15
/* 00034 */ 0000040,	// CSPACE:	40
};

/* From RCSL 52-AA894, Appendix D */
static uint16_t autorom_appendix_d[32] = {
			// . LOC	0
/* 00000 */ 0060477,	// BEG:		READS	0	; READ SWITCHES into AC0
/* 00001 */ 0105120,	//		MOVZL	0,1	; ISOLATE DEVICE CODE
/* 00002 */ 0124240,	//		COMOR	1,1	; -DEVICE CODE - 1
/* 00003 */ 0010011,	// LOOP:	ISZ	OP1	; COUNT DEVICE CODE INTO ALL
/* 00004 */ 0010031,	//		ISZ	OP2	; IO INSTRUCTIONS
/* 00005 */ 0010033,	//		ISZ	OP3	;
/* 00006 */ 0010014,	//		ISZ	OP4	;
/* 00007 */ 0125404,	//		INC	1,1,SZR	; DONE?
/* 00010 */ 0000003,	//		JMP	LOOP	; NO INCREMENT AGAIN

/* 00011 */ 0060077,	// OP1:		060077		; START DEVICE; (NIOS 0) - 1
/* 00012 */ 0030017,	//		LDA	2,C377	; YES, PUT JMP 377 INTO LOCATION 377
/* 00013 */ 0050377,	//		STA	2,377	;
/* 00014 */ 0063377,	// OP4:		063377		; BUSY?: (SKPBN 0) - 1
/* 00015 */ 0000011,	//		JMP	OP1	; NO, GO TO OP1
/* 00016 */ 0101102,	//		MOVL	0,0,SZC	; LOW SPEED DEVICE? (TEST SWITCH 0)
/* 00017 */ 0000377,	//		JMP	377	; NO, GO TO 377 AND WAIT FOR CHANNEL
/* 00020 */ 0004031,	// LOOP2:	JSR	GET+1	; GET A FRAME
/* 00021 */ 0101065,	//		MOVC	0,0,SNR	; IS IT NONZERO?
/* 00022 */ 0000020,	//		JMP	LOOP2	; NO, IGNORE AND GET ANOTHER
/* 00023 */ 0004030,	// LOOP4:	JSR	GET	; YES, GET A FULL WORD
/* 00024 */ 0046027,	//		STA	1,@C77	; STORE STARTING AT 100
/* 00025 */ 0010100,	//		ISZ	100	; COUNT WORD - DONE?
/* 00026 */ 0000023,	//		JMP	LOOP4	; NO, GET ANOTHER
/* 00027 */ 0000077,	// C77:		JMP	77	; YES - LOCATION COUNTER AND
			//				; JMP TO LAST WORD
/* 00030 */ 0126420,	// GET:		SUBZ	1,1	; CLEAR AC1, SET CARRY
			// OP2:
/* 00031 */ 0063577,	// LOOP3:	063577		; DONE?: (SKPDN 0) - 1
/* 00032 */ 0000031,	//		JMP	LOOP3	; NO, WAIT
/* 00033 */ 0060477,	//		060477		; YES, READ INTO AC0 (DIAS 0,0) - 1
/* 00034 */ 0107363,	//		ADDCS	0,1,SNC	; ADD 2 FRAMES SWAPPED - GOT SECOND?
/* 00035 */ 0000031,	//		JMP	LOOP3	; NO, GO BACK AFTER IT
/* 00036 */ 0125300,	//		MOVS	1,1	; YES, SWAP AC1
/* 00037 */ 0001400,	//		JMP	0,3	; RETURN WITH FULL WORD
};

/* From RCSL 52-AA894, Appendix 3 */
static uint16_t autorom_appendix_e[32] = {
			// ; CARD READER PROGRAM LOAD II
			// ; FOR LOADING OF PROGRAMS FROM A CARD READER CONNECTED
			// ; TO CRC 705 OR EQUIVALENT
			// . LOC	0
/* 00000 */ 0020006,	//		LDA	0,SA	; GET ADDRESS FOR CARD BUFFER
/* 00001 */ 0004007,	//		JSR	GETCD	; GET ONE CARD
/* 00002 */ 0004022,	//		JSR	CONV	; CONVERT ONE CARD TO WORDS
/* 00003 */ 0020110,	//		LDA	0,110	; GET CARD NUMBER
/* 00004 */ 0142004,	//		ADC	2,0,SZR	; CHECK FOR CARD NUMBER 1
/* 00005 */ 0063077,	//		HALT		; CARD NUMBER ERROR
/* 00006 */ 0000041,	// SA:		JMP	41	; GO TO PRE-LOADER
/* 00007 */ 0062016,	// GETCD:	DOB	0,CDR	; OUTPUT BUFFER ADDRESS
/* 00010 */ 0061116,	//		DOAS	0,CDR	; OUTPUT READ COMMAND
/* 00011 */ 0063516,	//		SKPBN	CDR	; CHECK FOR THE READER IS STARTED
/* 00012 */ 0000010,	//		JMP	.-1	; NO, TRY AGAIN
/* 00013 */ 0063516,	//		SKPBZ	CDR	; WAIT FOR COMPLETATION OF
/* 00014 */ 0000013,	//		JMP	.-1	; READING A CARD
/* 00015 */ 0001400,	//		JMP	0,3	; RETURN
			// ; VARIABLES
/* 00016 */ 0000000,	// COUNT:	0
/* 00017 */ 0177730,	// M50:		-50
/* 00020 */ 0000040,	// ADDR1:	40
/* 00021 */ 0000040,	// ADDR2:	40
			// ; CONVERTING SUBROUTINE
/* 00022 */ 0152400,	// CONV:	SUB	2,2	; CLEAR AC2
/* 00023 */ 0020017,	//		LDA	0,M50	; GET COUNT FOR NUMBER OF WORDS
/* 00024 */ 0040016,	//		STA	0,COUNT	; STORE COUNT
/* 00025 */ 0022020,	// LOOP:	LDA	0,@ADDR1; GET LEFT BYTE
/* 00026 */ 0101300,	//		MOVS	0,0	; SWAP BYTE
/* 00027 */ 0026020,	//		LDA	1,@ADDR1; GET RIGHT BYTE
/* 00030 */ 0107000,	//		ADD	0,1	; COMPUTE WORD
/* 00031 */ 0046021,	//		STA	1,@ADDR2; STORE WORD
/* 00032 */ 0133000,	//		ADD	1,2	; COMPUTER CHECKSUM
/* 00033 */ 0010016,	//		ISZ	COUNT	; CHECK FOR MORE WORDS
/* 00034 */ 0000025,	//		JMP	LOOP	; YES, GET THEM
/* 00035 */ 0151004,	//		MOV	2,2,SZR	; CHECK FOR CHECKSUM ERROR
/* 00036 */ 0063077,	//		HALT		; CHECKSUM ERROR
/* 00037 */ 0001400,	//		JMP	0,3	; RETURN
};

/* From RCSL 52-AA894, Appendix F */
static uint16_t autorom_appendix_f[32] = {
			// ; PROGRAM LOAD, FLEXIBLE DISC, HKM 75.11.01
			// ;  THIS PROGRAM LOAD RESIDES IN 32x16 ROM.
			// ;  IT IS DESIGNED FOR FLEXIBLE DISC AS PRIMARY LOAD MEDIUM
			// ;     AND USES MOVING HEAD DISC OR MAGTAPE AS SECONDARY
			// ;     LOAD MEDIUM.
			// ;
			// ;  FLEXIBLE DISC: SWITCH(0) = 0, SWITCH(1:15) - NOT USED,
			// ;		     THE DISC IS RECALIBRATED BY THE PROGRAM
			// ;  MAGTAPE:
			// ;  MOVING HEAD DISC: SWITCH(0) = 1, SWITCH(1:9) = 0
			// ;		     SWITCH(10:15) = DEVICE NUMBER,
			// ;		     BOTH DISC AND MAGTAPE MUST BE RECALIBRATED
			// ;		     BEFORE ACTIVATING THE PROGRAM LOAD
			// ;
			// ;  IN CASE OF MAGTAPE OF FLEXIBLE DISC THE LOAD WAITS UNTIL
			// ;     THE SELECTED DEVICE IS READY FOR COMMANDS.
			// .LOC		0
			// FLEX=	61		; FLEXIBLE DISC
/* 00000 */ 0070477,	//		READS	2	; READ SWITCHES(S);
/* 00001 */ 0150122,	//		COMZL	2,2,SZC	; IF S(0) == 0 THEN
/* 00002 */ 0000026,	//		JMP	FD	;   CARRY:= TRUE AND GOTO FLOP
/* 00003 */ 0151240,	//		MOVOR	2,2	; NOT FLOPPY: DEVICE:= OCT(77);
/* 00004 */ 0010010,	// LOOP:	ISZ	OP1	; FOR DEVICE INDEX:=S(1:15)-1
/* 00005 */ 0010013,	//		ISZ	OP2	;   STEP 1 UNTIL 0 DO
/* 00006 */ 0151404,	//		INC	2,2,SZR	;   DEVICE:= DEVICE +1
/* 00007 */ 0000004,	//		JMP	LOOP	; FOR FURTHER COMMENTS SEE OP1
/* 00010 */ 0071077,	// OP1:	071077			; DOAS 2 <DEV> -1 :INCREMENTS:
/* 00011 */ 0024015,	//		LDA	1,.377	; LOAD "JMP .+0" INTO LAST WORD
/* 00012 */ 0044377,	//		STA	1,377	;   OF PAGE ZERO
/* 00013 */ 0063377,	// OP2:	0633377			; SKPBN <DEV> -1 :INCREMENTS:
/* 00014 */ 0000010,	//		JMP	OP1	; READ FIRST BLOCK, WAIT UNTIL
			//				;   COMMAND IS ACCEPTED
/* 00015 */ 0000377,	// .377:	JMP	377	; GOTO WAIT BLOCK TRANSFERRED
/* 00016 */ 0126420,	// READN:	SUBZ	1,1	; GETWORDS: WORD:=0; CARRY=TRUE
/* 00017 */ 0061461,	//		DIB	0,FLEX	; READ(CHAR)
/* 00020 */ 0107363,	//		ADDCS	0,1,SNC	; WORD:= WORD SHIFT 8 + CHAR;
/* 00021 */ 0000017,	//		JMP	READN+1	; IF CARRY = FALSE THEN READ CH
/* 00022 */ 0046025,	//		STA@	1,ADR	; INCR(ADR); CORE(ADR) += WORD
/* 00023 */ 0010100,	//		ISZ	100	; IF INCR(CORE(100)) <> 0 THEN
/* 00024 */ 0000016,	//		JMP	READN	; READ NEXT WORD ELSE
/* 00025 */ 0000077,	// ADR:		JMP	77	; GOTO ADR
			// ; FLEXIBLE DISK: AT ENTRY, CARRY == TRUE!!
/* 00026 */ 0030037,	// FD:		LDA	2,COMM	; FLOPPY: COMMAND:=RECALIBRATE
/* 00027 */ 0071161,	// EXE:		DOAS	2,FLEX	; EXECUTE: EXECUTE(COMMAND)
/* 00030 */ 0063461,	//		SKPBN	FLEX	; ! COMMAND(0:7) = DONT CARE !
/* 00031 */ 0000027,	//		JMP	EXE	; WAIT UNTIL COMMAND ACCEPTED
/* 00032 */ 0063661,	//		SKPDN	FLEX	; WAIT UNTIL COMMAND EXECUTED
/* 00033 */ 0000032,	//		JMP	.-1	;
/* 00034 */ 0151102,	//		MOVL	2,2,SZC	; IF NEXT COMMAND = READ BLOCK
/* 00035 */ 0000027,	//		JMP	EXE	;   THEN GOTO EXECUTE ELSE
/* 00036 */ 0000016,	//		JMP	READN	;   GOTO GETWORDS
/* 00037 */ 0101000,	// COMM:	1B0+1B6		; COMMAND BITS
};

/* From RCSL 52-AA894, Appendix G */
static uint16_t autorom_appendix_g[32] = {
			// ; PROGRAM LOAD FROM DISK
			// ; AND OTHER HIGH SPEED DEVICES
			// .LOC		0
			// DEV		0
			// FLEX=	61		; FLEXIBLE DISC
/* 00000 */ 0064477,	//		READS	1	; READ SWITCHES
/* 00001 */ 0020037,	//		LDA	0, C77	; ISOLATE DEVICE CODE
/* 00002 */ 0123400,	//		AND	1,0
/* 00003 */ 0100404,	//		NEG	0,0,SZR
/* 00004 */ 0010031,	// LOOP:	ISZ	OP1	; COUNT DEVICE CODE
/* 00005 */ 0010032,	//		ISZ	OP2	; INTO ALL IN/OUT
/* 00006 */ 0010022,	//		ISZ	OP3	; INSTRUCTIONS
/* 00007 */ 0010025,	//		ISZ	OP4	;
/* 00010 */ 0101404,	//		INC	0,0,SZR	; DONE
/* 00011 */ 0000004,	//		JMP	LOOP	; NO, INCREMENT AGAIN
/* 00012 */ 0125102,	//		MOVL	1,1,SZC	; DISK
/* 00013 */ 0000022,	//		JMP	OP3	; NO
/* 00014 */ 0004030,	//		JSR	SPEC	; SEEK WITH CLEAR
/* 00015 */ 0175000,	//		175000		; SEEK INSTRUCTION
/* 00016 */ 0004030,	//		JSR	SPEC	; RECALIBRATE WITH CLEAR
/* 00017 */ 0175400,	//		175400		; RECALIBRATE INSTRUCTION
/* 00020 */ 0004030,	//		JSR	SPEC	; DISK READY
/* 00021 */ 0175000,	//		175000		; SEEK INSTRUCTION
/* 00022 */ 0061100,	// OP3:		DOAS	0,DEV	; START DEVICE WITH
			//				; RESET INSTRUCTION REGISTER
/* 00023 */ 0030027,	//		LDA	2,C377	; SETUP JMP 377 IN
/* 00024 */ 0050377,	//		STA	2,377	; LOCATION 377
/* 00025 */ 0063400,	// OP4:		SKPBN	DEV	; BUSY
/* 00026 */ 0000022,	//		JMP	OP3	; NO, START AGAIN
/* 00027 */ 0000377,	// C377:	JMP	377	; YES, WAIT PAGE ZERO
			//				; OVERWRITTEN
/* 00030 */ 0025400,	// SPEC:	LDA	1,0,3	; DISK ROUTINE
/* 00031 */ 0065300,	// OP1:		DOAP	1,DEV	; RECALIBRATE DISK
/* 00032 */ 0064400,	// OP2:		DIA	1,DEV	; READ STATUS
/* 00033 */ 0131300,	//		MOVS	1,2	;
/* 00034 */ 0133405,	//		AND	1,2,SNR	; DONE
/* 00035 */ 0000032,	//		JMP	OP2	; NO, WAIT
/* 00036 */ 0001401,	//		JMP	1,3	; RETURN
/* 00037 */ 0000077,	// C77:		77
};

void
AutoRom(struct rc3600 *cs)
{
	uint16_t *autorom;
	uint16_t adr;

	switch (cs->switches & 0x3f) {
	case 000:
		// CONSOLE INIT + MEMORY RESET
		autorom = autorom_appendix_a;
		break;
	case 001:
		// MEMORY TESTPROGRAM
		autorom = NULL;
		break;
	case 002:
		// CONSOLE ECHO/CHARGEN
		autorom = autorom_appendix_c;
		break;
	case 016:
		// CARD READER
		autorom = autorom_appendix_e;
		break;
	case 020:
		// Disc Storage Module
		autorom = NULL;
		break;
	case 056:
		// CARD READER
		autorom = autorom_appendix_e;
		break;
	case 061:
		// FLEXIBLE DISC PROGRAM LOAD
		autorom = autorom_appendix_f;
		break;
	case 073:
		// DISC PROGRAM LOAD
		autorom = autorom_appendix_g;
		break;
	default:
		autorom = autorom_appendix_d;
		break;
	}
	AN(autorom);
	for(adr = 0; adr < 32; adr++)
		core_write(cs, adr, autorom[adr], CORE_NULL);
}

