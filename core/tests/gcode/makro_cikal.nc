; test: axes=XYZ
; Macro B: променливи, изрази, WHILE/END, IF GOTO, IF THEN, празна променлива
G21 G90 G54 G17
#1 = 0
#2 = 5.5
#3 = [#2 * 2 - 1] / 2
G0 X#3 Y-#2 Z[#2 + 4.5]
WHILE [#1 LT 3] DO1
G1 X[#1 * 10] Y0 Z-[#1 + 1] F[200 + #1 * 100]
#1 = #1 + 1
END1
#4 = SQRT[9] + ABS[-2] + ROUND[1.6] + FIX[2.7] + FUP[2.2]
G0 X#4 Y#10
IF [#4 GT 100] GOTO 50
IF [#4 EQ 12] THEN #5 = SIN[30] * 4
G0 Y#5
IF [#10 EQ #0] GOTO 60
G0 X999
N50 G0 X888
N60 #6 = ATAN[1]/[1]
G0 X#6
#500 = #6 MOD 40
G0 Y#500
WHILE [#1 GT 0] DO2
#7 = 0
WHILE [#7 LT 2] DO1
#7 = #7 + 1
END1
G0 X[#1 * 100 + #7]
#1 = #1 - 1
END2
M30
