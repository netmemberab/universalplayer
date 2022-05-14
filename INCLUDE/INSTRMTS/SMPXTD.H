/****************************************************************************
   sample - a special instrument type: SAMPLE 
   (some often used sample conversions are implemented in this class)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 29.10.1996
****************************************************************************/

#if !defined(__SAMPLE_H__)
#define __SAMPLE_H__

#include "mtypes.h"
#include "instrmnt.h"
#include "iostream.h"


class SampleRegisterClass {
	public:
		SampleRegisterClass(int maxsamples); // ??
		~SampleRegisterClass();
		RegisterSample(/*sample datas*/);
};

#if 0
class ExtendedSampleLoaderClass: public SampleLoaderClass {
	public:
		ExtendedSampleLoaderClass(int maxsamples);
		~ExtendedSampleLoaderClass();
		RegisterSample(/*...sample data,samplespec...*/);
		PrepareUpload();
	protected:
		SampleIStruct { DWORD samplesize; // real size
				DWORD reduce2size; // size to reduce to
				DWORD base_frequency;
				InstrumentID id;
			      } * samples;
		typedef SampleIStruct * SampleIStructPtr;

		Status (LoadFunc*)(SampleIStructPtr,DataPtr,DataSize); 
		Status Load_S8(SampleIStructPtr,DataPtr,DataSize);
		Status Load_U8(SampleIStructPtr,DataPtr,DataSize);
		Status Load_S16(SampleIStructPtr,DataPtr,DataSize);
		Status Load_U16(SampleIStructPtr,DataPtr,DataSize);
};
#endif

#endif
