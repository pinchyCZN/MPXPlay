@echo off
echo ------------- Creating soundcard library for DOS version ----------------

del au_cards.lib > NUL
del *.err > NUL

wmake -h -f makedos

wlib -q au_cards.lib +au_cards.obj
wlib -q au_cards.lib +dmairq.obj
wlib -q au_cards.lib +pcibios.obj
wlib -q au_cards.lib +ac97_def.obj

wlib -q au_cards.lib +sc_cmi.obj
wlib -q au_cards.lib +sc_e1371.obj
wlib -q au_cards.lib +sc_ich.obj
wlib -q au_cards.lib +sc_inthd.obj
wlib -q au_cards.lib +sc_sbliv.obj
wlib -q au_cards.lib +sc_sbl24.obj
wlib -q au_cards.lib +sc_sbxfi.obj
wlib -q au_cards.lib +sc_via82.obj

wlib -q au_cards.lib +sc_ess.obj
wlib -q au_cards.lib +sc_gus.obj
wlib -q au_cards.lib +sc_sb16.obj
wlib -q au_cards.lib +sc_sbpro.obj
wlib -q au_cards.lib +sc_wss.obj
wlib -q au_cards.lib +sc_midas.obj

wlib -q au_cards.lib +sc_wav.obj
wlib -q au_cards.lib +sc_null.obj

del *.obj > NUL
del *.bak > NUL
