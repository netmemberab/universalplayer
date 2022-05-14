#*****************************************************************************
# Makefile for compiling libraries under SunOS-G++
# ----------------------------------------------------------------------------
# Created: 26.10.1996
#*****************************************************************************
# if you haven't got g++ on your system try using another C++ compiler, but
# that may fail

.EXPORT_ALL_VARIABLES:

SYSTEM=SUN 
MAIN_OPTS=-c -D__LITTLEENDIAN__ -D__SUN__ ${XOPT} ${OPTIMAL}
OPTIMIZE=-funroll-loops
COPTS=-c
DLIBS=-lcurses -ltermcap
CC=gcc
AUDIO_O=sun_au.o mixdev.o filter.o
AUDIO_H=sun_au.h mixdev.h filter.h
DISPLAY=d_curses
