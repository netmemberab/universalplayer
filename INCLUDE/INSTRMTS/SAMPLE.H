/****************************************************************************
   sample - sample defines - used in all sampleloaderclasses
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 29.10.1996
****************************************************************************/

#if !defined(__SAMPLE_H__)
#define __SAMPLE_H__

// sample specific input/output types (UserSpecPtr==SampleSpecPtr):
typedef int SampleType;
typedef SampleType * SampleTypePtr;
struct SampleSpec {
		SampleType type;
		DataSize size;
		int loopflag;   // bits: 1-loop,2-bidi
		DWORD loopstart;
		DWORD loopend;
	};
typedef SampleSpec * SampleSpecPtr;

#define SMP_UNDEFINED       0
#define SMP_SIGNED_8BIT     1
#define SMP_UNSIGNED_8BIT   2
#define SMP_SIGNED_16BIT    3
#define SMP_UNSIGNED_16BIT  4
#define SMP_COPY            5

#define SMP_LSB_ORDER       0
#define SMP_MSB_ORDER     128

#define LOOP_MODE           1
#define BIDI_MODE	    2

#endif
