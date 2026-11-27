/*
 * util.c - helpers shared by every module: error reporting (ReportError, errorCount, sourceName),
 * small string utilities and the same-file check (IsSameFile).
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> /* stat(): same-file check (POSIX; MinGW has it too) */

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

#ifndef _WIN32
/*
 * TidyPath - copy path into buffer with "." segments and repeated slashes removed, so "./o.hex",
 * ".//o.hex" and "o.hex" come out the same. Used for a file that does not exist yet, where stat()
 * cannot tell. ".." is not resolved.
 * path: the path to copy.
 * buffer, capacity: where the copy goes and its size; a path too long for it is cut short, never overrun.
 */
static void TidyPath(const char *path, char *buffer, size_t capacity)
{
    const char *start = path; size_t length = 0;
    /* one character per pass */
    while (*path && length + 1 < capacity) {
        /* the first of two slashes in a row is dropped */
        if (*path == '/' && path[1] == '/') { path++; continue; }
        /* a "./" at the start or right after a slash is dropped, with any slashes that follow it */
        if (*path == '.' && path[1] == '/' && (path == start || path[-1] == '/')) {
            path += 2;
            while (*path == '/') path++;
            continue;
        }
        buffer[length++] = *path++;
    }
    buffer[length] = '\0';
}
#endif

/*
 * IsSameFile - tell whether two paths name the same file, so main can refuse to overwrite an input.
 * first, second: the two paths. Neither has to exist.
 * POSIX: the same device and inode (this sees through symlinks and hard links); if either file does
 * not exist yet, the paths are compared after TidyPath.
 * Windows: MinGW stat() gives every file inode 0, so the full paths are compared instead, ignoring
 * letter case and slash direction. Hard links are not detected there.
 * Returns true when they are the same file.
 */
bool IsSameFile(const char *first, const char *second)
{
    /* identical text is always the same file */
    if (!strcmp(first, second)) return true;
#ifdef _WIN32
    {
        char fullFirst[512], fullSecond[512];
        return _fullpath(fullFirst, first, sizeof fullFirst) && _fullpath(fullSecond, second, sizeof fullSecond)
            && !_stricmp(fullFirst, fullSecond);
    }
#else
    {
        struct stat firstInfo, secondInfo; char tidyFirst[512], tidySecond[512];
        /* both files exist: compare which file they are, not how the path is written */
        if (stat(first, &firstInfo) == 0 && stat(second, &secondInfo) == 0)
            return firstInfo.st_dev == secondInfo.st_dev && firstInfo.st_ino == secondInfo.st_ino;
        TidyPath(first, tidyFirst, sizeof tidyFirst); TidyPath(second, tidySecond, sizeof tidySecond);
        return !strcmp(tidyFirst, tidySecond);
    }
#endif
}
