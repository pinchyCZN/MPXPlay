@echo off
if exist "e:\dev\dmd2\windows\bin" (
	set PATH=e:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
) else (
	set PATH=c:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
)

@echo on 

dmd  -m32 -c -betterC test.d
dmd  -m32 -c -betterC minimp3.d
dmd  -m32 -c -betterC libc_map.d
rem dmd  -m32 -c test.d
