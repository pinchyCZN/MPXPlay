@echo off
del mp3dec.lib
del *.err
wasm -q -w2 l3_deq.asm
wasm -q -w2 l3_mdct.asm
wasm -q -w2 synth.asm
wmake -h
wlib -q mp3dec.lib +l3_deq.obj
wlib -q mp3dec.lib +l3_mdct.obj
wlib -q mp3dec.lib +synth.obj
wlib -q mp3dec.lib +layer3.obj
wlib -q mp3dec.lib +layer2.obj

del *.obj
del *.bak
