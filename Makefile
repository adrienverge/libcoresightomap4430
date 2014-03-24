ifeq ($(shell which arm-linux-gnueabihf-gcc 2>&1 >/dev/null && echo 1), 1)
	TOOLCHAIN = arm-linux-gnueabihf-
else ifeq ($(shell which arm-none-eabihf-gcc 2>&1 >/dev/null && echo 1), 1)
	TOOLCHAIN = arm-none-eabihf-
else ifeq ($(shell which arm-linux-gnu-gcc 2>&1 >/dev/null && echo 1), 1)
	TOOLCHAIN = arm-linux-gnu-
endif

CC = $(TOOLCHAIN)gcc
LD = $(TOOLCHAIN)ld
CFLAGS = -mtune=cortex-a9 -Wall
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
