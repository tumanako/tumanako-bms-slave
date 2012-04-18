#
# Makefile for test slave firmware
#

ECHO_SRC=echo.c
ECHO_OBJ=$(ECHO_SRC:.c=.o) ../../c/util.o

SRCS=$(wildcard *.c)
HDRS=$(wildcard *.h)

CFLAGS=-mpic14 -p16f688 -V --debug -I../../c
CC=sdcc
LDFLAGS=-mpic14 -p16f688 -V --debug -Wl-c -Wl-m 

all: echo.hex

echo.hex: $(ECHO_OBJ)
	$(CC) $(ECHO_OBJ) $(LDFLAGS) $(LIBS) -oecho.hex

clean:
	rm -f *.o *.map *.asm *.p *.cod *.cof *.adb *.lst *.hex

writeEcho: echo.hex
	pk2cmd -PPIC16F688 -E -M -Z -Fecho.hex -R
