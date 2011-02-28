
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

