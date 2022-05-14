.AUTODEPEND

.PATH.obj = ARCH\DOS\RMODE

#		*Translator Definitions*
CC = tcc +PLAYER.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = ..\..\LIB;..\LIB
INCLUDEPATH = ..\..\INCLUDE;..\INCLUDE;MODULES;DEVICES;DISPLAY;ARCH


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 player.obj \
 module.obj \
 mod.obj \
 s3m.obj \
 xm.obj \
 device.obj \
 filter.obj \
 dos_sb.obj \
 d_conio.obj \
 endian.obj \
 rm_dma.obj \
 irq.obj

#		*Explicit Rules*
arch\dos\rmode\player.exe: player.cfg $(EXE_dependencies)
  $(TLINK) /x/d/L$(LIBPATH) @&&|
c0l.obj+
arch\dos\rmode\player.obj+
arch\dos\rmode\module.obj+
arch\dos\rmode\mod.obj+
arch\dos\rmode\s3m.obj+
arch\dos\rmode\xm.obj+
arch\dos\rmode\device.obj+
arch\dos\rmode\filter.obj+
arch\dos\rmode\dos_sb.obj+
arch\dos\rmode\d_conio.obj+
arch\dos\rmode\endian.obj+
arch\dos\rmode\rm_dma.obj+
arch\dos\rmode\irq.obj
player
		# no map file
emu.lib+
mathl.lib+
cl.lib
|


#		*Individual File Dependencies*
player.obj: player.cfg player.cc 
	$(CC) -c player.cc

module.obj: player.cfg modules\module.cc 
	$(CC) -c modules\module.cc

mod.obj: player.cfg modules\mod.cc 
	$(CC) -c modules\mod.cc

s3m.obj: player.cfg modules\s3m.cc 
	$(CC) -c modules\s3m.cc

xm.obj: player.cfg modules\xm.cc 
	$(CC) -c modules\xm.cc

device.obj: player.cfg devices\device.cc 
	$(CC) -c devices\device.cc

filter.obj: player.cfg devices\filter.cc 
	$(CC) -c devices\filter.cc

dos_sb.obj: player.cfg devices\dos_sb.cc 
	$(CC) -c devices\dos_sb.cc

d_conio.obj: player.cfg display\d_conio.cc 
	$(CC) -c display\d_conio.cc

endian.obj: player.cfg arch\endian.cc 
	$(CC) -c arch\endian.cc

rm_dma.obj: player.cfg arch\dos\rm_dma.cc 
	$(CC) -c arch\dos\rm_dma.cc

irq.obj: player.cfg arch\dos\irq.cc 
	$(CC) -c arch\dos\irq.cc

#		*Compiler Configuration File*
player.cfg: turboc.mak
  copy &&|
-ml
-2
-j10
-v
-y
-G
-O
-Z
-k-
-r-
-d
-h
-vi-
-H=PLAYER.SYM
-w-ret
-w-nci
-w-inl
-w-par
-w-cpt
-w-dup
-w-pia
-w-rvl
-w-ill
-w-sus
-w-ext
-w-ias
-w-ibc
-w-pre
-w-nst
-nARCH\DOS\RMODE
-I$(INCLUDEPATH)
-L$(LIBPATH)
-D__DOS__
-P.C
-Ff
| player.cfg


