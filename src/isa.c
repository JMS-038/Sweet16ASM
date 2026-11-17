/*
 * isa.c - loads and looks up the instruction set table for the Sweet16 assembler.
 * Moved out of main.c unchanged (only `static` removed from the two functions;
 * the table itself stays private to this file because nothing else reads it).
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "isa.h"
#include "util.h"

static IsaEntry isa[MAX_ISA];   static int n_isa;

const IsaEntry *find_isa(const char *m)
{
    int i;
    for (i = 0; i < n_isa; i++) if (EqualsIgnoreCase(isa[i].mnem, m)) return &isa[i];
    return NULL;
}

bool load_isa(const char *path)
{
    FILE *f = fopen(path, "r");
    char buf[LINE_LEN]; int ln = 0; bool ok = true;
    if (!f) { fprintf(stderr, "cannot open ISA table '%s' (use -i <file>)\n", path); return false; }
    while (fgets(buf, sizeof buf, f)) {
        char *s = TrimWhitespace(buf), *fld[3] = {NULL, NULL, NULL}, *p, *c, *m, *o, *d;
        int nf = 0; size_t k; IsaEntry e;
        ln++;
        if (!*s || *s == '#') continue;
        p = s;
        while (nf < 3) {
            fld[nf++] = p;
            c = strchr(p, ',');
            if (!c) break;
            *c = '\0'; p = c + 1;
        }
        m = TrimWhitespace(fld[0]);
        if (EqualsIgnoreCase(m, "mnemonic")) continue; /* header row */
        if (nf < 2) { fprintf(stderr, "%s:%d: need mnemonic,opcode,operands\n", path, ln); ok = false; continue; }
        o = TrimWhitespace(fld[1]); d = nf > 2 ? TrimWhitespace(fld[2]) : (char *)"";
        memset(&e, 0, sizeof e);
        if (!IsIdentifier(m) || strlen(m) >= sizeof e.mnem) { fprintf(stderr, "%s:%d: bad mnemonic '%s'\n", path, ln, m); ok = false; continue; }
        strcpy(e.mnem, m);
        if (strlen(d) >= sizeof e.ops) { fprintf(stderr, "%s:%d: too many operands\n", path, ln); ok = false; continue; }
        if (strcmp(o, "-") == 0) { /* whole-word instruction (LDW): no opcode field */
            if (strcmp(d, "k") != 0) { fprintf(stderr, "%s:%d: opcode '-' needs operand code 'k'\n", path, ln); ok = false; continue; }
            e.whole = true;
        } else {
            if (strlen(o) != 5 || strspn(o, "01") != 5) { fprintf(stderr, "%s:%d: opcode must be 5 binary digits (or '-')\n", path, ln); ok = false; continue; }
            e.opcode = (unsigned)strtoul(o, NULL, 2);
            for (k = 0; d[k]; k++) {
                if (!strchr("dabi", d[k]) || memchr(d, d[k], k)) { fprintf(stderr, "%s:%d: bad operand code '%s'\n", path, ln, d); ok = false; break; }
            }
            if (d[k]) continue;
            if (strchr(d, 'i') && (strchr(d, 'a') || strchr(d, 'b'))) { fprintf(stderr, "%s:%d: 'i' overlaps the a/b fields\n", path, ln); ok = false; continue; }
        }
        strcpy(e.ops, d); e.nops = (int)strlen(d);
        if (find_isa(e.mnem)) { fprintf(stderr, "%s:%d: duplicate mnemonic %s\n", path, ln, e.mnem); ok = false; continue; }
        if (n_isa == MAX_ISA) { fprintf(stderr, "%s: too many instructions\n", path); ok = false; break; }
        isa[n_isa++] = e;
    }
    fclose(f);
    return ok;
}
