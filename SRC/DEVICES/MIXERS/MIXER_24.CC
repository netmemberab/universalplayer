/****************************************************************************
    Mixer - mix routines for all non wavetable soundcards
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 1.8.1996
****************************************************************************/

/* Mixer24_c is SuperClass of Mono24_c & Stereo24_c, all methods which are
   equal for both mono and stereo mixing are implemented in that class     */

/* Mono24_c, Stereo24_c ... high quality mixing with full 16bit sample support,
     technical specs: - do mix 8/16bit samples (8bit *256)
		      - sample scaling with "multiplying": 
                          smp*vol>>3
			-0x3ffff...0x3ffff per channel
			with 32 channels a maximum range for mixing result
			of -0x7fffff...0x7fffff (24bits)
		      - this result is scaled with 4 lookup tables:
			result=UB*65536 + HB*256 + LB:
			  final=LOtab[LB+postab[UB]] + HItab[HB+postab[UB]]+UPtab[UB]
			max=32*8*256*127/vol; (vol:0..127)
		      - final is a 16bit value
		      - for 8bit output the upper 8bit'll be used, but quality
			won't be better than 13/16 bit mixing !!

  --> multiplying/shifting isn't that expensiv to use 4 lookups per sample, but
      the clipping branches make it worth to do so ! A 16MB lookup 
*/

#include "device.h"
#include "mixer.h"
#include "global.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/*****************************************************************************
 *  Mixer24_c    routines for mixing samples to 16bit data
 ****************************************************************************/
Mixer24_c::Mixer24_c(DWORD _buffersize,BYTE _interpolate,BYTE ouput_format)
{ VOLTB_BASE * h_i,i,j;
  status=0;
  buffersize=_buffersize;
  interpolate=_interpolate;
  if ((MixBuffer=(SDWORD *)malloc(4*buffersize)) == NULL )
    { status=NO_MEM;return; }
  outformat=ouput_format;
  mainvolume=127;
  // quadric table for interpolation:
  for (i=0;i<128;i++)
    quadtab[255-i]=255-(quadtab[i]=i*i/128);
}

Mixer24_c::~Mixer24_c(void)
{ free(MixBuffer);
}

void Mixer24_c::amplify_result(void * Dest,WORD count)
{ int i;
  SDWORD * h_buf24=MixBuffer,h;
  BYTE * bptr=(BYTE*)Dest;
  WORD * wptr=(WORD*)Dest;
  BYTE vol=mainvolume;
  SDWORD w;
  if (outformat==0 || outformat==2) // not very usefull ! but implemented anyway
  { for (i=count;i>0;i--)
    { w=*(h_buf24++)>>3;
      if (w>32767L) w=32767;
      if (w<-32767L) w=-32767;
      //printf("%ld,",(w+32767)>>8);getchar();
      *(bptr++)=((DWORD)w+32767)>>8;
      //(((h=(w*vol>>8))>0x07fffL)?0x0ffffL:(h<-32767L)?0x0000:h+32767L)>>8;
    }
  } else // 16bit output prefered ;)
  { for (i=count;i>0;i--)
    { w=*(h_buf24++)>>3;
      if (w>32767L) w=32767;
      if (w<-32767L) w=-32767;
      //printf("%ld.",w+32767);getchar();
      *(wptr++)=w+32767;
      //((h=(w*vol>>8))>32767L)?0x0ffffL:(h<-32767L)?0x0000:h+32767L;
    }
  }
}

void Mixer24_c::clear_buffer(WORD count)
{ memset(MixBuffer,0,4*count);
}

void * Mixer24_c::ptr2buf(void)
{ return this->MixBuffer;
}

void * Mixer24_c::add_sample8(void * dest, BYTE * src, DWORD *offset,WORD *frac,BYTE dir,
                             DWORD step, WORD todo, BYTE vol, BYTE panpos)
// source "src" is an 8bit sample
// mix_8(),mix_8i() are virtual -> they do point to a mono or stereo mix_8(),mix_8i()
{ DWORD ofs;
  DWORD overide=0;
  void * tmpdest;
  if (dir==0 && step>0)
  { // while mixing we can only handle a 16bit offset
    // (16bit offset, 16bit fraction in one 32bit value)
    // for that reason prepare offset and fraction:

    if (*offset>=0x10000L) src+=(*offset&0xffff0000L);
    if ((*offset&0xffffL) > 0x0ffffL-(step*todo>>16) )
    { overide=(todo*step>>16)+1;
      src+=overide; // move sample pointer forward to avoid later 16bit offset overides
    }
    ofs=((*offset&0xffff)-overide<<16)|(*frac);
    switch (interpolate) {
       default:
       case NO_INTERPOLATION:
           tmpdest=mix_8((SDWORD*)dest,src,(SDWORD*)&ofs,0,step,todo,vol,panpos);
           break;
       case LINEAR_INTERPOLATION:
           tmpdest=mix_8i((SDWORD*)dest,src,(SDWORD*)&ofs,0,step,todo,vol,panpos);
           break;
       case QUADRIC_INTERPOLATION:
           tmpdest=mix_8qi((SDWORD*)dest,src,(SDWORD*)&ofs,0,step,todo,vol,panpos);
           break;
    }
    *frac=(ofs&0xffff);
    *offset=((*offset&0xffff0000)|(ofs>>16))+overide;
    return tmpdest;
  }
  if (dir=1 && step>0)
  { 
    // a bit different to positiv dir, but overides in neg dir ! (msb=0!!)
    if (*offset>=0x8000L) src+=(*offset&0xffff8000L);
    if ((*offset&0x07fffL) <= (step*todo>>16)+1 )
    { overide=(step*todo>>16)+1;
      src-=overide; // move sample pointer back to avoid later 16bit overides
    }
    ofs=(((*offset&0x07fffL)+overide)<<16)|*frac;
    switch (interpolate) {
       default:
       case NO_INTERPOLATION:
           tmpdest=mix_8((SDWORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
           break;
       case LINEAR_INTERPOLATION:
           tmpdest=mix_8i((SDWORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
           break;
       case QUADRIC_INTERPOLATION:
           tmpdest=mix_8qi((SDWORD*)dest,src,(SDWORD*)&ofs,1,step,todo,vol,panpos);
           break;
    }
    *frac=(ofs&0xffff);
    *offset=((*offset&0xffff8000L)|((SDWORD)ofs>>16))-overide;
    return tmpdest;
  }
}

void * Mixer24_c::add_sample16(void * dest, WORD * src, DWORD *offset,WORD *frac,BYTE dir,
                               DWORD step, WORD todo, BYTE vol, BYTE panpos)
{ /* no implementation yet */ }


/*****************************************************************************
 *  Mono24_c    routines for mixing samples to 13bit data
 ****************************************************************************/

Mono24_c::Mono24_c(DWORD _buffersize,BYTE _interpolate,BYTE output_format):Mixer24_c(_buffersize,_interpolate,output_format)
{ no_samples=buffersize;
}


SDWORD * Mono24_c::mix_8(SDWORD *Dest,BYTE * Sourc,SDWORD* ofs,BYTE dir, 
		      DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SDWORD sample;
  DWORD tmpofs;

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16]-128;
      *(Dest++)+=(sample<<8)*vol>>3;
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16]-128;
      *(Dest++)+=(sample<<8)*vol>>3;
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

SDWORD * Mono24_c::mix_8i(SDWORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SDWORD sample;
  DWORD tmpofs;

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=(sample-128<<8)*vol>>3;
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=(sample-128<<8)*vol>>3;
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}


SDWORD * Mono24_c::mix_8qi(SDWORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SDWORD sample;
  DWORD tmpofs;

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=(sample-128<<8)*vol>>3;
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=(sample-128<<8)*vol>>3;
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

/*****************************************************************************
 *  Stereo24_c    routines for mixing samples to 13bit stereo data
 ****************************************************************************/

Stereo24_c::Stereo24_c(DWORD _buffersize,BYTE _interpolate,BYTE output_format):Mixer24_c(_buffersize,_interpolate,output_format)
{ no_samples=buffersize/2;
}

SDWORD * Stereo24_c::mix_8(SDWORD *Dest,BYTE * Sourc,SDWORD* ofs,BYTE dir, 
		      DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ BYTE sample;
  DWORD tmpofs;
  BYTE r_vol=(0xff-panpos)*vol>>8;
  BYTE l_vol=panpos*vol>>8;

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=(sample<<8)*l_vol>>3;
      *(Dest++)+=(sample<<8)*r_vol>>3;
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      *(Dest++)+=(sample<<8)*l_vol>>3;
      *(Dest++)+=(sample<<8)*r_vol>>3;
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}

SDWORD * Stereo24_c::mix_8i(SDWORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SDWORD sample;
  DWORD tmpofs;
  BYTE r_vol=(0xff-panpos)*vol>>8;
  BYTE l_vol=panpos*vol>>8;

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=(sample-128<<8)*l_vol>>3;
      *(Dest++)+=(sample-128<<8)*r_vol>>3;
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=(sample-128<<8)*l_vol>>3;
      *(Dest++)+=(sample-128<<8)*r_vol>>3;
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}


SDWORD * Stereo24_c::mix_8qi(SDWORD *Dest,BYTE * Sourc,SDWORD* ofs, BYTE dir,
		       DWORD delta, WORD todo,BYTE vol,BYTE panpos)
{ SDWORD sample;
  DWORD tmpofs;
  BYTE r_vol=(0xff-panpos)*vol>>8;
  BYTE l_vol=panpos*vol>>8;

  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=(sample-128<<8)*l_vol>>3;
      *(Dest++)+=(sample-128<<8)*r_vol>>3;
      tmpofs+=delta;
    }
  else
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*(quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=(sample-128<<8)*l_vol>>3;
      *(Dest++)+=(sample-128<<8)*r_vol>>3;
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
}
