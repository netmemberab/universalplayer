/****************************************************************************
   sample - a special instrument type: SAMPLE 
   (some often used sample conversions are implemented in this class)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 29.10.1996
  redesigned: 17.2.1997 for different sample loader classes
****************************************************************************/

#if !defined(__SAMPLESTREAM_H__)
#define __SAMPLESTREAM_H__

#include "iostream.h"
#include "instrmts/sample.h"

/*
  SampleStreamLoaderClass 
   - IT_SAMPLE is the only supported input & output format.
     (use SampleSpec to specify the type 8/16 & signed/unsigned)
*/
class SampleStreamLoaderClass: public InstrumentLoaderClass {
	protected:
		Status state;
		istream * input;
		DataSize SamplesLeft; // samples left on input stream
		// ----- Sample info: ------
		InstrumentID id;
		DataSize SampleSize;  // sample size
		DWORD loopstart,loopend;
		int loopflag;

		Status (SampleStreamLoaderClass::*LoadFunc)(DataPtr,DataSize); // depends on inputtype...
		SampleType inputtype,outputtype;

		Status Load_S8(DataPtr,DataSize);
		Status Load_U8(DataPtr,DataSize);
		Status Load_S16(DataPtr,DataSize);
		Status Load_U16(DataPtr,DataSize);
		Status Load_Unknown(DataPtr,DataSize) { return NO_SUPPORT; }
	public:
		SampleStreamLoaderClass():LoadFunc(NULL),id(0),state(NO_ERROR),SamplesLeft(0),SampleSize(0) { }
		Status SetOutputFormat(InstrumentType,UserSpecPtr); // for the next Load()
		Status SetPreferedFormat(InstrumentSpec*,int); // set prefered output format (one of the entries)

		DataSize InstrumentDataSize(InstrumentID); // returns number of output samples
		Status RegisterSample(InstrumentID,SampleType,istream&,DataSize,DWORD loopstart,DWORD loopend,int loopflag);
		Status GetInstrumentSpecs(InstrumentID,UserSpecPtr);
		Status Load(InstrumentID,DataPtr,DataSize);
		Status FinishLoad(InstrumentID);
		Status GetStatus() { return state; }
};

#endif
