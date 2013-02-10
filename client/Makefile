#-----------------------------------------------------------------------------
# This code is licensed to you under the terms of the GNU GPL, version 2 or,
# at your option, any later version. See the LICENSE.txt file for the text of
# the license.
#-----------------------------------------------------------------------------
include Makefile.common

CC=gcc
CXX=g++
#COMMON_FLAGS = -m32

COMMON_INCLUDE = ../include

OBJDIR = obj

LDLIBS = -L/opt/local/lib -L/usr/local/lib -lusb -lreadline -lpthread
LDFLAGS = $(COMMON_FLAGS)
CFLAGS = -std=gnu99 -I. -I$(COMMON_INCLUDE) -I../common -I/opt/local/include -Wall -Wno-unused-function $(COMMON_FLAGS) -g -O3

RM = rm -f
BINS = flasher
CLEAN = flasher flasher.exe $(OBJDIR)/*.o *.o *.moc.cpp

setup:
	mkdir -p $(OBJDIR)
	
all: setup $(BINS)

flasher: $(OBJDIR)/flash.o $(OBJDIR)/flasher.o $(OBJDIR)/flasherusb.o
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(CLEAN)

.PHONY: all clean
