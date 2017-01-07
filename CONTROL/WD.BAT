@echo off
echo --------------- Creating CONTROL library for DOS version ---------------

del control.lib > NUL
del *.err > NUL

wmake -h -f makedos

wlib -q control.lib +control.obj
wlib -q control.lib +fastlist.obj
wlib -q control.lib +keyboard.obj
wlib -q control.lib +keygroup.obj
wlib -q control.lib +mouse.obj
wlib -q control.lib +serial.obj
wlib -q control.lib +joy.obj
wlib -q control.lib +startup.obj

del *.obj > NUL
del *.bak > NUL
