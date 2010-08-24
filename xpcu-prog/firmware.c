#include "commands.h"

#define EXTERN
#define _AT_(a) at a
typedef unsigned char BYTE;

typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef signed int int16_t;
typedef unsigned int uint16_t;

typedef signed long int32_t;
typedef unsigned long uint32_t;

EXTERN xdata _AT_(0xE600) volatile BYTE CPUCS;
EXTERN xdata _AT_(0xE601) volatile BYTE IFCONFIG;
EXTERN xdata _AT_(0xE604) volatile BYTE FIFORESET;

EXTERN xdata _AT_(0xE68D) volatile BYTE EP1OUTBC;
EXTERN xdata _AT_(0xE68F) volatile BYTE EP1INBC;
EXTERN xdata _AT_(0xE6A1) volatile BYTE EP1OUTCS;
EXTERN xdata _AT_(0xE6A2) volatile BYTE EP1INCS;
EXTERN xdata _AT_(0xE780) volatile BYTE EP1OUTBUF[64];
EXTERN xdata _AT_(0xE7C0) volatile BYTE EP1INBUF[64];


EXTERN xdata _AT_(0xE6A3) volatile BYTE EP2CS;
EXTERN xdata _AT_(0xE6A5) volatile BYTE EP6CS;
EXTERN xdata _AT_(0xE6A6) volatile BYTE EP8CS;


EXTERN xdata _AT_(0xE690) volatile BYTE EP2BCH;
EXTERN xdata _AT_(0xE691) volatile BYTE EP2BCL;

EXTERN xdata _AT_(0xF000) volatile BYTE EP2FIFOBUF[1024];

EXTERN xdata _AT_(0xE698) volatile BYTE EP6BCH;
EXTERN xdata _AT_(0xE699) volatile BYTE EP6BCL;

EXTERN xdata _AT_(0xF800) volatile BYTE EP6FIFOBUF[1024];

EXTERN xdata _AT_(0xE69C) volatile BYTE EP8BCH;
EXTERN xdata _AT_(0xE69D) volatile BYTE EP8BCL;

EXTERN xdata _AT_(0xFC00) volatile BYTE EP8FIFOBUF[1024];


sfr at 0x80 IOA;
sfr at 0xB2 OEA;

sfr at 0xA0 IOC;
sfr at 0xB4 OEC;

sfr at 0xB0 IOD;
sfr at 0xB5 OED;

sfr at 0xB1 IOE;
sfr at 0xB6 OEE;


#define bmBIT1	2
#define bmBIT2	4
#define bmBIT3	8

#define NOP _asm nop; _endasm;


void core_init(void)
{
	/* 48MHz */
	CPUCS = 0x10;

	/* 48 MHz */
	IFCONFIG = 0xC0;
	NOP;

	FIFORESET = 0x80;
	NOP;
	FIFORESET = 2;
	NOP;
	FIFORESET = 4;
	NOP;
	FIFORESET = 6;
	NOP;
	FIFORESET = 8;
	NOP;
	FIFORESET = 0;
	NOP;
}

#define OUTBUF_N 4
#define OUTBUF_SIZE 2

uint8_t outbuf_inptr = 0;
uint8_t outbuf_outptr = 0;
uint8_t outbuf_len[OUTBUF_N];
uint8_t outbuf[OUTBUF_N][OUTBUF_SIZE];

void outbuf_init()
{
	outbuf_inptr = 0;
	outbuf_outptr = 0;
	outbuf_len[0] = 0;
}

void outbuf_maybe_send()
{
	uint8_t i;

	if (outbuf_outptr == outbuf_inptr) {
		/* there is no buffer to send */
		return;
	}

	if ((EP6CS & bmBIT2) == 0) {
		/* EP is not ready to send */
		return;
	}

	for (i=0; i<outbuf_len[outbuf_outptr]; i++) {
		EP6FIFOBUF[i] = outbuf[outbuf_outptr][i];
		NOP;
	}

	EP6BCH = 0;
	NOP;
	EP6BCL = outbuf_len[outbuf_outptr];
	NOP;

	outbuf_outptr = (outbuf_outptr + 1);
	if (outbuf_outptr == OUTBUF_N)
		outbuf_outptr = 0;
}

void outbuf_flush()
{
	uint8_t nextbuf = (outbuf_inptr + 1);
	if (nextbuf == OUTBUF_N)
		nextbuf = 0;

	while (nextbuf == outbuf_outptr)
		outbuf_maybe_send();

	outbuf_inptr = nextbuf;
	outbuf_len[outbuf_inptr] = 0;
}

void outbuf_addbyte(uint8_t value)
{
	uint8_t idx = outbuf_len[outbuf_inptr];
	outbuf[outbuf_inptr][idx++] = value;
	outbuf_len[outbuf_inptr] = idx;

	if (idx == OUTBUF_SIZE)
		outbuf_flush();
}


// CPLD Pinout:
//	TCK .. IOE[3]
//	TMS .. IOE[4]
//	TDO .. IOE[5]
//	TDI .. IOE[6]

uint8_t cpld_error;

void cpld_init()
{
	IOE = 1 << 3; NOP;
	OEE |= 1 << 3; NOP;
	OEE |= 1 << 4; NOP;
	OEE |= 1 << 6; NOP;
	cpld_error = 0;
}

void cpld_pulse_tck(uint8_t tms, uint8_t tdi, uint8_t tdo, uint8_t sync)
{
	uint8_t tdo_val;

	IOE = (1 << 3) | (tms << 4) | (tdi << 6); NOP;
	IOE = (0 << 3) | (tms << 4) | (tdi << 6); NOP;
	IOE = (1 << 3) | (tms << 4) | (tdi << 6); NOP;

	tdo_val = (IOE & (1 << 5)) != 0;

	if (tdo == 0 && tdo_val != 0)
		cpld_error = 1;

	if (tdo == 1 && tdo_val == 0)
		cpld_error = 1;

	if (sync) {
		outbuf_addbyte(CMD_BYTE_REPORT_CPLD | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (cpld_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
		outbuf_flush();
		cpld_error = 0;
	}
}

void cpld_sync()
{
	uint8_t tdo_val = (IOE & (1 << 5)) != 0;
	outbuf_addbyte(CMD_BYTE_REPORT_CPLD | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (cpld_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
	outbuf_flush();
	cpld_error = 0;
}


// JTAG Pinout:
//	TCK .. IOC[3]
//	TMS .. IOC[4]
//	TDO .. IOC[5]
//	TDI .. IOC[6]

uint8_t jtag_error;

void jtag_init()
{
	IOE = 1 << 3; NOP;
	OEE |= 1 << 3; NOP;
	OEE |= 1 << 4; NOP;
	OEE |= 1 << 6; NOP;
	cpld_error = 0;
}

void jtag_pulse_tck(uint8_t tms, uint8_t tdi, uint8_t tdo, uint8_t sync)
{
	uint8_t tdo_val;

	IOC = (1 << 3) | (tms << 4) | (tdi << 6); NOP;
	IOC = (0 << 3) | (tms << 4) | (tdi << 6); NOP;
	IOC = (1 << 3) | (tms << 4) | (tdi << 6); NOP;

	tdo_val = (IOC & (1 << 5)) != 0;

	if (tdo == 0 && tdo_val != 0)
		jtag_error = 1;

	if (tdo == 1 && tdo_val == 0)
		jtag_error = 1;

	if (sync) {
		outbuf_addbyte(CMD_BYTE_REPORT_CPLD | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (cpld_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
		outbuf_flush();
		jtag_error = 0;
	}
}

void jtag_sync()
{
	uint8_t tdo_val = (IOC & (1 << 5)) != 0;
	outbuf_addbyte(CMD_BYTE_REPORT_CPLD | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (cpld_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
	outbuf_flush();
	jtag_error = 0;
}


void main(void)
{
	uint8_t inbuf[64];
	uint32_t ledcount = 0;
	uint8_t ledblink = 0;
	uint16_t input_len, i;
	uint8_t cmd;

	core_init();
	outbuf_init();
	cpld_init();
	jtag_init();

	/* LEDs */
	OEA = 0x3;
	NOP;
	IOA = 0x1;
	NOP;

	/* arm EP2 (host->fw) */
	EP2BCL = 0xff;
	NOP;
	EP2BCL = 0xff;
	NOP;

	while (1)
	{
		outbuf_maybe_send();

		/* proccess input buffer */
		if ((EP2CS & bmBIT2) == 0)
		{
			input_len = (EP2BCH << 8) | EP2BCL;

			for (i = 0; i < input_len; i++)
				inbuf[i] = EP2FIFOBUF[i];

			EP2BCH = 0xff;
			NOP;
			EP2BCL = 0xff;
			NOP;

			for (i = 0; i < input_len; i++) {
				cmd = inbuf[i];
				switch (cmd & CMD_BYTE_CMD_MASK)
				{
				case CMD_BYTE_PULSE_CPLD:
					cpld_pulse_tck(cmd & CMD_BYTE_PULSE_FLAG_TMS != 0, cmd & CMD_BYTE_PULSE_FLAG_TDI != 0,
							(cmd & CMD_BYTE_PULSE_FLAG_TDO != 0) | (cmd & CMD_BYTE_PULSE_FLAG_CHK),
							cmd & CMD_BYTE_PULSE_FLAG_SYN != 0);
					break;
				case CMD_BYTE_PULSE_JTAG:
					jtag_pulse_tck(cmd & CMD_BYTE_PULSE_FLAG_TMS != 0, cmd & CMD_BYTE_PULSE_FLAG_TDI != 0,
							(cmd & CMD_BYTE_PULSE_FLAG_TDO != 0) | (cmd & CMD_BYTE_PULSE_FLAG_CHK),
							cmd & CMD_BYTE_PULSE_FLAG_SYN != 0);
					break;
				case CMD_BYTE_SYNC_CPLD:
					cpld_sync();
					break;
				case CMD_BYTE_SYNC_JTAG:
					jtag_sync();
					break;
				}
			}

			if (!ledblink)
				ledcount = 10000;
			ledblink = 2;
		}

		if (ledcount == 10000 && ledblink) {
			ledblink--;
			IOA |= 0x02;
			NOP;
		}
		if (ledcount == 20000) {
			ledcount = 0;
			IOA &= ~0x02;
			NOP;
		}
		ledcount++;
	}
}

