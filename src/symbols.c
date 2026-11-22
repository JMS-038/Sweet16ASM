/*
 * symbols.c - the label table: where each label was defined and its address. Pass 1 fills it with
 * DefineLabel; pass 2 and the jump pseudo-ops look labels up with FindLabel.
 */
#include <string.h>

#include "constants.h"
#include "symbols.h"
#include "util.h"

/* The defined labels, in the order they were defined. Only entries 0 .. labelCount-1 are in use. */
Label labelTable[MAX_LABELS];
/* How many entries of labelTable are in use. Private: other modules go through FindLabel. */
static int labelCount;

/*
 * FindLabel - look a label up by name. Letter case matters: "Loop" and "loop" are different labels.
 * Returns its index in labelTable, or -1 when no label has that name.
 */
int FindLabel(const char *name)
{
    int index;
    /* linear search: the table holds at most MAX_LABELS entries */
    for (index = 0; index < labelCount; index++) if (strcmp(labelTable[index].name, name) == 0) return index;
    return -1;
}

/*
 * DefineLabel - record that the label name is at address, defined on source line lineNumber.
 * address: the label's position in words (the address the next instruction gets).
 * Reports an error and records nothing when the name is already defined (the message gives the
 * earlier line) or the table is full.
 */
void DefineLabel(const char *name, unsigned address, int lineNumber)
{
    int existingIndex = FindLabel(name);
    if (existingIndex >= 0) { ReportError(lineNumber, "label '%s' already defined on line %d", name, labelTable[existingIndex].lineNumber); return; }
    if (labelCount == MAX_LABELS) { ReportError(lineNumber, "too many labels"); return; }
    strcpy(labelTable[labelCount].name, name); labelTable[labelCount].address = address; labelTable[labelCount].lineNumber = lineNumber;
    labelCount++;
}
