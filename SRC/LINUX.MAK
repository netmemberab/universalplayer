#*****************************************************************************
# Makefile for compiling libraries under Linux-g++
# ----------------------------------------------------------------------------
# Created: 26.10.1996
#*****************************************************************************
.EXPORT_ALL_VARIABLES:

SYSTEM=LINUX 
OPTIMIZE=-O3 -m486

PLAYER_INCLUDES=-I$(PLAYER) -I$(PLAYER)/display -I$(PLAYER)/modules -I$(PLAYER)/devices -I$(PLAYER)/arch -I$(PLAYER)/arch/dos
MAIN_OPTS=-c -g -D__LINUX__ $(PLAYER_INCLUDES) ${DEBUG_OPT}

ARCH_LIBS=
ARCH_TYPE=arch.linux

CC=gcc
MAKE=make
