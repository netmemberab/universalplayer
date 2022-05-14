/****************************************************************************
    MixDevice - device interface for all non wavetable soundcards
    (time critical mixing routines are implemented in Mixer.cc)
  --------------------------------------------------------------------------
  (c) copyright 1995,1996 by A.Baresel

  created: 5.3.1996
  ( Filter support added on 16.4.1996 )
  ( redesigned 08.07.1996 )
  ( 24.7.1996  mix routines moved to separate class Mixer_c )
****************************************************************************/

#ifndef __MIXDEV_H__
#define __MIXDEV_H__

#include "player.h"
#include "registry.h"
#include "device.h"
#include "filter.h"
#include "mixers.h"

// MIX output modes: e.g. (MIX_MONO|MIX_16BIT|MIX_SIGNED)
#define MIX_MONO           0
#define MIX_STEREO         1
#define STEREOMODE	   1

#define MIX_SIGNED         0
#define MIX_UNSIGNED       2
#define SIGNMODE	   2

#define MIX_8BIT           0
#define MIX_16BIT 	   4
#define BITMODE		   4


// settings:
#define NO_CHANGES	      0
#define NO_INTERPOLATION      1
#define LINEAR_INTERPOLATION  2
#define QUADRIC_INTERPOLATION 3

// mixing directions:
#define NEG_DIR 1
#define POS_DIR 0
#define MIX_NO_LOOP   0
#define MIX_FWD_LOOP  1
#define MIX_BIDI_LOOP 2

#define MAXCHANNELS 32

// not more than 255 resources !!!
#define MAX_RESOURCES		16
// do not change:
#define MAX_SAMPLES_PER_RSC   8192

struct MixerSettingsStruct {
	 DWORD MixFrequency;
	 int   mixmode;     // output format...
         Mixer_c * Mixer;   // no changes if ==NULL
	 DWORD maxbuffer; // maximum buffer size is mixed at once
	 Filter_c * Filter; // no changes if ==NULL
       };
typedef MixerSettingsStruct * MixerSettingsPtr;

class MixDevice_c: public DeviceClass {
  public:
	MixDevice_c();
	~MixDevice_c();

        Status Configure(DeviceSettingsPtr); /* Filter etc. */

        Status Start(ModuleClass*);
	// Status Stop();
	// Status Continue();

        void SetupChannel(BYTE chn,ChannelStruct&);
	void SetupChannels(ChannelSettings&);

	ResourceID AllocateInstruments(InstrumentIDs,int noi);
	Status UploadInstrument(InstrumentID,InstrumentLoaderClass&);
	Status FreeInstruments(ResourceID);

  protected:
        Status mixing(ModuleClass*,void* Buffer,DWORD Buffersize);

	#define MAXINSTRUMENTS 1000

	Filter_c * Filter;  // pointer to filter class
        Mixer_c  * Mixer;   // pointer to mixing class
        friend class Filter_c;

   private: /* following data and methods must be not used in classes are based on
	       MixDevice that's why they're private */

      // output format: (change this with Configure)
      int BitMode;          // MIX_8BIT,MIX_16BIT
      int StereoMode;       // MIX_MONO,MIX_STEREO
      int Format;          
      DWORD SampleRate;

      struct channel_t
	{ void * ptr;	       // pointer to sample _start_ in memory
	  BYTE bits;           // number of bits per sample (8/16bit)

	  DWORD offset;       // current offset from samplestart
	  DWORD frac_ofs;     // fractional part of current offset

	  BYTE  looptype;     // noloop/normalloop/pingpong-loop
	  DWORD loopstart;    // sample loop start offset
	  DWORD loopend;      // sample loop end offset
	  DWORD sampleend;    // endoffset of sample (EOS)
	  
	  BYTE  direction;    // ==NEG_DIR or ==POS_DIR
          DWORD step_int;     // 32bit step rate (integer part)
	  DWORD step_frac;    // 32bit step rate (fractional part)

	  DWORD frequency;    // # Hz to play the instrument with
	  InstrumentID instrid; // sample we currently mix in this channel
	  ResourceID instrsc;   // resource the instrument belongs to
	  DWORD	instno;		// instrument # in the resource

	  BYTE  mixchn;       // flag if we currently mixing a sample in this channel
	  BYTE  volume;       // channel volume
	  BYTE  panpos;       // channel pan position
	  //BYTE  mixflags;     // debug mixing flags (e.g. silent: bit0=1)
	} channel[MAXCHANNELS];

      struct SampleStruct {
	  InstrumentID id;
          int    bits;           // 8 or 16bits
	  BYTE   looptype;       // 0=noloop,1=normal loop,2=ping-pong loop
	  DWORD  endpos;         // stop mixing at this offset
	  DWORD  loopstart;
	  DWORD  loopend;

	  void * data;         // pointer to data in memory
	};
      struct SampleResourceStruct {
	  SampleStruct * smpinfo; // list of pointers to all samples of this resource
	  int no_samples;
	};
      typedef SampleResourceStruct * SampleResourcePtr;
      SampleResourcePtr sample[MAX_RESOURCES];

      int SamplesLeft;                  // number of samples left to nexttick()

      void  mix_chunk(void * buffer, DWORD nr_samples);
      void  newnotes(DeviceCommandsStruct&);
  protected:
        inline BYTE GetBitMode()    { return BitMode;}     // MIX_8BIT or MIX_16BIT
        inline BYTE GetStereoMode() { return StereoMode; } // MIX_MONO or MIX_STEREO
        inline BYTE GetFormat()     { return Format; }     // UNSIGNED, ULAW, RAW, SIGNED
};

#endif
