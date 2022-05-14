/****************************************************************************
   instrmnt - "load-instrument" class definitions, info records
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 20.10.1996
****************************************************************************/
// implemented sub classes in <instrmnts/>

#if !defined(__INSTRUMENT_H__)
#define __INSTRUMENT_H__

#include "player.h"

typedef void * UserTypePtr;
typedef void * UserSpecPtr;
typedef DWORD DataSize;
typedef void * DataPtr;
struct InstrumentSpec { InstrumentType type;
			UserTypePtr spec;
		      };

typedef InstrumentID * InstrumentIDs;

class InstrumentLoaderClass {
	public:
		virtual Status SetOutputFormat(InstrumentType,UserTypePtr) = 0;
		virtual Status SetPreferedFormat(InstrumentSpec*,int) = 0; // a list of specs !
		virtual Status GetInstrumentSpecs(InstrumentID,UserSpecPtr) = 0;
		virtual Status Load(InstrumentID,DataPtr,DataSize) = 0;
		virtual Status GetStatus() = 0;
};

#endif
