@echo off
del *.err 2> NUL
wmake -h /f makemdct
wmake -h /f makefile
del vorbis.lib
rem wlib -q vorbis.lib +ANALYSIS.OBJ
rem wlib -q vorbis.lib +BITRATE.OBJ
wlib -q vorbis.lib +BITWISE.OBJ
wlib -q vorbis.lib +BLOCK.OBJ
wlib -q vorbis.lib +CODEBOOK.OBJ
rem wlib -q vorbis.lib +ENVELOPE.OBJ
wlib -q vorbis.lib +FLOOR0.OBJ
wlib -q vorbis.lib +FLOOR1.OBJ
wlib -q vorbis.lib +FRAMING.OBJ
wlib -q vorbis.lib +INFO.OBJ
rem wlib -q vorbis.lib +LPC.OBJ
wlib -q vorbis.lib +LSP.OBJ
wlib -q vorbis.lib +MAPPING0.OBJ
wlib -q vorbis.lib +MDCT.OBJ
rem wlib -q vorbis.lib +PSY.OBJ
rem wlib -q vorbis.lib +REGISTRY.OBJ
wlib -q vorbis.lib +RES0.OBJ
wlib -q vorbis.lib +SHARBOOK.OBJ
rem wlib -q vorbis.lib +SMALLFT.OBJ
wlib -q vorbis.lib +SYNTHESI.OBJ
rem wlib -q vorbis.lib +TIME0.OBJ
wlib -q vorbis.lib +WINDOW.OBJ

del *.obj
del *.bak
