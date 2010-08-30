-- prog.vhd -- xc2c256 cpld for usb jtag
--
-- copyright (c) 2006 inisyn research
-- license: LGPL2
--
-- revision history:
-- 2006-05-27 initial
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;

entity prog is
    port(
        sys_tck : out std_logic;
        sys_tms : out std_logic;
        sys_tdi : out std_logic;
        sys_tdo : in std_logic;

        cy_tck  : in std_logic;
        cy_tms  : in std_logic;
        cy_tdi  : in std_logic;
        cy_tdo  : out std_logic
    );
end prog;

architecture syn of prog is

begin

    sys_tck <= cy_tck;

    sys_tms <= cy_tms;

    sys_tdi <= cy_tdi;

    cy_tdo <= sys_tdo;

end syn;

