ifeq (exists, $(shell [ -x "/usr/bin/arm-linux-gnueabi-gcc" ] && echo exists))
	TOOLCHAIN = arm-linux-gnueabi-
else ifeq (exists, $(shell [ -x "/usr/bin/arm-none-eabi-gcc" ] && echo exists))
	TOOLCHAIN = arm-none-eabi-
else
	TOOLCHAIN =
endif

CC = $(TOOLCHAIN)gcc
LD = $(TOOLCHAIN)ld
CFLAGS = -mcpu=cortex-a9 -Wall
LDFLAGS =

LIBS = libetb.o libstm.o libomap4430.o libstp.o
TARGETS = stmwrite etbread etbdecode stpdecode decodetimestamp

default: $(LIBS) $(TARGETS)

#
# Libraries
#

libetb.o: libetb.c libetb.h
	$(CC) -c -o $@ $(CFLAGS) $<

libstm.o: libstm.c libstm.h
	$(CC) -c -o $@ $(CFLAGS) $<

libomap4430.o: libomap4430.c libomap4430.h
	$(CC) -c -o $@ $(CFLAGS) $<

libstp.o: libstp.c libstp.h
	$(CC) -c -o $@ $(CFLAGS) $<

#
# Example programs
#

stmwrite: stmwrite.c libomap4430.o libstm.o
	$(CC) -o $@ $(CFLAGS) $^

etbread: etbread.c libomap4430.o libetb.o
	$(CC) -o $@ $(CFLAGS) $^

etbdecode: etbdecode.c libomap4430.o libetb.o libstp.o
	$(CC) -o $@ $(CFLAGS) $^

stpdecode: stpdecode.c libstp.o
	$(CC) -o $@ $(CFLAGS) $^

decodetimestamp: decodetimestamp.c libstp.o libomap4430.o libstm.o libetb.o
	$(CC) -o $@ $(CFLAGS) $^

.PHONY: clean mrproper

clean:
	rm -f *.o

mrproper: clean
	rm -f $(TARGETS)
