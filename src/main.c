/*
 * sweet16asm.c - two-pass assembler for the EE340 Sweet16 MCU.
 * Standard C99 library only.
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
static void PrintUsage(void)
{
    fputs("usage: sweet16asm [-i isa.csv] [-o out] [-f v3|bin] [-l listing] input.asm\n"
          "  -i  ISA table (default ./isa.csv)\n"
          "  -f  v3  = Logisim 'v3.0 hex words addressed' text (default)\n"
          "      bin = raw 64K big-endian 16-bit words (131072 bytes)\n", stderr);
}

int main(int argc, char **argv)
{
    const char *isaPath = "isa.csv", *outputPath = NULL, *format = "v3", *listingPath = NULL, *inputPath = NULL;
    char **lines = NULL, lineBuffer[LINE_LEN], defaultOutputPath[512]; int lineCount = 0, capacity = 0, argIndex; FILE *sourceFile; bool wroteOutput;
    AssemblerState state = { NULL };
    for (argIndex = 1; argIndex < argc; argIndex++) {
        if (!strcmp(argv[argIndex], "-i") && argIndex + 1 < argc) isaPath = argv[++argIndex];
        else if (!strcmp(argv[argIndex], "-o") && argIndex + 1 < argc) outputPath = argv[++argIndex];
        else if (!strcmp(argv[argIndex], "-f") && argIndex + 1 < argc) format = argv[++argIndex];
        else if (!strcmp(argv[argIndex], "-l") && argIndex + 1 < argc) listingPath = argv[++argIndex];
        else if (argv[argIndex][0] == '-' || inputPath) { PrintUsage(); return 2; }
        else inputPath = argv[argIndex];
    }
    if (!inputPath || (strcmp(format, "bin") && strcmp(format, "v3"))) { PrintUsage(); return 2; }
    sourceName = inputPath;
    if (!LoadISATable(isaPath)) return 1;
    if (!(sourceFile = fopen(inputPath, "r"))) { fprintf(stderr, "cannot open '%s'\n", inputPath); return 1; }
    while (fgets(lineBuffer, sizeof lineBuffer, sourceFile)) {
        size_t length = strlen(lineBuffer);
        if (length == sizeof lineBuffer - 1 && lineBuffer[length - 1] != '\n') {
            int character; ReportError(lineCount + 1, "line longer than %d characters", LINE_LEN - 2);
            while ((character = fgetc(sourceFile)) != EOF && character != '\n') { }
            lineBuffer[0] = '\0';
        }
        lineBuffer[strcspn(lineBuffer, "\r\n")] = '\0';
        if (lineCount == capacity) { capacity = capacity ? capacity * 2 : 128; lines = realloc(lines, (size_t)capacity * sizeof *lines); if (!lines) return 1; }
        lines[lineCount++] = DuplicateString(lineBuffer);
    }
    fclose(sourceFile);
    if (listingPath && !(state.listingFile = fopen(listingPath, "w"))) { fprintf(stderr, "cannot write '%s'\n", listingPath); return 1; }
    RunPasses(lines, lineCount, &state);
    if (state.listingFile) fclose(state.listingFile);
    if (errorCount) { fprintf(stderr, "%d error(s); no output written\n", errorCount); return 1; }
    if (!outputPath) {
        char *lastDot; snprintf(defaultOutputPath, sizeof defaultOutputPath, "%s", inputPath);
        lastDot = strrchr(defaultOutputPath, '.'); if (lastDot && !strchr(lastDot, '/') && !strchr(lastDot, '\\')) *lastDot = '\0';
        strncat(defaultOutputPath, strcmp(format, "bin") ? ".hex" : ".bin", sizeof defaultOutputPath - strlen(defaultOutputPath) - 1);
        outputPath = defaultOutputPath;
    }
    wroteOutput = strcmp(format, "bin") ? WriteV3Image(outputPath) : WriteBinImage(outputPath);
    if (!wroteOutput) { fprintf(stderr, "cannot write '%s'\n", outputPath); return 1; }
    printf("%s: %d word(s) -> %s (%s)\n", inputPath, image.highestAddress + 1, outputPath, format);
    return 0;
}
