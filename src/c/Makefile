#
# Makefile for evd5 slave firmware
#

EVD5_SRC=evd5.c util.c crc.c
EVD5_OBJ=$(EVD5_SRC:.c=.o)

SRCS=$(wildcard *.c)
HDRS=$(wildcard *.h)

CFLAGS=-mpic14 -p16f688 -V --debug -I. $(EXTRA)
CC=sdcc
LDFLAGS=-mpic14 -p16f688 -V --debug -Wl-c -Wl-m -I../../../slave/src/c/ -oevd5.hex

all: evd5.hex

evd5.hex: $(EVD5_OBJ)
	$(CC) -o monitor $(EVD5_OBJ) $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o *.map crc.asm evd5.asm util.asm *.p *.cod *.adb *.lst evd5.hex

write: all
	pk2cmd -PPIC16F688 -E -M -Z -Fevd5.hex -R
	pk2cmd -PPIC16F688 -GE10-17 -R 
