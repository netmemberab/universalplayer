/****************************************************************************
   Player Main Header
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 15.10.1996
****************************************************************************/

#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "mtypes.h"

#define __PLAYER_VERSION__   0x200

typedef SDWORD DeviceID;
typedef SDWORD InstrumentID;
typedef SDWORD InstrumentType;
typedef SDWORD ResourceID;
typedef SDWORD ModuleID;
typedef SDWORD Status;
typedef void * DeviceOptionsPtr;
typedef void * DeviceSettingsPtr;

// Default IDs: -------------------------------------------------------------
#define NO_DEVICE      0
#define NO_INSTRUMENTS 0
#define NO_RESOURCE    0
#define NO_MODULE      0
#define NO_OPTIONS     NULL
#define NO_SETTINGS    NULL
#define NO_INSTRID     0x3fff

// Status values: -----------------------------------------------------------
#define NO_ERROR         0
#define ERRORLOADING    -1
#define BADMUSICFILE    -2
#define NO_MEMORY       -3
#define NO_SUPPORT      -4
#define DEVICE_FAILED   -5
#define NO_RESOURCES    -6
#define BAD_ID		-7
#define OUT_OF_RANGE    -8
#define BAD_PARAMETER   -9
#define NO_MIXER	-10

// structures for global interface ------------------------------------------

#define MAX_COMMANDS 64
struct DeviceCommand {
		int             Channel;
		int 		Changes;
		int		Dependience;
		DWORD 		Frequency;
		InstrumentID 	instrid;
		DWORD           Offset;
		BYTE		Volume;
		BYTE		Panning;
	};

struct DeviceCommandsStruct {
		DWORD timerbase;    // Hz of "timer" <- set by device!
		WORD  countervalue; // number of timer ticks until next commands
		int no_commands;
		struct DeviceCommand Commands[MAX_COMMANDS];
	};

// Command-'Changes' field:
  #define CHG_INSTRUMENT      1
  #define CHG_FREQUENCY       2
  #define CHG_VOLUME          4
  #define CHG_PANNING         8
  #define CHG_SETOFFSET      16
  #define CHG_RESTART        32
  #define CHG_STOPPLAY       64
  #define CHG_CONTINUE      128

// Command-'Dependience' field:
  #define IS_PLAYING          1

// Command-'Panning' field:
  #define PAN_LEFT            0
  #define PAN_CENTER        128
  #define PAN_RIGHT         255

// Command-'Volume' field:
  #define VOL_ZERO            0
  #define VOL_MAX           255

#include "registry.h"
#include "plyctrl.h"

#endif
