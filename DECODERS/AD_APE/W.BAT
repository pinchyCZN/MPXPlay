@echo off
del apedec.lib
del *.err
wmake -h
wlib -q apedec.lib +apeinfo.obj
wlib -q apedec.lib +dec_new.obj
wlib -q apedec.lib +dec_old.obj
wlib -q apedec.lib +maclib.obj
wlib -q apedec.lib +pred_new.obj
wlib -q apedec.lib +pred_old.obj
wlib -q apedec.lib +prepare.obj
wlib -q apedec.lib +unbitbas.obj
wlib -q apedec.lib +unbitnew.obj
wlib -q apedec.lib +unbitold.obj

del *.obj
del *.bak

