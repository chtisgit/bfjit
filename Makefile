SHELL ?= /bin/sh
CC ?= gcc
MAKE ?= make
RM ?= rm

CFLAGS := -O2 -pipe -g -Wall -Wextra $(CXXFLAGS)
OBJS := bfjit.o

all: bfjit

%.o: %.c
	$(CC) -m32 -no-pie -fno-pic $(CFLAGS) -c -o $@ $<

bfjit: $(OBJS)
	$(CC) -m32 -no-pie -fno-pic -o $@ $(OBJS)

clean:
	$(RM) $(OBJS) bfjit

.PHONY: all clean
