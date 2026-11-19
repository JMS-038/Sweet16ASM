/*
 * assembler.c - the two assembly passes for the Sweet16 assembler: statement sizing, .org,
 * instruction encoding, pseudo-op expansion and directives.
 * Moved out of main.c with one mechanical change: the listing file that used to be a global is
 * now reached through an AssemblerState passed down (emit, do_jump_pseudo, do_statement and
 * run_passes take a state parameter). run_passes is the only public function.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assembler.h"
#include "constants.h"
#include "isa.h"
#include "lexer.h"
#include "output.h"
#include "symbols.h"
#include "util.h"

/* ---------- statement sizing / .org ---------- */
static bool is_pseudo2(const char *m) /* pseudo-ops that expand to 2 words */
{
    return EqualsIgnoreCase(m, "JMP") || EqualsIgnoreCase(m, "JZL") || EqualsIgnoreCase(m, "LJMP") || EqualsIgnoreCase(m, "LJZ");
}

static int stmt_size(const ParsedLine *p)
{
    if (!p->hasMnemonic || EqualsIgnoreCase(p->mnemonic, ".org")) return 0;
    return is_pseudo2(p->mnemonic) ? 2 : 1;
}

static void step_org(const ParsedLine *p, unsigned *addr, int line, bool report)
{
    long v;
    if (p->operandCount != 1 || !ParseNumber(p->operands[0], &v) || v < 0 || v >= MEM_WORDS) {
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
        if (!ParseNumber(ops[0], &v)) {
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
            if (!ParseNumber(ops[k], &v)) { ReportError(line, "'%s' is not a number", ops[k]); return false; }
            if (v < IMM_MIN || v > IMM_MAX) { ReportError(line, "immediate %ld out of range (%d..%d)", v, IMM_MIN, IMM_MAX); return false; }
            w |= (unsigned)(v & 0x3F);
        } else {
            int r;
            if (!ParseRegister(ops[k], &r)) { ReportError(line, "'%s' is not a register (r0-r7)", ops[k]); return false; }
            w |= (unsigned)r << (f == 'd' ? 6 : f == 'a' ? 3 : 0);
        }
    }
    *out = (uint16_t)w;
    return true;
}

static void emit(const AssemblerState *state, unsigned addr, uint16_t w, int line, const char *src)
{
    if (addr >= MEM_WORDS) { ReportError(line, "program exceeds 64K words"); return; }
    if (image.used[addr]) { ReportError(line, "address 0x%04X already written", addr); return; }
    image.mem[addr] = w; image.used[addr] = true;
    if ((int)addr > image.max_addr) image.max_addr = (int)addr;
    if (state->listing) fprintf(state->listing, "%04X: %04X  %s\n", addr, w, src ? src : "");
}

/* JMP rt,label     / JZL ra,rt,label : LDI rt,off ; J rt / JZ ra,rt   reach -32..31
 * LJMP label        / LJZ ra,label    : LDW off    ; J r7 / JZ ra,r7   reach -16384..16383, overwrites r7
 * off = label - address of the jump instruction (the jump is the second word, at addr+1). */
static void do_jump_pseudo(const AssemblerState *state, const ParsedLine *p, unsigned addr, int line, const char *src)
{
    bool lng  = EqualsIgnoreCase(p->mnemonic, "LJMP") || EqualsIgnoreCase(p->mnemonic, "LJZ");
    bool cond = EqualsIgnoreCase(p->mnemonic, "JZL")  || EqualsIgnoreCase(p->mnemonic, "LJZ");
    int want = lng ? (cond ? 2 : 1) : (cond ? 3 : 2), li, rt;
    static char r7s[] = "r7", *rts;
    const char *lab; char num[16], *first_ops[2], *j_ops[2];
    long lo = lng ? K_MIN : IMM_MIN, hi = lng ? K_MAX : IMM_MAX, off;
    uint16_t w1, w2;
    if (p->operandCount != want) { ReportError(line, "%s takes %d operand(s)", p->mnemonic, want); return; }
    rts = r7s;
    if (!lng) {
        rts = p->operands[want - 2];
        if (!ParseRegister(rts, &rt) || rt == 0) { ReportError(line, "scratch register '%s' must be r1-r7", rts); return; }
    }
    lab = p->operands[want - 1];
    li = find_label(lab);
    if (li < 0) { ReportError(line, "undefined label '%s'", lab); return; }
    off = (long)labels[li].addr - (long)(addr + 1); /* the jump sits at addr+1 */
    if (off < lo || off > hi) {
        ReportError(line, "label '%s' is %ld words away; %s reaches %ld..%ld%s", lab, off, p->mnemonic, lo, hi,
            lng ? "" : " (LJMP/LJZ reach further but overwrite r7)");
        return;
    }
    snprintf(num, sizeof num, "%ld", off);
    if (lng) { first_ops[0] = num; if (!encode_instr(line, "LDW", first_ops, 1, &w1)) return; }
    else     { first_ops[0] = rts; first_ops[1] = num; if (!encode_instr(line, "LDI", first_ops, 2, &w1)) return; }
    if (cond) { j_ops[0] = p->operands[0]; j_ops[1] = rts; if (!encode_instr(line, "JZ", j_ops, 2, &w2)) return; }
    else      { j_ops[0] = rts; if (!encode_instr(line, "J", j_ops, 1, &w2)) return; }
    emit(state, addr, w1, line, src);
    emit(state, addr + 1, w2, line, NULL);
}

static void do_statement(const AssemblerState *state, const ParsedLine *p, unsigned addr, int line, const char *src)
{
    static char r0s[] = "r0"; uint16_t w; char *t[3];
    if (EqualsIgnoreCase(p->mnemonic, ".word")) {
        long v;
        if (p->operandCount != 1) { ReportError(line, ".word takes one value"); return; }
        if (ParseNumber(p->operands[0], &v)) {
            if (v < -32768 || v > 65535) { ReportError(line, ".word value out of 16-bit range"); return; }
        } else {
            int li = find_label(p->operands[0]);
            if (li < 0) { ReportError(line, "'%s' is neither a number nor a defined label", p->operands[0]); return; }
            v = labels[li].addr;
        }
        emit(state, addr, (uint16_t)(v & 0xFFFF), line, src);
    } else if (p->mnemonic[0] == '.') {
        ReportError(line, "unknown directive '%s'", p->mnemonic);
    } else if (is_pseudo2(p->mnemonic)) {
        do_jump_pseudo(state, p, addr, line, src);
    } else if (EqualsIgnoreCase(p->mnemonic, "NOP")) {
        if (p->operandCount != 0) { ReportError(line, "NOP takes no operands"); return; }
        t[0] = t[1] = t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(state, addr, w, line, src);
    } else if (EqualsIgnoreCase(p->mnemonic, "MOV")) {
        if (p->operandCount != 2) { ReportError(line, "MOV takes rd, ra"); return; }
        t[0] = p->operands[0]; t[1] = p->operands[1]; t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(state, addr, w, line, src);
    } else if (EqualsIgnoreCase(p->mnemonic, "CLR")) {
        if (p->operandCount != 1) { ReportError(line, "CLR takes rd"); return; }
        t[0] = p->operands[0]; t[1] = t[2] = r0s;
        if (encode_instr(line, "ADD", t, 3, &w)) emit(state, addr, w, line, src);
    } else if (encode_instr(line, p->mnemonic, p->operands, p->operandCount, &w)) {
        emit(state, addr, w, line, src);
    }
}

/* ---------- passes ---------- */
void run_passes(char **lines, int nlines, const AssemblerState *state)
{
    unsigned addr = 0; int i; char tmp[LINE_LEN]; ParsedLine p;
    for (i = 0; i < nlines; i++) { /* pass 1: addresses + labels */
        strcpy(tmp, lines[i]); ParseLine(tmp, i + 1, &p, true);
        if (!p.isValid) continue;
        if (p.hasLabel) define_label(p.label, addr, i + 1);
        if (p.hasMnemonic && EqualsIgnoreCase(p.mnemonic, ".org")) step_org(&p, &addr, i + 1, true);
        addr += (unsigned)stmt_size(&p);
    }
    addr = 0;
    for (i = 0; i < nlines; i++) { /* pass 2: encode */
        strcpy(tmp, lines[i]); ParseLine(tmp, i + 1, &p, false);
        if (!p.isValid || !p.hasMnemonic) continue;
        if (EqualsIgnoreCase(p.mnemonic, ".org")) { step_org(&p, &addr, i + 1, false); continue; }
        do_statement(state, &p, addr, i + 1, lines[i]);
        addr += (unsigned)stmt_size(&p);
    }
}
