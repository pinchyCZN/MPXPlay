@echo off
del *.err
wmake -h
del mp4ff.lib
rem wlib -q mp4ff.lib +DRMS.OBJ
wlib -q mp4ff.lib +MP4ATOM.OBJ
wlib -q mp4ff.lib +MP4FF.OBJ
wlib -q mp4ff.lib +MP4META.OBJ
wlib -q mp4ff.lib +MP4SAMPL.OBJ
wlib -q mp4ff.lib +MP4UTIL.OBJ

del *.obj
del *.bak
