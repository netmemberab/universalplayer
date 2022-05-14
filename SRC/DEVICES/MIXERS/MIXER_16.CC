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
			  final=LOtab[LB][postab[HB]] + HItab[HB]
			max=8*256*128.0/vol; (vol:0..128)
		      final is a 16bit value -->
		      for 8bit results only upper 8bit of final are used
  --> multiplying isn't that expensiv to use 3 lookups per sample, but the 
      clipping branches make it worth to do so !
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
  buffersize=_buffersize;
  interpolate=_interpolate;
  if ((MixBuffer=(WORD *)malloc(2*buffersize)) == NULL )
    { status=NO_MEM;return; }

  // quadric table for interpolation:
  for (i=0;i<128;i++)
    quadtab[255-i]=255-(quadtab[i]=i*i/128);

  outformat=ouput_format;
  calc_lookuptables(127);
}

Mixer16_c::~Mixer16_c(void)
{ free(volumetable);
  free(MixBuffer);
}

void Mixer16_c::calc_lookuptables(BYTE Mainvolume)
{ SDWORD i,z,max;
  WORD val,negb,posb,delta,pdelta,ndelta;
  z=(Mainvolume>127)?127:Mainvolume;
  /*----------------------------------------------------------------------------
     Posttable is for converting 16 bit mixing result (all channels simply
     added) to the resulting 8 bit data for output (scaled with mainvolume)
  -----------------------------------------------------------------------------*/
  #ifdef __DEBUGGING__
  switch (outformat) {
    case 0: printf("8bit unsigned output\n"); break;
    case 1: printf("16bit unsigned output\n"); break;
    case 2: printf("8bit signed output\n"); break;
    case 3: printf("16bit signed output\n"); break;
    default: printf("not implemented.\n"); break;
  }
  #endif

  if (outformat<3)  // equal lookup tables for 8/16bit modes !
  {
    max=4*256*127/z;delta=128*256*256/max; // for the first: max>256 !!! or useless results
    // positiv & negativ border for HI value:
    negb=((32768-max-1)>>8);posb=((32768+max-1)>>8);
    pdelta=(32768+max-posb*256)*delta/256;
    ndelta=(32768-max-negb*256)*delta/256;
    #ifdef __DEBUGGING__
      printf("max+/-: %d (%d: %d,%d)   hi:(%d, %d)\n",max,delta,ndelta,pdelta,(int)negb,(int)posb);
    #endif
    for (i=0;i<256;i++)
    { val=i*delta/256;
      LOtab[    i]=0; // postab[hi]==0 (below 32768-max)
      LOtab[256+i]=(256*negb+i<=32768-max)?0:val-ndelta;    // postab[hi]==1 (near  32768-max --> does change with LOval)
      LOtab[512+i]=val;
      LOtab[768+i]=(256*posb+i>=32768+max)?0xffff:0xffff-pdelta+val;    // postab[hi]==1 (near  32768+max --> does change with LOval)
      if (i<negb)  { postab[i]=0;HItab[i]=0; } else
      if (i==negb) { postab[i]=256;HItab[i]=0; } else
      if (i<=128)   { postab[i]=512;HItab[i]=0x8000-(128-i)*delta; }

      if (i>posb)  { postab[i]=0;HItab[i]=0xffff; } else
      if (i==posb) { postab[i]=768;HItab[i]=0; } else
      if (i>128)   { postab[i]=512 ;HItab[i]=0x8000+(i-128)*delta; }
    }
    #ifdef __DEBUG_TABLES__
    WORD res2,res,w;
    int h;
    while (1)
    {
    fscanf(stdin,"%i",&h);
    w=32768+h;
    res=(SWORD)(w-32768)*128/z+128; res=((SWORD)res<0)?0:(res>255)?255:res;
    res2=LOtab[(BYTE)w + postab[w>>8]]+HItab[w>>8];
    printf("%d (%i) -->  %d  (%d,%d,%d)",w,h,res2,postab[w>>8],LOtab[(BYTE)w+postab[w>>8]],HItab[w>>8]);
    getchar();
    }
    exit(0);
    #endif
  } 
}

void Mixer16_c::amplify_result(void * Dest,WORD count)
{ int i;
  WORD * h_buf16=MixBuffer;
  BYTE * bptr=(BYTE*)Dest;
  WORD * wptr=(WORD*)Dest;
  WORD w;
  if (outformat==0 || outformat==2)
  { /*-------------------------------------------------------------------------
      after mixing all channels together, scale the 16bit results down to 8bit
      that's ofcourse done by lookup tables (scaling and clipping precalculated)
      -------------------------------------------------------------------------*/
    for (i=count;i>0;i--)
    { w=*(h_buf16++);
      *(bptr++)=(LOtab[(BYTE)w + postab[w>>8]]+HItab[w>>8])>>8;
    }
  } else // 16bit output
  { /*-------------------------------------------------------------------------
      after mixing all channels together, scale the 16bit results to 16bit
      that's ofcourse done by lookup tables (scaling and clipping precalculated)
      -------------------------------------------------------------------------*/
    for (i=count;i>0;i--)
    { w=*(h_buf16++);
      *(wptr++)=LOtab[(BYTE)w + postab[w>>8]]+HItab[w>>8];
    }
  }
}

void Mixer16_c::clear_buffer(WORD count)
{ DWORD i;
  WORD *h=MixBuffer;
  for (i=count;i>0;i--) *h++=32*1024;
}

void * Mixer16_c::ptr2buf(void)
{ return this->MixBuffer;
}

void * Mixer16_c::add_sample8(void * dest, BYTE * src, DWORD *offset,WORD *frac,
                             BYTE dir, DWORD step, WORD todo, BYTE vol, BYTE panpos)
// source "src" is an 8bit sample
// mix_8(),mix_8i() are virtual -> they do point to mono or stereo mix_8(),mix_8i()
{ DWORD ofs;
  DWORD overide=0;
  void * tmpdest;
  if (dir==0)
  { // while mixing we can only handle a 16bit offset
    // (16bit offset, 16bit fraction in one 32bit value)
    // for that reason prepare offset and fraction:

    if ((DWORD)*offset>=0x10000L) src+=(*offset&0xffff0000L);
    if ((DWORD)(*offset&0x0ffffLU) > 0x0ffffLU-(step*todo>>16) )
    { overide=(todo*step>>16)+1;
      src+=overide; // move sample pointer forward to avoid later 16bit offset overides
    }
    ofs=((*offset&0x0ffff)-overide<<16)|(*frac);
    switch (interpolate) {
       case NO_INTERPOLATION:
           tmpdest=mix_8((WORD*)dest,src,(SDWORD*)&ofs,0,step,todo,vol,panpos);
           break;
       case LINEAR_INTERPOLATION:
           tmpdest=mix_8i((WORD*)dest,src,(SDWORD*)&ofs,0,step,todo,vol,panpos);
           break;
       case QUADRIC_INTERPOLATION:
           tmpdest=mix_8qi((WORD*)dest,src,(SDWORD*)&ofs,0,step,todo,vol,panpos);
           break;
    }
    *frac=(ofs&0xffff);
    *offset=((*offset&0xffff0000)|(ofs>>16))+overide;
    return tmpdest;
  }
  if (dir==1)
  { 
    // a bit different to positiv dir, but overides in neg dir ! (msb=0!!)
    if (*offset>=0x8000L) src+=(*offset&0xffff8000L);
    if ((DWORD)(*offset&0x07fffL) <= (step*todo>>16)+1 )
    { overide=(step*todo>>16)+1;
      src-=overide; // move sample pointer back to avoid later 16bit overides
    }
    ofs=(((*offset&0x07fffL)+overide)<<16)|*frac;
    switch (interpolate) {
      case NO_INTERPOLATION:
        tmpdest=mix_8((WORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
        break;
      case LINEAR_INTERPOLATION:
        tmpdest=mix_8i((WORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
        break;
      case QUADRIC_INTERPOLATION:
        tmpdest=mix_8qi((WORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
        break;
    }
    *frac=(ofs&0xffff);
    *offset=((*offset&0xffff8000L)|((SDWORD)ofs>>16))-overide;
    return tmpdest;
  }
  return((WORD *)dest);
}

void * Mixer16_c::add_sample16(void * dest, WORD * src, DWORD *offset,WORD *frac,
                               BYTE dir, DWORD step, WORD todo, BYTE vol, BYTE panpos)
{ /* no implementation yet */ 
  return dest; 
}


/*****************************************************************************
 *  Mono16_c    routines for mixing samples to 13bit data
 ****************************************************************************/

Mono16_c::Mono16_c(DWORD _buffersize,BYTE _interpolate,BYTE output_format):Mixer16_c(_buffersize,_interpolate,output_format)
{ no_samples=buffersize;
}


WORD * Mono16_c::mix_8(WORD *Dest,BYTE * Sourc,SDWORD* ofs,BYTE dir, 
		      DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ BYTE sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *t_vol=&((*volumetable)[vol][0]);

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=t_vol[sample];
      tmpofs+=delta;
    }
  else // neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

WORD * Mono16_c::mix_8i(WORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *t_vol=&((*volumetable)[vol][0]);

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=t_vol[sample];
      tmpofs+=delta;
    }
  else // neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(SDWORD)(Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

WORD * Mono16_c::mix_8qi(WORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *t_vol=&((*volumetable)[vol][0]);

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=t_vol[sample];
      tmpofs+=delta;
    }
  else // neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
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

WORD * Stereo16_c::mix_8(WORD *Dest,BYTE * Sourc,SDWORD* ofs,BYTE dir, 
		      DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ BYTE sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  VOLTB_BASE *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs+=delta;
    }
  else // neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

WORD * Stereo16_c::mix_8i(WORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  VOLTB_BASE *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);
  
  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs+=delta;
    }
  else // neg dir
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

WORD * Stereo16_c::mix_8qi(WORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  VOLTB_BASE *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  VOLTB_BASE *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);
  
  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs+=delta;
    }
  else // neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}
