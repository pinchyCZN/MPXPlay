@echo off
echo --------------- Creating CONTROL library for Win32 version ---------------

del control.lib > NUL
del *.err 2> NUL

wmake -h -f makewin

wlib -q control.lib +control.obj
wlib -q control.lib +fastlist.obj
wlib -q control.lib +keyboard.obj
wlib -q control.lib +keygroup.obj
wlib -q control.lib +mouse.obj
wlib -q control.lib +serial.obj
wlib -q control.lib +joy.obj
wlib -q control.lib +startup.obj

del *.obj 2> NUL
del *.bak 2> NUL
