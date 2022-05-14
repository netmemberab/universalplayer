#*****************************************************************************
# Makefile for compiling libraries under Solaris-G++
# ----------------------------------------------------------------------------
# Created: 26.10.1996
#*****************************************************************************
# if you haven't got g++ on your system try using another C++ compiler, but
# that may fail...

.EXPORT_ALL_VARIABLES:

SYSTEM=SOLARIS 
OPTIMIZE=-funroll-loops
MAIN_OPTS=-c -D__LITTLEENDIAN__ -D__SOLARIS__ ${XOPT} ${OPTIMIZE}
COPTS=-c
DLIBS=-lcurses -ltermcap
CC=gcc
AUDIO_O=solrs_au.o mixdev.o filter.o
AUDIO_H=solrs_au.h mixdev.h filter.h
DISPLAY=d_curses
