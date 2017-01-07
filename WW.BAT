@echo off
echo ------------- Creating MPXPLAY executable for Win32 console -------------

del mpxplay.exe > NUL
del *.err > NUL

wmake -h -f makewin

del *.obj > NUL
del *.bak > NUL
