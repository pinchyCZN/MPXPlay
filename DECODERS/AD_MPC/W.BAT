@echo off
del *.err
wmake -h
del mpcdec.lib
wlib -q mpcdec.lib +mpcdec.obj
wlib -q mpcdec.lib +bitstrm.obj
wlib -q mpcdec.lib +huff_new.obj
wlib -q mpcdec.lib +requant.obj
del *.bak
del *.obj
