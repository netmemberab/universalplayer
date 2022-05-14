/****************************************************************************
   SoundBlaster AWE 32 - THE wavetable card
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 16.07.1996
  rewritten for new player interface: 23.10.1996
****************************************************************************/
// if you want to use SBAWE include this header and:
// 1. Enter  'REGISTRY( SBAWEclass , USE_SBAWE );'  (MACRO from REGISTRY.H)
// 2. Fill SBAWE_HardwareOptionStruct (e.g. with SBAWE_ReadEnvironment() );
// 3. Call   'PlayerControl.InstallDevice( USE_SBAWE ,SBAWE_HardwareOptionStruct* )'

#ifndef __AWE32_H__
#define __AWE32_H__

#include "player.h"
#include "registry.h"
#include "device.h"
#include "instrmnt.h"
#include "arch/dos/dos_irq.h"

#define DETECT_AWE32_MEM 0xffffffff
#define AWE32_DRAM_START 0x00200000

// not more than 255 resources !!!
#define MAX_RESOURCES		16
// do not change:
#define MAX_SAMPLES_PER_RSC   8192

struct SBAWE_HardwareOptionStruct {
		WORD awe32_base;
		WORD emu8000_base;
		DWORD dram_start;
		DWORD dram_end;
	};
extern SBAWE_HardwareOptionStruct default_opts;

class SBAWEclass: public DeviceClass { 

  public:
	SBAWEclass(DeviceOptionsPtr);
	~SBAWEclass(void);

	Status Configure(DeviceSettingsPtr) { } /* filters, chorus etc. */

	Status Start(ModuleClass*);
	Status Stop();
	Status Continue();

	void SetupChannel(BYTE chn,ChannelStruct&);
	void SetupChannels(ChannelSettings&);

	ResourceID AllocateInstruments(InstrumentIDs,int noi);
	Status UploadInstrument(InstrumentID,InstrumentLoaderClass&);
	Status FreeInstruments(ResourceID);

  protected: // internal structures ...

	WORD sbawe32_base;
	WORD emu8000_base;
	TimerID timerid;
        DWORD DRAM_free;        // first unused dram address in AWE memory
	int DRAM_transfer;      // set if enableDRAMtransfer was called
				// until disableDRAMtransfer is called
  private:
     BYTE VolTable[65];		// logarithmic volume table
     DWORD DRAM_size;           // in words

     struct SampleStruct { 
	  InstrumentID id;
	  DWORD startaddr;      // start in AWE32-DRAM
	  DWORD realloopend;    // --> if a loop is unrolled (loop,pingpong)
				// --> this marks the offset of loop end in AWE memory
	  DWORD loopstart;      // loop start point
	  DWORD loopend;        // loop end point
	  DWORD sampleend;	// for all nonlooping samples this is at the pseudo 'loop'
				// for all others it's at loopend+1 (they'll never reach sampleend)
	  // for non looping samples a small loop of 8 bytes is added after sampleend
	  // small loops will be unrolled a couple of times (problem at high frequencies)
	  // ping pong loops have to be unrolled (AWE couldn't handle that)
	};
     struct SampleResourceStruct { 
	  SampleStruct * smpinfo; // list of pointers to all samples of this resource
	  int no_samples;
	};
     typedef SampleResourceStruct * SampleResourcePtr;
     SampleResourcePtr sample[MAX_RESOURCES];
     int no_samples;

     struct channel_t
	{ DWORD frequency;    // # Hz to play the instrument with
          WORD  AWEpitchreg;  // value set in pitchreg of AWE32
	  int   instr;        // sample we currently play in this channel
	  BYTE  active;       // flag if channel is active (deactivate with noteoff)
	  BYTE  volume;       // channel volume
	  BYTE  panpos;       // channel pan position
	  BYTE  AWEchn;	      // number of currently used hardware channel on AWE (changes with PlayNote)
	  DWORD offset;	      // startoffset
	  DWORD cur_addr;
	  WORD  lastchanges;
	} channel[32];
     BYTE AWE_channel[30];   // flags which channels are really used (set by PlayNote(),NoteOff())
     WORD overal_volumes[30];  // current overal volumes for all channels (does currently not work)
     // internal instrument upload...
     Status LoadInstrument(InstrumentID,SampleStruct*,InstrumentLoaderClass&,DWORD,int,DWORD,DWORD);
     void  newnotes(ModuleClass*);
     void  PlayNote(BYTE chn);
     void  NoteOff(BYTE chn);
     BYTE  GetNextChannel(BYTE current);

     friend void AWE_play_irq(UserData);            // timer (IRQ0) for playing

};

/* Implement this: **
  SBAWE_Autodetect();
  SBAWE_ReadEnvironment();
*/

#endif
