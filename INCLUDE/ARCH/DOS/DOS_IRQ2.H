/****************************************************************************
    DOS_IRQ - object oriented IRQ manager class + TimerClass
    ** this time screen syncron ! **
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 30.10.1996
****************************************************************************/
//
// -- <!! ATTENTION !!> --
//    multible timers are currently not supported
// -- <               > --

#ifndef __DOS_IRQ_H__
#define __DOS_IRQ_H__

#include "mtypes.h"
#include "player.h"

#ifdef __GNUC__
  #define __GO32_IRQs__
  #include <go32.h>
  #include <dpmi.h>
  #include <stubinfo.h>
#endif

typedef void * UserData;
typedef void (*UserFunc)(UserData);
typedef void (*OldVec)(void);
typedef int TimerID;

class IRQ_ManagerClass {
    public:
	IRQ_ManagerClass();
	~IRQ_ManagerClass();
	Status ReserveIRQ(int irqno,UserFunc,UserData);
	TimerID ReserveTimer(int microsec,UserFunc,UserData,int multible);
	Status ReprogramTimer(TimerID,int microsec); // only if !multible (special TimerID!)
	Status ReprogramTimerCnt(TimerID,WORD counter); // only if !multible (special TimerID!)
	Status FreeIRQ(int irqno);
	Status FreeTimer(TimerID);
	void call(int not); // internally used
    private:
	inline int enable_irq(int);
	inline void disable_irq(int);
	inline int is_enabled(int);
	int multible_timer;
	struct IrqDataStruct
	      { int used; 
		UserFunc usrfunc;
		UserData usrdata;
		int oldmask;
		int wasmasked; // flag if old IRQ was masked out
		#ifdef __GO32_IRQs__
		  _go32_dpmi_seginfo oldvec,newvec;
		#else
		  OldVec oldvector;
		#endif
	      } IRQ[16]; // timer (IRQ0) is not stored here !
	struct TimerDataStruct
	      { int timing;   // number of ticks for next call
		int counter;  // ticks left to next call
		TimerDataStruct *next;
		UserFunc usrfunc;
		UserData usrdata;
	      } * Timer; // in the beginning there's only system vector or
			 // the timeridle() registered.
};

extern IRQ_ManagerClass IRQ_Manager;

#endif
