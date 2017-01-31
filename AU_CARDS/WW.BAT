@echo off
echo ------------- Creating soundcard library for Win32 version --------------

del au_cards.lib > NUL
del *.err 2> NUL

wmake -h /f makewin

wlib -q au_cards.lib +au_cards.obj
wlib -q au_cards.lib +dmairq.obj

wlib -q au_cards.lib +sc_wav.obj
wlib -q au_cards.lib +sc_null.obj

wlib -q au_cards.lib +sc_winds.obj
wlib -q au_cards.lib +sc_winwo.obj

del *.obj > NUL
del *.bak > NUL
