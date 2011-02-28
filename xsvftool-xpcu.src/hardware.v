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

// map 1-bit fds signal to internal fd bus name
wire [7:0] fd;
assign fd = { fd7, fd6, fd5, fd4, fd3, fd2, fd1, fd0 };

// simple direct i/o mappings
assign pc3 = tdo;
assign pc1 = init_b;

endmodule

