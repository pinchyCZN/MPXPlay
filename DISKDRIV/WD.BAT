@echo off
echo --------------- Creating DISKDRIV library for DOS version ---------------

del diskdriv.lib > NUL
del *.err 2> NUL

wmake -h -f makedos
wlib -q diskdriv.lib +diskdriv.obj
wlib -q diskdriv.lib +drv_cd.obj
wlib -q diskdriv.lib +drv_hd.obj
wlib -q diskdriv.lib +drv_ftp.obj
wlib -q diskdriv.lib +cd_mscd.obj

del *.obj 2> NUL
del *.bak 2> NUL
