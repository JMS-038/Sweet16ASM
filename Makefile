CC     ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2
sweet16asm: sweet16asm.c
	$(CC) $(CFLAGS) -o $@ $<
test: sweet16asm
	sh ./test.sh
clean:
	rm -f sweet16asm
.PHONY: test clean
