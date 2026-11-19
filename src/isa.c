/*
 * isa.c - loads and looks up the instruction set table for the Sweet16 assembler.
 * Moved out of main.c unchanged (only `static` removed from the two functions;
 * the table itself stays private to this file because nothing else reads it).
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "isa.h"
#include "util.h"

static ISAEntry isaTable[MAX_ISA];   static int isaEntryCount;

const ISAEntry *FindISAEntry(const char *mnemonic)
{
    int index;
    for (index = 0; index < isaEntryCount; index++) if (EqualsIgnoreCase(isaTable[index].mnemonic, mnemonic)) return &isaTable[index];
    return NULL;
}

bool LoadISATable(const char *path)
{
    FILE *file = fopen(path, "r");
    char rowBuffer[LINE_LEN]; int lineNumber = 0; bool allRowsValid = true;
    if (!file) { fprintf(stderr, "cannot open ISA table '%s' (use -i <file>)\n", path); return false; }
    while (fgets(rowBuffer, sizeof rowBuffer, file)) {
        char *text = TrimWhitespace(rowBuffer), *fields[3] = {NULL, NULL, NULL}, *cursor, *comma, *mnemonic, *opcodeText, *operandText;
        int fieldCount = 0; size_t codeIndex; ISAEntry entry;
        lineNumber++;
        if (!*text || *text == '#') continue;
        cursor = text;
        while (fieldCount < 3) {
            fields[fieldCount++] = cursor;
            comma = strchr(cursor, ',');
            if (!comma) break;
            *comma = '\0'; cursor = comma + 1;
        }
        mnemonic = TrimWhitespace(fields[0]);
        if (EqualsIgnoreCase(mnemonic, "mnemonic")) continue; /* header row */
        if (fieldCount < 2) { fprintf(stderr, "%s:%d: need mnemonic,opcode,operands\n", path, lineNumber); allRowsValid = false; continue; }
        opcodeText = TrimWhitespace(fields[1]); operandText = fieldCount > 2 ? TrimWhitespace(fields[2]) : (char *)"";
        memset(&entry, 0, sizeof entry);
        if (!IsIdentifier(mnemonic) || strlen(mnemonic) >= sizeof entry.mnemonic) { fprintf(stderr, "%s:%d: bad mnemonic '%s'\n", path, lineNumber, mnemonic); allRowsValid = false; continue; }
        strcpy(entry.mnemonic, mnemonic);
        if (strlen(operandText) >= sizeof entry.operandCodes) { fprintf(stderr, "%s:%d: too many operands\n", path, lineNumber); allRowsValid = false; continue; }
        if (strcmp(opcodeText, "-") == 0) { /* whole-word instruction (LDW): no opcode field */
            if (strcmp(operandText, "k") != 0) { fprintf(stderr, "%s:%d: opcode '-' needs operand code 'k'\n", path, lineNumber); allRowsValid = false; continue; }
            entry.isWholeWord = true;
        } else {
            if (strlen(opcodeText) != 5 || strspn(opcodeText, "01") != 5) { fprintf(stderr, "%s:%d: opcode must be 5 binary digits (or '-')\n", path, lineNumber); allRowsValid = false; continue; }
            entry.opcode = (unsigned)strtoul(opcodeText, NULL, 2);
            for (codeIndex = 0; operandText[codeIndex]; codeIndex++) {
                if (!strchr("dabi", operandText[codeIndex]) || memchr(operandText, operandText[codeIndex], codeIndex)) { fprintf(stderr, "%s:%d: bad operand code '%s'\n", path, lineNumber, operandText); allRowsValid = false; break; }
            }
            if (operandText[codeIndex]) continue;
            if (strchr(operandText, 'i') && (strchr(operandText, 'a') || strchr(operandText, 'b'))) { fprintf(stderr, "%s:%d: 'i' overlaps the a/b fields\n", path, lineNumber); allRowsValid = false; continue; }
        }
        strcpy(entry.operandCodes, operandText); entry.operandCount = (int)strlen(operandText);
        if (FindISAEntry(entry.mnemonic)) { fprintf(stderr, "%s:%d: duplicate mnemonic %s\n", path, lineNumber, entry.mnemonic); allRowsValid = false; continue; }
        if (isaEntryCount == MAX_ISA) { fprintf(stderr, "%s: too many instructions\n", path); allRowsValid = false; break; }
        isaTable[isaEntryCount++] = entry;
    }
    fclose(file);
    return allRowsValid;
}
