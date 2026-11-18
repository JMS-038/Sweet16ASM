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
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "isa.h"
#include "lexer.h"
#include "output.h"
#include "symbols.h"
#include "util.h"

static FILE    *listing;

/* ---------- statement sizing / .org ---------- */
static bool is_pseudo2(const char *m) /* pseudo-ops that expand to 2 words */
{
    return EqualsIgnoreCase(m, "JMP") || EqualsIgnoreCase(m, "JZL") || EqualsIgnoreCase(m, "LJMP") || EqualsIgnoreCase(m, "LJZ");
}

static int stmt_size(const Parsed *p)
{
    if (!p->has_mnem || EqualsIgnoreCase(p->mnem, ".org")) return 0;
    return is_pseudo2(p->mnem) ? 2 : 1;
}

static void step_org(const Parsed *p, unsigned *addr, int line, bool report)
{
    long v;
    if (p->nops != 1 || !parse_num(p->ops[0], &v) || v < 0 || v >= MEM_WORDS) {
        if (report) ReportError(line, ".org needs one address in 0..65535");
        return;
    }
    if ((unsigned)v < *addr) { if (report) ReportError(line, ".org cannot move backwards (at 0x%04X)", *addr); return; }
    *addr = (unsigned)v;
}

/* ---------- encoding ---------- */
static bool encode_instr(int line, const char *mn, char *const *ops, int nops, uint16_t *out)
{
    const IsaEntry *e = find_isa(mn);
    unsigned w; int k;
    if (!e) { ReportError(line, "unknown instruction '%s'", mn); return false; }
    if (nops != e->nops) { ReportError(line, "%s takes %d operand(s), got %d", e->mnem, e->nops, nops); return false; }
    if (e->whole) { /* LDW k: the instruction word is k itself (00.. for k>=0, 11.. for k<0) */
        long v;
        if (!parse_num(ops[0], &v)) {
            int li = find_label(ops[0]);
            if (li < 0) { ReportError(line, "'%s' is neither a number nor a defined label", ops[0]); return false; }
            v = (long)labels[li].addr;
            if (v > K_MAX) { ReportError(line, "label '%s' (address 0x%04lX) does not fit %s (max %d)", ops[0], v, e->mnem, K_MAX); return false; }
        }
        if (v < K_MIN || v > K_MAX) { ReportError(line, "%s value %ld out of range (%d..%d)", e->mnem, v, K_MIN, K_MAX); return false; }
        *out = (uint16_t)(v & 0xFFFF);
        return true;
    }
    w = PREFIX_BITS | (e->opcode << 9);
    for (k = 0; k < nops; k++) {
        char f = e->ops[k];
        if (f == 'i') {
            long v;
            if (!parse_num(ops[k], &v)) { ReportError(line, "'%s' is not a number", ops[k]); return false; }
            if (v < IMM_MIN || v > IMM_MAX) { ReportError(line, "immediate %ld out of range (%d..%d)", v, IMM_MIN, IMM_MAX); return false; }
            w |= (unsigned)(v & 0x3F);
        } else {
            int r;
            if (!parse_reg(ops[k], &r)) { ReportError(line, "'%s' is not a register (r0-r7)", ops[k]); return false; }
            w |= (unsigned)r << (f == 'd' ? 6 : f == 'a' ? 3 : 0);
        }
    }
    *out = (uint16_t)w;
    return true;
}

static void emit(unsigned addr, uint16_t w, int line, const char *src)
{
    if (addr >= MEM_WORDS) { ReportError(line, "program exceeds 64K words"); return; }
    if (image.used[addr]) { ReportError(line, "address 0x%04X already written", addr); return; }
    image.mem[addr] = w; image.used[addr] = true;
    if ((int)addr > image.max_addr) image.max_addr = (int)addr;
    if (listing) fprintf(listing, "%04X: %04X  %s\n", addr, w, src ? src : "");
}

/* JMP rt,label     / JZL ra,rt,label : LDI rt,off ; J rt / JZ ra,rt   reach -32..31
 * LJMP label        / LJZ ra,label    : LDW off    ; J r7 / JZ ra,r7   reach -16384..16383, overwrites r7
 * off = label - address of the jump instruction (the jump is the second word, at addr+1). */
static void do_jump_pseudo(const Parsed *p, unsigned addr, int line, const char *src)
{
    bool lng  = EqualsIgnoreCase(p->mnem, "LJMP") || EqualsIgnoreCase(p->mnem, "LJZ");
    bool cond = EqualsIgnoreCase(p->mnem, "JZL")  || EqualsIgnoreCase(p->mnem, "LJZ");
    int want = lng ? (cond ? 2 : 1) : (cond ? 3 : 2), li, rt;
    static char r7s[] = "r7", *rts;
    const char *lab; char num[16], *first_ops[2], *j_ops[2];
    long lo = lng ? K_MIN : IMM_MIN, hi = lng ? K_MAX : IMM_MAX, off;
    uint16_t w1, w2;
    if (p->nops != want) { ReportError(line, "%s takes %d operand(s)", p->mnem, want); return; }
    rts = r7s;
    if (!lng) {
        rts = p->ops[want - 2];
        if (!parse_reg(rts, &rt) || rt == 0) { ReportError(line, "scratch register '%s' must be r1-r7", rts); return; }
    }
    lab = p->ops[want - 1];
    li = find_label(lab);
    if (li < 0) { ReportError(line, "undefined label '%s'", lab); return; }
    off = (long)labels[li].addr - (long)(addr + 1); /* the jump sits at addr+1 */
    if (off < lo || off > hi) {
        ReportError(line, "label '%s' is %ld words away; %s reaches %ld..%ld%s", lab, off, p->mnem, lo, hi,
            lng ? "" : " (LJMP/LJZ reach further but overwrite r7)");
        return;
    }
    snprintf(num, sizeof num, "%ld", off);
    if (lng) { first_ops[0] = num; if (!encode_instr(line, "LDW", first_ops, 1, &w1)) return; }
    else     { first_ops[0] = rts; first_ops[1] = num; if (!encode_instr(line, "LDI", first_ops, 2, &w1)) return; }
    if (cond) { j_ops[0] = p->ops[0]; j_ops[1] = rts; if (!encode_instr(line, "JZ", j_ops, 2, &w2)) return; }
    else      { j_ops[0] = rts; if (!encode_instr(line, "J", j_ops, 1, &w2)) return; }
    emit(addr, w1, line, src);
    emit(addr + 1, w2, line, NULL);
}

static void do_statement(const Parsed *p, unsigned addr, int line, const char *src)
{
    static char r0s[] = "r0"; uint16_t w; char *t[3];
    if (EqualsIgnoreCase(p->mnem, ".word")) {
        long v;
        if (p->nops != 1) { ReportError(line, ".word takes one value"); return; }
        if (parse_num(p->ops[0], &v)) {
            if (v < -32768 || v > 65535) { ReportError(line, ".word value out of 16-bit range"); return; }
        } else {
            int li = find_label(p->ops[0]);
            if (li < 0) { ReportError(line, "'%s' is neither a number nor a defined label", p->ops[0]); return; }
            v = labels[li].addr;
        }
        emit(addr, (uint16_t)(v & 0xFFFF), line, src);
    } else if (p->mnem[0] == '.') {
        ReportError(line, "unknown directive '%s'", p->mnem);
    } else if (is_pseudo2(p->mnem)) {
        do_jump_pseudo(p, addr, line, src);
    } else if (EqualsIgnoreCase(p->mnem, "NOP")) {
        if (p->nops != 0) { ReportError(line, "NOP takes no operands"); return; }
        t[0] = t[1] = t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(addr, w, line, src);
    } else if (EqualsIgnoreCase(p->mnem, "MOV")) {
        if (p->nops != 2) { ReportError(line, "MOV takes rd, ra"); return; }
        t[0] = p->ops[0]; t[1] = p->ops[1]; t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(addr, w, line, src);
    } else if (EqualsIgnoreCase(p->mnem, "CLR")) {
        if (p->nops != 1) { ReportError(line, "CLR takes rd"); return; }
        t[0] = p->ops[0]; t[1] = t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(addr, w, line, src);
    } else if (encode_instr(line, p->mnem, p->ops, p->nops, &w)) {
        emit(addr, w, line, src);
    }
}

/* ---------- passes ---------- */
static void run_passes(char **lines, int nlines)
{
    unsigned addr = 0; int i; char tmp[LINE_LEN]; Parsed p;
    for (i = 0; i < nlines; i++) { /* pass 1: addresses + labels */
        strcpy(tmp, lines[i]); parse_line(tmp, i + 1, &p, true);
        if (!p.ok) continue;
        if (p.has_label) define_label(p.label, addr, i + 1);
        if (p.has_mnem && EqualsIgnoreCase(p.mnem, ".org")) step_org(&p, &addr, i + 1, true);
        addr += (unsigned)stmt_size(&p);
    }
    addr = 0;
    for (i = 0; i < nlines; i++) { /* pass 2: encode */
        strcpy(tmp, lines[i]); parse_line(tmp, i + 1, &p, false);
        if (!p.ok || !p.has_mnem) continue;
        if (EqualsIgnoreCase(p.mnem, ".org")) { step_org(&p, &addr, i + 1, false); continue; }
        do_statement(&p, addr, i + 1, lines[i]);
        addr += (unsigned)stmt_size(&p);
    }
}

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
    if (!load_isa(isa_path)) return 1;
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
        lines[nlines++] = dupstr(buf);
    }
    fclose(f);
    if (lst && !(listing = fopen(lst, "w"))) { fprintf(stderr, "cannot write '%s'\n", lst); return 1; }
    run_passes(lines, nlines);
    if (listing) fclose(listing);
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
