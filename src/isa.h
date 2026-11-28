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

/* The entries in file order: ISAEntryCount() of them, ISAEntryAt(0) .. ISAEntryAt(count - 1). */
int ISAEntryCount(void);
const ISAEntry *ISAEntryAt(int index);

/* Where an operand code (d, a, b or i) sits in an instruction word. The assembler and the disassembler
 * both use these, so the field layout is written down once. ISAFieldMask gives the bits already in place
 * (0 for any other code); ISAFieldShift gives how far the field is shifted up from bit 0. */
unsigned ISAFieldMask(char code);
int ISAFieldShift(char code);

/* Reads the CSV table at path (rows of mnemonic,opcode,operands) into the table; see isa.c for the format.
 * Each bad row is reported on stderr. Returns false when the file cannot be opened
 * or any row was bad. */
bool LoadISATable(const char *path);

#endif /* ISA_H */
