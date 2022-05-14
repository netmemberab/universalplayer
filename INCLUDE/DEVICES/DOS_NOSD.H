/****************************************************************************
   DOS no sound device
  --------------------------------------------------------------------------
  (c) copyright 1997 by A.Baresel

  created: 17.02.1997
****************************************************************************/

#ifndef __DOS_NOSND_H__
#define __DOS_NOSND_H__

#include "mtypes.h"
#include "player.h"
#include "arch/dos/dos_irq.h"

class NoSound_c: public DeviceClass {
  public:
	NoSound_c(DeviceOptionsPtr);
	~NoSound_c(void);
	Status Configure(DeviceSettingsPtr);

	Status Start(ModuleClass*);
	Status Stop();
	Status Continue();

	void SetupChannel(BYTE chn,ChannelStruct&);
	void SetupChannels(ChannelSettings&);

	ResourceID AllocateInstruments(InstrumentIDs,int noi);
	Status UploadInstrument(InstrumentID,InstrumentLoaderClass&);
	Status FreeInstruments(ResourceID);

  protected:
	TimerID timerid;
	void  newnotes(ModuleClassPtr);

  public:
    friend void NoSound_IRQ_handler(UserData);            // IRQ-handler for playing
};


#endif
