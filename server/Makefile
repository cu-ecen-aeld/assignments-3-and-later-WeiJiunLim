# AESD Assignment 5 Part 1 Makefile
# Author: Wei Jiun Lim

# Use specific cross-compiler if specified, else use default gcc
# Example: 
#     Use default GCC:
#        make
#     Use aarch64-none-linux-gnu- cross compiler:
#        make CROSS_COMPILE=aarch64-none-linux-gnu-
CC = $(CROSS_COMPILE)gcc

.PHONY: all clean

# Default build
all: aesdsocket

# Create aesdsocket application from aesdsocket object
aesdsocket: aesdsocket.o
	$(CC) aesdsocket.o -o aesdsocket

# Create aesdsocket object from C source
aesdsocket.o : aesdsocket.c
	$(CC) -c aesdsocket.c -o aesdsocket.o -Wall -Werror

# Clean aesdsocket application and aesdsocket object
clean:
	rm -f aesdsocket aesdsocket.o
