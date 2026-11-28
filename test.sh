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
expect_err "LDI huge literal"   'LDI r1, 99999999999999999999\n'  'immediate 99999999999999999999 out of range'
expect_err "LDI huge negative"  'LDI r1, -99999999999999999999\n' 'immediate -99999999999999999999 out of range'
expect_err "LDI hex too large"  'LDI r1, 0xFFFFFFFFFFFFFFFFF\n'   'immediate 0xFFFFFFFFFFFFFFFFF out of range'
expect_err "LDI above 2^31"     'LDI r1, 5000000000\n'            'immediate 5000000000 out of range'
expect_err "LDW huge literal"   'LDW 99999999999999999999\n'      'LDW value 99999999999999999999 out of range'
expect_err ".word huge literal" '.word 99999999999999999999\n'    'out of 16-bit range'
expect_err "LDI not a number"   'LDI r1, 12abc\n'                 "'12abc' is not a number"
expect_err "LDW label too far" '.org 20000\ny: NOP\nLDW y\n' 'does not fit'
expect_err "JMP hints LJMP"    'a: NOP\n.org 100\nJMP r1, a\n' 'LJMP'
expect_err "LJMP out of range" 'LJMP a\n.org 20000\na: NOP\n' 'words away'
expect_err "JZL scratch is condition" 'a: NOP\nJZL r4, r4, a\n' 'also the condition register'
expect_err "LJZ r7 is condition"      'a: NOP\nLJZ r7, a\n'     'cannot be the condition register'
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
printf 'a: NOP\nJZL r4, r5, a\n' > $T/zl.asm        # different scratch register: LDI r5,-2 ; JZ r4,r5
$ASM -l $T/zl.lst -o $T/zl.hex $T/zl.asm >/dev/null && [ "$(sed -n 2p $T/zl.lst | cut -c1-10)" = "0001: 537E" ] && [ "$(sed -n 3p $T/zl.lst | cut -c1-10)" = "0002: 5425" ] \
  && ok "JZL (LDI rt,off ; JZ ra,rt)" || bad "JZL"
printf 'a: NOP\nJZL r0, r5, a\nLJZ r0, a\n' > $T/z0.asm   # r0 as the condition register is not a clash
$ASM -l $T/z0.lst -o $T/z0.hex $T/z0.asm >/dev/null && [ "$(sed -n 3p $T/z0.lst | cut -c1-10)" = "0002: 5405" ] && [ "$(sed -n 5p $T/z0.lst | cut -c1-10)" = "0004: 5407" ] \
  && ok "JZL/LJZ with r0 as the condition" || bad "JZL/LJZ r0 condition"
# 9d. Numbers at the edge of every field, in every notation, still assemble (guards the number parser)
printf 'LDI r1, 31\nLDI r2, -32\nLDI r3, 0x1F\nLDI r4, 0b11111\nLDW 16383\nLDW -16384\n.word 65535\n.word -32768\n.word 0x7FFF\n.org 0x20\nNOP\n' > $T/nb.asm
$ASM -l $T/nb.lst -o $T/nb.hex $T/nb.asm >/dev/null && [ "$(cut -c7-10 $T/nb.lst | tr '\n' ' ')" = "525F 52A0 52DF 531F 3FFF C000 FFFF 8000 7FFF 4200 " ] \
  && ok "numbers at the field limits" || bad "numbers at the field limits"

# 10. ISA table rule: opcode '-' (whole-word) must use operand code 'k'
printf 'mnemonic,opcode,operands\nFOO,-,dab\n' > $T/bad.csv; printf 'NOP\n' > $T/n.asm
if $ASM -i $T/bad.csv -o $T/n.hex $T/n.asm 2>$T/c.err >/dev/null; then bad "bad ISA row accepted"
elif grep -q "needs operand code" $T/c.err; then ok "ISA table validation"; else bad "ISA table message"; fi

# 11. Output handling: never overwrite an input, never leave a listing behind after a failed run
printf 'NOP\n' > $T/c.asm; cp $T/c.asm $T/c.orig
expect_refuse() { # args: the assembler arguments; $NAME is the test name. The source c.asm must stay unchanged.
  if $ASM "$@" >/dev/null 2>$T/r.err; then bad "$NAME (should refuse)"
  elif grep -q "refusing\|both" $T/r.err && cmp -s $T/c.asm $T/c.orig; then ok "$NAME"; else bad "$NAME (wrong message or source changed)"; fi; }
NAME="-o is the input";             expect_refuse -o $T/c.asm $T/c.asm
NAME="-o is the input (./ form)";   expect_refuse -o $T/./c.asm $T/c.asm
NAME="-l is the input";             expect_refuse -l $T/c.asm -o $T/o.hex $T/c.asm
NAME="-l is the input (./ form)";   expect_refuse -l $T/./c.asm -o $T/o.hex $T/c.asm
NAME="-l and -o are the same file"; expect_refuse -l $T/o.hex -o $T/./o.hex $T/c.asm
NAME="-l and -o (.// form)";        expect_refuse -l $T/o.hex -o $T/.//o.hex $T/c.asm
cp isa.csv $T/isa.orig; $ASM -i $T/isa.orig -o $T/isa.orig $T/c.asm >/dev/null 2>&1
cmp -s isa.csv $T/isa.orig && ok "-o is the ISA table" || bad "-o is the ISA table"
cp $T/c.asm $T/dflt.hex; cp $T/c.asm $T/dflt.orig       # default output name (input.hex) equals the input
if $ASM $T/dflt.hex >/dev/null 2>&1; then bad "default output == input (should refuse)"
elif cmp -s $T/dflt.hex $T/dflt.orig; then ok "default output == input refused"; else bad "default output == input: source changed"; fi
if ln -s c.asm $T/link.asm 2>/dev/null && [ -L $T/link.asm ]; then   # symlinks need privileges on Windows: skip there
  NAME="-o is a symlink to the input"; expect_refuse -o $T/link.asm $T/c.asm
fi
printf 'NOP\nBOGUS r1\n' > $T/f.asm; rm -f $T/f.lst
$ASM -l $T/f.lst -o $T/f.hex $T/f.asm >/dev/null 2>&1; [ ! -e $T/f.lst ] && [ ! -e $T/f.hex ] && ok "failed assembly leaves no listing" || bad "failed assembly left a listing or output"
printf 'NOP\n' > $T/g.asm
$ASM -l $T/g.lst -o $T/g.hex $T/g.asm >/dev/null 2>&1 && [ "$(cat $T/g.lst)" = "0000: 4200  NOP" ] && ok "listing written on success" || bad "listing on success"

# 12. Disassembler (-d): every word round-trips, decoding follows the table, bad images are refused
# 12a. Hand-computed words, one per decoding rule (fields: 01-ooooo-ddd-aaa-bbb, expected text worked out from the bit layout)
{ echo "v3.0 hex words addressed"
  echo "0000: 4253 4200 41f5 4c98 4c99 5100 5108 52ff"
  echo "0008: 521f 5220 5405 5415 5455 5445 5400 5630"
  echo "0010: 5803 5a50 5c13 7e00 7e01 5e00 7c00 8000"
  echo "0018: bfff 0000 3fff c000 ffff"; } > $T/dv.hex
cat > $T/dv.expected <<'EOF'
        ADD r1, r2, r3
        ADD r0, r0, r0
        SLT r7, r6, r5
        SLL r2, r3
        .word 0x4C99
        IN r4
        .word 0x5108
        LDI r3, -1
        LDI r0, 31
        LDI r0, -32
        J r5
        JZ r2, r5
        JAL r1, r2, r5
        JAL r1, r0, r5
        J r0
        JR r6
        OUT r3
        LD r1, r2
        ST r2, r3
        RESET
        .word 0x7E01
        .word 0x5E00
        .word 0x7C00
        .word 0x8000
        .word 0xBFFF
        LDW 0
        LDW 16383
        LDW -16384
        LDW -1
EOF
$ASM -d $T/dv.hex -o $T/dv.asm >/dev/null 2>&1 && cmp -s $T/dv.asm $T/dv.expected && ok "disassembler: one word per decoding rule (canonical J/JZ/JAL, .word, LDW limits)" || bad "disassembler: decoding rules"
$ASM -o $T/dv2.hex $T/dv.asm >/dev/null 2>&1 && cmp -s $T/dv2.hex $T/dv.hex && ok "disassembler: those words reassemble to the same image" || bad "disassembler: decoding-rule words do not reassemble"
# 12a2. Windows line endings, tabs, upper-case hex and short words are accepted on input
printf 'v3.0 hex words addressed\r\n0000:\t5400   5C1B\r\n\r\n0002: 4\r\n' > $T/crlf.hex
printf '        J r0\n        ST r3, r3\n        LDW 4\n' > $T/crlf.expected
$ASM -d $T/crlf.hex 2>/dev/null | cmp -s - $T/crlf.expected && ok "disassembler: CRLF, tabs, upper-case hex, short words" || bad "disassembler: lenient v3 input"
# 12b. Rows that do not run on from each other give a .org
printf 'v3.0 hex words addressed\n0000: 5400\n0010: 5400\n' > $T/gap.hex
printf '        J r0\n        .org 0x0010\n        J r0\n' > $T/gap.expected
$ASM -d $T/gap.hex -o $T/gap.asm >/dev/null 2>&1 && cmp -s $T/gap.asm $T/gap.expected && ok "disassembler: gap in a v3 file -> .org" || bad "disassembler: .org for a gap"
# 12c. The real Logisim image reassembles to the same file; the text is the fixture apart from its header comment
$ASM -d tests/program_rom_matrix_marshall.expected.hex -o $T/mmd.asm >/dev/null 2>&1 && $ASM -o $T/mmd.hex $T/mmd.asm >/dev/null 2>&1 \
  && cmp -s $T/mmd.hex tests/program_rom_matrix_marshall.expected.hex && ok "disassembler: matrix_marshall image reassembles byte for byte" || bad "disassembler: matrix_marshall round trip"
sed 1d tests/matrix_marshall.asm | cmp -s - $T/mmd.asm && ok "disassembler: text equals tests/matrix_marshall.asm (minus its header comment)" || bad "disassembler: text differs from tests/matrix_marshall.asm"
# 12d. Every one of the 65536 words: image word n = n, disassemble, reassemble, compare (the file layout is the one WriteV3Image writes)
{ echo "v3.0 hex words addressed"
  for a in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do for b in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do for c in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do
    echo "$a$b${c}0: $a$b${c}0 $a$b${c}1 $a$b${c}2 $a$b${c}3 $a$b${c}4 $a$b${c}5 $a$b${c}6 $a$b${c}7"
    echo "$a$b${c}8: $a$b${c}8 $a$b${c}9 $a$b${c}a $a$b${c}b $a$b${c}c $a$b${c}d $a$b${c}e $a$b${c}f"
  done; done; done; } > $T/all.hex
$ASM -d $T/all.hex -o $T/all.asm >/dev/null 2>&1 && $ASM -o $T/all2.hex $T/all.asm >/dev/null 2>&1 && cmp -s $T/all2.hex $T/all.hex \
  && ok "disassembler: all 65536 words round-trip" || bad "disassembler: exhaustive round trip"
# 12e. The raw reader: same words, same text, whether the format is given or detected; trailing zeros are dropped
$ASM -f bin -o $T/all.bin $T/all.asm >/dev/null 2>&1
$ASM -d $T/all.bin -o $T/allb.asm >/dev/null 2>&1 && cmp -s $T/allb.asm $T/all.asm && ok "disassembler: raw image (format detected) == v3 text" || bad "disassembler: raw image differs from v3"
$ASM -d -f bin $T/all.bin -o $T/allb2.asm >/dev/null 2>&1 && cmp -s $T/allb2.asm $T/all.asm && ok "disassembler: raw image (-f bin)" || bad "disassembler: -f bin"
$ASM -d $T/p2.bin -o $T/p2d.asm >/dev/null 2>&1 && $ASM -o $T/p2d.hex $T/p2d.asm >/dev/null 2>&1 && cmp -s $T/p2d.hex $T/p2.hex \
  && ok "disassembler: raw image of a short program, zero padding dropped" || bad "disassembler: short raw image"
: > $T/empty.asm; $ASM -f bin -o $T/zero.bin $T/empty.asm >/dev/null 2>&1; $ASM -d $T/zero.bin 2>/dev/null | cmp -s - /dev/null && ok "disassembler: all-zero raw image -> no statements" || bad "disassembler: all-zero raw image"
# 12f. Standard output holds only the assembly; the summary goes to standard error
$ASM -d $T/dv.hex 2>$T/so.err | cmp -s - $T/dv.expected && grep -q "word(s)" $T/so.err && ok "disassembler: standard output is assembly only" || bad "disassembler: standard output"
# 12g. The ISA table drives decoding: rename ADD and the disassembler follows
sed 's/^ADD,/PLUS,/' isa.csv > $T/isa2.csv
printf 'v3.0 hex words addressed\n0000: 4253\n' > $T/one.hex
[ "$($ASM -i $T/isa2.csv -d $T/one.hex 2>/dev/null)" = "        PLUS r1, r2, r3" ] && ok "disassembler: decoding follows the ISA table" || bad "disassembler: ISA table not followed"
# 12h. Bad input is refused with a message, and creates nothing
expect_dis_err() { # name, image file, grep pattern for the message, optional extra option (e.g. -f v3)
  rm -f $T/de.asm
  if $ASM -d $4 $2 -o $T/de.asm >/dev/null 2>$T/de.err; then bad "$1 (should fail)"
  elif grep -q "$3" $T/de.err && [ ! -e $T/de.asm ]; then ok "$1"; else bad "$1 (wrong message or output created)"; fi; }
printf 'hello\n' > $T/b1.hex;                                                    expect_dis_err "disassembler: no v3 header" $T/b1.hex "not a v3 image" "-f v3"
: > $T/b2.hex;                                                                   expect_dis_err "disassembler: empty file" $T/b2.hex "not a v3 image" "-f v3"
expect_dis_err "disassembler: damaged header is read as raw, with a hint to override" $T/b1.hex "guessed from its first bytes"
printf 'v3.0 hex words addressed\n0000: 5400 zz12\n' > $T/b3.hex;               expect_dis_err "disassembler: bad word" $T/b3.hex "bad word 'zz12'"
printf 'v3.0 hex words addressed\n0000: 12345\n' > $T/b4.hex;                    expect_dis_err "disassembler: word longer than 4 digits" $T/b4.hex "bad word '12345'"
printf 'v3.0 hex words addressed\n0000: 5400\n0000: 5400\n' > $T/b5.hex;        expect_dis_err "disassembler: address given twice" $T/b5.hex "given twice"
printf 'v3.0 hex words addressed\nfffe: 5400 5400 5400\n' > $T/b6.hex;          expect_dis_err "disassembler: row runs past 0xFFFF" $T/b6.hex "beyond the 64K"
printf 'v3.0 hex words addressed\n10000: 5400\n' > $T/b7.hex;                   expect_dis_err "disassembler: address too wide" $T/b7.hex "bad address"
printf 'v3.0 hex words addressed\n0000: 3*5400\n' > $T/b8.hex;                  expect_dis_err "disassembler: count*word repeat is not guessed at" $T/b8.hex "bad word '3\*5400'"
printf 'abc' > $T/short.bin;                                                     expect_dis_err "disassembler: raw image too short" $T/short.bin "131072 bytes"
{ cat $T/all.bin; printf 'x'; } > $T/long.bin;                                   expect_dis_err "disassembler: raw image too long" $T/long.bin "131072 bytes"
expect_dis_err "disassembler: image file missing" $T/nonexistent.hex "cannot open"
# 12i. Option checks: -d takes no listing; the output may not be the image or the ISA table
$ASM -d -l $T/x.lst $T/dv.hex >/dev/null 2>&1; [ $? -eq 2 ] && [ ! -e $T/x.lst ] && ok "disassembler: -l rejected with -d (exit 2)" || bad "disassembler: -l with -d"
cp $T/dv.hex $T/dv.keep; $ASM -d -o $T/dv.hex $T/dv.hex >/dev/null 2>$T/same.err; [ $? -eq 2 ] && grep -q refusing $T/same.err && cmp -s $T/dv.hex $T/dv.keep \
  && ok "disassembler: -o is the image -> refused, image untouched" || bad "disassembler: -o is the image"
cp isa.csv $T/isa3.csv; $ASM -d -i $T/isa3.csv -o $T/isa3.csv $T/dv.hex >/dev/null 2>&1; [ $? -eq 2 ] && cmp -s isa.csv $T/isa3.csv && ok "disassembler: -o is the ISA table -> refused" || bad "disassembler: -o is the ISA table"

rm -rf $T
[ $fail -eq 0 ] && echo "ALL TESTS PASSED" || { echo "SOME TESTS FAILED"; exit 1; }
