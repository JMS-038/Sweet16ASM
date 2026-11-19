/*
 * sweet16asm.c - two-pass assembler for the EE340 Sweet16 MCU.
 * Standard C99 library only.
 *
 * Instruction word: 01-ooooo-ddd-aaa-bbb   (LDI: 01-01001-ddd-iiiiii)
 * The opcode and which operands fill which fields come from isa.csv.
 *
 * Output: Logisim 'v3.0 hex words addressed' text (default) or a raw 64K big-endian image.
 *
 * Source syntax:
 *   ; comment (everything after ';' is ignored)
 *   label:  MNEMONIC op, op, op       (label optional, own line or same line)
 *   Registers r0-r7. Numbers: decimal, 0x hex, 0b binary, optional sign.
 *   Directives: .org <addr>   (forward only)     .word <value|label>
 *   LDW k       whole-word immediate into R7 (k = -16384..16383, or a label address)
 *   Pseudo-ops: NOP | MOV rd,ra | CLR rd | JMP rt,label | JZL ra,rt,label
 *               LJMP label | LJZ ra,label   (long jumps through R7, built on LDW)
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assembler.h"
#include "constants.h"
#include "isa.h"
#include "output.h"
#include "util.h"

/* ---------- main ---------- */
static void usage(void)
{
    fputs("usage: sweet16asm [-i isa.csv] [-o out] [-f v3|bin] [-l listing] input.asm\n"
          "  -i  ISA table (default ./isa.csv)\n"
          "  -f  v3  = Logisim 'v3.0 hex words addressed' text (default)\n"
          "      bin = raw 64K big-endian 16-bit words (131072 bytes)\n", stderr);
}

int main(int argc, char **argv)
{
    const char *isa_path = "isa.csv", *out = NULL, *fmt = "v3", *lst = NULL, *in = NULL;
    char **lines = NULL, buf[LINE_LEN], outbuf[512]; int nlines = 0, cap = 0, a; FILE *f; bool ok;
    AssemblerState state = { NULL };
    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "-i") && a + 1 < argc) isa_path = argv[++a];
        else if (!strcmp(argv[a], "-o") && a + 1 < argc) out = argv[++a];
        else if (!strcmp(argv[a], "-f") && a + 1 < argc) fmt = argv[++a];
        else if (!strcmp(argv[a], "-l") && a + 1 < argc) lst = argv[++a];
        else if (argv[a][0] == '-' || in) { usage(); return 2; }
        else in = argv[a];
    }
    if (!in || (strcmp(fmt, "bin") && strcmp(fmt, "v3"))) { usage(); return 2; }
    sourceName = in;
    if (!LoadISATable(isa_path)) return 1;
    if (!(f = fopen(in, "r"))) { fprintf(stderr, "cannot open '%s'\n", in); return 1; }
    while (fgets(buf, sizeof buf, f)) {
        size_t n = strlen(buf);
        if (n == sizeof buf - 1 && buf[n - 1] != '\n') {
            int ch; ReportError(nlines + 1, "line longer than %d characters", LINE_LEN - 2);
            while ((ch = fgetc(f)) != EOF && ch != '\n') { }
            buf[0] = '\0';
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        if (nlines == cap) { cap = cap ? cap * 2 : 128; lines = realloc(lines, (size_t)cap * sizeof *lines); if (!lines) return 1; }
        lines[nlines++] = DuplicateString(buf);
    }
    fclose(f);
    if (lst && !(state.listing = fopen(lst, "w"))) { fprintf(stderr, "cannot write '%s'\n", lst); return 1; }
    run_passes(lines, nlines, &state);
    if (state.listing) fclose(state.listing);
    if (errorCount) { fprintf(stderr, "%d error(s); no output written\n", errorCount); return 1; }
    if (!out) {
        char *dot; snprintf(outbuf, sizeof outbuf, "%s", in);
        dot = strrchr(outbuf, '.'); if (dot && !strchr(dot, '/') && !strchr(dot, '\\')) *dot = '\0';
        strncat(outbuf, strcmp(fmt, "bin") ? ".hex" : ".bin", sizeof outbuf - strlen(outbuf) - 1);
        out = outbuf;
    }
    ok = strcmp(fmt, "bin") ? write_v3(out) : write_bin(out);
    if (!ok) { fprintf(stderr, "cannot write '%s'\n", out); return 1; }
    printf("%s: %d word(s) -> %s (%s)\n", in, image.max_addr + 1, out, fmt);
    return 0;
}
