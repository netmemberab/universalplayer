/****************************************************************************
   GUS - gravis ultrasound wavetable card
  --------------------------------------------------------------------------
  driver (c) copyright 1996 by A.Baresel

  created: 23.12.1996
****************************************************************************/
// if you want to use GUS include this header and:
// 1. Enter  'REGISTRY( GUS_c , USE_GUS );'  (MACRO from REGISTRY.H)
// 2. Call   'PlayerControl.InstallDevice( USE_GUS ,GUS_HardwareOptionStruct* )'

#ifndef __GUS_H__
#define __GUS_H__

#include "player.h"
#include "registry.h"
#include "device.h"
#include "instrmnt.h"
#include "arch/dos/dos_irq.h"

// not more than 255 resources !!!
#define MAX_RESOURCES		16
// do not change:
#define MAX_SAMPLES_PER_RSC   8192

struct GUS_HardwareOptionStruct {
		WORD base;     // !!
                BYTE dram_dma;
                BYTE adc_dma;
                BYTE gf1_irq;  // !!
                BYTE midi_irq;
		BYTE voices;
	};
typedef GUS_HardwareOptionStruct * GUS_HardwareOptionsPtr;

#define VOICES_OPTIMZED 255

class GUS_c: public DeviceClass {

  public:
	GUS_c(DeviceOptionsPtr);
	~GUS_c(void);

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

	TimerID timerid;
	DWORD DRAM_free,DRAM_size;
	WORD gusbase;
        BYTE gusvoices;        // number of active voices

        WORD GUS_volumes[65];  // logarithmic volume table

  private:

     struct SampleStruct {
	  InstrumentID id;
          BYTE  mode;           // mode bits:  2=16bit,3=loop,4=bidi
	  DWORD startaddr;      // start in GUS-DRAM
	  DWORD loopstart;      // loop start point
	  DWORD loopend;        // loop end point
	  DWORD sampleend;      // for all nonlooping samples this is at the pseudo 'loop'
				// for all others it's at loopend+1 (they'll never reach sampleend)
	};
     struct SampleResourceStruct {
	  SampleStruct * smpinfo; // list of pointers to all samples of this resource
	  int no_samples;
	};
     typedef SampleResourceStruct * SampleResourcePtr;
     SampleResourcePtr sample[MAX_RESOURCES];

     struct channel_t
	{ BYTE  GUSchn;       // hardware channel
          DWORD frequency;    // # Hz to play the instrument with
          WORD  GUS_pitchreg; // value set in pitch register of GUS channel
	  int   instr;        // sample we currently play in this channel
	  BYTE  active;       // flag if virtual channel is active (an instrument is playing)
	  BYTE  volume;       // channel volume
	  BYTE  panpos;       // channel pan position
	  DWORD offset;	      // startoffset
	  DWORD stopaddr;     // if channel is stopped...
	} channel[32];        // 32 virtual channels

     BYTE GUS_channel[32];    // hardware channels off/on/fadingoff
     #define OFF     0
     #define ON      1
     #define FADEOFF 2
     BYTE chn_map[32];        // maps hardware channel to virtual channel
     #define NOT_USED 0xff

     // internal hardware sample upload:
     Status LoadInstrument(SampleStruct*,InstrumentLoaderClass&,DWORD size);
     void  newnotes(ModuleClass*);

     void  SetNewNote(BYTE vchn);
     void  NoteOff(BYTE vchn);
     void  StopNote(BYTE vchn);
     BYTE  GetNextChannel(BYTE current);

     void MemInit(void);

     friend void GUS_timer_irq(UserData);            // timer (IRQ0) for playing
     friend void GUS_card_irq(UserData);             // GUS-card IRQ for playing
							(feature not yet implemented)

};

#endif
