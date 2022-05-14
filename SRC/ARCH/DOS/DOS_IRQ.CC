/****************************************************************************
    DOS_IRQ - object oriented IRQ manager class + TimerClass
              DOS protected & realmode!
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 06.11.1996
****************************************************************************/
//
// -- <!! ATTENTION !!> --
//    multible timers are currently not supported !
//    realmode code not yet written !
// -- <               > --

#include "arch/dos/dos_irq.h"
#include "mtypes.h"
#include <dos.h>
#include <stddef.h>
#include <stdio.h>

#define ALL_TIMERS   0xFFFFFFFF
#define SINGLE_TIMER 0x81818181

/* interrupt controller 1 */
#define CNTRL_8259              0x21
#define OCR_8259                0x20
#define EOI                     0x20
#define REARM3                  0x2F3
#define REARM5                  0x2F5

/* interrupt controller 2 */
#define CNTRL_M_8259		0x21
#define CNTRL_M2_8259  		0xA1
#define OCR_2_8259              0xA0

IRQ_ManagerClass IRQ_Manager; // Only one instance of this class !!!

#ifdef __GO32_IRQs__

#define HardwareCall(no) \
  void hardwarecall##no () \
  { outportb(0x20,0x20); \
    IRQ_Manager.call(no);  \
  }

#define ExtHardwareCall(no) \
  void hardwarecall##no () \
  { outportb(0xA0,0x20); \
    outportb(0x20,0x20); \
    IRQ_Manager.call(no);  \
  }

#define HC(no) HardwareCall(no)
#define EHC(no) ExtHardwareCall(no)

HC(0) HC(1) HC(2) HC(3) HC(4) HC(5) HC(6) HC(7)
EHC(8) EHC(9) EHC(10) EHC(11) EHC(12) EHC(13) EHC(14) EHC(15)

#define NHC(no) hardwarecall##no

const void (*(irqfunctions[16]))(void) =
     { NHC(0),NHC(1),NHC(2),NHC(3),NHC(4),NHC(5),NHC(6),NHC(7),
       NHC(8),NHC(9),NHC(10),NHC(11),NHC(12),NHC(13),NHC(14),NHC(15) };

#define IRQ2INT(irq)     (irq&8)?(irq+0x68):(irq+8)

inline void IRQ_ManagerClass::disable_irq(int irqno)
{ if (!IRQ[irqno].used) return;
  disable();
  int intno=IRQ2INT(irqno);
  if (irqno<8) outportb(0x21,IRQ[irqno].oldmask);
  else outportb(0xA1,IRQ[irqno].oldmask);
  _go32_dpmi_set_protected_mode_interrupt_vector(intno,&IRQ[irqno].oldvec);
  IRQ[irqno].used=0;
  enable();
}

inline int IRQ_ManagerClass::enable_irq(int irqno)
{
  int intno=IRQ2INT(irqno);
  disable();
  _go32_dpmi_get_protected_mode_interrupt_vector(intno,&(IRQ[irqno].oldvec));
  IRQ[irqno].newvec.pm_selector=_my_cs();
  IRQ[irqno].newvec.pm_offset=(long)(*irqfunctions[irqno]);
  _go32_dpmi_allocate_iret_wrapper(&(IRQ[irqno].newvec));
  _go32_dpmi_set_protected_mode_interrupt_vector(intno, &(IRQ[irqno].newvec));
  if (irqno<8) outportb(0x21,(IRQ[irqno].oldmask=inportb(0x21))&~(1<<irqno));
     else outportb(0xA1,(IRQ[irqno].oldmask=inportb(0xA1))&~(1<<(irqno-8)));
  IRQ[irqno].used=1;
  enable();
  return IRQ[irqno].oldmask;
}

inline int IRQ_ManagerClass::is_enabled(int no)
{ int val;
  if (no<8) val=(inportb(0x21)&(1<<no))==0;
  else val=(inportb(0xA1)&(1<<(no-8)))==0;
  return val;
}
#else
// realmode
#error(test)
typedef unsigned int WORD;
typedef unsigned char BYTE;
#endif

IRQ_ManagerClass::IRQ_ManagerClass()
{ // read old IRQ vectors:
  int i;
  for (i=0;i<16;i++)
  { IRQ[i].used=0;
    IRQ[i].wasmasked=!is_enabled(i);
  }
}

IRQ_ManagerClass::~IRQ_ManagerClass()
{ // restore all old IRQ vectors
  int i;
  for (i=0;i<16;i++)
    if (IRQ[i].used) FreeIRQ(i);
  if (Timer!=NULL) FreeTimer(ALL_TIMERS);
}

Status IRQ_ManagerClass::ReserveIRQ(int irqno,UserFunc usrfct,UserData usrdata)
{ // set new IRQ-handler and user function/data
  if (IRQ[irqno].used) ;// remove old!!
  disable_irq(irqno);
  IRQ[irqno].usrdata=usrdata;
  IRQ[irqno].usrfunc=usrfct;
  enable_irq(irqno);
}

TimerID IRQ_ManagerClass::ReserveTimer(int microsec,UserFunc usrfunc,UserData usrdata,int multible)
{ // set new timer - 'multible=1' is not supported yet
  if (multible==1) { printf("Multible timer aree not supported !");exit(0); }
  // forget installed timers...
  disable_irq(0); // no more IRQ calls while modifying
  Timer = new TimerDataStruct;
  Timer->usrfunc=usrfunc;Timer->usrdata=usrdata;
  enable_irq(0);

  // timing: 1193181/microsec*1000
  WORD timercounter=1;if (microsec) timercounter=(WORD)(1193181000.0/microsec);
  outportb(0x40,char(timercounter&0xff));
  outportb(0x40,char(timercounter>>8));
  outportb(0x43,0);

  return SINGLE_TIMER;
}

Status IRQ_ManagerClass::ReprogramTimer(TimerID id,int nanosec)
{ // currently only single timer reprogramming !
  if (id==SINGLE_TIMER)
  {
    // timing: 1193181/1000000*nanosec
    WORD timercounter=(WORD)(1.193181*nanosec);
    outportb(0x40,(BYTE)(timercounter&0xff));
    outportb(0x40,(BYTE)(timercounter>>8));
    outportb(0x43,0);
  }
}

Status IRQ_ManagerClass::ReprogramTimerCnt(TimerID id,WORD counter)
{ // currently only single timer reprogramming !
  if (id==SINGLE_TIMER)
  {
    outportb(0x40,(BYTE)(counter&0xff));
    outportb(0x40,(BYTE)(counter>>8));
    outportb(0x43,0);
  }
}

void IRQ_ManagerClass::call(int i)
{ // is called by all interrupts...
  if (i==0) // the timer...
  { // not fully implemented:
    Timer->usrfunc(Timer->usrdata);
  } else
  if (IRQ[i].used)
  { // normal IRQs
    IRQ[i].usrfunc(IRQ[i].usrdata);
  }
}


Status IRQ_ManagerClass::FreeIRQ(int irqno)
{ // remove IRQ-handler
  if (IRQ[irqno].used) disable_irq(irqno);
}

Status IRQ_ManagerClass::FreeTimer(TimerID timerid)
{ // remove Timer
  if (timerid==ALL_TIMERS) ; // all ...
  if (IRQ[0].used) disable_irq(0);
  outportb(0x40,0);
  outportb(0x40,0);
  outportb(0x43,0);
  // free timer struct:
  delete Timer; // only single timer supported !
}

/*
	int multible_timer;
	struct IrqDataStruct
	      { int used; 
		UserFunc usrfunc;
		UserData usrdata;
		OldVec oldvector;
	      } IRQ[16]; // timer (IRQ0) is not stored here !
	struct TimerDataStruct
	      { int timing;   // number of ticks for next call (multi)
		int counter;  // ticks left to next call       (multi)
		TimerDataStruct *next;
		UserFunc usrfunc;
		UserData usrdata;
	      } * Timer; // in the beginning there's only system vector or
			 // the timeridle() registered.
*/
