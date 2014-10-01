SHELL = /bin/sh

CC = gcc
CFLAGS = -std=c99 -O0 -g -Wall -Wno-unused-variable
#CFLAGS = -std=c99 -O2 -ffast-math -g -pedantic -pipe -Wall -Wextra

LD = gcc
LDFLAGS += -g -static

SRCS = $(wildcard *.c) 
OBJS = $(SRCS:%.c=%.o)

INCLUDES = -I . -I../tslib -I../common -I../libstructures/ -I../h264bitstream/ -I../logging/
LIBS = -L . -L../tslib -ltslib -L../h264bitstream/.libs -lh264bitstream -L../logging/ -llogging   -L../libstructures/ -ldatastruct -lm

CFLAGS  += $(INCLUDES)
LDFLAGS += $(LIBS)

BINARIES = test

all: $(BINARIES)

test: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(OBJS) $(BINARIES) core
