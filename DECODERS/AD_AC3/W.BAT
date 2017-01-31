@echo off
del a52dec.lib
del *.err 2> NUL
wmake -h
rem goto skiplib
wlib -q a52dec.lib +bit_allo.obj
wlib -q a52dec.lib +bitstrea.obj
wlib -q a52dec.lib +crc.obj
wlib -q a52dec.lib +downmix.obj
wlib -q a52dec.lib +imdct.obj
wlib -q a52dec.lib +parse.obj
wlib -q a52dec.lib +resample.obj

:skiplib
del *.obj
del *.bak
