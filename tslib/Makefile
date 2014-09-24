SHELL = /bin/sh

CC = gcc
CFLAGS = -std=c99 -O0 -g -Wall -Wno-unused-variable
#CFLAGS = -std=c99 -O2 -ffast-math -g -pedantic -pipe -Wall -Wextra

LD = gcc
LDFLAGS += -g -static

AR = ar
ARFLAGS = rcls

RANLIB = ranlib
RM = rm -f

SRCS = $(wildcard *.c) 
OBJS = $(SRCS:%.c=%.o)

INCLUDES = -I . -I../common -I../libstructures/ -I../h264bitstream/ -I../logging/
LIBS = -L . -ltslib -L../h264bitstream/.libs -lh264bitstream -L../logging/ -llogging   -L../libstructures/ -ldatastruct -lm

CFLAGS  += $(INCLUDES)
LDFLAGS += $(LIBS)

BINARIES = apps/ts_split apps/ts_validate_single_segment apps/ts_validate_mult_segment

all: libtslib.a $(BINARIES)

libtslib.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^ 
	$(RANLIB) $@

apps/ts_split: apps/ts_split.c libtslib.a
	$(CC) $(CFLAGS) -o apps/ts_split apps/ts_split.c $(LIBS)

apps/ts_validate_single_segment: apps/ts_validate_single_segment.c libtslib.a
	$(CC) $(CFLAGS) -o apps/ts_validate_single_segment apps/ts_validate_single_segment.c $(LIBS)

apps/ts_validate_mult_segment: apps/ts_validate_mult_segment.c libtslib.a
	$(CC) $(CFLAGS) -o apps/ts_validate_mult_segment apps/ts_validate_mult_segment.c $(LIBS)

clean:
	rm -f $(OBJS) *.a $(BINARIES) core
