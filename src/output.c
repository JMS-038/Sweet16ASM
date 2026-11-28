/*
 * output.c - the ROM image (the one shared Image) and the two writers that save it: raw binary and
 * Logisim "v3.0 hex words addressed" text. Also the two readers that load such a file back into the
 * image for the disassembler (ReadV3Image, ReadBinImage), DetectImageFormat, and PublishListing,
 * which saves the listing once assembly has succeeded.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "output.h"
#include "util.h"

/* The first line of every v3 file. */
#define V3_HEADER "v3.0 hex words addressed"

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
    fputs(V3_HEADER "\n", file);
    /* one word per pass: start a row every 8 words, and end it after the 8th word or the last word written */
    for (address = 0; address <= image.highestAddress; address++) {
        if (address % 8 == 0) fprintf(file, "%04x:", address);
        fprintf(file, " %04x", image.words[address]);
        if (address % 8 == 7 || address == image.highestAddress) fputc('\n', file);
    }
    /* fclose can fail when the final buffered write does, so its result counts */
    return fclose(file) == 0;
}

/*
 * DetectImageFormat - tell a v3 text image from a raw binary one by its first bytes.
 * path: the saved image.
 * Returns "v3" when the file starts with "v3.0" (the v3 header), "bin" otherwise. An unreadable file
 * gives "v3"; ReadV3Image then reports that it cannot be opened.
 */
const char *DetectImageFormat(const char *path)
{
    FILE *file = fopen(path, "rb"); char start[4]; size_t count;
    if (!file) return "v3";
    count = fread(start, 1, sizeof start, file);
    fclose(file);
    return count == sizeof start && !memcmp(start, "v3.0", sizeof start) ? "v3" : "bin";
}

/*
 * ReadBinImage - load a raw binary image (the format WriteBinImage writes) into the shared image.
 * path: the file; it must hold exactly 131072 bytes, the 65536 words big-endian (high byte first).
 * Words are marked written up to the last non-zero word. A raw image carries no record of which
 * words were written, and zero is also what WriteBinImage puts in every word that was not, so the
 * zero words after the last non-zero one are left out. Zero words before it stay, as LDW 0.
 * Returns true on success; false (after a message on stderr) if the file cannot be opened or is not
 * exactly 131072 bytes.
 */
bool ReadBinImage(const char *path)
{
    FILE *file = fopen(path, "rb"); int address, high, low;
    if (!file) { fprintf(stderr, "cannot open '%s'\n", path); return false; }
    /* two bytes per word, high byte first */
    for (address = 0; address < MEM_WORDS; address++) {
        high = fgetc(file); low = fgetc(file);
        if (high == EOF || low == EOF) { fprintf(stderr, "%s: a raw image must be %d bytes (%d big-endian words); this file is shorter\n", path, MEM_WORDS * 2, MEM_WORDS); fclose(file); return false; }
        image.words[address] = (uint16_t)((high << 8) | low);
    }
    /* anything after the last word means the file is the wrong size too */
    if (fgetc(file) != EOF) { fprintf(stderr, "%s: a raw image must be %d bytes (%d big-endian words); this file is longer\n", path, MEM_WORDS * 2, MEM_WORDS); fclose(file); return false; }
    fclose(file);
    /* find the last non-zero word; everything up to it counts as written */
    image.highestAddress = -1;
    for (address = 0; address < MEM_WORDS; address++) if (image.words[address]) image.highestAddress = address;
    for (address = 0; address <= image.highestAddress; address++) image.isWritten[address] = true;
    return true;
}

/*
 * ReadV3Image - load a Logisim "v3.0 hex words addressed" file (the format WriteV3Image writes)
 * into the shared image.
 * path: the file. The first non-blank line must be the header. After it each non-blank row is
 * "AAAA: w w w ...": a 1-4 digit hex address and colon, then any number of 1-4 digit hex words
 * (either letter case, spaces or tabs between) for AAAA, AAAA+1, and so on. Rows may start anywhere
 * and appear in any order; gaps are left unwritten. Other forms, such as Logisim's "count*word"
 * repeats, are reported as bad words rather than guessed at.
 * Every problem in the file is reported as "path:line: error: message" (through ReportError, so
 * sourceName must already be set to path), so one run lists them all.
 * Returns true only if the file opened and no problem was found.
 */
bool ReadV3Image(const char *path)
{
    FILE *file = fopen(path, "r"); char lineBuffer[LINE_LEN]; int lineNumber = 0; bool sawHeader = false, allRowsValid = true;
    if (!file) { fprintf(stderr, "cannot open '%s'\n", path); return false; }
    /* one row per pass */
    while (fgets(lineBuffer, sizeof lineBuffer, file)) {
        size_t length = strlen(lineBuffer), digits; char *text, *cursor; unsigned long address; int offset = 0;
        lineNumber++;
        /* the buffer filled without reaching the end of the line: it is longer than the limit */
        if (length == sizeof lineBuffer - 1 && lineBuffer[length - 1] != '\n') {
            int character; ReportError(lineNumber, "line longer than %d characters", LINE_LEN - 2);
            while ((character = fgetc(file)) != EOF && character != '\n') { }
            allRowsValid = false; continue;
        }
        text = TrimWhitespace(lineBuffer);
        if (!*text) continue;
        /* the first non-blank line must be the header; a file without it is not a v3 image at all */
        if (!sawHeader) {
            if (strcmp(text, V3_HEADER)) { ReportError(lineNumber, "not a v3 image: the first line must be \"%s\"", V3_HEADER); fclose(file); return false; }
            sawHeader = true; continue;
        }
        /* the address: 1-4 hex digits, then ':' */
        digits = strspn(text, "0123456789abcdefABCDEF");
        if (digits < 1 || digits > 4 || text[digits] != ':') { ReportError(lineNumber, "bad address in '%s' (expected 1-4 hex digits and a colon)", text); allRowsValid = false; continue; }
        text[digits] = '\0'; address = strtoul(text, NULL, 16); cursor = text + digits + 1;
        /* the words, one per pass, until the end of the row or the first bad word */
        for (;;) {
            unsigned long word; size_t position;
            cursor += strspn(cursor, " \t");
            if (!*cursor) break;
            digits = strspn(cursor, "0123456789abcdefABCDEF");
            if (digits < 1 || digits > 4 || (cursor[digits] && cursor[digits] != ' ' && cursor[digits] != '\t')) {
                ReportError(lineNumber, "bad word '%.*s' (expected 1-4 hex digits)", (int)strcspn(cursor, " \t"), cursor); allRowsValid = false; break;
            }
            if (address + (unsigned long)offset >= MEM_WORDS) { ReportError(lineNumber, "address 0x%lX is beyond the 64K image", address + (unsigned long)offset); allRowsValid = false; break; }
            if (image.isWritten[address + (unsigned long)offset]) { ReportError(lineNumber, "address 0x%04lX given twice", address + (unsigned long)offset); allRowsValid = false; break; }
            /* turn the hex digits into a number (strtoul is avoided: it would also accept a sign or 0x) */
            word = 0;
            for (position = 0; position < digits; position++) {
                char digit = cursor[position];
                word = word * 16 + (unsigned long)(digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
            }
            image.words[address + (unsigned long)offset] = (uint16_t)word; image.isWritten[address + (unsigned long)offset] = true;
            if ((int)(address + (unsigned long)offset) > image.highestAddress) image.highestAddress = (int)(address + (unsigned long)offset);
            offset++; cursor += digits;
        }
    }
    fclose(file);
    if (!sawHeader) { ReportError(1, "not a v3 image: the file is empty (the first line must be \"%s\")", V3_HEADER); return false; }
    return allRowsValid;
}

/*
 * PublishListing - save the finished listing to its real file. The passes write the listing to a
 * temporary file, and main calls this only when assembly had no errors, so a failed run never leaves
 * a partial listing behind.
 * temporaryFile: the open temporary file holding the listing.
 * path: the listing file to create (overwritten if it exists). Opened in text mode, as the listing
 * always was.
 * Returns true on success; false if the file cannot be opened, written or closed cleanly.
 */
bool PublishListing(FILE *temporaryFile, const char *path)
{
    FILE *file; char chunk[4096]; size_t count; bool wroteAll = true;
    if (fflush(temporaryFile) != 0) return false;
    rewind(temporaryFile);
    if (!(file = fopen(path, "w"))) return false;
    /* copy the temporary file in chunks until it is used up or a write fails */
    while ((count = fread(chunk, 1, sizeof chunk, temporaryFile)) > 0)
        if (fwrite(chunk, 1, count, file) != count) { wroteAll = false; break; }
    /* fclose can fail when the final buffered write does, so its result counts */
    return fclose(file) == 0 && wroteAll;
}
