/*****************************************************************************
   MTM    support routines for music file format MTM
  --------------------------------------------------------------------------
   (c) copyright 1996 by A.Baresel
    Created on 23.7.1996
*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mtm.h"
#include "module.ids"
#include "device.h"
#include "xendian.h"

#define magicnum 3579364lu

#if defined(__DOS__) && !defined(__GNUC__)
  #define strncasecmp strncmpi
#endif

// variables :
static const char * MTM_idstr=MTM_ID;

static short sinuswav[64]={  0, 24, 50, 74, 98,120,142,162,180,196,212,224, 234,244,250,252,
			   254,252,250,244,234,224,212,196,180,162,142,120, 98, 74, 50, 24,
			     0,-24,-50,-74,-98,-120,-142,-162,-180,-196,-212,-224,-234,-244,
			   -250,-252,-254,-252,-250,-244,-234,-224,-212,-196,-180,-162,-142,-120,
			   -98,-74,-50,-24
			  };

static short rampdwav[64]={255,246,238,229,221,212,204,195,187,178,170,161,153,144,136,127,
			   119,110,102, 93, 85, 76, 68, 59, 51, 42, 34, 23, 17,  8,  0,
			   -255,-246,-238,-229,-221,-212,-204,-195,-187,-178,-170,-161,-153,
			   -144,-136,-127,-119,-110,-102,-93,-85,-76,-68,-59,-51,-42,-34,-23,
			   -17,-8,0
			  };

static short squarwav[64]={  0, 70,128,140,160,180,200,215,235,243,247,250,255,255,255,255,255,
			   255,255,250,247,243,235,215,200,180,160,140,128, 70,  0,
			     0,-70,-128,-140,-160,-180,-200,-215,-235,-243,-247,-250,-255,-255,
			   -255,-255,-255,-255,-255,-250,-247,-243,-235,-215,-200,-180,-160,-140,
			   -128,-70, 0
			  };

static short (* all_tabs[3])[64]={&sinuswav,&rampdwav,&squarwav};

static const WORD period_tab[]={1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907};

// 0-parameter handling:
#define reloadif0  if (!effpar) effpar=MTM_chn->parameter; else MTM_chn->parameter=effpar;
#define storeifn0  if (effpar) MTM_chn->parameter=effpar;

void MTM_c::effect_handling(struct playinfo_t * playinfo)
{ /*
  int i;
  char al;
  for (i=0;i<MTM_header.no_Tracks;i++)
  { struct MOD_c::channel_t *MOD_chn=&chn[i];
    struct chninfo_t *MIX_chn=&(playinfo->chn[i]);
    switch (MOD_chn->useeffect) // new effect numbers
    { case 0x1:  // arpeggio
	{ int period;
	  MOD_chn->ArpeggioPos++;MOD_chn->ArpeggioPos%=3;
	  period=MOD_chn->periods[MOD_chn->ArpeggioPos];
	  MIX_chn->frequency=magicnum/period;
	  MIX_chn->changes|=CHG_FREQ;
	  break;
	}
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

      case 0x6:  // Tone Portamento + Volume Slide
	// slide volume:
	{ int period;
	  char al=(MOD_chn->parameter>>4)-(MOD_chn->parameter&0xf)+MOD_chn->volume;
	  al=(al<0)?0:(al>63)?63:al;
	  MIX_chn->volume=(MOD_chn->volume=al);
	  MIX_chn->changes|=CHG_VOLUME;
	  // toneporta follows:
	}
      case 0x4:
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

      case 0x7:  // Vibrato + Volume Slide
	{ int period;
	  // slide volume:
	  char al=(MOD_chn->parameter>>4)-(MOD_chn->parameter&0xf)+MOD_chn->volume;
	  al=(al<0)?0:(al>63)?63:al;
	  MIX_chn->volume=(MOD_chn->volume=al);
	  MIX_chn->changes|=CHG_VOLUME;
	  // vibrato follows:
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

      case 0x8: // Tremolo
	{ char volume;
	  MOD_chn->TrmtabPos=(MOD_chn->TrmtabPos+(MOD_chn->parameter>>4))&63; // speed
	  int dl=(*all_tabs[MOD_chn->TrmwavNo&0x3])[MOD_chn->TrmtabPos];
	  dl=(dl*(MOD_chn->parameter&0xf))>>7; // depth
	  volume=(char)MOD_chn->volume+dl;
	  volume=(volume<0)?0:(volume>63)?63:volume;
	  MIX_chn->changes|=CHG_VOLUME;
	  MIX_chn->volume=volume;
	  break;
	}
      case 0x9: // Volume Slide
	al=(MOD_chn->parameter>>4)-(MOD_chn->parameter&0xf)+MOD_chn->volume;
	al=(al<0)?0:(al>63)?63:al;
	MIX_chn->volume=(MOD_chn->volume=al);
	MIX_chn->changes|=CHG_VOLUME;
	break;
      case 0xA: // E9 - Retrig Note
	if (!MOD_chn->ctick--) // retrigg if zero
	{ MOD_chn->ctick=MOD_chn->numticks;
	  MIX_chn->changes|=CHG_SETOFS|CHG_DOMIX;
	  MIX_chn->offset=0;
	}
	break;
      case 0xB: // EC - NoteCut
	if (!MOD_chn->ctick--) // cut it if zero
	{ MIX_chn->volume=0;
	  MIX_chn->changes|=CHG_VOLUME;
	  MOD_chn->useeffect=0;
	}
	break;
      case 0xC: // ED - NoteDelay
	if (!--MOD_chn->ctick) // start playing it if zero
	{
	  if (MOD_chn->sInst)
	  { // set a new instrument
	    MOD_chn->instr=MIX_chn->instno=MOD_chn->sInst;
	    MOD_chn->finetune=Instrument[MOD_chn->sInst].finetune;
	    MIX_chn->changes|=CHG_VOLUME;
	    MOD_chn->volume=MIX_chn->volume=Instrument[MOD_chn->sInst].volume;
	  } else MIX_chn->instno=MOD_chn->instr;
	  if (MOD_chn->sPeriod)
	  { // start the delayed note
	    MOD_chn->period=MOD_chn->sPeriod-(MOD_chn->sPeriod*MOD_chn->finetune>>7);
	    MIX_chn->frequency=magicnum/MOD_chn->period;
	    if (MOD_chn->sInst) MIX_chn->changes|=CHG_INSTR|CHG_FREQ;
	    else
	    { MIX_chn->changes|=CHG_FREQ|CHG_SETOFS|CHG_DOMIX;
	      MIX_chn->offset=MOD_chn->startoffset;
	    }
	  }
	  MOD_chn->useeffect=0;
	}
	break;
    }
  }
  */
  c_tick--;
}

void MTM_c::setup_channel(struct playinfo_t *playinfo,BYTE chnnr,
			  BYTE note,BYTE instr,BYTE effno, BYTE effpar)
{ struct MTM_c::channel_t *MTM_chn=&chn[chnnr];
  struct chninfo_t *MIX_chn=&(playinfo->chn[chnnr]);
  unsigned int converteff[16]={1,2,3,4,5,6,7,8,0,0,9,0,0,0,0,0};
  unsigned int conv2eff[16]={0,0,0,0,0,0,0,0,0,10,0,0,11,12,0,0};

  if (effno==0xe&&(effpar>>4)==0xd) // note delay
  { MTM_chn->sNote=note;note=0;
    MTM_chn->sInst=instr;instr=0;
    MTM_chn->ctick=effpar&0xf;
  }

  if (instr)
  { // set a new instrument
    MTM_chn->instr=MIX_chn->instno=instr;
    MTM_chn->finetune=Instrument[instr].finetune;
    MIX_chn->changes|=CHG_VOLUME;
    MTM_chn->volume=MIX_chn->volume=Instrument[instr].volume;
    MTM_chn->startoffset=0;
    if (!note) MIX_chn->changes|=CHG_RESTART;
  } else MIX_chn->instno=MTM_chn->instr;

  // all effects have to be proccessed before new period is set:
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  switch (effno)
  { /*
    case 0x0:  // arpeggio or nothing
      if (effpar)
      { int p=period?period:get_period(MTM_chn->period,0); // not finetuned period
	MTM_chn->parameter=effpar;
	if (period) MTM_chn->ArpeggioPos=0;
	if (p)
	{ int i;
	  MTM_chn->periods[0]=p-(p*MTM_chn->finetune>>7);
	  i=get_period(p,(unsigned char)effpar>>4);
	  MTM_chn->periods[1]=i-(i*MTM_chn->finetune>>7);
	  i=get_period(p,effpar&0xf);
	  MTM_chn->periods[2]=i-(i*MTM_chn->finetune>>7);
	} else effpar=0;
      }
      break;
    case 0x3: // Tone Portamento
      { if (effpar) MTM_chn->PortPara=effpar;
	if (period) { MTM_chn->Slideto=period-(period*MTM_chn->finetune>>7);period=0; }
	else if (!MTM_chn->Slideto) { effno=0;effpar=0; }
	break;
      }
    case 0x5: // Tone Portamento + VolSlide
      if (period) { MTM_chn->Slideto=period-(period*MTM_chn->finetune>>7);period=0; }
      else if (!MTM_chn->Slideto) effno=0xa;
      break;
    case 0x4: case 0x6: // vibratos
      if (((MTM_chn->useeffect!=0x5 && MTM_chn->effect!=0x7)||period) &&
	   MTM_chn->VibwavNo>>2)
	MTM_chn->VibtabPos=0;  // reset vibrato wave position
      break;
    case 0xE: // special effects need to be processed before period is set
      { if (effpar) MTM_chn->parameter=effpar;
	if ((effpar>>4)==0x5)  // set finetune
	  MTM_chn->finetune=((char)effpar<<4)>>4; // signed nibble to signed char
	break;
      }
    */
  }

  // ending effects:

  if ((MTM_chn->useeffect==0x5 || MTM_chn->effect==0x7) && // last effect was a vibrato
      effno!=0x4 && effno!=0x6)                            // and this vibrato ends now
    { // restore old values
      MIX_chn->frequency=magicnum/MTM_chn->period;
      MIX_chn->changes|=CHG_FREQ;
    }
  if (MTM_chn->useeffect==0x1 && // last effect was arpeggio
       (effno||!effpar))         // and this arpeggio ends now
    { MIX_chn->frequency=magicnum/MTM_chn->period;
      MIX_chn->changes|=CHG_FREQ;
    }

  MTM_chn->effect=effno;
  MTM_chn->useeffect=converteff[effno];

  if (!effno && !effpar) MTM_chn->useeffect=0;

  if (note)
  { MTM_chn->note=note;
    WORD period=period_tab[note%12]>>(note/12);
    MTM_chn->period=period-(period*MTM_chn->finetune>>7);
    MTM_chn->Slideto=0; // stop old portamento (if there was...)
    MIX_chn->frequency=magicnum/period;
    if (instr) MIX_chn->changes|=CHG_INSTR|CHG_FREQ;
    else
    { MIX_chn->changes|=CHG_FREQ|CHG_SETOFS|CHG_DOMIX;
      MIX_chn->offset=MTM_chn->startoffset;
    }
  }

  // all effects have to be processed after period&instr:
  switch (effno)
  { /*
    case 0x1: case 0x2:
      storeifn0;break; // nothing else !
    case 0x4:  // Vibrato 
      if (effpar) 
      { if (effpar&0xf0) MTM_chn->VibSpeed=effpar>>4;
        MTM_chn->VibDepth=effpar&0xf; }
      if (!MTM_chn->period)
        MTM_chn->useeffect=0; break;
    case 0x5:  // Tone Portamento +  Volume Slide
      storeifn0; break; 
    case 0x6:  // Vibrato + Volume Slide
      storeifn0; break;
    case 0x7:  // Tremolo
      storeifn0; break;
    case 0x8:  // NOT USED ??  
      storeifn0; break; 
    case 0x9:  // set offset only if (period)!
       storeifn0; 
	if (period) {
	  MIX_chn->changes|=CHG_SETOFS;
	  MTM_chn->startoffset=MIX_chn->offset=effpar<<8;
	}
      break;
    case 0xA:  // Volume Slide
      storeifn0;
      break;
    case 0xB:  // Position Jump
      storeifn0;
      positionjump=1;
      pjumpto=effpar;
      break;
    case 0xC:  // set volume
      storeifn0;
      MIX_chn->changes|=CHG_VOLUME;
      if (effpar>63) effpar=63;
      MTM_chn->volume=MIX_chn->volume=effpar;
      break;
    case 0xD: // Patternbreak
      storeifn0;
      patternbreak=1;
      pbreakto=10*(effpar>>4)+(effpar&0xf);
      break;
    case 0xE:  // special effects:
      { storeifn0;
	MTM_chn->useeffect=conv2eff[effpar>>4];
	switch (effpar>>4)
	{ // case 0x0: break;   E0 - set filter no supported
	  case 0x1: // E1 - FineSlide Up
	    if (MTM_chn->period)
	    { int p=MTM_chn->period-15;
	      MTM_chn->period=(p<(int)MTM_chn->parameter)?15:MTM_chn->period-(int)MTM_chn->parameter;
	      MIX_chn->frequency=magicnum/MTM_chn->period;
	      MIX_chn->changes=CHG_FREQ;
	    }
	    break;
	  case 0x2: // E2 - FineSlide Down
	    if (MTM_chn->period)
	    { int p=MTM_chn->period;
	      if (p+(int)MTM_chn->parameter>1712) MTM_chn->period=1712;
	      else MTM_chn->period=p+(int)MTM_chn->parameter;
	      if (MTM_chn->period) MIX_chn->frequency=magicnum/MTM_chn->period;
	      MIX_chn->changes=CHG_FREQ;
	    }
	    break;
	  case 0x3: // E3 - Glissando Control
	    MTM_chn->Glisscontrol=effpar;
	    break;
	  case 0x4: // E4 - Set Vibrato Waveform
	    MTM_chn->VibwavNo=effpar&0xf;
	    break;
	  case 0x6: // E6 - Set/Jump to Loop
	    if (!(effpar&0xf)) // set loop point
	      MTM_chn->loopto=c_row;
	    else
	    { if (!MTM_chn->inside)
	      { MTM_chn->inside=1;
		MTM_chn->loops=effpar&0xf;
	      }
	      if (MTM_chn->loops)
	      { MTM_chn->loops--;
		patternloop=1;
		ploopto=MTM_chn->loopto;
	      } else
	      { MTM_chn->inside=0;
		MTM_chn->loopto=c_row+1;
	      }
	    }
	    break;
	  case 0x7: // E7 - Set Tremolo Waveform
	    MTM_chn->TrmwavNo=effpar&0xf;
	    break;
	  case 0x8: // E8 - SET PANPOSITION
	    MIX_chn->panposition=effpar&0xf;
	    MIX_chn->changes|=CHG_PANNING;
	    break;
	  case 0x9:  // E9 - Retrig Note
	    if (effpar&0xf&&MTM_chn->period)
	    { MTM_chn->numticks=effpar&0xf;
	      MTM_chn->ctick=(effpar&0xf);
	    } else MTM_chn->useeffect=0;
	    break;
	  case 0xA:  // EA - Fine VolumeSlide Up
	    { short t=MTM_chn->volume+(effpar&0xf);
	      if (t>63) t=63;
	      MIX_chn->volume=(MTM_chn->volume=t);
	      MIX_chn->changes|=CHG_VOLUME;
	    }  
	    break;
	  case 0xB:  // EB - Fine VolumeSlide Down
	    { short t=MTM_chn->volume-(effpar&0xf);
	      if (t<0) t=0;
	      MIX_chn->volume=(MTM_chn->volume=t);
	      MIX_chn->changes|=CHG_VOLUME;
	    }
	    break;
	  case 0xC: // EC - NoteCut
	    MTM_chn->ctick=effpar&0xf;
	    break;
	  case 0xE: // EE - PatternDelay
	    patterndelay=effpar&0xf;
	    break;
	  // case 0xF: break; EF - Invert Loop   not supported
	}
	break;
      }
    */
    case 0xF: // speed or tempo changes
      storeifn0;
      if (effpar && effpar<32) c_speed=effpar;
      if (effpar && effpar>31) c_tempo=effpar;
      break;
  }
}

void MTM_c::read_new_notes(playinfo_t* playinfo_p)
{ int info;
  int ch=0;
  // clear global effects:
  patternbreak=0;
  patternloop=0;
  positionjump=0;
  for (ch=0;ch<MTM_header.no_Tracks;ch++)
  { BYTE *data=&((track_p[(*c_Tracks)[ch]])[c_row][0]);
    BYTE note=*data>>2;
    BYTE instr=((*data&3)<<4)|(*(data+1)>>4);
    BYTE effno=*(data+1)&0x0f;
    BYTE effpar=*(data+2);
    setup_channel(playinfo_p,ch,note,instr,effno,effpar);
  }
  /*
  if (patternloop)
  { c_Tracks=&((*pattern_p)[order[c_order]]);
    c_row=ploopto;
  } else
  */
  if (/*positionjump || patternbreak || */c_row++>=63)
  { // end of pattern !
    /*
    if (positionjump)
    { if (pjumpto>no_Orders) pjumpto=0;
      if (pjumpto<c_order) startmodule(); // kind of loop!
      c_order=pjumpto;
    } else
    */
    if (++c_order==MTM_header.no_Orders) // end of song ... loop
    { startmodule();
      c_order=0;
    }
    c_Tracks=&((*pattern_p)[order[c_order]]);
    /*
    if (patternbreak)
    {
      c_row=pbreakto;
      c_order=pbreakto;
      c_Tracks=&((*pattern_p)[order[c_order]]);
    }
    else */c_row=0;
  }
  c_tick=c_speed-1;
}

short MTM_c::nextnotes(playinfo_t* playinfo_p)
{
  memset(playinfo_p,0,sizeof(playinfo_t));
  if (!c_tick && patterndelay)
  { patterndelay--;
    c_tick = c_speed-1;
  }
  if (c_tick) effect_handling(playinfo_p); else
  read_new_notes(playinfo_p);
  playinfo_p->whatTT=0;
  playinfo_p->tempo =c_tempo;
  return(0);
}

void MTM_c::delete_module(void)
{ int no;
  trace("Remove MTM from memory.\n");
  /*free(pattern_p);
  free(track_p);*/
}

short MTM_c::startmodule(void)
{ int i; BYTE b;
  c_order=0;
  c_row=0;
  c_tick=0;
  c_speed=6;
  c_tempo=125;
  c_Tracks=&((*pattern_p)[order[c_order]]);
  patterndelay=0;
  for (i=0;i<MTM_header.no_Tracks;i++)
  { b=MTM_header.chnpan[i]<<4;
    Device->SetupChannel(i,CHG_PANNING,&b);
  }
  return(0);
}

// /--------------------------------------------------------------------------------\
// |- main loader: -----------------------------------------------------------------|
// \--------------------------------------------------------------------------------/

void MTM_c::load_module(void)
{ int i,j,k;
  status=0;
  trace("loading MultiTracker module.\n");
  Module=this;
  ModuleID=MTM_idstr;
  // check id
  if (fread(&MTM_header,1,sizeof(MTM_header),file)!=sizeof(MTM_header) ||
      strncmp((char *)&(MTM_header.ID),"MTM",3))
     { status=WRONGFORMAT;return; }
  for (i=0;i<32;i++) printf("%i,",MTM_header.chnpan[i]);
  trace("Header loaded.\n");
  #ifdef __DEBUGGING__
    printf("beats per track: %i\n",MTM_header.bpt);
  #endif
  // convert endian format: (MTM is little endian storage type!)
  lit2m_w(&MTM_header.no_SavedTracks);
  lit2m_w(&MTM_header.commentlen);

  // load instrument headers:
  trace("Load sample headers...\n");
  printf("no: %i\n",MTM_header.no_Samples);
  memset(&Instrument,0,sizeof(Instrument));

  for (i=1;i<MTM_header.no_Samples+1;i++)
  { if (fread(&(Instrument[i]),1,22,file)!=22)
      { status=WRONGFORMAT;return; }
    if (fread(&(Instrument[i].smplen),1,15,file)!=15)
      { status=WRONGFORMAT;return; }

    // convert endian format:
    lit2m_dw(&Instrument[i].smplen);
    lit2m_dw(&Instrument[i].loopstart);
    lit2m_dw(&Instrument[i].loopend);

    #ifdef __DEBUGGING__
    if (Instrument[i].smplen)
      printf("name: %22s  size: %ld, %ld, %ld\n",Instrument[i].name,
        Instrument[i].smplen,Instrument[i].loopstart,Instrument[i].loopend);
    #endif

    // convert 4bit signed nibbel to 8bit signed character ??
    // if (Instrument[i].finetune>7) Instrument[i].finetune-=16;
  }

  if (fread(&order,1,128,file)!=128)
    { status=WRONGFORMAT;return; }
  trace("order loaded.\n");

  // Track0 is empty:
  if ((track_p=(track_t (*))calloc(sizeof(track_t),MTM_header.no_SavedTracks))==NULL)
    { status=NOMEM;return; }
  //for (i=0;i<64;i++) (*track_p)[0][3*i][0]=0xfc; // note=0x3f; --> no note

  trace("loading tracks...\n");
  // load tracks:
  if (fread(&(track_p[1]),1,192*MTM_header.no_SavedTracks,file)!=MTM_header.no_SavedTracks*192)
    { status=WRONGFORMAT;return; }

  trace("Load pattern trackinfo...\n");
  // load pattern trackinfo:
  if ((pattern_p=(WORD (*)[256][32])calloc(MTM_header.no_SavedPattern+1,32*2))==NULL)
    { status=NOMEM; return; }
  if (fread(pattern_p,1,2*32*(MTM_header.no_SavedPattern+1),file)!=2*32*(MTM_header.no_SavedPattern+1))
    { status=WRONGFORMAT;return; }
  // those words need to be converted !
  for (i=1;i<MTM_header.no_SavedPattern;i++)
  { c_Tracks=&((*pattern_p)[i]);
    for (j=0;j<MTM_header.no_Tracks;j++) 
      lit2m_w(&((*c_Tracks)[j]));
  }
  // forget about extra comment:
  if (fseek(file,MTM_header.commentlen,SEEK_CUR))
    { status=WRONGFORMAT;return; }

  // load samples of instruments
  trace("Loading sample data...\n");
  for (i=1;i<MTM_header.no_Samples;i++)
  { BYTE loopflag=(Instrument[i].loopend-Instrument[i].loopstart>0)?1:0;
    //BYTE loopflag=Instrument[i].smplen>=Instrument[i].loopend;
    if (Instrument[i].smplen)
      status=Device->LoadInstrument(i,Instrument[i].smplen,8,loopflag,Instrument[i].loopstart,Instrument[i].loopend);
    if (status!=0) return;
  }
  trace("loading done.\n");
}

DWORD MTM_c::readSMP8_BLK(BYTE * dest,DWORD size,BYTE _bits)
{ printf("loading %ld bytes.\n",size);
  while (!feof(file) && size) { *(dest++)=(BYTE)getc(file);size--; }
  return(size);
}

DWORD MTM_c::readSMP16_BLK(WORD * dest,DWORD size,BYTE _bits)
{ printf("loading %ld bytes.\n",size);
  while (!feof(file) && size) { *(dest++)=((WORD)getc(file)<<8);size--; }
  return(size);
}
