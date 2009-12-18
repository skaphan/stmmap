
# You need to install the boost library from www.boost.org.
IDIR =/usr/local/boost_1_41_0
#IDIR = ../boost_1_41_0

CC=gcc
CPP=g++

THREADFLAGS = -DTHREADS

CFLAGS = $(shell ./autoconfigure)

CPLUSPLUSFLAGS = $(CFLAGS) -I$(IDIR)


NLIBSTEM = stm
THLIBSTEM = stm-th

NLIB = lib$(NLIBSTEM).a
THLIB = lib$(THLIBSTEM).a

LIBDIR = -L.

LIBS=-lpthread
THLIBS = -l$(THLIBSTEM) $(LIBS)
NLIBS = -l$(NLIBSTEM) $(LIBS)


OBJ = stm.o stmalloc.o atomic-compat.o

NOBJ = AVLtree.o segalloc.o example.o

THOBJ = segalloc.th.o AVLtree.th.o example.th.o 

TARGETS = autoconfigure stmtest1 stmtest2

all: $(TARGETS)


# this is a single-threaded test program that uses absolute pointers
# in the shared segment.  Uses pthreads library for thread local storage
# which is the same in both single- and multi-threaded versions.
#
stmtest1: example.o $(NLIB)
	$(CC) -o $@ example.o $(LIBDIR) $(NLIBS)


# this is a multi-threading test program that uses position-independent
# relative pointers in the shared segment.
#
stmtest2: example.th.o $(THLIB)
	$(CPP) -o $@ example.th.o $(LIBDIR) $(THLIBS)

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

# The two following rules must appear in the order they appear here.
%.th.o: %.cpp Makefile 
	$(CPP) -c $(CPLUSPLUSFLAGS) $(THREADFLAGS) $< -o $@

%.th.o: %.c Makefile
	$(CC) -c $(CFLAGS) $(THREADFLAGS) $< -o $@

%: %.c
	$(CC) -o $@ $@.c

$(NLIB): $(NLIB)($(OBJ) $(NOBJ))
	ranlib $(NLIB)

$(THLIB): $(THLIB)($(OBJ) $(THOBJ))
	ranlib $(THLIB)

.PHONY: clean

clean:
	rm -f *.o *~ core $(TARGETS) $(NLIB) $(THLIB)
