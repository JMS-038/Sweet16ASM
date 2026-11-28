/*
 * disassembler.h - turns the shared image back into Sweet16 assembly source (sweet16asm -d).
 */
#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stdio.h>

/* Writes the words of the shared image (filled by ReadV3Image or ReadBinImage) to out as assembly
 * source that the assembler accepts and turns back into exactly the same words: one statement per
 * word, a ".org" line wherever a gap is skipped, and ".word 0xNNNN" for any word that is not exactly
 * one instruction. Needs the ISA table to be loaded. The rules for choosing between encodings that
 * overlap are in the README ("Disassembler"). */
void DisassembleImage(FILE *out);

#endif /* DISASSEMBLER_H */
