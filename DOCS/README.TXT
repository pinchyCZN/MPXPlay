MPXPLAY Audio player v1.57 for DOS and Windows by PDSoft (Attila Padar, Hungary)

(currently DOS and Windows versions have common docs)

Supported filetypes (decoders,demuxers):

AAC     (Advanced Audio Coding from http://www.audiocoding.com (in RAW/ADTS/ADIF fileformats))
AC3     (Dolby AC3 from http://a52lib.sourceforge.net)
AIF     (a wav-like container, only uncompressed pcm data is supported)
APE     (Monkey's Audio from http://www.monkeysaudio.com)
AVI     (with AAC,AC3,DTS,MP2,MP3,PCM,WMA audio)
CDW     (Audio-CD 'ripp and play' on the fly)
FLAC    (Free Lossless Audio Codec by http://flac.sourceforge.net and http://ffmpeg.sourceforge.net)
MP2,MP3 (Audio MPEG Layer II/III from http://www.mpg123.de)
MP4/M4A (MPEG4 container with AAC, ALAC and MP3 audio)
MPC     (MPEGPlus/MusePack from http://www.musepack.net)
OGG     (Ogg-Vorbis from http://www.xiph.org and Ogg-Flac)
WAV/W64 (Wave32 and Wave64 with AAC,AC3,DTS,MP2,MP3,WMA or uncompressed PCM audio in 8,16,24,32 integer or 32,64 float formats)
WMA/WMV (ASF container with WMA (7.0,8.0) and with AAC,AC3,DTS,MP2,MP3,PCM audio)
WV      (WavPack hybrid lossless audio from http://www.wavpack.com)

With additional plugins:
DTS     (Digital Theatre Systems from http://www.videolan.org/dtsdec.html)
MOD     (Module playing based on LibModPlug, see readme.txt in in_modd.zip)
SPX     (Ogg-Speex from http://www.xiph.org)

Playlists:

M3U,M3U8,PLS,FPL,CUE,MXU


Send suggestions and error reports to

 mpxplay@freemail.hu   (please don't public this email address (ie: don't put it on your web page))

important notes and requests before you send me mail:
 - write MPXPLAY in the subject line !!!
 - before You ask something from me or you would like to send me a bug report,
   please read README.TXT (this), MOREINFO.TXT and FAQ.TXT files!
 - you can find a lot of extra infos in the MPXPLAY.INI file too
   (keyboard mapping, LCD, joystick and serial port (infra receiver) configuration)
 - some infos are in the source files only (mpxsrc.zip)(ie: hardware description of LCD types in display\lcd.c)
 - don't send attachments (only if I ask for it)
 - sorry, I can't answer to everybody, but I read your requests/problems
   and I try to implement/modify them, if it's possible

Visit my homepage(s) for the latest Mpxplay versions:

   http://mpxplay.sourceforge.net
   http://www.freewebtown.com/mpxplay
   http://www.sf.net/projects/mpxplay (older Mpxplay versions, newer forum)
   http://mpxplay.cjb.net   <- this address is linked to my primary page (but filled with a lot of commercial)

And visit the Mpxplay forums for extra informations (linked on my homepages too).

   http://www.sf.net/projects/mpxplay
   http://forum.supergamez.hu/forum.php3?azonosito=mpxplay (not active, just keeped for information database)


Mpxplay system requirements (mainly for DOS):

- Minimum CPU for playing 44.1khz, stereo, (128kbit/s) files (with '-f0 -sl 0 -bl -bn8' options):

  AAC : Intel Pentium 66    (LC object type)
  AC3 : Intel 80486 DX4-120 (48khz,2.0ch,192kbit)
  APE : Intel Pentium 166 MMX (fast-mode encoded files work without MMX too)
  DTS : Intel PII-300 (48kHz,5.1ch,768kbit)
  FLAC: Intel Pentium 66  (depending on the compression ratio too)
  MP2 : Intel 80486 DX2-50  (DX2-66)
  MP3 : Intel 80486 DX2-80  (DX4-100)
  MPC : Intel 80486 DX4-100
  OGG : Intel 80486 DX4-120 (DX4-133)
  WMA : Intel Pentium 90
  WV  : Intel Pentium 166
  (note: demuxing from AVI,ASF,MP4 requires some extra time if it contains more than one stream (video or audio))

- Supported soundcards in DOS version (see sndcards.txt for more infos):
 -PCI cards (use in DOS only):
  - SB Live, Audigy 1,2,4, Live 24, Audigy LS, X-Fi Xtreme Audio
  - CMI 8338/8738
  - Ensoniq ES1371/1373 (SB PCI 16/64/128)
  - Intel ICH(0-7), NForce(1-3)
  - Intel HDA compatible chips (AD19xx,ATI,CMI9880,Realtek,VIA170x)
  - VIA VT686/8233/8235/8237
 -ISA cards (or emulation):
  - SB16 or compatible (DSP 4.x):
    - SB16 (ISA not PCI!)
    - SB AWE-32/64
    - SB PCI 128,512(?),Live,Audigy 1 (with SB16 emulation)
    - CMI-8330
  - WSS compatible: 
    - Crystal Sound (CS423x)(ie:Turtle Beach Malibu)
    - Yamaha OPL3
    - SB PCI 64, Ensoniqe (?)
    - some OPTI cards (?)
    - some CMI cards (?)
    - some ESS cards (?)
  - ESS 688,1688,1868,1869,1878,1879,1887,1888 chips (?)
  - GUS (GUS MAX, GUS PnP (AMD Interwave)(?))
  - SB Pro or compatible (DSP 1.5, 2.x, 3.x)(a lot of cards)
   ((?) means: not tested, maybe it doesn't work properly)

- Supported sound outputs in Windows version:
 - DirectSound (WDS) (default/primary)
 - Wave Mapper (WIN)

- Memory (not exact calculations):
 - 300k kbyte conventional
 - 8Mb XMS (+size_of_file at -bl)

- Display: VGA (or EGA)

- OS: 
 - MS-DOS 5.0 or above, FreeDOS,DR-DOS, Windows 95/98 (dos box), Linux & OS/2 dos emu (?)
  (under DOS the DOS32A or DOS4G version is recommended, the PMODE/W may cause memory allocation and IRQ problems)
  (under Linux and OS/2 (DOS emu) the PMODE/W or DOS32A version is recommended)
 - Windows (Win98,Win2000,WinXP)

Command line option examples:

 MPXPLAY.EXE [option(s)] file [file2] [file3] [...] ('file' can be an audiofile (mp3,wav,etc.), playlist (m3u,pls,fpl,cue,mxu) or dir-scan (d:\subd*.*\file*.*)) (multiply input files don't work at WAV output (-o))
 MPXPLAY.EXE [option(s)] -ds cde [filemask]  (scan c: d: e: drives [for 'filemask' files (ie: -ds cde *.mp3)])
 MPXPLAY.EXE [option(s)] d:\*.cdw            (play all audio CD tracks)
 MPXPLAY.EXE -o -sl 0 infile.mp3 outfile.wav (decode mp3 to a wav file)
 MPXPLAY.EXE -o -sl 0 playlist.m3u           (decode all audio files (from the playlist) to wav files (into the current dir))
 MPXPLAY.EXE -o -sl 0 *.mp3 d:\outdir        (decode all audio files (*.mp3) to wav files (into the d:\outdir directory)
 MPXPLAY.EXE -o -obs *.mp4                   (demux aac audio from mp4 container)
 MPXPLAY.EXE -o -sl 0 d:\1.cdw 1.wav         (copy the 1. audio CD track into a wav file (ripp))
 MPXPLAY.EXE -o -sl 0 d:\*.cdw [d:\outdir]   (copy all audio CD tracks into wav files (into the current or into a specified dir))

for slow computers (486DX4-100 or bellow)(-bl requires 7-10Mb RAM (3Mb + filesize)):
 MPXPLAY.EXE -irl -sl 0 -ddma -f0 -v -bl -bn8 playlist.m3u
 MPXPLAY.EXE -irl -sl 0 -ddma -f0 -v -bs playlist.m3u

List of all command line options:

Playlist:
 -@  PLAYLIST : load a playlist (not really needed, Mpxplay can detect the playlist filetypes (M3U,CUE,etc.))
 -@i          : load/read playlist from stdin (m3u style)(ie: dir /b *.mp3 | mpxplay.exe -@i)
 -@d PLAYLIST : jukebox mode (load playlist to the left side)
 -@s PLAYLIST.M3U  : save playlist from editor into PLAYLIST file on exit (default:MPXPLAY.M3U)
 -@sw PLAYLIST.M3U : save playlist in EXTM3U format (like Winamp)
 -@sx PLAYLIST.MXU : save playlist in (own) extended format (playlist with file infos)
 -@sc PLAYLIST.CUE : save playlist in CUE format (indexed files)
 -ds DRIVES : multiple drive scan (-ds cde -> scans c: d: e: drives)(-ds cde *.wav -> search wav files only)
 -dd : enable jukebox mode (load files to the left side)(for dir-scan (*.*) drive scan (-ds) and directory browser)

 -prn  : random play
 -prn2 : randomize playlist at start
 -pre  : replay/repeat playlist
 -pre1 : replay/repeat only 1 song (rewind to the begin of file)

 -psu NUM  : startup-type (StartupEnabled) override (0,3,7,11,19,27) (see mpxplay.ini)
 -pss NUM  : start play at NUM. song  (PlayStartSong) (without NUM, it just starts the playing at the first (or last played) song)
 -pst TIME : start song at TIME pos.  (PlayStartTime) (TIME may be in HOURS:MINUTES:SECONDS.HUNDREDTHS (HH:MM:SS.NN or MM:SS.NN or SSS.NN or SSS) format (ie: 3:17 or 197.25))
 -psf NUM  : start song at NUM. frame (PlayStartFrame)
 -psp NUM  : start song at NUM-16%    (PlayStartPercent)(16% is the begin of the song (116% is the end))
 -pcs NUM  : exit after NUM song      (PlayCountSong)
 -pct TIME : play duration (in 1 song)(PlayCountTime) (TIME may be in HH:MM:SS.NN or MM:SS.NN or MM:SS or SSS.NN or SSS (etc.) format)
 -pcf NUM  : play NUM frames only     (PlayCountFrame)
 -pcp NUM  : play NUM% from the song  (PlayCountPercent) (1-100)
 -pef NUM  : finish playing at NUM. frame (PlayEndFrame)(PlayFrameCount=PlayEndFrame-PlayStartFrame)(doesn't work with -psp)
 -pslf NUM : use/load NUM. fastlist (PlayStartList)(NUM. list in [fastlist] config)(without NUM, it loads the last used list and restores the list/song position from the [startup])
 -phs      : hi-lite scan, play only 10 seconds from every songs (1m00s-1m10s)(configurable with -pst,-pct options)
 -ppa      : pause at each (all) songs
 -ppn      : pause at first (next) song (only)

Display:
 -f0  : no screen output (some warning/error messages are not displayed in this mode (ie: serial-control and LCD errors))
 -ff  : only framepos and current filename are displayed
 -fl  : one line display output
 -v   : verbose mode (mpeg info,id3tag info for -f0,-ff,-fl options)(use -fl -v order, not -v -fl)
 -fe  : full screen playlist-editor (without spectrum-analyser & browser)
 -fs  : single lines mode (disable 50 lines mode change at start)
 -db  : disable directory browser in playlist editor (left side)(use -xel option to exit after the last song)
 -db2 : directory browser on both sides (if StartupEnabled=3 in mpxplay.ini) (it has higher priority than the playlist loading)
 -ebs NUM : editor border size (1-64)(def:64)

Visual plugins (only at DOS4G version):
 -vps NAME : visual plugin selected name (else pick up all visual DLLs and skip after user-request (keyboard or mouse))
 -vpt SECS : visual plugin start time (start visualization after SECS seconds)(default: 0, visual plugins will not start without this option)

Input (file) buffer:
 -bp  : large prebuffer (32*32k=1024k) (decoder runs on INT08) (default mode)
 -bpn : -bp + preload next song before the end of current one (for better gapless playing)
 -bpm NUM : prebuffer size in megabytes (for -bp,-bs) (def: 1 (at the default -bp mode)) (1Mb = 1048576 bytes)
 -bpb NUM : prebuffer size in blocks (for -bp,-bs)(1 block=32kbytes)(use: 8 - 64 (MP3-WAV))
 -bb  : back (rewind) buffer (useful for slow drives, like cd,flash,ftp) (without -bbm option: size_of_backbuffer = size_of_prebuffer)
 -bbm NUM : use backbuffer with NUM megabytes
 -bl  : fullbuffer (it loads the (whole) file into memory before playing (depends on the free memory and the used extender too))
 -bs  : small prebuffer (4*32k=128k) (without INT08 decoding) (use this only if you have problems with the default -bp mode)
 -bni : disable interrupt decoding (for -bl)
 -bn8 : disable all INT08 functions (interrupt decoding,dma monitor)
 -bn  : no input buffer (not recommended, for testing only)
 (note for -bp/-bb: the complete size of the file (ring) buffer will be: size_of_backbuffer + size_of_prebuffer, the prebuffer-size sets that how many bytes are pre-readed from the file)

Sound card and file (wav/bitstream) output:
 -scs CARD : manual soundcard select (force) (CARD can be: in DOS version: CMI,ENS,ICH,IHD,SBA,VIA,S16,ESS,WSS,GUS,SBP,WAV,NUL, in Win version: WDS,WIN,WAV,NUL)
 -scd NUM  : select sub-device number (for WIN and WDS only) (use -sct to display device numbers (0,1,2))
 -scc NUM  : soundcard sub-config (currently for WDS only: +1 use primary buffer, +2 loc-hardware, default=1) (value 3 can give the best sound quality, but doesn't work in every cases)
 -sct CARD : soundcard test (without CARD argument it tests and displays all detected cards)
 -scv NUM : set sound card volume (master & pcm together)(0-100(%))(at some soundcards under Win9x use 100% (ie: SB16 emu of SB Live/Audigy))
 -ddma    : double dma (soundcard buffer) (16-bit output: 32k->64k; 24-bit: 48k->96k; 32-bit: 64k->128k) (recommended to use if outfreq>48000)
 -o       : write pcm data to a wav file (instead of soundcard)(rather use this than -scs WAV)
 -of  NUM : set output frequency (constant soundcard freq or freq conversion at wav output)
 -oc  NUM : set output channels (WDS: 2-8, wav-file: 1-2)
 -ob  NUM : set output bits (Audigy 2/4,Live24,ICH4,IHD,WIN,WDS,wav-file)
 -obf     : write 32-bit floating-point pcm data (id=0x0003) (WDS and wav-file)
 -obs     : bitstream mode output (no decoding)(currently for file-out (-o) and with AAC,AC3,DTS,MP2,MP3 (AVI,ASF,MP4) input/output only)(ie: mpxplay.exe -o -obs infile.mp3 outfile.mp3 -pst 1:00 -pct 0:30)
 -oni     : create output filename from the id3-info (at -o,-obs)(nn. artist - title.wav)(works only if LFN is supported (with DOSLFN or under Windows))
 -oat     : automatically copy tags from input to output file (at encoders: FLAC,LAME,OGGV)
 -t       : testmode (no sound output, only decoding)
 Separated soundcard mixer channel settings (most of the cards have master and pcm mixchan only):
  -scvom NUM : output master volume (0-100)
  -scvop NUM : output pcm volume
  -scvoh NUM : output headphone volume
  -scvos NUM : output spdif volume
  -scvim NUM : input mic volume
  -scvil NUM : input line volume
  -scvic NUM : input cd volume
  -scvia NUM : input aux volume

Sound mixing and output modifying:
 -sv   NUM : software volume setting (amplifying)(0-700)(def.: 100 (%))(volume is increased manually, decreased by limiter (latter has no desktop effect))
 -sva      : enable auto volume correction (slow)
 -sr   NUM : surround/stereo expander (def.:100 (%) = no surround, 10=mono, 150=wide stereo)
 -sp   NUM : playing speed (freq) control (50-990)(def.: 100) (or 500-9999 (def. 1000) if you set MixerControl=5)
 -sctr NUM : set tone-treble (1-100) (SB16,AWE32/64: hardware; otherwise (or with MixerControl=3): software)
 -scbs NUM : set tone-bass   (1-100) (def.: 50 = no tone change)
 -sl   NUM : soundlimit (playlimit)(0-200)(def.:20)(finishes the playing of the song if the sound volume is under the limit)
 -mxlo NUM : volume limiter overflow value in dB (0-18) (def.:0) (you can get a little more sound volume with distortion)
 -mxmd NUM : mute volume divisor (1-999) (def: 8)

 -csa NUM : select audio stream (in ASF/AVI/MP4/OGG, if it contains more than one audio stream)

 -cl : decode left channel only (1.9x/1.4x faster decoding at MP3/OGG files (only))
 -cm : downmix channels (stereo to mono)(ca. 1.7x faster decoding at MP3 files only)

 -cf      : enable crossfade
 -cft NUM : crossfadetype   (def. 1) (0.bit (+1) :fade-out, 1.bit (+2) :fade-in)(-cft 3 : fade out + fade-in)
 -cfo NUM : fade-out length (def. 250)
 -cfp NUM : crossfade point (def. 120)(start crossfade at num. frame, relative to begining of fade-out)
 -cfi NUM : fade-in length  (def. 200)
 -cfl NUM : crossfade limit (def. 0) (start crossfade, when volume less than num (don't use crossfade point))
 (crossfade only: -cfo 150 -cfp 0 -cfi 150 , fade-out & fade-in only: -cfo 150 -cfp 150 -cfi 100 )

ID3-info, file-info loading:
 -inl : don't preload (load at skip only) file informations (header,id3tag) (but MXU/EXTM3U works (has effect))
 -irl : load infos at program start (start playing only if all fileinfos are loaded)
 -ipl : load (all) infos paralell with playing
 -idl : load infos of displayed files only (displayed in playlist editor window)
 -ihl : load infos of highlighted file only (at cursor move (up/down arrows))
 -in  : don't load/display id3tag infos (only filename and time-length)
 -inf : don't load id3tag infos from files (but use LFN and EXTM3U/MXU/CUE infos) (equal to LoadID3Tag=5 in mpxplay.ini)
 -if KEYWORD : tag-filter (keep files which have KEYWORD in their id3tag(s) (or in filename(s) if there's no id3tag), delete the others)(restrictions: works properly with -irl option only; KEYWORD may not contain space(s); very slow at large playlists)

ID3-tag write/update in audio files (at aac,ac3,ape,dts,mp2,mp3,mpc,wv files only) (ID3v1,ID3v2,APETag):
 -iw  : enable id3-tag writing
 -ita "artist"  : set artist tag (these options work without -iw option too)
 -itt "title"   : set title tag  (you also can use a listfile (-il) to set artist:title infos)
 -itl "album"   : set album tag
 -ity "year"    : set year tag
 -itc "comment" : set comment tag
 -itg "genre"   : set genre tag  (note: ID3v1 has standard (static) genre-types only (see id3genres[] in in_mp3.c))
 -itn "tracknum": set tracknum tag (note: ID3v1.1 (MP2,MP3) can store a number between 0 and 255 (ie: -itn "63") only)
 (note: if the audio file already has id3-tag and you don't set new tag(s)
        then the program keeps the original tags of the file
        you can use "" to clear a tag (ie: -itl ""))

Character (codepage) conversion (from audio files to Mpxplay and back):
 -8   : convert characters in id3tags (configured by WinChars/DosChars in mpxplay.ini) (options priority: -8w < -8ucp/-8ccp < -8)
 -8f  : convert characters in filenames too (not recomended) (use it with -8)(-8u has no effect on filenames, -8ua converts filenames in M3U8,FPL and on display)(if a char conversion is wrong, remove it from the WinChars/DosChars)
 -8u  : enable UTF-8 text decoding in tags (in every tags/filetypes) (not recomended)
 -8ua : enable auto UTF-8 and UTF-16 decoding (where it's required)(APEv2,ASF,FLAC,ID3v2,Ogg,FPL,M3U8)(enabled by default, you can disable it in mpxplay.ini (Conv852437))
 -8uv : validate (use only) WinChars at UTF-8 decoding (above ascii 128)(to avoid wrong/false decoding at non-utf8 texts)
 -8b  : convert back chars (for -iw)(from Mpxplay to audio files, from DOSchars to WINchars only)(because -8 converts in both direction)
 -8w  : set (fill) automatically -8ucp/-8ccp by the ansi and oem codepages of windows (win32 version only)
 -8ucp CPNAME : source codepage (for UTF decoding and for codepage conversion) (default: ISO-8859-2) (see moreinfo.txt for more CPNAMEs)
 -8ccp CPNAME : target codepage for codepage conversion and set the codepage of win32 console-window (both are CP437 by default)
 (For convertable codepages like CP1250/CP852 you can use the -8ucp/-8ccp options, but for the non-convertable ones like CP1250/CP437 you have to use the manual configuration of WinChars/DosChars with the -8 option)
 (Using -8ccp and -8 option together in win32, the console codepage will be set by the -8ccp, but the char conversion will be configured by the WinChars/DosChars)
 (The desktop of Mpxplay has built in CP437, some drawing chars are missing in other codepages)

Playlist sort (read moreinfo.txt, sort bugs too):
 -io  NUM : primary order key; order playlist by 0=title, 1=artist, 2=album, 3=year, 4=comment (file-extension in commander mode), 5=genre, 6=tracknum, 7=time, 8=filesize, 9=path, 10=filename, 11=path\filename, 12=filedate, 99=disable ordering (order only directory entires by filename)
 -io2 NUM : secondary order key (0=title, 1=artist, etc.)
 -io3 NUM : third order key (-"-)
 -io4 NUM : fourth order key (-"-)
 -iod     : descending order (else ascending)(use it with -io)

ID3-list:
 -il [infofile] : load LOCAL id3info listfile (for directory browser (left side), (re)loads list from every directory (after dir change)) (see moreinfo.txt too)
 -ig [infofile] : load GLOBAL id3info listfile (for both sides (directory browser and playlist)(create of id3list: mpxplay.exe playlist.m3u -@sx infofile.txt)
 -is [infofile] : save id3 infos in a listfile
 -ist NUM : field-types for -is (the bits of the NUM change/enable the fields)(def.:61)
   0. bit (  +1): list directory names
   1. bit (  +2): list filenames
   2. bit (  +4): artist-title list mode (non-fixed/fixed position)
   3. bit (  +8): list time of file
   4. bit ( +16): list bitrate (kbit/s)
   5. bit ( +32): list filesize in megabytes
   6. bit ( +64): list filenames with full path

Other (program controls):
 -x PRGNAME : set program name for DOS shell (i.e. -x nc.exe)
 -xs        : call a dosshell (automatically) after the start of Mpxplay
 -xr        : DOS version: TSR mode (works properly for CDW files only)('mpxplay.exe -bl -xr -xs -pre1 song.mp3' for continuous background playing)(this option has keeped for a possible future developing, it's not really usefull)
              Win32 version: +1 thread for file prebuffering (disk reading doesn't stop the control/display functions) (but seeking is not perfect and sometimes it can crash the program)
 -xas       : enable atx/apm power (hardware) shutdown (at exit)
 -xce       : confirm exit
 -xel       : exit at end of (play)list (has effect in directory browser and in jukebox queue too)
 -xts TIME  : program play start time (start playing (unpause) at TIME)(in the next 24 hours) (TIME may be in HH:MM:SS or HH:MM or HH format)
 -xte TIME  : program exit time (in the next 24 hours) (TIME may be in HH:MM:SS or HH:MM or HH format)
 -xtl TIME  : program run-time length (from -xts or from the start of the program)(max. 24 hours)(TIME may be in HH:MM:SS or HH:MM or HH format)
              (note: HH is hour(s), MM is minute(s), SS is second(s))
 -dlls      : list informations about DLLs in Mpxplay's dir (at DOS4G and WIN versions)
 -drfc NUM  : ftp-client config (default=7) (+1 : use passive mode, +2 : auto reconnect if connection losts, +4: use anonym login, if no user/passw, +8 : use UTF8 filenames)

Video out (not used yet):
 -vos NAME: select video output (driver) (built-in: VESA)
 -volm    : list available video modes (for the selected video driver)
 -vom HEX : set video mode manually (from the list by -volm)
 -vox NUM : set horizontal resolution (def:1024) (ie:640,800,1024,1280,1600)
 -voy NUM : set vertical resolution (ie:400,480,600,768,1024,1200)
 -vob NUM : set output bits / color depth (def:32) (if the videoout driver supports it) (16:hicolor; 24,32:truecolor)


Keyboard controls (some keys are case sensitive (lower case only)!)
(suggestion: set the keyboard speed to 30 char/sec in BIOS):

 ESC      - exit from the program (or terminate drive scanning, playlist checking, file loading (-bl), AAC seeking)
 CTRL-C   - exit from the program (in win version: the exit confirm doesn't work and usually it terminates the program at freezing too)
 F10      - exit from the program without shutdown (at -xas)
 gray-'-' - step back one song in playlist
 gray-'+' - step forward one song in playlist
 white-'-'- step back like a CD player (if framenum>38, step to begin of song, else step to previous song)
 gray-'/' - step back one subdirectory (album) in playlist
 gray-'*' - step forward one subdirectory (album) in playlist
 alt-'/'  - load previous fastlist (from [fastlist] config in mpxplay.ini)
 alt-'*'  - load next fastlist
      ->  - (right arrow) forward 50 frames (configurable with SeekFrames in mpxplay.ini)
      <-  - (left arrow)  rewind  50 frames (50 frames is usually 1.3 sec)
 CTRL-gray-RIGHT  - seek +1% (or min. 200 frames)
 CTRL-gray-LEFT   - seek -1% (or min. 200 frames)
 CTRL-white-RIGHT - 4x seek forward (200 frames)
 CTRL-white-LEFT  - 4x seek rewind (200 frames)
 BACKSPACE- jump to begin of song (to 0. frame)
  A       - spectrum analyser on/off
  C       - crossfader on/off
  d       - hi-lite scan (demo mode) start/stop (plays 10 secs from every file)
 CTRL-'d' - open a DOS-shell (type 'exit' to return)
  E       - switch between player and commander mode playlist editor (show song or file infos) (LCD: show only filenames instead of id3infos) (note: loading of filesize and filedate infos in commander mode depends on the using of Preloadinfo/-inl/-idl/-ipl settings, same like in player mode)
 CTRL-'e'   rename file(s) by ID3-tag(s) (trackNN.MP3 -> NN. Artist - Title.MP3) (note: there is a codepage-back char conversion in it)
  F       - (cross) fade out/in select (on/off)
 CTRL-'f' - open ftp session (url) (ftps: imlicit SSL, ftpes: explicit TLS)
  g       - switch between text and vga (video) mode (not useful yet)
 CTRL-'g' - skip one LCD page
  i       - insert index / set index start point (duplicate entry with startpos=currpos)
 CTRL-'i' - remove index (append index to next/previous index)
  J       - switch between juke-box (doombox) and normal mode (reloads playlist!)
 CTRL-'k' - close ftp session
  l       - move cursor to the currently played entry/song (and change drive/directory too if playside is dir-browser)
  M       - mute sound (while press) (1/8 sound volume)
 CTRL-'m' - mute sound (on/off switch)
  N       - random play (shuffle) on/off
 CTRL-'n' - randomize side-entries immediately
  o       - open/close CD door (of primary CD drive)
  P       - start/pause playing (SPACE button too)
 CTRL-'p' - autopause on/off
  R       - replay (repeat) mode (off / 1 song / full playlist)
 CTLR-'r' - refresh directory browser OR reload playlist/rescan directories,drive(s) (ie: after CD change)
  S       - stop playing (and jump to begin of playlist)
 CTRL-'s' - pause playing at (the start of) next song (doesn't work in win32 version, map it to an other button, if you need it, or use the desktop "Pn" switch)
  T       - time mode (elapsed,remaining,allelapsed,allremaining)
  V       - auto volume on/off
 CTRL-V   - reset all mixer functions to center/zero value (volume,surround,speed,balance,treble,bass)
  X       - swap channels (reverse stereo)

  .       - volume up   ('white home' too) (0-14: +1%, 15-100: +6%, 100-700: +0.5dB)
  ,       - volume down ('white end' too)
  '       - surround up   (stereo expander)
  ;       - surround down (to mono)
  ]       - playing speed up
  [       - playing speed down
  \       - speed up while push (fast-forward like tape-deck by speed-control)
  <       - balance-left
  >       - balance-right
  "       - bass up (hardware tone on SB16 and AWE32/64 cards, else software tone at MP3/MP2/MPC files)
  :       - bass down (-"-)
  }       - treble up (-"-)
  {       - treble down (-"-)
  |       - loudness on/off (80/50 bass&treble)('80' is configurable with an initial bass/treble value (!=50))
 alt-'.'  - soundcard volume up   (doesn't work on every cards (ie:GUS))
 alt-','  - soundcard volume down

  F1      - display help window (keyboard controls)

   ALT-F9 - 25/50 lines mode switch
  CTRL-F9 - full screen playlist-editor mode (analyser and browser are disabled)
 SHIFT-f9 - equal to ALT-F9 + CTRL-F9

 NUMBER   - type a playlist-entry number (1-9999)(jump to n. song)(type zeroes at begin to start songs faster)(ie: 05,005,0005; depending on how many files are in the list (>10,>100,>1000))


 Unmapped keyboard functions (configurable in mpxplay.ini):

  KeyCRewind      - continuous rewind (press again key to stop it)
  KeyCForward     - -"-        forward
  KeySkipAlbL1Back- skip back one album-group (like SkipAlbumBack, but with 1 directory level higher)
  KeySkipAlbL1Frwd- skip forward -"-
  KeySkipAlbL2Back- skip back one album-group (like SkipAlbumBack, but with 2 directory level higher)
  KeySkipAlbL2Frwd- skip forward -"- (note: put all audio files in the same (sub)directory level, else these functions will not work properly)
  KeyEditCpySngEhl- copy song (entry) and move/shift it to editorhighline position and skip editorhighline
  KeyLCDpage0,KeyLCDpage1,KeyLCDpage2,KeyLCDpage3 - select LCD page 0,1,2 or 3
  KeyID3winscroll - on/off scroll of id3-window if artist:title is too long (enabled by default)
  KeyShutDownExit - exit and shutdown computer (ATX only)

 Playlist-editor keys:
  TAB               : change playlist editor side
  Up & Down arrows  : move cursor up/down 1 song in the playlist
  PageUp,PageDown   :       -"-           1 editor page (1 page=30 song on the default screen)
  ALT-gray-PgUp/PgDn:       -"-           1 album (subdir difference in the playlist)
  Home,End          :       -"-           to the top/bottom of playlist
  CTRL-PgUp/PgDn    : move to updir/subdir (in directory browser) or uplist/sublist (in playlist)
  CTRL-'\'          : jump to root dir/list (directory browser/sublist structure)
  CTRL-gray-'/','*' : jump to prev/next paralell playlist (using sublists) or directory (in directory browser)

  ENTER             : start the selected song OR load playlist OR change dir/sublist OR add file/list to jukebox
  CTRL-ENTER        : select/unselect next song (and set playing side)

  white-Ins         : select/unselect song/list/dir
  ALT-gray-'+'      : select songs by filemask or metadata (works similar like search + wildchars are supported; "*." selects directories only)
  ALT-gray-'-'      : unselect songs
  ALT-gray-'*'      : invert selection
  CTRL-gray-'+'     : select all songs
  CTRL-gray-'-'     : unselect all songs

  F5                : add (selected) song(s)/playlist(s)/subdir(s) to the other side (loads/expands playlists/subdirs)
  gray-Ins          : add highlighted song/playlist/subdir to the other side
  SHIFT-F5          : copy song/list/subdir name to the other side (do not load/expand list/dir) (usefull for creating sublist)
  SHIFT-F6          : move playlist entries to the other side
  CTRL-Ins          : copy all playlist entries to the other side
  F8                : delete selected song(s) from the playlist
  Del               : delete highlighted song from the playlist
  CTRL-Del          : clear playlist (usually the right side)
  CTRL-UP / DOWN    : shift (drag and move) a song up/down in playlist

  ALT-F1,ALT-F2     : select drive or side type (drive,playlist,jukebox)
  F2                : save playlist (in selected filename/type) of current editorside (not playlist side!)
  F3                : show detailed fileinfos of highlighted song or summary of selected files or subdirectories
  F4                : edit id3tag or index (highlighted song or group of selected songs)
  ALT-F5            : copy file(s) on disk
  ALT-F6            : move/rename file(s) on disk
  F7                : make directory on disk (in directory browser only)
  ALT-F8            : delete file(s) from disk
  (note: filemask: "*.*" = supported files, "*.?*" = all files)
  ('*' is handled at end of name or extension only (ie: track*.m?*); tr*ck.*p3 format is not supported)

                      set primary key and re-order playlist on current editorside
  CTRL-F1           : orfer by path\filenames and disable sorting (sort directory entries only in browser/dir-scan)
  CTRL-F2           : order by artist /title/pathfile (1./2./3. keys) or in commander mode: by fileextension/filename
  CTRL-F3           : order by title  /artist/pathfile                or in commander mode: by filesize/pathfile
  CTRL-F4           : order by time   /artist/title/pathfile          or in commander mode: by filedate/pathfile

  ALT-LEFT / RIGHT  : modify size of playlist-editorsides (horizontal)
  ALT-UP / DOWN     : resize playlist editor / song browser (vertical)

 Searching in the editor (searches in all metadatas and filenames):
  ALT-'a'-'z'     : init search (you must use ALT at the 1st character only)
  Backspace       : clear back
  Up/Down Arrows  : search (again) backward/forward in the playlist
  ':'-char        : artist:title separator (to search in artist and title only)
  ESC             : exit from the search
  Executed keys in search (exit from the search, execute function): F5,TAB,ENTER,ctrl-ENTER

 Mouse buttons:
  left button (used to desktop functions by default (reconfigurable in mpxplay.ini)):
   - one click on song-position line () to seek in song (to set a new position)
   - one click in the playlist to move the editor highline (and change editorside)
   - double click in the playlist to start a new (selected) song
   - one click on song-browser to skip to (select and start) a new song
   - press + move vertically : scroll playlist

  right button:  seek forward / skip song  (without keymap: select/unselect song)
  center button: seek backward / skip back-like-cd (press/click)
  wheel up/down: editorhighline up/down  (http://cutemouse.sourceforge.net)
