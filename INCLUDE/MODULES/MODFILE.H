/*****************************************************************************
   MOD    support routines for music file format MOD
   - 15 instrument MODs are not supported
   - MOD-types: FastTracker1.0 ,NoiseTracker , TakeTracker, Octamed
     and other well known (IDs: FLT,OCTA,xxCH,xCHN,M.K.,M!K!)
   - patterns will be packed while loading ! for less memory use.
     (I wrote this a long time ago...under DOS, you know ?)
  --------------------------------------------------------------------------
   (c) copyright 1995,96 by A.Baresel
    Created on 21.1.1996
    Rewritten for new interface on 29.10.1996
*****************************************************************************/
// sorry for ugly coding style, but this code is quite old - it was used in
// a former mod-player (written in 1995 - the good'ole times ;)

// to use MOD_loader:
// enter  'REGISTER( LOAD_MOD , MOD_c )'
// call   'PlayerControl.LoadModule( devid, LOAD_MOD , ... )'

#if !defined(__MOD_H__)
#define __MOD_H__

#include "player.h"
#include "module.h"
#include <stdio.h>

#define MAX_MOD_CHANNELS 32

/* ++++++++++++++++ Class def. +++++++++++++++++++++++++++++++++++++++++++ */
class MOD_c: public ModuleClass {
private:
	FILE * modin;
public:
  MOD_c(FILE*f,DeviceClassPtr d):ModuleClass(d),modin(f) { load_module(); }
  ~MOD_c(void) { delete_module(); }
  Status StartModule();
  Status GetDeviceCommands(DeviceCommandsStruct&);

protected: 
/**************************************************************************/
/* ++++++++++++++++++ MOD specific types ++++++++++++++++++++++++++++++++ */

struct channel_t    {  int  period;
		       char finetune;  // last finetune setup
		       BYTE instr;
		       short volume;

		       // effect & parameter
		       WORD effect;
		       WORD useeffect;    /* converted effect number */
		       BYTE parameter;    /* last parameter != 0 */
		       BYTE continueEff;

		       // vibrato and tremolo
		       WORD VibtabPos;   // position in table 0..63
		       WORD TrmtabPos;   // position in table 0..63
		       BYTE VibwavNo;    // waveform (sinus/ramp/square)
		       BYTE TrmwavNo;    // waveform
		       BYTE VibDepth;
		       BYTE VibSpeed;     // last vibrato parameters

		       // portamentos
		       BYTE PortPara;      // last portamento parameters
		       BYTE Glisscontrol;  // Slide with glisscontrol
		       int Slideto;      // period value we slide to

		       // arpeggio stuff
		       BYTE ArpeggioPos;  // 0..2
		       int periods[3];

		       // for timing of some effects (e.g. retrigg,notecut)
		       BYTE ctick;
		       WORD numticks;
		       BYTE sInst;
		       WORD sPeriod;

		       // for pattern loops
		       BYTE inside;       // inside a loop ?
		       BYTE loopto;       // row to loop to
		       BYTE loops;        // number of loops

		       // for setofs effect
		       WORD startoffset;
		     } chn[MAX_MOD_CHANNELS];

// no allignment in next structur: packed structure !!
// tell your compiler this, if it does allign automaticly...

struct instrument_t      {  char  name[22];
			    WORD  smplen;
			    char  finetune;
			    BYTE  volume;
			    WORD  loopstart;
			    WORD  looplen;
			    BYTE  loopflag;
			  } Instrument[32];

        InstrumentID instrIDs[32]; // for each instrument an ID (device access)
	char songname[21];
	char Id[5];
	BYTE emptypatn[64];
	BYTE order[128];
	BYTE chnpan[32];
	BYTE * Pattern_p[256]; 

	short no_Pattern;
	short no_Channels;
	BYTE  no_Orders;
	short no_Instr;

	//---- extra variables for global effects for all channels
	//|  they are set in the channels, but handling follows after reading
	//|  all channels !!
	//-------------------
	BYTE patternbreak;
	BYTE pbreakto;
	BYTE patternloop;
	BYTE ploopto;
	BYTE positionjump;
	BYTE pjumpto;

	//---- internal playdata
	short c_order;
	short c_row;
	short c_tick;
	BYTE *c_position;     
	short c_speed;
	short c_tempo;
	BYTE  restart_pos;

	BYTE patterndelay;      // counter for pattern delay

	void   delete_module(void);    // delete MOD-module from memory
	void   load_module(void);      // load MOD-module
	short  load_pattern(WORD pos,WORD no);

	void   effect_handling(DeviceCommandsStruct*);
	void   doeffects_patterndly(DeviceCommandsStruct*);
	void   read_new_notes(DeviceCommandsStruct*);
	void   jumpto(char jp2);
	void   setup_channel(DeviceCommandsStruct*,unsigned char chnnr,
			      int period,short instr,short effno, short effpar);
};

#endif
