; Same idea as program2, written with labels and the JMP/JZL pseudo-ops.
start:  LDI r2, 1
        IN  r3
        LDI r1, 0
loop:   SLT r4, r1, r3
        JZL r4, r5, start   ; LDI r5,off ; JZ r4,r5
        ADD r1, r1, r2
        JMP r6, loop        ; LDI r6,off ; J r6
