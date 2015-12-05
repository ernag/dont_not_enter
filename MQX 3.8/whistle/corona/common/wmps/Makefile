OBJ = wmps1.o tests.o
INCLUDES=-I../../include
DEFINES=
CFLAGS = -g $(DEFINES) $(INCLUDES)

#===============================================================
# Splint can be found at http://www.splint.org.
# If you're on a mac, you can get it with `brew install splint`.

SPLINT ?= splint
SPLINTFLAGS = -I../../include +weak

default: test

test: tests
	./tests

tests: $(OBJ)
	$(CC) $(CFLAGS) -o tests $(OBJ)

wmps1.o: wmps1.c

tests.o: tests.c

clean:
	rm -f *.o tests

splint: wmps1.c
	$(SPLINT) $(SPLINTFLAGS) $<
	
