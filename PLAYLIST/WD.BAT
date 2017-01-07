@echo off
echo --------------- Creating PLAYLIST library for DOS version ---------------

del playlist.lib
del *.err

wmake -h -f makedos
wlib -q playlist.lib +playlist.obj
wlib -q playlist.lib +chkentry.obj
wlib -q playlist.lib +diskfile.obj
wlib -q playlist.lib +editlist.obj
wlib -q playlist.lib +fileinfo.obj
wlib -q playlist.lib +id3list.obj
wlib -q playlist.lib +jukebox.obj
wlib -q playlist.lib +loaddir.obj
wlib -q playlist.lib +loadlist.obj
wlib -q playlist.lib +loadsub.obj
wlib -q playlist.lib +randlist.obj
wlib -q playlist.lib +savelist.obj
wlib -q playlist.lib +skiplist.obj
wlib -q playlist.lib +sortlist.obj
wlib -q playlist.lib +textconv.obj

del *.obj
del *.bak
