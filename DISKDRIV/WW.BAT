@echo off
echo --------------- Creating DISKDRIV library for Win32 version -------------

del diskdriv.lib > NUL
del *.err > NUL

wmake -h -f makewin
wlib -q diskdriv.lib +diskdriv.obj
wlib -q diskdriv.lib +drv_cd.obj
wlib -q diskdriv.lib +drv_hd.obj
wlib -q diskdriv.lib +drv_ftp.obj
wlib -q diskdriv.lib +cd_aspi.obj

del *.obj > NUL
del *.bak > NUL
