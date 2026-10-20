# sweet16asm: an assembler for the EE 340 Sweet16 MCU

`sweet16asm` turns Sweet16 assembly source into a ROM image that loads into the
Logisim implementation of the Sweet16 microcontroller. It is written in C99,
uses only the standard C library (no external dependencies), and reads its
instruction set from a CSV table instead of hard-coding it.

- **Author:** Joshua Smith
- **Course:** EE 340 Computer Architecture, 2026
- **Machine described by:** *Sweet16 MCU Instruction Summary* (Brad Smith, EE 340, revision 2026-10-14)

## Quick start

```sh
make                                   # builds ./sweet16asm
./sweet16asm examples/program2.asm     # writes examples/program2.hex
./sweet16asm -l prog.lst prog.asm      # also writes an address / word / source listing
make test                              # runs the regression tests
```

**Windows:** install MSYS2 (`winget install MSYS2.MSYS2`), then in the *MSYS2 UCRT64* terminal run
`pacman -S mingw-w64-ucrt-x86_64-gcc make diffutils`. In that terminal, `cd /c/path/to/this/repo` and use
`make` and `make test` exactly as above. To use `make` from PowerShell, add `C:\msys64\ucrt64\bin` and
`C:\msys64\usr\bin` to your Path (in that order). The Makefile picks `gcc` and the `.exe` name
automatically on Windows. Without `make`, one command also works:
`gcc -std=c99 -Wall -Wextra -pedantic -O2 -o sweet16asm.exe sweet16asm.c`, then
`.\sweet16asm.exe examples\program2.asm` from the repository folder (the ISA table `isa.csv` is read from
the current directory unless you pass `-i`). `make test` needs a Unix-style shell, which the MSYS2 terminal provides.

The default output is a Logisim `v3.0 hex words addressed` text file. In Logisim,
load it into the program ROM with the ROM's *Load Image* menu item.

```
usage: sweet16asm [-i isa.csv] [-o out] [-f v3|bin] [-l listing] input.asm
  -i  ISA table (default ./isa.csv)
  -f  v3  = Logisim 'v3.0 hex words addressed' text (default)
      bin = raw 64K big-endian 16-bit words (131072 bytes)
```

Any error is reported as `file:line: error: message`, and no output file is
written unless the whole program assembles cleanly.

## Source syntax

```asm
; a comment runs from ';' to the end of the line
start:  LDI r2, 1          ; label, instruction, operands separated by commas
        IN  r3             ; labels may also sit on their own line
        add R1,r2,R3       ; mnemonics and registers are case-insensitive
```

- Registers are `r0` to `r7`. `r0` always reads 0 and ignores writes (per the ISA summary).
- Numbers are decimal, `0x` hex or `0b` binary, with an optional sign.
- `LDI` takes a signed 6-bit immediate, so the range is -32 to 31. Anything else is an error.
- `LDW k` loads a bigger value into **R7**: `k` is -16384 to 16383, or a label (its address, up to 16383).
- Blank lines, comment-only lines, Windows line endings and `ADD,r1,r2,r3` spacing are accepted.

### Directives

| Directive | Meaning |
|---|---|
| `.org addr` | Continue assembling at word address `addr` (forward only). Skipped words are zero. |
| `.word value` | Emit a raw 16-bit word. `value` is a number or a label (its address). |

### Pseudo-instructions

These are conveniences built on the real instructions. They are not part of the ISA.

| Pseudo | Expands to | Notes |
|---|---|---|
| `NOP` | `ADD r0, r0, r0` | Writes r0, which is discarded. |
| `MOV rd, ra` | `ADD rd, ra, r0` | |
| `CLR rd` | `ADD rd, r0, r0` | |
| `JMP rt, label` | `LDI rt, offset` then `J rt` | 2 words. `rt` is an explicit scratch register (r1 to r7). |
| `JZL ra, rt, label` | `LDI rt, offset` then `JZ ra, rt` | 2 words. Jumps to `label` if `ra` is 0. |
| `LJMP label` | `LDW offset` then `J r7` | 2 words. Reaches -16384 to 16383 words, but **overwrites R7**. |
| `LJZ ra, label` | `LDW offset` then `JZ ra, r7` | 2 words. Same reach; overwrites R7. |

**Why jumps need a scratch register.** `J`, `JZ` and `JAL` are register-relative:
`PC <- PC + R[B]`, where `PC` is the address of the jump instruction itself. A
label jump therefore has to load the distance into a register first. The
assembler computes `offset = label - address_of_the_jump` and loads it with
`LDI`, so the reach is -32 to 31 words from the jump. A label outside that
range is an error that reports the distance and points to `LJMP`/`LJZ`. Using a scratch register
explicitly means the assembler never silently overwrites one of your registers. `LJMP` and `LJZ`
use `LDW`, which always loads R7, so R7 is overwritten by definition.

Real listing from `examples/jumps.asm` (the second word of each pair is the jump):

```
0000: 5281  start:  LDI r2, 1
0001: 50C0          IN  r3
0002: 5240          LDI r1, 0
0003: 410B  loop:   SLT r4, r1, r3
0004: 537B          JZL r4, r5, start   ; LDI r5,off ; JZ r4,r5
0005: 5425
0006: 424A          ADD r1, r1, r2
0007: 53BB          JMP r6, loop        ; LDI r6,off ; J r6
0008: 5406
```

## Instruction set

Ordinary instructions: `01 ooooo ddd aaa bbb`, that is bits 15:14 = `01`, 13:9 = opcode, 8:6 = Rd,
5:3 = Ra, 2:0 = Rb. `LDI` replaces Ra and Rb with the 6-bit immediate. Fields an
instruction does not use are encoded as 000. `LDW` is different: it has no opcode field, and the
word is the value itself (`k` sign-extended from 15 bits: top bits `00` for k >= 0, `11` for k < 0),
which loads R7. Example encodings are the ones printed in the Instruction Summary.

| Syntax | Opcode | Example | Encoding |
|---|---|---|---|
| `SLT Rd, Ra, Rb` | 00000 | `SLT R3, R1, R2` | `40CA` |
| `ADD Rd, Ra, Rb` | 00001 | `ADD R3, R1, R2` | `42CA` |
| `SUB Rd, Ra, Rb` | 00010 | `SUB R3, R1, R2` | `44CA` |
| `AND Rd, Ra, Rb` | 00011 | `AND R3, R1, R2` | `46CA` |
| `OR  Rd, Ra, Rb` | 00100 | `OR R3, R1, R2` | `48CA` |
| `XOR Rd, Ra, Rb` | 00101 | `XOR R3, R1, R2` | `4ACA` |
| `SLL Rd, Ra` | 00110 | `SLL R2, R1` | `4C88` |
| `SRL Rd, Ra` | 00111 | `SRL R2, R1` | `4E88` |
| `IN Rd` | 01000 | `IN R1` | `5040` |
| `LDI Rd, imm` | 01001 | `LDI R2, -5` | `52BB` |
| `J Rb` | 01010 | `J R2` | `5402` |
| `JZ Ra, Rb` | 01010 | `JZ R1, R2` | `540A` |
| `JAL Rd, Ra, Rb` | 01010 | `JAL R4, R0, R2` | `5502` |
| `JR Ra` | 01011 | `JR R4` | `5620` |
| `OUT Rb` | 01100 | `OUT R3` | `5803` |
| `LD Rd, Ra` | 01101 | `LD R5, R2` | `5B50` |
| `ST Ra, Rb` | 01110 | `ST R2, R4` | `5C14` |
| `RESET` | 11111 | `RESET` | `7E00` |
| `LDW k` | none (types 00 / 11) | `LDW 0x0300` | `0300` |
| `LDW k` | none | `LDW -1` | `FFFF` |

`J`, `JZ` and `JAL` share opcode 01010 and are told apart by their register
fields: `J Rb` is `JZ R0, Rb` (R0 is always 0, so it always jumps).

## The ISA table (`isa.csv`)

The assembler does not hard-code instructions. At startup it loads `isa.csv`:

```
mnemonic,opcode,operands,description
ADD,00001,dab,Add
LDI,01001,di,Load immediate (imm6 sign-extended)
ST,01110,ab,Store RAM[Ra] <- Rb
RESET,11111,,Reset PC
```

`operands` lists, in assembly order, which instruction field each operand fills:
`d` = Rd, `a` = Ra, `b` = Rb, `i` = 6-bit immediate. Fields not listed stay 000. A whole-word
instruction such as `LDW` is written with opcode `-` and operand code `k`:
`LDW,-,k,Load word into R7`. The same
column drives operand-count checking and encoding, so changing or adding an
instruction is a one-line edit with no recompile. Lines starting with `#` are comments.

## Output formats

- **`v3` (default).** The text format Logisim saves for this machine's ROMs: a
  `v3.0 hex words addressed` header, then lines of `AAAA:` followed by up to eight
  4-digit lowercase hex words, contiguous from address 0 to the last word written.
  Words after the end of the program are not written, matching the saved ROM files.
- **`bin`.** The whole 64K-word address space as 131072 bytes, big-endian.

## How it works

Two passes over the source, in `sweet16asm.c`:

1. **Pass 1** tokenizes each line (strip the comment, split off an optional label,
   split operands on commas), assigns an address to every statement, and records
   labels. Pseudo-ops that expand to two words are sized here, so forward
   references resolve correctly.
2. **Pass 2** encodes each statement using the ISA table and writes words into a
   64K-word image. It checks register names, immediate ranges, operand counts,
   undefined and duplicate labels, and overlapping addresses.

## Verification

`make test` runs `test.sh`, which checks:

- **The ISA summary's own examples.** All 20 example encodings printed in the
  Instruction Summary (page 3, revision 2026-10-14, including both `LDW` rows) are
  reproduced exactly (`tests/pdf_examples.*`).
- **A real ROM image.** `tests/program_rom_matrix_marshall.expected.hex` is a
  753-word program image saved from the Logisim machine. Its words were
  disassembled to `tests/matrix_marshall.asm`, and reassembling that source
  reproduces the saved file **byte for byte**. All 753 words decode as valid
  Sweet16 instructions.
- **Every instruction form,** with expected words computed independently from the
  bit-field table (`tests/vectors.*`), plus `LDW` limits, the pseudo-ops and the `bin` format.
- **Error handling:** bad register, immediate out of range (`LDI` and `LDW`), wrong operand
  count, unknown mnemonic, duplicate or undefined label, jump too far, `.org` backwards,
  stray comma, malformed ISA table row.
- **Syntax handling:** comments, Windows line endings, case-insensitivity, label placement.

## Limitations

- The automated tests check the encoder and the file format. They do not run the Logisim circuit.
  As a manual check, `examples/smoke_test.asm` was assembled, loaded into the program ROM and run on
  the machine, and the observed results matched the expected values listed in that file.
- `matrix_marshall.asm` is disassembler output (no labels or comments). It proves the
  encoder against a real image, not that any particular source text would have produced it.
- Unused opcodes (15 to 30 in the summary) and the unused type `10` are not assembler
  instructions. They can be emitted with `.word`.
- `LDW` always overwrites R7. Per the summary, every word whose top bits are `00` or `11` is an
  `LDW`, so a zero word in unused ROM executes as `LDW 0` rather than doing nothing. Ending a
  program with a deliberate halt (for example a jump to itself) is safer than running off the end.
- `JMP`/`JZL` reach -32 to 31 words (the `LDI` range); `LJMP`/`LJZ` reach -16384 to 16383 words
  but use R7.
- Interrupts (the IRQ path) are not built in the machine, so there is no interrupt syntax.

## Repository layout

```
sweet16asm.c   the assembler (single file, C99)
isa.csv        instruction table loaded at startup
Makefile       make / make test / make clean
test.sh        regression tests
examples/      program2.asm (explicit offsets), jumps.asm (labels and pseudo-ops),
               smoke_test.asm (short program to confirm an assembled image runs in Logisim)
tests/         fixtures: PDF examples, vectors, and the matrix_marshall ROM round trip
```

Build requirements: any C99 compiler (`cc`). Nothing else.
