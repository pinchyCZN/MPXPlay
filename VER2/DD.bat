@echo off
if exist "e:\dev\dmd2\windows\bin" (
	set PATH=e:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
) else (
	set PATH=c:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
)

@echo on 

dmd -m32 -c -betterC -I.\MP3123 mp3_file.d
dmd -m32 -c -betterC -I.\MP3123 .\MP3123\minimp3.d
dmd -m32 -c -betterC -I.\MP3123 .\MP3123\libc_map.d
dmd -m32 -c -betterC -I.\MP3123 intel_hda.d
dmd -m32 -c -betterC -I.\MP3123 dos_map.d
rem dmd  -m32 -c test.d
