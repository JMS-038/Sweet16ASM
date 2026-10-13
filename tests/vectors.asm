; One of each instruction, registers chosen so d/a/b fields are all distinguishable.
        SLT r1, r2, r3
        ADD r1, r2, r3
        SUB r1, r2, r3
        AND r1, r2, r3
        OR  r1, r2, r3
        XOR r1, r2, r3
        SLL r1, r2
        SRL r1, r2
        IN  r7
        LDI r3, -32
        LDI r3, 31
        J   r7
        JZ  r2, r5
        JAL r1, r2, r3
        JR  r6
        OUT r4
        LD  r1, r2
        ST  r3, r4
        RESET
        NOP
        MOV r1, r2
        CLR r5
        .word 0xBEEF
