@echo off
echo --------------- Creating VIDEOOUT library for DOS ---------------

del videoout.lib > NUL
del *.err 2> NUL

wmake -h -f makedos
wlib -q videoout.lib +videoout.obj
wlib -q videoout.lib +vo_vesa.obj

del *.obj 2> NUL
del *.bak 2> NUL
