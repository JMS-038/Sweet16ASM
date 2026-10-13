#!/bin/sh
# Regression tests for sweet16asm. Run from this directory after `make` (or just `make test`).
cd "$(dirname "$0")" || exit 1
ASM=./sweet16asm; fail=0; T=$(mktemp -d)
ok()  { echo "PASS  $1"; }
bad() { echo "FAIL  $1"; fail=1; }

# 1. The 18 example encodings printed in the instruction-summary PDF (page 2)
$ASM -l $T/p.lst -o $T/p.hex tests/pdf_examples.asm >/dev/null || bad "pdf examples: assemble"
cut -c1-10 $T/p.lst | diff -q - tests/pdf_examples.expected >/dev/null && ok "PDF example encodings (18 rows)" || bad "PDF example encodings differ"

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
expect_err "org backwards"     'NOP\nNOP\n.org 1\n'      'backwards'

# 7. Syntax: comments, CRLF, label on its own line, no-space comma form, case-insensitivity
printf 'top:\r\n  add R1,r2,R3 ; c, with, commas\r\n; only a comment\r\n\r\nADD,r1,r2,r3\r\n' > $T/s.asm
$ASM -l $T/s.lst -o $T/s.hex $T/s.asm >/dev/null && [ "$(cut -c7-10 $T/s.lst | sort -u)" = "4253" ] && [ "$(wc -l < $T/s.lst)" -eq 2 ] \
  && ok "comments / CRLF / case / comma forms" || bad "syntax handling"

# 8. Backward jump label resolves relative to the J's own address (J at 0002, label at 0000 -> -2)
printf 'a: NOP\nJMP r1, a\n' > $T/j.asm
$ASM -l $T/j.lst -o $T/j.hex $T/j.asm >/dev/null && [ "$(sed -n 2p $T/j.lst | cut -c7-10)" = "527E" ] && [ "$(sed -n 3p $T/j.lst | cut -c7-10)" = "5401" ] \
  && ok "JMP offset (LDI r1,-2 ; J r1)" || bad "JMP offset"

rm -rf $T
[ $fail -eq 0 ] && echo "ALL TESTS PASSED" || { echo "SOME TESTS FAILED"; exit 1; }
