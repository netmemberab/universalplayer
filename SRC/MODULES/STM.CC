
// STM is like MOD -> so change MOD code to STM features:

/*****************************************************************************
   MOD   support routines for music file format MOD
   - 15 instrument MODs are not supported
   - MOD-types: FastTracker1.0 ,NoiseTracker , TakeTracker, Octamed
     and other well known (IDs: FLT,OCTA,xxCH,xCHN,M.K.,M!K!)
  --------------------------------------------------------------------------
   (c) copyright 1995,96 by A.Baresel
    Created on 21.1.1996
*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mod.h"
#include "module.ids"
#include "device.h"
#include "xendian.h"

/* magic number for playing speed:
   PAL    : 7093789.2 / 2
   NTSC   : 7159090.5 / 2
   my own : 7158728.0 / 2 = 428*8363   (play C-2 1:1)
*/
#define magicnum 3579364lu

#if defined(__DOS__) && !defined(__GNUC__)
  #define strncasecmp strncmpi
#endif

// variables :
static const char * MOD_idstr=MOD_ID;

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

//struct typ_MOD_header MOD_header;

// /--------------------------------------------------------------------------------\
// |- main loader: -----------------------------------------------------------------|
// \--------------------------------------------------------------------------------/

void MOD_c::load_module(void)
{ char id[5];
  int i,j,k,m,pp;
  long pm=0;
  startoffset=0;
  int patlen; // unpacked pattern size
  unsigned char pat[32*64*4]; // maximum size of a unpacked pattern
  unsigned char packed[32*64*4+64]; // worst case: should never(!) happen with this format !
				    // whatever is stored in pattern !
  trace("loading Protracker module.\n");
  Module=this;
  ModuleID=MOD_idstr;
  // detect typ
  if (fseek(file,startoffset+1080,SEEK_CUR) || fread(id,1,4,file)!=4 )
    { status=WRONGFORMAT;return; }

  /*
     different types are supported:
     xxCH ... xx channels
     xCHN ... x channels
     FLTx ... x channels
     OCTA ... 8 channels (really correct? I read about different structur of oktalizers)
     M.K. ... 4 channels
     M!K! ... 4 channels
     ---- there're some more ----
     only new format with 31 Instruments is supported !
  */

  no_Instr =31;
  if (!strncasecmp(&id[2],"CH",2))
    { id[2]=0;no_Channels=atoi(id); } else
  if (!strncasecmp(&id[1],"CHN",3))
    { id[1]=0;no_Channels =atoi(id); } else
  if (!strncasecmp(id,"FLT",3))
    { no_Channels=id[3]-'0'; } else
  if (!strncasecmp(id,"OCTA",4) || !strncasecmp(id,"M.K.",4) || !strncasecmp(id,"M!K!",4))
    { no_Channels=4; } else
  { status=WRONGFORMAT;return; }
   
  #ifdef __DEBUGGING__
  printf("Number of channels: %i\n",no_Channels);
  #endif

  if (no_Channels>32)
  { trace("Too many channels. Format not supported.\n");
    status=NOSUPPORT;return;
  }

  fseek(file,0 /* startoffset*/,SEEK_SET);
  if (fread(&songname,1,20,file)!=20)
    { status=WRONGFORMAT;return; }   

  // first load instruments: *#*#*#*#*#*#*#*#*#*

  for (i=1;i<32;i++)
  { if (fread(&Instrument[i],1,30,file)!=30)
      { status=WRONGFORMAT;return; } 

    // convert amiga formats to intel:
    WORD h;
    big2m_w(&Instrument[i].smplen);Instrument[i].smplen<<=1;
    big2m_w(&Instrument[i].loopstart);Instrument[i].loopstart<<=1;
    big2m_w(&Instrument[i].looplen);Instrument[i].looplen<<=1;

    // convert 4bit signed nibbel to 8bit signed character 
    if (Instrument[i].finetune>7) Instrument[i].finetune-=16;

    if (Instrument[i].loopstart+Instrument[i].looplen>Instrument[i].smplen)
	Instrument[i].looplen=Instrument[i].smplen-Instrument[i].loopstart;

    #ifdef __DEBUGGING__
    if (Instrument[i].smplen)
      printf("instr.%2i: \"%-22.22s\"  %u, %u, %u\n",i,Instrument[i].name,Instrument[i].smplen,Instrument[i].loopstart,Instrument[i].looplen);
    #endif

    Instrument[i].loopflag=(Instrument[i].looplen>2)?1:0;
  }
  getchar();

  if (fread(&no_Orders,1,1,file)!=1 ||        /* Orders to play */
      fread(&restart_pos,1,1,file)!=1 ||      /* ??? not defined ??? */
      fread(&order[0],1,128,file)!=128 ||        /* order table */
      fread(&Id,1,4,file)!=4)                 /* 4 byte ID string */
    { status=WRONGFORMAT;return; } 

  trace("order loaded.\n");

  if (restart_pos>no_Orders) restart_pos=0;

  // find out number of patterns are stored
  no_Pattern=0;
  for (i=0;i<128;i++)
  {
    if (order[i]>no_Pattern) no_Pattern=order[i];
    #ifdef __DEBUGGING__
    printf("%i,",order[i]);
    #endif
  }
  no_Pattern++;

  #ifdef __DEBUGGING__
  printf("Number of patterns detected: %u\n",no_Pattern);
  #endif 

  // load patterns and pack them:
  patlen=((int)no_Channels)<<8;             /* 4*64 bytes per channel */
  for (i=0;i<no_Pattern;i++)
  { // load data
    #ifdef Debugging
    printf("load pattern %i:",i);
    #endif
    if ((BYTE)fread(&pat,1,patlen,file)!=(BYTE)patlen)
      { status=WRONGFORMAT;return; } 

    /* and now pack it:

       per channel:
	       xyz******:  xyz:  000 -> nothing (1 byte)
				     (xxxxxx - next channel with something
				      000000 - last channel)
				 001 -> only period (2 bytes) ***** + next byte= period
				 010 -> only effect no (1 byte)
				 011 -> effno & par (2 bytes) *****=effect,parameter follows
				 100 -> instr & effectno (2 byte) *****=instr,effect follows
				 101 -> no period   (3 bytes) ***** = instr,effect no & par follows
				 110 -> no effect parameter (3 bytes) ******=instrument,period,effno follows
				 111 -> format as it is (4 bytes)

       * for 4 channel MODs pack rate is usally 40 to 50%
       * the more channels the better the pack rate (I saw 28chs with
	 23% packrate)
    */
    pp=0; // position in packed pattern
    k=0;  // line position in unpacked pattern
    for (j=0;j<64;j++) // do it for all 64 rows in a pattern
    {
      int flag=0;
      for (m=0;m<no_Channels;m++)
      { int l=k+4*m;
	int period=((int)(pat[l]&0xf)<<8)+pat[l+1];
	int instr =(pat[l]&0xf0)+(pat[l+2]>>4);
	int effno =pat[l+2]&0xf;
	int effpar=pat[l+3];
	if (!pat[l] && !pat[l+1] && !pat[l+2] && !pat[l+3]) { flag=1;} // store nothing for that channel
	else
	{ if (flag) packed[pp++]=m;  // next stored channel
	  if (!instr && !effno && !effpar) // only period
	  { packed[pp++]=0x20+(period>>8);
	    packed[pp++]=period&0xff;
	  } else
	  if (!period && !instr && !effpar)      // only effect no
	  { packed[pp++]=0x40+effno;
	  } else
	  if (!period && !instr)      // effect no and par
	  { packed[pp++]=0x60+effno;
	    packed[pp++]=effpar;
	  } else
	  if (!period && !effpar)      // instrument & effno
	  { packed[pp++]=0x80+instr;
	    packed[pp++]=effno;
	  } else
	  if (!period)                 // no period
	  { packed[pp++]=0xA0+instr;
	    packed[pp++]=effno;
	    packed[pp++]=effpar;
	  } else
	  if (!effpar)
	  { packed[pp++]=0xC0+instr;
	    packed[pp++]=(effno<<4)+(period>>8);
	    packed[pp++]=period&0xff;
	  } else // standart (but we loose nothing !)
	  { packed[pp++]=0xe0+(instr&0x10)+(period>>8);
	    packed[pp++]=period&0xff;
	    packed[pp++]=((instr<<4)&0xf0)+effno;
	    packed[pp++]=effpar;
	  }
	  flag=0;
	}
      } if (flag) packed[pp++]=0; // last channel !
      k+=no_Channels*4;
    }
    #ifdef __DEBUGGING__
    printf("  packed size: %u (%3.2f%%)\n",pp,100.0*pp/patlen);
    #endif
    {
      if ((Pattern_p[i] = (BYTE *)malloc(pp))==NULL)
	{ status=NOMEM;return; } 
      memcpy(Pattern_p[i],packed,pp);
    }
    pm+=pp;
  }
  #ifdef __DEBUGGING__
  printf(" average packrate : %3.2f%%\n",100.0*pm/no_Pattern/patlen);
  #endif

  // load samples of instruments
  trace("Loading Instruments...\n");
  for (i=1;i<32;i++)
  { 
    if (Instrument[i].smplen)
    {
      if ((status=Device->LoadInstrument(i,Instrument[i].smplen,8,Instrument[i].loopflag,
	   Instrument[i].loopstart,Instrument[i].loopstart+Instrument[i].looplen))!=0)
	return;
    }
  }
  trace("loading done.\n");
}

DWORD MOD_c::readSMP8_BLK(BYTE * dest,DWORD size,BYTE _bits)
{ // requested format is 8bit !
  while (!feof(file)&&size) { *(dest++)=128+getc(file);size--; }
  #ifdef __DEBUGGING__
    printf("done. (%ld)\n",size);
  #endif
  return(size);
}

DWORD MOD_c::readSMP16_BLK(WORD * dest,DWORD size,BYTE _bits)
{ // requested format is 16bit !
  while (!feof(file)&&size) { *(dest++)=128+(WORD)getc(file)<<8;size--; }
  #ifdef __DEBUGGING__
    printf("done. (%ld)\n",size);
  #endif
  return(size);
}

static const int fix[12]={1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907};

int get_period(int original,unsigned char dif)
{ int i=0,j=0;
  while ((fix[11]>>i)>original) i++;
  while ((fix[j]>>i)>original) j++;
  j+=dif;i+=j/12;j%=12;
  j=fix[j]>>i;// if (!j) j=1;
  return(j);
}

// 0-parameter handling:
#define reloadif0  if (!effpar) effpar=MOD_chn->parameter; else MOD_chn->parameter=effpar;
#define storeifn0  if (effpar) MOD_chn->parameter=effpar;

void MOD_c::effect_handling(struct playinfo_t * playinfo)
{ 
  int i;
  char al;
  for (i=0;i<no_Channels;i++)
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

// -- MACRO: --------------------------------------------
#define do_portamento(endeff) \
	period=MOD_chn->period; \
	if (MOD_chn->period>MOD_chn->Slideto) \
	{ \
	  period-=(unsigned int)MOD_chn->PortPara; \
	  if (period<MOD_chn->Slideto) \
	  { period=MOD_chn->Slideto; \
	    MOD_chn->useeffect=endeff; \
	  } \
	} else \
	{ period+=(unsigned int)MOD_chn->PortPara; \
	  if (period>MOD_chn->Slideto) \
	  { period=MOD_chn->Slideto; \
	    MOD_chn->useeffect=endeff; \
	  } \
	} \
	if (MOD_chn->Glisscontrol) \
	{ int i=get_period(period,0); \
	  period=i-(i*MOD_chn->finetune>>7); \
	} \
	else MOD_chn->period=period; \
	MIX_chn->changes|=CHG_FREQ; \
	MIX_chn->frequency=magicnum/period;
// -------------------------------------------------------

      case 0x4:
	{ int period;
	  do_portamento(0);
	  break; // Tone Portamento
	}

// MACRO: ---------------------------------------------
#define do_vibrato \
	{ int dl;\
	  MOD_chn->VibtabPos=(MOD_chn->VibtabPos+MOD_chn->VibSpeed)&63;\
	  dl=(*all_tabs[MOD_chn->VibwavNo&0x3])[MOD_chn->VibtabPos];\
	  dl=(dl*MOD_chn->VibDepth)>>7;\
	  period=MOD_chn->period+dl;\
	  MIX_chn->changes|=CHG_FREQ;\
	  MIX_chn->frequency=magicnum/period;\
	}
// ----------------------------------------------------

      case 0x5:  // Vibrato
	{ int period;
	  do_vibrato;
	  break;
	}
      case 0x6:  // Tone Portamento + Volume Slide
	// slide volume:
	{ int period;
	  char al=(MOD_chn->parameter>>4)-(MOD_chn->parameter&0xf)+MOD_chn->volume;
	  al=(al<0)?0:(al>63)?63:al;
	  MIX_chn->volume=(MOD_chn->volume=al);
	  MIX_chn->changes|=CHG_VOLUME;
	  // toneporta:
	  do_portamento(0x9);
	  break;
	}
      case 0x7:  // Vibrato + Volume Slide
	{ int period;
	  // slide volume:
	  char al=(MOD_chn->parameter>>4)-(MOD_chn->parameter&0xf)+MOD_chn->volume;
	  al=(al<0)?0:(al>63)?63:al;
	  MIX_chn->volume=(MOD_chn->volume=al);
	  MIX_chn->changes|=CHG_VOLUME;
	  // vibrato:
	  do_vibrato;
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
  c_tick--;
}

void MOD_c::jumpto(char row)
{
  while (row--!=0)
  { unsigned int chnnr=0;
    do
    { unsigned char info=*(c_position++);
      switch (info>>5)
      { case 0: chnnr=info?info-1:no_Channels; break;
	case 1: case 3: case 4: c_position++; break;
	case 5: case 6: c_position+=2; break;
	case 7: c_position+=3; break;
      }
      chnnr++;
    } while (chnnr<no_Channels);
  }
}

void MOD_c::setup_channel(struct playinfo_t *playinfo,BYTE chnnr,
			  int period,short instr,short effno, short effpar)
{
  struct MOD_c::channel_t *MOD_chn=&chn[chnnr];
  struct chninfo_t *MIX_chn=&(playinfo->chn[chnnr]);
  unsigned int converteff[16]={1,2,3,4,5,6,7,8,0,0,9,0,0,0,0,0};
  unsigned int conv2eff[16]={0,0,0,0,0,0,0,0,0,10,0,0,11,12,0,0};
  if (effno==0xe&&(effpar>>4)==0xd) // note delay
  { MOD_chn->sPeriod=period;period=0;
    MOD_chn->sInst=instr;instr=0;
    MOD_chn->ctick=effpar&0xf;
  }
  if (instr)
  { // set a new instrument
    MOD_chn->instr=MIX_chn->instno=instr;
    MOD_chn->finetune=Instrument[instr].finetune;
    MIX_chn->changes|=CHG_VOLUME;
    MOD_chn->volume=MIX_chn->volume=Instrument[instr].volume;
    MOD_chn->startoffset=0;
    if (!period && MOD_chn->period) MIX_chn->changes|=CHG_RESTART;
    else MIX_chn->changes|=CHG_INSTR;
  } else MIX_chn->instno=MOD_chn->instr;

  // all effects have to be proccessed before new period is set:
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Attention: setfinetune needs to be called _after_ setinstrument !! or it 
  // won't effect anything

  switch (effno)
  {
    case 0x0:  // arpeggio or nothing
      if (effpar)
      { int p=period?period:get_period(MOD_chn->period,0); // not finetuned period
	MOD_chn->parameter=effpar;
	if (period) MOD_chn->ArpeggioPos=0;
	if (p)
	{ int i;
	  MOD_chn->periods[0]=p-(p*MOD_chn->finetune>>7);
	  i=get_period(p,(unsigned char)effpar>>4);
	  MOD_chn->periods[1]=i-(i*MOD_chn->finetune>>7);
	  i=get_period(p,effpar&0xf);
	  MOD_chn->periods[2]=i-(i*MOD_chn->finetune>>7);
	} else effpar=0;
      }
      break;
    case 0x3: // Tone Portamento
      { if (effpar) MOD_chn->PortPara=effpar;
	if (period) { MOD_chn->Slideto=period-(period*MOD_chn->finetune>>7);period=0; }
	else if (!MOD_chn->Slideto) { effno=0;effpar=0; }
	break;
      }
    case 0x5: // Tone Portamento + VolSlide
      if (period) { MOD_chn->Slideto=period-(period*MOD_chn->finetune>>7);period=0; }
      else if (!MOD_chn->Slideto) effno=0xa;
      break;
    case 0x4: case 0x6: // vibratos
      if (((MOD_chn->useeffect!=0x5 && MOD_chn->effect!=0x7)||period) && 
	   MOD_chn->VibwavNo>>2)
	MOD_chn->VibtabPos=0;  // reset vibrato wave position
      break;
    case 0xE: // special effects need to be processed before period is set
      { if (effpar) MOD_chn->parameter=effpar;
	if ((effpar>>4)==0x5)  // set finetune
	  MOD_chn->finetune=((char)effpar<<4)>>4; // signed nibble to signed char
	break;
      }
  }

  // ending effects:

  if ((MOD_chn->useeffect==0x5 || MOD_chn->effect==0x7) && // last effect was a vibrato
      effno!=0x4 && effno!=0x6)                            // and this vibrato ends now
    { // restore old values
      MIX_chn->frequency=magicnum/MOD_chn->period;
      MIX_chn->changes|=CHG_FREQ;
    }
  if (MOD_chn->useeffect==0x1 && // last effect was arpeggio
       (effno||!effpar))         // and this arpeggio ends now
    { MIX_chn->frequency=magicnum/MOD_chn->period;
      MIX_chn->changes|=CHG_FREQ;
    }

  MOD_chn->effect=effno;
  MOD_chn->useeffect=converteff[effno];

  if (!effno && !effpar) MOD_chn->useeffect=0;
  if (period)
  { // amiga finetune ... pretty easy, isn't it ?
    period=period-(period*MOD_chn->finetune>>7);
    MOD_chn->period=period;
    MOD_chn->Slideto=0; // stop old portamento (if there was...)
    MIX_chn->frequency=magicnum/period;
    if (instr) MIX_chn->changes|=CHG_INSTR|CHG_FREQ;
    else
    { MIX_chn->changes|=CHG_FREQ|CHG_SETOFS|CHG_DOMIX;
      MIX_chn->offset=MOD_chn->startoffset; } 
  } 
  
  // all effects have to be processed after period&instr:
  switch (effno) 
  { case 0x1: case 0x2:
      storeifn0;break; // nothing else ! 
    case 0x4:  // Vibrato 
      if (effpar) 
      { if (effpar&0xf0) MOD_chn->VibSpeed=effpar>>4;
	MOD_chn->VibDepth=effpar&0xf; } 
      if (!MOD_chn->period)
	MOD_chn->useeffect=0; break; 
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
	  MOD_chn->startoffset=MIX_chn->offset=effpar<<8;
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
      MOD_chn->volume=MIX_chn->volume=effpar;
      break;
    case 0xD: // Patternbreak
      storeifn0;
      patternbreak=1;
      pbreakto=10*(effpar>>4)+(effpar&0xf);
      break;
    case 0xE:  // special effects:
      { storeifn0;
	MOD_chn->useeffect=conv2eff[effpar>>4];
	switch (effpar>>4)
	{ // case 0x0: break;   E0 - set filter no supported
	  case 0x1: // E1 - FineSlide Up
	    if (MOD_chn->period)
	    { int p=MOD_chn->period-15; 
	      MOD_chn->period=(p<(int)MOD_chn->parameter)?15:MOD_chn->period-(int)MOD_chn->parameter; 
	      MIX_chn->frequency=magicnum/MOD_chn->period;
	      MIX_chn->changes=CHG_FREQ;
	    }
	    break;
	  case 0x2: // E2 - FineSlide Down
	    if (MOD_chn->period)
	    { int p=MOD_chn->period;
	      if (p+(int)MOD_chn->parameter>1712) MOD_chn->period=1712;
	      else MOD_chn->period=p+(int)MOD_chn->parameter;
	      if (MOD_chn->period) MIX_chn->frequency=magicnum/MOD_chn->period;
	      MIX_chn->changes=CHG_FREQ;
	    }
	    break;
	  case 0x3: // E3 - Glissando Control
	    MOD_chn->Glisscontrol=effpar;
	    break;
	  case 0x4: // E4 - Set Vibrato Waveform
	    MOD_chn->VibwavNo=effpar&0xf;
	    break;
	  case 0x6: // E6 - Set/Jump to Loop
	    if (!(effpar&0xf)) // set loop point
	      MOD_chn->loopto=c_row;
	    else
	    { if (!MOD_chn->inside)
	      { MOD_chn->inside=1;
		MOD_chn->loops=effpar&0xf;
	      }
	      if (MOD_chn->loops)
	      { MOD_chn->loops--;
		patternloop=1;
		ploopto=MOD_chn->loopto;
	      } else
	      { MOD_chn->inside=0;
		MOD_chn->loopto=c_row+1;
	      }
	    }
	    break;
	  case 0x7: // E7 - Set Tremolo Waveform
	    MOD_chn->TrmwavNo=effpar&0xf;
	    break;
	  case 0x8: // E8 - SET PANPOSITION
	    MIX_chn->panposition=effpar&0xf;
	    MIX_chn->changes|=CHG_PANNING;
	    break;
	  case 0x9:  // E9 - Retrig Note
	    if (effpar&0xf&&MOD_chn->period)
	    { MOD_chn->numticks=effpar&0xf;
	      MOD_chn->ctick=(effpar&0xf);
	    } else MOD_chn->useeffect=0;
	    break;
	  case 0xA:  // EA - Fine VolumeSlide Up
	    { short t=MOD_chn->volume+(effpar&0xf);
	      if (t>63) t=63;
	      MIX_chn->volume=(MOD_chn->volume=t);
	      MIX_chn->changes|=CHG_VOLUME;
	    }  
	    break;
	  case 0xB:  // EB - Fine VolumeSlide Down
	    { short t=MOD_chn->volume-(effpar&0xf);
	      if (t<0) t=0;
	      MIX_chn->volume=(MOD_chn->volume=t);
	      MIX_chn->changes|=CHG_VOLUME;
	    }
	    break;
	  case 0xC: // EC - NoteCut
	    MOD_chn->ctick=effpar&0xf;
	    break;
	  case 0xE: // EE - PatternDelay
	    patterndelay=effpar&0xf;
	    break;
	  // case 0xF: break; EF - Invert Loop   not supported
	}
	break;
      }
    case 0xF: // speed or tempo changes
      storeifn0;
      if (effpar && effpar<32) c_speed=effpar;
      if (effpar && effpar>31) c_tempo=effpar;
      break;
  }
}

void MOD_c::read_new_notes(playinfo_t* playinfo_p)
{ int info;
  int ch=0;
  // clear global effects:
  patternbreak=0;
  patternloop=0;
  positionjump=0;
  do
  { info=*(c_position++);
    switch (info>>5)
    {
      case 0x00: // nothing stored (only next channel)
	    { int i=info;
	      if (!i) i=no_Channels;
	      while (ch<i) setup_channel(playinfo_p,ch++,0,0,0,0);
	      break;
	    }
      case 0x1: // 001 -> only period (2 bytes) ***** + next byte= period
	    { int period=((info&0xf)<<8)+*(c_position++);
	      setup_channel(playinfo_p,ch,period,0,0,0);
	      ch++;
	      break;
	    }
      case 0x2: //010 -> only effect no (1 byte)
	    { int effno=(info&0x0f);
	      setup_channel(playinfo_p,ch,0,0,effno,0);
	      ch++;
	      break;
	    }
      case 0x3: // 011 -> effno & par (2 bytes) *****=effect,parameter follows
	    { int effno=(info&0x0f);
	      int effpar=*(c_position++);
	      setup_channel(playinfo_p,ch,0,0,effno,effpar);
	      ch++;
	      break;
	    }
      case 0x4: // 100 -> instr & effectno (2 byte) *****=instr,effect follows
	    { int instr=info&0x1f;
	      int effno=*(c_position++);
	      setup_channel(playinfo_p,ch,0,instr,effno,0);
	      ch++;
	      break;
	    }
      case 0x5: // 101 -> no period   (3 bytes) ***** = instr,effect no & par follows
	    { int instr=info&0x1f;
	      int effno=*(c_position++);
	      int effpar=*(c_position++);
	      setup_channel(playinfo_p,ch,0,instr,effno,effpar);
	      ch++;
	      break;
	    }
      case 0x6: // 110 -> no effect parameter (3 bytes) ******=instrument,period,effno follows
	    { int instr=info&0x1f;
	      unsigned char a=*(c_position++);
	      int effno=a>>4;
	      int period=(((int)a&0xf)<<8)+*(c_position++);
	      setup_channel(playinfo_p,ch,period,instr,effno,0);
	      ch++;
	      break;
	    }
      case 0x7: // standart format
	    { unsigned char a1=*(c_position++);
	      unsigned char a2=*(c_position++);
	      int period=((int)(info&0xf)<<8)+a1;
	      int instr =(info&0x10)+(a2>>4);
	      int effno =a2&0xf;
	      int effpar=*(c_position++);
	      setup_channel(playinfo_p,ch,period,instr,effno,effpar);
	      ch++;
	      break;
	    }
    }
  } while (ch<no_Channels);
  if (patternloop)
  { c_position=Pattern_p[order[c_order]];
    c_row=ploopto;
    jumpto(ploopto);
  } else
  if (positionjump || patternbreak || c_row++==63)
  { // end of pattern !
    if (positionjump)
    { if (pjumpto>no_Orders) pjumpto=0;
      if (pjumpto<c_order) startmodule(); // kind of loop!
      c_order=pjumpto;
    } else
    if (++c_order==no_Orders) // end of song ... loop
    { startmodule();
      c_order=restart_pos;
    }
    c_position=Pattern_p[order[c_order]];
    // 'loop to' first row:
    for (ch=0;ch<no_Channels;ch++) chn[ch].loopto=0;
    if (patternbreak)
    { // run through packed pattern data to row we have to start with
      c_row=pbreakto;
      jumpto(pbreakto);
    }
    else c_row=0;
  }
  c_tick=c_speed-1;
}

short MOD_c::nextnotes(playinfo_t* playinfo_p)
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

void MOD_c::delete_module(void)
{ int no;
  trace("Remove MOD from memory.\n");
  /*for (no=0;no<no_Pattern;no++)
    free(Pattern_p[no]);
  */
}

short MOD_c::startmodule(void)
{ BYTE pandata[]={200,50,50,200 }; /* left,right,right,left */
  int i;
  c_order=0;
  c_row=0;
  c_tick=0;
  c_position = Pattern_p[order[0]];
  c_speed=6;
  c_tempo=125;
  patterndelay=0;
  memset(&(chn),0,sizeof(chn));
  for (i=0;i<no_Channels;i++)
    Device->SetupChannel(i,CHG_PANNING,(void*)&(pandata[i%4]));
  return(0);
}

