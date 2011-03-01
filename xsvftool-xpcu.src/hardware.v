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

module top(
	clk,
	tck, tms, tdi, tdo, init, init_b,
	fd0, fd1, fd2, fd3, fd4, fd5, fd6, fd7,
	ctl0, ctl1, ctl2,
	pc0, pc1, pc2, pc3, pc4, pc5, pc6, pc7,
	pd0, pd1, pd2, pd3, pd4, pd5, pd6, pd7
);

// General Signal
input clk;

// JTAG Interface
output tck, tms, tdi, init;
input tdo, init_b;

// GPIF Interface
input fd0, fd1, fd2, fd3, fd4, fd5, fd6, fd7;
input ctl0, ctl1, ctl2;

// The entire PC and PD regs for various flags
output pc0, pc1, pc2, pc3, pc4, pc5, pc6, pc7;
input pd0, pd1, pd2, pd3, pd4, pd5, pd6, pd7;

// simple direct i/o mappings
assign pc3 = tdo;
assign pc1 = init_b;
assign init = pd2;

// checksum
wire chksum_rst, chksum_clk;
reg [23:0] chksum_buffer;
always @(posedge chksum_clk) begin
	if (chksum_rst)
		chksum_buffer <=
			`include "hardware_cksum_vl.inc"
		;
	else
		chksum_buffer <= chksum_buffer << 1;
end
assign chksum_rst = pd0;
assign chksum_clk = pd1;
assign pc2 = chksum_buffer[23];

// main engine
reg [3:0] sync;
reg [7:0] lastbyte;
reg go_exec0, go_exec1, set_sync, err;
reg reg_tck, reg_tms, reg_tdi, reg_tdo, reg_tdo_en;
always @(negedge clk) begin
	go_exec0 <= 0;
	go_exec1 <= 0;
	if (!ctl0) begin
		lastbyte <= { fd7, fd6, fd5, fd4, fd3, fd2, fd1, fd0 };
		go_exec0 <= 1;
	end
	if (!ctl1) begin
		lastbyte <= lastbyte >> 4;
		go_exec0 <= 1;
	end
	if (!ctl2) begin
		go_exec1 <= 1;
	end
	if (go_exec0) begin
		reg_tdo_en <= 0;
		if (set_sync) begin
			sync <= lastbyte[3:0];
			set_sync <= 0;
		end else
		if (lastbyte[3:0] == 0) begin
			/* NOP */
		end else
		if (lastbyte[3:0] == 1) begin
			/* Set sync signal in next insn */
			set_sync <= 1;
		end else
		if (lastbyte[3:1] == 1) begin
			/* reserved */
		end else
		if (lastbyte[3:2] == 1) begin
			/* transaction with or without TDO check */
			reg_tck <= 0;
			reg_tdo <= 'bx;
			reg_tms <= lastbyte[1];
			reg_tdi <= lastbyte[0];
		end else
		if (lastbyte[3] == 1) begin
			/* transaction with TDO check */
			reg_tck <= 0;
			reg_tdo <= lastbyte[2];
			reg_tms <= lastbyte[1];
			reg_tdi <= lastbyte[0];
			reg_tdo_en <= 1;
		end
	end
	if (go_exec1) begin
		reg_tck <= 1;
		if (reg_tdo_en && tdo != reg_tdo)
			err <= 1;
	end
	if (pd3) begin
		/* RESET ERR */
		err <= 0;
	end
	if (pd4) begin
		/* RESET SYNC */
		set_sync <= 0;
		sync <= 0;
	end
end
assign tck = reg_tck;
assign tms = reg_tms;
assign tdi = reg_tdi;
assign pc7 = sync[3];
assign pc6 = sync[2];
assign pc5 = sync[1];
assign pc4 = sync[0];
assign pc0 = err;

endmodule

