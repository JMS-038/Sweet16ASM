; Auto-disassembled from program_rom_matrix_marshall.bin (no labels or comments). Used as a round-trip fixture.
        LDI r2, 1
        LDI r1, 16
        ADD r1, r1, r1
        ADD r1, r1, r1
        ADD r1, r1, r1
        ADD r1, r1, r1
        SUB r1, r1, r2
        LDI r6, 3
        LDI r7, -3
        ST r1, r0
        JZ r1, r6
        SUB r1, r1, r2
        J r7
        LDI r7, 1
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        LDI r4, 6
        ADD r7, r7, r4
        J r7
        LDI r2, 1
        LDI r3, 24
        ADD r3, r3, r3
        ADD r3, r3, r3
        ADD r3, r3, r3
        LDI r1, 31
        ADD r1, r1, r1
        ADD r1, r1, r2
        LDI r6, 3
        LDI r7, -5
        LD r4, r1
        ADD r5, r1, r3
        ST r5, r4
        JZ r1, r6
        SUB r1, r1, r2
        J r7
        LDI r5, 0
        LDI r7, 2
        LDI r1, 31
        ADD r1, r1, r1
        ADD r1, r1, r2
        LDI r3, 3
        LDI r6, -6
        ADD r4, r1, r5
        LD r4, r4
        OR r4, r4, r1
        OUT r4
        JZ r1, r3
        SUB r1, r1, r2
        J r6
        LDI r1, 31
        ADD r1, r1, r1
        ADD r1, r1, r2
        SUB r7, r7, r2
        LD r4, r0
        OUT r0
        LDI r4, 13
        JZ r7, r4
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        ADD r4, r4, r0
        LDI r4, 16
        J r4
        LDI r7, 2
        ADD r5, r5, r2
        LDI r3, 24
        ADD r3, r3, r3
        ADD r3, r3, r3
        ADD r3, r3, r3
        SUB r3, r5, r3
        LDI r4, 4
        JZ r3, r4
        ADD r4, r4, r0
        LDI r4, 4
        J r4
        LDI r5, 0
        LDI r4, 1
        J r4
        LDI r4, -24
        ADD r4, r4, r4
        ADD r4, r4, r2
        J r4
        LDI r3, 1
        LDI r1, 0
        LDI r5, -32
        ADD r5, r5, r5
        ADD r5, r5, r5
        SUB r5, r5, r3
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        LDI r6, -32
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        SUB r6, r6, r3
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        LDI r7, 24
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ST r1, r7
        ADD r1, r1, r3
        ADD r5, r7, r7
        ST r1, r5
        ADD r1, r1, r3
        LDI r2, 30
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        LDI r2, -32
        ADD r2, r2, r2
        ADD r2, r2, r2
        SUB r2, r2, r3
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        LDI r4, 2
        ADD r1, r1, r4
        OR r7, r5, r7
        ST r1, r7
        ADD r1, r1, r3
        LDI r5, 31
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        LDI r6, 21
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        LDI r2, 29
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        LDI r2, 26
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        LDI r4, 4
        ADD r1, r1, r4
        LDI r6, 31
        LDI r4, -32
        SUB r6, r6, r4
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        LDI r7, 31
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        LDI r4, -24
        SUB r7, r7, r4
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ST r1, r7
        ADD r1, r1, r3
        LDI r5, 19
        LDI r4, -32
        SUB r5, r5, r4
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        ADD r2, r7, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r6
        LDI r4, 2
        ADD r1, r1, r4
        LDI r5, 31
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        LDI r6, 16
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        LDI r7, 24
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ST r1, r7
        LDI r4, 2
        ADD r1, r1, r4
        LDI r2, 28
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        LDI r6, 21
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        OR r2, r6, r7
        ST r1, r2
        ADD r1, r1, r3
        LDI r2, 26
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        LDI r4, 4
        ADD r1, r1, r4
        LDI r5, -32
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        SUB r5, r5, r3
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        LDI r6, 24
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        ADD r5, r7, r7
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        LDI r5, -32
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        SUB r5, r5, r3
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        LDI r4, 2
        ADD r1, r1, r4
        LDI r6, 18
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        LDI r5, 23
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        LDI r6, 21
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        OR r5, r5, r7
        ST r1, r5
        ADD r1, r1, r3
        LDI r2, 30
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        LDI r4, 2
        ADD r1, r1, r4
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        LDI r2, 16
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r7
        LDI r4, 2
        ADD r1, r1, r4
        LDI r2, 18
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        OR r2, r6, r7
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        LDI r7, 23
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ST r1, r7
        ADD r1, r1, r3
        LDI r6, 18
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        LDI r4, 2
        ADD r1, r1, r4
        LDI r7, -32
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        SUB r7, r7, r3
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        LDI r2, 16
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        SUB r7, r5, r2
        ST r1, r7
        LDI r4, 2
        ADD r1, r1, r4
        ST r1, r6
        ADD r1, r1, r3
        LDI r2, 23
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        LDI r2, 21
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r7
        LDI r4, 2
        ADD r1, r1, r4
        LDI r5, -32
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        SUB r5, r5, r3
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ADD r5, r5, r5
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        LDI r4, 2
        ADD r1, r1, r4
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        LDI r4, 4
        ADD r1, r1, r4
        LDI r6, 24
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        ST r1, r6
        LDI r4, 4
        ADD r1, r1, r4
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        LDI r7, 27
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        LDI r4, -24
        SUB r7, r7, r4
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        LDI r6, 24
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        LDI r4, -24
        SUB r6, r6, r4
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        LDI r4, 2
        ADD r1, r1, r4
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r6
        LDI r4, 2
        ADD r1, r1, r4
        LDI r2, 31
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        LDI r4, -32
        SUB r2, r2, r4
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        LDI r6, 29
        ADD r6, r6, r6
        ADD r6, r6, r6
        LDI r4, -29
        SUB r6, r6, r4
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        ST r1, r6
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        SUB r2, r5, r6
        ST r1, r2
        LDI r4, 2
        ADD r1, r1, r4
        LDI r2, 24
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        LDI r2, 28
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        LDI r2, 26
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        LDI r2, 25
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        LDI r2, 16
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ADD r2, r2, r2
        ST r1, r2
        LDI r4, 2
        ADD r1, r1, r4
        LDI r6, 31
        LDI r4, -32
        SUB r6, r6, r4
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ADD r6, r6, r6
        ST r1, r6
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        SUB r7, r5, r6
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r7
        ADD r1, r1, r3
        ST r1, r5
        ADD r1, r1, r3
        ST r1, r6
        LDI r7, -11
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        ADD r7, r7, r7
        LDI r4, -25
        ADD r7, r7, r4
        J r7
