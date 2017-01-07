@echo off
echo --------------- Creating DEMUXERS library for Win32 version --------------

del demuxers.lib > NUL
del *.err > NUL

wmake -h -f makewin
wlib -q demuxers.lib +in_asf.obj
wlib -q demuxers.lib +in_avi.obj
wlib -q demuxers.lib +in_mp4.obj
wlib -q demuxers.lib +in_ogg.obj
wlib -q demuxers.lib +in_wav.obj
rem wlib -q demuxers.lib +in_ffmpg.obj

del *.obj > NUL
del *.bak > NUL
