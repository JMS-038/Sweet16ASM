/*
 * isa.c - the instruction table. LoadISATable reads isa.csv into a private table at startup and
 * FindISAEntry looks mnemonics up in it. Opcodes and operand layouts come only from this table.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "isa.h"
#include "util.h"

/* The loaded instructions in file order. Only entries 0 .. isaEntryCount-1 are in use. */
static ISAEntry isaTable[MAX_ISA];   static int isaEntryCount;

/*
 * FindISAEntry - look an instruction up by mnemonic (letter case ignored).
 * Returns a pointer into the table, or NULL when there is no such instruction. The table does not
 * change after LoadISATable returns, so the pointer stays valid.
 */
const ISAEntry *FindISAEntry(const char *mnemonic)
{
    int index;
    /* linear search over the loaded entries */
    for (index = 0; index < isaEntryCount; index++) if (EqualsIgnoreCase(isaTable[index].mnemonic, mnemonic)) return &isaTable[index];
    return NULL;
}

/*
 * LoadISATable - read the instruction table from the CSV file at path.
 * Each row is  mnemonic,opcode,operands  (any further columns are ignored):
 *   mnemonic  a name (a letter or '_', then letters, digits or '_'), unique in the file
 *   opcode    5 binary digits, or '-' for the whole-word instruction LDW (its operand must be 'k')
 *   operands  the operand fields in assembly order, from d, a, b, i (each at most once; 'i' cannot be
 *             combined with a or b), or empty for no operands
 * Blank lines, lines starting with '#' and a header row starting with "mnemonic" are skipped.
 * A bad row is reported on stderr as "path:line: message" and skipped, so one run reports every
 * problem; reading stops with an error if the table fills up (MAX_ISA instructions).
 * Returns true only if the file opened and every row was valid.
 */
bool LoadISATable(const char *path)
{
    FILE *file = fopen(path, "r");
    char rowBuffer[LINE_LEN]; int lineNumber = 0; bool allRowsValid = true;
    if (!file) { fprintf(stderr, "cannot open ISA table '%s' (use -i <file>)\n", path); return false; }
    /* one CSV row per pass */
    while (fgets(rowBuffer, sizeof rowBuffer, file)) {
        char *text = TrimWhitespace(rowBuffer), *fields[3] = {NULL, NULL, NULL}, *cursor, *comma, *mnemonic, *opcodeText, *operandText;
        int fieldCount = 0; size_t codeIndex; ISAEntry entry;
        lineNumber++;
        /* skip blank lines and # comments */
        if (!*text || *text == '#') continue;
        cursor = text;
        /* split the row at commas into at most 3 fields (mnemonic, opcode, operands); a 4th column is ignored */
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
            /* each operand code must be one of d, a, b, i and appear only once */
            for (codeIndex = 0; operandText[codeIndex]; codeIndex++) {
                if (!strchr("dabi", operandText[codeIndex]) || memchr(operandText, operandText[codeIndex], codeIndex)) { fprintf(stderr, "%s:%d: bad operand code '%s'\n", path, lineNumber, operandText); allRowsValid = false; break; }
            }
            /* the loop above stopped on a bad code: that row is already reported, so skip it */
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
