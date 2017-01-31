@echo off
echo --------------- Creating DISPLAY library for DOS version ---------------

del display.lib > NUL
del *.err 2> NUL

wmake -h -f makedos
wlib -q display.lib +display.obj
wlib -q display.lib +buttons.obj
wlib -q display.lib +lcd.obj
wlib -q display.lib +textwin.obj
wlib -q display.lib +visualpi.obj

del *.obj > NUL
del *.bak > NUL
