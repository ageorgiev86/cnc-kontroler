; test: mach3 axes=XYZ
(G42 С ДЪГИ, СПУСКАНЕ ПО Z ПРИ ВКЛЮЧЕНА КОМПЕНСАЦИЯ, КРАЙ БЕЗ G40)
G10 L1 P2 R1
G21 G90 G17 F300
T2 M6
G0 X0 Y-5 Z5
G42 D2 G1 X0 Y0
G1 Z-2
G2 X10 Y10 R10
G3 X20 Y20 I10 J0
G1 X30
M30
