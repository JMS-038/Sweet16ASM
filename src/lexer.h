/*
 * lexer.h - splits one source line into label, mnemonic and operands, and parses
 * register and number operands.
 */
#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

#include "constants.h"

/* The result of parsing one source line.
 *   isValid       false when the line is malformed
 *   hasLabel      the line starts with "label:"; label holds the name
 *   hasMnemonic   the line has an instruction, pseudo-op or directive; mnemonic holds its name
 *   operands      the operand texts, which point into the line buffer; operandCount says how many */
typedef struct {
    bool  isValid, hasLabel, hasMnemonic;
    char  label[NAME_LEN];
    char  mnemonic[NAME_LEN];
    char *operands[MAX_OPS];
    int   operandCount;
} ParsedLine;

/* Parses "rN" (N = 0..7, either case) into *registerNumber. Returns false for anything else. */
bool ParseRegister(const char *text, int *registerNumber);

/* Parses a decimal, 0x hex or 0b binary number with an optional sign into *valueOut.
 * Returns false when text is not a number or does not fit in a long. */
bool ParseNumber(const char *text, long *valueOut);

/* Splits buffer (modified in place) into parsed->label, parsed->mnemonic and parsed->operands.
 * parsed->isValid is false when the line is malformed; the error is reported only when reportErrors is true. */
void ParseLine(char *buffer, int lineNumber, ParsedLine *parsed, bool reportErrors);

#endif /* LEXER_H */
