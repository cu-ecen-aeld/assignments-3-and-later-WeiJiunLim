# AESD Assignment 2 Makefile
# Author: Wei Jiun Lim

# Use specific cross-compiler if specified, else use default gcc
# Example: 
#     Use default GCC:
#        make
#     Use aarch64-none-linux-gnu- cross compiler:
#        make CROSS_COMPILE=aarch64-none-linux-gnu-
CC = $(CROSS_COMPILE)gcc

# Create writer application from writer object
writer: writer.o
	$(CC) writer.o -o writer

# Create writer object from C source
writer.o : writer.c
	$(CC) -c writer.c -o writer.o

# Clean writer application and writer object
.PHONY: clean
clean:
	rm -f writer writer.o
