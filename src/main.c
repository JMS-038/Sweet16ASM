/*
 * main.c - command line for sweet16asm, the two-pass assembler for the EE340 Sweet16 MCU.
 * Standard C99 library only.
 * main reads the options and the source file, loads the ISA table, hands the lines to the two passes
 * (assembler.c) and saves the image (output.c). The README lists the other modules.
 *
 * Instruction word: 01-ooooo-ddd-aaa-bbb   (LDI: 01-01001-ddd-iiiiii)
 * The opcode and which operands fill which fields come from isa.csv.
 *
 * Output: Logisim 'v3.0 hex words addressed' text (default) or a raw 64K big-endian image.
 *
 * Source syntax:
 *   ; comment (everything after ';' is ignored)
 *   label:  MNEMONIC op, op, op       (label optional, own line or same line)
 *   Registers r0-r7. Numbers: decimal, 0x hex, 0b binary, optional sign.
 *   Directives: .org <addr>   (forward only)     .word <value|label>
 *   LDW k       whole-word immediate into R7 (k = -16384..16383, or a label address)
 *   Pseudo-ops: NOP | MOV rd,ra | CLR rd | JMP rt,label | JZL ra,rt,label
 *               LJMP label | LJZ ra,label   (long jumps through R7, built on LDW)
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assembler.h"
#include "constants.h"
#include "isa.h"
#include "output.h"
#include "util.h"

/* ---------- main ---------- */
/* PrintUsage - print the command-line summary to stderr. main calls it for a bad command line, then returns 2. */
static void PrintUsage(void)
{
    fputs("usage: sweet16asm [-i isa.csv] [-o out] [-f v3|bin] [-l listing] input.asm\n"
          "  -i  ISA table (default ./isa.csv)\n"
          "  -f  v3  = Logisim 'v3.0 hex words addressed' text (default)\n"
          "      bin = raw 64K big-endian 16-bit words (131072 bytes)\n", stderr);
}

/*
 * main - assemble the file named on the command line.
 * Usage: sweet16asm [-i isa.csv] [-o out] [-f v3|bin] [-l listing] input.asm
 * Steps: read the options, load the ISA table, read the source lines, run the two passes and, only if
 * no error was reported, write the image and print a one-line summary.
 * Returns 0 on success; 1 when a file cannot be read or written or any assembly error was reported
 * (no output is written then); 2 for a bad command line.
 */
int main(int argc, char **argv)
{
    const char *isaPath = "isa.csv", *outputPath = NULL, *format = "v3", *listingPath = NULL, *inputPath = NULL;
    char **lines = NULL, lineBuffer[LINE_LEN], defaultOutputPath[512]; int lineCount = 0, capacity = 0, argIndex; FILE *sourceFile; bool wroteOutput;
    AssemblerState state = { NULL };
    /* read the options; the one argument that is not an option is the input file */
    for (argIndex = 1; argIndex < argc; argIndex++) {
        if (!strcmp(argv[argIndex], "-i") && argIndex + 1 < argc) isaPath = argv[++argIndex];
        else if (!strcmp(argv[argIndex], "-o") && argIndex + 1 < argc) outputPath = argv[++argIndex];
        else if (!strcmp(argv[argIndex], "-f") && argIndex + 1 < argc) format = argv[++argIndex];
        else if (!strcmp(argv[argIndex], "-l") && argIndex + 1 < argc) listingPath = argv[++argIndex];
        /* an unknown option, an option missing its value, or a second input file is a usage error */
        else if (argv[argIndex][0] == '-' || inputPath) { PrintUsage(); return 2; }
        else inputPath = argv[argIndex];
    }
    /* an input file is required, and the format must be v3 or bin */
    if (!inputPath || (strcmp(format, "bin") && strcmp(format, "v3"))) { PrintUsage(); return 2; }
    /* error messages show the input path */
    sourceName = inputPath;
    if (!LoadISATable(isaPath)) return 1;
    if (!(sourceFile = fopen(inputPath, "r"))) { fprintf(stderr, "cannot open '%s'\n", inputPath); return 1; }
    /* read the source one line at a time into the lines array, which grows as needed */
    while (fgets(lineBuffer, sizeof lineBuffer, sourceFile)) {
        size_t length = strlen(lineBuffer);
        /* the buffer filled without reaching the end of the line: it is longer than the limit */
        if (length == sizeof lineBuffer - 1 && lineBuffer[length - 1] != '\n') {
            int character; ReportError(lineCount + 1, "line longer than %d characters", LINE_LEN - 2);
            /* discard the rest of the long line */
            while ((character = fgetc(sourceFile)) != EOF && character != '\n') { }
            /* keep an empty line in its place, so later line numbers stay right */
            lineBuffer[0] = '\0';
        }
        /* strip the line ending (\n or \r\n) */
        lineBuffer[strcspn(lineBuffer, "\r\n")] = '\0';
        /* double the array when it is full (it starts with room for 128 lines) */
        if (lineCount == capacity) { capacity = capacity ? capacity * 2 : 128; lines = realloc(lines, (size_t)capacity * sizeof *lines); if (!lines) return 1; }
        /* the lines are never freed: the program ends right after assembling */
        lines[lineCount++] = DuplicateString(lineBuffer);
    }
    fclose(sourceFile);
    /* open the listing file if one was asked for; the passes write to it */
    if (listingPath && !(state.listingFile = fopen(listingPath, "w"))) { fprintf(stderr, "cannot write '%s'\n", listingPath); return 1; }
    /* assemble; every problem is reported as it is found and counted in errorCount */
    RunPasses(lines, lineCount, &state);
    if (state.listingFile) fclose(state.listingFile);
    /* no output file is written when there were errors */
    if (errorCount) { fprintf(stderr, "%d error(s); no output written\n", errorCount); return 1; }
    /* default output name: the input name with its extension replaced by .hex (v3) or .bin;
     * a '.' in a directory name is not an extension */
    if (!outputPath) {
        char *lastDot; snprintf(defaultOutputPath, sizeof defaultOutputPath, "%s", inputPath);
        lastDot = strrchr(defaultOutputPath, '.'); if (lastDot && !strchr(lastDot, '/') && !strchr(lastDot, '\\')) *lastDot = '\0';
        strncat(defaultOutputPath, strcmp(format, "bin") ? ".hex" : ".bin", sizeof defaultOutputPath - strlen(defaultOutputPath) - 1);
        outputPath = defaultOutputPath;
    }
    /* save the image in the chosen format */
    wroteOutput = strcmp(format, "bin") ? WriteV3Image(outputPath) : WriteBinImage(outputPath);
    if (!wroteOutput) { fprintf(stderr, "cannot write '%s'\n", outputPath); return 1; }
    printf("%s: %d word(s) -> %s (%s)\n", inputPath, image.highestAddress + 1, outputPath, format);
    return 0;
}
