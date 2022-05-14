/****************************************************************************
   registry - infoheader for all supported devices & modules
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 08.09.1996
****************************************************************************/

#if !defined(__REGISTRY_H__)
#define __REGISTRY_H__

//#include <fstream.h>
#include <stdio.h>
#include "player.h"

// DEVICE REGISTRY ----------------------------------------------------------
class DeviceClass;
typedef DeviceClass* (*DeviceRegisterFunc)(DeviceOptionsPtr);

#define REGISTER_DEVICE(REGISTER_NAME,DEVICE_CLASS) \
  class DEVICE_CLASS; \
  inline DeviceClass* REGISTER_NAME(DeviceOptionsPtr DOP) \
    { return new DEVICE_CLASS(DOP); }

// MODULE REGISTRY ----------------------------------------------------------
class ModuleClass;
/*
typedef ModuleClass* (*ModuleRegisterFunc)(ifstream&,DeviceClass*);

#define REGISTER_MODULE(REGISTER_NAME,MODULE_CLASS) \
  class MODULE_CLASS; \
  inline ModuleClass* REGISTER_NAME(ifstream& i,DeviceClass* d) \
    { return new MODULE_CLASS(i,d); }
*/
typedef ModuleClass* (*ModuleRegisterFunc)(FILE*,DeviceClass*);

#define REGISTER_MODULE(REGISTER_NAME,MODULE_CLASS) \
  class MODULE_CLASS; \
  inline ModuleClass* REGISTER_NAME(FILE* i,DeviceClass* d) \
    { return new MODULE_CLASS(i,d); }

// INSTRUMENT TYPEs ---------------------------------------------------------
// -> that's in the wrong header !! <-
// output formats for instrument-load-subclasses
#define IT_SAMPLE      1
#define IT_ADLIB       2

#endif
