/*
 * lexer.h - splits one source line into label, mnemonic and operands, and parses
 * register and number operands.
 */
#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

#include "constants.h"

/* The result of parsing one source line. The ops pointers point into the line buffer. */
typedef struct {
    bool  ok, has_label, has_mnem;
    char  label[NAME_LEN];
    char  mnem[NAME_LEN];
    char *ops[MAX_OPS];
    int   nops;
} Parsed;

/* Parses "rN" (N = 0..7, either case) into *r. Returns false for anything else. */
bool parse_reg(const char *s, int *r);

/* Parses a decimal, 0x hex or 0b binary number with an optional sign into *out.
 * Returns false when s is not a number or does not fit in a long. */
bool parse_num(const char *s, long *out);

/* Splits buf (modified in place) into p->label, p->mnem and p->ops.
 * p->ok is false when the line is malformed; the error is reported only when report is true. */
void parse_line(char *buf, int line, Parsed *p, bool report);

#endif /* LEXER_H */
