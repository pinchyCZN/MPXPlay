@echo off
del *.err
wmake -h
del wavpack.lib

wlib -q wavpack.lib +bits.obj
wlib -q wavpack.lib +float.obj
wlib -q wavpack.lib +metadata.obj
wlib -q wavpack.lib +unpack.obj
wlib -q wavpack.lib +words.obj
wlib -q wavpack.lib +wputils.obj

del *.bak
del *.obj
