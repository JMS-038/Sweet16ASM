/*
 * util.c - helpers shared by every module: error reporting (ReportError, errorCount, sourceName)
 * and small string utilities.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/* Number of errors reported so far. main checks it after assembling: any error means no output file. */
int errorCount;

/* File name shown in error messages (main sets it to the input path). */
const char *sourceName = "";

/*
 * ReportError - print "<sourceName>:<lineNumber>: error: <message>" to stderr and count it.
 * lineNumber: the 1-based line of the source file the problem is on.
 * format, ...: the message, written like printf arguments.
 * Side effects: writes one line to stderr and adds 1 to errorCount. It does not stop the program,
 * so a single run can report many errors.
 */
void ReportError(int lineNumber, const char *format, ...)
{
    va_list arguments;
    fprintf(stderr, "%s:%d: error: ", sourceName, lineNumber);
    va_start(arguments, format); vfprintf(stderr, format, arguments); va_end(arguments);
    fputc('\n', stderr);
    errorCount++;
}

/*
 * TrimWhitespace - remove white space from both ends of text, in place.
 * Returns a pointer to the first character that is not white space. That pointer is inside the
 * same buffer (not a copy) and may be later than text; trailing white space is cut off by writing
 * a '\0'. Nothing is allocated.
 */
char *TrimWhitespace(char *text)
{
    char *end;
    /* skip the leading white space */
    while (isspace((unsigned char)*text)) text++;
    end = text + strlen(text);
    /* cut the trailing white space by moving the end of the string back */
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
    return text;
}

/*
 * EqualsIgnoreCase - compare two strings without regard to letter case.
 * Returns true when both strings have the same length and the same letters (case ignored).
 */
bool EqualsIgnoreCase(const char *first, const char *second)
{
    /* walk both strings together; stop at the first difference or when either string ends */
    for (; *first && *second; first++, second++)
        if (toupper((unsigned char)*first) != toupper((unsigned char)*second)) return false;
    /* equal only if both strings ended at the same time */
    return *first == *second;
}

/*
 * IsIdentifier - check that text is a valid label or mnemonic name.
 * A valid name starts with a letter or '_' and continues with letters, digits or '_'.
 * Returns false for an empty string.
 */
bool IsIdentifier(const char *text)
{
    if (!(isalpha((unsigned char)*text) || *text == '_')) return false;
    /* every character must be a letter, digit or '_' */
    for (; *text; text++) if (!(isalnum((unsigned char)*text) || *text == '_')) return false;
    return true;
}

/*
 * DuplicateString - make a copy of text on the heap.
 * Returns the copy, or NULL when memory runs out. The caller owns the copy. (main keeps its copies
 * of the source lines for the whole run and lets the program exit release them.)
 */
char *DuplicateString(const char *text) { size_t size = strlen(text) + 1; char *copy = malloc(size); if (copy) memcpy(copy, text, size); return copy; }
