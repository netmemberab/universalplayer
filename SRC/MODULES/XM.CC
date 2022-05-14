/*****************************************************************************
   XM    support routines for Fasttracker II modules

  --------------------------------------------------------------------------
   (c) copyright 1995,96 by A.Baresel
    Created on 21.2.1996
*****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "global.h"
#include "xendian.h"
#include "xm.h"
#include "device.h"
#include "module.ids"

static const char * XM_idstr=XM_ID;

#ifdef __PLAYINFO__
/* - --- ------------------------------- --- -
      Additional code for play info system
class XMinfo_c:public ModuleInfo_c {
	public:
		
}
*/
/* - --- ------------------------------- --- - */
#endif

// keyoff note is 96 ?
#define KEY_OFF 96

// constants :
static const WORD period[12*8]=
     {907,900,894,887,881,875,868,862,856,850,844,838,832,826,820,814,
      808,802,796,791,785,779,774,768,762,757,752,746,741,736,730,725,
      720,715,709,704,699,694,689,684,678,675,670,665,660,655,651,646,
      640,636,632,628,623,619,614,610,604,601,597,592,588,584,580,575,
      570,567,563,559,555,551,547,543,538,535,532,528,524,520,516,513,
      508,505,502,498,494,491,487,484,480,477,474,470,467,463,460,457};

// only the positiv half of all waves
static BYTE av_sinus[128];
static BYTE av_flag_wave[128];
static BYTE av_rampright[128];
static BYTE av_rampleft[128];
static BYTE (*av_wave[4])[128]=
			 {&av_sinus,&av_flag_wave,&av_rampright,&av_rampleft};

// global volume: used in next_notes() & effect_handling()
static BYTE chg_glob_vol;

/*
 *  calculate_period(note,finetune): 
 *       calculates period value from note and finetune depending on
 *       linear or amiga stylish frequency table
 *
 */
WORD calculate_period(BYTE note,SBYTE finetune)
/* -> difficult thing - is it correct in that way ? <- */
{ int p,tab_p;
  WORD p_e;
  char shift=0;
  if (!((XM_c*)Module)->header.flags&FLAGS_LINEAR_FTAB)
  { // amiga frequency table:
    /********** (quote XM.TXT) *****************************************.
    # Period =								#
    # (PeriodTab[(Note MOD 12)*8 + FineTune/16]*(1-Frac(FineTune/16)) +	#
    #  PeriodTab[(Note MOD 12)*8 + FineTune/16]*(Frac(FineTune/16)))	#
    #       *16/2^(Note DIV 12);					#
    '********************************************************************/

    // C-code: (without finetune interpolation)
    tab_p=((note%12)<<3)+(finetune>>7);
    while (tab_p<0) { shift--;tab_p+=12*8; }
    while (tab_p>=12*8) { shift++;tab_p-=12*8; }
    p_e=period[tab_p];
    p=p_e*32>>(shift+note/12);
    // "The period is interpolated for finer finetune values"
    // Frequency = 8363*1712/Period;
  } else
  { // linear frequency table
    p = 10lu*12*16*4 - (note-1)*16lu*4 - finetune/2;
  }
  return(p);
}

/*
 * calculate_note(period,finetune):
 *     calculates note from period and finetune depending on used frequency 
 *     table this is used for arpeggio
 */
BYTE calculate_note(WORD period_val,SBYTE finetune)
{ BYTE note;
  if (!((XM_c*)Module)->header.flags&FLAGS_LINEAR_FTAB)
  { BYTE i=5;
    int j=0;
    WORD p;
    // find out shift value
    while ((period_val<<i)>period[0]) i--;
    p=period_val<<i;
    // find out table offset
    while (p<period[j]) j++;
    // calulate note number
    j-=finetune>>4;
    note=(j>>3)+i*12;
  } else
    note=(10lu*12*16*4-period_val-finetune/2)>>(2+4);
  return note;
}

/*
 * calculate_frequency(period):
 *       calculate the finally used frequency the sample is played with.
 */

DWORD calculate_frequency(WORD period_val)
{ if (!((XM_c*)Module)->header.flags&FLAGS_LINEAR_FTAB) // Amiga freqeuncy
   { if (period_val>0) return(14317456lu/period_val); else return(0); }
  else
   { // linear frequency table:
     //    Frequency = 8363*2^((6*12*16*4 - Period) / (12*16*4));

     float f=(6.0*12*16*4-period_val)/(12.0*16*4);
     return((long)(8363.0*pow(2,f)));
   }
}

// XM stylish effect numbers will be mapped to internally used numbers for
// easier effect lookup
#define NOEFFECT    0
#define ARPEGGIO    1
#define PORTA_UP    2
#define PORTA_DWN   3
#define PORTAMENT   4
#define VIBRATO     5
#define PORT_VSLD   6
#define VIBR_VSLD   7
#define TREMOLO     8
#define VOL_SLD     9
#define RETRIGG    10
#define NOTECUT    11
#define NOTEDLY    12
#define GLOB_VSLD  13
#define KEYOFF_CMD 14
#define PAN_SLIDE  15
#define MRETRIGG   16
#define TREMOR     17
#define EXTENDED   18

// convertion tables: for effect column
static char cnv_eff[256]= { ARPEGGIO, PORTA_UP, PORTA_DWN, PORTAMENT, VIBRATO,
  PORT_VSLD, VIBR_VSLD, TREMOLO, 0, 0, VOL_SLD, 0, 0, 0, EXTENDED, 0, 0,
  GLOB_VSLD, 0, 0, KEYOFF_CMD, 0, 0, 0, 0, PAN_SLIDE, 0, MRETRIGG, 0 , TREMOR, 0 , 
  0 , 0 , 0 , 0 , 0 };

static char cnv_xeff[]= { 0, 0, 0, 0, 0, 0, 0, 0, 0, RETRIGG, 0, 0, NOTECUT,
  NOTEDLY, 0 , 0};

// also volume column effects will be mapped:
#define V_NOEFFECT  0
#define V_VOL_DWN   1
#define V_VOL_UP    2
#define V_VIBRATO   3
#define V_PAN_LFT   4
#define V_PAN_RGT   5
#define V_PORTA     6

// lookup table to convert XM stylish numbers to internal numbers
char cnv_veff[16]= { 0,0,0,0,0,0, V_VOL_DWN, V_VOL_UP, 0, 0, 0, V_VIBRATO, 0,
  V_PAN_LFT, V_PAN_RGT, V_PORTA};

//
// volume slides code is used many times, that's why I put this in an
// independed methode (it's called in volslide,vib_volslide,porta_volslide)
//
void XM_c::do_volumeslide(struct playinfo_t *playinfo, int i)
{ 
  if (chn[i].VolSld_par&0xf0)
  { // vol slide up
    if (chn[i].volume+(chn[i].VolSld_par>>4)>64)
    { chn[i].volume=64;chn[i].eff2no=0; }
    else
      chn[i].volume+=(BYTE)chn[i].VolSld_par>>4;
    playinfo->chn[i].changes|=CHG_VOLUME;
  } else
  { // vol slide down
    if (chn[i].volume<chn[i].VolSld_par)
    { chn[i].volume=0;chn[i].eff2no=0; }
    else
      chn[i].volume-=chn[i].VolSld_par;
    playinfo->chn[i].changes|=CHG_VOLUME;
  }
  chn[i].tmp_volume=chn[i].volume; 
}

//
// effect handling for all effects need to be called every tick !
//
void XM_c::effect_handling(struct playinfo_t *playinfo)
{ int i;
  chg_glob_vol=global_vol;
  for (i=0;i<header.no_Channels;i++)
  { struct chninfo_t *MIX_chn=&(playinfo->chn[i]);
    chn[i].tmp_volume=chn[i].volume; // <- for tremolo & tremor

    // effects in volum colum
    switch (chn[i].eff1no)
    { case V_VOL_DWN: // volume slide down
	  if (chn[i].volume<chn[i].VolSld_par)
    	    { chn[i].volume=0;chn[i].eff1no=0; }
	  else
	    chn[i].volume-=chn[i].VolSld_par;
	  MIX_chn->changes=CHG_VOLUME;
	  break;
      case V_VOL_UP: // volume slide up
	  if (chn[i].volume+(chn[i].VolSld_par>>4)>64)
    	    { chn[i].volume=64;chn[i].eff1no=0; }
	  else
	    chn[i].volume+=(BYTE)chn[i].VolSld_par>>4;
	  MIX_chn->changes=CHG_VOLUME;
	  break;
      case V_VIBRATO: // vibrato
	  { SWORD cd;
	    chn[i].vib_tabcount+=4*chn[i].Vib_speed;
	    cd=((*chn[i].vib_wavtab)[chn[i].vib_tabcount&127]*chn[i].Vib_depth)>>7;
	    if (chn[i].vib_tabcount>127) cd=-cd;
	    MIX_chn->frequency=calculate_frequency(chn[i].period+(cd<<2));
	    MIX_chn->changes|=CHG_FREQ;
	  }
	  break;
      case V_PAN_LFT: // panning slide left (to 0)
	  if ((WORD)chn[i].panning<chn[i].PanSld_par)
    	    { chn[i].panning=0;chn[i].eff1no=0; }
	  else
	    chn[i].panning-=chn[i].PanSld_par;
	  MIX_chn->changes=CHG_PANNING;
	  break;
      case V_PAN_RGT: // panning slide right (to 255)
	  if ((WORD)chn[i].panning+(chn[i].PanSld_par>>4)>255)
    	    { chn[i].panning=255;chn[i].eff1no=0; }
	  else
	    chn[i].panning+=(BYTE)chn[i].PanSld_par>>4;
	  MIX_chn->changes=CHG_PANNING;
	  break;
      case V_PORTA: // tone portamento
	 { WORD delta=(WORD)chn[i].Port_par<<2;
	   if (chn[i].period>chn[i].SlideTo)
	   { if ((chn[i].period-=delta)<chn[i].SlideTo)
	     { chn[i].period=chn[i].SlideTo;
	       chn[i].eff1no=0;
	     }
	   } else
	   { if ((chn[i].period+=delta)>chn[i].SlideTo)
	     { chn[i].period=chn[i].SlideTo;
	       chn[i].eff1no=0;
	     }
	   }
	   if (chn[i].glisscontrol)
	   { // do glissando ... (round to nearest note value) 
	     // ... hope that works correct in all cases ...
             MIX_chn->frequency=
		calculate_frequency( calculate_period(
		    calculate_note(chn[i].period,chn[i].finetune),chn[i].finetune
                ));
	   }
	   if (chn[i].period)
	   { MIX_chn->changes|=CHG_FREQ;
	     MIX_chn->frequency=calculate_frequency(chn[i].period);
	   }
	 }
         break;
    }
    // effects in effect column - like protracker plus some extentions (e.g. from S3M)
    switch (chn[i].eff2no)
    { case   ARPEGGIO: // arpeggio
        if ((chn[i].count2=(chn[i].count2+1)%3)!=0)
	{ BYTE note;
	  note=calculate_note(chn[i].period,chn[i].finetune)+chn[i].arp_delta[chn[i].count2];
          MIX_chn->frequency=calculate_frequency(calculate_period(note,chn[i].finetune));
	} else MIX_chn->frequency=calculate_frequency(chn[i].period);
        MIX_chn->changes|=CHG_FREQ;
        break;
      case   PORTA_UP: // pitch up
	chn[i].period-=(chn[i].Sldup_par<<2);
	MIX_chn->frequency=calculate_frequency(chn[i].period);
	MIX_chn->changes|=CHG_FREQ;
	break;
      case   PORTA_DWN: // pitch down
	chn[i].period+=(chn[i].Slddwn_par<<2);
	MIX_chn->frequency=calculate_frequency(chn[i].period);
	MIX_chn->changes|=CHG_FREQ;
	break;
      case   PORT_VSLD: // portamento+volumeslide
         // do volume slide first and then handle portamento
	 do_volumeslide(playinfo,i);
      case   PORTAMENT: // portamento
	 { WORD delta=(WORD)chn[i].Port_par<<2;
	   if (chn[i].period>chn[i].SlideTo)
	   { if ((chn[i].period-=delta)<chn[i].SlideTo)
	     { chn[i].period=chn[i].SlideTo;
	       chn[i].eff2no=(chn[i].eff2no==PORT_VSLD)?0:VOL_SLD; /* end porta, but continue volume slide */
	     }
	   } else
	   { if ((chn[i].period+=delta)>chn[i].SlideTo)
	     { chn[i].period=chn[i].SlideTo;
	       chn[i].eff2no=(chn[i].eff2no==PORT_VSLD)?0:VOL_SLD; /* end porta, but continue volume slide */
	     }
	   }

	   if (chn[i].glisscontrol)
	   { // do glissando ... (round to nearest note value) 
	     // ... hope that works correct in all cases ...
             MIX_chn->frequency=
		calculate_frequency( calculate_period(
		    calculate_note(chn[i].period,chn[i].finetune),chn[i].finetune
                ));
	   }
	   if (chn[i].period)
	   { MIX_chn->changes|=CHG_FREQ;
	     MIX_chn->frequency=calculate_frequency(chn[i].period);
	   }
	 }
         break;
      case   VIBR_VSLD: // vibrato+volumeslide
	 do_volumeslide(playinfo,i);
      case   VIBRATO: // vibrato
	 { SWORD cd;
	   chn[i].vib_tabcount+=4*chn[i].Vib_speed;
	   cd=((*chn[i].vib_wavtab)[chn[i].vib_tabcount&127]*chn[i].Vib_depth)>>7;
	   if (chn[i].vib_tabcount>127) cd=-cd;
	   MIX_chn->frequency=calculate_frequency(chn[i].period+(cd<<2));
	   MIX_chn->changes|=CHG_FREQ;
	 }
	 break;
      case   TREMOLO: // tremolo
	 { SWORD cd;
	   chn[i].trml_tabcount+=4*chn[i].Trml_speed;
	   cd=((*chn[i].trml_wavtab)[chn[i].trml_tabcount&127]*chn[i].Trml_depth)>>6;
	   if (chn[i].trml_tabcount>127) cd=-cd;
	   cd+=chn[i].volume;if (cd>64) cd=64; if (cd<0) cd=0;
	   chn[i].tmp_volume=cd;
	   MIX_chn->changes|=CHG_VOLUME;
	 }
	 break;
      case  VOL_SLD: // volume slide
	 do_volumeslide(playinfo,i);
         break;
      case  RETRIGG: // retrigg note
	 if (!--chn[i].count2)
	 { MIX_chn->changes|=CHG_SETOFS|CHG_DOMIX;
	   MIX_chn->offset=0;
	   chn[i].count2=chn[i].rtrg_len;
	 }
	 break;
      case  NOTECUT: // note cut
	if (!--chn[i].count2) // cut it now !
	{ chn[i].volume=MIX_chn->volume=0;
	  MIX_chn->changes|=CHG_VOLUME;
	  chn[i].eff2no=0;
	}
	break;
      case  NOTEDLY: // note delay
	if (!--chn[i].count2) // delay is over...
	{ setup_channel(playinfo,i,chn[i].notedly_note,chn[i].notedly_instr,0,0,0);
	  chn[i].eff2no=0;
	}
	break;
      case  GLOB_VSLD: // global volume slide
	  { if (chn[i].Gvolsl_par&0xf0)
	    { // vol slide up
	      if (chg_glob_vol+chn[i].Gvolsl_par>64)
    	        { chg_glob_vol=64;chn[i].eff2no=0; }
	      else
	        chg_glob_vol+=(BYTE)chn[i].Gvolsl_par>>4;
	    } else
	    { // vol slide down
	      if (chg_glob_vol<chn[i].Gvolsl_par)
    	        { chg_glob_vol=0;chn[i].eff2no=0; }
	      else
	        chg_glob_vol-=chn[i].Gvolsl_par;
	    }
          break;
	  }
      case  KEYOFF_CMD: // key off command
	if (!--chn[i].count2) // key off now !
	{ struct typ_instrument*  cins_p = chn[i].instr_p;
          chn[i].ve_sustain=0; // don't halt at sustain point ...
          if (cins_p!=NULL)
    	  {
      	    if (cins_p->voltype&1) // only fade if volume envelope !
      	    { chn[i].fo_speed=cins_p->volfadeout; // no fade if = 0 (stay at this volume)
            } else // if no volume envelope stop mixing immediatly...
            MIX_chn->changes|=CHG_STOPMIX;
    	  }
	  chn[i].eff2no=0;
	}
	break;
      case  PAN_SLIDE: // panning slide
  	if (chn[i].PanSld_par&0xf0)
  	{ // pan slide right
    	  if ((WORD)chn[i].panning+(chn[i].PanSld_par>>4)>255)
    	  { chn[i].panning=255;chn[i].eff2no=0; }
    	  else chn[i].panning+=(BYTE)chn[i].PanSld_par>>4;
    	  playinfo->chn[i].changes|=CHG_PANNING;
  	} else
  	{ // pan slide left
    	  if (chn[i].panning<chn[i].PanSld_par)
    	  { chn[i].panning=0;chn[i].eff2no=0; }
    	  else chn[i].panning-=chn[i].PanSld_par;
    	  playinfo->chn[i].changes|=CHG_PANNING;
	}
	break;
      case  MRETRIGG: // multi retrigg note
	if (!--chn[i].mrtrg_count)
	{ SBYTE vsl=chn[i].mrtrg_volsld;
	  MIX_chn->changes|=CHG_SETOFS|CHG_DOMIX;
	  MIX_chn->offset=0;
	  chn[i].mrtrg_count=chn[i].mrtrg_len;
	  // volume changes:
	  switch (vsl)
	  { case 1: case 2: case 3: case 4: case 5:
	      { vsl=1<<(vsl-1);
	        if (chn[i].volume>vsl) chn[i].tmp_volume=(chn[i].volume-=vsl);
	          else chn[i].tmp_volume=chn[i].volume=0;
		MIX_chn->changes|=CHG_VOLUME;
	      }
	      break;
	    case 6: // 2 div 3
	      if (chn[i].volume)
	      { chn[i].tmp_volume=chn[i].volume=chn[i].volume*2/3;
		MIX_chn->changes|=CHG_VOLUME;
	      }
	      break;
	    case 7: // 1 div 2
	      if (chn[i].volume)
	      { chn[i].tmp_volume=chn[i].volume>>=1;
		MIX_chn->changes|=CHG_VOLUME;
	      }
	      break;
	    case 9: case 10: case 11: case 12: case 13:
	      { vsl=1<<(vsl-9);
	        if (64-chn[i].volume>vsl) chn[i].tmp_volume=(chn[i].volume+=vsl);
	          else chn[i].tmp_volume=chn[i].volume=64;
		MIX_chn->changes|=CHG_VOLUME;
	      }
	      break;
	    case 14: // 3 div 2
	      if (chn[i].volume)
	      { if (chn[i].volume<42)
		  chn[i].tmp_volume=chn[i].volume=chn[i].volume*3/2;
		else chn[i].tmp_volume=chn[i].volume=64;
		MIX_chn->changes|=CHG_VOLUME;
	      }
	      break;
	    case 15: // 2
	      if (chn[i].volume)
	      { if (chn[i].volume<32)
		  chn[i].tmp_volume=chn[i].volume<<=1;
		else chn[i].tmp_volume=chn[i].volume=64;
		MIX_chn->changes|=CHG_VOLUME;
	      }
	      break;
	  }
	}
	break;
      case  TREMOR: // tremor
	if (chn[i].Trm_again)
	{
	  chn[i].Trm_count=(chn[i].Trm_count+1)%chn[i].Trm_again;
	  if (chn[i].Trm_count>=chn[i].Trm_on) 
	    { chn[i].tmp_volume=0;MIX_chn->changes|=CHG_VOLUME; }
	  if (chn[i].Trm_count==0) 
	    { MIX_chn->changes|=CHG_VOLUME; }
	}
	break;
    }
  }
  c_tick--;
}

// some global pattern effects:
BYTE flag_pattern_break;
BYTE pattern_break_to;

BYTE flag_position_jump;
BYTE position_jump_to;

BYTE flag_pattern_loop;
BYTE pattern_loop_to;

BYTE patterndelay_count;

// ****************************************************************************
// setup_channel's called in next_notes() for each channel with its current 
// values also possible with (0,0,0...) if nothing is specified
// ****************************************************************************

void XM_c::setup_channel(struct playinfo_t *playinfo,
      BYTE chnnr, BYTE note, BYTE instr, BYTE volcol, BYTE effno, BYTE effpar)
{ struct typ_channel  *XM_chn=&chn[chnnr];
  struct chninfo_t *MIX_chn=&(playinfo->chn[chnnr]);
  short no_note_change=0; // flag for some effects 
  BYTE next_eff=0;
  XM_chn->tmp_volume=XM_chn->volume; // <- is used for tremolo & tremor while effect handling (don't remove it)

  if (instr && (Instrument_p[instr]==NULL||(Instrument_p[instr])->nosamples==0))
  { // instrument does not exist - ignore changes
    XM_chn->eff1no=0; 
    XM_chn->eff2no=0; // forget about effects with last note&instrument...
    note=0xff;
    instr=0;
  }

  if (effno==0x5 || effno==0x3 || (volcol>>4)==0xf || 
      (effno==0xe && (effpar>>4)==0xd && (effpar&0x0f)!=0))
  // don't change note if any of these effects are active !
  // they'll care about changes itself
    no_note_change=1;

  // special case portamento - no note,instr change, but volume (if instr)!!
  if (effno==0x3 && (XM_chn->smp_p!=NULL) && instr)
  { MIX_chn->changes|=CHG_VOLUME;
    XM_chn->tmp_volume=XM_chn->volume=XM_chn->smp_p->volume;
  }

  // change instrument & note    if a real or no note is set
  if ((note<96 || note==0xff) && instr && !no_note_change)
  { 
    if (note<96)
    {
      XM_chn->eff1no=0; 
      XM_chn->eff2no=0; // forget about effects with last note&instrument...

      XM_chn->instr_p = Instrument_p[instr];
      XM_chn->smpno=(XM_chn->instr_p->note_smp)[note-1];
    } else
    if (instr!=XM_chn->instr)
    { // any changes in that case ???
    }
    
    XM_chn->smp_p = &((*(XM_chn->instr_p)->sample)[XM_chn->smpno]);

    if (XM_chn->smpno>=XM_chn->instr_p->nosamples||XM_chn->smp_p==NULL||XM_chn->smp_p->smplen==0)
    { // not existing sample -> stop mixing at this channel
      MIX_chn->changes|=CHG_STOPMIX;
      note=0xff;
      instr=0;
    } else
    {
      if (note<96)
      {
        XM_chn->instr=instr;
        XM_chn->note=note;
        XM_chn->realsmpno=MIX_chn->instno =XM_chn->smp_p->smpnum;
        MIX_chn->changes=CHG_INSTR|CHG_FREQ;
        // setup new note:
        // ~~~~~~~~~~~~~~~
        XM_chn->plynote=XM_chn->note+XM_chn->smp_p->relnote;
        XM_chn->finetune=XM_chn->smp_p->finetune;
	if (effno==0xe && (effpar>>4)==0x5) // set finetune command
	  XM_chn->finetune=((char)(effpar&0xf)-7)*16;
        XM_chn->period=calculate_period(XM_chn->plynote,XM_chn->finetune);
        MIX_chn->frequency =calculate_frequency(XM_chn->period);
      }
      // setup instrument:
      // ~~~~~~~~~~~~~~~~~
      MIX_chn->changes|=CHG_VOLUME|CHG_PANNING;
      XM_chn->tmp_volume=XM_chn->volume=XM_chn->smp_p->volume;
      MIX_chn->panposition=XM_chn->panning=XM_chn->smp_p->panning;

      // Setup Volume Envelope:
      XM_chn->ve_sustain=((XM_chn->instr_p->voltype&2)==2); /* stop at sustain point */
      XM_chn->ve_point=0;                 /* last envelop point we met */
      XM_chn->ve_tick=0;                  /* current tick from instrument start */
      XM_chn->ve_delta=XM_chn->ve_nextep=(XM_chn->instr_p->volenv)[1].pos;
      if (XM_chn->instr_p->volsustain==0) // stay at first point...
         XM_chn->ve_nextep=0;
      XM_chn->ve_from=(XM_chn->instr_p->volenv)[0].level;
      XM_chn->ve_to=(XM_chn->instr_p->volenv)[1].level;    /* slide linear between those to values */
      if (XM_chn->instr_p->voltype&1) XM_chn->env_vol=XM_chn->ve_from;
        else XM_chn->env_vol=64;

      // Set no fade out volume
      XM_chn->fo_speed=0;                  /* Volume fadeout is _Off_ */
      XM_chn->fadeout_vol=0x8000;          /* max.Vol */
  
      // Setup Auto Vibrato
      XM_chn->av_vibtabpos=0;
      XM_chn->av_count=XM_chn->instr_p->vibsweep;
    }
  }

  if (note>=KEY_OFF && note<0xff) 
  { XM_chn->ve_sustain=0; // don't halt at sustain point ...
    if (XM_chn->instr_p!=NULL)
    {
      if (XM_chn->instr_p->voltype&1) // only fade if volume envelope !
      { XM_chn->fo_speed=XM_chn->instr_p->volfadeout; // no fade if = 0 (stay at this volume)
      } else // if no volume envelope stop mixing immediatly...
        MIX_chn->changes|=CHG_STOPMIX;
    }
  }

  // VOLCOL ------------------------------------------------------------------
  if (volcol>0x0f && volcol<0x51)  // set volume
    { MIX_chn->changes|=CHG_VOLUME;
      XM_chn->tmp_volume=XM_chn->volume=volcol-16;
      XM_chn->eff1no=0; // noeffect in volume colum
    }

  if (XM_chn->eff1no==V_VIBRATO && (volcol&0xb0!=0xb0)) 
  { // vibrato in volume column ends
    MIX_chn->frequency=calculate_frequency(XM_chn->period);
    MIX_chn->changes|=CHG_FREQ;
  }

  if (volcol>0x5f)  // volume column effect:
    { switch (volcol>>4)
      { case 0x6: // slide volume down
	  XM_chn->VolSld_par=volcol&0xf;
	  break;
	case 0x7: // slide volume up
          XM_chn->VolSld_par=(volcol&0xf)<<4;
	  break;
	case 0x8: // fine volume down
	  { char vol=XM_chn->volume-(volcol&0xf);
	    if (vol<0) vol=0;
	    XM_chn->volume=vol;
	    MIX_chn->changes|=CHG_VOLUME;
	    break;
	  }
	case 0x9: // fine volume up
	  { char vol=XM_chn->volume+(volcol&0xf);
	    if (vol>63) vol=63;
	    XM_chn->volume=vol;
	    MIX_chn->changes|=CHG_VOLUME;
	    break;
	  }  
	case 0xa: // set vibrato speed
	  if ((volcol&0xf)!=0) XM_chn->Vib_speed=volcol&0xf;
	  break;
	case 0xb: // vibrato
	  //if ((volcol&0xf)!=0) XM_chn->Vib_depth=volcol&0xf;
	  break;
	case 0xc: // set panning
	  MIX_chn->panposition=(volcol&0xf)<<4;
	  MIX_chn->changes|=CHG_PANNING;
	  break;
	case 0xd: // panning slide left
	  if ((volcol&0xf)!=0) XM_chn->PanSld_par=volcol&0xf;
	  break;
	case 0xe: // panning slide right
	  if ((volcol&0xf)!=0) XM_chn->PanSld_par=(volcol&0xf)<<4;
	  break;
	case 0xf: // tone portamento (par*16 ??or not??)
	  if ((volcol&0xf)!=0) XM_chn->Port_par=(volcol&0xf)<<4;
  	  if (note<96) // a new note to slide to
  	  { // portamento does not change the sample of an instrument
	    XM_chn->note=note;
	    XM_chn->plynote=note+(*(Instrument_p[XM_chn->instr])->sample)[XM_chn->smpno].relnote;
	    XM_chn->SlideTo=calculate_period(XM_chn->plynote,XM_chn->finetune);
          } else if (XM_chn->SlideTo==0) volcol=0;
	  break;
      }
    }
  XM_chn->eff1no=cnv_veff[volcol>>4];

  // EFFECT COLUMN -----------------------------------------------------------
  next_eff=cnv_eff[effno];
  if (effno==0xe) // extended effects !
    next_eff=cnv_xeff[effpar>>4];

  // special case for arpeggio:
  if (effno==0x0&&effpar==0) next_eff=0; // no arpeggio !

  if (XM_chn->eff2no!=next_eff) // last effects ends now !
  { // for some effects we need to reset some values:
    switch (XM_chn->eff2no) {
      case ARPEGGIO: 
      case VIBRATO:
         MIX_chn->frequency=calculate_frequency(XM_chn->period);
	 MIX_chn->changes|=CHG_FREQ;
	 break;
      case TREMOLO:
	 MIX_chn->volume=XM_chn->volume;
	 MIX_chn->changes|=CHG_VOLUME;
	 break;
    }
  }
  // handle current effects:
  switch (effno)
  { case 0x0: // arpeggio
        { // prepare arpeggio:
          XM_chn->arp_delta[0]=0;
          XM_chn->arp_delta[1]=(BYTE)(effpar>>4);
          XM_chn->arp_delta[2]=(BYTE)(effpar&0xf);
	  XM_chn->Arp_par=effpar;
	  XM_chn->count2=0;
	}
        break;
    case 0x1: // porta up
	if (effpar) XM_chn->Sldup_par=effpar;
	break;
    case 0x2: // porta down
	if (effpar) XM_chn->Slddwn_par=effpar;
	break;
    case 0x3: // portamento
        if (effpar) XM_chn->Port_par=effpar;
	if (note<96 && XM_chn->instr) // a new note to slide to and an instrument was/is playing
	{ XM_chn->note=note;
	  XM_chn->plynote=note+(*(Instrument_p[XM_chn->instr])->sample)[XM_chn->smpno].relnote;
	  XM_chn->SlideTo=calculate_period(XM_chn->plynote,XM_chn->finetune);
        } else if (XM_chn->SlideTo==0) effno=0;
	break;
    case 0x4: // vibrato
	if ( XM_chn->vib_reset_counter && instr ) XM_chn->vib_tabcount=0;
	// use nonzero nibbles
	if (effpar&0xf0) XM_chn->Vib_speed=effpar>>4;
	if (effpar&0x0f) XM_chn->Vib_depth=effpar&0xf;
	break;
    case 0x5: // portamento+volumeslide
        if (effpar) XM_chn->VolSld_par=effpar;
	if (note<96) // a new note to slide to
	{ // portamento does not change the sample of an instrument
	  XM_chn->note=note;
	  XM_chn->plynote=note+(*(Instrument_p[XM_chn->instr])->sample)[XM_chn->smpno].relnote;
	  XM_chn->SlideTo=calculate_period(XM_chn->plynote,XM_chn->finetune);
        } else if (XM_chn->SlideTo==0) effno=0;
	break;
    case 0x6: // vibrato+volumeslide
        if (effpar) XM_chn->VolSld_par=effpar;
	if ( XM_chn->vib_reset_counter && instr ) XM_chn->vib_tabcount=0;
	break;
    case 0x7: // tremolo
	if ( XM_chn->trml_reset_counter && instr ) XM_chn->trml_tabcount=0;
	// use nonzero nibbles
	if (effpar&0xf0) XM_chn->Trml_speed=effpar>>4;
	if (effpar&0x0f) XM_chn->Trml_depth=effpar&0xf;
	break;
    case 0x8: // set panning position
      MIX_chn->panposition=effpar;
      MIX_chn->changes|=CHG_PANNING;
      break;
    case 0x9: // set offset
      if (note<96) // only if a new note is given !
      { MIX_chn->offset=(DWORD)effpar<<8;
	MIX_chn->changes|=CHG_SETOFS;
      }
      break;
    case 0xa: // volume slide
      if (effpar) XM_chn->VolSld_par=effpar;
      break;
    case 0xb: // position jump
	flag_position_jump=1;
	position_jump_to=effpar;
        break;
    case 0xc: // set volume
        XM_chn->tmp_volume=XM_chn->volume=effpar;
        MIX_chn->changes|=CHG_VOLUME;
        break;
    case 0xd: // pattern break
	flag_pattern_break=1;
	pattern_break_to=effpar;
        break;
    case 0xe: // extended effects
      { switch (effpar>>4)
	{ case 1: // fine porta up
	    if (effpar&0xf) XM_chn->FSldup_par=effpar&0xf;
	    XM_chn->period-=XM_chn->FSldup_par<<2;
	    MIX_chn->frequency=calculate_frequency(XM_chn->period);
	    MIX_chn->changes|=CHG_FREQ;
	    break;
	  case 2: // fine porta down
	    if (effpar&0xf) XM_chn->FSlddwn_par=effpar&0xf;
	    XM_chn->period+=XM_chn->FSlddwn_par<<2;
	    MIX_chn->frequency=calculate_frequency(XM_chn->period);
	    MIX_chn->changes|=CHG_FREQ;
	    break;
	  case 3: // set glisscontrol
	    XM_chn->glisscontrol=effpar&0xf;
	    break;
	  case 4: // set vibrato control
	    XM_chn->vib_wavtab=av_wave[effpar&0x3];
	    XM_chn->vib_reset_counter=((effpar&0x4)!=0x4);
	    break;
	  /* case 5: set finetune --> look at setting a new note ^ */
	  case 6: // set loop begin/loop
	    if ((effpar&0xf)==0) // set loop start
	      XM_chn->ploop_start=c_row;
	    else
	    { // loop or start loop
	      if (XM_chn->ploop_count==0xff)
	      { XM_chn->ploop_count=effpar&0xf;
	        pattern_loop_to=XM_chn->ploop_start;
	        flag_pattern_loop=1;
	      }
	      else
	        if (!--XM_chn->ploop_count) XM_chn->ploop_count=0xff;
	      else
	      { pattern_loop_to=XM_chn->ploop_start;
	        flag_pattern_loop=1;
	      }
	    }
	    break;
	  case 7: // set tremolo control
	    XM_chn->trml_wavtab=av_wave[effpar&0x3];
	    XM_chn->trml_reset_counter=((effpar&0x4)!=0x4);
	    break;
	  case 9: // retrigg note
	    if (effpar&0x0f)
	      XM_chn->count2=XM_chn->rtrg_len=effpar&0x0f;
	    else next_eff=0;
	    break;
	  case 0xa: // fine volume up
	    { char vol=XM_chn->volume+(effpar&0xf);
	      if (vol>63) vol=63;
	      XM_chn->volume=vol;
	      MIX_chn->changes|=CHG_VOLUME;
	      break;
	    }
	  case 0xb: // fine volume down
	    { char vol=XM_chn->volume-(effpar&0xf);
	      if (vol<0) vol=0;
	      XM_chn->volume=vol;
	      MIX_chn->changes|=CHG_VOLUME;
	      break;
	    }
	  case 0xc: // note cut
	    if ((effpar&0xf)==0) // cut immediatly
	    { XM_chn->volume=MIX_chn->volume=0;
	      MIX_chn->changes|=CHG_VOLUME;
	      next_eff=0;
	    } else
	      XM_chn->count2=effpar&0xf;
	    break;
	  case 0xd: // note delay
	    if ((effpar&0x0f)!=0) // a real delay
	    { XM_chn->notedly_note=note;
	      XM_chn->notedly_instr=instr;
	      XM_chn->count2=effpar&0x0f;
	    } else next_eff=0;
	    break;
	  case 0xe: // pattern delay
	    patterndelay_count=effpar&0xf;
	    break;
	}
      break;
      }
    case 0xf: // set speed or tempo
      if (effpar && effpar<32) c_speed=effpar;
      if (effpar && effpar>31) c_tempo=effpar;
      break;
    case 0x10: // set global volume
      chg_glob_vol=(effpar>64)?64:effpar;
      break;
    case 0x11: // global volume slide
      if (effpar) XM_chn->Gvolsl_par=effpar;
      break;
    case 0x14: // keyoff
      if (!effpar) // key off immediatly
      { XM_chn->ve_sustain=0; // don't halt at sustain point ...
        if (XM_chn->instr_p!=NULL)
    	{
      	  if (XM_chn->instr_p->voltype&1) // only fade if volume envelope !
      	  { XM_chn->fo_speed=XM_chn->instr_p->volfadeout; // no fade if = 0 (stay at this volume)
          } else // if no volume envelope stop mixing immediatly...
          MIX_chn->changes|=CHG_STOPMIX;
    	}
	next_eff=0;
      } else XM_chn->count2=effpar;
      break;
    case 0x15: // set envelop position (that's only for volume envelope)
	{ struct typ_instrument*  cins_p = XM_chn->instr_p;

          // only if instrument does exist and envelope is on:
	  if (cins_p!=NULL && (cins_p->voltype&1)!=0) 
	  { int i=0;
	    while ((cins_p->volenv)[i].pos<effpar && i<cins_p->novolpts) i++;
	    XM_chn->ve_point=i;
	    if (i<cins_p->novolpts) // somewhere inside envelope
	      XM_chn->ve_nextep=(cins_p->volenv)[i+1].pos-effpar;
	    else XM_chn->ve_nextep=0;
	  }
	}
      break;
    case 0x19: // panning slide
      if (effpar) XM_chn->PanSld_par=effpar;
      break;
    case 0x1B: // multi retrigg note
      if ((effpar&0xf0)!=0) // changes in volume slide
        XM_chn->mrtrg_volsld=effpar>>4;
      if ((effpar&0x0f)!=0) // changes in "speed"
      { XM_chn->mrtrg_len=(effpar&0x0f);
	if (effpar>1) XM_chn->mrtrg_len-=1;
      }
      if (XM_chn->mrtrg_count==0) XM_chn->mrtrg_count=XM_chn->mrtrg_len; 
      break;
    case 0x1D: // tremor
	{ BYTE on=XM_chn->Trm_on,off=XM_chn->Trm_again-on;
	  if ((effpar&0xf0)!=0) on=XM_chn->Trm_on=(effpar>>4);
	  if ((effpar&0x0f)!=0) off=(effpar&0xf)+1;
	  if (effpar!=0) XM_chn->Trm_again=on+off;
	}
	break;
    case 0x21: 
      { switch (effpar>>4)
	{ case 1: // extra fine porta up
	    if (effpar&0xf) XM_chn->xFSldup_par=effpar&0xf;
	    XM_chn->period-=XM_chn->xFSldup_par;
	    MIX_chn->frequency=calculate_frequency(XM_chn->period);
	    MIX_chn->changes|=CHG_FREQ;
	    break;
	  case 2: // extra fine porta down
	    if (effpar&0xf) XM_chn->xFSlddwn_par=effpar&0xf;
	    XM_chn->period+=XM_chn->xFSlddwn_par;
	    MIX_chn->frequency=calculate_frequency(XM_chn->period);
	    MIX_chn->changes|=CHG_FREQ;
	    break;
        }
      }
  }
  XM_chn->eff2no=next_eff;
}

// Instrument_envolopes is called every tick and checks all channels
// for possible volume or panning envelopes (automatic changes of volume
// and pan-position while playing an instrument) - (take it as an automatic
// effect independed from the two effect rows)
// panning-envelopes are not yet implemented ... useless since stereo mixing
// with panning is not done yet correct...

void XM_c::Instrument_envelopes(struct playinfo_t *playinfo)
{  unsigned int i;
  for (i=0;i< header.no_Channels;i++)
  if (chn[i].instr!=0&&Instrument_p[chn[i].instr]!=NULL)
  { struct typ_channel * XM_chn=&chn[i]; // <- to remove the slow array adressing
    struct typ_instrument * cins_p = XM_chn->instr_p; 
    struct chninfo_t *MIX_chn=&(playinfo->chn[i]);          

    if (cins_p->voltype&1)
    { // -----------------------
      // Handle Volume Envelope:
      // ----------------------- 
      int vol; // final volume

      if (XM_chn->ve_nextep) // some ticks left to next envelope point ?
      { // yes ! interpolate volume
	XM_chn->ve_nextep--;if (!XM_chn->ve_nextep) XM_chn->ve_point++;
        if (XM_chn->ve_delta) // if volume really has to change
        { vol=XM_chn->ve_to+(XM_chn->ve_from-XM_chn->ve_to)*XM_chn->ve_nextep/XM_chn->ve_delta;
	  if (vol>64) vol=64;
	  MIX_chn->changes|=CHG_VOLUME;
	  XM_chn->env_vol=vol; // <- that's only the envelop component of volume
        }
      } else
      // ----------------- go to next envelope point -------------------
      // stay at this sustain point ?
      if (XM_chn->ve_sustain&&XM_chn->ve_point==cins_p->volsustain)
      { // yes: do nothin
      } else

      // was this the last envelope point ?
      if (XM_chn->ve_point<cins_p->novolpts)
      { // yes: go to the next one
	int nep=0; // number of (n)ext (e)nvelope (p)oint after this point

        // reached loop end ?
	if ((cins_p->voltype&4)&&XM_chn->ve_point==cins_p->volloopend)
        { // yes: loop
	  nep=(XM_chn->ve_point=cins_p->volloopstart)+1;
	  if (nep>cins_p->volloopend) nep=cins_p->volloopend; // loopstart>loopend ? then no loop
	  XM_chn->ve_delta=XM_chn->ve_nextep=(cins_p->volenv)[nep].pos-(cins_p->volenv)[XM_chn->ve_point].pos;
	  XM_chn->ve_from=(cins_p->volenv)[XM_chn->ve_point].level;
	  XM_chn->ve_to=(cins_p->volenv)[nep].level;
	} 

        else // didn't reach last envelope point ?
	if (XM_chn->ve_point<=cins_p->novolpts)
	{ // yes, fade to next
	  nep=XM_chn->ve_point--;
	  XM_chn->ve_delta=XM_chn->ve_nextep=(cins_p->volenv)[nep].pos-(cins_p->volenv)[XM_chn->ve_point].pos;
	  XM_chn->ve_from=(cins_p->volenv)[XM_chn->ve_point].level;
	  XM_chn->ve_to=(cins_p->volenv)[nep].level;
	}
      } 

      // special case: fadeout volume is active
      if (XM_chn->fo_speed)
      { // volume fade out
	if (XM_chn->fadeout_vol>XM_chn->fo_speed) XM_chn->fadeout_vol-=XM_chn->fo_speed;
        else // end fading , stop mixing
	{ XM_chn->fo_speed=0; 
	  XM_chn->fadeout_vol=0;
	  MIX_chn->changes|=CHG_STOPMIX;
	}
	MIX_chn->changes|=CHG_VOLUME;
      }
    }

    // autovibrato:
    if ( XM_chn->volume && 
         chn[i].instr!=0 && cins_p!=NULL &&
         XM_chn->period && cins_p->vibdeth && cins_p->vibspeed ) // if no depth or speed then no vibrato
    { int s=(int)(*av_wave[cins_p->vibtype])[XM_chn->av_vibtabpos&127];
      int d=(int)cins_p->vibdeth;
      if (cins_p->vibsweep) d=d*(cins_p->vibsweep-XM_chn->av_count)/cins_p->vibsweep;
      if (XM_chn->av_vibtabpos<128) s=-s;
      MIX_chn->frequency=calculate_frequency(XM_chn->period+((s*d)>>8));
      MIX_chn->changes=CHG_FREQ;
      XM_chn->av_vibtabpos+=cins_p->vibspeed;
      if (XM_chn->av_count) XM_chn->av_count--;
    }
  }
}

void XM_c::set_final_vol(struct playinfo_t * playinfo)
{ // if one component of:
  // vol=(instrument_vol/64)*(global_vol/64)*(envelop_vol/64) did change, then
  // recalc volume for channel
  int i; 
  DWORD vol;
  // note on global volume effect:
  //  unlike S3M in Fasttracker modules "global-volume" changes
  //  volumes not only for next upcomming instruments but also for all playing
  //  channels !

  if (global_vol!=chg_glob_vol)
  for (i=0;i<header.no_Channels;i++)
  { if (chn[i].tmp_volume) // change all channels where volume is not 0
    { playinfo->chn[i].changes|=CHG_VOLUME;
      vol=(DWORD)chn[i].tmp_volume*(DWORD)chg_glob_vol*(DWORD)chn[i].env_vol; 
      playinfo->chn[i].volume=(((vol*4096)>>18)*chn[i].fadeout_vol)>>21;
    }
    global_vol=chg_glob_vol;
  } else
  for (i=0;i<header.no_Channels;i++)
  { if (playinfo->chn[i].changes&CHG_VOLUME) // some other volume did change
    { vol=(DWORD)chn[i].tmp_volume*(DWORD)global_vol*(DWORD)chn[i].env_vol;
      playinfo->chn[i].volume=(((vol*4096)>>18)*chn[i].fadeout_vol)>>21;
    }
  }
}

// reads on row of packed pattern data
// (packformat is very simple !)
void XM_c::read_new_notes(struct playinfo_t * playinfo)
{ unsigned int i;
  BYTE info;
  chg_glob_vol=global_vol;
  flag_pattern_break=0;
  flag_position_jump=0;
  flag_pattern_loop=0;

  for (i=0;i<header.no_Channels;i++)
  { BYTE note=0xff; // no note
    BYTE instr=0;   // no instrument
    BYTE volcol=0;  // no enty in volume column
    BYTE effno=0;   // no effect
    BYTE effpar=0;  // parameter 0

    info=*(c_position++);
    if (info&128) // note is packed
    {
      if (info&1)  note=*c_position++;
      if (info&2)  instr=*c_position++;
      if (info&4)  volcol=*c_position++;
      if (info&8)  effno =*c_position++;
      if (info&16) effpar=*c_position++;
    } else // note is not packed
    { note  =info;
      instr =*c_position++;
      volcol=*c_position++;
      effno =*c_position++;
      effpar=*c_position++;
    }
    setup_channel(playinfo,i,note,instr,volcol,effno,effpar); 
  }

  if (flag_pattern_break||flag_position_jump||
     (!flag_pattern_loop && c_row++==Pattern[order[c_order]].norows-1))
  { // end of this pattern !

    if (flag_position_jump)
    { if ((c_order=position_jump_to)>header.songlen) c_order=header.restart; }
    else if (++c_order==header.songlen) { c_order=header.restart;global_vol=64; }

    c_position=(BYTE *)(Pattern[order[c_order]].data);
    c_row=0;
    if (flag_pattern_break && pattern_break_to>0 && pattern_break_to<Pattern[order[c_order]].norows-1)
    { c_row=pattern_break_to;
      while (pattern_break_to--)
        for (i=0;i<header.no_Channels;i++)
	{ if (((info=*c_position++)&128)==128)
	  { if (info&1)  c_position++;
            if (info&2)  c_position++;
            if (info&4)  c_position++;
      	    if (info&8)  c_position++;
      	    if (info&16) c_position++;
	  } else c_position+=4;
	}
    }
  } else if (flag_pattern_loop)
  { // loop to specified row
    c_row=pattern_loop_to;
    c_position=(BYTE *)Pattern[order[c_order]].data;
    while (pattern_loop_to--)
      for (i=0;i<header.no_Channels;i++)
	{ if (((info=*c_position++)&128)==128)
	  { if (info&1)  c_position++;
            if (info&2)  c_position++;
            if (info&4)  c_position++;
      	    if (info&8)  c_position++;
      	    if (info&16) c_position++;
	  } else c_position+=4;
	}

  }
  c_tick=c_speed-1;
}

// this function is called every tick, it handles speed, envelopes and
// note-reading
short XM_c::nextnotes(playinfo_t* playinfo_p)
{  // put the next notes into (*playinfo_p)
   // this is called every tick
   memset(&playinfo_p->chn,0,sizeof(playinfo_p->chn));

   Instrument_envelopes(playinfo_p);
   if (!c_tick && patterndelay_count) 
   { c_tick=c_speed-1;
     patterndelay_count--;
   }

   if (c_tick) effect_handling(playinfo_p);
   else read_new_notes(playinfo_p);

   set_final_vol(playinfo_p); // <- calculates the final volume for all channels
   playinfo_p->whatTT=0;playinfo_p->tempo=c_tempo;
   return(0);
}

XM_c::~XM_c(void)
{ int i;
  /*
   for (i=0;i<header.no_Instruments;i++)
  if (Instrument_p[i]!=NULL)
  { if (Instrument_p[i]->sample!=NULL)
      free(Instrument_p[i]->sample);
    free(Instrument_p[i]);
  }
  for (i=0;i<header.no_Patterns;i++)
    if (Pattern[i].data!=NULL) free(Pattern[i].data);
  */
}

short XM_c::startmodule(void)
{ int i;
  // Init some variables
  c_tick =0;
  c_row  =0;
  c_order=0;
  c_position =(BYTE*)Pattern[order[c_order]].data; 
  c_speed    =header.defaulttempo;
  c_tempo    =header.defaultBPM;
  global_vol =64; // maximum
  patterndelay_count=0;
  // clear the channels
  memset(&(chn),0,sizeof(chn));
  for (i=0; i<header.no_Channels; i++)
  { // initialize channels:
    chn[i].vib_wavtab=&av_sinus;
    chn[i].vib_reset_counter=1; // reset vibrato wave start at new instrument
    chn[i].trml_wavtab=&av_sinus;
    chn[i].trml_reset_counter=1; // reset tremolo wave start reset at new instrument

    chn[i].mrtrg_len=1;
    chn[i].ploop_count=0xff;    // no pattern loop yet
  }
  return(0);
}

// sample load routines for XMs need the following global variables:
int load_first_value; /* FLAG for blockwise instrument loading (set before first call) */
int last_value;	      /* last sample value in read_SMPblock call  -> remember XM uses delta encoding */

// XM-format:
// - header (main song infos)
// - patterns (with variable length)
// - instruments (variable! max.256)
// - samples for each instrument (max.15 per instrument)
// in that way there's a maximum of 256*15 different samples to store !
// I load them in a row and put their "virtual" number (smpnum) into instrument
// info - in that way I can call the samples by number as it is required in the
// device interface

void XM_c::load_module(void)
{ int i;

  trace("loading FastTracker2-module.\n");
  ModuleID=XM_idstr;
  Module=this;
  no_Samples=0;
  memset(&Instrument_p,0,sizeof(Instrument_p));
  memset(&Pattern,0,sizeof(Pattern));

  /* prepare tabs: */
  // prepare autovibratos:
  for (i=0;i<128;i++)
  { av_sinus[i]=(unsigned char)(255*sin(M_PI*i/128));
    av_flag_wave[i]=(unsigned char)255;
    av_rampright[i]=(unsigned char)2*i;
    av_rampleft[i]=(unsigned char)(255-2*i);
  }

  if (fread(&header,1,sizeof(struct typ_header),file)!=sizeof(struct typ_header))
    { memset(&header,0,sizeof(header));status=WRONGFORMAT;return; }
  /* convert little endian-format to machine format: */
  lit2m_w(&header.trackerversion);
  lit2m_dw(&header.headersize);
  lit2m_ws(&header.songlen,8); /* convert them in a row */

  if ((empty_patterndata=(BYTE*)calloc(header.no_Channels*64,1))==NULL)
    { status=NOMEM;return; }
  memset(empty_patterndata,128,header.no_Channels*64);

  if (strncmp(header.IDtext,"Extended Module: ",17) || header.endchar!=0x1a) 
    { memset(&header,0,sizeof(header));status=WRONGFORMAT;return; }

  // load pattern order table
  if (fread(&order,1,256,file)!=256)
    { status=WRONGFORMAT;return; }

  // load patterns
  for (i=0;i<header.no_Patterns;i++)
  { unsigned int len;
    if (fread(&Pattern[i],1,5,file)!=5)
      { status=WRONGFORMAT;return; }
    lit2m_dw(&Pattern[i].headlen);
    
    if (fread(&Pattern[i].norows,1,4,file)!=4)
      { status=WRONGFORMAT;return; }
    lit2m_ws(&Pattern[i].norows,2); // convert both words in a row

    if ((len=Pattern[i].datasize)==0) { Pattern[i].data=empty_patterndata;continue; }
    if ((Pattern[i].data=(BYTE*)calloc(len,1))==NULL) { status=NOMEM;return; }

    #ifdef __DEBUGGING__
    printf("load pattern no. %u (%u bytes)\n",i,len);
    #endif
    if (fread(Pattern[i].data,1,len,file)!=len)
      { status=WRONGFORMAT;return; }
  }
  for (i=header.no_Patterns;i<256;i++)
  { Pattern[i].headlen=0;
    Pattern[i].norows=0x40;
    Pattern[i].datasize=0x40;
    Pattern[i].data=empty_patterndata;
  }
  // load instruments
  for (i=1;i<=header.no_Instruments;i++)
  { DWORD isize;
    char  iname[23];
    BYTE  itype;
    WORD  nosmp;
    if (fread(&isize,1,4,file)!=4 || fread(&iname,1,22,file)!=22 ||
	fread(&itype,1,1,file)!=1 || fread(&nosmp,1,2,file)!=2) 
      { status=WRONGFORMAT;return; }
    lit2m_dw(&isize);
    iname[22]=0;
    if (!nosmp)
    { int s;
      /* no samples for this instrument */
      if ((Instrument_p[i]=(typ_instrument *)calloc(25,1))==NULL) { status=NOMEM;return; }
      #ifdef __DEBUGGING__
      printf("load instrno. %u name: \"%s\"\n",i,iname);
      #endif
      strncpy((char*)Instrument_p[i],iname,23);
      (Instrument_p[i])->nosamples=0;
      for (s=0;s<isize-29;s++) getc(file);
    } else
    { unsigned int s;
      unsigned long headlen;
      if ((Instrument_p[i]=(typ_instrument *)calloc(sizeof(struct typ_instrument),1))==NULL)
	{ status=NOMEM;return; }
      #ifdef __DEBUGGING__
      printf("load instrno. %u  name: \"%22.22s\"\n",i,iname);
      #endif

      // load the sample header
      if (fread(&headlen,1,4,file)!=4  ||
	  fread(&(Instrument_p[i]->note_smp),1,210,file)!=210)
      { status=WRONGFORMAT;return; }
      lit2m_dw(&headlen);
      /* FORGET unused bytes */
      for (s=0;s<isize-243;s++) getc(file);

      // get memory for sample headers of this instrument:
      Instrument_p[i]->nosamples=nosmp;
      if ((Instrument_p[i]->sample=(struct typ_sample (*)[16])calloc(nosmp*sizeof(struct typ_sample),1))==NULL)
	{ status=NOMEM;return; }

      // load the sample headers:
      for (s=0;s<nosmp;s++)
      { struct typ_sample * cursmp=(struct typ_sample *)&((*Instrument_p[i]->sample)[s]);
	int er;
	if (fread(cursmp,1,40,file)!=40)
	  { status=WRONGFORMAT;return; }
	lit2m_dws(&(cursmp->smplen),3);
	for (er=headlen-40;er>0;er--) getc(file);
	cursmp->smpname[22]=0;
	#ifdef __DEBUGGING__
	printf("%u.sample: \"%s\" size: %lu\n",s,cursmp->smpname,cursmp->smplen);
	#endif
      }
      // load data of samples
      for (s=0;s<nosmp;s++)
      { unsigned long t;
	char bits,loopflag;
	if ((t=(*Instrument_p[i]->sample)[s].smplen)!=0)
	{ DWORD loopend=(*Instrument_p[i]->sample)[s].smplooplen+(*Instrument_p[i]->sample)[s].smploopstart;
	  DWORD loopstart=(*Instrument_p[i]->sample)[s].smploopstart;
	  DWORD bytelen=(*Instrument_p[i]->sample)[s].smplen;

	  no_Samples++;(*Instrument_p[i]->sample)[s].smpnum=no_Samples;
	  load_first_value=1; /* for Devices call readSMPblock more than one time with no bytes < instrumentsize */
	  bits=(8+(((*Instrument_p[i]->sample)[s].smptype&16)>>1)); /* 8 or 16bit data */
	  if (bits==16) { bytelen>>=1;loopend>>=1;loopstart>>=1; }

	  if ((status=Device->LoadInstrument((*Instrument_p[i]->sample)[s].smpnum,bytelen,
	      bits,(*Instrument_p[i]->sample)[s].smptype&3, // looptype
	      loopstart,loopend))!=0)
	    return;
	  if (bits==16) t+=t;
	}
      }
    }
  }
  #ifdef __DEBUGGING__
  printf("press <enter>...");
  getchar();
  #endif
  
}

DWORD XM_c::readSMP8_BLK(BYTE * dest,DWORD size,BYTE _bits)
{ // is called by the initialized device to read size bytes to buffer at "dest"
  SBYTE c;
  SWORD c2;
  if (_bits==8)
  { /* ----- 8bit sample -------- */
    if (load_first_value)
      { *dest++=128+(char)(c=getc(file));size--;load_first_value=0; }
    else c=last_value;
    while (!feof(file) && size) { *(dest++)=128+(c+=getc(file));size--; }
    last_value=c;
  } else
  { /* -------- 16bit sample ------- */
    // convert it to 8bit
    if (load_first_value)
    { *dest++=0x80+(char)((c2=getc(file)+(getc(file)<<8))>>8);
      size--;load_first_value=0;
    } else c2=last_value;
    while (!feof(file) && size) { *dest++=0x80+(char)((c2+=getc(file)+(getc(file)<<8))>>8);size--; }
    last_value=c2;
  }
  return(size);
}

DWORD XM_c::readSMP16_BLK(WORD * dest,DWORD size,BYTE _bits)
{ // is called by the initialized device to read size bytes to buffer at "dest"
  SBYTE c;
  SWORD c2;
  if (_bits==8)
  { /* ----- 8bit sample -------- */
    // convert it to 16bit 
    if (load_first_value)
      { *dest++=(128U+(WORD)(c=getc(file))<<8);size--;load_first_value=0; }
    else c=last_value;
    while (!feof(file) && size) { *(dest++)=(128U+(WORD)(c+=getc(file))<<8);size--; }
    last_value=c;
  } else
  { /* -------- 16bit sample ------- */
    if (load_first_value)
    { *dest++=0x8000+((c2=getc(file)+((WORD)getc(file)<<8)));
      size--;load_first_value=0;
    } else c2=last_value;
    while (!feof(file) && size) 
    { *dest++=0x8000+((c2+=getc(file)+((WORD)getc(file)<<8)));size--; }
    last_value=c2;
  }
  return(size);
}

#ifdef __PLAYINFO__

#endif