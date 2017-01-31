@echo off
echo --------------- Creating MPXPLAY executable for DOS ---------------
set DOS4G=QUIET

del mpxplay.exe > NUL
del *.err 2> NUL

wmake -h -f makedos

del *.obj 2> NUL
del *.bak 2> NUL
