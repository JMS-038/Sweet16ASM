/*
 * output.c - the assembled ROM image (the one shared Image) and the two writers that save it:
 * raw binary and Logisim "v3.0 hex words addressed" text.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "constants.h"
#include "output.h"

/* The assembled program. Every word starts as 0 and unwritten; highestAddress is -1 until a word is written. */
Image image = { .highestAddress = -1 };

/*
 * WriteBinImage - save the image as a raw binary file: all 65536 words, big-endian (high byte first),
 * 131072 bytes in total, however much of the image was written. Matches the earlier
 * program2_romJMS image byte for byte.
 * path: the file to create (overwritten if it exists).
 * Returns true on success; false if the file cannot be opened or closed cleanly.
 */
bool WriteBinImage(const char *path)
{
    FILE *file = fopen(path, "wb"); int address;
    if (!file) return false;
    /* every word, high byte first and then the low byte (big-endian) */
    for (address = 0; address < MEM_WORDS; address++) { fputc(image.words[address] >> 8, file); fputc(image.words[address] & 0xFF, file); }
    /* fclose can fail when the final buffered write does, so its result counts */
    return fclose(file) == 0;
}

/*
 * WriteV3Image - save the image as Logisim "v3.0 hex words addressed" text, as saved by the Sweet16
 * ROMs (microcode_rom.bin, program_rom_matrix_marshall.bin): a header line, then one row per 8
 * words, "AAAA: w w w w w w w w", in 4-digit lowercase hex. Rows are contiguous from address 0 up to
 * the last word written, so a program that ends early gives a short file (an empty program gives
 * just the header). The address is 4 digits wide for the 64K program ROM (the 256-word microcode
 * ROM uses 2).
 * path: the file to create (overwritten if it exists). It is opened in binary mode so line ends are
 * a plain '\n' on Windows too.
 * Returns true on success; false if the file cannot be opened or closed cleanly.
 */
bool WriteV3Image(const char *path)
{
    FILE *file = fopen(path, "wb"); int address; /* "wb": keep plain '\n' line endings on Windows too */
    if (!file) return false;
    fputs("v3.0 hex words addressed\n", file);
    /* one word per pass: start a row every 8 words, and end it after the 8th word or the last word written */
    for (address = 0; address <= image.highestAddress; address++) {
        if (address % 8 == 0) fprintf(file, "%04x:", address);
        fprintf(file, " %04x", image.words[address]);
        if (address % 8 == 7 || address == image.highestAddress) fputc('\n', file);
    }
    /* fclose can fail when the final buffered write does, so its result counts */
    return fclose(file) == 0;
}
