@echo off
echo ------------- Creating MPXPLAY executable for Win32 console -------------

del mpxplay.exe > NUL
del *.err 2> NUL

wmake -h -f makewin

del *.obj 2> NUL
del *.bak 2> NUL
