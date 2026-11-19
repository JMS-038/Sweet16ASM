/*
 * symbols.h - the label table: names defined in the source and their addresses.
 */
#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "constants.h"

typedef struct { char name[NAME_LEN]; unsigned address; int lineNumber; } Label;

/* The label table. Entries 0..n-1 are defined, in order of definition; use FindLabel to look one up. */
extern Label labelTable[MAX_LABELS];

/* Returns the index of the label called name in labelTable, or -1 when it is not defined. */
int FindLabel(const char *name);

/* Adds a label at address, defined on source line lineNumber. Reports an error (and adds nothing)
 * when the name is already defined or the table is full. */
void DefineLabel(const char *name, unsigned address, int lineNumber);

#endif /* SYMBOLS_H */
