/*
 * output.h - the assembled ROM image, the writers that save it, and the listing copy step.
 */
#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "constants.h"

/* The assembled program: one 16-bit word per address, which addresses have been
 * written, and the highest address written (-1 while the image is empty). */
typedef struct {
    uint16_t words[MEM_WORDS];
    bool     isWritten[MEM_WORDS];
    int      highestAddress;
} Image;

/* The one image the assembler fills in. Defined in output.c. */
extern Image image;

/* Writes the image as 65536 big-endian 16-bit words. Returns false on any file error. */
bool WriteBinImage(const char *path);

/* Writes the image as Logisim "v3.0 hex words addressed" text, from address 0 up to the last
 * word written. Returns false on any file error. */
bool WriteV3Image(const char *path);

/* Copies the listing built in temporaryFile (read from its start) to path, creating or replacing the
 * file. Returns false on any file error. The caller closes temporaryFile. */
bool PublishListing(FILE *temporaryFile, const char *path);

#endif /* OUTPUT_H */
