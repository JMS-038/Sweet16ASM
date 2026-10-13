; Equivalent of the uploaded program2_romJMS image (decoded from its bytes).
        LDI r2, 1        ; r2 = 1
        IN  r3           ; r3 = Data_In
        LDI r1, 0        ; r1 = 0
        SLT r4, r1, r3   ; r4 = (r1 < r3)
        LDI r5, -4
        JZ  r4, r5       ; if r4 == 0, PC <- PC-4 (from this instruction's own address)
        ADD r1, r1, r2
        LDI r6, -5
        J   r6           ; PC <- PC-5
