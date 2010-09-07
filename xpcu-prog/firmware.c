
#include "commands.h"


typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef signed int int16_t;
typedef unsigned int uint16_t;

typedef signed long int32_t;
typedef unsigned long uint32_t;


#define REGISTER_FUNCTIONS(_name)			\
	static uint8_t _name ## _get(void) {		\
		uint8_t r;				\
		_asm nop _endasm;			\
		r = _name;				\
		_asm nop _endasm;			\
		return r;				\
	}						\
	static void _name ## _set(uint8_t val) {	\
		register uint8_t r = val;		\
		_asm nop _endasm;			\
		_name = r;				\
		_asm nop _endasm;			\
	}						\
	static void _name ## _setbits(uint8_t val) {	\
		register uint8_t r = val;		\
		_asm nop _endasm;			\
		_name |= r;				\
		_asm nop _endasm;			\
	}						\
	static void _name ## _clearbits(uint8_t val) {	\
		register uint8_t r = ~val;		\
		_asm nop _endasm;			\
		_name &= r;				\
		_asm nop _endasm;			\
	}

#define XDATA_REGISTER(_addr, _name)			\
	xdata at (_addr) volatile uint8_t _name;	\
	REGISTER_FUNCTIONS(_name)

#define SFR_REGISTER(_addr, _name)			\
	sfr at (_addr) _name;				\
	REGISTER_FUNCTIONS(_name)

XDATA_REGISTER(0xE600, CPUCS)
XDATA_REGISTER(0xE601, IFCONFIG)
XDATA_REGISTER(0xE604, FIFORESET)

XDATA_REGISTER(0xE68D, EP1OUTBC)
XDATA_REGISTER(0xE68F, EP1INBC)
XDATA_REGISTER(0xE6A1, EP1OUTCS)
XDATA_REGISTER(0xE6A2, EP1INCS)

xdata at (0xE780) volatile uint8_t EP1OUTBUF[64];
xdata at (0xE7C0) volatile uint8_t EP1INBUF[64];

SFR_REGISTER(0x80, IOA)
SFR_REGISTER(0xB2, OEA)

SFR_REGISTER(0xA0, IOC)
SFR_REGISTER(0xB4, OEC)

SFR_REGISTER(0xB0, IOD)
SFR_REGISTER(0xB5, OED)

SFR_REGISTER(0xB1, IOE)
SFR_REGISTER(0xB6, OEE)


#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80


void core_init(void)
{
	// /* 48MHz */
	CPUCS_set(0x10);

	// /* 48 MHz */
	IFCONFIG_set(0xC0);
}

void signal(uint8_t blinks)
{
	uint8_t i;
	uint32_t j;

	IOA_set(0x22);

	for (i = 0; i < blinks; i++) {
		IOA_set(0x23);
		for (j = 0; j < 100000; j++) { _asm nop _endasm; }
		IOA_set(0x22);
		for (j = 0; j < 200000; j++) { _asm nop _endasm; }
	}
	for (j = 0; j < 1000000; j++) { _asm nop _endasm; }

	IOA_set(0x21);
}

void panic(uint8_t blinks)
{
	while (1)
		signal(blinks);
}

uint8_t send_bytecount;

void send_byte(uint8_t value)
{
	uint32_t waitcount;
	send_bytecount++;

	/* check if EP is ready */
	for (waitcount = 0; 1; waitcount++) {
		if ((EP1INCS_get() & BIT1) == 0)
			break;
		// this is synchronous - so the EP should never be busy here!
		if (waitcount > 1000000)
			signal(send_bytecount);
	}

	EP1INBUF[0] = send_bytecount;
	EP1INBUF[1] = value;
	EP1INBC_set(2);
}


// CPLD Pinout:
//	TCK .. IOE[3]
//	TMS .. IOE[4]
//	TDO .. IOE[5]
//	TDI .. IOE[6]

uint8_t cpld_error;

void cpld_init()
{
	IOE_set(1 << 3);
	OEE_set(1 << 3);
	OEE_setbits(1 << 4);
	OEE_setbits(1 << 6);
	cpld_error = 0;
}

void cpld_pulse_tck(uint8_t tms, uint8_t tdi, uint8_t tdo, uint8_t sync)
{
	uint8_t tdo_val;
	uint8_t ioe1, ioe2;

	ioe1 = (1 << 3) | (tms << 4) | (tdi << 6);
	ioe2 = (0 << 3) | (tms << 4) | (tdi << 6);

	IOE_set(ioe1);
	IOE_set(ioe2);
	IOE_set(ioe1);

	tdo_val = (IOE_get() & (1 << 5)) != 0;

	if (tdo == 0 && tdo_val != 0)
		cpld_error = 1;

	if (tdo == 1 && tdo_val == 0)
		cpld_error = 1;

	if (sync) {
		send_byte(CMD_BYTE_REPORT_CPLD | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (cpld_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
		cpld_error = 0;
	}
}

void cpld_sync()
{
	uint8_t tdo_val = (IOE_get() & (1 << 5)) != 0;
	send_byte(CMD_BYTE_REPORT_CPLD | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (cpld_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
	cpld_error = 0;
}


// JTAG Pinout:
//	TCK .. IOC[3]
//	TMS .. IOC[4]
//	TDO .. IOD[5]
//	TDI .. IOC[6]
//	OE ... IOA[5]  (always set)

uint8_t jtag_error;

void jtag_init()
{
	IOC_set(1 << 3);
	OEC_set(1 << 3);
	OEC_setbits(1 << 4);
	OEC_setbits(1 << 6);
	IOC_set(0);
	OEC_set(0);
	jtag_error = 0;
}

void jtag_pulse_tck(uint8_t tms, uint8_t tdi, uint8_t tdo, uint8_t sync)
{
	uint8_t tdo_val;
	uint8_t ioc1, ioc2;

	ioc1 = (1 << 3) | (tms << 4) | (tdi << 6);
	ioc2 = (0 << 3) | (tms << 4) | (tdi << 6);

	IOC_set(ioc1);
	IOC_set(ioc2);
	IOC_set(ioc1);

	tdo_val = (IOD_get() & (1 << 5)) != 0;

	if (tdo == 0 && tdo_val != 0)
		jtag_error = 1;

	if (tdo == 1 && tdo_val == 0)
		jtag_error = 1;

	if (sync) {
		send_byte(CMD_BYTE_REPORT_JTAG | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (jtag_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
		jtag_error = 0;
	}
}

void jtag_sync()
{
	uint8_t tdo_val = (IOD_get() & (1 << 5)) != 0;
	send_byte(CMD_BYTE_REPORT_JTAG | (tdo_val ? CMD_BYTE_REPORT_FLAG_TDO : 0) | (jtag_error ? CMD_BYTE_REPORT_FLAG_ERR : 0));
	jtag_error = 0;
}


void main(void)
{
	uint8_t inbuf[64];
	uint32_t ledcount = 0;
	uint8_t ledblink = 0;
	uint8_t input_len, cmd, i;
	uint32_t j;

	core_init();
	cpld_init();
	jtag_init();

	send_bytecount = 0;

	/* LEDs */
	OEA_set(0x23);
	IOA_set(0x23);

	/* arm EP1OUT (host -> psoc) */
	EP1OUTBC_set(1);

#if 0
	/* simple I/O test */
	while (1)
	{
		IOA_set(0x22);
		jtag_pulse_tck(1, 0, 2, 0);
		for (j = 0; j < 1000000; j++) { _asm nop _endasm; }

		IOA_set(0x21);
		jtag_pulse_tck(0, 1, 2, 0);
		for (j = 0; j < 1000000; j++) { _asm nop _endasm; }
	}
#endif

	while (1)
	{
		/* proccess input buffer */
		if ((EP1OUTCS_get() & BIT1) == 0)
		{
			input_len = EP1OUTBC_get();

			for (i = 0; i < input_len; i++) {
				inbuf[i] = EP1OUTBUF[i];
			}

			/* arm EP1OUT (host -> psoc) */
			EP1OUTBC_set(1);

			for (i = 0; i < input_len; i++) {
				cmd = inbuf[i];
				switch (cmd & CMD_BYTE_CMD_MASK)
				{
				case CMD_BYTE_PULSE_CPLD:
					cpld_pulse_tck((cmd & CMD_BYTE_PULSE_FLAG_TMS) != 0, (cmd & CMD_BYTE_PULSE_FLAG_TDI) != 0,
							((cmd & CMD_BYTE_PULSE_FLAG_TDO) != 0) | ((cmd & CMD_BYTE_PULSE_FLAG_CHK) ? 0 : 2),
							(cmd & CMD_BYTE_PULSE_FLAG_SYN) != 0);
					break;
				case CMD_BYTE_PULSE_JTAG:
					jtag_pulse_tck((cmd & CMD_BYTE_PULSE_FLAG_TMS) != 0, (cmd & CMD_BYTE_PULSE_FLAG_TDI) != 0,
							((cmd & CMD_BYTE_PULSE_FLAG_TDO) != 0) | ((cmd & CMD_BYTE_PULSE_FLAG_CHK) ? 0 : 2),
							(cmd & CMD_BYTE_PULSE_FLAG_SYN) != 0);
					break;
				case CMD_BYTE_SYNC_CPLD:
					cpld_sync();
					break;
				case CMD_BYTE_SYNC_JTAG:
					jtag_sync();
					break;
				case CMD_BYTE_ECHO:
					send_byte(cmd);
					break;
				}
			}

			if (!ledblink)
				ledcount = 10000;
			ledblink = 2;
		}

		if (ledcount == 10000 && ledblink) {
			ledblink--;
			IOA_setbits(0x02);
		}
		if (ledcount == 20000) {
			ledcount = 0;
			IOA_clearbits(0x02);
		}
		ledcount++;
	}
}

