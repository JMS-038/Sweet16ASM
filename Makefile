# Builds the assembler. Works with GNU make on Linux/macOS and on Windows (MSYS2 make + MinGW gcc).
ifeq ($(OS),Windows_NT)
  EXE = .exe
  CC  = gcc
else
  EXE =
  CC ?= cc
endif
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2
TARGET  = sweet16asm$(EXE)

$(TARGET): sweet16asm.c
	$(CC) $(CFLAGS) -o $@ $<

test: $(TARGET)
	sh ./test.sh

clean:
	rm -f $(TARGET)

.PHONY: test clean
