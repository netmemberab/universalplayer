/****************************************************************************
   Module base class
  --------------------------------------------------------------------------
  (c) copyright 1995,1996 by A.Baresel

  redesigned: 11.09.1996
****************************************************************************/

#if !defined(__MODULE_H__)
#define __MODULE_H__

#include "player.h"
#include <stdio.h>
#include <iostream.h>

class DeviceClass;
typedef DeviceClass * DeviceClassPtr;

/*
   ModuleClass - abstract base class of all music file classes.
    + devices are using this interface to read module data for playing,
    + a DeviceCommandStruct with basic commands is used for this
*/

#define NO_MODULE 0

class ModuleClass {
	protected:
		Status status;
		ModuleID id;
		ResourceID instruments;
		DeviceClassPtr device;
	public:
		ModuleClass(DeviceClassPtr d):device(d),status(0) { }
		virtual ~ModuleClass() { }
		virtual Status StartModule() = 0;
		inline  Status GetStatus() { return status; }
		inline  ResourceID GetInstrumentsID() { return instruments; }
		inline  ModuleID   GetModuleID() { return id; }
		virtual Status GetDeviceCommands(DeviceCommandsStruct&) = 0;
	private:
		ModuleClass() {/*not allowed*/}
		ModuleClass(const ModuleClass&) {/*not allowed*/}
		operator=(const ModuleClass&) {/*not allowed*/}
};

#endif
