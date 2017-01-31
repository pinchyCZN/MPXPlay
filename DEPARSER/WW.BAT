@echo off
echo --------------- Creating DEPARSER library for Win32 version --------------

del deparser.lib > NUL
del *.err 2> NUL

wmake -h -f makewin
wlib -q deparser.lib +in_aac.obj
wlib -q deparser.lib +in_ac3.obj
wlib -q deparser.lib +in_ape.obj
wlib -q deparser.lib +in_flac.obj
wlib -q deparser.lib +in_mp3.obj
wlib -q deparser.lib +in_mpc.obj
wlib -q deparser.lib +in_wavpa.obj
wlib -q deparser.lib +in_rawau.obj
wlib -q deparser.lib +tagging.obj

del *.obj > NUL
del *.bak > NUL
