/*****************************************************************************
   669    support routines for music file format 669
  --------------------------------------------------------------------------
   (c) copyright 1996 by A.Baresel
    Created on 28.7.1996
*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "669.h"
#include "module.ids"
#include "device.h"
#include "xendian.h"

#define magicnum 3579364lu

#if defined(__DOS__) && !defined(__GNUC__)
  #define strncasecmp strncmpi
#endif

// variables :
static const char * _669_idstr=_669_ID;

static short sinuswav[64]={  0, 24, 50, 74, 98,120,142,162,180,196,212,224, 234,244,250,252,
			   254,252,250,244,234,224,212,196,180,162,142,120, 98, 74, 50, 24,
			     0,-24,-50,-74,-98,-120,-142,-162,-180,-196,-212,-224,-234,-244,
			   -250,-252,-254,-252,-250,-244,-234,-224,-212,-196,-180,-162,-142,-120,
			   -98,-74,-50,-24
			  };

static const WORD period_tab[]={1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907};

// 0-parameter handling:
#define reloadif0  if (!effpar) effpar=_669_chn->parameter; else _669_chn->parameter=effpar;
#define storeifn0  if (effpar) _669_chn->parameter=effpar;

void _669_c::effect_handling(struct playinfo_t * playinfo)
{ /*
  int i;
  char al;
  for (i=0;i<MTM_header.no_Tracks;i++)
  { struct MOD_c::channel_t *MOD_chn=&chn[i];
    struct chninfo_t *MIX_chn=&(playinfo->chn[i]);
    switch (MOD_chn->useeffect) // new effect numbers
    { 
      case 0x2: // Slide Up
	if (MOD_chn->period)
	{ int p=MOD_chn->period-MOD_chn->parameter;
	  MOD_chn->period=(p<15)?15:p;
	  MIX_chn->frequency=magicnum/MOD_chn->period;
	  MIX_chn->changes=CHG_FREQ;
	}
	break;
      case 0x3: // Slide Down
	if (MOD_chn->period)
	{ int p=MOD_chn->period+MOD_chn->parameter;
	  MOD_chn->period=(p>1712)?1712:p;
	  MIX_chn->frequency=magicnum/MOD_chn->period;
	  MIX_chn->changes=CHG_FREQ;
	}
	break;

      case 0x4: // toneporta
	{ int period;
	  period=MOD_chn->period;
	  if (MOD_chn->period>MOD_chn->Slideto)
	  { 
	    period-=(unsigned int)MOD_chn->PortPara; 
	    if (period<MOD_chn->Slideto) 
	    { period=MOD_chn->Slideto; 
	      MOD_chn->useeffect=(MOD_chn->useeffect==0x4)?0:0x9; 
	    } 
	  } else 
	  { period+=(unsigned int)MOD_chn->PortPara;
	    if (period>MOD_chn->Slideto)
	    { period=MOD_chn->Slideto;
	      MOD_chn->useeffect=(MOD_chn->useeffect==0x4)?0:0x9; 
	    }
	  } 
	  if (MOD_chn->Glisscontrol)
	  { int i=get_period(period,0);
	    period=i-(i*MOD_chn->finetune>>7);
	  }
	  else MOD_chn->period=period;
	  MIX_chn->changes|=CHG_FREQ; 
	  MIX_chn->frequency=magicnum/period;
	  break; // Tone Portamento done
	}

      case 0x5:  // Vibrato
	{ int period;
	  int dl;
	  MOD_chn->VibtabPos=(MOD_chn->VibtabPos+MOD_chn->VibSpeed)&63;
	  dl=(*all_tabs[MOD_chn->VibwavNo&0x3])[MOD_chn->VibtabPos];
	  dl=(dl*MOD_chn->VibDepth)>>7;
	  period=MOD_chn->period+dl;
	  MIX_chn->changes|=CHG_FREQ;
	  MIX_chn->frequency=magicnum/period;
	  break;
	}
	// case retrigg ----
    }
  }
  */
  c_tick--;
}

void _669_c::setup_channel(struct playinfo_t *playinfo,BYTE chnnr,
			  BYTE note,BYTE instr,BYTE volume,BYTE effno, BYTE effpar)
{ struct _669_c::channel_t *_669_chn=&chn[chnnr];
  struct chninfo_t *MIX_chn=&(playinfo->chn[chnnr]);

  if (instr)
  { // set a new instrument
    _669_chn->instr=MIX_chn->instno=instr;
    /*_669_chn->finetune=Instrument[instr].finetune;*/
    MIX_chn->changes|=CHG_VOLUME;
    _669_chn->volume=MIX_chn->volume=64/*Instrument[instr].volume*/;
    if (!note) MIX_chn->changes|=CHG_RESTART;
  } else MIX_chn->instno=_669_chn->instr;

  // all effects have to be proccessed before new period is set:
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  switch (effno)
  { /*
    case 0x3: // Tone Portamento
      { if (effpar) _669_chn->PortPara=effpar;
	if (period) { _669_chn->Slideto=period-(period*_669_chn->finetune>>7);period=0; }
	else if (!_669_chn->Slideto) { effno=0;effpar=0; }
	break;
      }
    case 0x4: case 0x6: // vibratos
      if (((_669_chn->useeffect!=0x5 && _669_chn->effect!=0x7)||period) &&
	   _669_chn->VibwavNo>>2)
	_669_chn->VibtabPos=0;  // reset vibrato wave position
      break;
    */
  }

  // ending effects:
  /*
  if ((_669_chn->useeffect==0x5 || _669_chn->effect==0x7) && // last effect was a vibrato
      effno!=0x4 && effno!=0x6)                            // and this vibrato ends now
    { // restore old values
      MIX_chn->frequency=magicnum/_669_chn->period;
      MIX_chn->changes|=CHG_FREQ;
    }

  _669_chn->effect=effno;
  _669_chn->useeffect=converteff[effno];

  if (!effno && !effpar) _669_chn->useeffect=0;
  */

  if (note!=0x3f)
  { _669_chn->note=note;
    WORD period=period_tab[note%12]>>(note/12);
    _669_chn->period=period;
    _669_chn->Slideto=0; // stop old portamento (if there was...)
    MIX_chn->frequency=magicnum/period;
    if (instr) MIX_chn->changes|=CHG_INSTR|CHG_FREQ;
  }

  // all effects have to be processed after period&instr:
  switch (effno)
  { /*
    case 0x1: case 0x2:
      storeifn0;break; // nothing else !
    case 0x4:  // Vibrato 
      if (effpar) 
      { if (effpar&0xf0) _669_chn->VibSpeed=effpar>>4;
        _669_chn->VibDepth=effpar&0xf; 
      }
      if (!_669_chn->period)
        _669_chn->useeffect=0; 
       break;
    case 0xF: // speed or tempo changes
      storeifn0;
      if (effpar && effpar<32) c_speed=effpar;
      if (effpar && effpar>31) c_tempo=effpar;
      break;
    */
  }
}

void _669_c::read_new_notes(playinfo_t* playinfo_p)
{ int info;
  int ch=0;

  for (ch=0;ch<8;ch++)
  { BYTE b0=*c_position++;
    BYTE b1=*c_position++;
    BYTE b2=*c_position++;
    BYTE note=b0>>2;
    BYTE instr=((b0 & 3)<<4)|(b1>>4);
    BYTE vol=(b1&0xf)<<2;
    if (b0==0xfe) instr=0xff;  // no instr/note change !
    if (b0==0xff) vol=0xff; // no volume change !
    BYTE effno=b2>>4;
    BYTE effpar=b2&0xf;
    if (b2==0xff) { effno=0;effpar=0; }
    setup_channel(playinfo_p,ch,note,instr+1,vol,effno,effpar);
  }
  if (c_row++>=pattern_len[c_pattern])
  { // end of pattern !
    if (++c_order==_669_header.no_Orders) // end of song ... loop
      startmodule();
    c_row=0;
    c_pattern=order[c_order];
    c_speed=pattern_speed[c_pattern];
    c_position=Pattern_p[c_pattern];
  }
  c_tick=c_speed-1;
}

short _669_c::nextnotes(playinfo_t* playinfo_p)
{
  memset(playinfo_p,0,sizeof(playinfo_t));
  if (c_tick) effect_handling(playinfo_p); else
  read_new_notes(playinfo_p);
  playinfo_p->whatTT=0;
  playinfo_p->tempo =c_tempo;
  return(0);
}

void _669_c::delete_module(void)
{ int no;
  trace("Remove 669-module from memory.\n");
  /*for (i) free(pattern_p[i]);*/
}

#define LEFT   200
#define RIGHT  (255-LEFT)

short _669_c::startmodule(void)
{ int i; BYTE b;
  c_row=0;
  c_tick=0;
  c_tempo=0x3e;
  c_order=0;
  c_pattern=order[c_order];
  c_speed=pattern_speed[c_pattern];
  c_position=Pattern_p[c_pattern];
  for (i=0;i<8;i++)
  { b=(i&1)?RIGHT:LEFT;  // channels 0,2,4,6 on the left !
    Device->SetupChannel(i,CHG_PANNING,&b);
  }
  return(0);
}

// /--------------------------------------------------------------------------------\
// |- main loader: -----------------------------------------------------------------|
// \--------------------------------------------------------------------------------/

void _669_c::load_module(void)
{ int i,j,k;
  status=0;
  trace("loading MultiTracker module.\n");
  Module=this;
  ModuleID=_669_idstr;
  // check id
  if (fread(&_669_header,1,sizeof(_669_header),file)!=sizeof(_669_header) ||
      (strncmp((char *)&(_669_header.ID),"JN",2) &&
       strncmp((char *)&(_669_header.ID),"if",2)))
     { status=WRONGFORMAT;return; }
  trace("Header loaded.\n");

  if (fread(&order,1,128,file)!=128)
    { status=WRONGFORMAT;return; }
  trace("order loaded.\n");
  if (fread(&pattern_speed,1,128,file)!=128)
    { status=WRONGFORMAT;return; }
  trace("pattern tempos loaded.\n");
  if (fread(&pattern_len,1,128,file)!=128)
    { status=WRONGFORMAT;return; }
  trace("pattern break locations loaded.\n");

  // load instrument headers:
  trace("Load sample headers...\n");
  memset(&Instrument,0,sizeof(Instrument));

  for (i=1;i<_669_header.no_Samples+1;i++)
  { if (fread(&(Instrument[i]),1,13,file)!=13)
      { status=WRONGFORMAT;return; }
    if (fread(&(Instrument[i].smplen),1,12,file)!=12)
      { status=WRONGFORMAT;return; }

    // convert endian format:
    lit2m_dw(&Instrument[i].smplen);
    lit2m_dw(&Instrument[i].loopstart);
    lit2m_dw(&Instrument[i].loopend);

    // a 669 'feature' (loopend==0xf0ffff ---> noloop)
    if (Instrument[i].loopend>=0xf00000LU) { Instrument[i].loopend=Instrument[i].loopstart=Instrument[i].smplen; }

    #ifdef __DEBUGGING__
    if (Instrument[i].smplen)
      printf("name: %-13.13s  size: %ld, %ld, %ld\n",Instrument[i].name,
        Instrument[i].smplen,Instrument[i].loopstart,Instrument[i].loopend);
    #endif

  }
  getchar();

  // pattern loading:
  trace("Loading patterns:\n");
  for (i=0;i<_669_header.no_Pattern;i++)
  { // pack them later !
    if ((Pattern_p[i]=(BYTE *)calloc(64*8*3,1))==NULL)
      { status=NOMEM; return; }
    if (fread(Pattern_p[i],1,3*8*64,file)!=3*8*64)
      { status=WRONGFORMAT;return; }
  }

  // load sample data of instruments
  trace("Loading sample data...\n");
  for (i=1;i<_669_header.no_Samples;i++)
  { //BYTE loopflag=((SDWORD)Instrument[i].loopend-(SDWORD)Instrument[i].loopstart>0)?1:0;
    BYTE loopflag=Instrument[i].smplen>=Instrument[i].loopend;
    if (Instrument[i].smplen)
      status=Device->LoadInstrument(i,Instrument[i].smplen,8,loopflag,Instrument[i].loopstart,Instrument[i].loopend);
    if (status!=0) return;
  }
  trace("loading done.\n");
}

DWORD _669_c::readSMP8_BLK(BYTE * dest,DWORD size,BYTE _bits)
{ // load samples as 8bit unsigned -> no conversion needed :)
  printf("loading %ld bytes.\n",size);
  while (!feof(file) && size) { *(dest++)=(BYTE)getc(file);size--; }
  return(size);
}

DWORD _669_c::readSMP16_BLK(WORD * dest,DWORD size,BYTE _bits)
{ // load samples as 16bit unsigned -> simply multiply 256 ...
  printf("loading %ld bytes.\n",size);
  while (!feof(file) && size) { *(dest++)=((WORD)getc(file)<<8);size--; }
  return(size);
}
