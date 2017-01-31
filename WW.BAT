@echo off
echo ------------- Creating MPXPLAY executable for Win32 console -------------

del mpxplay.exe > NUL
del *.err 2> NUL

wmake -h -f makewin

del *.obj > NUL
del *.bak > NUL
