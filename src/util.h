/*
 * util.h - shared helpers for the Sweet16 assembler: error reporting and
 * string utilities.
 */
#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

/* Number of errors reported so far, and the file name shown in error messages.
 * Defined in util.c; the assembler sets sourceName and reads errorCount. */
extern int errorCount;
extern const char *sourceName;

/* Prints "<sourceName>:<lineNumber>: error: <message>" to stderr and counts it. */
void ReportError(int lineNumber, const char *format, ...);

/* Strips leading and trailing white space in place; returns the first kept character. */
char *TrimWhitespace(char *text);

/* True when the two strings are equal, ignoring letter case. */
bool EqualsIgnoreCase(const char *first, const char *second);

/* True when text is a valid identifier: a letter or '_', then letters, digits or '_'. */
bool IsIdentifier(const char *text);

/* Returns a heap copy of s, or NULL when allocation fails. The caller frees it. */
char *DuplicateString(const char *text);

#endif /* UTIL_H */
