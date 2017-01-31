@echo off
echo --------------- Creating DECODERS library for DOS version ---------------

del decoders.lib > NUL
del *.err 2> NUL

wmake -h -f makedos
wlib -q decoders.lib +decoders.obj
wlib -q decoders.lib +ad_aac.obj
wlib -q decoders.lib +ad_ac3.obj
wlib -q decoders.lib +ad_alac.obj
wlib -q decoders.lib +ad_mp3.obj
wlib -q decoders.lib +ad_pcm.obj
wlib -q decoders.lib +ad_ffmpg.obj
wlib -q decoders.lib +ad_vorbi.obj

del *.obj > NUL
del *.bak > NUL
