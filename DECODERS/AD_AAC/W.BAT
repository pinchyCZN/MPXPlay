@echo off
del aacdec.lib
del *.err 2> NUL
wmake -h
rem goto end
wlib -q aacdec.lib +BITS.OBJ
wlib -q aacdec.lib +CFFT.OBJ
wlib -q aacdec.lib +COMMON.OBJ
wlib -q aacdec.lib +DECODER.OBJ
rem wlib -q aacdec.lib +DITHER.OBJ
wlib -q aacdec.lib +DRC.OBJ
rem wlib -q aacdec.lib +ERROR.OBJ
wlib -q aacdec.lib +FILTBANK.OBJ
wlib -q aacdec.lib +HCR.OBJ
wlib -q aacdec.lib +HUFFMAN.OBJ
wlib -q aacdec.lib +IC_PREDI.OBJ
wlib -q aacdec.lib +LT_PREDI.OBJ
wlib -q aacdec.lib +MDCT.OBJ
wlib -q aacdec.lib +MP4.OBJ
wlib -q aacdec.lib +MS_IS.OBJ
rem wlib -q aacdec.lib +MS.OBJ
rem wlib -q aacdec.lib +IS.OBJ
wlib -q aacdec.lib +OUTPUT.OBJ
wlib -q aacdec.lib +PNS.OBJ
wlib -q aacdec.lib +PULSE.OBJ
wlib -q aacdec.lib +RVLC.OBJ
wlib -q aacdec.lib +SBR_DCT.OBJ
wlib -q aacdec.lib +SBR_DEC.OBJ
wlib -q aacdec.lib +SBR_E_NF.OBJ
wlib -q aacdec.lib +SBR_FBT.OBJ
wlib -q aacdec.lib +SBR_HFAD.OBJ
wlib -q aacdec.lib +SBR_HFGE.OBJ
rem wlib -q aacdec.lib +SBR_HUFF.OBJ
wlib -q aacdec.lib +SBR_QMF.OBJ
wlib -q aacdec.lib +SBR_SYNT.OBJ
wlib -q aacdec.lib +SBR_TF_G.OBJ
wlib -q aacdec.lib +SPECREC.OBJ
rem wlib -q aacdec.lib +SSR.OBJ
rem wlib -q aacdec.lib +SSR_FB.OBJ
rem wlib -q aacdec.lib +SSR_IPQF.OBJ
wlib -q aacdec.lib +SYNTAX.OBJ
wlib -q aacdec.lib +TNS.OBJ

:end

del *.obj
del *.bak
