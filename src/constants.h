/*
 * constants.h - size limits and encoding constants shared by the Sweet16 assembler modules.
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MEM_WORDS  65536
#define MAX_ISA    64
#define MAX_LABELS 4096
#define MAX_OPS    8
#define NAME_LEN   64
#define LINE_LEN   512
#define PREFIX_BITS 0x4000u /* bits 15:14 = 01 on every instruction */
#define IMM_MIN (-32)
#define IMM_MAX 31
#define K_MIN (-16384) /* LDW: sign-extended 15-bit value (types 00 and 11) */
#define K_MAX 16383

#endif /* CONSTANTS_H */
