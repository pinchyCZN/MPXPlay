@echo off
echo ---------------- Creating MIXER library for DOS version -----------------

del au_mixer.lib > NUL
del *.err 2> NUL

wmake -h -f makedos

wlib -q au_mixer.lib +au_mixer.obj
wlib -q au_mixer.lib +analiser.obj
wlib -q au_mixer.lib +mx_crosf.obj
wlib -q au_mixer.lib +mx_avol.obj
wlib -q au_mixer.lib +mx_volum.obj
wlib -q au_mixer.lib +cutsilen.obj
wlib -q au_mixer.lib +cv_bits.obj
wlib -q au_mixer.lib +cv_freq.obj
wlib -q au_mixer.lib +cv_chan.obj
wlib -q au_mixer.lib +fx_surr.obj
wlib -q au_mixer.lib +fx_tone.obj

del *.obj > NUL
del *.bak > NUL
