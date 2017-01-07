@echo off
echo ---------- Creating DISPLAY library for Win32 console version ----------

del display.lib > NUL
del *.err > NUL

wmake -h -f makewin
wlib -q display.lib +display.obj
wlib -q display.lib +buttons.obj
wlib -q display.lib +lcd.obj
wlib -q display.lib +textwin.obj
wlib -q display.lib +visualpi.obj

del *.obj > NUL
del *.bak > NUL
