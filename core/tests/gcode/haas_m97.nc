; test: haas axes=XYZ
%
O01000 (M97 ЛОКАЛНА ПОДПРОГРАМА)
G00 G90 G54 X0. Y0. Z5.
M97 P100 L3
G00 Z50.
M30
N100 G91 G00 X10.
G90 G01 Z-1. F200.
G00 Z5.
M99
%
