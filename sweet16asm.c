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
 *   Pseudo-ops: NOP | MOV rd,ra | CLR rd | JMP rt,label | JZL ra,rt,label
 */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_WORDS  65536
#define MAX_ISA    64
#define MAX_LABELS 4096
#define MAX_OPS    8
#define NAME_LEN   64
#define LINE_LEN   512
#define PREFIX_BITS 0x4000u /* bits 15:14 = 01 on every instruction */
#define IMM_MIN (-32)
#define IMM_MAX 31

typedef struct {
    char     mnem[NAME_LEN];
    unsigned opcode;
    char     ops[8]; /* letters from {d,a,b,i}, in assembly order */
    int      nops;
} IsaEntry;

typedef struct { char name[NAME_LEN]; unsigned addr; int line; } Label;

typedef struct {
    bool  ok, has_label, has_mnem;
    char  label[NAME_LEN];
    char  mnem[NAME_LEN];
    char *ops[MAX_OPS];
    int   nops;
} Parsed;

static IsaEntry isa[MAX_ISA];   static int n_isa;
static Label    labels[MAX_LABELS]; static int n_labels;
static uint16_t mem[MEM_WORDS]; static bool used[MEM_WORDS];
static int      max_addr = -1;
static int      n_errors;
static const char *src_name = "";
static FILE    *listing;

/* ---------- small helpers ---------- */
static void err(int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", src_name, line);
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr);
    n_errors++;
}

static char *trim(char *s)
{
    char *e;
    while (isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static bool ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return false;
    return *a == *b;
}

static bool is_ident(const char *s)
{
    if (!(isalpha((unsigned char)*s) || *s == '_')) return false;
    for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '_')) return false;
    return true;
}

static bool parse_reg(const char *s, int *r)
{
    if ((s[0] == 'r' || s[0] == 'R') && s[1] >= '0' && s[1] <= '7' && s[2] == '\0') {
        *r = s[1] - '0'; return true;
    }
    return false;
}

static bool parse_num(const char *s, long *out)
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

/* ---------- ISA table (CSV) ---------- */
static const IsaEntry *find_isa(const char *m)
{
    int i;
    for (i = 0; i < n_isa; i++) if (ieq(isa[i].mnem, m)) return &isa[i];
    return NULL;
}

static bool load_isa(const char *path)
{
    FILE *f = fopen(path, "r");
    char buf[LINE_LEN]; int ln = 0; bool ok = true;
    if (!f) { fprintf(stderr, "cannot open ISA table '%s' (use -i <file>)\n", path); return false; }
    while (fgets(buf, sizeof buf, f)) {
        char *s = trim(buf), *fld[3] = {NULL, NULL, NULL}, *p, *c, *m, *o, *d;
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
        m = trim(fld[0]);
        if (ieq(m, "mnemonic")) continue; /* header row */
        if (nf < 2) { fprintf(stderr, "%s:%d: need mnemonic,opcode,operands\n", path, ln); ok = false; continue; }
        o = trim(fld[1]); d = nf > 2 ? trim(fld[2]) : (char *)"";
        memset(&e, 0, sizeof e);
        if (!is_ident(m) || strlen(m) >= sizeof e.mnem) { fprintf(stderr, "%s:%d: bad mnemonic '%s'\n", path, ln, m); ok = false; continue; }
        strcpy(e.mnem, m);
        if (strlen(o) != 5 || strspn(o, "01") != 5) { fprintf(stderr, "%s:%d: opcode must be 5 binary digits\n", path, ln); ok = false; continue; }
        e.opcode = (unsigned)strtoul(o, NULL, 2);
        if (strlen(d) >= sizeof e.ops) { fprintf(stderr, "%s:%d: too many operands\n", path, ln); ok = false; continue; }
        for (k = 0; d[k]; k++) {
            if (!strchr("dabi", d[k]) || memchr(d, d[k], k)) { fprintf(stderr, "%s:%d: bad operand code '%s'\n", path, ln, d); ok = false; break; }
        }
        if (d[k]) continue;
        if (strchr(d, 'i') && (strchr(d, 'a') || strchr(d, 'b'))) { fprintf(stderr, "%s:%d: 'i' overlaps the a/b fields\n", path, ln); ok = false; continue; }
        strcpy(e.ops, d); e.nops = (int)strlen(d);
        if (find_isa(e.mnem)) { fprintf(stderr, "%s:%d: duplicate mnemonic %s\n", path, ln, e.mnem); ok = false; continue; }
        if (n_isa == MAX_ISA) { fprintf(stderr, "%s: too many instructions\n", path); ok = false; break; }
        isa[n_isa++] = e;
    }
    fclose(f);
    return ok;
}

/* ---------- line parsing ---------- */
static void parse_line(char *buf, int line, Parsed *p, bool report)
{
    char *s, *colon, *c, *o; size_t n;
    memset(p, 0, sizeof *p); p->ok = true;
#define FAIL(...) do { if (report) err(line, __VA_ARGS__); p->ok = false; return; } while (0)
    if ((c = strchr(buf, ';')) != NULL) *c = '\0';
    s = trim(buf);
    if ((colon = strchr(s, ':')) != NULL) {
        char *lab; *colon = '\0'; lab = trim(s);
        if (!is_ident(lab) || strlen(lab) >= NAME_LEN) FAIL("invalid label '%s'", lab);
        strcpy(p->label, lab); p->has_label = true;
        s = trim(colon + 1);
        if (strchr(s, ':')) FAIL("only one label per line");
    }
    if (!*s) return;
    n = strcspn(s, " \t,");
    if (n >= NAME_LEN) FAIL("mnemonic too long");
    memcpy(p->mnem, s, n); p->mnem[n] = '\0'; p->has_mnem = true;
    s = trim(s + n);
    if (*s == ',') s = trim(s + 1);
    if (!*s) return;
    for (;;) {
        c = strchr(s, ','); if (c) *c = '\0';
        o = trim(s);
        if (!*o) FAIL("empty operand");
        if (p->nops == MAX_OPS) FAIL("too many operands");
        p->ops[p->nops++] = o;
        if (!c) break;
        s = c + 1;
    }
#undef FAIL
}

/* ---------- labels ---------- */
static int find_label(const char *name)
{
    int i;
    for (i = 0; i < n_labels; i++) if (strcmp(labels[i].name, name) == 0) return i;
    return -1;
}

static void define_label(const char *name, unsigned addr, int line)
{
    int i = find_label(name);
    if (i >= 0) { err(line, "label '%s' already defined on line %d", name, labels[i].line); return; }
    if (n_labels == MAX_LABELS) { err(line, "too many labels"); return; }
    strcpy(labels[n_labels].name, name); labels[n_labels].addr = addr; labels[n_labels].line = line;
    n_labels++;
}

/* ---------- statement sizing / .org ---------- */
static bool is_pseudo2(const char *m) { return ieq(m, "JMP") || ieq(m, "JZL"); } /* expand to 2 words */

static int stmt_size(const Parsed *p)
{
    if (!p->has_mnem || ieq(p->mnem, ".org")) return 0;
    return is_pseudo2(p->mnem) ? 2 : 1;
}

static void step_org(const Parsed *p, unsigned *addr, int line, bool report)
{
    long v;
    if (p->nops != 1 || !parse_num(p->ops[0], &v) || v < 0 || v >= MEM_WORDS) {
        if (report) err(line, ".org needs one address in 0..65535");
        return;
    }
    if ((unsigned)v < *addr) { if (report) err(line, ".org cannot move backwards (at 0x%04X)", *addr); return; }
    *addr = (unsigned)v;
}

/* ---------- encoding ---------- */
static bool encode_instr(int line, const char *mn, char *const *ops, int nops, uint16_t *out)
{
    const IsaEntry *e = find_isa(mn);
    unsigned w; int k;
    if (!e) { err(line, "unknown instruction '%s'", mn); return false; }
    if (nops != e->nops) { err(line, "%s takes %d operand(s), got %d", e->mnem, e->nops, nops); return false; }
    w = PREFIX_BITS | (e->opcode << 9);
    for (k = 0; k < nops; k++) {
        char f = e->ops[k];
        if (f == 'i') {
            long v;
            if (!parse_num(ops[k], &v)) { err(line, "'%s' is not a number", ops[k]); return false; }
            if (v < IMM_MIN || v > IMM_MAX) { err(line, "immediate %ld out of range (%d..%d)", v, IMM_MIN, IMM_MAX); return false; }
            w |= (unsigned)(v & 0x3F);
        } else {
            int r;
            if (!parse_reg(ops[k], &r)) { err(line, "'%s' is not a register (r0-r7)", ops[k]); return false; }
            w |= (unsigned)r << (f == 'd' ? 6 : f == 'a' ? 3 : 0);
        }
    }
    *out = (uint16_t)w;
    return true;
}

static void emit(unsigned addr, uint16_t w, int line, const char *src)
{
    if (addr >= MEM_WORDS) { err(line, "program exceeds 64K words"); return; }
    if (used[addr]) { err(line, "address 0x%04X already written", addr); return; }
    mem[addr] = w; used[addr] = true;
    if ((int)addr > max_addr) max_addr = (int)addr;
    if (listing) fprintf(listing, "%04X: %04X  %s\n", addr, w, src ? src : "");
}

/* JMP rt,label  ->  LDI rt,(label-addr_of_J) ; J rt        (J is PC-relative) */
static void do_jump_pseudo(const Parsed *p, unsigned addr, int line, const char *src)
{
    bool cond = ieq(p->mnem, "JZL");
    int want = cond ? 3 : 2, rt;
    const char *rts, *lab; char num[16], *ldi_ops[2], *j_ops[2];
    uint16_t w1, w2; int li; long off;
    if (p->nops != want) { err(line, "%s takes %d operands", p->mnem, want); return; }
    rts = p->ops[want - 2]; lab = p->ops[want - 1];
    if (!parse_reg(rts, &rt) || rt == 0) { err(line, "scratch register '%s' must be r1-r7", rts); return; }
    li = find_label(lab);
    if (li < 0) { err(line, "undefined label '%s'", lab); return; }
    off = (long)labels[li].addr - (long)(addr + 1); /* J sits at addr+1 */
    if (off < IMM_MIN || off > IMM_MAX) { err(line, "label '%s' is %ld words away; LDI reaches %d..%d", lab, off, IMM_MIN, IMM_MAX); return; }
    snprintf(num, sizeof num, "%ld", off);
    ldi_ops[0] = (char *)rts; ldi_ops[1] = num;
    if (!encode_instr(line, "LDI", ldi_ops, 2, &w1)) return;
    if (cond) { j_ops[0] = p->ops[0]; j_ops[1] = (char *)rts; if (!encode_instr(line, "JZ", j_ops, 2, &w2)) return; }
    else      { j_ops[0] = (char *)rts; if (!encode_instr(line, "J", j_ops, 1, &w2)) return; }
    emit(addr, w1, line, src);
    emit(addr + 1, w2, line, NULL);
}

static void do_statement(const Parsed *p, unsigned addr, int line, const char *src)
{
    static char r0s[] = "r0"; uint16_t w; char *t[3];
    if (ieq(p->mnem, ".word")) {
        long v;
        if (p->nops != 1) { err(line, ".word takes one value"); return; }
        if (parse_num(p->ops[0], &v)) {
            if (v < -32768 || v > 65535) { err(line, ".word value out of 16-bit range"); return; }
        } else {
            int li = find_label(p->ops[0]);
            if (li < 0) { err(line, "'%s' is neither a number nor a defined label", p->ops[0]); return; }
            v = labels[li].addr;
        }
        emit(addr, (uint16_t)(v & 0xFFFF), line, src);
    } else if (p->mnem[0] == '.') {
        err(line, "unknown directive '%s'", p->mnem);
    } else if (is_pseudo2(p->mnem)) {
        do_jump_pseudo(p, addr, line, src);
    } else if (ieq(p->mnem, "NOP")) {
        if (p->nops != 0) { err(line, "NOP takes no operands"); return; }
        t[0] = t[1] = t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(addr, w, line, src);
    } else if (ieq(p->mnem, "MOV")) {
        if (p->nops != 2) { err(line, "MOV takes rd, ra"); return; }
        t[0] = p->ops[0]; t[1] = p->ops[1]; t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(addr, w, line, src);
    } else if (ieq(p->mnem, "CLR")) {
        if (p->nops != 1) { err(line, "CLR takes rd"); return; }
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
        if (p.has_mnem && ieq(p.mnem, ".org")) step_org(&p, &addr, i + 1, true);
        addr += (unsigned)stmt_size(&p);
    }
    addr = 0;
    for (i = 0; i < nlines; i++) { /* pass 2: encode */
        strcpy(tmp, lines[i]); parse_line(tmp, i + 1, &p, false);
        if (!p.ok || !p.has_mnem) continue;
        if (ieq(p.mnem, ".org")) { step_org(&p, &addr, i + 1, false); continue; }
        do_statement(&p, addr, i + 1, lines[i]);
        addr += (unsigned)stmt_size(&p);
    }
}

/* ---------- output ---------- */
/* bin: 65536 big-endian 16-bit words (131072 bytes). Matches the earlier program2_romJMS image byte for byte. */
static bool write_bin(const char *path)
{
    FILE *f = fopen(path, "wb"); int i;
    if (!f) return false;
    for (i = 0; i < MEM_WORDS; i++) { fputc(mem[i] >> 8, f); fputc(mem[i] & 0xFF, f); }
    return fclose(f) == 0;
}

/* v3: Logisim "v3.0 hex words addressed" text, as saved by the Sweet16 ROMs (microcode_rom.bin,
 * program_rom_matrix_marshall.bin): header line, then "AAAA: w w w w w w w w" with 4-digit lowercase
 * hex, 8 words per line, contiguous from address 0 up to the last word written. Address width is 4
 * digits for the 64K program ROM (the 256-word microcode ROM uses 2). */
static bool write_v3(const char *path)
{
    FILE *f = fopen(path, "w"); int i;
    if (!f) return false;
    fputs("v3.0 hex words addressed\n", f);
    for (i = 0; i <= max_addr; i++) {
        if (i % 8 == 0) fprintf(f, "%04x:", i);
        fprintf(f, " %04x", mem[i]);
        if (i % 8 == 7 || i == max_addr) fputc('\n', f);
    }
    return fclose(f) == 0;
}

/* ---------- main ---------- */
static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *d = malloc(n); if (d) memcpy(d, s, n); return d; }

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
    src_name = in;
    if (!load_isa(isa_path)) return 1;
    if (!(f = fopen(in, "r"))) { fprintf(stderr, "cannot open '%s'\n", in); return 1; }
    while (fgets(buf, sizeof buf, f)) {
        size_t n = strlen(buf);
        if (n == sizeof buf - 1 && buf[n - 1] != '\n') {
            int ch; err(nlines + 1, "line longer than %d characters", LINE_LEN - 2);
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
    if (n_errors) { fprintf(stderr, "%d error(s); no output written\n", n_errors); return 1; }
    if (!out) {
        char *dot; snprintf(outbuf, sizeof outbuf, "%s", in);
        dot = strrchr(outbuf, '.'); if (dot && !strchr(dot, '/')) *dot = '\0';
        strncat(outbuf, strcmp(fmt, "bin") ? ".hex" : ".bin", sizeof outbuf - strlen(outbuf) - 1);
        out = outbuf;
    }
    ok = strcmp(fmt, "bin") ? write_v3(out) : write_bin(out);
    if (!ok) { fprintf(stderr, "cannot write '%s'\n", out); return 1; }
    printf("%s: %d word(s) -> %s (%s)\n", in, max_addr + 1, out, fmt);
    return 0;
}
