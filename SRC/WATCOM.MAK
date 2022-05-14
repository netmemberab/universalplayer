#*****************************************************************************
# Makefile for compiling libraries under DJGPP
# ----------------------------------------------------------------------------
# created: 26.10.1996
#*****************************************************************************
.EXPORT_ALL_VARIABLES:

SYSTEM=DOS

# compiler depended extra compiling switches:
#ECOMP=-Wall -Wbad-function-cast -Wcast-qual -Wpointer-arith -Wshadow \
#   -Wstrict-prototypes -Wtraditional -Wwrite-strings -nostdinc

# architecture depended optimzing switch:
# OPTIMIZE=-O6

#MAIN_OPTS=-c -g $(DEBUG_OPT) -I$(PLAYER_INC) -D__DOS__
MAIN_OPTS = -i=<$(PLAYER_INC)>
#LINK_OPTS=$(OUT_MAP) $(NO_DEBUGINFO)

export MAIN_OPTS

#------------------------------------------------
# architecture depended additional objects
#
ARCH_TYPE=arch.dos
DMA=pm_dma.o
IRQ=dos_irq.o
