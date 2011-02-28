/*
 *  xsvftool-xpcu - An (X)SVF player for the Xilinx Platform Cable USB
 *
 *  Copyright (C) 2011  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2011  Clifford Wolf <clifford@clifford.at>
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. The names of the authors may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 *  Command Reference (EP1)
 *  -----------------------
 *
 *  Request: T<nn>
 *  Response: OK (T<nn>)
 *    Configure timing for EP2/JTAG transfers
 *
 *  Request: R
 *  Response: OK (R)
 *    Perform internal and CPLD reset
 *
 *  Request: W<n>
 *  Response: OK (W<n>)
 *    Wait for the CPLD sync signal to become <n>
 *
 *  Request: C
 *  Response: <nnnnnn> (C)
 *    Read out the CPLD verilog checksum
 *
 *  Request: B<n>
 *  Response: OK (B<n>)
 *    Set BUFFER_OE to <n>
 *
 *  Request: I<n>
 *  Response: OK (I<n>)
 *    Set INIT_INT to <n>
 *
 *  Request: L<r><g>
 *  Response: OK (L<r><g>)
 *    Set LEDs (<r> = red, <g> = green)
 *
 *  Request: S
 *  Response: <n><m><k><p><x><y> (S)
 *    Read and reset status bits
 *    (<n> = FX2-JTAG-ERR, <m> = CPLD-JTAG-ERR, <k> = INIT_B_INT,
 *     <p> = SLOE_INT, <x> = FX2-JTAG-TDO, <y> = CPLD-JTAG-TDO)
 *
 *  Request: J<bindata>
 *  Response: -- NONE --
 *    Execute JTAG transaction (4bit/cycle)
 *
 *
 *  Target JTAG Programming (EP2)
 *  -----------------------------
 *
 *  Raw JTAG transaction codes.
 *  (4bit/cycle when T=0, 8bit/cycle otherwise)
 *
 *
 *  JTAG Transaction codes
 *  ----------------------
 *
 *  0000:
 *    NOP
 *
 *  0001 xxxx:
 *    Set sync signal to 'xxxx' (engine on CPLD only)
 *
 *  001x:
 *    reserved for future use
 *
 *  01xy:
 *    JTAG transaction without TDO check. TMS=x, TDI=y
 *
 *  1zxy:
 *    JTAG transaction with TDO check. TDO=z, TMS=x, TDI=y
 *
 *
 *  FX2 <-> CPLD Interface
 *  ----------------------
 *
 *  FD[7:0]   --->   FD[7:0]
 *  CTL0      --->   STROBE_FD (neg)
 *  CTL1      --->   STROBE_SHIFT (neg)
 *  CTL2      --->   STROBE_PUSH (neg)
 *
 *  PC[7:4]   <---   SYNC
 *  PC3       <---   TDO
 *  PC2       <---   CKSUM
 *  PC1       <---   INIT_B_INT
 *  PC0       <---   ERR
 *
 *  PD4       --->   RESET_SYNC
 *  PD3       --->   RESET_ERR
 *  PD2       --->   INIT_INT
 *  PD1       --->   SHIFT_CKSUM
 *  PD0       --->   RESET_CKSUM
 *
 *
 *  Other FX2 Connections
 *  ---------------------
 *
 *  PA0       --->   LED_GREEN
 *  PA1       --->   LED_RED
 *  PA2       <---   SLOE_INT
 *  PA5       --->   BUFFER_OE
 *
 *  IOE[3]    --->   CPLD TCK
 *  IOE[4]    --->   CPLD TMS
 *  IOE[5]    <---   CPLD TDO
 *  IOE[6]    --->   CPLD TDI
 *
 */

// #include "fx2.h"
// #include "fx2regs.h"
// #include "fx2sdly.h"
#include "gpifprog_fixed.c"

// set to '1' on CPLD JTAG error
BYTE state_err;

void setup(void)
{
	BYTE i;

	/* CPU: 48MHz, don't drive CLKOUT */
	CPUCS = 0x10;

	/* Configure the Endpoints (EP2 => 4x 1kB) */
	EP2CFG = 0xA8; // VALID=1, DIR=0, TYPE=10, SIZE=1, BUF=00
	EP4CFG = 0x00; // VALID=0, DIR=0, TYPE=00, SIZE=0, BUF=00
	EP6CFG = 0x00; // VALID=0, DIR=0, TYPE=00, SIZE=0, BUF=00
	EP8CFG = 0x00; // VALID=0, DIR=0, TYPE=00, SIZE=0, BUF=00

	/* USB FIFO */
	FIFORESET = 0x80;
	SYNCDELAY;
	FIFORESET = 2;
	SYNCDELAY;
	FIFORESET = 4;
	SYNCDELAY;
	FIFORESET = 6;
	SYNCDELAY;
	FIFORESET = 8;
	SYNCDELAY;
	FIFORESET = 0;
	SYNCDELAY;

	/* Set WORDWIDE=0 for all FIFOs */
	EP2FIFOCFG &= ~bmWORDWIDE;
	SYNCDELAY;
	EP4FIFOCFG &= ~bmWORDWIDE;
	SYNCDELAY;
	EP6FIFOCFG &= ~bmWORDWIDE;
	SYNCDELAY;
	EP8FIFOCFG &= ~bmWORDWIDE;
	SYNCDELAY;

	/* Initialize GPIF Subsystem */
	GpifInit();

	/* Misc signals on port A */
	PORTACFG = 0;
	OEA = bmBIT0 | bmBIT1 | bmBIT5;
	IOA = 0;

	/* FX2 <-> CPLD signals on port C */
	PORTCCFG = 0;
	OEC = 0;
	IOC = 0;

	/* FX2 <-> CPLD signals on port D */
	OED = bmBIT0 | bmBIT1 | bmBIT2 | bmBIT3 | bmBIT4;
	IOD = 0;

	/* XC2S256 JTAG on port E */
	OEE = bmBIT3|bmBIT4|bmBIT6;
	IOE = bmBIT3|bmBIT4|bmBIT6;

	/* Set TAP to logic reset state */
	for (i=0; i<16; i++) {
		// TMS is high - just generate a few TCK pulses
		IOE &= ~bmBIT3; SYNCDELAY;
		IOE |= bmBIT3; SYNCDELAY;
	}

	/* All set up: Let the host find out about the new EP config */
#if 0
	USBCS |= bmDISCON;
	for (i=0; i<100; i++) {
		SYNCDELAY;
	}
	USBCS &= ~bmDISCON;
#endif
}

BYTE nibble2hex(BYTE v)
{
	return "0123456789ABCDEF"[v&0x0f];
}

BYTE hex2nibble(BYTE v)
{
	if (v >= '0' && v <= '9')
		return v - '0';
	if (v >= 'a' && v <= 'f')
		return 0x0A + v - 'a';
	if (v >= 'A' && v <= 'F')
		return 0x0A + v - 'A';
	return 0;
}

xdata at (0xE400 + 64) volatile BYTE GPIF_WAVE2_LEN0;
xdata at (0xE400 + 66) volatile BYTE GPIF_WAVE2_LEN2;

void proc_command_t(BYTE t)
{
	if (t == 0) {
		GPIFWFSELECT = 0x4E; SYNCDELAY;
	} else {
		GPIFWFSELECT = 0x4A; SYNCDELAY;
		GPIF_WAVE2_LEN0 = t; SYNCDELAY;
		GPIF_WAVE2_LEN2 = t; SYNCDELAY;
	}

	EP1INBUF[0] = 'O'; SYNCDELAY;
	EP1INBUF[1] = 'K'; SYNCDELAY;
	EP1INBUF[2] = ' '; SYNCDELAY;
	EP1INBUF[3] = '('; SYNCDELAY;
	EP1INBUF[4] = nibble2hex((t >> 4) & 0x0f); SYNCDELAY;
	EP1INBUF[5] = nibble2hex((t >> 0) & 0x0f); SYNCDELAY;
	EP1INBUF[6] = ')'; SYNCDELAY;
	EP1INBC = 7; SYNCDELAY;
}

void proc_command_r(void)
{
	BYTE i, *p = "OK (R)";

	/* Reset TAP to logic reset state */
	IOE = bmBIT3|bmBIT4|bmBIT6;
	for (i=0; i<16; i++) {
		// TMS is high - just generate a few TCK pulses
		IOE &= ~bmBIT3; SYNCDELAY;
		IOE |= bmBIT3; SYNCDELAY;
	}

	/* Reset speed to max. */
	GPIFWFSELECT = 0x4E; SYNCDELAY;

	/* Reset JTAG error state */
	state_err = 0;

	/* Reset LEDs and BUFFER_OE */
	IOA = 0;

	/* Assert CPLD reset pins */
	IOD = bmBIT0 | bmBIT3 | bmBIT4;
	SYNCDELAY;
	SYNCDELAY;
	IOD = 0;

	/* Send response */
	for (i = 0; p[i]; i++) {
		EP1INBUF[i] = p[i]; SYNCDELAY;
	}
	EP1INBC = i; SYNCDELAY;
}

void proc_command_w(BYTE v)
{
	BYTE i, *p = "NOT IMPLEMENTED";
	for (i = 0; p[i]; i++) {
		EP1INBUF[i] = p[i]; SYNCDELAY;
	}
	EP1INBC = i; SYNCDELAY;
}

void proc_command_c(void)
{
	BYTE i, *p = "NOT IMPLEMENTED";
	for (i = 0; p[i]; i++) {
		EP1INBUF[i] = p[i]; SYNCDELAY;
	}
	EP1INBC = i; SYNCDELAY;
}

void proc_command_b(BYTE v)
{
	BYTE i, *p = "NOT IMPLEMENTED";
	for (i = 0; p[i]; i++) {
		EP1INBUF[i] = p[i]; SYNCDELAY;
	}
	EP1INBC = i; SYNCDELAY;
}

void proc_command_i(BYTE v)
{
	BYTE i, *p = "NOT IMPLEMENTED";
	for (i = 0; p[i]; i++) {
		EP1INBUF[i] = p[i]; SYNCDELAY;
	}
	EP1INBC = i; SYNCDELAY;
}

void proc_command_l(BYTE r, BYTE g)
{
	BYTE i, *p = "NOT IMPLEMENTED";
	for (i = 0; p[i]; i++) {
		EP1INBUF[i] = p[i]; SYNCDELAY;
	}
	EP1INBC = i; SYNCDELAY;
}

void proc_command_s(void)
{
	EP1INBUF[0] =            PC0 ? '1' : '0'; SYNCDELAY;
	EP1INBUF[1] =      state_err ? '1' : '0'; SYNCDELAY;
	EP1INBUF[2] =            PC1 ? '1' : '0'; SYNCDELAY;
	EP1INBUF[3] =            PA2 ? '1' : '0'; SYNCDELAY;
	EP1INBUF[4] =            PC3 ? '1' : '0'; SYNCDELAY;
	EP1INBUF[5] = (IOE & bmBIT5) ? '1' : '0'; SYNCDELAY;
	EP1INBUF[6] = ' '; SYNCDELAY;
	EP1INBUF[7] = '('; SYNCDELAY;
	EP1INBUF[8] = 'S'; SYNCDELAY;
	EP1INBUF[9] = ')'; SYNCDELAY;
	EP1INBC = 10; SYNCDELAY;

	// reset error state
	state_err = 0;
	IOD = bmBIT3;
	SYNCDELAY;
	SYNCDELAY;
	IOD &= ~bmBIT3;
}

BYTE proc_command_j_exec_skip_next;
void proc_command_j_exec(BYTE cmd)
{
	if (proc_command_j_exec_skip_next) {
		proc_command_j_exec_skip_next = 0;
		return;
	}
	if (cmd == 0x00)
		return;
	if (cmd == 0x01) {
		// 0001 xxxx: Set sync signal to 'xxxx' (engine on CPLD only)
		proc_command_j_exec_skip_next = 1;
		return;
	}
	if ((cmd & 0x0c) == 0x04)
	{
		// 01xy: JTAG transaction without TDO check. TMS=x, TDI=y
	
		/* set tms line */
		if (cmd & 0x02)
			IOE |= bmBIT4;
		else
			IOE &= ~bmBIT4;
	
		/* set tdi line */
		if (cmd & 0x01)
			IOE |= bmBIT6;
		else
			IOE &= ~bmBIT6;
	
		/* generate tck pulse */
		SYNCDELAY;
		IOE &= ~bmBIT3;
		SYNCDELAY;
		IOE |= bmBIT3;
		SYNCDELAY;
	
		return;
	}
	if ((cmd & 0x08) == 0x08)
	{
		// 1zxy: JTAG transaction with TDO check. TDO=z, TMS=x, TDI=y
	
		/* set tms line */
		if (cmd & 0x02)
			IOE |= bmBIT4;
		else
			IOE &= ~bmBIT4;
	
		/* set tdi line */
		if (cmd & 0x01)
			IOE |= bmBIT6;
		else
			IOE &= ~bmBIT6;
	
		/* generate tck pulse */
		SYNCDELAY;
		IOE &= ~bmBIT3;
		SYNCDELAY;
		IOE |= bmBIT3;
		SYNCDELAY;
	
		/* perform tdo check */
		if (((cmd & 0x04) == 0) != ((IOE & bmBIT5) == 0))
			state_err = 1;
	
		return;
	}
}

void proc_command_j(BYTE len)
{
	BYTE i;
	proc_command_j_exec_skip_next = 0;
	for (i = 1; i < len; i++) {
		BYTE cmd = EP1OUTBUF[i];
		proc_command_j_exec(cmd & 0x0f);
		proc_command_j_exec(cmd >> 4);
	}
}

void proc_command(void)
{
	BYTE len, cmd;

	PA1 = 1;

	/* process command(s) */
	len = EP1OUTBC;
	cmd = EP1OUTBUF[0];

	if (cmd == 'T' && len == 3)
		proc_command_t((hex2nibble(EP1OUTBUF[1]) << 4) | hex2nibble(EP1OUTBUF[2]));
	else if (cmd == 'R' && len == 1)
		proc_command_r();
	else if (cmd == 'W' && len == 2)
		proc_command_w(EP1OUTBUF[1]);
	else if (cmd == 'C' && len == 1)
		proc_command_c();
	else if (cmd == 'B' && len == 2)
		proc_command_b(EP1OUTBUF[1] == '1');
	else if (cmd == 'I' && len == 2)
		proc_command_i(EP1OUTBUF[1] == '1');
	else if (cmd == 'L' && len == 3)
		proc_command_l(EP1OUTBUF[1] == '1', EP1OUTBUF[2] == '1');
	else if (cmd == 'S' && len == 1)
		proc_command_s();
	else if (cmd == 'J')
		proc_command_j(len);
	else
	{
		/* send error response */
		EP1INBUF[0] = 'E'; SYNCDELAY;
		EP1INBUF[1] = 'R'; SYNCDELAY;
		EP1INBUF[2] = 'R'; SYNCDELAY;
		EP1INBUF[3] = 'O'; SYNCDELAY;
		EP1INBUF[4] = 'R'; SYNCDELAY;
		EP1INBC = 5; SYNCDELAY;
	}

	/* accept new data on EP1OUT */
	EP1OUTBC = 0xff; SYNCDELAY;

	PA1 = 0;
}

void proc_bulkdata(void)
{
	WORD len;

	len = (EP2BCH << 8) | EP2BCL;
	if (len == 0)
	{
		/* ignore this and accept data on EP2 */
		EP2BCL = 0xff; SYNCDELAY;
	}
#if 0
	else if (len == 1)
	{
		while ((GPIFTRIG & 0x80) == 0) { /* GPIF is busy */ }

		/* transfer single byte */
		XGPIFSGLDATH = 0; SYNCDELAY;
		XGPIFSGLDATLX = EP2FIFOBUF[0]; SYNCDELAY;

		/* accept new data on EP2 */
		EP2BCL = 0xff; SYNCDELAY;
	}
#endif
	else
	{
		while ((GPIFTRIG & 0x80) == 0) { /* GPIF is busy */ }

		/* pass pkt to GPIF master */
		EP2GPIFTCH = EP2BCH;
		EP2GPIFTCL = EP2BCL;
		EP2BCL = 0x00;
		EP2GPIFTRIG = 0xff;
	}
}

void main(void)
{
	state_err = 0;

	setup();

	/* accept data on EP2 */
	EP2BCL = 0xff; SYNCDELAY; // 1st buffer
	EP2BCL = 0xff; SYNCDELAY; // 2nd buffer
	EP2BCL = 0xff; SYNCDELAY; // 3rd buffer
	EP2BCL = 0xff; SYNCDELAY; // 4th buffer

	while (1)
	{
		/* check for data on EP1 */
		if((EP1OUTCS & bmBIT1) == 0)
			proc_command();

		/* check for data on EP2 */
		if((EP2CS & bmBIT2) == 0)
			proc_bulkdata();
	}
}

