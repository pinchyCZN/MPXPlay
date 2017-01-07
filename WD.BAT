@echo off
echo --------------- Creating MPXPLAY executable for DOS ---------------
set DOS4G=QUIET

del mpxplay.exe > NUL
del *.err > NUL

wmake -h -f makedos

del *.obj > NUL
del *.bak > NUL
