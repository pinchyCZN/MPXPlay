@echo off
echo --------------- Creating MPXPLAY executable for DOS ---------------
set DOS4G=QUIET
call set_env.bat

del mpxplay.exe 2> NUL
del *.err 2> NUL

wmake -h -f makedos

del *.bak 2> NUL
