/*
 * symbols.c - the label table for the Sweet16 assembler.
 * Moved out of main.c unchanged (only `static` removed from the table and the two functions;
 * the label count stays private to this file because nothing else uses it).
 */
#include <string.h>

#include "constants.h"
#include "symbols.h"
#include "util.h"

Label labels[MAX_LABELS];
static int n_labels;

int find_label(const char *name)
{
    int i;
    for (i = 0; i < n_labels; i++) if (strcmp(labels[i].name, name) == 0) return i;
    return -1;
}

void define_label(const char *name, unsigned addr, int line)
{
    int i = find_label(name);
    if (i >= 0) { ReportError(line, "label '%s' already defined on line %d", name, labels[i].line); return; }
    if (n_labels == MAX_LABELS) { ReportError(line, "too many labels"); return; }
    strcpy(labels[n_labels].name, name); labels[n_labels].addr = addr; labels[n_labels].line = line;
    n_labels++;
}
