/****************************************************************************
   SoundBlaster support (SB 1.0 compatible, and higher)
   ( all supported modes: 8/16bit mono,stereo up to 44kHz)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 09.01.1996
  rewritten for new player interface: 03.01.1997
****************************************************************************/
// if you want to use SB include this header and:
// 1. Enter  'REGISTRY( SB_c , USE_SB );'  (MACRO from REGISTRY.H)
// 2. Fill SB_HardwareOptionStruct (e.g. with SoundBlaster_Autodetect() )
// 3. Call 'PlayerControl.InstallDevice( USE_SB ,SB_HardwareOptionStruct* )'

#ifndef __SB_H__
#define __SB_H__

#include "mtypes.h"
#include "player.h"
#include "devices/mixdev.h"
#include "devices/mixers.h"
#include "arch/dos/dos_irq.h"

struct SB_HardwareOptionStruct {
		WORD base;
		BYTE irq;
		BYTE dma8;
		BYTE dma16;
		Mixer_c * Mixer; // configure Mixer for your needs...
	};
typedef SB_HardwareOptionStruct * SB_HardwareOptionsPtr;

// defines for SB_settings:
#define SB_UNSIGNED 0
#define SB_SIGNED   1
#define SB_BOTH     2

#define SB_MONO     1
#define SB_STEREO   2

#define SB_8BIT     0
#define SB_16BIT    1

struct SB_Settings { DWORD MixFrequency;
		     BYTE bits;   // 0: 8bits 1:16 bits
		     BYTE stereo; // 0: off   1: on
		     BYTE sign;   // 0: unsigned data (all SBs) 1: signed data (SB16)
		     Mixer_c * Mixer; // not used if ==NULL (configure mixer for your needs)
		     Filter_c * Filter;
		   };
typedef SB_Settings * SB_SettingsPtr;

class SB_c: public MixDevice_c {
  public:
	SB_c(DeviceOptionsPtr);
        ~SB_c(void);
        Status Configure(DeviceSettingsPtr);

	Status Start(ModuleClass*);
	Status Stop();
	Status Continue();

  protected:
	WORD Card_Base;
	BYTE Card_IRQ;
	BYTE Card_DMA8;
	BYTE Card_DMA16;
        int StereoMultiply2; // used for SBPRO stereo
        BYTE DSP_version; // info about SB model
	BYTE emulAI;      // flag: emulate autoinit
	int active;       // flag: device active

        void fillbuffer(ModuleClass*); // is called by the IRQ handler
  public:
    friend void SB_IRQ_handler(UserData);            // IRQ-handler for playing
};

/* Implement this: **
  SoundBlaster_Autodetect(struct SB_HardwareOptionStruct *);
  SoundBlaster_ReadEnvironment(struct SB_HardwareOptionStruct *);
*/

#endif
