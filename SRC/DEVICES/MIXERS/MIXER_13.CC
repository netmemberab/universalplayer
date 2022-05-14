/****************************************************************************
    Mixer - mix routines for all non wavetable soundcards
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 1.8.1996
****************************************************************************/

// Intelx86 Assembler Code (highly optimized)
#define __I86_CODE__

/* Mixer13_c is SuperClass of Mono13_c & Stereo13_c, all methods which are
   equal for both mono and stereo mixing are implemented in that class     */

/* Mono13_c, Stereo13_c ...
    technical specs: - do mix only 8bit samples (high byte of 16bit samples)
		     - samples scaled with volume: (smp*64>>6): -128...127
		       with 32 channels a maximum range for mixing result 
		       of -4096...4095 (13bits)
		     - result is scaled with a 8kb lookup table to 8bit or 16bit
		       (table[value]=scale*value*mainvol/128 <-- value is cliped 
			at borders)
    about quality:   - non interpolated modes have the common problems with
		       distorsion
		     - 8bit output: bad quality at low channel volumes 
			(test it at level 5)
		     - 16bit output uses 5bits more than in 8bit mode but the
			same problem with low volumes
    hints for best sound:
		     - use full mainvol: calc_posttable(127)
		     - set soundcard hardware at 66% volume (e.g. SoundBlaster
			voice, mainvol at 200)

*/

#ifdef __TURBOC__
#error "this code is using 32bit offsets. TurboC doesn't work with this !!"
#endif

#include "device.h"
#include "mtypes.h"
#include "devices/mixdev.h"
#include "devices/mixers.h"
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>

#ifdef __I86_CODE__
#include "m13i86.h"

extern "C" void showregs_Cpp(DWORD EAX,DWORD EBX,DWORD ECX,DWORD EDX,DWORD ESI,
	DWORD EDI,DWORD EBP,DWORD ESP,WORD CS,WORD DS,WORD ES,WORD FS,WORD GS,WORD SS)
{ printf("EAX: %.8LX\nEBX: %.8LX\nECX: %.8LX\nEDX: %.8LX\nESI: %.8LX\nEDI: %.8LX\nEBP: %.8LX\nESP: %.8LX\
\nCS: %.4X\nDS: %.4X\nES: %.4X\nFS: %.4X\nGS: %.4X\nSS: %.4X\n",EAX,EBX,ECX,EDX,ESI,EDI,EBP,ESP,CS,DS,ES,FS,GS,SS);
  getchar();
}
#endif

/*****************************************************************************
 *  Mixer13_c    routines for mixing samples to 13bit data
 ****************************************************************************/
Mixer13_c::Mixer13_c():buffersize(0),MixBuffer(NULL)
{ WORD * h_i,i,j;
  status=0;
  interpolate=NO_INTERPOLATION;
  outformat=MIX_8BIT|MIX_MONO|MIX_UNSIGNED;

  #ifdef __I86_CODE__
    volumetable=&volumetab;
    posttable=(WORD*)&posttab;
  #else
  if ((volumetable=(WORD (*)[65][256])malloc(4*65*256)) == NULL )
    { status=NO_MEMORY;return; }
  #endif

  // initialize Volume-lookup-table
  h_i=(WORD*)volumetable;
  for (i=0;i<65;i++)
    for (j=0;j<256;j++)
      *(h_i++)=((i*(j-128))>>6);

  calc_posttable(127);

  // quadric table for interpolation:
  for (i=0;i<128;i++)
    quadtab[255-i]=255-(quadtab[i]=i*i/128);
}

Mixer13_c::~Mixer13_c(void)
{ 
  #ifndef __I86_CODE
    if (volumetable) free(volumetable);
  #endif
  if (MixBuffer) free(MixBuffer);
  #ifdef __DEBUGGING__
    printf("OPTIMIZE: ------------ \n");
    printf("mixclocks: %li\nmixloops: %li\ncycles per sample: %6.2lf\n",mixclocks,mixloops,(double)mixclocks/mixloops);
    printf("OPTIMIZE ------------- \n");
  #endif
}

void Mixer13_c::setup(DWORD _buffersize,BYTE _interpolate,BYTE _outputformat)
{ buffersize=_buffersize;

  if (_interpolate!=NO_CHANGES) interpolate=_interpolate;

  if ((MixBuffer=(WORD *)malloc(2*buffersize)) == NULL )
    { status=NO_MEMORY;return; }
  if (_outputformat!=NO_CHANGES) outformat=_outputformat;
  calc_posttable(127);
  trace("mixing buffer ok.\n");
}

void Mixer13_c::calc_posttable(BYTE Mainvolume)
{ float a,b,c;
  SWORD i,z;
  WORD sign=0;
  z=Mainvolume&127;
  /*----------------------------------------------------------------------------
     Posttable is for converting 13 bit mixing result (all channels simply
     added) to the resulting 8 bit data for output (scaled with mainvolume)
  -----------------------------------------------------------------------------*/
  if ((outformat & BITMODE)==MIX_8BIT ) 
  { 
    if ((outformat& SIGNMODE)==MIX_SIGNED) sign=128;
    c=256*127.0/z;
    a=2048-c/2;
    b=2048+c/2;
    for (i=0;i<4096;i++)
    { if (i<a) posttable[i]=0; else
      if (i>b) posttable[i]=255; else
      posttable[i]=(SWORD)((i-a)*z/128-sign);
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
    if ((outformat & SIGNMODE)==MIX_SIGNED) sign=0x8000;
    c=256*127.0/z;
    a=2048-c/2;
    b=2048+c/2;
    for (i=0;i<4096;i++)
    { if (i<a) posttable[i]=0; else
      if (i>b) posttable[i]=0xffff; else
      posttable[i]=(WORD)(256*(i-a)*z/128-sign);
    }
  }
}

void Mixer13_c::amplify_result(void * Dest,WORD count)
{ int i;
  WORD * h_buf16=MixBuffer;
  BYTE * bptr=(BYTE*)Dest;
  WORD * wptr=(WORD*)Dest;
  if ( (outformat & BITMODE)==MIX_8BIT ) 
  { /*-------------------------------------------------------------------------
      after mixing all channels together, scale the 13bit results down to 8bit
      that's ofcourse done by a lookup table (scaling and clipping precalculated)
      -------------------------------------------------------------------------*/
    #ifndef __I86_CODE__
      for (i=count;i>0;i--)
        *(bptr++)=(BYTE)(*(posttable+ *(h_buf16++)));
    #else
      _I86_amp13_8(Dest,MixBuffer,count);
    #endif
  } else // 16bit output
  { /*-------------------------------------------------------------------------
      after mixing all channels together, scale the 13bit results to 16bit
      that's ofcourse done by a lookup table (scaling and clipping precalculated)
      -------------------------------------------------------------------------*/
    for (i=count;i>0;i--)
      *(wptr++)=(WORD)(*(posttable+ *(h_buf16++)));
  }
}

void Mixer13_c::clear_buffer(WORD count)
{ int i;
  #ifndef __I86_CODE__
    WORD *h=MixBuffer;
    i=count;while (i--) *h++=2048;
  #else
    _I86_clrbuf13(MixBuffer,2048L,(DWORD)count);
  #endif
}

void * Mixer13_c::ptr2buf(void)
{ return this->MixBuffer;
}

// stupid ........
void * Mixer13_c::add_sample8(void * dest, BYTE * src, DWORD *offset,DWORD *frac,
                             BYTE dir, DWORD step_int,DWORD step_frac, WORD todo, BYTE vol, BYTE panpos)
// source "src" is an 8bit sample
// mix_8(),mix_8i() are virtual -> they do point to mono or stereo mix_8(),mix_8i()
{ 
  switch (interpolate) {
    default:
    case NO_INTERPOLATION:
       return mix_8((WORD*)dest,src,(SDWORD*)offset,frac,dir,step_int,step_frac,todo,vol,panpos);
       break;
    case LINEAR_INTERPOLATION:
       return mix_8i((WORD*)dest,src,(SDWORD*)offset,frac,dir,step_int,step_frac,todo,vol,panpos);
       break;
    case QUADRIC_INTERPOLATION:
       return mix_8qi((WORD*)dest,src,(SDWORD*)offset,frac,dir,step_int,step_frac,todo,vol,panpos);
       break;
  }
}

void * Mixer13_c::add_sample16(void * dest, WORD * src, DWORD *offset,DWORD *frac,
                               BYTE dir, DWORD step_int,DWORD step_frac, WORD todo, BYTE vol, BYTE panpos)
{ /* no implementation yet */
  printf("16 bit samples not yet supported...\n"); exit(-1);
}


/*****************************************************************************
 *  Mono13_c    routines for mixing samples to 13bit data
 ****************************************************************************/

void Mono13_c::setup(DWORD _buffersize,BYTE _interpolate,BYTE output_format)
{ Mixer13_c::setup(_buffersize,_interpolate,output_format);
  no_samples=buffersize;
}


WORD * Mono13_c::mix_8(register WORD *Dest,BYTE * Sourc,SDWORD* ofs,DWORD *frac,BYTE dir, 
		      DWORD delta_int,DWORD delta_frac, WORD todo,BYTE vol,BYTE panpos)
{ // pointer to volumetable allready indexed with volume
  
  #ifndef __I86_CODE__
  WORD *t_vol=&((*volumetable)[vol][0]);

  if ((WORD*)Dest+todo>(WORD*)MixBuffer+buffersize)
  {
    printf("SHITTTTTTTTTTTTT ARGL\n");
    printf("%p, %p, todo: %d, buffersize: %d, allready done: %d\n",Dest,MixBuffer,todo,(WORD)buffersize,Dest-MixBuffer);
    fflush(stdout);exit(-1);
  }

  register BYTE *tmpSRC=Sourc+*ofs;
  register DWORD tmpfrac=*frac>>16;
  register DWORD step=delta_frac>>16;

  if (dir==0)
    do
    { *(Dest++)+=t_vol[*(tmpSRC+(tmpfrac>>16))];
      tmpfrac+=step;
      tmpSRC+=delta_int;
    } while (--todo);
  else // neg dir
    do
    { *(Dest++)+=t_vol[*(tmpSRC+((SDWORD)tmpfrac>>16))];
      tmpfrac-=step;
      tmpSRC-=delta_int;
    } while (--todo);

  *ofs=tmpSRC+(tmpfrac>>16)-Sourc;*frac=tmpfrac<<16;
  return Dest;
  #else  
    _I86_mix13_m8(Dest,Sourc,ofs,frac,dir,delta_int,delta_frac,todo,vol,panpos);
    return Dest+todo;
  #endif
}

WORD * Mono13_c::mix_8i(WORD *Dest,BYTE * Sourc,SDWORD* ofs, DWORD *frac,BYTE dir,
		       DWORD delta_int,DWORD delta_frac, WORD todo,BYTE vol,BYTE panpos)
{ /*
  SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  WORD *t_vol=&((*volumetable)[vol][0]);
  
  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=t_vol[sample];
      tmpofs+=delta;
    }
  else //neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(SDWORD)(Sourc[1+(tmpofs>>16)]-sample)*((WORD)tmpofs)>>16;
      *(Dest++)+=t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
  */
}

WORD * Mono13_c::mix_8qi(WORD *Dest,BYTE * Sourc,SDWORD* ofs, DWORD *frac,BYTE dir,
		       DWORD delta_int,DWORD delta_frac, WORD todo,BYTE vol,BYTE panpos)
{ /*
  SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  WORD *t_vol=&((*volumetable)[vol][0]);
  
  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=t_vol[sample];
      tmpofs+=delta;
    }
  else //neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
  */
}

/*****************************************************************************
 *  Stereo13_c    routines for mixing samples to 13bit stereo data
 ****************************************************************************/

void Stereo13_c::setup(DWORD _buffersize,BYTE _interpolate,BYTE output_format)
{ Mixer13_c::setup(_buffersize,_interpolate,output_format);
  no_samples=buffersize/2;
}

WORD * Stereo13_c::mix_8(WORD *Dest,BYTE * Sourc,SDWORD* ofs,DWORD *frac,BYTE dir, 
		       DWORD delta_int,DWORD delta_frac, WORD todo,BYTE vol,BYTE panpos)
{ 
  #ifndef __I86_CODE__
  WORD *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  WORD *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);

  if ((WORD*)Dest+2*todo>(WORD*)MixBuffer+buffersize)
  {
    printf("SHITTTTTTTTTTTTT ARGL\n");
    printf("%p, %p, todo: %d, buffersize: %d, allready done: %d\n",Dest,MixBuffer,todo,(WORD)buffersize,Dest-MixBuffer);
    fflush(stdout);exit(-1);
  }

  register BYTE *tmpSRC=Sourc+*ofs;
  register DWORD tmpfrac=*frac>>16;
  register DWORD step=delta_frac>>16;

  if (dir==0)
    do
    { *(Dest++)+=l_t_vol[*(tmpSRC+(tmpfrac>>16))];
      *(Dest++)+=r_t_vol[*(tmpSRC+(tmpfrac>>16))];
      tmpfrac+=step;
      tmpSRC+=delta_int;
    } while (--todo);
  else // neg dir
    do
    { *(Dest++)+=l_t_vol[*(tmpSRC+((SDWORD)tmpfrac>>16))];
      *(Dest++)+=r_t_vol[*(tmpSRC+((SDWORD)tmpfrac>>16))];
      tmpfrac-=step;
      tmpSRC-=delta_int;
    } while (--todo);

  *ofs=tmpSRC+(tmpfrac>>16)-Sourc;*frac=tmpfrac<<16;
  return Dest;
  #else  
    _I86_mix13_s8(Dest,Sourc,ofs,frac,dir,delta_int,delta_frac,todo,vol,panpos);
    return Dest+2*todo;
  #endif
}

WORD * Stereo13_c::mix_8i(WORD *Dest,BYTE * Sourc,SDWORD* ofs, DWORD *frac,BYTE dir,
		       DWORD delta_int,DWORD delta_frac, WORD todo,BYTE vol,BYTE panpos)
{ /*
  SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  WORD *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  WORD *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);
  
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
  if (*ofs<0)
  { printf("SHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHIIIIIIIIIIIIT\n");getchar(); }
  return Dest;
  */
}

WORD * Stereo13_c::mix_8qi(WORD *Dest,BYTE * Sourc,SDWORD* ofs, DWORD *frac,BYTE dir,
		       DWORD delta_int,DWORD delta_frac, WORD todo,BYTE vol,BYTE panpos)
{ /*
  SWORD sample;
  DWORD tmpofs;
  // pointer to volumetable allready indexed with volume
  WORD *r_t_vol=&((*volumetable)[(0xff-panpos)*vol>>8][0]);
  WORD *l_t_vol=&((*volumetable)[panpos*vol>>8][0]);
  
  tmpofs=*ofs;
  if (dir==0)
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs+=delta;
    }
  else // neg dir
    for (;todo>0;todo--)
    { sample=Sourc[tmpofs>>16];
      sample+=(((SDWORD)Sourc[1+(tmpofs>>16)]-sample)*quadtab[(WORD)tmpofs>>8])>>8;
      *(Dest++)+=l_t_vol[sample];
      *(Dest++)+=r_t_vol[sample];
      tmpofs-=delta;
    }

  *ofs=tmpofs;
  return Dest;
  */
}
