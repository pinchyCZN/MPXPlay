@echo off
if exist "e:\dev\dmd2\windows\bin" (
	set PATH=e:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
) else (
	set PATH=c:\dev\dmd2\windows\bin;c:\dos\;%windir%;%windir%\system32
)

SET _SRC_DIR_=.\src
SET _OBJ_DIR_=.\obj
@echo on

SET _DPARAMS_=-m32 -c -betterC -I%_SRC_DIR_% -od%_OBJ_DIR_%

REM dmd %_DPARAMS_% %_SRC_DIR_%\dos_map.d
REM dmd %_DPARAMS_% %_SRC_DIR_%\dummy_.d
REM dmd %_DPARAMS_% %_SRC_DIR_%\intel_hda.d
REM dmd %_DPARAMS_% %_SRC_DIR_%\libc_map.d
rem dmd %_DPARAMS_% %_SRC_DIR_%\main.d
REM dmd %_DPARAMS_% %_SRC_DIR_%\minimp3.d
dmd %_DPARAMS_% %_SRC_DIR_%\mp3_file.d
rem dmd %_DPARAMS_% %_SRC_DIR_%\drflac.d
