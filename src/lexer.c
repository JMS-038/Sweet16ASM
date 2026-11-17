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

bool parse_reg(const char *s, int *r)
{
    if ((s[0] == 'r' || s[0] == 'R') && s[1] >= '0' && s[1] <= '7' && s[2] == '\0') {
        *r = s[1] - '0'; return true;
    }
    return false;
}

bool parse_num(const char *s, long *out)
{
    const char *p = s; bool neg = false; int base = 10; char *end; long v;
    if (*p == '-') { neg = true; p++; } else if (*p == '+') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { base = 2; p += 2; }
    if (!isalnum((unsigned char)*p)) return false;
    errno = 0;
    v = strtol(p, &end, base);
    if (*end != '\0' || errno) return false;
    *out = neg ? -v : v;
    return true;
}

/* ---------- line parsing ---------- */
void parse_line(char *buf, int line, Parsed *p, bool report)
{
    char *s, *colon, *c, *o; size_t n;
    memset(p, 0, sizeof *p); p->ok = true;
#define FAIL(...) do { if (report) ReportError(line, __VA_ARGS__); p->ok = false; return; } while (0)
    if ((c = strchr(buf, ';')) != NULL) *c = '\0';
    s = TrimWhitespace(buf);
    if ((colon = strchr(s, ':')) != NULL) {
        char *lab; *colon = '\0'; lab = TrimWhitespace(s);
        if (!IsIdentifier(lab) || strlen(lab) >= NAME_LEN) FAIL("invalid label '%s'", lab);
        strcpy(p->label, lab); p->has_label = true;
        s = TrimWhitespace(colon + 1);
        if (strchr(s, ':')) FAIL("only one label per line");
    }
    if (!*s) return;
    n = strcspn(s, " \t,");
    if (n >= NAME_LEN) FAIL("mnemonic too long");
    memcpy(p->mnem, s, n); p->mnem[n] = '\0'; p->has_mnem = true;
    s = TrimWhitespace(s + n);
    if (*s == ',') s = TrimWhitespace(s + 1);
    if (!*s) return;
    for (;;) {
        c = strchr(s, ','); if (c) *c = '\0';
        o = TrimWhitespace(s);
        if (!*o) FAIL("empty operand");
        if (p->nops == MAX_OPS) FAIL("too many operands");
        p->ops[p->nops++] = o;
        if (!c) break;
        s = c + 1;
    }
#undef FAIL
}
