; test: axes=XYZ
; както пише постпроцесорът на SolidCAM за Mach3: G68 X0 Y0 G91 R… (центърът е сегашната точка)
G21 G90 G54 G17
G0 X20 Y10 Z5
G68 X0 Y0 G91 R30
G90 G1 X30 Y10 F500
G68 X0 Y0 G91 R60
G90 G1 X30 Y10
G69
G1 X30 Y10
M30
