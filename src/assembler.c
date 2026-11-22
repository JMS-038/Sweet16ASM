/*
 * assembler.c - the two assembly passes. RunPasses (the only public function) parses every source
 * line twice: pass 1 gives each statement an address and records labels, pass 2 encodes each
 * statement into the image. The helpers here size statements, apply .org, encode instructions from
 * the ISA table, expand the pseudo-ops (NOP, MOV, CLR, JMP, JZL, LJMP, LJZ) and handle .word.
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
/*
 * IsTwoWordPseudo - true for the pseudo-ops that expand to two words: JMP, JZL, LJMP and LJZ
 * (letter case ignored). StatementSize and AssembleStatement both use it.
 */
static bool IsTwoWordPseudo(const char *mnemonic) /* pseudo-ops that expand to 2 words */
{
    return EqualsIgnoreCase(mnemonic, "JMP") || EqualsIgnoreCase(mnemonic, "JZL") || EqualsIgnoreCase(mnemonic, "LJMP") || EqualsIgnoreCase(mnemonic, "LJZ");
}

/*
 * StatementSize - how many words a parsed line takes in the image.
 * 0 for a line with no mnemonic and for .org (which only moves the address), 2 for the two-word
 * pseudo-ops, 1 for everything else. An unknown mnemonic also counts as 1 word; it is reported as
 * an error in pass 2.
 */
static int StatementSize(const ParsedLine *parsed)
{
    if (!parsed->hasMnemonic || EqualsIgnoreCase(parsed->mnemonic, ".org")) return 0;
    return IsTwoWordPseudo(parsed->mnemonic) ? 2 : 1;
}

/*
 * ApplyOrgDirective - carry out ".org <address>": move *address forward to the given address.
 * parsed: the .org statement. address: the current address, updated on success. lineNumber: for
 * error messages. reportErrors: true in pass 1 so each error appears once (pass 2 repeats the same
 * checks silently, so it ends up at the same address).
 * Leaves *address unchanged and reports an error when the operand is missing, is not a number, is
 * outside 0..65535, or is before the current address (.org cannot move backwards).
 */
static void ApplyOrgDirective(const ParsedLine *parsed, unsigned *address, int lineNumber, bool reportErrors)
{
    long target;
    if (parsed->operandCount != 1 || !ParseNumber(parsed->operands[0], &target) || target < 0 || target >= MEM_WORDS) {
        if (reportErrors) ReportError(lineNumber, ".org needs one address in 0..65535");
        return;
    }
    if ((unsigned)target < *address) { if (reportErrors) ReportError(lineNumber, ".org cannot move backwards (at 0x%04X)", *address); return; }
    *address = (unsigned)target;
}

/* ---------- encoding ---------- */
/*
 * EncodeInstruction - build the 16-bit word for one instruction from its mnemonic and operands,
 * using that mnemonic's entry in the ISA table.
 * lineNumber: for error messages. mnemonic, operands, operandCount: the instruction as written.
 * wordOut: receives the word on success.
 * Returns true on success. Otherwise it reports an error and returns false without setting *wordOut:
 * unknown mnemonic, wrong operand count, a register that is not r0-r7, an immediate outside
 * IMM_MIN..IMM_MAX, or an LDW operand that is neither a number nor a defined label or is outside
 * K_MIN..K_MAX.
 * Word layout: bits 15:14 = 01 (PREFIX_BITS), bits 13:9 = the opcode, then each operand goes in the
 * field its code names in isa.csv: d = bits 8:6, a = bits 5:3, b = bits 2:0 (registers), i = bits 5:0
 * (a 6-bit immediate). The whole-word instruction LDW has no fields: the word is the value itself.
 */
static bool EncodeInstruction(int lineNumber, const char *mnemonic, char *const *operands, int operandCount, uint16_t *wordOut)
{
    const ISAEntry *entry = FindISAEntry(mnemonic);
    unsigned word; int index;
    if (!entry) { ReportError(lineNumber, "unknown instruction '%s'", mnemonic); return false; }
    if (operandCount != entry->operandCount) { ReportError(lineNumber, "%s takes %d operand(s), got %d", entry->mnemonic, entry->operandCount, operandCount); return false; }
    if (entry->isWholeWord) { /* LDW k: the instruction word is k itself (00.. for k>=0, 11.. for k<0) */
        long value;
        /* not a number: it must be a label, which pass 1 has already defined */
        if (!ParseNumber(operands[0], &value)) {
            int labelIndex = FindLabel(operands[0]);
            if (labelIndex < 0) { ReportError(lineNumber, "'%s' is neither a number nor a defined label", operands[0]); return false; }
            value = (long)labelTable[labelIndex].address;
            if (value > K_MAX) { ReportError(lineNumber, "label '%s' (address 0x%04lX) does not fit %s (max %d)", operands[0], value, entry->mnemonic, K_MAX); return false; }
        }
        if (value < K_MIN || value > K_MAX) { ReportError(lineNumber, "%s value %ld out of range (%d..%d)", entry->mnemonic, value, K_MIN, K_MAX); return false; }
        *wordOut = (uint16_t)(value & 0xFFFF);
        return true;
    }
    /* a regular instruction: the fixed prefix and the opcode first, then the operand fields below */
    word = PREFIX_BITS | (entry->opcode << 9);
    /* place each operand in the field its operand code names */
    for (index = 0; index < operandCount; index++) {
        char code = entry->operandCodes[index];
        if (code == 'i') {
            long value;
            if (!ParseNumber(operands[index], &value)) { ReportError(lineNumber, "'%s' is not a number", operands[index]); return false; }
            if (value < IMM_MIN || value > IMM_MAX) { ReportError(lineNumber, "immediate %ld out of range (%d..%d)", value, IMM_MIN, IMM_MAX); return false; }
            word |= (unsigned)(value & 0x3F);
        } else {
            int registerNumber;
            if (!ParseRegister(operands[index], &registerNumber)) { ReportError(lineNumber, "'%s' is not a register (r0-r7)", operands[index]); return false; }
            word |= (unsigned)registerNumber << (code == 'd' ? 6 : code == 'a' ? 3 : 0);
        }
    }
    *wordOut = (uint16_t)word;
    return true;
}

/*
 * EmitWord - store one assembled word in the image and write its listing line.
 * state: where the listing goes (state->listingFile; NULL means no listing).
 * address: the word's position. word: its value. lineNumber: for error messages.
 * sourceText: the source line to show in the listing, or NULL to show none (the second word of a
 * two-word pseudo-op).
 * Side effects: sets the word in image, marks the address written, raises image.highestAddress, and
 * prints "AAAA: WWWW  source" to the listing. Reports an error and stores nothing when the address is
 * past the end of the 64K image or already holds a word.
 */
static void EmitWord(const AssemblerState *state, unsigned address, uint16_t word, int lineNumber, const char *sourceText)
{
    if (address >= MEM_WORDS) { ReportError(lineNumber, "program exceeds 64K words"); return; }
    if (image.isWritten[address]) { ReportError(lineNumber, "address 0x%04X already written", address); return; }
    image.words[address] = word; image.isWritten[address] = true;
    if ((int)address > image.highestAddress) image.highestAddress = (int)address;
    if (state->listingFile) fprintf(state->listingFile, "%04X: %04X  %s\n", address, word, sourceText ? sourceText : "");
}

/*
 * AssembleJumpPseudo - assemble one of the two-word jump pseudo-ops: load the distance to a label into
 * a scratch register, then jump through that register.
 *   JMP rt,label     / JZL ra,rt,label : LDI rt,offset ; J rt / JZ ra,rt   reach -32..31
 *   LJMP label       / LJZ ra,label    : LDW offset    ; J r7 / JZ ra,r7   reach -16384..16383, overwrites r7
 * offset = label - address of the jump instruction (the jump is the second word, at address + 1).
 * state, address, lineNumber, sourceText: as for EmitWord; address is where the first word goes.
 * Emits both words on success. Reports an error and emits nothing for a wrong operand count, a scratch
 * register that is not r1-r7 (JMP and JZL), an undefined label, a label out of reach, or an operand the
 * encoder rejects.
 */
static void AssembleJumpPseudo(const AssemblerState *state, const ParsedLine *parsed, unsigned address, int lineNumber, const char *sourceText)
{
    bool usesLDW  = EqualsIgnoreCase(parsed->mnemonic, "LJMP") || EqualsIgnoreCase(parsed->mnemonic, "LJZ");
    bool isConditional = EqualsIgnoreCase(parsed->mnemonic, "JZL")  || EqualsIgnoreCase(parsed->mnemonic, "LJZ");
    int expectedOperands = usesLDW ? (isConditional ? 2 : 1) : (isConditional ? 3 : 2), labelIndex, scratchNumber;
    static char r7Text[] = "r7", *scratchRegister;
    const char *labelName; char offsetText[16], *loadOperands[2], *jumpOperands[2];
    long minOffset = usesLDW ? K_MIN : IMM_MIN, maxOffset = usesLDW ? K_MAX : IMM_MAX, offset;
    uint16_t loadWord, jumpWord;
    if (parsed->operandCount != expectedOperands) { ReportError(lineNumber, "%s takes %d operand(s)", parsed->mnemonic, expectedOperands); return; }
    /* LJMP and LJZ use r7 as the scratch register, because LDW always loads r7 */
    scratchRegister = r7Text;
    if (!usesLDW) {
        /* JMP rt,label and JZL ra,rt,label: the scratch register is the operand just before the label */
        scratchRegister = parsed->operands[expectedOperands - 2];
        if (!ParseRegister(scratchRegister, &scratchNumber) || scratchNumber == 0) { ReportError(lineNumber, "scratch register '%s' must be r1-r7", scratchRegister); return; }
    }
    /* the label is always the last operand */
    labelName = parsed->operands[expectedOperands - 1];
    labelIndex = FindLabel(labelName);
    if (labelIndex < 0) { ReportError(lineNumber, "undefined label '%s'", labelName); return; }
    offset = (long)labelTable[labelIndex].address - (long)(address + 1); /* the jump sits at address + 1 */
    if (offset < minOffset || offset > maxOffset) {
        ReportError(lineNumber, "label '%s' is %ld words away; %s reaches %ld..%ld%s", labelName, offset, parsed->mnemonic, minOffset, maxOffset,
            usesLDW ? "" : " (LJMP/LJZ reach further but overwrite r7)");
        return;
    }
    if (isConditional) { /* the offset is loaded into the scratch register first, so it must not be the register being tested */
        int conditionNumber;
        if (ParseRegister(parsed->operands[0], &conditionNumber) && conditionNumber == (usesLDW ? 7 : scratchNumber)) {
            if (usesLDW) ReportError(lineNumber, "r7 cannot be the condition register of LJZ; LDW overwrites it with the jump offset before it is tested");
            else         ReportError(lineNumber, "scratch register r%d is also the condition register; the jump offset overwrites it before it is tested", conditionNumber);
            return;
        }
    }
    /* the offset goes to the encoder as text, so it is parsed and range-checked like any other operand */
    snprintf(offsetText, sizeof offsetText, "%ld", offset);
    /* first word: load the offset (LDW into r7, or LDI into the scratch register) */
    if (usesLDW) { loadOperands[0] = offsetText; if (!EncodeInstruction(lineNumber, "LDW", loadOperands, 1, &loadWord)) return; }
    else     { loadOperands[0] = scratchRegister; loadOperands[1] = offsetText; if (!EncodeInstruction(lineNumber, "LDI", loadOperands, 2, &loadWord)) return; }
    /* second word: the jump through the scratch register (JZ tests ra first for the conditional forms) */
    if (isConditional) { jumpOperands[0] = parsed->operands[0]; jumpOperands[1] = scratchRegister; if (!EncodeInstruction(lineNumber, "JZ", jumpOperands, 2, &jumpWord)) return; }
    else      { jumpOperands[0] = scratchRegister; if (!EncodeInstruction(lineNumber, "J", jumpOperands, 1, &jumpWord)) return; }
    EmitWord(state, address, loadWord, lineNumber, sourceText);
    /* the second word has no source text of its own in the listing */
    EmitWord(state, address + 1, jumpWord, lineNumber, NULL);
}

/*
 * AssembleStatement - assemble one statement at address and emit its word(s).
 * parsed: the parsed line (it has a mnemonic and is not .org). state, address, lineNumber, sourceText:
 * as for EmitWord.
 * Handles, in this order: ".word", an unknown directive (any mnemonic starting with '.'), the two-word
 * jump pseudo-ops, NOP, MOV and CLR (each an ADD that uses r0), and finally any instruction in the ISA
 * table.
 *   NOP = ADD r0,r0,r0     MOV rd,ra = ADD rd,ra,r0     CLR rd = ADD rd,r0,r0
 *   .word takes one number (-32768..65535) or label and stores its low 16 bits.
 * Reports an error and emits nothing when the statement is invalid.
 */
static void AssembleStatement(const AssemblerState *state, const ParsedLine *parsed, unsigned address, int lineNumber, const char *sourceText)
{
    static char r0Text[] = "r0"; uint16_t word; char *addOperands[3];
    if (EqualsIgnoreCase(parsed->mnemonic, ".word")) {
        long value;
        if (parsed->operandCount != 1) { ReportError(lineNumber, ".word takes one value"); return; }
        if (ParseNumber(parsed->operands[0], &value)) {
            if (value < -32768 || value > 65535) { ReportError(lineNumber, ".word value out of 16-bit range"); return; }
        } else {
            int labelIndex = FindLabel(parsed->operands[0]);
            if (labelIndex < 0) { ReportError(lineNumber, "'%s' is neither a number nor a defined label", parsed->operands[0]); return; }
            value = labelTable[labelIndex].address;
        }
        EmitWord(state, address, (uint16_t)(value & 0xFFFF), lineNumber, sourceText);
    } else if (parsed->mnemonic[0] == '.') {
        ReportError(lineNumber, "unknown directive '%s'", parsed->mnemonic);
    } else if (IsTwoWordPseudo(parsed->mnemonic)) {
        AssembleJumpPseudo(state, parsed, address, lineNumber, sourceText);
    } else if (EqualsIgnoreCase(parsed->mnemonic, "NOP")) {
        if (parsed->operandCount != 0) { ReportError(lineNumber, "NOP takes no operands"); return; }
        addOperands[0] = addOperands[1] = addOperands[2] = r0Text;
        if (EncodeInstruction(lineNumber, "ADD", addOperands, 3, &word)) EmitWord(state, address, word, lineNumber, sourceText);
    } else if (EqualsIgnoreCase(parsed->mnemonic, "MOV")) {
        if (parsed->operandCount != 2) { ReportError(lineNumber, "MOV takes rd, ra"); return; }
        addOperands[0] = parsed->operands[0]; addOperands[1] = parsed->operands[1]; addOperands[2] = r0Text;
        if (EncodeInstruction(lineNumber, "ADD", addOperands, 3, &word)) EmitWord(state, address, word, lineNumber, sourceText);
    } else if (EqualsIgnoreCase(parsed->mnemonic, "CLR")) {
        if (parsed->operandCount != 1) { ReportError(lineNumber, "CLR takes rd"); return; }
        addOperands[0] = parsed->operands[0]; addOperands[1] = addOperands[2] = r0Text;
        if (EncodeInstruction(lineNumber, "ADD", addOperands, 3, &word)) EmitWord(state, address, word, lineNumber, sourceText);
    } else if (EncodeInstruction(lineNumber, parsed->mnemonic, parsed->operands, parsed->operandCount, &word)) {
        EmitWord(state, address, word, lineNumber, sourceText);
    }
}

/* ---------- passes ---------- */
/*
 * RunPasses - assemble the whole program in two passes.
 * lines: the lineCount source lines, without line endings. They are not modified: ParseLine edits its
 * buffer, so each pass parses a copy.
 * state: where the listing goes (see EmitWord).
 * Pass 1 gives every statement an address and defines the labels, so forward references work.
 * Pass 2 encodes each statement and stores its words in the image.
 * Side effects: fills labelTable and image, writes the listing, and reports problems with ReportError
 * (the caller checks errorCount afterwards).
 */
void RunPasses(char **lines, int lineCount, const AssemblerState *state)
{
    unsigned address = 0; int index; char scratchLine[LINE_LEN]; ParsedLine parsed;
    /* pass 1: for each line, define its label at the current address, apply .org, and move the address
     * past the statement. A malformed line is reported here (once) and takes no words. */
    for (index = 0; index < lineCount; index++) {
        /* parse a copy of the line, because ParseLine edits its buffer */
        strcpy(scratchLine, lines[index]); ParseLine(scratchLine, index + 1, &parsed, true);
        if (!parsed.isValid) continue;
        if (parsed.hasLabel) DefineLabel(parsed.label, address, index + 1);
        if (parsed.hasMnemonic && EqualsIgnoreCase(parsed.mnemonic, ".org")) ApplyOrgDirective(&parsed, &address, index + 1, true);
        address += (unsigned)StatementSize(&parsed);
    }
    address = 0;
    /* pass 2: for each line, apply .org or assemble the statement at the current address. Syntax errors
     * were reported in pass 1, so the line is parsed silently here. */
    for (index = 0; index < lineCount; index++) {
        strcpy(scratchLine, lines[index]); ParseLine(scratchLine, index + 1, &parsed, false);
        if (!parsed.isValid || !parsed.hasMnemonic) continue;
        if (EqualsIgnoreCase(parsed.mnemonic, ".org")) { ApplyOrgDirective(&parsed, &address, index + 1, false); continue; }
        AssembleStatement(state, &parsed, address, index + 1, lines[index]);
        address += (unsigned)StatementSize(&parsed);
    }
}
