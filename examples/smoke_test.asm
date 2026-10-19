; Hardware smoke test: exercises LDI, ADD, LDW, OUT and a self-jump halt.
; Expected on the Logisim machine once it halts:
;   R1 = 5, R2 = 7, R3 = 12 (0x000C), R7 = 0x0300; Data_Out ends at 0x0300 (it showed 12 first).
        LDI r1, 5
        LDI r2, 7
        ADD r3, r1, r2
        OUT r3            ; Data_Out = 12
        LDW 0x0300        ; R7 = 0x0300 (overwrites R7)
        OUT r7            ; Data_Out = 0x0300
halt:   J   r0            ; PC <- PC + R0 = this same instruction, so the machine stays here
