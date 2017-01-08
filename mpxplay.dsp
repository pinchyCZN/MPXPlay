# Microsoft Developer Studio Project File - Name="mpxplay" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 60000
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mpxplay - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mpxplay.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mpxplay.mak" CFG="mpxplay - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mpxplay - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mpxplay - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mpxplay - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".\\" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "mpxplay - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I ".\\" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib Mpr.lib Winmm.lib Dsound.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "mpxplay - Win32 Release"
# Name "mpxplay - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "NEWFUNC"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\NEWFUNC\BITSTRM.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\COMMCTRL.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\CPU.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\DLL_LOAD.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\DLL_LOAD.H
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\DOS32LIB.H
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\DPMI.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\DRIVEHND.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\ERRORHND.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\FILEHAND.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\FPU.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\MEMORY.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\MIXED.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\N_KEYBOARD.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\NEWFUNC.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\NEWFUNC.H
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\RSITYPES.H
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\STRING.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\TEXTDISP.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\TIME.C
# End Source File
# Begin Source File

SOURCE=.\NEWFUNC\TIMER.C
# End Source File
# End Group
# Begin Group "CONTROL"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CONTROL\CNTFUNCS.H
# End Source File
# Begin Source File

SOURCE=.\CONTROL\CONTROL.C
# End Source File
# Begin Source File

SOURCE=.\CONTROL\CONTROL.H
# End Source File
# Begin Source File

SOURCE=.\CONTROL\FASTLIST.C
# End Source File
# Begin Source File

SOURCE=.\CONTROL\JOY.C
# End Source File
# Begin Source File

SOURCE=.\CONTROL\KEYBOARD.C
# End Source File
# Begin Source File

SOURCE=.\CONTROL\KEYGROUP.C
# End Source File
# Begin Source File

SOURCE=.\CONTROL\MOUSE.C
# End Source File
# Begin Source File

SOURCE=.\CONTROL\SERIAL.C
# End Source File
# Begin Source File

SOURCE=.\CONTROL\STARTUP.C
# End Source File
# End Group
# Begin Group "DISPLAY"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DISPLAY\BUTTONS.C
# End Source File
# Begin Source File

SOURCE=.\DISPLAY\DISPLAY.C
# End Source File
# Begin Source File

SOURCE=.\DISPLAY\DISPLAY.H
# End Source File
# Begin Source File

SOURCE=.\DISPLAY\LCD.C
# End Source File
# Begin Source File

SOURCE=.\DISPLAY\TEXTWIN.C
# End Source File
# Begin Source File

SOURCE=.\DISPLAY\VISUALPI.C
# End Source File
# Begin Source File

SOURCE=.\DISPLAY\VISUALPI.H
# End Source File
# End Group
# Begin Group "DISKDRV"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DISKDRIV\CD_ASPI.C
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\CD_ASPI.H
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\CD_DRIVE.H
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\CD_MSCD.C
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\DISKDRIV.C
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\DISKDRIV.H
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\DRV_CD.C
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\DRV_FTP.C
# End Source File
# Begin Source File

SOURCE=.\DISKDRIV\DRV_HD.C
# End Source File
# End Group
# Begin Group "AU_CARDS"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\AU_CARDS\AC97_DEF.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\AC97_DEF.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\AU_CARDS.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\AU_CARDS.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\DMAIRQ.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\DMAIRQ.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\DSOUND.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\EMU10K1.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\PCIBIOS.C

!IF  "$(CFG)" == "mpxplay - Win32 Release"

!ELSEIF  "$(CFG)" == "mpxplay - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\PCIBIOS.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_CMI.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_E1371.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_ESS.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_GUS.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_ICH.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_INTHD.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_INTHD.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_MIDAS.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_NULL.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_SB16.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_SBL24.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_SBL24.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_SBLIV.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_SBLIV.H
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_SBPRO.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_SBXFI.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_VIA82.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_WAV.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_WINDS.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_WINWO.C
# End Source File
# Begin Source File

SOURCE=.\AU_CARDS\SC_WSS.C
# End Source File
# End Group
# Begin Group "PLAYLIST"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\PLAYLIST\CHARMAPS.H
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\CHKENTRY.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\DISKFILE.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\EDITLIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\FILEINFO.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\ID3LIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\JUKEBOX.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\LOADDIR.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\LOADLIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\LOADSUB.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\PLAYLIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\PLAYLIST.H
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\RANDLIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\SAVELIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\SKIPLIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\SORTLIST.C
# End Source File
# Begin Source File

SOURCE=.\PLAYLIST\TEXTCONV.C
# End Source File
# End Group
# Begin Group "VIDEOOUT"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\VIDEOOUT\VIDEOOUT.C
# End Source File
# Begin Source File

SOURCE=.\VIDEOOUT\VIDEOOUT.H
# End Source File
# Begin Source File

SOURCE=.\VIDEOOUT\VO_VESA.C
# End Source File
# End Group
# Begin Group "AU_MIXER"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\AU_MIXER\ANALISER.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\AU_MIXER.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\AU_MIXER.H
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\CUTSILEN.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\CV_BITS.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\CV_CHAN.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\CV_FREQ.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\FX_SURR.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\FX_TONE.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\MIX_FUNC.H
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\MX_AVOL.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\MX_CROSF.C
# End Source File
# Begin Source File

SOURCE=.\AU_MIXER\MX_VOLUM.C
# End Source File
# End Group
# Begin Group "DECODERS"

# PROP Default_Filter ""
# Begin Group "AD_MP3"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DECODERS\AD_MP3\LAYER2.C

!IF  "$(CFG)" == "mpxplay - Win32 Release"

!ELSEIF  "$(CFG)" == "mpxplay - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_MP3\LAYER3.C

!IF  "$(CFG)" == "mpxplay - Win32 Release"

!ELSEIF  "$(CFG)" == "mpxplay - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_MP3\MP3DEC.H
# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_MP3\SYNTH.ASM

!IF  "$(CFG)" == "mpxplay - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
IntDir=.\Release
InputPath=.\DECODERS\AD_MP3\SYNTH.ASM
InputName=SYNTH

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw.exe -t -f  win32 -o$(IntDir)\$(InputName).obj -Xvc $(InputName).asm

# End Custom Build

!ELSEIF  "$(CFG)" == "mpxplay - Win32 Debug"

# PROP Exclude_From_Build 1
# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# End Group
# Begin Group "AD_WAVPA"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DECODERS\AD_WAVPA\BITS.C
# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_WAVPA\FLOAT.C
# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_WAVPA\METADATA.C
# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_WAVPA\UNPACK.C
# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_WAVPA\WAVPACK.H
# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_WAVPA\WORDS.C
# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_WAVPA\WPUTILS.C
# End Source File
# End Group
# Begin Source File

SOURCE=.\DECODERS\AD_MP3.C

!IF  "$(CFG)" == "mpxplay - Win32 Release"

!ELSEIF  "$(CFG)" == "mpxplay - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\DECODERS\AD_PCM.C
# End Source File
# Begin Source File

SOURCE=.\DECODERS\DECODERS.C
# End Source File
# Begin Source File

SOURCE=.\DECODERS\DECODERS.H
# End Source File
# End Group
# Begin Group "DEPARSER"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DEPARSER\IN_AAC.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_AC3.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_APE.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_FLAC.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_MP3.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_MPC.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_RAWAU.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_RAWAU.H
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\IN_WAVPA.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\TAGGING.C
# End Source File
# Begin Source File

SOURCE=.\DEPARSER\TAGGING.H
# End Source File
# End Group
# Begin Group "DEMUXERS"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DEMUXERS\IN_ASF.C
# End Source File
# Begin Source File

SOURCE=.\DEMUXERS\IN_AVI.C
# End Source File
# Begin Source File

SOURCE=.\DEMUXERS\IN_FFMPG.C
# End Source File
# Begin Source File

SOURCE=.\DEMUXERS\IN_MP4.C
# End Source File
# Begin Source File

SOURCE=.\DEMUXERS\IN_OGG.C
# End Source File
# Begin Source File

SOURCE=.\DEMUXERS\IN_WAV.C
# End Source File
# End Group
# Begin Source File

SOURCE=.\IN_FILE.C
# End Source File
# Begin Source File

SOURCE=.\MPXINBUF.C
# End Source File
# Begin Source File

SOURCE=.\MPXPLAY.C
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\IN_FILE.H
# End Source File
# Begin Source File

SOURCE=.\MPXINBUF.H
# End Source File
# Begin Source File

SOURCE=.\MPXPLAY.H
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
