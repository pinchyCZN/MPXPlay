@echo off
echo --------------- Creating VIDEOOUT library for Win32 ---------------

del videoout.lib > NUL
del *.err > NUL

wmake -h -f makewin
wlib -q videoout.lib +videoout.obj
wlib -q videoout.lib +vo_vesa.obj

del *.obj > NUL
del *.bak > NUL
