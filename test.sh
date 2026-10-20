#!/bin/sh
# Regression tests for sweet16asm. Run from this directory after `make` (or just `make test`).
cd "$(dirname "$0")" || exit 1
for tool in cmp diff cut sed grep wc mktemp; do
  command -v $tool >/dev/null 2>&1 || { echo "test.sh needs '$tool'. In MSYS2 run: pacman -S diffutils coreutils sed grep"; exit 1; }
done
for f in tests/pdf_examples.asm tests/pdf_examples.expected tests/vectors.asm tests/vectors.expected \
         tests/matrix_marshall.asm tests/program_rom_matrix_marshall.expected.hex tests/program2.expected.bin \
         examples/program2.asm isa.csv; do
  [ -f "$f" ] || { echo "test.sh: missing file '$f' (copy the whole repository folder, including tests/)"; exit 1; }
done
ASM=./sweet16asm; [ -f ./sweet16asm.exe ] && ASM=./sweet16asm.exe; fail=0; T=$(mktemp -d)
ok()  { echo "PASS  $1"; }
bad() { echo "FAIL  $1"; fail=1; }

# 1. The 20 example encodings printed in the Instruction Summary (page 3, revision 2026-10-14)
$ASM -l $T/p.lst -o $T/p.hex tests/pdf_examples.asm >/dev/null || bad "pdf examples: assemble"
cut -c1-10 $T/p.lst | diff -q - tests/pdf_examples.expected >/dev/null && ok "PDF example encodings (20 rows, incl. LDW)" || bad "PDF example encodings differ"

# 2. Every instruction form, expected words computed independently from the bit-field table
$ASM -l $T/v.lst -o $T/v.hex tests/vectors.asm >/dev/null || bad "vectors: assemble"
cut -c1-10 $T/v.lst | diff -q - tests/vectors.expected >/dev/null && ok "all instruction forms + pseudo-ops" || bad "vector encodings differ"

# 3. Real ROM image from the Logisim machine: 753 words, must reproduce the saved file byte for byte
$ASM -o $T/mm.hex tests/matrix_marshall.asm >/dev/null && cmp -s $T/mm.hex tests/program_rom_matrix_marshall.expected.hex \
  && ok "matrix_marshall ROM reproduced byte for byte (v3.0 text)" || bad "matrix_marshall ROM differs"

# 4. Earlier raw-binary image
$ASM -f bin -o $T/p2.bin examples/program2.asm >/dev/null && cmp -s $T/p2.bin tests/program2.expected.bin \
  && ok "program2 == raw 64K image (131072 bytes)" || bad "program2 raw image differs"

# 5. v3 layout: header, 4-digit address, 8 words per line, lowercase hex, partial last line
$ASM -o $T/p2.hex examples/program2.asm >/dev/null
[ "$(sed -n 1p $T/p2.hex)" = "v3.0 hex words addressed" ] && [ "$(sed -n 2p $T/p2.hex)" = "0000: 5281 50c0 5240 410b 537c 5425 424a 53bb" ] \
  && [ "$(sed -n 3p $T/p2.hex)" = "0008: 5406" ] && [ "$(wc -l < $T/p2.hex)" -eq 3 ] && ok "v3.0 file layout" || bad "v3.0 file layout"

# 6. Error cases must fail and name the problem
expect_err() { # name, source text, grep pattern
  printf '%b' "$2" > $T/e.asm
  if $ASM -o $T/e.hex $T/e.asm 2>$T/e.err >/dev/null; then bad "$1 (should fail)"
  elif grep -q "$3" $T/e.err; then ok "$1"; else bad "$1 (wrong message)"; fi; }
expect_err "bad register"      'ADD r1, r2, r8\n'        'not a register'
expect_err "imm too big"       'LDI r1, 32\n'            'out of range'
expect_err "imm too small"     'LDI r1, -33\n'           'out of range'
expect_err "operand count"     'ADD r1, r2\n'            'takes 3'
expect_err "unknown mnemonic"  'FOO r1\n'                'unknown instruction'
expect_err "duplicate label"   'a: NOP\na: NOP\n'        'already defined'
expect_err "undefined label"   'JMP r1, nowhere\n'       'undefined label'
expect_err "scratch r0"        'x: JMP r0, x\n'          'r1-r7'
expect_err "trailing comma"    'ADD r1, r2,\n'           'empty operand'
expect_err "jump out of range" 'a: NOP\n.org 100\nJMP r1, a\n' 'words away'
expect_err "LDW too big"       'LDW 16384\n'           'out of range'
expect_err "LDW too small"     'LDW -16385\n'          'out of range'
expect_err "LDW label too far" '.org 20000\ny: NOP\nLDW y\n' 'does not fit'
expect_err "JMP hints LJMP"    'a: NOP\n.org 100\nJMP r1, a\n' 'LJMP'
expect_err "LJMP out of range" 'LJMP a\n.org 20000\na: NOP\n' 'words away'
expect_err "org backwards"     'NOP\nNOP\n.org 1\n'      'backwards'

# 7. Syntax: comments, CRLF, label on its own line, no-space comma form, case-insensitivity
printf 'top:\r\n  add R1,r2,R3 ; c, with, commas\r\n; only a comment\r\n\r\nADD,r1,r2,r3\r\n' > $T/s.asm
$ASM -l $T/s.lst -o $T/s.hex $T/s.asm >/dev/null && [ "$(sed -n 1p $T/s.lst | cut -c7-10)" = "4253" ] && [ "$(sed -n 2p $T/s.lst | cut -c7-10)" = "4253" ] && [ "$(wc -l < $T/s.lst)" -eq 2 ] \
  && ok "comments / CRLF / case / comma forms" || bad "syntax handling"

# 8. Backward jump label resolves relative to the J's own address (J at 0002, label at 0000 -> -2)
printf 'a: NOP\nJMP r1, a\n' > $T/j.asm
$ASM -l $T/j.lst -o $T/j.hex $T/j.asm >/dev/null && [ "$(sed -n 2p $T/j.lst | cut -c7-10)" = "527E" ] && [ "$(sed -n 3p $T/j.lst | cut -c7-10)" = "5401" ] \
  && ok "JMP offset (LDI r1,-2 ; J r1)" || bad "JMP offset"

# 9. LDW with a label operand, and the long jumps (LDW off ; J r7 / JZ ra,r7). off = label - address of the jump
printf '.org 0x100\nx: NOP\nLDW x\n' > $T/l.asm
$ASM -l $T/l.lst -o $T/l.hex $T/l.asm >/dev/null && [ "$(sed -n 2p $T/l.lst | cut -c1-10)" = "0101: 0100" ] && ok "LDW label operand" || bad "LDW label operand"
printf 'a: NOP\n.org 40\nLJMP a\n' > $T/lj.asm   # LDW at 0x28, J at 0x29: off = 0-41 = -41 = 0xFFD7
$ASM -l $T/lj.lst -o $T/lj.hex $T/lj.asm >/dev/null && [ "$(sed -n 2p $T/lj.lst | cut -c1-10)" = "0028: FFD7" ] && [ "$(sed -n 3p $T/lj.lst | cut -c1-10)" = "0029: 5407" ] \
  && ok "LJMP (LDW off ; J r7)" || bad "LJMP"
printf 'a: NOP\nLJZ r2, a\n' > $T/lz.asm        # LDW at 1, JZ at 2: off = 0-2 = -2 = 0xFFFE
$ASM -l $T/lz.lst -o $T/lz.hex $T/lz.asm >/dev/null && [ "$(sed -n 2p $T/lz.lst | cut -c1-10)" = "0001: FFFE" ] && [ "$(sed -n 3p $T/lz.lst | cut -c1-10)" = "0002: 5417" ] \
  && ok "LJZ (LDW off ; JZ ra,r7)" || bad "LJZ"

# 10. ISA table rule: opcode '-' (whole-word) must use operand code 'k'
printf 'mnemonic,opcode,operands\nFOO,-,dab\n' > $T/bad.csv; printf 'NOP\n' > $T/n.asm
if $ASM -i $T/bad.csv -o $T/n.hex $T/n.asm 2>$T/c.err >/dev/null; then bad "bad ISA row accepted"
elif grep -q "needs operand code" $T/c.err; then ok "ISA table validation"; else bad "ISA table message"; fi

rm -rf $T
[ $fail -eq 0 ] && echo "ALL TESTS PASSED" || { echo "SOME TESTS FAILED"; exit 1; }
