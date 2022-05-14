/****************************************************************************
    Mixer - mix routines for all non wavetable soundcards
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 1.8.1996
****************************************************************************/

/* Mixer16_c is SuperClass of Mono16_c & Stereo16_c, all methods which are
   equal for both mono and stereo mixing are implemented in that class     */

/* Mono16_c, Stereo16_c ... medium speed routines for more quality,
     technical specs: - do mix only 8bit samples (high byte of 16bit samples)
		      - samples scaled with volume: (smp*64*8>>6): -1024...1023
			with 32 channels a maximum range for mixing result
			of -32768...32767 (16bits)
		      - result is scaled with 3 lookup tables (a 256words):
			result=HB*256+LB:
			  final=(LOtab[LB] & andtab[HB]) + HItab[HB]
		      for 8bit results:
			only lower 8bit of final are used
			LOtab[val]=
			andtab[val]=
			ortab[val]=
*/

#include "device.h"
#include "mixer.h"
#include "global.h"
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>

/*****************************************************************************
 *  Mixer16_c    routines for mixing samples to 16bit data
 ****************************************************************************/
Mixer16_c::Mixer16_c(DWORD _buffersize,BYTE _interpolate,BYTE ouput_format)
{ VOLTB_BASE * h_i,i,j;
  status=0;
  if ((volumetable=(voltb_t *)malloc(sizeof(voltb_t))) == NULL )
     { status=NO_MEM; return; }
  /* initialize volume LOOKUP-table */
  h_i=(VOLTB_BASE *)volumetable;
  for (i=0;i<65;i++)
    for (j=0;j<256;j++)
      *(h_i++)=(VOLTB_BASE)((8*i*(j-128))>>6);
  if ((posttable=(WORD *)malloc(2*65*1024L)) == NULL )
    { status=NO_MEM;return; }
  buffersize=_buffersize;
  interpolate=_interpolate;
  if ((MixBuffer=(WORD *)malloc(buffersize)) == NULL )
    { status=NO_MEM;return; }
  outformat=ouput_format;
  calc_posttable(127);
}

Mixer16_c::~Mixer16_c(void)
{ free(volumetable);
  free(posttable);
  free(MixBuffer);
}

void Mixer16_c::calc_posttable(BYTE Mainvolume)
{ float a,b,c;
  SDWORD i,z;
  WORD sign=0;
  z=Mainvolume&127;
  /*----------------------------------------------------------------------------
     Posttable is for converting 13 bit mixing result (all channels simply
     added) to the resulting 8 bit data for output (scaled with mainvolume)
  -----------------------------------------------------------------------------*/
  #ifdef __DEBUGGING__
  switch (outformat) {
    case 0: printf("8bit unsigned output\n"); break;
    case 1: printf("16bit unsigned output\n"); break;
    case 2: printf("8bit signed output\n"); break;
    case 3: printf("16bit signed output\n"); break;
    default: printf("not implemented.\n"); break;
  } getchar();
  #endif

  if (outformat<3 && (outformat&1)==0 ) // 8bit output:
  { 
    if ((outformat&2)==1) sign=128;
    c=8*256*127.0/z;
    a=32*1024-c/2;
    b=32*1024+c/2;
    for (i=0;i<65*1024;i++)
    { if (i<a) posttable[i]=0; else
      if (i>b) posttable[i]=255; else
      posttable[i]=(WORD)((i-a)*z/1024-sign);
    }
  } else // 16bit output:
  /*----------------------------------------------------------------------------
     Posttable is for converting 13 bit mixing result (all channels simply
     added) to the resulting 16 bit data for output (scaled with mainvolume)
  -----------------------------------------------------------------------------*/
  { 
    #ifdef __DEBUGGING__
      printf("Mainvol: %d\n",Mainvolume);
    #endif
    if ((outformat&2)==1) sign=0x8000;
    c=8*256*127.0/z;
    a=32*1024-c/2;
    b=32*1024+c/2;
    for (i=0;i<65*1024;i++)
    { if (i<a) posttable[i]=0; else
      if (i>b) posttable[i]=0xffff; else
      posttable[i]=(WORD)(256*(i-a)*z/1024-sign);
    }
  }
}

void Mixer16_c::amplify_result(void * Dest)
{ int i;
  WORD * h_buf16=MixBuffer;
  BYTE * bptr=(BYTE*)Dest;
  WORD * wptr=(WORD*)Dest;
  if (outformat==0 || outformat==2)
  { /*-------------------------------------------------------------------------
      after mixing all channels together, scale the 13bit results down to 8bit
      that's ofcourse done by a lookup table (scaling and clipping precalculated)
      -------------------------------------------------------------------------*/
    for (i=buffersize;i>0;i--)
    { // under DOS - TASM style:
      // MOV BX,es:[DI]
      // MOV AL,fs:[BX]  - lookup table was at offset 0 !
      // MOV ds:[SI],AL
      // SI++,DI++
      *(bptr++)=(BYTE)(*(posttable+ *(h_buf16++)));
    }
  } else // 16bit output
  { /*-------------------------------------------------------------------------
      after mixing all channels together, scale the 13bit results to 16bit
      that's ofcourse done by a lookup table (scaling and clipping precalculated)
      -------------------------------------------------------------------------*/
    for (i=buffersize;i>0;i--)
      *(wptr++)=(WORD)(*(posttable+ *(h_buf16++)));
  }
}

void Mixer16_c::clear_buffer(void)
{ int i;
  WORD *h=MixBuffer;
  for (i=0;i<buffersize;i++) *h++=32*1024;
}

void * Mixer16_c::ptr2buf(void)
{ return this->MixBuffer;
}

void * Mixer16_c::add_sample8(void * dest, BYTE * src, SDWORD *offset,WORD *frac,
                             SDWORD step, WORD todo, BYTE vol, BYTE panpos)
// source "src" is an 8bit sample
// mix_8(),mix_8i() are virtual -> they do point to mono or stereo mix_8(),mix_8i()
{ DWORD ofs;
  DWORD overide=0;
  void * tmpdest;
  if (step>0)
  { // while mixing we can only handle a 16bit offset
    // (16bit offset, 16bit fraction in one 32bit value)
    // for that reason prepare offset and fraction:

    if (*offset>=0x10000L) src+=(*offset&0xffff0000L);
    if ((*offset&0xffffL) > 0x0ffffL-(step*todo>>16) )
    { overide=(todo*step>>16)+1;
      src+=overide; // move sample pointer forward to avoid later 16bit offset overides
    }
    ofs=((*offset&0xffff)-overide<<16)|(*frac);
    if (interpolate)
      tmpdest=mix_8i((WORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
    else
      tmpdest=mix_8((WORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
    *frac=(ofs&0xffff);
    *offset=((*offset&0xffff0000)|(ofs>>16))+overide;
    return tmpdest;
  }
  if (step<0)
  { 
    // a bit different to positiv dir, but overides in neg dir ! (msb=0!!)
    if (*offset>=0x8000L) src+=(*offset&0xffff8000L);
    if ((*offset&0x07fffL) <= (-step*todo>>16)+1 )
    { overide=(-step*todo>>16)+1;
      src-=overide; // move sample pointer back to avoid later 16bit overides
    }
    ofs=(((*offset&0x07fffL)+overide)<<16)|*frac;
    if (interpolate)
      tmpdest=mix_8i((WORD*)dest,src,(SDWORD*)&ofs,-1,-step,todo,vol,panpos);
    else
      tmpdest=mix_8((WORD*)dest,src,(SDWORD*)&ofs,-1,-step,todo,vol,panpos);

    *frac=(ofs&0xffff);
    *offset=((*offset&0xffff8000L)|((SDWORD)ofs>>16))-overide;
    return tmpdest;
  }
  return((WORD *)dest+todo);
}

void * Mixer16_c::add_sample16(void * dest, WORD * src, SDWORD *offset,WORD *frac,
                                           SDWORD step, WORD todo, BYTE vol, BYTE panpos)
{ /* no implementation yet */ }


/*****************************************************************************
 *  Mono16_c    routines for mixing samples to 13bit data
 ****************************************************************************/

Mono16_c::Mono16_c(DWORD _buffersize,BYTE _interpolate,BYTE output_format):Mixer16_c(_buffersize,_interpolate,output_format)
{ no_samples=buffersize;
}


WORD * Mono16_c::mix_8(WORD *Dest,BYTE * Sourc,SDWORD* ofs,SBYTE dir, 
		      SDWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ BYTE sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *t_vol=&((*volumetable)[vol][0]);

  tmpofs=*ofs;
  if (dir==1)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=t_vol[sample];
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

WORD * Mono16_c::mix_8i(WORD *Dest,BYTE * Sourc,SDWORD* ofs, SBYTE dir,
		       SDWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *t_vol=&((*volumetable)[vol][0]);
  
  tmpofs=*ofs;
  if (dir==1)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=t_vol[sample];
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(SDWORD)(Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

/*****************************************************************************
 *  Stereo16_c    routines for mixing samples to 13bit stereo data
 ****************************************************************************/

Stereo16_c::Stereo16_c(DWORD _buffersize,BYTE _interpolate,BYTE output_format):Mixer16_c(_buffersize,_interpolate,output_format)
{ no_samples=buffersize/2;
}

WORD * Stereo16_c::mix_8(WORD *Dest,BYTE * Sourc,SDWORD* ofs,SBYTE dir, 
		      SDWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ BYTE sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  VOLTB_BASE *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);

  tmpofs=*ofs;
  if (dir==1)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

WORD * Stereo16_c::mix_8i(WORD *Dest,BYTE * Sourc,SDWORD* ofs, SBYTE dir,
		       SDWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  VOLTB_BASE *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);
  
  tmpofs=*ofs;
  if (dir==1)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(SDWORD)(Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}
