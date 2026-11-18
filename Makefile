# Builds the assembler. Works with GNU make on Linux/macOS and on Windows (MSYS2 make + MinGW gcc).
#
# Every src/*.c file is one module of the program. To add a module, put name.c and name.h in src/;
# there is nothing to edit here. Any change to a source or header rebuilds the whole program
# (it is small, so a full rebuild is quick), so there are no stale object files to clean up.
ifeq ($(OS),Windows_NT)
  EXE = .exe
  CC  = gcc
else
  EXE =
  CC ?= cc
endif
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2
TARGET  = sweet16asm$(EXE)
SRCS    = $(wildcard src/*.c)
HDRS    = $(wildcard src/*.h)

# One compile-and-link step: CC and CFLAGS given on the command line still apply to every file.
$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test: $(TARGET)
	sh ./test.sh

clean:
	rm -f $(TARGET)

.PHONY: test clean
