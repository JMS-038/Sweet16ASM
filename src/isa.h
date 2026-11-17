/*
 * isa.h - the instruction set table, loaded from isa.csv: mnemonic, opcode and operand fields.
 */
#ifndef ISA_H
#define ISA_H

#include <stdbool.h>

#include "constants.h"

typedef struct {
    char     mnem[NAME_LEN];
    unsigned opcode;
    char     ops[8]; /* letters from {d,a,b,i}, in assembly order; 'k' = whole-word immediate */
    int      nops;
    bool     whole;  /* LDW-style: no type/opcode fields, the 16-bit word is k itself */
} IsaEntry;

/* Returns the table entry for mnemonic m (letter case ignored), or NULL when there is none. */
const IsaEntry *find_isa(const char *m);

/* Reads the CSV table at path (rows of mnemonic,opcode,operands) into the table.
 * Each bad row is reported on stderr. Returns false when the file cannot be opened
 * or any row was bad. */
bool load_isa(const char *path);

#endif /* ISA_H */
