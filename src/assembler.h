/*
 * assembler.h - the two assembly passes: from source lines to the assembled image.
 */
#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdio.h>

/* Settings and outputs the passes share with the caller. */
typedef struct {
    FILE *listing; /* receives one listing line per word written; NULL means no listing */
} AssemblerState;

/* Assembles the nlines source lines into image (see output.h).
 * Pass 1 assigns addresses and defines labels; pass 2 encodes each statement.
 * Problems are reported with ReportError; the caller checks errorCount afterwards. */
void run_passes(char **lines, int nlines, const AssemblerState *state);

#endif /* ASSEMBLER_H */
