SHELL = /bin/sh

CC = gcc
CFLAGS = -std=c99 -O0 -g -Wall -Wno-unused-variable
#CFLAGS = -std=c99 -O2 -ffast-math -g -pedantic -pipe -Wall -Wextra

LD = gcc
LDFLAGS += -g -static

SRCS = $(wildcard *.c) 
OBJS = $(SRCS:%.c=%.o)

LOG_LIB_DIR = ../logging
LOG_LIB = $(LOG_LIB_DIR)/liblogging.a

TSLIB_LIB_DIR = ../tslib
TSLIB_LIB = $(TSLIB_LIB_DIR)/libtslib.a

STRUCT_LIB_DIR = ../libstructures
STRUCT_LIB = $(STRUCT_LIB_DIR)/libdatastruct.a

ALL_LIBS = $(LOG_LIB) $(TSLIB_LIB) $(STRUCT_LIB)

INCLUDES = -I . -I../tslib -I../common -I../libstructures/ -I../h264bitstream/ -I../logging/
LIBS = -L . -L$(TSLIB_LIB_DIR) -ltslib -L../h264bitstream/.libs -lh264bitstream -L$(LOG_LIB_DIR) -llogging -L$(STRUCT_LIB_DIR) -ldatastruct -lm

CFLAGS  += $(INCLUDES)
LDFLAGS += $(LIBS)

BINARIES = ATSTestApp

all: $(BINARIES)

ATSTestApp: $(OBJS) $(ALL_LIBS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(BINARIES) core
