@echo off
echo --------------- Creating NEWFUNC library for DOS version ---------------

del newfunc.lib > NUL
del *.err 2> NUL

wmake -h -f makedos
wlib -q newfunc.lib +newfunc.obj
wlib -q newfunc.lib +bitstrm.obj
rem wlib -q newfunc.lib +commctrl.obj
wlib -q newfunc.lib +cpu.obj
wlib -q newfunc.lib +dll_load.obj
wlib -q newfunc.lib +dpmi.obj
wlib -q newfunc.lib +drivehnd.obj
wlib -q newfunc.lib +errorhnd.obj
wlib -q newfunc.lib +filehand.obj
wlib -q newfunc.lib +fpu.obj
wlib -q newfunc.lib +n_keyboard.obj
wlib -q newfunc.lib +memory.obj
wlib -q newfunc.lib +mixed.obj
wlib -q newfunc.lib +string.obj
wlib -q newfunc.lib +textdisp.obj
wlib -q newfunc.lib +time.obj
wlib -q newfunc.lib +timer.obj

wlib -q newfunc.lib +dos32wcr.lib

del *.obj > NUL
del *.bak > NUL
