; test: mach3 axes=XYZ
(КАКТО В MACH3 ПОСТПРОЦЕСОРА НА SOLIDCAM: G91 G10 L2 ПРЕМЕСТВА НУЛАТА)
G10 L2 P1 X100 Y50 Z-20
G91 G10 L2 P1 X5 Y-5 Z0
G90 G54 G0 X0 Y0
M30
