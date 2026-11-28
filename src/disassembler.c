/*
 * disassembler.c - the disassembler behind sweet16asm -d. It reads the same ISA table as the
 * assembler, so which opcodes and operand fields exist is defined in one place (isa.csv), and it
 * writes source that the assembler turns back into exactly the same words.
 * Phase 1 only: every instruction is written in its plain table form. There are no pseudo-ops
 * (NOP, MOV, CLR, JMP, JZL, LJMP, LJZ) and no synthesized labels.
 */
#include <stdint.h>
#include <stdio.h>

#include "constants.h"
#include "disassembler.h"
#include "isa.h"
#include "output.h"

/* Room for one statement: the longest mnemonic plus up to three operands of at most 5 characters each. */
#define STATEMENT_SIZE (NAME_LEN + 32)

/*
 * DecodeWord - write the assembly text for one 16-bit word.
 * word: the word. text: where the statement goes, STATEMENT_SIZE characters (without indent or newline).
 * The word is read by its top two bits:
 *   00 or 11  LDW k, when the table has the whole-word instruction (k is the word read as a signed
 *             16-bit number, which lies in K_MIN..K_MAX exactly for these two types)
 *   01        an instruction: bits 13:9 are the opcode; of the table entries with that opcode, those
 *             whose unused fields (the bits 8:0 that none of their operands names) are all zero could
 *             have produced the word, and the one with the fewest operands is written (the first in
 *             the table when several tie). This is the rule that makes J, JZ and JAL (one opcode,
 *             told apart only by the d and a fields) come out as the shortest form that fits.
 *   10        no instruction uses it
 * A word that fits no instruction, or LDW when the table has none, is written as ".word 0xNNNN".
 * Because only exact fits are written, assembling the text always gives the same word back.
 */
static void DecodeWord(uint16_t word, char *text)
{
    int index, bestIndex = -1, signedWord = word >= 0x8000u ? (int)word - 65536 : (int)word;
    unsigned opcode = (word >> OPCODE_SHIFT) & OPCODE_MASK;
    /* LDW: look for the whole-word entry, which has no opcode or fields */
    for (index = 0; index < ISAEntryCount(); index++) {
        const ISAEntry *entry = ISAEntryAt(index);
        if (entry->isWholeWord) {
            if (signedWord >= K_MIN && signedWord <= K_MAX) { snprintf(text, STATEMENT_SIZE, "%s %d", entry->mnemonic, signedWord); return; }
            break;
        }
    }
    /* instructions: among the entries with this opcode, pick the exact fit with the fewest operands */
    if ((word & TYPE_MASK) == PREFIX_BITS) {
        for (index = 0; index < ISAEntryCount(); index++) {
            const ISAEntry *entry = ISAEntryAt(index); unsigned usedBits = 0; int operandIndex;
            if (entry->isWholeWord || entry->opcode != opcode) continue;
            /* the bits this entry's operands cover; every other bit below the opcode must be 0 */
            for (operandIndex = 0; operandIndex < entry->operandCount; operandIndex++) usedBits |= ISAFieldMask(entry->operandCodes[operandIndex]);
            if (word & FIELDS_MASK & ~usedBits) continue;
            /* strictly fewer operands wins, so a tie keeps the earlier table entry */
            if (bestIndex < 0 || entry->operandCount < ISAEntryAt(bestIndex)->operandCount) bestIndex = index;
        }
    }
    /* nothing fits: keep the word as data */
    if (bestIndex < 0) { snprintf(text, STATEMENT_SIZE, ".word 0x%04X", (unsigned)word); return; }
    {
        const ISAEntry *entry = ISAEntryAt(bestIndex); int operandIndex, length = snprintf(text, STATEMENT_SIZE, "%s", entry->mnemonic);
        /* the operands in assembly order, separated by ", " */
        for (operandIndex = 0; operandIndex < entry->operandCount; operandIndex++) {
            char code = entry->operandCodes[operandIndex];
            int field = (int)((word & ISAFieldMask(code)) >> ISAFieldShift(code));
            const char *separator = operandIndex ? ", " : " ";
            /* an immediate is a signed 6-bit number; a register is r0-r7 */
            if (code == 'i') length += snprintf(text + length, STATEMENT_SIZE - (size_t)length, "%s%d", separator, field >= 32 ? field - 64 : field);
            else             length += snprintf(text + length, STATEMENT_SIZE - (size_t)length, "%sr%d", separator, field);
        }
    }
}

/*
 * DisassembleImage - write the whole image as source, one statement per written word.
 * out: where the text goes. Each statement is indented 8 spaces, as in tests/matrix_marshall.asm.
 * Addresses that were not written are skipped, with a ".org 0xNNNN" before the next written word so
 * it lands at the same address again (the addresses only ever go up, which .org requires). A raw
 * image has no such gaps, so this only shows for a v3 file whose rows do not run on from each other.
 */
void DisassembleImage(FILE *out)
{
    int address, nextAddress = 0;
    /* one address per pass */
    for (address = 0; address <= image.highestAddress; address++) {
        char text[STATEMENT_SIZE];
        if (!image.isWritten[address]) continue;
        if (address != nextAddress) fprintf(out, "        .org 0x%04X\n", (unsigned)address);
        DecodeWord(image.words[address], text);
        fprintf(out, "        %s\n", text);
        nextAddress = address + 1;
    }
}
