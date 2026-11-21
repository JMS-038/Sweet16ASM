/*
 * isa.h - the instruction set table, loaded from isa.csv: mnemonic, opcode and operand fields.
 */
#ifndef ISA_H
#define ISA_H

#include <stdbool.h>

#include "constants.h"

typedef struct {
    char     mnemonic[NAME_LEN];
    unsigned opcode;
    char     operandCodes[8]; /* letters from {d,a,b,i}, in assembly order; 'k' = whole-word immediate */
    int      operandCount;
    bool     isWholeWord;  /* LDW-style: no type/opcode fields, the 16-bit word is k itself */
} ISAEntry;

/* Returns the table entry for mnemonic (letter case ignored), or NULL when there is none. */
const ISAEntry *FindISAEntry(const char *mnemonic);

/* Reads the CSV table at path (rows of mnemonic,opcode,operands) into the table; see isa.c for the format.
 * Each bad row is reported on stderr. Returns false when the file cannot be opened
 * or any row was bad. */
bool LoadISATable(const char *path);

#endif /* ISA_H */
