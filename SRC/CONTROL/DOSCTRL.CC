/****************************************************************************
   Player Command Class implementation
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 03.10.1996
****************************************************************************/
#include "player.h"
#include <stdio.h>
//#include <iostream.h>

#include "plyctrl.h"

/* ---------------------------------------------------------------------------
   Module list and Device list handling - might change so don't modify the
   lists by hand, use the following routines:
*/

inline Status PlayerControlClass::GetDeviceClassbyID(DeviceID id,DeviceClassPtr& p)
{ if (id!=devicelist[id&0x0fff].devid) { p=NULL;return BAD_ID; }
  p=devicelist[id&0x0fff].device;
  return NO_ERROR;
}

inline Status PlayerControlClass::SetDeviceClassbyID(DeviceID& id,DeviceClassPtr p)
{ if (id==0)
  { // new entry
    int i=0;while (i<MAX_DEVICES && devicelist[i].devid!=0) i++;
    if (i==MAX_DEVICES) return NO_RESOURCES;
    devicelist[i].devid=id=((DeviceID)this & 0xfffff000) + i;
  }
  devicelist[id&0x0fff].device=p;
  if (p==NULL) // delete entry
  { devicelist[id&0x0fff].devid=0;
  }
  return p->GetStatus();
}

inline Status PlayerControlClass::GetModuleClassbyID(ModuleID id,ModuleClassPtr& p,DeviceID& loaddev)
{ if (id!=modulelist[id&0x0fff].modid) { p=NULL;return BAD_ID; }
  p=modulelist[id&0x0fff].module;
  loaddev=modulelist[id&0x0fff].loaddev;
  return NO_ERROR;
}

inline Status PlayerControlClass::SetModuleClassbyID(ModuleID& id,ModuleClassPtr p,DeviceID loaddev)
{ if (id==0)
  { // new entry
    int i=0;while (i<MAX_MODULES && modulelist[i].modid!=0) i++;
    if (i==MAX_MODULES) return NO_RESOURCES;
    modulelist[i].modid=id=((ModuleID)this & 0xfffff000) + i;
  }
  modulelist[id&0x0fff].module=p;
  modulelist[id&0x0fff].loaddev=loaddev;
  if (p==NULL) // delete entry
  { modulelist[id&0x0fff].modid=0;
  }
  return NO_ERROR;
}

void PlayerControlClass::RemoveLoadedModules(DeviceID devid)
/*----- removes all modules where loaded on device 'devid' -----*/
{ int i;
  for (i=0;i<MAX_MODULES;i++)
    if (modulelist[i].loaddev==devid) 
    { delete modulelist[i].module;
      modulelist[i].modid=0;
    };
}
/* --======================= END OF LIST HANDLING =======================--- */

// Initializing: -------------------------------------------------------------
PlayerControlClass::PlayerControlClass()
{ int i;
  trace("Initialize PlayerControl.\n");
  for (i=0;i<MAX_DEVICES;i++) devicelist[i].devid=0;
  for (i=0;i<MAX_MODULES;i++) modulelist[i].modid=0;
}

PlayerControlClass::~PlayerControlClass()
{ // close all opened devices and destruct the modules
  // trace("Destroy PlayerControl.\n");
  int i;
  for (i=0;i<MAX_DEVICES;i++)
    if (devicelist[i].devid!=0) delete (devicelist[i].device);
  for (i=0;i<MAX_MODULES;i++)
    if (modulelist[i].modid!=0) delete (modulelist[i].module);
  // trace("END: ~Control\n");
}

/*
 ==== DeviceSettings: =======================================================
*/

// ---------------- ---- -  -
 // initialize one Device and return an ID is used for later accessing
// ---------- --- -  -
DeviceID PlayerControlClass::InstallDevice(DeviceRegisterFunc dr,DeviceOptionsPtr options)
{ Status status;
  DeviceID devid=0;
  trace("PlayerControl: try installing device...\n");
  if (status=SetDeviceClassbyID(devid,dr(options))) return 0;
  if (devid==0) trace("PlayerControl: install device failed.\n");
  return devid;
}

DeviceID PlayerControlClass::InstallDevice(DeviceClassPtr d)
{ Status status;
  DeviceID devid=0;
  trace("PlayerControl: insert device...\n");
  if (status=SetDeviceClassbyID(devid,d)) return 0;
  if (devid==0) trace("PlayerControl: no free entrie for this device.\n");
  return devid;
}


Status PlayerControlClass::UninstallDevice(DeviceID devid)
{ // close device, and remove all modules if there are some loaded on this dev.
  DeviceClass * dev;
  Status stat;
  if (stat=GetDeviceClassbyID(devid,dev)) return stat;
  dev->Stop();
  RemoveLoadedModules(devid); // removes all modules were loaded on this device
  delete dev;
  SetDeviceClassbyID(devid,NULL); // delete entry !
  return NO_ERROR;
}

Status PlayerControlClass::ConfigureDevice(DeviceID devid,DeviceSettingsPtr sets)
{ DeviceClass * dev;
  Status stat;
  if (stat=GetDeviceClassbyID(devid,dev)) return stat;
  dev->Configure(sets);
}

/*
 ==== Module Settings =======================================================
*/
ModuleID PlayerControlClass::LoadModule(DeviceID devid,ModuleRegisterFunc mr,FILE*f,offset_t ofs=0)
{ DeviceClass * dev;
  ModuleClass * mod;
  ModuleID modid;
  Status stat;
  trace("PlayerControl: try loading module...\n");
  if (status=GetDeviceClassbyID(devid,dev)) return 0; // no id
  if (ftell(f)!=ofs) fseek(f,ofs,SEEK_SET);
  trace("Get device ok.\n");
  mod=mr(f,dev);  // call initialize/load registered module class
  if (mod==NULL) { status=NO_MEMORY;return 0; } // not enough memory for module class instance
  #ifdef __DEBUGGING__
    printf("Status: %i\n",mod->GetStatus());
    trace("load done.\n");
  #endif
  modid=0;
  if (status=SetModuleClassbyID(modid,mod,devid))
  { delete mod; // sorry but no free entry...
    return 0; // no id
  }
  if ((status=mod->GetStatus())==NO_ERROR) return modid; 
  else return 0; // no ID
}

/*
ModuleID PlayerControlClass::LoadModule(DeviceID devid,ModuleRegisterFunc mr,ifstream& f,offset_t ofs=0)
{
  DeviceClass * dev;
  ModuleClass * mod;
  ModuleID modid;
  Status stat;
  trace("PlayerControl: try loading module...\n");
  if (status=GetDeviceClassbyID(devid,dev)) return 0; // no id
  if (f.tellg()!=ofs) f.seekg(ofs,ios::beg);
  trace("Get device ok.\n");
  mod=mr(f,dev);  // call initialize/load registered module class
  if (mod==NULL) { status=NO_MEMORY;return 0; } // not enough memory for module class instance
  printf("Status: %i\n",mod->GetStatus());
  trace("load done.\n");
  modid=0;
  if (status=SetModuleClassbyID(modid,mod,devid))
  { delete mod; // sorry but no free entry...
    return 0; // no id
  }
  if ((status=mod->GetStatus())==NO_ERROR) return modid; 
  else return 0; // no ID
  return NO_SUPPORT;
}
*/

ModuleID PlayerControlClass::LoadModule(DeviceID d,ModuleRegisterFunc mr,char *filename,offset_t ofs=0)
{ /*
  ifstream m=ifstream(filename,ios::in | ios::bin);
  if (!m) { status=BADMUSICFILE;return 0; }
  return LoadModule(d,mr,m,ofs);
  */
  FILE *f=fopen(filename,"rb");
  if (f==NULL) { status=BADMUSICFILE;return 0; }
  return LoadModule(d,mr,f,ofs);

}

Status PlayerControlClass::RemoveModule(ModuleID modid)
{ DeviceClass * dev;
  ModuleClass * mod;
  DeviceID devid;
  Status stat;
  if (stat=GetModuleClassbyID(modid,mod,devid)) return stat;
  if (stat=GetDeviceClassbyID(devid,dev)) return stat;
  dev->FreeInstruments(mod->GetInstrumentsID()); 
  delete mod;
  SetModuleClassbyID(modid,NULL,0); // delete it
  return NO_ERROR;
}

/*		
 ==== Playing routines: ======================================================
*/
Status   PlayerControlClass::StartPlay(DeviceID devid,ModuleID modid)
{ DeviceClass * dev;
  ModuleClass * mod;
  Status stat;
  if (stat=GetDeviceClassbyID(devid,dev)) return stat;
  if (stat=GetModuleClassbyID(modid,mod,devid)) return stat;
  return dev->Start(mod);
}

Status   PlayerControlClass::StopPlay(DeviceID devid)
{ DeviceClass * dev;
  Status stat;
  if (stat=GetDeviceClassbyID(devid,dev)) return stat;
  return dev->Stop();
}

Status   PlayerControlClass::ContinuePlay(DeviceID devid)
{ DeviceClass * dev;
  Status stat;
  if (stat=GetDeviceClassbyID(devid,dev)) return stat;
  return dev->Continue();
}
