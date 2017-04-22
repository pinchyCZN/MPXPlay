@echo off
if exist "e:\dev\dmd2\windows\bin" (
	set PATH=e:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
) else (
	set PATH=c:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
)

SET _SRC_DIR_=.\src
SET _OBJ_DIR_=.\obj
@echo on

SET _DPARAMS_=-m32 -c -betterC -boundscheck=off -release -I%_SRC_DIR_% -od%_OBJ_DIR_%

dmd %_DPARAMS_% %_SRC_DIR_%\dos_map.d
dmd %_DPARAMS_% %_SRC_DIR_%\dummy_.d
dmd %_DPARAMS_% %_SRC_DIR_%\intel_hda.d
dmd %_DPARAMS_% %_SRC_DIR_%\libc_map.d
dmd %_DPARAMS_% %_SRC_DIR_%\main.d
dmd %_DPARAMS_% %_SRC_DIR_%\minimp3.d
dmd %_DPARAMS_% %_SRC_DIR_%\mp3_file.d
dmd %_DPARAMS_% %_SRC_DIR_%\drflac.d
