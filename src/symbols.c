/*
 * symbols.c - the label table for the Sweet16 assembler.
 * Moved out of main.c unchanged (only `static` removed from the table and the two functions;
 * the label count stays private to this file because nothing else uses it).
 */
#include <string.h>

#include "constants.h"
#include "symbols.h"
#include "util.h"

Label labelTable[MAX_LABELS];
static int labelCount;

int FindLabel(const char *name)
{
    int index;
    for (index = 0; index < labelCount; index++) if (strcmp(labelTable[index].name, name) == 0) return index;
    return -1;
}

void DefineLabel(const char *name, unsigned address, int lineNumber)
{
    int existingIndex = FindLabel(name);
    if (existingIndex >= 0) { ReportError(lineNumber, "label '%s' already defined on line %d", name, labelTable[existingIndex].lineNumber); return; }
    if (labelCount == MAX_LABELS) { ReportError(lineNumber, "too many labels"); return; }
    strcpy(labelTable[labelCount].name, name); labelTable[labelCount].address = address; labelTable[labelCount].lineNumber = lineNumber;
    labelCount++;
}
