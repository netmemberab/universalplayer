/****************************************************************************
   sample - a special instrument type: SAMPLE 
   (some often used sample conversions are implemented in this class)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 29.10.1996
****************************************************************************/

#if !defined(__SAMPLEFILE_H__)
#define __SAMPLEFILE_H__

#include <stdio.h>
#include "instrmts/sample.h"

/*
  SampleFileLoaderClass 
   - IT_SAMPLE is the only supported input & output format.
     (use SampleSpec to specify the type 8/16 & signed/unsigned)
*/
class SampleFileLoaderClass: public InstrumentLoaderClass {
	protected:
		Status state;
		FILE* input;
		DataSize SamplesLeft; // samples left on input stream
		// ----- Sample info: ------
		InstrumentID id;
		DataSize SampleSize;  // sample size
		DWORD loopstart,loopend;
		int loopflag;

		Status (SampleFileLoaderClass::*LoadFunc)(DataPtr,DataSize); // depends on inputtype...
		SampleType inputtype,outputtype;

		Status Load_S8(DataPtr,DataSize);
		Status Load_U8(DataPtr,DataSize);
		Status Load_S16(DataPtr,DataSize);
		Status Load_U16(DataPtr,DataSize);
		Status Load_Unknown(DataPtr,DataSize) { return NO_SUPPORT; }
	public:
		SampleFileLoaderClass():LoadFunc(NULL),id(0),state(NO_ERROR),SamplesLeft(0),SampleSize(0) { }
		Status SetOutputFormat(InstrumentType,UserSpecPtr); // for the next Load()
		Status SetPreferedFormat(InstrumentSpec*,int); // set prefered output format (one of the entries)

		DataSize InstrumentDataSize(InstrumentID); // returns number of output samples
		Status RegisterSample(InstrumentID,SampleType,FILE*,DataSize,DWORD loopstart,DWORD loopend,int loopflag);
		Status GetInstrumentSpecs(InstrumentID,UserSpecPtr);
		Status Load(InstrumentID,DataPtr,DataSize);
		Status FinishLoad(InstrumentID);
		Status GetStatus() { return state; }
};

#endif
