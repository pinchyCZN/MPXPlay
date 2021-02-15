@ECHO OFF
SET WATCOM=c:\DEV\WATCOM
IF EXIST %WATCOM% GOTO HAVE_WATCOM

SET WATCOM=E:\DEV\WATCOM
IF EXIST %WATCOM% GOTO HAVE_WATCOM

ECHO CANT FIND WATCOM DIRECTORY
EXIT /B

:HAVE_WATCOM
ECHO WATCOM DIR=%WATCOM%
SET INCLUDE=%WATCOM%\h;%WATCOM%\h\nt
SET DOS4G=QUIET

SET PATH=%WATCOM%\BINNT;%WATCOM%\BINW

rem call %WATCOM%\owsetenv.bat