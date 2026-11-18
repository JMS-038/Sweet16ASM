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

Image image = { .max_addr = -1 };

/* bin: 65536 big-endian 16-bit words (131072 bytes). Matches the earlier program2_romJMS image byte for byte. */
bool write_bin(const char *path)
{
    FILE *f = fopen(path, "wb"); int i;
    if (!f) return false;
    for (i = 0; i < MEM_WORDS; i++) { fputc(image.mem[i] >> 8, f); fputc(image.mem[i] & 0xFF, f); }
    return fclose(f) == 0;
}

/* v3: Logisim "v3.0 hex words addressed" text, as saved by the Sweet16 ROMs (microcode_rom.bin,
 * program_rom_matrix_marshall.bin): header line, then "AAAA: w w w w w w w w" with 4-digit lowercase
 * hex, 8 words per line, contiguous from address 0 up to the last word written. Address width is 4
 * digits for the 64K program ROM (the 256-word microcode ROM uses 2). */
bool write_v3(const char *path)
{
    FILE *f = fopen(path, "wb"); int i; /* "wb": keep plain '\n' line endings on Windows too */
    if (!f) return false;
    fputs("v3.0 hex words addressed\n", f);
    for (i = 0; i <= image.max_addr; i++) {
        if (i % 8 == 0) fprintf(f, "%04x:", i);
        fprintf(f, " %04x", image.mem[i]);
        if (i % 8 == 7 || i == image.max_addr) fputc('\n', f);
    }
    return fclose(f) == 0;
}
