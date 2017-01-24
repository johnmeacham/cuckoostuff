CC=c99
CFLAGS= -g  -Wall -DDO_TEST=1
CFLAGS+= -Os
LDFLAGS= $(CFLAGS)
LDLIBS=

all:  cuckoostuff

cuckoostuff.o: cuckoostuff.c cuckoostuff.h

clean:
	rm -f -- *.o cuckoostuff
