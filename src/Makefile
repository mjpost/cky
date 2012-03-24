# The following flags are for Linux.  
# These are primarily for debugging and optimization
# No flags are really necessary.

CC = gcc
CFLAGS = -O6 $(GCCFLAGS) -finline-functions -fomit-frame-pointer -ffast-math -fstrict-aliasing -Wall
LDFLAGS = -lm

# Debugging
#
#CFLAGS = -g -finline-functions -Wall
#LDFLAGS = -g -lm

# Profiling
#
# CFLAGS = -pg -O6 $(GCCFLAGS) -finline-functions -ffast-math -fstrict-aliasing -Wall
# LDFLAGS = -pg 

all: llncky 

clean:
	rm -f *.o *.tcov *.d *.out core llncky 

llncky: llncky.o hash-string.o mmm.o tree.o ledge.o llgrammar.o vindex.o
