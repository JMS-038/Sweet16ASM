/*
 * output.c - the assembled ROM image and the writers that save it (bin and Logisim v3 hex).
 * The writers are moved out of main.c with one mechanical change: the image state that used to
 * be three separate globals (mem, used, max_addr) is now one shared Image, so mem[i] reads
 * image.mem[i] and max_addr reads image.max_addr.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "constants.h"
#include "output.h"

Image image = { .highestAddress = -1 };

/* bin: 65536 big-endian 16-bit words (131072 bytes). Matches the earlier program2_romJMS image byte for byte. */
bool WriteBinImage(const char *path)
{
    FILE *file = fopen(path, "wb"); int address;
    if (!file) return false;
    for (address = 0; address < MEM_WORDS; address++) { fputc(image.words[address] >> 8, file); fputc(image.words[address] & 0xFF, file); }
    return fclose(file) == 0;
}

/* v3: Logisim "v3.0 hex words addressed" text, as saved by the Sweet16 ROMs (microcode_rom.bin,
 * program_rom_matrix_marshall.bin): header line, then "AAAA: w w w w w w w w" with 4-digit lowercase
 * hex, 8 words per line, contiguous from address 0 up to the last word written. Address width is 4
 * digits for the 64K program ROM (the 256-word microcode ROM uses 2). */
bool WriteV3Image(const char *path)
{
    FILE *file = fopen(path, "wb"); int address; /* "wb": keep plain '\n' line endings on Windows too */
    if (!file) return false;
    fputs("v3.0 hex words addressed\n", file);
    for (address = 0; address <= image.highestAddress; address++) {
        if (address % 8 == 0) fprintf(file, "%04x:", address);
        fprintf(file, " %04x", image.words[address]);
        if (address % 8 == 7 || address == image.highestAddress) fputc('\n', file);
    }
    return fclose(file) == 0;
}
