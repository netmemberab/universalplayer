/****************************************************************************
   Device Base Class
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  rewritten: 08.09.1996
****************************************************************************/

#if !defined(__DEVICE_H__)
#define __DEVICE_H__

#include "mtypes.h"
#include "instrmnt.h"

#define MAX_CHANNELS 32

struct ChannelStruct {  
			BYTE volume;
			BYTE panning;
			int  enabled;
		};
typedef ChannelStruct ChannelSettings[MAX_CHANNELS];

class DeviceClass {
	public:
		int status;
		int id;
		inline DeviceClass() { status=0; }
		virtual ~DeviceClass(void) { }

		virtual Status Configure(DeviceSettingsPtr) = 0;
		Status  GetStatus(void) { return(status); }

		virtual Status Start(ModuleClass*)=0;
		virtual Status Stop(void)=0;
		virtual Status Continue(void)=0;

		virtual void SetupChannel(BYTE chn,ChannelStruct&) =0;
		virtual void SetupChannels(ChannelSettings&)=0;

		virtual ResourceID AllocateInstruments(InstrumentIDs,int noi)=0;
		virtual Status UploadInstrument(InstrumentID,InstrumentLoaderClass&)=0;
		virtual Status FreeInstruments(ResourceID)=0;
};

#endif
