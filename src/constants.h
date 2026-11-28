/*
 * constants.h - size limits and encoding constants shared by the Sweet16 assembler modules.
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MEM_WORDS  65536  /* words in the program ROM: the whole 16-bit address space */
#define MAX_ISA    64     /* most instructions isa.csv may define */
#define MAX_LABELS 4096   /* most labels one program may define */
#define MAX_OPS    8      /* most operands one statement may have */
#define NAME_LEN   64     /* size of a label or mnemonic buffer: names are at most NAME_LEN - 1 characters */
#define LINE_LEN   512    /* size of a source-line buffer: lines are at most LINE_LEN - 2 characters */
#define PREFIX_BITS 0x4000u /* bits 15:14 = 01 on every instruction */
#define TYPE_MASK   0xC000u /* bits 15:14, the type: 00 and 11 = LDW, 01 = instruction, 10 = unused */
#define OPCODE_SHIFT 9      /* opcode = bits 13:9 */
#define OPCODE_MASK  0x1Fu
#define FIELDS_MASK  0x01FFu /* bits 8:0: everything below the opcode (the d, a, b and i fields) */
#define FIELD_D_SHIFT 6     /* register field d = bits 8:6 */
#define FIELD_A_SHIFT 3     /* register field a = bits 5:3 */
#define FIELD_B_SHIFT 0     /* register field b = bits 2:0 */
#define REG_MASK 0x7u       /* a register field is 3 bits wide */
#define IMM_MASK 0x3Fu      /* operand 'i': a 6-bit immediate in bits 5:0 */
#define IMM_MIN (-32) /* smallest 6-bit signed immediate (operand code 'i') */
#define IMM_MAX 31    /* largest 6-bit signed immediate */
#define K_MIN (-16384) /* LDW: sign-extended 15-bit value (types 00 and 11) */
#define K_MAX 16383

#endif /* CONSTANTS_H */
