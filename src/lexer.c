/*
 * lexer.c - line splitting and operand parsing for the Sweet16 assembler.
 * Moved out of main.c unchanged (only `static` removed).
 */
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "lexer.h"
#include "util.h"

bool ParseRegister(const char *text, int *registerNumber)
{
    if ((text[0] == 'r' || text[0] == 'R') && text[1] >= '0' && text[1] <= '7' && text[2] == '\0') {
        *registerNumber = text[1] - '0'; return true;
    }
    return false;
}

bool ParseNumber(const char *text, long *valueOut)
{
    const char *digits = text; bool isNegative = false; int base = 10; char *end; long value;
    if (*digits == '-') { isNegative = true; digits++; } else if (*digits == '+') digits++;
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) { base = 16; digits += 2; }
    else if (digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B')) { base = 2; digits += 2; }
    if (!isalnum((unsigned char)*digits)) return false;
    errno = 0;
    value = strtol(digits, &end, base);
    if (*end != '\0' || errno) return false;
    *valueOut = isNegative ? -value : value;
    return true;
}

/* ---------- line parsing ---------- */
void ParseLine(char *buffer, int lineNumber, ParsedLine *parsed, bool reportErrors)
{
    char *text, *colon, *separator, *operand; size_t mnemonicLength;
    memset(parsed, 0, sizeof *parsed); parsed->isValid = true;
#define FAIL(...) do { if (reportErrors) ReportError(lineNumber, __VA_ARGS__); parsed->isValid = false; return; } while (0)
    if ((separator = strchr(buffer, ';')) != NULL) *separator = '\0';
    text = TrimWhitespace(buffer);
    if ((colon = strchr(text, ':')) != NULL) {
        char *labelText; *colon = '\0'; labelText = TrimWhitespace(text);
        if (!IsIdentifier(labelText) || strlen(labelText) >= NAME_LEN) FAIL("invalid label '%s'", labelText);
        strcpy(parsed->label, labelText); parsed->hasLabel = true;
        text = TrimWhitespace(colon + 1);
        if (strchr(text, ':')) FAIL("only one label per line");
    }
    if (!*text) return;
    mnemonicLength = strcspn(text, " \t,");
    if (mnemonicLength >= NAME_LEN) FAIL("mnemonic too long");
    memcpy(parsed->mnemonic, text, mnemonicLength); parsed->mnemonic[mnemonicLength] = '\0'; parsed->hasMnemonic = true;
    text = TrimWhitespace(text + mnemonicLength);
    if (*text == ',') text = TrimWhitespace(text + 1);
    if (!*text) return;
    for (;;) {
        separator = strchr(text, ','); if (separator) *separator = '\0';
        operand = TrimWhitespace(text);
        if (!*operand) FAIL("empty operand");
        if (parsed->operandCount == MAX_OPS) FAIL("too many operands");
        parsed->operands[parsed->operandCount++] = operand;
        if (!separator) break;
        text = separator + 1;
    }
#undef FAIL
}
