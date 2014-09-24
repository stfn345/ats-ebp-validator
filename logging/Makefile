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

INCLUDES = -I . -I../common  
LIBS = -L . 

CFLAGS  += $(INCLUDES)
LDFLAGS += $(LIBS)

all: liblogging.a 

liblogging.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^ 
	$(RANLIB) $@

clean:
	rm -f $(OBJS) *.a core
