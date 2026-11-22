/*
 * lexer.c - turns one line of source text into its parts. ParseLine splits a line into label,
 * mnemonic and operands; ParseRegister and ParseNumber parse single operands for the encoder.
 */
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "lexer.h"
#include "util.h"

/*
 * ParseRegister - parse a register name, "r0" to "r7" (either letter case).
 * On success stores the register number in *registerNumber and returns true. Anything else
 * ("r8", "r10", "r", a number) returns false and leaves *registerNumber unchanged.
 */
bool ParseRegister(const char *text, int *registerNumber)
{
    if ((text[0] == 'r' || text[0] == 'R') && text[1] >= '0' && text[1] <= '7' && text[2] == '\0') {
        *registerNumber = text[1] - '0'; return true;
    }
    return false;
}

/*
 * ParseNumber - parse an integer operand: decimal, 0x hex or 0b binary, with an optional + or - sign.
 * On success stores the value in *valueOut and returns true. Returns false when text is not a
 * number, has characters left over, or does not fit in a long. Callers cannot tell those cases apart.
 */
bool ParseNumber(const char *text, long *valueOut)
{
    const char *digits = text; bool isNegative = false; int base = 10; char *end; long value;
    /* optional sign; it is applied after the digits are parsed, so "-0x10" is -16 */
    if (*digits == '-') { isNegative = true; digits++; } else if (*digits == '+') digits++;
    /* optional 0x (hex) or 0b (binary) prefix; otherwise the number is decimal */
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) { base = 16; digits += 2; }
    else if (digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B')) { base = 2; digits += 2; }
    /* reject an empty digit string ("-", "0x"), which strtol would read as 0 */
    if (!isalnum((unsigned char)*digits)) return false;
    /* strtol reports a value too large for a long only through errno, so clear it first and check it after */
    errno = 0;
    value = strtol(digits, &end, base);
    /* characters left over (such as "0x1g" or "1_0"), or overflow: not a valid number */
    if (*end != '\0' || errno) return false;
    *valueOut = isNegative ? -value : value;
    return true;
}

/* ---------- line parsing ---------- */
/*
 * ParseLine - split one source line into its parts:   [label:] [mnemonic [operand, operand, ...]] [; comment]
 * buffer: the text of the line. It is MODIFIED: the comment, the label's colon and the commas between
 *   operands are overwritten with '\0'. parsed->operands point into buffer, so keep buffer alive while
 *   parsed is used (label and mnemonic are copied into parsed).
 * lineNumber: the 1-based line number, used in error messages.
 * parsed: receives the result. isValid is false when the line is malformed. A blank or comment-only
 *   line is valid, with no label and no mnemonic.
 * reportErrors: when true a malformed line is reported with ReportError. The passes parse every line
 *   twice and pass true only the first time, so each error appears once.
 * Malformed lines: an invalid label, a second label, a mnemonic that is too long, an empty operand
 *   (including one after a trailing comma), or more than MAX_OPS operands.
 */
void ParseLine(char *buffer, int lineNumber, ParsedLine *parsed, bool reportErrors)
{
    char *text, *colon, *separator, *operand; size_t mnemonicLength;
    memset(parsed, 0, sizeof *parsed); parsed->isValid = true;
    /* FAIL(message, ...): report the problem (only if reportErrors), mark the line invalid and return */
#define FAIL(...) do { if (reportErrors) ReportError(lineNumber, __VA_ARGS__); parsed->isValid = false; return; } while (0)
    /* drop the comment */
    if ((separator = strchr(buffer, ';')) != NULL) *separator = '\0';
    text = TrimWhitespace(buffer);
    /* optional "label:" at the start of the line */
    if ((colon = strchr(text, ':')) != NULL) {
        char *labelText; *colon = '\0'; labelText = TrimWhitespace(text);
        if (!IsIdentifier(labelText) || strlen(labelText) >= NAME_LEN) FAIL("invalid label '%s'", labelText);
        strcpy(parsed->label, labelText); parsed->hasLabel = true;
        text = TrimWhitespace(colon + 1);
        if (strchr(text, ':')) FAIL("only one label per line");
    }
    /* nothing but a label (or a blank line): valid, with no mnemonic */
    if (!*text) return;
    /* the mnemonic is everything up to the first space, tab or comma */
    mnemonicLength = strcspn(text, " \t,");
    if (mnemonicLength >= NAME_LEN) FAIL("mnemonic too long");
    memcpy(parsed->mnemonic, text, mnemonicLength); parsed->mnemonic[mnemonicLength] = '\0'; parsed->hasMnemonic = true;
    text = TrimWhitespace(text + mnemonicLength);
    /* a comma right after the mnemonic is tolerated ("ADD, r1, r2, r3") */
    if (*text == ',') text = TrimWhitespace(text + 1);
    /* a mnemonic with no operands */
    if (!*text) return;
    /* split the operand list at the commas, one operand per pass */
    for (;;) {
        separator = strchr(text, ','); if (separator) *separator = '\0';
        operand = TrimWhitespace(text);
        /* empty between two commas, or after the last comma */
        if (!*operand) FAIL("empty operand");
        if (parsed->operandCount == MAX_OPS) FAIL("too many operands");
        parsed->operands[parsed->operandCount++] = operand;
        if (!separator) break;
        text = separator + 1;
    }
#undef FAIL
}
