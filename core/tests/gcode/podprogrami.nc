; test: mach3 axes=XYZ
G21 G90 G54 F500
G0 X0 Y0 Z5
M98 P10 L2
G0 X100
M98 P20
M30
O10 (ВЛОЖЕНА: ВСЕКИ ПЪТ ПО ЕДИН ОТВОР НАДЯСНО)
G91 G1 X10
G90
M98 P20
M99
O20
G1 Z-1
G0 Z5
M99
