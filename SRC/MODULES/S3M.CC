/****************************************************************************
   S3M (ScreamTracker 3 module) reading functions
   note: all features of ST3.20 are supported (panning too)
   ehhhm, Retrigg-volsld,finetune not yet...
  --------------------------------------------------------------------------
  (c) copyright 1995,96 by A.Baresel

  created: 11.11.1995
****************************************************************************/

#include "s3m.h"
#include "module.ids"
#include "device.h"
#include "xendian.h"
#include "module.h"
#include <string.h>
#include <stdlib.h>

static const char * S3M_idstr=S3M_ID;

// --- Class def. -----------------------------------------------------------
static const WORD period_tab[]={1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907};

static int sinuswav[64]={  0, 24, 50, 74, 98,120,142,162,180,196,212,224, 234,244,250,252,
			   254,252,250,244,234,224,212,196,180,162,142,120, 98, 74, 50, 24,
			     0,-24,-50,-74,-98,-120,-142,-162,-180,-196,-212,-224,-234,-244,
			   -250,-252,-254,-252,-250,-244,-234,-224,-212,-196,-180,-162,-142,-120,
			   -98,-74,-50,-24
			  };

static int rampdwav[64]={255,246,238,229,221,212,204,195,187,178,170,161,153,144,136,127,
			   119,110,102, 93, 85, 76, 68, 59, 51, 42, 34, 23, 17,  8,  0,
			   -255,-246,-238,-229,-221,-212,-204,-195,-187,-178,-170,-161,-153,
			   -144,-136,-127,-119,-110,-102,-93,-85,-76,-68,-59,-51,-42,-34,-23,
			   -17,-8,0
			  };

static int squarwav[64]={  0, 70,128,140,160,180,200,215,235,243,247,250,255,255,255,255,255,
			   255,255,250,247,243,235,215,200,180,160,140,128, 70,  0,
			     0,-70,-128,-140,-160,-180,-200,-215,-235,-243,-247,-250,-255,-255,
			   -255,-255,-255,-255,-255,-250,-247,-243,-235,-215,-200,-180,-160,-140,
			   -128,-70, 0
			  };

static int (*wofstable[4])[64]={&sinuswav,&squarwav,&rampdwav,&sinuswav}; // no random wave !

void S3M_c::delete_module(void)
{ int no;
  trace("Remove S3M from memory.\n");
  if (Instrument_p) free(Instrument_p);
  for (no=0;no<no_Pattern;no++)
    if (Pattern_p[no]) free(Pattern_p[no]);
}

WORD S3M_c::getnoChannels()
{ return(16); 
  /* ---> check through all pattern and get highest channeln number is 
     really used <--- */
}

short S3M_c::startmodule(void)
{ int i;
  c_row=0;
  c_tick=0;
  c_order=0;
  c_position=Pattern_p[order[c_order]];
  c_speed=S3M_header.initialspeed;
  c_tempo=S3M_header.initialtempo;
  patterndelay=0;
  for (i=0;i<no_Channels;i++)
    Device->SetupChannel(i,CHG_PANNING,&chnpan[i]);
  memset(&(chn),0,sizeof(chn));
  return(0);
}

void readtill(FILE *_file,long pos)
{ unsigned long len,l2;
  len=ftell(_file);
  if (len>(l2=((DWORD)pos<<4)))
    { len=l2-len;while (!feof(_file) && len--) getc(_file); }
  else (void)fseek(_file, (long)pos<<4, SEEK_SET) ;
}

/*-----------------------------------------------------------------------------------**
** ------------------------ PATTERN READING WHILE PLAYING ... ---------------------- **
**-----------------------------------------------------------------------------------*/

void S3M_c::effect_handling(struct playinfo_t *playinfo)
{ // effects done every tick :
  int i;
  BYTE dl,dh;
  char d;
  WORD dx;
  WORD period;
  for (i=0;i<no_Channels;i++)
  { struct S3M_c::channel_t *S3M_chn=&chn[i];
    struct chninfo_t *MIX_chn=&(playinfo->chn[i]);

    switch (S3M_chn->command) // new effect numbers !

    #define volumeslide                                         \
	 if (!(S3M_chn->parameter&0xf))                         \
	 { /* volume slide up */                                \
	   d=S3M_chn->volume+(S3M_chn->parameter>>4);    \
	   if (d>63) d=63;                                    \
	   MIX_chn->volume=((S3M_chn->volume=d)*S3M_header.gvolume)/64;    \
	   MIX_chn->changes|=CHG_VOLUME;                        \
	 } else  /* S3M_chn->parameter>>4!=0xf */               \
	 { /* volume slide down */                              \
	   d=S3M_chn->volume-(S3M_chn->parameter&0xf);    \
	   if (d<0) d=0;                                        \
	   MIX_chn->volume=((S3M_chn->volume=d)*S3M_header.gvolume)/64;    \
	   MIX_chn->changes|=CHG_VOLUME;                        \
	 }

    { case  1: /* Volume slides ------------------------------------------------------- */
	 volumeslide;
	 break;
      case  2: /* Pitch down ---------------------------------------------------------- */
	 { dx = S3M_chn->period+((WORD)S3M_chn->parameter<<2);
	   S3M_chn->period=(dx>S3M_chn->lownote_border)?S3M_chn->lownote_border:dx;
	   MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
	   MIX_chn->changes|=CHG_FREQ;
	 }  
	 break;
      case  3: /* Pitch up ------------------------------------------------------------ */
	 { dx = S3M_chn->period-((WORD)S3M_chn->parameter<<2);
	   S3M_chn->period=(dx<S3M_chn->uppernote_border)?S3M_chn->uppernote_border:dx;
	   MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
	   MIX_chn->changes|=CHG_FREQ;
	 }  
	 break;
      case  9: /* Portamento + Volumeslide -------------------------------------------- */
	 volumeslide; // short after the portamento stuff:
      case  4: /* Portamento */
	 { period=S3M_chn->period;
	   dx=(WORD)S3M_chn->PortPara<<2;
	   if (S3M_chn->period>S3M_chn->Slideto)
	   {
	     period-=dx;
	     if (period<S3M_chn->Slideto)
	     { period=S3M_chn->Slideto;
	       S3M_chn->command=(S3M_chn->command==4)?0:1; /* end sliding, but maybe continue volume slide */
	     }
	   } else
	   { period+=dx;
	     if (period>S3M_chn->Slideto)
	     { period=S3M_chn->Slideto;
	       S3M_chn->command=(S3M_chn->command==4)?0:1; /* end sliding, but maybe continue volume slide */
	     }
	   }
           S3M_chn->period=period;

	   if (S3M_chn->Glisscontrol)
	   { // do glissando ... (round to nearest period value)
             BYTE i=0,j=0;
             WORD a,b,c;
             while (((16lu*period_tab[11])>>i)>period) i++;
             b=(16lu*period_tab[11])>>(i-1);
             while ((c=(16lu*period_tab[j])>>i)>period) { b=a;a=c;j++; }a=c;
             if (period-a<b-period) period=a; else period=b;
	   }
	   if (period)
	   { MIX_chn->changes|=CHG_FREQ;
	     MIX_chn->frequency=8363lu*1712lu/period;
	   }  
	   break;
	 }
      case  8: /* Vibrato + Volumeslide ----------------------------------------------- */
	 volumeslide; // short after the portamento stuff:
      case 14: /* Fine Vibrato -------------------------------------------------------- */
      case  5: /* Vibrato ------------------------------------------------------------- */
	 { period=0;
	   S3M_chn->VibtabPos=(S3M_chn->VibtabPos+S3M_chn->VibSpeed)&63;
	   if (S3M_chn->VibtabPos<32) 
	      dx=(*wofstable[S3M_chn->VibwavNo&0x3])[S3M_chn->VibtabPos];
	   else 
	      dx=(*wofstable[S3M_chn->VibwavNo&0x3])[63-S3M_chn->VibtabPos];
	   dx=(dx * S3M_chn->VibDepth)>>7;
	   if (S3M_chn->VibtabPos>31) dx=-dx;
	   period=dx;
	   if (S3M_chn->period)
	   { if (S3M_chn->command != 14) period<<=2; // normal vibrato 4times bigger
	     period+=S3M_chn->period;
	     MIX_chn->changes|=CHG_FREQ;
	     MIX_chn->frequency=8363lu*1712lu/period;
	   }
	 }
	 break;
      case  6: /* Tremor --------------------------------------------------------------- */ 
	       // check this again, it's unnessary to write such cryptic code ##############<<#####

	 if ((--S3M_chn->ctick)&0x10)
	 { BYTE i=(S3M_chn->ctick&0x20);
	   MIX_chn->volume=i?0:((S3M_chn->volume*S3M_header.gvolume)/64);
	   MIX_chn->changes=CHG_VOLUME;
	   S3M_chn->ctick=(S3M_chn->store&0xf)|i;
	   dh=(dl=S3M_chn->store)>>4;dl=((dl&0xf)<<4)|dh; /* exchange high and low nibble... */
	   S3M_chn->store=dl;
	 }
	 break;
      case  7: /* Arpeggio --------------------------------------------------------- */
	 S3M_chn->ArpeggioPos++;S3M_chn->ArpeggioPos%=3;
	 MIX_chn->frequency=8363lu*1712lu/S3M_chn->periods[S3M_chn->ArpeggioPos];
	 MIX_chn->changes|=CHG_FREQ;
	 break;
      case 10: /* Retrigg ---------------------------------------------------------- */
	 if (!S3M_chn->ctick--) // retrigg if zero
	 { S3M_chn->ctick=S3M_chn->store;
	   MIX_chn->changes|= CHG_SETOFS | CHG_DOMIX;
	   MIX_chn->offset=0;
	 }
	 break;
      case 11: /* Tremolo ---------------------------------------------------------- */
	 { char volume;
	   S3M_chn->TrmtabPos=(S3M_chn->TrmtabPos+(S3M_chn->parameter>>4))&63; // speed 
	   dx=(*wofstable[S3M_chn->TrmwavNo&0x3])[S3M_chn->TrmtabPos];
	   dx=(dx*(S3M_chn->parameter&0xf))>>7; // depth
	   volume=(BYTE)S3M_chn->volume + dx;
	   if (volume>63) volume=63;
	   if (volume<0) volume=0;
	   MIX_chn->changes=CHG_VOLUME;
	   MIX_chn->volume=(volume*S3M_header.gvolume)/64;
	   break;
	 }
      case 12: /* Notecut --------------------------------------------------------- */
	 if (!S3M_chn->ctick--)
	 { // note cut == (setvol=0)
	   MIX_chn->changes=CHG_VOLUME;
	   MIX_chn->volume=0;
	   S3M_chn->command=0;
	 }
	 break;
      case 13: /* note delay ------------------------------------------------------ */
	 if (!--S3M_chn->ctick) // start playing it if zero
	 { // setup instrument
	   if (S3M_chn->sInst)
	   { // recalc borders etc.
	     struct S3M_c::instrument_t *instrument=&((*Instrument_p)[S3M_chn->instr]);
	     S3M_chn->instr=MIX_chn->instno=S3M_chn->sInst;
	     MIX_chn->volume=((S3M_chn->volume=instrument->defaultvol)*S3M_header.gvolume)/64;
	     // First only for non Amiga periods (C-0 to C-7)
	     S3M_chn->lownote_border  =8363lu*16lu*1712lu/instrument->c2speed;
	     S3M_chn->uppernote_border=8363lu*16lu*13lu/instrument->c2speed;
	     MIX_chn->changes|=CHG_VOLUME;
	     S3M_chn->startoffset=0;
	     if (S3M_chn->sNote==255) MIX_chn->changes|=CHG_RESTART;
	   } else MIX_chn->instno=S3M_chn->instr;
	   // setup note:
	   { BYTE note=S3M_chn->sNote;
	     S3M_chn->period=(8363lu*(16lu*period_tab[(int)(note&0xf)]>>(note>>4)))
		/((*Instrument_p)[S3M_chn->instr].c2speed);
	     MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
	     S3M_chn->note=note;
	     if (S3M_chn->sInst) 
	       MIX_chn->changes|=CHG_INSTR|CHG_FREQ;
	     else
	       { MIX_chn->changes|=CHG_FREQ|CHG_SETOFS|CHG_DOMIX;
		 MIX_chn->offset=S3M_chn->startoffset;
	       }
	   }
	   S3M_chn->command=0;
	 }
	 break;
    }
  }
  c_tick--;
}

#define reloadif0  if (!effpar) effpar=S3M_chn->parameter; else S3M_chn->parameter=effpar;
#define storeifn0  if (effpar) S3M_chn->parameter=effpar;

/*------------------------------------------------------------------------------
   This function is quite long. It's divided in three parts: effects before
   setting new note/instrument/volume, setup new values, effects after new 
   note etc.
------------------------------------------------------------------------------*/

void S3M_c::setup_channel(struct playinfo_t *playinfo,BYTE chnnr,
			  BYTE note,BYTE instr,BYTE vol,BYTE effno, BYTE effpar)

{ struct S3M_c::channel_t *S3M_chn=&chn[chnnr];
  struct chninfo_t *MIX_chn=&(playinfo->chn[chnnr]);
  // convert effect numbers for easier effect handling (every tick)
  unsigned int converteff[27]={0,0,0,0,1,2,3,4,5,6,7,8,9,0,0,0,0,10,11,0,0,15,0,0,0,0,0};
  unsigned int conv2eff[16]  ={0,0,0,0,0,0,0,0,0,0,0,0,12,13,0,0};
  // effects have to be processed before set instrument,note,volume
  switch (effno)
  { case  0: case 13: case 14: case 16: case 23:
    case 24: case 25: case 26: /* no effects ... but check parameter ! */
      storeifn0; break;
    case  1:/*A - set speed */
       storeifn0;
       if (effpar) c_speed=effpar;
       break;
    case  2:/*B - jump to order */
       storeifn0;
       positionjump=1;
       pjumpto=effpar;
       break;
    case  3:/*C - pattern break */
       storeifn0;
       patternbreak=1;
       pbreakto=10*(effpar>>4)+(effpar&0xf);
       break;
    case  7:/*G - portamento */
       storeifn0;
       if (effpar) S3M_chn->PortPara=effpar;
       if (note<0xfe) // change note to slide to
       { if (instr) S3M_chn->instr=instr;
	 S3M_chn->Slideto=(8363lu*(16lu*period_tab[note&0xf]>>(note>>4)))/((*Instrument_p)[S3M_chn->instr].c2speed);
	 note=0xff;
       } else if (!S3M_chn->Slideto) effno=0; // don't start porta ...
       break;
    case  8: case 21: /*H - vibrato & U - fine vibrato */
       storeifn0;
       if (effpar) // new vibrato parameters
       { if (effpar&0xf0) S3M_chn->VibSpeed=(unsigned char)effpar>>4;
	 S3M_chn->VibDepth=effpar&0xf;
       }
       if ((S3M_chn->VibwavNo&0x4)&&S3M_chn->command!=5&&S3M_chn->command!=15&&S3M_chn->command!=8)
       // no (vibrato || vibrato+volsld || finevibrato) before
	 S3M_chn->VibtabPos=0; // reset wave position
       break;
    case  9:/*I - Tremor */
       storeifn0;
       if (effpar) // new tremor parameters
       { S3M_chn->ctick=effpar&0xf;
	 BYTE dl=effpar,dh=dl>>4;dl=((dl&0xf)<<4)|dh; /* exchange high and low nibble...*/
	 S3M_chn->store=dl;
       }
       if (S3M_chn->command!=6)
       { // Tremor does start
	 MIX_chn->volume=(S3M_chn->volume*S3M_header.gvolume)/64;
	 MIX_chn->changes|=CHG_VOLUME;
       } else // continue tremor ... (hmm right in that way ?)
       { if ((--S3M_chn->ctick)&0x10)
	 { char i=(S3M_chn->ctick&0x20);
	   MIX_chn->volume=i?0:((S3M_chn->volume*S3M_header.gvolume)/64);
	   MIX_chn->changes=CHG_VOLUME;
	   S3M_chn->ctick=(S3M_chn->store&0xf)|i;
	   BYTE dl=S3M_chn->store,dh=dl>>4;dl=((dl&0xf)<<4)|dh;
	   S3M_chn->store=dl;
	 }
       }
       break;
    case 12: /*L - portamento + volsld */
       // prepare Portamento
       if (note<0xfe) // change note to slide to
       { if (instr) S3M_chn->instr=instr;
	 S3M_chn->Slideto=(8363lu*(16lu*period_tab[note&0xf]>>(note>>4)))/((*Instrument_p)[S3M_chn->instr].c2speed);
	 note=0xff;
       } else if (!S3M_chn->Slideto) effno=0; // don't start porta ...
       // changes for simultane volume slide later
       break;
    case 18:/*R - Tremolo */
       reloadif0;
       if (S3M_chn->command!=11||S3M_chn->TrmwavNo&0x4||note<254)
       { // start a Tremolo with reseting table position
	 S3M_chn->TrmtabPos=0;
       }
       break;
    case 19:/*S - special effects: */
      { S3M_chn->parameter=effpar;
	switch (effpar>>4)
	{ case 1: /* S1 - set glisscontrol */
	     S3M_chn->Glisscontrol=effpar&0xf;
	     break;
	  case 2: /* S2 - set finetune */
	     break;
	  case 4: /* S4 - set vibrato waveform */
	     S3M_chn->VibwavNo=effpar&0xf;
	     break;
	  case 5: /* S5 - set tremolo waveform */
	     S3M_chn->TrmwavNo=effpar&0xf;
	     break;
	  case 8: /* S8 - set pan position */
	     MIX_chn->panposition=(effpar&0xf)<<4;
	     MIX_chn->changes|=CHG_PANNING;
	     break;
	  case 11: /* SB - pattern loop */
	     if (!(effpar&0xf)) // set loop point
	       loopto=c_row;
	     else
	     { if (!inside)
	       { inside=1;
		 loops=effpar&0xf;
	       }
	       if (loops)
	       { loops--;
		 patternloop=1;
		 ploopto=loopto;
	       } else
	       { inside=0;
		 loopto=c_row+1;
	       }
	     }
	     break;
	  case 12: /* SC - note cut */
	     if (!effpar&0xf) effno=0; // no note cut !
	     else S3M_chn->ctick=effpar&0xf;
	     break;
	  case 13: /* SD - note delay */
	     if (note>=254) { effno=0;effpar=0;break; }
	     S3M_chn->ctick=effpar&0xf;
	     S3M_chn->sInst=instr;instr=0;
	     S3M_chn->sNote=note;note=255;
	     if (!S3M_chn->ctick) effno=0; // no delay !
	     break;
	  case 14: /* SE - pattern delay */
	     patterndelay=effpar&0xf;
	     break;
	}
	break;
      }
    case 20:/*T - set tempo */
       storeifn0;if (effpar) c_tempo=effpar; break;
    case 22:/*V - set global volume */
       storeifn0;newgvol=effpar; break;
  }
  // setup instrument
  if (instr&&(*Instrument_p)[instr].typ==1) // a new _sample_ instrument
  { // recalc borders etc.
    struct instrument_t *instrument=&((*Instrument_p)[instr]);
    S3M_chn->instr=MIX_chn->instno=instr;
    MIX_chn->volume=((S3M_chn->volume=instrument->defaultvol)*S3M_header.gvolume)/64;
    // First only for non Amiga periods (C-0 to C-7)
    S3M_chn->lownote_border  =8363lu*16lu*1712lu/instrument->c2speed;
    S3M_chn->uppernote_border=8363lu*16lu*13lu/instrument->c2speed;
    MIX_chn->changes|=CHG_VOLUME;
    S3M_chn->startoffset=0;
    if (note==255) MIX_chn->changes|=CHG_RESTART;
  } else MIX_chn->instno=S3M_chn->instr;
  // setup note:
  if (note==254) /* 'keyoff' in S3M, like a note cut 0 */ 
      MIX_chn->changes|=CHG_STOPMIX;
  else
  if (note!=255) // a new note
  {
    S3M_chn->period=(8363lu*(16lu*period_tab[note&0xf]>>(note>>4)))/((*Instrument_p)[S3M_chn->instr].c2speed);
    MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
    S3M_chn->note=note;
    if (instr) MIX_chn->changes|=CHG_INSTR|CHG_FREQ;
    else
    { MIX_chn->changes|=CHG_FREQ|CHG_SETOFS|CHG_DOMIX;
      MIX_chn->offset=S3M_chn->startoffset;
    }
  }
  else if ((S3M_chn->command==5||S3M_chn->command==8||S3M_chn->command==15)&&effno!=8&&effno!=11&&effno!=21)
  { // vibrato ends ... refresh frequency
    MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
    MIX_chn->changes|=CHG_FREQ;
  } else
  if (S3M_chn->command==7&&effno!=10)
  { // arpeggio ends ... refresh frequency
    MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
    MIX_chn->changes|=CHG_FREQ;
  }
  if (vol!=0xff) { MIX_chn->volume=((S3M_chn->volume=vol)*S3M_header.gvolume)/64;
		   MIX_chn->changes|=CHG_VOLUME;}
  S3M_chn->command=converteff[effno];
  if (effno==19) // special effect
      S3M_chn->command=conv2eff[effpar>>4];
  // commands can or need to be processed after setting note and instr
  switch (effno)
  { case 4:  /* D - volume slides */
    case 11: /* K - vibrato + volsld */
    case 12: /* L - portamento volume slide */
       reloadif0;
       if (!(effpar&0xf)||!(effpar>>4)||((effpar>>4)|(effpar&0xf))!=0xf)
       { /*volume slide up || volume slide down */
	 /* do nothing ... */
       } else
       if ((effpar&0xf)==0xf)
       { // fine volume slide up
	 BYTE dl=S3M_chn->volume+(effpar>>4);
	 if (dl>63) dl=63;
	 MIX_chn->volume=((S3M_chn->volume=dl)*S3M_header.gvolume)/64;
	 MIX_chn->changes|=CHG_VOLUME;
	 S3M_chn->command=0; // fine ...
       } else
       { // fine volume slide down
	 short dl=S3M_chn->volume-(effpar&0xf);
	 if (dl<0) dl=0;
	 MIX_chn->volume=((S3M_chn->volume=dl)*S3M_header.gvolume)/64;
	 MIX_chn->changes|=CHG_VOLUME;
	 S3M_chn->command=0; // fine ...
       }
       break;
    case 5: /*E - pitch slides down */
       reloadif0;
       if (effpar>0xDF) // extra down slides ...
       { WORD dx;
	 S3M_chn->command=0; // do not slide every tick ...
	 if (effpar<0xF0)
	 // fine slide down (4 times faster than Xfine)
	   dx=S3M_chn->period+((effpar&0xf)<<2);
	 else
	 // extra fine slide down
	   dx=S3M_chn->period+(effpar&0xf);
	 S3M_chn->period=(dx>S3M_chn->lownote_border)?S3M_chn->lownote_border:dx;
	 MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
	 MIX_chn->changes|=CHG_FREQ;
       }
       break;
    case 6: /*F - pitch slides up */
       reloadif0;
       if (effpar>0xDF) // extra up slides ...
       { WORD dx;
	 S3M_chn->command=0; // do not slide every tick ...
	 if (effpar<0xF0)
	 // fine slide up (4 times faster than Xfine)
	   dx=S3M_chn->period-((effpar&0xf)<<2);
	 else
	 // extra fine slide up
	   dx=S3M_chn->period-(effpar&0xf);
	 S3M_chn->period=(dx<S3M_chn->uppernote_border)?S3M_chn->uppernote_border:dx;
	 MIX_chn->frequency=8363lu*1712lu/S3M_chn->period;
	 MIX_chn->changes|=CHG_FREQ;
       }
       break;
    case 10: /*J - arpeggio */
       storeifn0;
       if (effpar||note<=0xfe) // new arpeggio notes
       { BYTE n1=S3M_chn->note&0xf,h1=S3M_chn->note>>4,n2=n1,h2=h1;
	 if (!effpar) effpar=S3M_chn->parameter;
	 n2 =n1+(effpar>>4);h2=h1+n2/12;n2%=12;
	 n1+= effpar&0xf;h1+=n1/12;n1%=12;
	 if (note<=0xfe) S3M_chn->ArpeggioPos=0; // restart arpeggio if new note
	 // calculate arpeggio stuff
	 S3M_chn->periods[0]=(8363lu*(16lu*period_tab[S3M_chn->note&0xf]>>(S3M_chn->note>>4)))/((*Instrument_p)[S3M_chn->instr].c2speed);
	 S3M_chn->periods[1]=(8363lu*(16lu*period_tab[n1]>>h1))/((*Instrument_p)[S3M_chn->instr].c2speed);
	 S3M_chn->periods[2]=(8363lu*(16lu*period_tab[n2]>>h2))/((*Instrument_p)[S3M_chn->instr].c2speed);
       }
       break;
    case 15: /*O - set sample offset */
       S3M_chn->parameter=effpar;
       if (note)
       { MIX_chn->changes|=CHG_SETOFS;
	  S3M_chn->startoffset=MIX_chn->offset=effpar<<8;
       }
       break;
    case 17: /* Q - Retrig */
       if (effpar)
       { S3M_chn->parameter=effpar;
	 if (effpar&0xf&&S3M_chn->note)
	 { S3M_chn->store=effpar&0xf;
	   S3M_chn->ctick=effpar&0xf;
	 } else S3M_chn->command=0; /* no retrigg */
       }
       break;
    case 19:   // S - special effects:
      { switch (effpar>>4)
	{ case 3:  /*S3 - set finetune*/   break; /* how does this work ????? change c4speed ? */
	}
	break;
      }
  }
}

void S3M_c::jumpto(char jp2)
{ while (jp2--)
  { int info;
    // skip one row:
    while ((info=*(c_position++))!=0)
    { if (info&32) c_position+=2;
      if (info&64) c_position++;
      if (info&128) c_position+=2;
    }
  }
}

void S3M_c::read_new_notes(struct playinfo_t *playinfo)
{ char chndone[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  BYTE info;
  BYTE * lastrow=c_position;
  // clear global effects:
  newgvol=255; /* set to : "no new global volume" */
  patternbreak=0;
  patternloop=0;
  positionjump=0;
  for (info=0;info<32;info++) chndone[info]=0;
  while ((info=*(c_position++))!=0) // read till next row
  { BYTE note=255;
    BYTE instr=0;
    BYTE vol=255;
    BYTE effno=0;
    BYTE effpar=0;
    chndone[info&31]=1;
    if (info&32)
    { note=*c_position++;
      instr=*c_position++;
    }
    if (info&64) // set volume
      vol=*c_position++;
    if (info&128) // command & info
    { effno=*(c_position++);
      effpar=*(c_position++);
    }
    if (!(S3M_header.channelset[info&31]&0x80))
      setup_channel(playinfo,info&31,note,instr,vol,effno,effpar);
  }

  // go through all channels did contain no data and were not yet set
  for (info=0;info<32;info++) 
    if (!chndone[info])
      setup_channel(playinfo,info,0xff,0,0xff,0,0); // an empty entry

  if (patterndelay)
  { // patterndelay in S3M format is like a loop one row w/out reading notes,
    // but only the effects (look at member "doeffects_patterndly()")
    c_position=lastrow;
  }

  /* do handle global effects : */
  if (newgvol!=255) S3M_header.gvolume=newgvol;

  if (patternloop)
  { c_position=Pattern_p[order[c_order]];
    c_row=ploopto;
    jumpto(ploopto);
  } else
  if (positionjump || patternbreak || ++(c_row)==64)
  { // next pattern
    if (positionjump)
    { if (pjumpto>no_Orders) pjumpto=0;
      if (pjumpto<c_order) startmodule(); // kind of loop!
      c_order=pjumpto;
    } else
    do if (c_order++==no_Orders)
       { c_order=0;
	 startmodule();
       }
    while (order[c_order]>253);
    c_position=Pattern_p[order[c_order]];
    loopto=0;inside=0;
    if (patternbreak)
    { // run through packed pattern data to row we have to start with
      c_row=pbreakto;
      jumpto(pbreakto);
    }
    else c_row=0;
  }
  c_tick=c_speed-1;
}


void S3M_c::doeffects_patterndly(struct playinfo_t * playinfo)
{ char chndone[16];
  BYTE info;
  BYTE * lastrow = c_position; // pointer in packed data of current pattern

  for (info=0;info<16;info++) chndone[info]=0;
  while ((info=*(c_position++))!=0) // read untill next row starts
  { BYTE vol=255;
    BYTE effno=0;
    BYTE effpar=0;
    chndone[info&31]=1;
    if (info&32)
    { c_position+=2;
    }
    if (info&64) // set volume
      vol=*(c_position++);
    if (info&128) // command & info
    { effno=*(c_position++);
      effpar=*(c_position++);
    }
    if (effno==19&&(effpar>>4)==14) // pattern delay (don't resetup patterndelay)
      { effno=0;effpar=0; }
    setup_channel(playinfo,info&31,0xff,0,vol,effno,effpar);
  }
  for (info=0;info<16;info++) 
    if (!chndone[info]) setup_channel(playinfo,info,0xff,0,0xff,0,0);
  if (patterndelay) c_position=lastrow;
  c_tick=c_speed-1;
}

short S3M_c::nextnotes(struct playinfo_t* playinfo)
{
   // this is called every 'TICK' (shortest segment of a note)

   memset(playinfo,0,sizeof(*playinfo));
   if (!c_tick && patterndelay)
   { patterndelay--;
     doeffects_patterndly(playinfo);
   }
   (*playinfo).whatTT=0;
   (*playinfo).tempo =c_tempo;
   if (c_tick>0)  
      effect_handling(playinfo);
   else 
      read_new_notes(playinfo);

   return(0);
}

/* ------------------------------------------------------------------------ */
/* S3M-files use LITTLE-ENDIAN-format so we have to use the "lit????"-routines */
/* to convert data in machine format (that's only a problem while loading   */
/* ------------------------------------------------------------------------ */

void S3M_c::load_module() // Is called by constructor
{ WORD paraIns[256]; //
  WORD paraPat[256]; // arrays of 'parapointers' (parapointer*16 = offset)
  WORD paraSmp[256]; //
  WORD *npIns=&paraIns[0],*npPat=&paraPat[0],*npSmp=&paraSmp[0]; // pointer into arrays
  WORD *lpSmp=&paraSmp[0];  // while instrument loading,
			    // we get 'parapointers'
  WORD cIns=1,cPat=0,cSmp=1,lIns,lPat=0;
  int i;

  // the following variables need to be set, or destructor might hang on
  // Systems on some stupid OSes (e.g.DOS)
  no_Pattern=0;
  Instrument_p=NULL;
  Module=this;
  ModuleID=S3M_idstr;
  memset(&Pattern_p,0,sizeof(Pattern_p));

  trace("loading S3M-module.\n");
  // load header and check for correct IDs in it
  if (fread(&S3M_header,1,96,file)!=96||
      strncmp((char *)&(S3M_header.SCRM_id),"SCRM",4)||
      S3M_header.Filetype!=16) 
     { status=WRONGFORMAT;return; }
  trace("Header loaded.\n");

  /* convert header words to machine format: */
  lit2m_w(&S3M_header.ordnum);
  lit2m_w(&S3M_header.patnum);
  lit2m_w(&S3M_header.insnum);
  lit2m_w(&S3M_header.flags);
  lit2m_w(&S3M_header.CreatedwithTrackerVersion);
  lit2m_w(&S3M_header.FileFormatInformation);
  lit2m_w(&S3M_header.specialdata);

  no_Channels =S3M_header.noChannels=16;
  no_Orders   =S3M_header.ordnum;

  // initialize arrays
  { int i=0; 
    while (i<256) {order[i]=0xff;paraIns[i]=0xffff;paraPat[i]=0xffff;paraSmp[i]=0xffff;i++;}
    i=0; while (i<256) {Pattern_p[i]=emptypatn;i++;}
  }

  // loading order
  if (fread(&order,1,S3M_header.ordnum,file)!=S3M_header.ordnum) 
    { status=WRONGFORMAT;return; }

  // load parapointers for instruments
  lIns=S3M_header.insnum;
  if (fread(&paraIns[0],2,lIns,file)!=lIns) { status=WRONGFORMAT;return; }

  // get memory for instrument data
  if ((Instrument_p=(struct instrument_t (*)[99])calloc((lIns+1)*sizeof(struct instrument_t),1))
      ==NULL) { status=NOMEM;return; }
  trace("Got memory for instruments.\n");

  // load para-pointers to patterns
  lPat=S3M_header.patnum;
  if (fread(&paraPat[0],2,lPat,file)!=lPat) { status=WRONGFORMAT;return; }
  //lit2m_ws(&paraPat[0],lPat);

  // load panning table if exists
  if (S3M_header.pantablepresent==252)
  { if (fread(&chnpan,1,32,file)!=32)
    { status=WRONGFORMAT;return; }
  } else
  { // generate chnpan out of (channelset)
    for (i=0;i<32;i++)
     chnpan[i]=(S3M_header.channelset[i]<8)?0xD:0x2; /* left/right */
  }
  for (i=0;i<32;i++) chnpan[i]=chnpan[i]<<4;

  trace("loading samples/patterns/instruments.\n");
  // now load samples/patterns/instruments in order is stored in parapointers
  while (cIns-1<lIns || cPat<lPat || cSmp-1<lIns)
  { // (npPat==0 || npIns==0) possible ??
    // the following lines look weird, but what we have to do is to find
    // out what value out of three is the smallest...
    if (*npIns<*npPat)
      if (*npSmp>*npIns)
      { // NEXT DATA BLOCK IS AN INSTRUMENT
        load_instrument(*npIns,cIns);
	npIns++;
	if ((*Instrument_p)[cIns].typ==1) // sample instrument
	{ *lpSmp=(*Instrument_p)[cIns].memseg; // add parapointer to array
	  trace("(sampledata!!)\n");
	} else {*lpSmp=0xffff;trace("\n");}
	lpSmp++; // next parapointer of an instrument
	cIns++;
      } else
      { // NEXT DATA BLOCK IS A SAMPLE
	readtill(file,*npSmp);
	if ((status=Device->LoadInstrument(cSmp,(*Instrument_p)[cSmp].slength,8,
		 (*Instrument_p)[cSmp].flags&1,(*Instrument_p)[cSmp].loopbeg,(*Instrument_p)[cSmp].loopend))!=0)
		 return;
	npSmp++; // next parapointer to a sample
	cSmp++;
      }
    else if(*npSmp>*npPat)
      { // NEXT DATA BLOCK IS A PATTERN
	if (cIns>=lIns)
	{ //printf("%u (I), %u(S), %u(P)",*npIns,*npSmp,*npPat);getchar();
          if (status=load_pattern(*npPat,cPat)) return;
	  npPat++; // next parapointer to a pattern
          cPat++;no_Pattern++;
	} else { cPat++;npPat++; }
      } else
      { // NEXT DATA BLOCK IS A SAMPLE
	readtill(file,*npSmp);
	if ((status=Device->LoadInstrument(cSmp,(*Instrument_p)[cSmp].slength,8,
		 (*Instrument_p)[cSmp].flags&1,(*Instrument_p)[cSmp].loopbeg,(*Instrument_p)[cSmp].loopend))!=0)
		 return;
	npSmp++; // next parapointer to a sample
        cSmp++;
      }
    while (cSmp<cIns && ((*npSmp==0) || (*npSmp==0xffff))) {npSmp++;cSmp++;}
	   // forget about instruments with no sampledata
  }
  #ifdef __DEBUGGING__
  printf("no pattern: %u\n",cPat);
  #endif
  S3M_header.noChannels=getnoChannels();
  trace("Well done. Module loaded.\n");
}

short S3M_c::load_pattern(WORD pos,WORD no)
{ WORD len;
  BYTE *h;

  if (!pos) { printf("bug (maybe no pattern defind in S3M !) ???\n");return(0); } 
  readtill(file,pos);
  fread(&len,2,1,file); len-=2;
  lit2m_w(&len);
  if (len==0) // empty pattern !
    { Pattern_p[no]=emptypatn; return(0); }
  if ((Pattern_p[no]=(BYTE *)calloc(len,1))==NULL)
    { printf("No memory for pattern ...\n"); // that should not happen under LINUX ?
      return(NOMEM); } 
  #ifdef __DEBUGGING__ 
  printf("load pattern (%2u)   %u bytes\n",no,len);
  #endif
  h=Pattern_p[no];
  while (!feof(file) && len--) *h++=getc(file);
  return(0);
}

void S3M_c::load_instrument(WORD pos,WORD no)
{
  readtill(file,pos);
  fread(&(*Instrument_p)[no],1,sizeof(struct instrument_t),file);
  lit2m_w (&((*Instrument_p)[no].memseg));
  lit2m_dw(&((*Instrument_p)[no].slength));
  lit2m_dw(&((*Instrument_p)[no].loopbeg));
  lit2m_dw(&((*Instrument_p)[no].loopend));
  lit2m_dw(&((*Instrument_p)[no].c2speed));
  /* GUSaddr,SB_flags not used in playing routines */

  #ifdef __DEBUGGING__
  printf("load instrument (%2u) \"%-28s\".",no,(*Instrument_p)[no].Instrumentname);
  #endif
}

DWORD S3M_c::readSMP8_BLK(BYTE * destination,DWORD size,BYTE bits)
{ // I've never seen 16bit S3Ms, in this case the 16bit values need to be converted
  while (!feof(file)&&size) { *(destination++)=getc(file);size--; }
  trace("done.\n");
  return(size);
}

DWORD S3M_c::readSMP16_BLK(WORD * destination,DWORD size,BYTE bits)
{ // I've never seen 16bit S3Ms, in this case the 16bit values don't need to be 
  // converted
  while (!feof(file)&&size) { *(destination++)=((WORD)getc(file)<<8);size--; }
  trace("done.\n");
  return(size);
}
