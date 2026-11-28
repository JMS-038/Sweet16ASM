/*
 * output.h - the ROM image, the writers that save it, the readers that load a saved one for the
 * disassembler, and the listing copy step.
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

/* Reads a saved image into the shared image, as the disassembler needs it. Both return false after
 * reporting what is wrong on stderr.
 *  ReadV3Image: the Logisim "v3.0 hex words addressed" text that WriteV3Image writes (rows may start
 *    at any address, in any order; an address given twice is an error). Report problems with
 *    ReportError, so set sourceName to path first.
 *  ReadBinImage: exactly 131072 bytes, 65536 big-endian words. Zero words after the last non-zero
 *    one are treated as not written, because WriteBinImage pads every image with them. */
bool ReadV3Image(const char *path);
bool ReadBinImage(const char *path);

/* Guesses the format of a saved image from its first bytes: "v3" if the file starts with the v3.0
 * header, otherwise "bin". A file that cannot be opened gives "v3"; the reader then reports it. */
const char *DetectImageFormat(const char *path);

/* Copies the listing built in temporaryFile (read from its start) to path, creating or replacing the
 * file. Returns false on any file error. The caller closes temporaryFile. */
bool PublishListing(FILE *temporaryFile, const char *path);

#endif /* OUTPUT_H */
