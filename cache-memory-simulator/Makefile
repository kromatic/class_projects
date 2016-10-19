CFLAGS = -g -Wall -Werror -std=c99
CC = gcc

all: csim.c cachelab.c cachelab.h
	$(CC) $(CFLAGS) -o csim csim.c cachelab.c -lm

#
# Clean the src dirctory
#
clean:
	rm -rf *.o
	rm -f csim
	rm -f test-trans tracegen
	rm -f trace.all trace.f*
	rm -f .csim_results .marker
