/*
 * assembler.c - the two assembly passes for the Sweet16 assembler: statement sizing, .org,
 * instruction encoding, pseudo-op expansion and directives.
 * Moved out of main.c with one mechanical change: the listing file that used to be a global is
 * now reached through an AssemblerState passed down (EmitWord, AssembleJumpPseudo, AssembleStatement and
 * RunPasses take a state parameter). RunPasses is the only public function.
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
static bool IsTwoWordPseudo(const char *mnemonic) /* pseudo-ops that expand to 2 words */
{
    return EqualsIgnoreCase(mnemonic, "JMP") || EqualsIgnoreCase(mnemonic, "JZL") || EqualsIgnoreCase(mnemonic, "LJMP") || EqualsIgnoreCase(mnemonic, "LJZ");
}

static int StatementSize(const ParsedLine *parsed)
{
    if (!parsed->hasMnemonic || EqualsIgnoreCase(parsed->mnemonic, ".org")) return 0;
    return IsTwoWordPseudo(parsed->mnemonic) ? 2 : 1;
}

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
static bool EncodeInstruction(int lineNumber, const char *mnemonic, char *const *operands, int operandCount, uint16_t *wordOut)
{
    const ISAEntry *entry = FindISAEntry(mnemonic);
    unsigned word; int index;
    if (!entry) { ReportError(lineNumber, "unknown instruction '%s'", mnemonic); return false; }
    if (operandCount != entry->operandCount) { ReportError(lineNumber, "%s takes %d operand(s), got %d", entry->mnemonic, entry->operandCount, operandCount); return false; }
    if (entry->isWholeWord) { /* LDW k: the instruction word is k itself (00.. for k>=0, 11.. for k<0) */
        long value;
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
    word = PREFIX_BITS | (entry->opcode << 9);
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

static void EmitWord(const AssemblerState *state, unsigned address, uint16_t word, int lineNumber, const char *sourceText)
{
    if (address >= MEM_WORDS) { ReportError(lineNumber, "program exceeds 64K words"); return; }
    if (image.isWritten[address]) { ReportError(lineNumber, "address 0x%04X already written", address); return; }
    image.words[address] = word; image.isWritten[address] = true;
    if ((int)address > image.highestAddress) image.highestAddress = (int)address;
    if (state->listingFile) fprintf(state->listingFile, "%04X: %04X  %s\n", address, word, sourceText ? sourceText : "");
}

/* JMP rt,label     / JZL ra,rt,label : LDI rt,offset ; J rt / JZ ra,rt   reach -32..31
 * LJMP label        / LJZ ra,label    : LDW offset    ; J r7 / JZ ra,r7   reach -16384..16383, overwrites r7
 * offset = label - address of the jump instruction (the jump is the second word, at address + 1). */
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
    scratchRegister = r7Text;
    if (!usesLDW) {
        scratchRegister = parsed->operands[expectedOperands - 2];
        if (!ParseRegister(scratchRegister, &scratchNumber) || scratchNumber == 0) { ReportError(lineNumber, "scratch register '%s' must be r1-r7", scratchRegister); return; }
    }
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
    snprintf(offsetText, sizeof offsetText, "%ld", offset);
    if (usesLDW) { loadOperands[0] = offsetText; if (!EncodeInstruction(lineNumber, "LDW", loadOperands, 1, &loadWord)) return; }
    else     { loadOperands[0] = scratchRegister; loadOperands[1] = offsetText; if (!EncodeInstruction(lineNumber, "LDI", loadOperands, 2, &loadWord)) return; }
    if (isConditional) { jumpOperands[0] = parsed->operands[0]; jumpOperands[1] = scratchRegister; if (!EncodeInstruction(lineNumber, "JZ", jumpOperands, 2, &jumpWord)) return; }
    else      { jumpOperands[0] = scratchRegister; if (!EncodeInstruction(lineNumber, "J", jumpOperands, 1, &jumpWord)) return; }
    EmitWord(state, address, loadWord, lineNumber, sourceText);
    EmitWord(state, address + 1, jumpWord, lineNumber, NULL);
}

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
void RunPasses(char **lines, int lineCount, const AssemblerState *state)
{
    unsigned address = 0; int index; char scratchLine[LINE_LEN]; ParsedLine parsed;
    for (index = 0; index < lineCount; index++) { /* pass 1: addresses + labels */
        strcpy(scratchLine, lines[index]); ParseLine(scratchLine, index + 1, &parsed, true);
        if (!parsed.isValid) continue;
        if (parsed.hasLabel) DefineLabel(parsed.label, address, index + 1);
        if (parsed.hasMnemonic && EqualsIgnoreCase(parsed.mnemonic, ".org")) ApplyOrgDirective(&parsed, &address, index + 1, true);
        address += (unsigned)StatementSize(&parsed);
    }
    address = 0;
    for (index = 0; index < lineCount; index++) { /* pass 2: encode */
        strcpy(scratchLine, lines[index]); ParseLine(scratchLine, index + 1, &parsed, false);
        if (!parsed.isValid || !parsed.hasMnemonic) continue;
        if (EqualsIgnoreCase(parsed.mnemonic, ".org")) { ApplyOrgDirective(&parsed, &address, index + 1, false); continue; }
        AssembleStatement(state, &parsed, address, index + 1, lines[index]);
        address += (unsigned)StatementSize(&parsed);
    }
}
