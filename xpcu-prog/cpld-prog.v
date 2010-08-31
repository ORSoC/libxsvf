`timescale 1ns / 1ps

module cpldprog(
    output sys_tck,
    output sys_tms,
    output sys_tdi,
    input sys_tdo,
    input cy_tck,
    input cy_tms,
    input cy_tdi,
    output cy_tdo
    );

assign sys_tck = cy_tck;
assign sys_tms = cy_tms;
assign sys_tdi = cy_tdi;
assign cy_tdo = sys_tdo;

endmodule
