/*
 * symbols.h - the label table: names defined in the source and their addresses.
 */
#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "constants.h"

typedef struct { char name[NAME_LEN]; unsigned addr; int line; } Label;

/* The label table. Entries 0..n-1 are defined, in order of definition; use find_label to look one up. */
extern Label labels[MAX_LABELS];

/* Returns the index of the label called name in labels, or -1 when it is not defined. */
int find_label(const char *name);

/* Adds a label at addr, defined on source line line. Reports an error (and adds nothing)
 * when the name is already defined or the table is full. */
void define_label(const char *name, unsigned addr, int line);

#endif /* SYMBOLS_H */
