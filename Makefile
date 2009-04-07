
#CC = ppc_6xx-gcc
#CFLAGS += -DXSVFTOOL_RLMS_VLINE

CFLAGS += -Wall -Os -ggdb -std=gnu99 -MD
#CFLAGS += -Wextra -Wno-unused-parameter -Werror

xsvftool: tap.o svf.o xsvf.o play.o xsvftool.o

clean:
	rm -f xsvftool *.o *.d

-include *.d

