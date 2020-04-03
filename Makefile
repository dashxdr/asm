GDB= #-g
CC=gcc
CFLAGS=-O2 $(GDB) -Wall
PROGS:=asm link od makerom
LDFLAGS:=$(GDB)

all: $(PROGS)

asm:	x86.o asm.o 68k.o z80.o

asm.o:	asm.c asm.h

x86.o:	x86.c asm.h

68k.o:	68k.c asm.h

z80.o:	z80.c asm.h

clean:
	rm -f $(PROGS) *.o
