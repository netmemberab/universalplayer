/*****************************************************************************
   MOD   support routines for music file format MOD
   - 15 instrument MODs are not supported
   - MOD-types: FastTracker1.0 ,NoiseTracker , TakeTracker, Octamed
     and other well known (IDs: FLT,OCTA,xxCH,xCHN,M.K.,M!K!)
   - patterns will be packed while loading
  --------------------------------------------------------------------------
   (c) copyright 1995,96 by A.Baresel
    Created on 21.1.1996
    reassigned for new player interface on 29.10.1996
*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "arch/xendian.h"
#include "player.h"
#include "modules/modfile.h"
#include "instrmts/smpfile.h"

/* magic number for playing speed:
   PAL    : 7093789.2 / 2
   NTSC   : 7159090.5 / 2
   my own : 7158728.0 / 2 = 428*8363   (play C-2 at 8363 Hz)
*/
#define magicnum 3579364lu

#if defined(__DOS__) && !defined(__GNUC__)
  #define strncasecmp strncmpi
#endif

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

// /--------------------------------------------------------------------------------\
// |- main loader: -----------------------------------------------------------------|
// \--------------------------------------------------------------------------------/
int fileread(void *buf,long int c,FILE* f)
{ return fread(buf,1,c,f)!=c; }

void MOD_c::load_module(void)
{ char id[5];
  int i,j,k,m,pp;
  long pm=0;
  streampos startoffset;
  int patlen; // unpacked pattern size
  unsigned char pat[32*64*4]; // maximum size of a unpacked pattern
  unsigned char packed[32*64*4+64]; // worst case: should never(!) happen with this format !
				    // whatever is stored in pattern !
  SampleFileLoaderClass smpload;
  trace("loading Protracker module.\n");
  ModuleClass::id=(ModuleID)this;
  
  startoffset=ftell(modin);
  // detect typ
  if (fseek(modin,startoffset+1080,SEEK_SET) || fileread(id,4,modin) )
    { status=BADMUSICFILE;return; }
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
  { status=BADMUSICFILE;return; }
  trace("type ok\n");
  #ifdef __DEBUGGING__
  printf("Number of channels: %i\n",no_Channels);
  #endif

  if (no_Channels>32)
  { trace("Too many channels. Format not supported.\n");
    status=NO_SUPPORT;return;
  }

  fseek(modin,startoffset,SEEK_SET);
  if (fileread(songname,20,modin))
    { status=BADMUSICFILE;return; }   
  printf("Modulename: \"%s\"\n",songname);

  // first load instruments: *#*#*#*#*#*#*#*#*#*

  for (i=1;i<32;i++)
  { if (fileread(&Instrument[i],30,modin))
      { status=BADMUSICFILE;trace("bad music file.\n");
	printf("State: %i\n",ferror(modin));return; } 

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

  if ((fileread(&no_Orders,1,modin)) ||        /* Orders to play      */
      (fileread(&restart_pos,1,modin)) ||   /* ??? not defined ??? */
      (fileread(&order[0],128,modin)) ||       /* order table         */
      (fileread(&Id,4,modin)) )                /* 4 byte ID string    */
    { status=BADMUSICFILE;return; } 

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
    if (fileread(&pat,patlen,modin))
      { status=BADMUSICFILE;return; } 

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
	{ status=NO_MEMORY;return; } 
      memcpy(Pattern_p[i],packed,pp);
    }
    pm+=pp;
  }
  #ifdef __DEBUGGING__
  printf(" average packrate : %3.2f%%\n",100.0*pm/no_Pattern/patlen);
  #endif

  // load samples of instruments
  trace("Uploading samples...\n");
  { int n=0; 
    for (i=1;i<32;i++) if (Instrument[i].smplen>2) 
    { n++;
      #ifdef __DEBUGGING__
        printf("%i,",i);
      #endif
    }
    instruments=device->AllocateInstruments((InstrumentIDs)&instrIDs,n);
    trace("---\n");
    if (instruments==NO_RESOURCE) { status=NO_RESOURCES;return; }
    #ifdef __DEBUGGING__
      printf("Device instrument resource id: %lu\n",instruments);
    #endif
    n--;
    #ifdef __DEBUGGING__
      for (i=31;i>0;i--) 
      { printf("(%i)",i);
        if (Instrument[i].smplen>2) 
	  printf("<%li>",instrIDs[i]=instrIDs[n--]); 
        else printf("!%li!",instrIDs[i]=NO_INSTRID);
      }
    getchar();
    #else
      for (i=31;i>0;i--) 
      { if (Instrument[i].smplen>2) instrIDs[i]=instrIDs[n--];
        else instrIDs[i]=NO_INSTRID;
      }
    #endif

    instrIDs[0]=NO_INSTRID;
  }
  for (i=1;i<32;i++)
    if (Instrument[i].smplen>2)
    { // register & upload sample
      Status st;
      #ifdef __DEBUGGING__
        printf("Register and Upload instrument %i: %li\n",i,instrIDs[i]);
      #endif
      if ((st=smpload.RegisterSample(instrIDs[i],SMP_SIGNED_8BIT,modin,
		Instrument[i].smplen,
		Instrument[i].loopstart,
		Instrument[i].loopstart+Instrument[i].looplen,
		Instrument[i].loopflag))!=NO_ERROR)
      { trace("Error uploading sample. (register)\n");
        status=st;return;
      }
      if (st=device->UploadInstrument(instrIDs[i],smpload))
      { trace("Error uploading sample. (upload)\n");
	status=st;return;
      }
    }
  trace("loading done.\n");
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

void MOD_c::effect_handling(DeviceCommandsStruct * devcmd)
{ 
  int i;
  char al;
  devcmd->no_commands=no_Channels;
  for (i=0;i<no_Channels;i++)
  { struct MOD_c::channel_t *MOD_chn=&chn[i];
    DeviceCommand *MIX_chn=&(devcmd->Commands[i]);
    switch (MOD_chn->useeffect) // new effect numbers
    { case 0x1:  // arpeggio
	{ int period;
	  MOD_chn->ArpeggioPos++;MOD_chn->ArpeggioPos%=3;
	  period=MOD_chn->periods[MOD_chn->ArpeggioPos];
	  MIX_chn->Frequency=magicnum/period;
	  MIX_chn->Changes|=CHG_FREQUENCY;
	  break;
	}
      case 0x2: // Slide Up
	if (MOD_chn->period)
	{ int p=MOD_chn->period-MOD_chn->parameter;
	  MOD_chn->period=(p<15)?15:p; 
	  MIX_chn->Frequency=magicnum/MOD_chn->period;
	  MIX_chn->Changes=CHG_FREQUENCY;
	}
	break;
      case 0x3: // Slide Down
	if (MOD_chn->period)
	{ int p=MOD_chn->period+MOD_chn->parameter;
	  MOD_chn->period=(p>1712)?1712:p;
	  MIX_chn->Frequency=magicnum/MOD_chn->period;
	  MIX_chn->Changes=CHG_FREQUENCY;
	}
	break;

// Probably the ugliest part of the code:
// (anyway, it does work and I'm to lazy for some copying arround)
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
	MIX_chn->Changes|=CHG_FREQUENCY; \
	MIX_chn->Frequency=magicnum/period;
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
	  MIX_chn->Changes|=CHG_FREQUENCY;\
	  MIX_chn->Frequency=magicnum/period;\
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
	  char al=(int)(MOD_chn->parameter>>4)-(int)(MOD_chn->parameter&0xf)+MOD_chn->volume;
	  al=(al<0)?0:(al>63)?63:al;
	  MIX_chn->Volume=(MOD_chn->volume=al);
	  MIX_chn->Changes|=CHG_VOLUME;
	  // toneporta:
	  do_portamento(0x9);
	  break;
	}
      case 0x7:  // Vibrato + Volume Slide
	{ int period;
	  // slide volume:
	  char al=(MOD_chn->parameter>>4)-(MOD_chn->parameter&0xf)+MOD_chn->volume;
	  al=(al<0)?0:(al>63)?63:al;
	  MIX_chn->Volume=(MOD_chn->volume=al);
	  MIX_chn->Changes|=CHG_VOLUME;
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
	  MIX_chn->Changes|=CHG_VOLUME;
	  MIX_chn->Volume=volume;
	  break;
	}
      case 0x9: // Volume Slide
	//printf("t(%X),",(int)MOD_chn->volume);
	al=char(MOD_chn->parameter>>4)+char(MOD_chn->volume)-char(MOD_chn->parameter&0x0f);
        if (al<0) al=0;
        if (al>64) al=64;
	MIX_chn->Volume=(MOD_chn->volume=al);
	MIX_chn->Changes|=CHG_VOLUME;
	break;
      case 0xA: // E9 - Retrig Note
	if (!MOD_chn->ctick--) // retrigg if zero
	{ MOD_chn->ctick=MOD_chn->numticks;
	  MIX_chn->Changes|=CHG_SETOFFSET|CHG_CONTINUE;
	  MIX_chn->Offset=0;
	}
	break;
      case 0xB: // EC - NoteCut
	if (!MOD_chn->ctick--) // cut it if zero
	{ MIX_chn->Volume=0;
	  MIX_chn->Changes|=CHG_VOLUME;
	  MOD_chn->useeffect=0;
	}
	break;
      case 0xC: // ED - NoteDelay
	if (!--MOD_chn->ctick) // start playing it if zero
	{
	  if (MOD_chn->sInst)
	  { // set a new instrument
	    MIX_chn->instrid=instrIDs[MOD_chn->instr=MOD_chn->sInst];
	    MOD_chn->finetune=Instrument[MOD_chn->sInst].finetune;
	    MIX_chn->Changes|=CHG_VOLUME;
	    MOD_chn->volume=MIX_chn->Volume=Instrument[MOD_chn->sInst].volume;
	  } else MIX_chn->instrid=instrIDs[MOD_chn->instr];
	  if (MOD_chn->sPeriod)
	  { // start the delayed note
	    MOD_chn->period=MOD_chn->sPeriod-(MOD_chn->sPeriod*MOD_chn->finetune>>7);
	    MIX_chn->Frequency=magicnum/MOD_chn->period;
	    if (MOD_chn->sInst) MIX_chn->Changes|=CHG_INSTRUMENT|CHG_FREQUENCY;
	    else
	    { MIX_chn->Changes|=CHG_FREQUENCY|CHG_SETOFFSET|CHG_CONTINUE;
	      MIX_chn->Offset=MOD_chn->startoffset;
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

void MOD_c::setup_channel(DeviceCommandsStruct * devcmd,BYTE chnnr,
			  int period,short instr,short effno, short effpar)
{
  struct MOD_c::channel_t *MOD_chn=&chn[chnnr];
  DeviceCommand *MIX_chn=&(devcmd->Commands[chnnr]);
  unsigned int converteff[16]={1,2,3,4,5,6,7,8,0,0,9,0,0,0,0,0};
  unsigned int conv2eff[16]={0,0,0,0,0,0,0,0,0,10,0,0,11,12,0,0};
  if (effno==0xe&&(effpar>>4)==0xd) // note delay
  { MOD_chn->sPeriod=period;period=0;
    MOD_chn->sInst=instr;instr=0;
    MOD_chn->ctick=effpar&0xf;
  }
  if (instr)
  { // set a new instrument
    MIX_chn->instrid=instrIDs[MOD_chn->instr=instr];
    MOD_chn->finetune=Instrument[instr].finetune;
    MIX_chn->Changes|=CHG_VOLUME;
    MOD_chn->volume=MIX_chn->Volume=Instrument[instr].volume;
    MOD_chn->startoffset=0;
    if (!period && MOD_chn->period) MIX_chn->Changes|=CHG_RESTART;
    else MIX_chn->Changes|=CHG_INSTRUMENT;
  } else MIX_chn->instrid=instrIDs[MOD_chn->instr];

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
      MIX_chn->Frequency=magicnum/MOD_chn->period;
      MIX_chn->Changes|=CHG_FREQUENCY;
    }
  if (MOD_chn->useeffect==0x1 && // last effect was arpeggio
       (effno||!effpar))         // and this arpeggio ends now
    { MIX_chn->Frequency=magicnum/MOD_chn->period;
      MIX_chn->Changes|=CHG_FREQUENCY;
    }

  MOD_chn->effect=effno;
  MOD_chn->useeffect=converteff[effno];

  if (!effno && !effpar) MOD_chn->useeffect=0;
  if (period)
  { // amiga finetune ... pretty easy, isn't it ?
    period=period-(period*MOD_chn->finetune>>7);
    MOD_chn->period=period;
    MOD_chn->Slideto=0; // stop old portamento (if there was...)
    MIX_chn->Frequency=magicnum/period;
    if (instr) MIX_chn->Changes|=CHG_INSTRUMENT|CHG_FREQUENCY;
    else
    { MIX_chn->Changes|=CHG_FREQUENCY|CHG_SETOFFSET|CHG_CONTINUE;
      MIX_chn->Offset=MOD_chn->startoffset; } 
  } 
  
  // all effects have to be processed after period&instr:
  switch (effno) 
  { case 0x4:  // Vibrato 
      if (effpar) 
      { if (effpar&0xf0) MOD_chn->VibSpeed=effpar>>4;
	MOD_chn->VibDepth=effpar&0xf; } 
      if (!MOD_chn->period)
	MOD_chn->useeffect=0; 
      break; 
    case 0x1:  // pitch slide up
    case 0x2:  // pitch slide down
    case 0x5:  // Tone Portamento +  Volume Slide
    case 0x6:  // Vibrato + Volume Slide
    case 0x7:  // Tremolo 
    case 0x8:  // NOT USED ??  
    case 0xA:  // Volume Slide
      storeifn0; break; 
    case 0x9:  // set offset only if (period)!
       storeifn0; 
	if (period) {
	  MIX_chn->Changes|=CHG_SETOFFSET;
	  MOD_chn->startoffset=MIX_chn->Offset=(WORD)effpar<<8;
	}
      break;
    case 0xB:  // Position Jump
      storeifn0;
      positionjump=1;
      pjumpto=effpar;
      break;
    case 0xC:  // set volume
      storeifn0;
      MIX_chn->Changes|=CHG_VOLUME;
      if (effpar>63) effpar=63;
      MOD_chn->volume=MIX_chn->Volume=effpar;
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
	      MIX_chn->Frequency=magicnum/MOD_chn->period;
	      MIX_chn->Changes=CHG_FREQUENCY;
	    }
	    break;
	  case 0x2: // E2 - FineSlide Down
	    if (MOD_chn->period)
	    { int p=MOD_chn->period;
	      if (p+(int)MOD_chn->parameter>1712) MOD_chn->period=1712;
	      else MOD_chn->period=p+(int)MOD_chn->parameter;
	      if (MOD_chn->period) MIX_chn->Frequency=magicnum/MOD_chn->period;
	      MIX_chn->Changes=CHG_FREQUENCY;
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
	    MIX_chn->Panning=effpar&0xf;
	    MIX_chn->Changes|=CHG_PANNING;
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
	      MIX_chn->Volume=(MOD_chn->volume=t);
	      MIX_chn->Changes|=CHG_VOLUME;
	    }  
	    break;
	  case 0xB:  // EB - Fine VolumeSlide Down
	    { short t=MOD_chn->volume-(effpar&0xf);
	      if (t<0) t=0;
	      MIX_chn->Volume=(MOD_chn->volume=t);
	      MIX_chn->Changes|=CHG_VOLUME;
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
    case 0xF: // speed or tempo Changes
      storeifn0;
      if (effpar && effpar<32) c_speed=effpar;
      if (effpar && effpar>31) c_tempo=effpar;
      break;
  }
}

void MOD_c::read_new_notes(DeviceCommandsStruct * devcmd)
{ int info;
  int ch=0;
  // clear global effects:
  patternbreak=0;
  patternloop=0;
  positionjump=0;
  devcmd->no_commands=no_Channels;
  do
  { info=*(c_position++);
    switch (info>>5)
    {
      case 0x00: // nothing stored (only next channel)
	    { int i=info;
	      if (!i) i=no_Channels;
	      while (ch<i) setup_channel(devcmd,ch++,0,0,0,0);
	      break;
	    }
      case 0x1: // 001 -> only period (2 bytes) ***** + next byte= period
	    { int period=((info&0xf)<<8)+*(c_position++);
	      setup_channel(devcmd,ch,period,0,0,0);
	      ch++;
	      break;
	    }
      case 0x2: //010 -> only effect no (1 byte)
	    { int effno=(info&0x0f);
	      setup_channel(devcmd,ch,0,0,effno,0);
	      ch++;
	      break;
	    }
      case 0x3: // 011 -> effno & par (2 bytes) *****=effect,parameter follows
	    { int effno=(info&0x0f);
	      int effpar=*(c_position++);
	      setup_channel(devcmd,ch,0,0,effno,effpar);
	      ch++;
	      break;
	    }
      case 0x4: // 100 -> instr & effectno (2 byte) *****=instr,effect follows
	    { int instr=info&0x1f;
	      int effno=*(c_position++);
	      setup_channel(devcmd,ch,0,instr,effno,0);
	      ch++;
	      break;
	    }
      case 0x5: // 101 -> no period   (3 bytes) ***** = instr,effect no & par follows
	    { int instr=info&0x1f;
	      int effno=*(c_position++);
	      int effpar=*(c_position++);
	      setup_channel(devcmd,ch,0,instr,effno,effpar);
	      ch++;
	      break;
	    }
      case 0x6: // 110 -> no effect parameter (3 bytes) ******=instrument,period,effno follows
	    { int instr=info&0x1f;
	      unsigned char a=*(c_position++);
	      int effno=a>>4;
	      int period=(((int)a&0xf)<<8)+*(c_position++);
	      setup_channel(devcmd,ch,period,instr,effno,0);
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
	      setup_channel(devcmd,ch,period,instr,effno,effpar);
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
      if (pjumpto<c_order) StartModule(); // kind of loop!
      c_order=pjumpto;
    } else
    if (++c_order==no_Orders) // end of song ... loop
    { StartModule();
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

Status MOD_c::GetDeviceCommands(DeviceCommandsStruct& devcmd)
{ 
  if (!c_tick && patterndelay)
  { patterndelay--;
    c_tick = c_speed-1;
  }
  if (c_tick) effect_handling(&devcmd); 
  else read_new_notes(&devcmd);
  devcmd.countervalue=125*devcmd.timerbase/(50L*c_tempo);
  return( NO_ERROR );
}

void MOD_c::delete_module(void)
{ int no;
  trace("Remove MOD from memory.\n");
  // sorry, loading routines are not yet prepared for this.
  // a corrupt module might stop the loading process and that may
  // cause problems if not all pointers were set. Check this first !
  /*for (no=0;no<no_Pattern;no++)
    if (Pattern_p[no]!=NULL) free(Pattern_p[no]);
  */
  trace("END delete_module\n");
}

// panning:
#define left 200
#define right (255-left)

Status MOD_c::StartModule()
{ int i;
  trace("StartModule\n");
  c_order=0; c_row=0; c_tick=0;
  c_position = Pattern_p[order[0]];
  c_speed=6; c_tempo=125;
  patterndelay=0;
  /* CHANNEL START SETTINGS
  BYTE pandata[]={left,right,right,left }; // left,right,right,left
  memset(&(chn),0,sizeof(chn));
  for (i=0;i<no_Channels;i++)
     Device->SetupChannel(i,CHG_PANNING,(void*)&(pandata[i%4]));
  */
  return NO_ERROR;
}
