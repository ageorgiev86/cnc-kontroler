; test: mach3 axes=XYZ
(КВАДРАТ 20 X 20, ФРЕЗА Ø4: ВЪНШЕН КОНТУР G41 ПО ЧАСОВНИКА, ПОСЛЕ ВЪТРЕШЕН G41 ОБРАТНО)
G10 L1 P1 R2
G21 G90 G17 G54 F500
T1 M6
G0 X-10 Y-10 Z5
G1 Z-1
G41 D1 G1 X0 Y0
Y20
X20
Y0
X0
G40 G1 X-10 Y-10
G0 Z5
G0 X10 Y10
G1 Z-1
G41 D1 G1 X10 Y0
X20
Y20
X0
Y0
X10
G40 G1 X10 Y10
G0 Z5
M30
