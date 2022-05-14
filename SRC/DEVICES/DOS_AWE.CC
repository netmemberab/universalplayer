/****************************************************************************
   SoundBlaster AWE 32 - THE wavetable card
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 16.07.1996
  rewritten: 14.10.1996 (due code clean up)
  reassigned for new player interface: 29.10.1996
****************************************************************************/
// NOTE: this driver is usable for only one active AWE - sorry but I tried to
//       get it working FAST !

// BUGs: - volume/panning fade not yet - might click
//       - autodetection of AWE32 port is wrong (does also detect on SB16).
//       - panning not implemented -> ?? error ??

// TODO:
//       - click removale to simple! write better interface for this ->
//         GetFreeChannel(),ChannelState(chn,state) ...states: off/fading out/on
//       - and put the channel-at-sampleend-check into GetFreeChannel()
//       - add unrolling ping-pong-loops   

#include "player.h"
#include "module.h"
#include "devices/dos_awe.h"
#include "arch/dos/dos_irq.h"
#include "instrmts/sample.h"

#include <string.h>
#include <dos.h>
#include <math.h>

#ifdef __GNUC__
#include <pc.h>
#define inp(a)  inportb(a)
#define inpw(a)  inportw(a)
#define outp(a,b) outportb(a,b)
#define outpw(a,b) outportw(a,b)
#endif

// hack stylish, I don't really like these TMPs
WORD TMP_AWE32_Base;
WORD TMP_EMU8000_Base;

// AWE port names:
#define DATA0   (TMP_EMU8000_Base)
#define DATA1   (TMP_EMU8000_Base+0x400)
#define DATA2   (TMP_EMU8000_Base+0x402)
#define DATA3   (TMP_EMU8000_Base+0x800)
#define Pointer (TMP_EMU8000_Base+0x802)
// AWE register names:
#define CPF(chn)     0,chn,DATA0   /* current pitch and fractional address */
#define PTRX(chn)    1,chn,DATA0   /* pitch target, Rvb send, AUX byte     */
#define CVCF(chn)    2,chn,DATA0   /* current volume and filter cutoff     */
#define VTFT(chn)    3,chn,DATA0   /* volume and filter cutoff tagets      */
#define PSST(chn)    6,chn,DATA0   /* Pan send and loop start address      */
#define CLS(chn)     7,chn,DATA0   /* chorus send and loop end address     */
#define CCCA(chn)    0,chn,DATA1   /* Q-ControlBits and current address    */
#define HWCF4        1,9,DATA1     /* configuration dw 4		   */
#define HWCF5        1,10,DATA1    /* configuration dw 5		   */
#define HWCF6        1,13,DATA1    /* configuration dw 6		   */
#define SMALR        1,20,DATA1    /* sound memory address for "left" SMR  */
#define SMARR        1,21,DATA1    /* sound memory address for "right" SMR */
#define SMALW        1,22,DATA1    /* sound memory address for "left" SMW  */
#define SMARW        1,23,DATA1    /* sound memory address for "right" SMW */
#define SMLD         1,26,DATA1    /* sound memory "left" data             */
#define SMRD         1,26,DATA2    /* sound memory "right" data            */
#define WC           1,27,DATA2    /* sample counter                       */
#define HWCF1        1,29,DATA1    /* Configuration word 1                 */
#define HWCF2        1,30,DATA1    /* Configuration word 2                 */
#define HWCF3        1,31,DATA1    /* Configuration word 3                 */
#define INIT1        2,0,DATA1     /* initialization array 1               */
#define INIT2        2,0,DATA2     /* initialization array 2               */
#define INIT3        3,0,DATA1     /* initialization array 3               */
#define INIT4        3,0,DATA2     /* initialization array 4               */
#define ENVVOL(chn)  4,chn,DATA1   /* Volume envelope delay		   */
#define DCYSUSV(chn) 5,chn,DATA1   /* Volume env sustain & decay           */
#define ENVVAL(chn)  6,chn,DATA1   /* Modulation envelope delay            */
#define DCYSUS(chn)  7,chn,DATA1   /* Modulation env sustain & decay       */
#define ATKHLDV(chn) 4,chn,DATA2   /* Volume env hold & attack		   */
#define LFO1VAL(chn) 5,chn,DATA2   /* LFO #1 delay			   */
#define ATKHLD(chn)  6,chn,DATA2   /* modulation env hold & attack         */
#define LFO2VAL(chn) 7,chn,DATA2   /* LFO #2 delay			   */
#define IP(chn)      0,chn,DATA3   /* initial pitch			   */
#define IFATN(chn)   1,chn,DATA3   /* initial filter cutoff & attentuation */
#define PEFE(chn)    2,chn,DATA3   /* pitch & filter cutoff heights        */
#define FMMOD(chn)   3,chn,DATA3   /* vibrato & filter modulation from LFO #1 */
#define TREMFRQ(chn) 4,chn,DATA3   /* LFO #1 Tremolo amount & frequency    */
#define FM2FRQ2(chn) 5,chn,DATA3   /* LFO #2 vibrato amount and frequency  */

struct SBAWE_HardwareOptionStruct default_opts = { 0x220, 0x620, AWE32_DRAM_START , DETECT_AWE32_MEM };

volatile static BYTE playing=0;
volatile static DWORD timercounter; // used for timing sbawe->newnotes() calls
BYTE overal_volumes_read;  // flag if current overal volume table is allready read

/* 
    Prototypes for hardware accessing functions: 
*/
void  AWE32Init(void);
WORD  AWE32Detect(WORD&,WORD&);
void  EnableDRAMtransfer(void);
void  DisableDRAMtransfer(void);
DWORD CheckDRAM(void);

/*
    AWE32-register acess routines
*/
static inline void write_AWE32_Wreg (int Register,int Channel,int DataPort, WORD  Data);
static inline void write_AWE32_Wreg (int Command,int DataPort,WORD Data);
static inline void write_AWE32_DWreg(int Register,int Channel,int DataPort, DWORD Data);
static inline WORD  read_AWE32_Wreg (int Register,int Channel,int DataPort);
static inline DWORD read_AWE32_DWreg(int Register,int Channel,int DataPort);
static inline void  write_AWE32_W(int DataPort,WORD Data);

void AWE_play_irq(UserData);   // timer for playing

SBAWEclass::SBAWEclass(DeviceOptionsPtr opts):DeviceClass()
{ // -------- --- --  -
  // initialize AWE (hardware options in 'opts'
  // --------  --  -
  disable();
  if (AWE32Detect(sbawe32_base,emu8000_base)==0) // detection failed ?
    { status=DEVICE_FAILED; enable();return; }
  TMP_AWE32_Base=sbawe32_base;
  TMP_EMU8000_Base=emu8000_base;
  memset(sample,0,sizeof(sample));
  enable();
  #ifdef __DEBUGGING__
    printf("detect ok. (Card: 0x%3X, EMU8000: 0x%3X)\n",sbawe32_base,emu8000_base);
    getchar();
  #endif
  AWE32Init();
  trace("init ok.\n");
  DRAM_size=CheckDRAM(); // dramsize in words !
  trace("Trace DRAM ok.\n");
  DRAM_free=0x200000; // start of dram (first 4MB are ROM !)
  // init voltable:
  VolTable[0] = 127;
  for(int k=1; k<65; k++)
    VolTable[k] = (BYTE)(127 - log(k)*26.6899);

  EnableDRAMtransfer();  // setup all oscilators for transfer
  DRAM_transfer=1;
  // dram transfer will be enabled until startplay() !!!
  trace("end constructor.\n");
}

SBAWEclass::~SBAWEclass(void)
{ //trace("ENT: AWE::~\n");
  Stop();
  IRQ_Manager.FreeTimer(timerid);
}

void SBAWEclass::SetupChannel(BYTE chn,ChannelStruct&)
{
}

void SBAWEclass::SetupChannels(ChannelSettings&)
{
}

struct AWE32_IRQ_data { 
		SBAWEclass * sbawe;
		ModuleClass * Module;
};

// ------ -- -
// program hardware for playing - set timer (playing starts emmediatly)
//
static int timerflag;

static inline void setborder(BYTE color)
{ outp(0x3c0,0x31);
  outp(0x3c0,color);
}

static void waitraster()
{ while (!(inp(0x3da)&0x08));
  while (inp(0x3da)&0x08);
}

Status SBAWEclass::Start(ModuleClass * Module)
{ int i;
  Module->StartModule();
  #ifdef __DEBUGGING__
    printf("start AWE\n");getchar();
  #endif
  DisableDRAMtransfer();
  DRAM_transfer=0;
  memset(&channel,0,sizeof(channel));
  //for (i=0;i<32;i++) channel[i].AWEchn=0xff;
  memset(&AWE_channel,0,sizeof(AWE_channel));
  timercounter=1;		// call newnotes() in next timer irq
  playing=1;			// activate timer now for playing
  trace("variables initialized.\nstart\n");
  // program timer:
  timerflag=0;
  { AWE32_IRQ_data * data = new AWE32_IRQ_data;
    data->sbawe=this;data->Module=Module;
    timerid=IRQ_Manager.ReserveTimer(1000,AWE_play_irq,data,0);
    // single timer (do not a multi timer - we need to reprogram it !)
  }
  do {
    if (timerflag)
      { timerflag=0;
        setborder(4);
	newnotes(Module);
        setborder(0);
      }
  }
  while (1);
  trace("END.    AWE::Start\n");
  return NO_ERROR;
}

Status SBAWEclass::Stop(void)
{ playing=0; // no more counting in IRQ
  // trace("ENT: AWE::stop\n");
  for (int i=0;i<31;i++)
    NoteOff(i);  // all channels off
}

Status SBAWEclass::Continue()
{ playing=1;
}

/* ------ =================================================== ------
   #####  Instrument handling: allocating, uploading, freeing  #####
   ------ =================================================== ------
*/
// --------------------------------------------------------------------
// Instrument allocation (reserve memory for samples) is done only
// in a logical way - SBAWEclass stores all free and reserved memory
// locations in AWE32-DRAM, Alloc does change this data.
// ....no restrictions for call situation....
// --------------------------------------------------------------------
ResourceID SBAWEclass::AllocateInstruments(InstrumentIDs ins,int noi)
{ int i=0,j;
  ResourceID rid;
  InstrumentID iid;
  if (noi>MAX_SAMPLES_PER_RSC) { status=OUT_OF_RANGE;return NO_RESOURCE; }
  while (i<MAX_RESOURCES && sample[i]!=NULL) i++;
  if (i==MAX_RESOURCES) { status=NO_RESOURCES;return NO_RESOURCE; }
  sample[i]= new SampleResourceStruct;
  if (sample[i]==NULL) { status=NO_RESOURCES;return NO_RESOURCE; }
  (sample[i])->no_samples=0;
  if (((sample[i])->smpinfo=new SampleStruct[noi])==NULL) { status=NO_MEMORY;return NO_MEMORY; }
  (sample[i])->no_samples=noi;
  if ((sample[i])->smpinfo==NULL) { status=NO_RESOURCES;return NO_RESOURCE; }
  rid=(((ResourceID)this)&~0xffU)+i;
  // generate IDs
    // Idea:
    //    resource= (id>>12)&0xff
    //    sampleno=  id&0x3fff
  iid=(((InstrumentID)this)&0xff000000)+((DWORD)i<<12);
  trace("Device reserves:\n");
  for (j=0;j<noi;j++) 
  { *ins++=iid+j;
    #ifdef __DEBUGGING__
      printf("%li:",*(ins-1));
    #endif
  }
  return rid;
}

// --------------------------------------------------------------------
// Free is only a logical (not done by hardware) operation, it does
// change internal SBAWEclass data structurs - all Instruments were
// stored in this resource are not accessible after freeing.
// ....no restrictions for call situation....
// --------------------------------------------------------------------
Status SBAWEclass::FreeInstruments(ResourceID id)
{ int i=id&0xff;
  int j;
  if (sample[i]==NULL) return BAD_ID;
  if (j=(sample[i])->no_samples) delete ((sample[i])->smpinfo);
  delete (sample[i]);
  sample[i]=NULL;
  return NO_ERROR;
}

// --------------------------------------------------------------------
// Uploading does write the sample data into the AWE-DRAM, this is only
// possible until if no other module is playing ! Just stop playing or
// do it short after initializing the SBAWEclass
// ....don't call while playing (it does change the hardware state)....
// --------------------------------------------------------------------
Status SBAWEclass::UploadInstrument(InstrumentID Iid,InstrumentLoaderClass& loader)
{ int rscno=(Iid>>12)&0xff;
  Status st;
  if (sample[rscno]==NULL || (Iid&0x3fff)>sample[rscno]->no_samples) return BAD_ID;  
  trace("upload id ok\n");
  SampleStruct * smpptr= &sample[rscno]->smpinfo[ Iid & 0x3fff ];
  // set output format ...
  SampleType ss=SMP_UNSIGNED_16BIT;
  if ((st=loader.SetOutputFormat(IT_SAMPLE,&ss))!=NO_ERROR) 
    return st; // instrument cannot be converted ...

  // get sample specs ...
  SampleSpec sp;
  loader.GetInstrumentSpecs(Iid,(UserSpecPtr)&sp);
  /* smpptr->startaddr=0;smpptr->sampleend=sp.size;
     smpptr->loopend=sp.loopend;smpptr->loopstart=sp.loopstart;
  */
  #ifdef __DEBUGGING__
  printf("Sample: (%li) %li-%li\n",sp.size,sp.loopstart,sp.loopend);
  #endif
  smpptr->id=Iid;
  // do hardware upload
  st=LoadInstrument(Iid,smpptr,loader,sp.size,sp.loopflag,sp.loopstart,sp.loopend);
  return st;
}

// flags for hardware changes (to set them only once per call !)
BYTE change_volume;
BYTE change_panning; 
BYTE change_offset;
DWORD write_addr[32];

void   SetPanning(BYTE AWEchn,BYTE panpos);
static inline DWORD getSamplePos(BYTE AWEchn);
void   SetSamplePos(DWORD DRAMpos);

/* ------ =================================================== ------
     newnotes reads the 'simple hardware instructions' and
     does the programming job of the AWE32-hardware
     (it uses several private members for hardware changes)
   ------ =================================================== ------
*/

void  SBAWEclass::newnotes(ModuleClass * Module)
{ int i;
  DeviceCommandsStruct devcmd;
  memset(&devcmd,0,sizeof(devcmd));
  // read next notes from module
  memset(&write_addr,0,sizeof(write_addr));

  // free all channels former 'noteoff' channels now
  for (i=0;i<30;i++)
    if (AWE_channel[i]==2) AWE_channel[i]=0;

  devcmd.timerbase=1193181; // timer base in Hz
  Module->GetDeviceCommands(devcmd);

  DWORD counter=devcmd.countervalue;
  if (counter>65535) { printf("AWE support error...big timer val...\n"); exit(1); }
  IRQ_Manager.ReprogramTimerCnt(timerid,counter);

  //disable(); // disable maskable interrupts - we do changes to AWE hardware
  overal_volumes_read=0;
  // read devcmd data and program the AWE for the requests:
  for (i=0;i<devcmd.no_commands;i++)
  { SampleStruct * smpptr= &(sample[(channel[i].instr>>12)&0xff]->smpinfo[channel[i].instr&0x3fff]);

    change_volume=0;
    change_offset=0;
    change_panning=0;

    // for debugging:
    if (devcmd.Commands[i].Changes) channel[i].lastchanges=devcmd.Commands[i].Changes;

    if (channel[i].active)
    { channel[i].cur_addr=getSamplePos(channel[i].AWEchn);
      if (channel[i].instr &&
          channel[i].cur_addr> (smpptr->sampleend) )
          NoteOff(i); // create an hard stop for this case ?
    }

    if (devcmd.Commands[i].Changes&CHG_PANNING) // panposition does change
    { channel[i].panpos=devcmd.Commands[i].Panning;
      change_panning=1;
    }

    if (devcmd.Commands[i].Changes&CHG_STOPPLAY)
    { channel[i].offset=getSamplePos(channel[i].AWEchn)- (smpptr->startaddr);
      NoteOff(i);
    }

    if (devcmd.Commands[i].Changes&CHG_INSTRUMENT)
    { InstrumentID &iid=devcmd.Commands[i].instrid;
      if ((iid&0x3fff)!=NO_INSTRID)
      { channel[i].instr = iid;
  	smpptr= &(sample[(iid>>12)&0xff]->smpinfo[iid&0x3fff]);
        channel[i].volume=64;
        channel[i].offset=0;
      } else devcmd.Commands[i].Changes&=~CHG_INSTRUMENT;
    }

    if (devcmd.Commands[i].Changes&CHG_VOLUME)
    { unsigned char v=devcmd.Commands[i].Volume;if (v>64) v=64;
      channel[i].volume=v;
      change_volume=1;
    }

    if (devcmd.Commands[i].Changes&CHG_SETOFFSET) // set offset
    { channel[i].offset=devcmd.Commands[i].Offset;
      change_offset=1;
    }

    setborder(7);
    if (devcmd.Commands[i].Changes&CHG_FREQUENCY)
    { channel[i].frequency=devcmd.Commands[i].Frequency;
      channel[i].AWEpitchreg=(WORD)(log(channel[i].frequency)*5909.27 - 5850.98);
      if (devcmd.Commands[i].Changes&CHG_INSTRUMENT)
	PlayNote(i);
      else // only pitch change:
	write_AWE32_Wreg( IP(channel[i].AWEchn) , channel[i].AWEpitchreg); // initial pitch
    }

    if (devcmd.Commands[i].Changes&CHG_RESTART && !channel[i].active &&
	channel[i].AWEpitchreg && channel[i].instr)
      PlayNote(i); // restart it

    if (devcmd.Commands[i].Changes&CHG_CONTINUE)  
    { // channel[i].offset was set in CHG_STOPPLAY to last sampleposition
      PlayNote(i);
    }

    if (change_volume)
    { BYTE AWE32Vol = VolTable[channel[i].volume];
      write_AWE32_Wreg( IFATN(channel[i].AWEchn) ,0xFF00 | AWE32Vol ); // initial filter, attentuation
    }

    if (change_panning)
    { DWORD Temp = channel[i].panpos;
      Temp <<= 24L;
      // big panning moving might cause clicks, smaller steps can avoid this...
      write_AWE32_DWreg(PSST(channel[i].AWEchn),Temp | smpptr->loopstart-1);
    }

    if (change_offset)
      PlayNote(i);

  }
  //enable();
}

/* ------ =================================================== ------
           private members for secondary hardware access
   ------ =================================================== ------
*/

/*
   currently only a simple click removale routine: no note settings after a
   noteoff for a while (very speed depended !! but seems to work fine)
     - this routine was ment to chose the best channel == non active channel with
       the lowest volume
   --> check for idle channels (look below)
   --> check volume targets==0 for current volume and use the one with lowest
*/
BYTE  SBAWEclass::GetNextChannel(BYTE current)
{
  WORD min=0xffff;
  BYTE no=32,i;
  for (i=0;i<30;i++)
    if (!AWE_channel[i]) { no=i;break; }
  if (no!=32) return(no); else return(current);

  /* BTW) manual saids: channel is idle if:
        1) DCYSUSV has been set to 0x0080
	2) VTFT & CVCF have been zeroed
	3) PTRX & CPF have been zeroed
  */
}

void Read_overal_volumes(WORD *channels)
{ BYTE i;
  WORD w;
  for (i=0;i<30;i++)
    *channels++=(read_AWE32_DWreg(VTFT(i))>>16); // read volume targets
}

/*
    Start playing a sample in channel "i"
*/
void  SBAWEclass::PlayNote(BYTE i)
{ BYTE AWEchn;
  WORD w;
  SampleStruct * smpptr= &(sample[(channel[i].instr>>12)&0xff]->smpinfo[channel[i].instr&0x3fff]);
  //TMP_AWE32_Base=sbawe32_base;
  //TMP_EMU8000_Base=emu8000_base;

  /* Read Hardware channel volumes: .... take a look at GetNextChannel()
    if (!overal_volumes_read)
    { Read_overal_volumes((WORD*)&overal_volumes);
      overal_volumes_read=1;
    }
  */
  AWEchn=GetNextChannel(channel[i].AWEchn);

  if (channel[i].AWEchn!=AWEchn)
  { if (channel[i].active) NoteOff(i);
    channel[i].AWEchn=AWEchn;
  }
  channel[i].active=1;
  AWE_channel[AWEchn]=1;

  change_volume=0;
  change_panning=0;
  change_offset=0;

  // first set envelope engine parameters:
  write_AWE32_Wreg (ENVVOL(AWEchn) ,0x8000);  // volume envelope delay
  /*
    write_AWE32_Wreg (ENVVAL(AWEchn) ,0x8000);  // ENVVAL: modulation envelope delay
  */
  write_AWE32_Wreg (DCYSUSV(AWEchn),0x7F7F);  // DCYSUSV: volume envelope sustain & decay
  write_AWE32_Wreg (ATKHLDV(AWEchn),0x7F7F);  // ATKHLDV: volume envelope hold & attack
  /*
    write_AWE32_Wreg (LFO1VAL(AWEchn),0x8000);  // LFO1VAL: LFO #1 delay
    write_AWE32_Wreg (ATKHLD(AWEchn) ,0x7F7F);  // ATKHLD: modulation envelope hold & attack
    write_AWE32_Wreg (LFO2VAL(AWEchn),0x8000);  // LFO2VAL: LFO #2 delay
  */
  write_AWE32_DWreg(IP(AWEchn),channel[i].AWEpitchreg);  // IP: initial pitch
  write_AWE32_Wreg (IFATN(AWEchn),0xFF00 | VolTable[channel[i].volume]); // IFATN: initial filter cutoff and atten.
  /*
    write_AWE32_Wreg (PEFE(AWEchn),0x0000);  // PEFE: pitch and filter heights
    write_AWE32_Wreg (FMMOD(AWEchn),0x0000);  // FMMOD: vibrato and filter modulation
    write_AWE32_Wreg (TREMFRQ(AWEchn),0x0010);  // TREMFRQ: LFO #1 tremolo amount, frequency
    write_AWE32_Wreg (FM2FRQ2(AWEchn),0x0010);  // FM2FRQ2: LFO #2 vibrato amount, frequency
  */

  // set loop start adress, loop end adress and  start adress (one less of all)
  long Temp = channel[i].panpos;Temp <<= 24L;
  write_AWE32_DWreg(PSST(AWEchn),Temp | smpptr->loopstart-1);
  #define CHORUS 0
  Temp = CHORUS<<24L;
  write_AWE32_DWreg(CLS(AWEchn),Temp | smpptr->loopend-1); // CLS: chorus send, loop end adress
  write_addr[AWEchn]= smpptr->startaddr + channel[i].offset-1;
  write_AWE32_DWreg(CCCA(AWEchn),write_addr[AWEchn]);	 // CCCA: Q,control, current address

  // the following settings emmediatly
  //write_AWE32_DWreg(3,AWEchn,DATA0,0x0000FFFF); // VTFT: volume, filter cut targets
  write_AWE32_DWreg(2,AWEchn,DATA0,0x0000FFFF);	// CVCF: current volume, filter cutoff (manual!!)
  //write_AWE32_Wreg (7,AWEchn,DATA1,0x7F7F);	// DCYSUSV: modulation env sustain, decay
  write_AWE32_DWreg(1,AWEchn,DATA0,0x40000000); // PTRX: pitch target, rvb send, aux
  write_AWE32_DWreg(0,AWEchn,DATA0,0x40000000);	// CPF: current pitch, fractional addr
  // the note has now begun

  /* useless since there are no Changes !
    AWE32RegW(0xA0+AWEchn,0xA22,0x8000);  	// LFO1 delay
    AWE32RegW(0xE0+AWEchn,0xA22,0x8000);  	// LFO2 delay
    AWE32RegW(0x40+AWEchn,0xE20,0x0000);       // envelope 1 to pitch & filter
    AWE32RegW(0x60+AWEchn,0xE20,0x0000);       // LFO1 to pitch & filter
    AWE32RegW(0x80+AWEchn,0xE20,0x007F);       // LFO1 frequnecy & LFO1 to volume
    AWE32RegW(0x80+AWEchn,0xE20,0x0000);       // LFO1 frequnecy & LFO1 to volume
    AWE32RegW(0xA0+AWEchn,0xE20,0x0000);  
    AWE32RegDW(0x20+AWEchn,0x620,0);     	// reverb+unknown
    AWE32RegW(0xA0+AWEchn,0xA20,0x7F7F);  // envelope 2 sustain & decay (ramp to full vol)
  */
}

void  SBAWEclass::NoteOff(BYTE i)
{ BYTE AWEchn=channel[i].AWEchn;
  // no emmediate cutting - this will cause clicks !
  write_AWE32_Wreg( DCYSUSV(AWEchn) , 0x8075); // very fast ramp down
  //write_AWE32_Wreg( DCYSUS(AWEchn) , 0x8075);  // very fast ramp to modulation goal
  channel[i].active=0;
  if (AWE_channel[AWEchn]) AWE_channel[AWEchn]=2; // mark as sliding down, next newnotes() call it's free
}

static inline DWORD getSamplePos(BYTE AWEchn)
{
  return( read_AWE32_DWreg(CCCA(AWEchn))/*&0x0ffffff*/ );
  // fractional adress is stored in CPF(AWEchn) & 0xffff
}

/*
   optimize this for parallel writes of left & right DMA accesses !
*/
Status SBAWEclass::LoadInstrument(InstrumentID id,SampleStruct * smpptr,InstrumentLoaderClass& loader,DWORD size,int loopflag,DWORD loopbeg,DWORD loopend)
{ // sample uploading: 
  if (DRAM_transfer) { EnableDRAMtransfer(); DRAM_transfer=1; }
  // 
  // code includes unrolling very small loops (<128samples), they sound
  // horrible on AWE at high frequencies ! 
  // - additional code for unclicking loops (some extra bytes at sample end)
  //
  WORD SampleBuffer[16*1024U];
  DWORD len,i,ofs;
  SWORD unroll_loops=0;
  DWORD looplen=0;

  DWORD foo;

  if (DRAM_free+size>0x200000L+DRAM_size) 
  { trace("Not enough memory on your AWE.\n");return(NO_MEMORY); }

  disable();
  write_AWE32_DWreg( SMALW , DRAM_free ); // start adress for left writin'

  smpptr->startaddr=DRAM_free;
  if (loopflag!=0 && loopbeg<loopend)
  { // remove horrible small loopings
    smpptr->realloopend=DRAM_free+loopend+5;
    if ((looplen=loopend-loopbeg)<128) // a small loop
      unroll_loops=1+128/looplen;
    else unroll_loops=1;
    
    smpptr->loopstart=DRAM_free+loopbeg;
    smpptr->loopend=DRAM_free+loopend+(unroll_loops-1)*looplen;
    smpptr->sampleend=DRAM_free+size+(unroll_loops-1)*looplen+5;
    DRAM_free+=size+(unroll_loops-1)*looplen+1; // 1 extra bytes for unclick loop
    #ifdef __DEBUGGING__
    printf("have to write: %li\n",size+(unroll_loops-1)*looplen+1);foo=0;
    #endif
  } else // non looping sample:
  { smpptr->loopstart=DRAM_free+size+1;
    smpptr->loopend=DRAM_free+size+10;
    smpptr->sampleend=DRAM_free+size;
    unroll_loops=0;
    loopflag=0;
    DRAM_free+=size+16;
    #ifdef __DEBUGGING__
    printf("have to write: %li\n",size+16);foo=0;
    #endif
  }
  ofs=0;
  while (size>0)
  { len=(size>16*1024U)?16*1024U:size;
    if ((unroll_loops>1) && (ofs+len>loopbeg)) len=loopbeg-ofs;
    if (len==0) len=looplen;

    #ifdef __DEBUGGING__
    printf("ID: %li. load %lu at %lu (%lu,%lu)\n",id,len,smpptr->startaddr,loopbeg,loopend);
    #endif
    if (loader.Load(id,(DataPtr*)&SampleBuffer,2*len))
      { trace("error reading sample.\n");return(ERRORLOADING); }

    if (unroll_loops && ofs==loopbeg)
    { trace("urol\n");
      write_AWE32_Wreg( SMLD , SampleBuffer[0]-0x8000); // write converted sample to AWE memory
      foo++;
      i=1;
      while (unroll_loops)
      { while (i<len)
	{ write_AWE32_W ( DATA1,SampleBuffer[i]-0x8000); // upload data to AWE32 DRAM
	  i++;
	} 
	foo+=len;
	i=0;unroll_loops--;
      }
    } else
    {
      write_AWE32_Wreg( SMLD , SampleBuffer[0]-0x8000); // write converted sample to AWE memory
      for(i=1;i<len;i++)
        write_AWE32_W ( DATA1,SampleBuffer[i]-0x8000); 
      foo+=len;
    }
    size-=len;ofs+=len;
  }
  if (loopflag) // add 1 byte after loop to unclick loops
  { write_AWE32_Wreg ( SMLD,SampleBuffer[0]-0x8000); foo++;
  } else // add empty loop at sample end for nonlooping samples
  { write_AWE32_Wreg(SMLD,0);
    for (i=0; i<16; i++)
      write_AWE32_W ( DATA1,0 );
    foo+=16;
  }
  #ifdef __DEBUGGING__
  printf("wrote: %li\n",foo);
  #endif
  DRAM_free+=100;
  enable();
  return NO_ERROR;
}

void AWE_play_irq(UserData ud) // timer for playing
{ timerflag=1;
  /*
  AWE32_IRQ_data * data=(AWE32_IRQ_data*)ud;
  if (!playing) return;
  if (data->sbawe!=NULL) data->sbawe->newnotes(data->Module); 
  */
}

/* ------ =================================================== ------
            C-functions for primary hardware accesses
   ------ =================================================== ------
*/
unsigned int InitialData1_2[] =
         { 0x03FF,0x0030,0x07FF,0x0130,0x0BFF,0x0230,0x0FFF,0x0330,
	   0x13FF,0x0430,0x17FF,0x0530,0x1BFF,0x0630,0x1FFF,0x0730,
	   0x23FF,0x0830,0x27FF,0x0930,0x2BFF,0x0A30,0x2FFF,0x0B30,
	   0x33FF,0x0C30,0x37FF,0x0D30,0x3BFF,0x0E30,0x3FFF,0x0F30,

	   0x43FF,0x0030,0x47FF,0x0130,0x4BFF,0x0230,0x4FFF,0x0330,
	   0x53FF,0x0430,0x57FF,0x0530,0x5BFF,0x0630,0x5FFF,0x0730,
	   0x63FF,0x0830,0x67FF,0x0930,0x6BFF,0x0A30,0x6FFF,0x0B30,
	   0x73FF,0x0C30,0x77FF,0x0D30,0x7BFF,0x0E30,0x7FFF,0x0F30,

	   0x83FF,0x0030,0x87FF,0x0130,0x8BFF,0x0230,0x8FFF,0x0330,
	   0x93FF,0x0430,0x97FF,0x0530,0x9BFF,0x0630,0x9FFF,0x0730,
	   0xA3FF,0x0830,0xA7FF,0x0930,0xABFF,0x0A30,0xAFFF,0x0B30,
	   0xB3FF,0x0C30,0xB7FF,0x0D30,0xBBFF,0x0E30,0xBFFF,0x0F30,

	   0xC3FF,0x0030,0xC7FF,0x0130,0xCBFF,0x0230,0xCFFF,0x0330,
	   0xD3FF,0x0430,0xD7FF,0x0530,0xDBFF,0x0630,0xDFFF,0x0730,
	   0xE3FF,0x0830,0xE7FF,0x0930,0xEBFF,0x0A30,0xEFFF,0x0B30,
	   0xF3FF,0x0C30,0xF7FF,0x0D30,0xFBFF,0x0E30,0xFFFF,0x0F30 };

unsigned int InitialData3_4[] =
         { 0x0C10,0x8470,0x14FE,0xB488,0x167F,0xA470,0x18E7,0x84B5,
	   0x1B6E,0x842A,0x1F1D,0x852A,0x0DA3,0x0F7C,0x167E,0x7254,
	   0x0000,0x842A,0x0001,0x852A,0x18E6,0x0BAA,0x1B6D,0x7234,
	   0x229F,0x8429,0x2746,0x8529,0x1F1C,0x06E7,0x229E,0x7224,

 	   0x0DA4,0x8429,0x2C29,0x8529,0x2745,0x07F6,0x2C28,0x7254,
	   0x383B,0x8428,0x320F,0x8528,0x320E,0x0F02,0x1341,0x7264,
	   0x3EB6,0x8428,0x3EB9,0x8528,0x383A,0x0FA9,0x3EB5,0x7294,
	   0x3EB7,0x8474,0x3EBA,0x8575,0x3EB8,0x44C3,0x3EBB,0x45C3,

	   0x0000,0xA404,0x0001,0xA504,0x141F,0x0671,0x14FD,0x0287,
	   0x3EBC,0xE610,0x3EC8,0x0C7B,0x031A,0x07E6,0x3EC8,0x86F7,
	   0x3EC0,0x821E,0x3EBE,0xD280,0x3EBD,0x021F,0x3ECA,0x0386,
	   0x3EC1,0x0C03,0x3EC9,0x031E,0x3ECA,0x8C4C,0x3EBF,0x0C55,

	   0x3EC9,0xC280,0x3EC4,0xBC84,0x3EC8,0x0EAD,0x3EC8,0xD380,
	   0x3EC2,0x8F7E,0x3ECB,0x0219,0x3ECB,0xD2E6,0x3EC5,0x031F,
	   0x3EC6,0xC380,0x3EC3,0x327F,0x3EC9,0x0265,0x3EC9,0x8319,
	   0x1342,0xD3E6,0x3EC7,0x337F,0x0000,0x8365,0x1420,0x9570 };

struct  { int command;
	 int DataPort;
	} initIndex[] =
         { { 0x40,0xA20 }, { 0x40,0xA22 }, { 0x60,0xA20 }, { 0x60,0xA22 } };

// 
// INLINEs ... easily readable and optimal code!
//

static inline void write_AWE32_Wreg (int Register,int Channel,int DataPort, WORD  Data)
{ outpw(Pointer,(Register<<5)+Channel);
  outpw(DataPort,Data);
}

static inline void write_AWE32_Wreg (int Command,int DataPort,WORD Data)
{ outpw(Pointer,Command);
  outpw(DataPort,Data);
}

static inline void write_AWE32_DWreg(int Register,int Channel,int DataPort, DWORD Data)
{ outpw(Pointer,(Register<<5)+Channel);
  outpw(DataPort,Data);
  outpw(DataPort+2,Data>>16);
}

static inline WORD  read_AWE32_Wreg (int Register,int Channel,int DataPort)
{ outpw(Pointer,(Register<<5)+Channel);
  return inpw(DataPort);
}

static inline DWORD read_AWE32_DWreg(int Register,int Channel,int DataPort)
{ outpw(Pointer,(Register<<5)+Channel);
  WORD w=inpw(DataPort);
  DWORD dw=inpw(DataPort+2);
  return (w|(dw<<16));
}

static inline void AWE32_select_reg(int Register,int Channel)
{ outpw(Pointer,(Register<<5)+Channel);
}

static inline void  write_AWE32_W(int DataPort,WORD Data)
{ outpw(DataPort,Data);
}

// autodetect AWE32 at 220h,240h,260h or 280h
// autodected EMU8000 at 620h,640h,660h or 680h

WORD AWE32Detect(WORD& sbawe_base,WORD& emu8000_base)
{ TMP_AWE32_Base=0x220;
  for(int TMP_EMU8000_Base=0x620;TMP_EMU8000_Base<=0x680;TMP_EMU8000_Base +=0x20)
  { if(read_AWE32_Wreg(7,0,DATA3)&0xC  != 0xC ) continue; // what kind of register ?
    if(read_AWE32_Wreg(1,29,DATA1)&0x58 != 0x58) continue; // HWCF1 ?
    if(read_AWE32_Wreg(1,30,DATA1)&3    != 3   ) continue; // HWCF2 ?
    sbawe_base=TMP_AWE32_Base;
    emu8000_base=TMP_EMU8000_Base;
    return 1; // well done - found
  }
  return 0; // no chance, no emu8000 found
}

void InitEffect(unsigned int* Data)
{ for(int k=0;k<4;++k)
    for(int i=0;i<32;++i)
      write_AWE32_Wreg(initIndex[k].command,initIndex[k].DataPort,*Data++);
}

void InitEffect_2_(unsigned int* Data)
{ for(int k=0;k<4;++k)
    for(int i=0;i<32;++i)
      write_AWE32_Wreg(initIndex[k].command,initIndex[k].DataPort,(*Data++)|(i&1)<<15);
}

/*
 * Wait 'Clocks' number of AWE32 44100Hz ticks
 * -> it's used for timing while initializing
 */

void AWE32Wait(WORD Clocks)
{ WORD k = read_AWE32_Wreg( WC ); // 44100Hz clock
  WORD Time = k;
  while(Clocks > (Time-k))
    Time = read_AWE32_Wreg( WC );
}

/****************************************/
/* Initialization of the AWE32 hardware */
/****************************************/
void AWE32Init(void)
{ int k;
  // initialize hardware configuration
  trace("initialize hardware configuration\n");
  write_AWE32_Wreg( HWCF1 ,0x0059 );
  write_AWE32_Wreg( HWCF2 ,0x0020 );

  // first turn off envelope maschine on ALL channels:
  trace("first turn off envelope maschine on ALL channels:\n");
  for(k=0;k<32;++k) 
     write_AWE32_Wreg( DCYSUSV(k) , 0x0080 ); // turn off
  trace("second zero some registers on ALL channels:\n");
  for(k=0;k<32;++k) 
  {
     write_AWE32_Wreg( ENVVOL(k)  , 0);
     write_AWE32_Wreg( ENVVAL(k)  , 0);
     write_AWE32_Wreg( DCYSUS(k)  , 0);
     write_AWE32_Wreg( ATKHLDV(k) , 0);
     write_AWE32_Wreg( LFO1VAL(k) , 0);
     write_AWE32_Wreg( ATKHLD(k)  , 0);
     write_AWE32_Wreg( LFO2VAL(k) , 0);
     write_AWE32_Wreg( IP(k)      , 0);
     write_AWE32_Wreg( IFATN(k)   , 0);
     write_AWE32_Wreg( PEFE(k)    , 0);
     write_AWE32_Wreg( FMMOD(k)   , 0);
     write_AWE32_Wreg( TREMFRQ(k) , 0);
     write_AWE32_Wreg( FM2FRQ2(k) , 0);
     write_AWE32_DWreg( PTRX(k)    , 0);
     write_AWE32_DWreg( VTFT(k)    , 0);
     write_AWE32_DWreg( PSST(k)    , 0);
     write_AWE32_DWreg( CLS(k)     , 0);
     write_AWE32_DWreg( CCCA(k)    , 0);
  }
  // third write "current" register last
  trace("third write current register last\n");
  disable();
  for(k=0;k<32;++k) 
  { 
     write_AWE32_DWreg( CPF(k)    , 0);
     write_AWE32_DWreg( CVCF(k)   , 0);
  }
  enable();
  
  trace("write SM's\n");
  write_AWE32_DWreg( SMALR  , 0);
  write_AWE32_DWreg( SMARR  , 0);
  write_AWE32_DWreg( SMALW  , 0);
  write_AWE32_DWreg( SMARW  , 0);

  trace("Init effects\n");
  InitEffect(InitialData1_2);      // write Data1
  AWE32Wait(1024);
  InitEffect_2_(InitialData1_2);    // write Data2
  InitEffect_2_(InitialData3_4);   // write Data3
  write_AWE32_DWreg( HWCF4  , 0);
  write_AWE32_DWreg( HWCF5  , 0x00000083);
  write_AWE32_DWreg( HWCF6  , 0x00008000);
  InitEffect(InitialData3_4);       // write Data4
  write_AWE32_Wreg ( HWCF3  , 0x0004); // enable output audio

/*
  // init DRAM refresh ????
  AWE32RegW(0xBE,0xA20,0x80);   
  AWE32RegDW(0xDE,0x620,0xFFFFFFE0);
  AWE32RegDW(0xFE,0x620,0xFFFFE8);
  AWE32RegDW(0x3E,0x620,0);
  AWE32RegDW(0x1E,0x620,0);
  AWE32RegDW(0x1E,0xA20,0xFFFFE3);

  AWE32RegW(0xBF,0xA20,0x80);
  AWE32RegDW(0xDF,0x620,0xFFFFF0);
  AWE32RegDW(0xFF,0x620,0xFFFFF8);
  AWE32RegDW(0x3F,0x620,0xFF);
  AWE32RegDW(0x1F,0x620,0x8000);
  AWE32RegDW(0x1F,0xA20,0xFFFFF3);
*/

/* kinda reset: (??????)
  disable();
  outpw(TMP_AWE32Base+0x802,0x3E);
  outpw(TMP_AWE32Base,0);
  while(k==0)
    k = inpw(TMP_AWE32Base+0x802) & 0x10;
  while(k==0x10)
    k = inpw(TMP_AWE32Base+0x802) & 0x10;
  enable();
  outpw(TMP_AWE32Base+2,0x4828);
  outpw(TMP_AWE32Base+0x802,0x3C);
  outpw(TMP_AWE32Base+0x400,0);
  enable();

  AWE32RegDW(0x7E,0x620,0xFFFFFFFF);
  AWE32RegDW(0x7F,0x620,0xFFFFFFFF);
*/
}

/****************************/
/* Enable AWE32 DRAM access */
/****************************/
// this routine allocates all "DMAs" (channels) for memory transfer
void EnableDRAMtransfer(void)
{ DWORD Scratch;

  write_AWE32_Wreg( HWCF2 ,0x0020 ); // ?? really needed ??
  for(int k=0;k<30;k++)
  { write_AWE32_Wreg ( DCYSUSV(k) , 0x80 ); // disable channel (volume=0)
    write_AWE32_DWreg( VTFT(k)    , 0 );    // target volume, filter cutoff (target volume zero)
    write_AWE32_DWreg( CVCF(k)    , 0 );    // current vol, filter (emmediatly volume zero)
    write_AWE32_DWreg( PSST(k)    , 0 );    // pan send, loop start (NO LOOPs!!)
    write_AWE32_DWreg( CLS(k)     , 0 );    // chor send, loop end (NO LOOPs!!)
    write_AWE32_DWreg( PTRX(k)    , 0x40000000 );    // step word wise through memory ! (target pitch)
    write_AWE32_DWreg( CPF(k)     , 0x40000000 );    // emmediatly word wise step ! (current pitch)
    Scratch = (((k&1)<<9)+0x400);
    Scratch = Scratch <<16;       // chose DMA stream for this channel
    write_AWE32_DWreg( CCCA(k) , Scratch );    // step word wise through memory ! (target pitch)
  }
}

/*****************************/
/* Disable AWE32 DRAM access */
/*****************************/
// this disables all allocated DMAs (then free for playing)
void DisableDRAMtransfer(void)
{ WORD timeout=30000;
  // wait for full-bit which shows transfer is completed (timeout counter...)
  while (read_AWE32_Wreg(SMLD)&0x80000000 && timeout--) ;
  for(int k=0;k<30;k++)										// disable DRAM access
    write_AWE32_DWreg( CCCA(k)    , 0 );    // deallocate channel for transfer
}

/* ------------------ -- -
   get size of installed DRAM
   --------------- -- -
*/
DWORD CheckDRAM(void)
{ DWORD DRAMSize=0;
  disable();
  EnableDRAMtransfer();
  AWE32Wait(2);
  write_AWE32_DWreg(SMALW,0x200000); // sound memory address for "left" SM-writes

  // write two words:
  write_AWE32_Wreg(SMLD,0x1234); // sound memory left write (automatic incr.)
  write_AWE32_Wreg(SMLD,0x7777); // sound memory left write (automatic incr.)

  // continue writing until address wrap arround overwrites 0x200000 adress
  while(DRAMSize < 28*1024)  // 28 MB is max onboard memory
  { AWE32Wait(2);
    write_AWE32_DWreg(SMALR,0x200000);  // sound memory address for "left" SM-reads
    read_AWE32_Wreg(SMLD); // Skip "scale" data, previously fetched from SMLD
    if (read_AWE32_Wreg(SMLD) != 0x1234) break;
    if (read_AWE32_Wreg(SMLD) != 0x7777) break;
    DRAMSize += 32;
    #ifdef __DEBUGGING__
      printf("\rDRAM: %li",(DRAMSize>>1));
    #endif
    write_AWE32_DWreg(SMALW,0x200000+DRAMSize*512L); // sound memory address for "left" SM-writes
    write_AWE32_Wreg (SMLD,0xFFFF); // sound memory left write (automatic incr.)
  }

  DisableDRAMtransfer();
  enable();
  #ifdef __DEBUGGING__
    printf("\rSBAWE32 %.4uKB memory onboard.\n",DRAMSize);
    getchar();
  #endif
  return(DRAMSize*512L); // size in words !!
}
