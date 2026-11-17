/*
 * util.c - shared helpers for the Sweet16 assembler: error reporting and
 * string utilities. Moved out of main.c unchanged (only `static` removed).
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

int errorCount;
const char *sourceName = "";

void ReportError(int lineNumber, const char *format, ...)
{
    va_list arguments;
    fprintf(stderr, "%s:%d: error: ", sourceName, lineNumber);
    va_start(arguments, format); vfprintf(stderr, format, arguments); va_end(arguments);
    fputc('\n', stderr);
    errorCount++;
}

char *TrimWhitespace(char *text)
{
    char *end;
    while (isspace((unsigned char)*text)) text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
    return text;
}

bool EqualsIgnoreCase(const char *first, const char *second)
{
    for (; *first && *second; first++, second++)
        if (toupper((unsigned char)*first) != toupper((unsigned char)*second)) return false;
    return *first == *second;
}

bool IsIdentifier(const char *text)
{
    if (!(isalpha((unsigned char)*text) || *text == '_')) return false;
    for (; *text; text++) if (!(isalnum((unsigned char)*text) || *text == '_')) return false;
    return true;
}

char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *d = malloc(n); if (d) memcpy(d, s, n); return d; }
