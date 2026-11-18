/*
 * output.h - the assembled ROM image and the writers that save it.
 */
#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "constants.h"

/* The assembled program: one 16-bit word per address, which addresses have been
 * written, and the highest address written (-1 while the image is empty). */
typedef struct {
    uint16_t mem[MEM_WORDS];
    bool     used[MEM_WORDS];
    int      max_addr;
} Image;

/* The one image the assembler fills in. Defined in output.c. */
extern Image image;

/* Writes the image as 65536 big-endian 16-bit words. Returns false on any file error. */
bool write_bin(const char *path);

/* Writes the image as Logisim "v3.0 hex words addressed" text, from address 0 up to the last
 * word written. Returns false on any file error. */
bool write_v3(const char *path);

#endif /* OUTPUT_H */
