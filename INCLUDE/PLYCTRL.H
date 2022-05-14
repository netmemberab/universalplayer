/****************************************************************************
   Player Control Class definition
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 08.09.1996
****************************************************************************/

#if !defined(__PLYCTRL_H__)
#define __PLYCTRL_H__

#include <stdio.h>
#include <iostream.h>

#include "mtypes.h"
#include "instrmnt.h"
#include "registry.h"
#include "module.h"
#include "device.h"

typedef long offset_t;

typedef DeviceClass * DeviceClassPtr;
typedef ModuleClass * ModuleClassPtr;

#define MAX_DEVICES 10
#define MAX_MODULES 10
class PlayerControlClass {
	private: Status status;
	public:
		PlayerControlClass();
		~PlayerControlClass();

		// DeviceSettings:
		DeviceID InstallDevice(DeviceRegisterFunc,DeviceOptionsPtr);
		DeviceID InstallDevice(DeviceClassPtr);
		Status   UninstallDevice(DeviceID);
		Status   ConfigureDevice(DeviceID,DeviceSettingsPtr);

		// Module Settings
		ModuleID LoadModule(DeviceID,ModuleRegisterFunc,FILE*,offset_t=0);
		ModuleID LoadModule(DeviceID,ModuleRegisterFunc,char * filename,offset_t=0);
		//ModuleID LoadModule(DeviceID,ModuleRegisterFunc,ifstream&,offset_t=0);
		//ModuleID LoadModule(DeviceID,ModuleRegisterFunc,irscstream&,char *filename=NULL);
		Status   RemoveModule(ModuleID);
		
		// Playing routines:
		Status	 StartPlay(DeviceID,ModuleID);
		Status   StopPlay(DeviceID);
		Status   ContinuePlay(DeviceID);

		inline Status GetStatus() { return status; }

	protected:
		struct DeviceList { 
				DeviceID devid;
				DeviceClass * device;
			} devicelist[MAX_DEVICES];
		inline Status GetDeviceClassbyID(DeviceID,DeviceClassPtr&);
		inline Status SetDeviceClassbyID(DeviceID&,DeviceClassPtr);

		struct ModuleList { 
				ModuleID modid;
				ModuleClass * module;
				DeviceID loaddev;
			} modulelist[MAX_MODULES];
		inline Status GetModuleClassbyID(ModuleID,ModuleClassPtr&,DeviceID&);
		inline Status SetModuleClassbyID(ModuleID&,ModuleClassPtr,DeviceID);
                void RemoveLoadedModules(DeviceID devid);

	private:
		PlayerControlClass(const PlayerControlClass&) {/*not allowed*/};
		operator=(const PlayerControlClass&)          {/*not allowed*/};
};

#endif
