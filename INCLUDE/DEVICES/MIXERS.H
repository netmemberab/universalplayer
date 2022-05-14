/****************************************************************************
    Mixers - mix routines for all non wavetable soundcards
  --------------------------------------------------------------------------
  (c) copyright 1995,1996 by A.Baresel

  created: 24.7.1996
****************************************************************************/

#ifndef __MIXERS_H__
#define __MIXERS_H__

#include "mtypes.h"

/*****************************************************************************
 *  Mixer_c    super class of all mixing classes. Abstract !
 ****************************************************************************/
class Mixer_c { public:
		  Status status;

		  virtual ~Mixer_c() { }
		  virtual void setup(DWORD size,BYTE interpolate,BYTE outputformat)=0;
                  virtual void clear_buffer(WORD count)=0;
		  virtual void amplify_result(void * dest,WORD count)=0;
                  virtual void * ptr2buf(void)=0;
                  virtual void * add_sample8( void * dest, BYTE * src,DWORD *offset,DWORD *frac,
                                           BYTE dir, DWORD step_int,DWORD step_frac, WORD todo, BYTE vol, BYTE panpos)=0;
                  virtual void * add_sample16( void * dest, WORD * src,DWORD *offset,DWORD *frac,
                                           BYTE dir, DWORD step_int,DWORD step_frac, WORD todo, BYTE vol, BYTE panpos)=0;
              };

#include "mixer_13.h"
#include "mixer_16.h"
#include "mixer_24.h"

#endif
