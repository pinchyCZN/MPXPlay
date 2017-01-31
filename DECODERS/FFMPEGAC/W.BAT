@echo off
del *.err 2> NUL
wmake -h
del avcodec.lib
wlib -q avcodec.lib +UTILS.OBJ
wlib -q avcodec.lib +MEM.OBJ
wlib -q avcodec.lib +BITSTREA.OBJ
wlib -q avcodec.lib +GOLOMB.OBJ
wlib -q avcodec.lib +FFT.OBJ
wlib -q avcodec.lib +MDCT.OBJ

wlib -q avcodec.lib +FLAC.OBJ
wlib -q avcodec.lib +WMADEC.OBJ

del *.obj
del *.bak
