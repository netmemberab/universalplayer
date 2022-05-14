/****************************************************************************
  GUS - gravis ultra sound native support
  --------------------------------------------------------------------------
  (c)opyright 1996 by A.Baresel
  
  created: 23.12.1996
****************************************************************************/
// NOTE: a 'couple' of things from MIKMOD (defines+tables)
//       thx to MIKMOD coder - some well structured code lines ;)
// NOTE: this driver is usable for one active GUS - sorry but I tried to
//       get it working FAST !

// BUGs: - VolumeFade not yet - clicks !
//       - PanningFade not yet - clicks !
//	 - playing clicks

// ???:
// #define USE_ROLLOVER 0

#include "player.h"
#include "module.h"
#include "devices/dos_gus.h"
#include "instrmts/sample.h"
#include "arch/dos/dos_irq.h"
#include "mtypes.h"

#include <string.h>
#include <dos.h>
#include <math.h>

#ifdef __GNUC__
#include <pc.h>
#define inpb(a)  inportb(a)
#define inpw(a)  inportw(a)
#define outpb(a,b) outportb(a,b)
#define outpw(a,b) outportw(a,b)
#endif

static WORD TMP_GUS_BASE;

// memory address conversions:
DWORD convert_to_16bit(DWORD address); // convert adress for 16bit samples...
DWORD physical_address(WORD w1,WORD w2,BYTE mode);
#define ADDR_HIGH(x) ((WORD)((WORD)((x>>7L)&0x1fffL)))
#define ADDR_LOW(x)  ((WORD)((WORD)((x&0x7fL)<<9L)))
// GUS memory access:
BYTE GUS_read(DWORD loc);
void GUS_write(DWORD loc,BYTE val);
void GUS_writefast(DWORD loc,BYTE* buffer,WORD size);
void GUS_writechunk(DWORD loc,BYTE *buffer,DWORD size);
DWORD GUS_readlong(DWORD loc);
void GUS_writelong(DWORD loc,DWORD val);
// GUS register access:
static inline void GF1OutB(BYTE x,BYTE y);
static inline void GF1OutW(BYTE x,WORD y);
static inline BYTE GF1InB(BYTE x);
static inline WORD GF1InW(BYTE x);
// global GUS action:
static int  GUS_test(WORD base);
static void GUS_delay();
static WORD GUS_base_detect();
static void GUS_SetInterface(int dram,int adc,int gf1,int midi);
static DWORD GUS_detect_memsize();
static void GUS_reset(int voices);
// channel specific action
inline static void GUS_SetChannel(BYTE chn);
void GUS_StartChannel(BYTE vmode);
static WORD GUS_SetFrequency(DWORD frequency,int voices);
static void GUS_FadeVolume(WORD oldvolume,WORD newvolume);
static void GUS_SetPanning(BYTE pan);
static DWORD GUS_getsampleposition();
static void GUS_setsampleposition(DWORD);

//#############################################################################
//-----------------------------------------------------------------------------
//    LOWLEVEL GUS DEFINES
//-----------------------------------------------------------------------------
//#############################################################################


// GUS-Ports: -----------------------------------------------------------------
#define JOYSTICK_TIMER  (TMP_GUS_BASE+0x201)                /* 201 */
#define JOYSTICK_DATA   (TMP_GUS_BASE+0x201)                /* 201 */

#define GF1_MIDI_CTRL   (TMP_GUS_BASE+0x100)                /* 3X0 */
#define GF1_MIDI_DATA   (TMP_GUS_BASE+0x101)                /* 3X1 */

#define GF1_PAGE        (TMP_GUS_BASE+0x102)                /* 3X2 */
#define GF1_REG_SELECT  (TMP_GUS_BASE+0x103)                /* 3X3 */
#define GF1_VOICE_SELECT (TMP_GUS_BASE+0x102)               /* 3X3 */
#define GF1_DATA_LOW    (TMP_GUS_BASE+0x104)                /* 3X4 */
#define GF1_DATA_HI     (TMP_GUS_BASE+0x105)                /* 3X5 */
#define GF1_IRQ_STAT    (TMP_GUS_BASE+0x006)                /* 2X6 */
#define GF1_DRAM        (TMP_GUS_BASE+0x107)                /* 3X7 */

#define GF1_MIX_CTRL    (TMP_GUS_BASE+0x000)                /* 2X0 */
#define GF1_TIMER_CTRL  (TMP_GUS_BASE+0x008)                /* 2X8 */
#define GF1_TIMER_DATA  (TMP_GUS_BASE+0x009)                /* 2X9 */
#define GF1_IRQ_CTRL    (TMP_GUS_BASE+0x00B)                /* 2XB */

// GUS-Registers: -------------------------------------------------------------
// The GF1 Hardware clock:
#define CLOCK_RATE              9878400L

#define DMA_CONTROL             0x41
#define SET_DMA_ADDRESS         0x42
#define SET_DRAM_LOW            0x43
#define SET_DRAM_HIGH           0x44
#define TIMER_CONTROL           0x45
#define TIMER1                  0x46
#define TIMER2                  0x47
#define SET_SAMPLE_RATE         0x48
#define SAMPLE_CONTROL          0x49
#define SET_JOYSTICK            0x4B
#define MASTER_RESET            0x4C
#define TIMER_CONTROL           0x45
#define TIMER1                  0x46
#define TIMER2                  0x47
#define SET_SAMPLE_RATE         0x48
#define SAMPLE_CONTROL          0x49
#define SET_JOYSTICK            0x4B
#define MASTER_RESET            0x4C

// Voice register mapping:
#define SET_CONTROL             0x00
#define SET_FREQUENCY           0x01
#define SET_START_HIGH          0x02
#define SET_START_LOW           0x03
#define SET_END_HIGH            0x04
#define SET_END_LOW             0x05
#define SET_VOLUME_RATE         0x06
#define SET_VOLUME_START        0x07
#define SET_VOLUME_END          0x08
#define SET_VOLUME              0x09
#define SET_ACC_HIGH            0x0a
#define SET_ACC_LOW             0x0b
#define SET_BALANCE             0x0c
#define SET_VOLUME_CONTROL      0x0d
#define SET_VOICES              0x0e
#define GET_CONTROL             0x80
#define GET_FREQUENCY           0x81
#define GET_START_HIGH          0x82
#define GET_START_LOW           0x83
#define GET_END_HIGH            0x84
#define GET_END_LOW             0x85
#define GET_VOLUME_RATE         0x86
#define GET_VOLUME_START        0x87
#define GET_VOLUME_END          0x88
#define GET_VOLUME              0x89
#define GET_ACC_HIGH            0x8a
#define GET_ACC_LOW             0x8b
#define GET_BALANCE             0x8c
#define GET_VOLUME_CONTROL      0x8d
#define GET_VOICES              0x8e
#define GET_IRQV                0x8f

// GF1_IRQ_STATUS (port 3X6) --------------------------------------------------
#define MIDI_TX_IRQ             0x01            /* pending MIDI xmit IRQ */
#define MIDI_RX_IRQ             0x02            /* pending MIDI recv IRQ */
#define GF1_TIMER1_IRQ          0x04            /* general purpose timer */
#define GF1_TIMER2_IRQ          0x08            /* general purpose timer */
#define WAVETABLE_IRQ           0x20            /* pending wavetable IRQ */
#define ENVELOPE_IRQ            0x40            /* pending volume envelope IRQ */
#define DMA_TC_IRQ              0x80            /* pending dma tc IRQ */

// GF1_MIX_CTRL (port 2X0) ----------------------------------------------------
#define ENABLE_LINE_IN          0x01            /* 0=enable */
#define ENABLE_OUTPUT           0x02            /* 0=enable */
#define ENABLE_MIC_IN           0x04            /* 1=enable */
#define ENABLE_GF1_IRQ          0x08            /* 1=enable */
#define GF122                   0x10            /* ?? */
#define ENABLE_MIDI_LOOP        0x20            /* 1=enable loop back */
#define SELECT_GF1_REG          0x40            /* 0=irq latches */ /* 1=dma latches */

// DMA control register --------------------------------------------------------
#define DMA_ENABLE              0x01
#define DMA_READ                0x02            /* 1=read,0=write */
#define DMA_WIDTH_16            0x04            /* 1=16 bit,0=8 bit (dma chan width)*/
#define DMA_RATE                0x18            /* 00=fast, 11=slow */
#define DMA_IRQ_ENABLE          0x20            /* 1=enable */
#define DMA_IRQ_PENDING         0x40            /* read */
#define DMA_DATA_16             0x40            /* write (data width) */
#define DMA_TWOS_COMP           0x80            /* 1=do twos comp */

// These are the xfer rate bits ... -------------------------------------------
#define DMA_R0          	0x00            /* Fastest DMA xfer (~650khz) */
#define DMA_R1          	0x08            /* fastest / 2 */
#define DMA_R2          	0x10            /* fastest / 4 */
#define DMA_R3          	0x18            /* Slowest DMA xfer (fastest / 8) */

// SAMPLE control register ----------------------------------------------------
#define ENABLE_ADC              0x01
#define ADC_MODE                0x02            /* 0=mono, 1=stereo */
#define ADC_DMA_WIDTH           0x04            /* 0=8 bit, 1=16 bit */
#define ADC_IRQ_ENABLE          0x20            /* 1=enable */
#define ADC_IRQ_PENDING         0x40            /* 1=irq pending */
#define ADC_TWOS_COMP           0x80            /* 1=do twos comp */

// RESET control register -----------------------------------------------------
#define GF1_MASTER_RESET        0x01            /* 0=hold in reset */
#define GF1_OUTPUT_ENABLE       0x02            /* enable output */
#define MIDI_RESET              0x03
#define GF1_MASTER_IRQ          0x04            /* master IRQ enable */

// ------------------------------------------------------
// GF1 voice specific registers ($00 - $0E and $80-$8f)
// ------------------------------------------------------

/* ($0,$80) Voice control register */
#define VOICE_STOPPED           0x01            /* voice has stopped */
#define STOP_VOICE              0x02            /* stop voice */
#define VC_DATA_TYPE            0x04            /* 0=8 bit,1=16 bit */
#define VC_LOOP_ENABLE          0x08            /* 1=enable */
#define VC_BI_LOOP              0x10            /* 1=bi directional looping */
#define VC_WAVE_IRQ             0x20            /* 1=enable voice's wave irq */
#define VC_DIRECT               0x40            /* 0=increasing,1=decreasing */
#define VC_IRQ_PENDING          0x80            /* 1=wavetable irq pending */

/* ($1,$81) Frequency control */
/* 		Bit 0  - Unused */
/* 		Bits 1-9 - Fractional portion */
/* 		Bits 10-15 - Integer portion */

/* ($2,$82) Accumulator start address (high) */
/* 		Bits 0-11 - HIGH 12 bits of address */
/* 		Bits 12-15 - Unused */

/* ($3,$83) Accumulator start address (low) */
/* 		Bits 0-4 - Unused */
/* 		Bits 5-8 - Fractional portion */
/* 		Bits 9-15 - Low 7 bits of integer portion */

/* ($4,$84) Accumulator end address (high) */
/* 		Bits 0-11 - HIGH 12 bits of address */
/* 		Bits 12-15 - Unused */

/* ($5,$85) Accumulator end address (low) */
/* 		Bits 0-4 - Unused */
/* 		Bits 5-8 - Fractional portion */
/* 		Bits 9-15 - Low 7 bits of integer portion */


/* ($6,$86) Volume Envelope control register */
#define VL_RATE_MANTISSA        0x3f
#define VL_RATE_RANGE           0xC0

/* ($7,$87) Volume envelope start */
#define VL_START_MANT           0x0F
#define VL_START_EXP            0xF0

/* ($8,$88) Volume envelope end */
#define VL_END_MANT             0x0F
#define VL_END_EXP              0xF0

/* ($9,$89) Current volume register */
/* 		Bits 0-3 are unused */
/* 		Bits 4-11 - Mantissa of current volume */
/* 		Bits 10-15 - Exponent of current volume */

/* ($A,$8A) Accumulator value (high) */
/* 		Bits 0-12 - HIGH 12 bits of current position (a19-a7) */

/* ($B,$8B) Accumulator value (low) */
/* 		Bits 0-8 - Fractional portion */
/* 		Bits 9-15 - Integer portion of low adress (a6-a0) */

/* ($C,$8C) Pan (balance) position */
/* 		Bits 0-3 - Balance position  0=full left, 0x0f=full right */

/* ($D,$8D) Volume control register */
#define VOLUME_STOPPED          0x01            /* volume has stopped */
#define STOP_VOLUME             0x02            /* stop volume */
#define VC_ROLLOVER             0x04            /* Roll PAST end & gen IRQ */
#define VL_LOOP_ENABLE          0x08            /* 1=enable */
#define VL_BI_LOOP              0x10            /* 1=bi directional looping */
#define VL_WAVE_IRQ             0x20            /* 1=enable voice's wave irq */
#define VL_DIRECT               0x40            /* 0=increasing,1=decreasing */
#define VL_IRQ_PENDING          0x80            /* 1=wavetable irq pending */

/* ($E,$8E) # of Active voices */
/* 		Bits 0-5 - # of active voices -1 */

/* ($F,$8F) - Sources of IRQs */
/* 		Bits 0-4 - interrupting voice number */
/* 		Bit 5 - Always a 1 */

#define VOICE_VOLUME_IRQ        0x40            /* individual voice irq bit */
#define VOICE_WAVE_IRQ          0x80            /* individual waveform irq bit */




/*#############################################################################
-------------------------------------------------------------------------------
      GUS CLASS
-------------------------------------------------------------------------------
#############################################################################*/

GUS_c::GUS_c(DeviceOptionsPtr p):DeviceClass()
{ GUS_HardwareOptionsPtr gus_opts=(GUS_HardwareOptionsPtr)p;
  int voices=14; // DEBUG !!
  gusvoices=voices;

  // autodetection etc. if settings ok then initialize the GUS
  memset(sample,0,sizeof(sample));
  gusbase=gus_opts->base;
  if (!gusbase) gusbase=GUS_base_detect();
  if (!gusbase) { status=DEVICE_FAILED;return; }
  printf("GUS base: %3Xh\n",gusbase);
  // from GUS_open:
  // --------
  TMP_GUS_BASE=gusbase;
  trace("try reset.\n");
  GUS_reset(voices);
  GUS_SetInterface(gus_opts->dram_dma,gus_opts->adc_dma,
                   gus_opts->gf1_irq,gus_opts->midi_irq);
  trace("Interface ok.\n");
  GUS_reset(voices);
  // memory initialization:
  DRAM_size=GUS_detect_memsize();
  trace("memory ok.\n");
  DRAM_free=0; // first free adress !
  printf("DRAM free: %li\n",DRAM_size);
  MemInit();
  trace("initialize volumes\n");
  GUS_volumes[0] = 0;
  for(int k=1; k<65; k++)
    printf("%i,",GUS_volumes[k] = (WORD)(log(8+k)*12772.829));
  getchar();
}

GUS_c::~GUS_c(void)
{ // this is called to remove GUS driver from memory
  // stop playing and free all used hardware resources
  Stop();
  GUS_reset(14);
  // MIrq_OnOff(GUS_GF1_IRQ,0);
  // MIrq_SetHandler(GUS_GF1_IRQ,oldhandler);
  IRQ_Manager.FreeTimer(timerid);
}

void GUS_c::SetupChannel(BYTE chn,ChannelStruct&)
{
}

void GUS_c::SetupChannels(ChannelSettings&)
{
}

struct GUS_IRQ_data {
		GUS_c * gus;
		ModuleClass * Module;
};

static int timerflag;

Status GUS_c::Start(ModuleClass* Module)
{ Module->StartModule();
  memset(&channel,0,sizeof(channel));
  memset(&GUS_channel,0,sizeof(GUS_channel));
  memset(&chn_map,NOT_USED,sizeof(chn_map));
  // setup your timer to call newnotes() emmediatly
  // (where you'll get next timing information)
  timerflag=0;
  { GUS_IRQ_data * data = new GUS_IRQ_data;
    data->gus=this;data->Module=Module;
    timerid=IRQ_Manager.ReserveTimer(1000,GUS_timer_irq,data,0);
    // single timer (do not a multi timer - we need to reprogram it !)
  }
  // for debugging:
  do {
    if (timerflag)
      { timerflag=0;
	newnotes(Module);
      }
  }
  while (1);

  // GUS timer driven -----------------------------------
  //oldhandler=MIrq_SetHandler(GUS_GF1_IRQ,gf1handler);
  //MIrq_OnOff(GUS_GF1_IRQ,1);
  // GUS timer driven -----------------------------------

  trace("END.    GUS::Start\n");
  return NO_ERROR;
}

Status GUS_c::Stop(void)
{ // all channels off - stop calling newnotes()
  int i;
  for (i=0;i<gusvoices;i++)
  if (channel[i].active)
  { GUS_SetChannel(channel[i].GUSchn);
    channel[i].stopaddr=GUS_getsampleposition();
    NoteOff(i);
  }
}

Status GUS_c::Continue(void)
{ // continue were we have been stopped
  int i;
  for (i=0;i<gusvoices;i++)
  if (channel[i].active)
  { 
    GUS_SetChannel(channel[i].GUSchn);
    SetNewNote(i);
    GUS_setsampleposition(channel[i].stopaddr);
    GUS_StartChannel(channel[i].GUSchn);
  }
}

/* ------ =================================================== ------
   #####  Instrument handling: allocating, uploading, freeing  #####
   ------ =================================================== ------
*/
// --------------------------------------------------------------------
// Instrument allocation (reserve memory for samples) is done only
// in a logical way - GUS_c stores all free and reserved memory
// locations in GUS-DRAM, Alloc does change this data.
// ....no restrictions for call situation....
// --------------------------------------------------------------------
ResourceID GUS_c::AllocateInstruments(InstrumentIDs ins,int noi)
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
  printf("Device reserves:\n");
  for (j=0;j<noi;j++) printf("%li:",*ins++=iid+j);
  printf("rid: %i\n",rid);
  return rid;
}

// --------------------------------------------------------------------
// Free is only a logical (not done by hardware) operation, it does
// change internal GUS_c data structurs - all Instruments were
// stored in this resource are not accessible after freeing.
// ....no restrictions for call situation....
// --------------------------------------------------------------------
Status GUS_c::FreeInstruments(ResourceID id)
{ int i=id&0xff;
  int j;
  if (sample[i]==NULL) return BAD_ID;
  if (j=(sample[i])->no_samples) delete ((sample[i])->smpinfo);
  delete (sample[i]);
  sample[i]=NULL;
  return NO_ERROR;
}

// --------------------------------------------------------------------
// Uploading does write the sample data into the GUS-DRAM, this is only
// possible until if no other module is playing ! Just stop playing or
// do it short after initializing the GUS_c
// ....don't call while playing (it does change the hardware state)....
// --------------------------------------------------------------------
Status GUS_c::UploadInstrument(InstrumentID Iid,InstrumentLoaderClass& loader)
{ int rscno=(Iid>>12)&0xff;
  Status st;
  printf("access resource %i\n",rscno);
  if (sample[rscno]==NULL || (Iid&0x3fff)>sample[rscno]->no_samples) return BAD_ID;
  trace("upload id ok\n");
  SampleStruct * smpptr= &sample[rscno]->smpinfo[ Iid & 0x3fff ];
  // set output format ... (currently only 8bit upload !)
  SampleType ss=SMP_SIGNED_8BIT;
  if ((st=loader.SetOutputFormat(IT_SAMPLE,&ss))!=NO_ERROR)
    return st; // instrument cannot be converted ...

  // get sample specs ...
  SampleSpec sp;
  loader.GetInstrumentSpecs(Iid,(UserSpecPtr)&sp);
  printf("Sample: (%li) %li-%li\n",sp.size,sp.loopstart,sp.loopend);
  smpptr->id=Iid;
  // enter specs into instrument struct
  if (DRAM_free+sp.size>DRAM_size) { trace("Not enough memory on your GUS.\n");return(NO_MEMORY); }

  // form mode flag: pass mode bits:  2=16bit,3=loop,4=bidi
  smpptr->mode=0;
  if (sp.loopflag&LOOP_MODE) smpptr->mode|=VC_LOOP_ENABLE;
  if (sp.loopflag&BIDI_MODE) smpptr->mode|=VC_BI_LOOP;
  // VC_DATA_TYPE - 16bit
  smpptr->startaddr=DRAM_free;
  if (sp.loopflag&LOOP_MODE)
  { smpptr->loopstart=DRAM_free+sp.loopstart;
    smpptr->loopend=DRAM_free+sp.loopend;
  } else
  { smpptr->loopstart=DRAM_free+sp.size;
    smpptr->loopend=DRAM_free+sp.size+4;
  }
  DRAM_free+=sp.size+10;
  // do hardware upload:
  return LoadInstrument(smpptr,loader,sp.size);
}

// --------------------------------------------------------------------
//   newnotes reads the 'simple hardware instructions' and
//   does the programming job of the GUS-hardware
//   (it uses several private members for hardware changes)
// --------------------------------------------------------------------

void  GUS_c::newnotes(ModuleClass * Module)
{ int i;
  int start_note;
  // check channels if they are off and
  // all "out fading" channels'll be stopped !
  for (i=0;i<gusvoices;i++)
  {
    if (chn_map[i]!=NOT_USED)
    { GUS_SetChannel(i);
      BYTE mode=GF1InB(GET_CONTROL);
      if (mode & VOICE_STOPPED) 
      { BYTE vchn=chn_map[i];
        channel[vchn].active=0;
        GUS_channel[i]=OFF;printf("%i is off,",i); 
        chn_map[i]=NOT_USED;
      }
    } 
    if (GUS_channel[i]==FADEOFF)
    { GUS_SetChannel(i);
      /*BYTE mode=GF1InB(GET_VOLUME_CONTROL);
      if (mode & VOLUME_STOPPED) */
      { printf("is out "); StopNote(i); }
    }
  }

  // get next commands:
  DeviceCommandsStruct devcmd;
  memset(&devcmd,0,sizeof(devcmd));
  TMP_GUS_BASE=gusbase;
  devcmd.timerbase=1193181; // timer base in Hz
  Module->GetDeviceCommands(devcmd); // <- here you'll get the information what to do next !

  // first check through timing information
  DWORD counter=devcmd.countervalue;
  if (counter>65535) { printf("GUS support error...big timer val...\n"); exit(1); }
  IRQ_Manager.ReprogramTimerCnt(timerid,counter);

  disable();
  // read playinfo data and program the GUS for the requests:
  for (i=0;i<devcmd.no_commands;i++)
  { SampleStruct * smpptr= &(sample[(channel[i].instr>>12)&0xff]->smpinfo[channel[i].instr&0x3fff]);
    GUS_SetChannel(channel[i].GUSchn); 
    start_note=0;

    if (devcmd.Commands[i].Changes&CHG_STOPPLAY)
    { // stop playing instrument emmediatly, but store position where it stays
      // currently
      // save current position...
      // Stop it...
      NoteOff(i);
    }

    if (devcmd.Commands[i].Changes&CHG_INSTRUMENT)    // set instrument data
    { // another instrument is used on this channel now !
      // (stop playing current one, change internal instrument settings
      //  it will be used in next CHG_FREQUENCY)
      InstrumentID iid=devcmd.Commands[i].instrid;
      if ((iid&0x3fff)!=NO_INSTRID)
      { channel[i].instr = iid;
  	smpptr= &(sample[(iid>>12)&0xff]->smpinfo[iid&0x3fff]);
        channel[i].volume=64;
        channel[i].offset=0;
      } else devcmd.Commands[i].Changes&=~CHG_INSTRUMENT;
    }

    if (devcmd.Commands[i].Changes&CHG_SETOFFSET) // set offset
    { // start playing the current instrument from this position (given in samples)
      // requested sample offset is: devcmd.Commands[i].Offset=DWORD
      channel[i].offset=devcmd.Commands[i].Offset;
      // BUGGY ????
      if (!(devcmd.Commands[i].Changes&CHG_FREQUENCY)) SetNewNote(i);
    }

    if (devcmd.Commands[i].Changes&CHG_FREQUENCY)
    { // change pitch in this channel
      // if it comes with a CHG_INSTRUMENT start this instrument with this pitch,
      // requested pitch as frequency: devcmd.Commands[i].frequency
      channel[i].frequency=devcmd.Commands[i].Frequency;
      if (devcmd.Commands[i].Changes&CHG_INSTRUMENT)
      { SetNewNote(i);
	start_note=1;
      }
      channel[i].GUS_pitchreg=GUS_SetFrequency(channel[i].frequency,14/*gusvoices*/);
    }

    if (devcmd.Commands[i].Changes&CHG_PANNING) // panposition does change
    { // not yet correct... (clicks!!!)
      GUS_SetPanning((channel[i].panpos=devcmd.Commands[i].Panning>>4)&0xf);
    }

    if (devcmd.Commands[i].Changes&CHG_VOLUME)
    { BYTE volume=devcmd.Commands[i].Volume;
      printf("vol(%i),",volume);
      if (volume>64) volume=64;
      if (start_note)
        GF1OutW(SET_VOLUME,GUS_volumes[volume]);
        //GUS_FadeVolume(GUS_volumes[0],GUS_volumes[volume]);
      else
       GF1OutW(SET_VOLUME,GUS_volumes[volume]);
       //GUS_FadeVolume(GUS_volumes[channel[i].volume],GUS_volumes[volume]);
      channel[i].volume=volume;
    }

    if (devcmd.Commands[i].Changes&CHG_RESTART)
    { // do only restart if the instrument is really currently playing and
      // does not stay at sample end !
    }

    if (devcmd.Commands[i].Changes&CHG_CONTINUE)
    { // continue playing at the position where it was stoped with CHG_STOPPLAY
    }

    if (start_note) 
	GUS_StartChannel(smpptr->mode);
  }
  enable();
  fflush(stdout);
}



/*#############################################################################
-------------------------------------------------------------------------------
           private members for secondary hardware access
-------------------------------------------------------------------------------
#############################################################################*/

Status GUS_c::LoadInstrument(SampleStruct*smpptr,InstrumentLoaderClass& loader,DWORD size)
{ BYTE SampleBuffer[16*1024U];
  WORD len,i;
  InstrumentID id=smpptr->id;
  DWORD addr=smpptr->startaddr;
  // 8bit load .. only
  while (size>0)
  { len=(size>16*1024U)?16*1024U:size;
    printf("%i,",len);
    if (loader.Load(id,(DataPtr*)&SampleBuffer,len))
      { trace("error reading sample.\n");return(ERRORLOADING); }
    GUS_writechunk(addr,(BYTE*)&SampleBuffer,len);
    addr+=len;
    size-=len;
  }
  for (i=0;i<5;i++) GUS_write(addr++,SampleBuffer[len-1]);
  return NO_ERROR;
}

BYTE GUS_c::GetNextChannel(BYTE current)
{ int i=0;
  while (i<gusvoices && GUS_channel[i]!=OFF) i++;
  printf("-%i-",i);
  if (i==gusvoices) return current; // no free channel
  else return i; // nice :)
}

void  GUS_c::SetNewNote(BYTE vchn)
{ // program hardware for a new note (but don't start!)
  BYTE chn=channel[vchn].GUSchn;
  if (channel[vchn].active) { printf("š");NoteOff(vchn); } else chn=0xff;
  chn=GetNextChannel(chn); // get next free channel

  GUS_SetChannel(chn);
  if (GUS_channel[chn]!=OFF) { printf("#");StopNote(chn); }
  GUS_channel[chn]=ON;channel[vchn].active=1;
  channel[vchn].GUSchn=chn;chn_map[chn]=vchn;
  printf("start  %i,",chn);

  SampleStruct * smpptr= &(sample[(channel[vchn].instr>>12)&0xff]->smpinfo[channel[vchn].instr&0x3fff]);
  DWORD phys_loopstart,phys_loopend;
  DWORD phys_startadr;
  // if 16 bit data, must convert addresses
  if (smpptr->mode&VC_DATA_TYPE)
  { phys_startadr = convert_to_16bit(smpptr->startaddr+channel[vchn].offset);
    phys_loopstart= convert_to_16bit(smpptr->loopstart);
    phys_loopend  = convert_to_16bit(smpptr->loopend);
  } else
  { phys_startadr = smpptr->startaddr+channel[vchn].offset;
    phys_loopstart= smpptr->loopstart;
    phys_loopend  = smpptr->loopend;
  }

  // set/reset the rollover bit as per user request
  // BYTE vmode;
  // vmode=GF1InB(GET_VOLUME_CONTROL);
  // if(smpptr->mode&USE_ROLLOVER) vmode |= VC_ROLLOVER;
  // else vmode &= ~VC_ROLLOVER;
  // GF1OutB(SET_VOLUME_CONTROL,vmode);
  // GUS_delay();
  // GF1OutB(SET_VOLUME_CONTROL,vmode);

  // set adresses
  GF1OutW(SET_ACC_LOW,ADDR_LOW(phys_startadr));
  GF1OutW(SET_ACC_HIGH,ADDR_HIGH(phys_startadr));
  GF1OutW(SET_START_HIGH,ADDR_HIGH(phys_loopstart));
  GF1OutW(SET_START_LOW,ADDR_LOW(phys_loopstart));
  GF1OutW(SET_END_HIGH,ADDR_HIGH(phys_loopend));
  GF1OutW(SET_END_LOW,ADDR_LOW(phys_loopend));
}

void  GUS_c::NoteOff(BYTE vchn)
{ // stop currently active channel
  printf("fadeoff  %i,",channel[vchn].GUSchn);

  // set internal data:
  BYTE chn=channel[vchn].GUSchn;
  GUS_channel[chn]=FADEOFF;chn_map[chn]=NOT_USED;
  channel[vchn].active=0;
  if (channel[vchn].volume==0) { printf("/"); StopNote(chn);return; }
  GUS_FadeVolume(GUS_volumes[channel[vchn].volume],GUS_volumes[0]);

  // turn off the roll over bit first ...
  // we don't use GUSes volume envelopes, so we don't need the following stuff:
  //
  // data=GF1InB(GET_VOLUME_CONTROL);
  // data&=~VC_ROLLOVER;
  // GF1OutB(SET_VOLUME_CONTROL,data);
  // GUS_delay();
  // GF1OutB(SET_VOLUME_CONTROL,data);
}

void  GUS_c::StopNote(BYTE chn) // chn==hardware channel needs to be set before !!
{ // stop voice emmediatly - no fading !!
  // !clicks if volume not zero!
  // set internal data:
  printf("stop  %i,",chn);GUS_channel[chn]=OFF;

  BYTE data;
  data=GF1InB(GET_CONTROL);
  data&=~VC_WAVE_IRQ;             /* disable irq's & stop voice .. */
  data|=VOICE_STOPPED|STOP_VOICE;
  GF1OutB(SET_CONTROL,data);      /* turn it off */
  GUS_delay();
  GF1OutB(SET_CONTROL,data);
}

void GUS_timer_irq(UserData ud) // IRQ for playing
{ // called by PC-hardware
  timerflag=1;
  /*
  GUS_IRQ_data * data=(GUS_IRQ_data*)ud;
  if (!playing) return;
  if (data->gus!=NULL) data->gus->newnotes(data->Module);
  */
}

void GUS_card_irq(UserData ud) // card IRQ for playing
{ // called by GUS hardware
  /*
  GUS_IRQ_data * data=(GUS_IRQ_data*)ud;
  if (!playing) return;
  if (data->gus!=NULL) data->gus->newnotes(data->Module);
  */
}



/*#############################################################################
-------------------------------------------------------------------------------
            C-functions for primary hardware accesses
-------------------------------------------------------------------------------
#############################################################################*/

// Basic GUS register access: -----------------------------------------------
static inline void GF1OutB(BYTE x,BYTE y)
{ outpb(GF1_REG_SELECT,x);
  outportb(GF1_DATA_HI,y);
}

static inline void GF1OutW(BYTE x,WORD y)
{ outpb(GF1_REG_SELECT,x);
  outpw(GF1_DATA_LOW,y);
}

static inline BYTE GF1InB(BYTE x)
{ outpb(GF1_REG_SELECT,x);
  return inpb(GF1_DATA_HI);
}

static inline WORD GF1InW(BYTE x)
{ outpb(GF1_REG_SELECT,x);
  return inpw(GF1_DATA_LOW);
}

void GUS_delay()
{ inpb(GF1_DRAM);
  inpb(GF1_DRAM);
  inpb(GF1_DRAM);
  inpb(GF1_DRAM);
  inpb(GF1_DRAM);
  inpb(GF1_DRAM);
  inpb(GF1_DRAM);
}

// GUS adress conversions: -------------------------------------------------
DWORD physical_address(WORD w1,WORD w2,BYTE mode)
{
  if (mode&VC_DATA_TYPE) // 16bit sample
  { // like the good old DMA-chip in 16bit mode -> shift lower 16bit one
    // time to the left
    DWORD l1=((DWORD)(w1)<<7)+((DWORD)(w2)>>9),l2=l1<<1;
    return (l1&0xc0000L)|(l2&0x3ffffL);
  } else // simple 8bit sample:
    return ((DWORD)(w1)<<7)+((DWORD)(w2)>>9);
}

DWORD convert_to_16bit(DWORD address) // convert adress for 16bit samples...
{ DWORD l=address&0xc0000L;
  address>>=1;
  return (address&0x1ffffL)|l;
}

// GUS memory access: ------------------------------------------------------
// simple and slow memory writes/reads:
BYTE GUS_read(DWORD loc)
{ GF1OutW(SET_DRAM_LOW,loc);
  GF1OutB(SET_DRAM_HIGH,(loc>>16)&0xff);      /* MSByte */
  return(inpb(GF1_DRAM));
}

void GUS_write(DWORD loc,BYTE val)
{ GF1OutW(SET_DRAM_LOW,loc);
  GF1OutB(SET_DRAM_HIGH,(loc>>16)&0x0ff);
  outpb(GF1_DRAM,val);
}

// faster memory access:
void GUS_writefast(DWORD loc,BYTE* buffer,WORD size) // no 64K boundary check
{ outpb(GF1_REG_SELECT,SET_DRAM_HIGH);
  outpb(GF1_DATA_HI,(loc>>16)&0x0ff);       /* 8 bits */
  outpb(GF1_REG_SELECT,SET_DRAM_LOW);
  while (size--)
  { outpw(GF1_DATA_LOW,loc++);
    outpb(GF1_DRAM,*buffer++);
  }
}

void GUS_writechunk(DWORD loc,BYTE *buffer,DWORD size) // fast write with 64K check !
{ DWORD len;
  while (size>0)
  { len=0x10000L-(loc&0xffffL);
    if (len>size) len=size;
    GUS_writefast(loc,buffer,len);
    size-=len;loc+=len;buffer+=len;
  }
}

DWORD GUS_readlong(DWORD loc)
{ DWORD val;
  BYTE *v=(BYTE*)&val;
  *v++=GUS_read(loc++);
  *v++=GUS_read(loc++);
  *v++=GUS_read(loc++);
  *v=GUS_read(loc++);
  return val;
}

void GUS_writelong(DWORD loc,DWORD val)
{ GUS_writechunk(loc,(BYTE*)&val,4);
}

// global GUS activities ----------------------------------------------------
// check if gus is on this 'base'address   range: 0x210...0x280
int GUS_test(WORD base)
{ BYTE s1,s2,t1,t2;
  // Pull a reset on the GF1
  GF1OutB(MASTER_RESET,0x00);
  GUS_delay();
  GUS_delay();
  // Release Reset
  GF1OutB(MASTER_RESET,GF1_MASTER_RESET);
  GUS_delay();
  GUS_delay();
  // save values
  s1=GUS_read(0); s2=GUS_read(1);
  GUS_write(0,0xaa);
  GUS_write(1,0x55);
  t1=GUS_read(0);
  t2=GUS_read(1);
  // restore values
  GUS_write(0,s1); GUS_write(1,s2);
  return(t1==0xaa && t2==0x55);
}

// returns GUS base adress
WORD GUS_base_detect()
{ WORD i=0x210;
  while (i<0x280 && !GUS_test(i)) i+=0x10;
  if (i<0x290) { printf("GUS found at 0x%3Xdh.\n",i);return i; }
  return 0;
}

// returns avaliable memory on GUS
DWORD GUS_detect_memsize()
{ GUS_write(0x40000,0xAA);
  if (GUS_read(0x40000)!=0xAA) return 0x3ffff; // 256 KB
  GUS_write(0x80000,0xAA);
  if (GUS_read(0x80000)!=0xAA) return 0x7ffff; // 512 KB
  GUS_write(0xC0000,0xAA);
  if (GUS_read(0xC0000)!=0xAA) return 0xBffff; // 768 KB
  GUS_write(0xC0000,0xAA);
  if (GUS_read(0xC0000)!=0xAA) return 0xBffff; // 768 KB
  return 0xfffff; // 1 Meg
}

void GUS_reset(int voices)
{ int v;

  if(voices<14) voices=14;
  if(voices>32) voices=32;

  // GUS_TIMER_CTRL=0;
  // GUS_TIMER_MASK=0;
  trace("in\n");
  GUS_writelong(0,0); // GUS-memory handling (like the old simple pascal heap)
  trace("write.\n");
  GF1OutB(MASTER_RESET,0x00);
  trace("reset\n");
  for(v=0;v<10;v++) GUS_delay();
  trace("master reset.\n");
  // Release Reset and wait
  GF1OutB(MASTER_RESET,GF1_MASTER_RESET);
  for (v=0;v<10;v++) GUS_delay();

  // Reset the MIDI port also
  outpb(GF1_MIDI_CTRL,MIDI_RESET);
  for (v=0;v<10;v++) GUS_delay();
  outpb(GF1_MIDI_CTRL,0x00);
  trace("Interrupts.\n");
  // Clear all interrupts.
  GF1OutB(DMA_CONTROL,0x00);
  GF1OutB(TIMER_CONTROL,0x00);
  GF1OutB(SAMPLE_CONTROL,0x00);

  // Set the number of active voices
  GF1OutB(SET_VOICES,((voices-1) | 0xC0));

  // Clear interrupts on voices.
  // Reading the status ports will clear the irqs.
  inpb(GF1_IRQ_STAT);
  GF1InB(DMA_CONTROL);
  GF1InB(SAMPLE_CONTROL);
  GF1InB(GET_IRQV);

  trace("Now clear voices.\n");
  for(v=0;v<voices;v++)
  { // Select the proper voice
    outpb(GF1_VOICE_SELECT,v);
    // Stop the voice and volume
    GF1OutB(SET_CONTROL,VOICE_STOPPED|STOP_VOICE);
    GF1OutB(SET_VOLUME_CONTROL,VOLUME_STOPPED|STOP_VOLUME);
    // Wait 4.8 micos. or more:
    GUS_delay();
    // Initialize each voice specific registers. This is not
    // really necessary, but is nice for completeness sake ..
    // Each application will set up these to whatever values
    // it needs.
    GF1OutW(SET_FREQUENCY,0x0400);
    GF1OutW(SET_START_HIGH,0);
    GF1OutW(SET_START_LOW,0);
    GF1OutW(SET_END_HIGH,0);
    GF1OutW(SET_END_LOW,0);
    GF1OutB(SET_VOLUME_RATE,0x01);
    GF1OutB(SET_VOLUME_START,0x10);
    GF1OutB(SET_VOLUME_END,0xe0);
    GF1OutW(SET_VOLUME,0x0000);
    GF1OutW(SET_ACC_HIGH,0);
    GF1OutW(SET_ACC_LOW,0);
    GF1OutB(SET_BALANCE,0x07);
  }
  inpb(GF1_IRQ_STAT);
  GF1InB(DMA_CONTROL);
  GF1InB(SAMPLE_CONTROL);
  GF1InB(GET_IRQV);
  // Set up GF1 Chip for interrupts & enable DACs.
  // GF1OutB(MASTER_RESET,GF1_MASTER_RESET|GF1_OUTPUT_ENABLE);
  GF1OutB(MASTER_RESET,GF1_MASTER_RESET|GF1_OUTPUT_ENABLE|GF1_MASTER_IRQ);
  trace("out\n");
}

// change global GUS settings:------------
static BYTE dmalatch[8]       ={ 0,1,0,2,0,3,4,5 };
static BYTE irqlatch[16]      ={ 0,0,1,3,0,2,0,4,0,0,0,5,6,0,0,7 };

static void GUS_SetInterface(int dram,int adc,int gf1,int midi)
// int dram;    dram dma channel
// int adc;     adc dma channel
// int gf1;     gf1 irq number
// int midi;    midi irq number
{
  BYTE gf1_irq, midi_irq,dram_dma,adc_dma;
  BYTE irq_control,dma_control;
  BYTE mix_image;
  // Don't need to check for 0 irq #. Its latch entry = 0
  gf1_irq =irqlatch[gf1];
  midi_irq=irqlatch[midi];
  midi_irq<<=3;

  dram_dma=dmalatch[dram];
  adc_dma =dmalatch[adc];
  adc_dma<<=3;

  irq_control=dma_control=0x0;
  mix_image=0; //GUS_MIX_IMAGE
  irq_control|=gf1_irq;
  if((gf1==midi) && (gf1!=0)) irq_control|=0x40;
  else irq_control|=midi_irq;

  dma_control|=dram_dma;

  if((dram==adc) && (dram!=0)) dma_control|=0x40;
  else dma_control|=adc_dma;

  // Set up for Digital ASIC
  outpb(TMP_GUS_BASE+0x0f,0x5);
  outpb(GF1_MIX_CTRL,mix_image);
  outpb(GF1_IRQ_CTRL,0x0);
  outpb(TMP_GUS_BASE+0x0f,0x0);
  // First do DMA control register
  outpb(GF1_MIX_CTRL,mix_image);
  outpb(GF1_IRQ_CTRL,dma_control|0x80);
  // IRQ CONTROL REG
  outpb(GF1_MIX_CTRL,mix_image|0x40);
  outpb(GF1_IRQ_CTRL,irq_control);
  // First do DMA control register */
  outpb(GF1_MIX_CTRL,mix_image);
  outpb(GF1_IRQ_CTRL,dma_control);
  // IRQ CONTROL REG
  outpb(GF1_MIX_CTRL,mix_image|0x40);
  outpb(GF1_IRQ_CTRL,irq_control);
  // IRQ CONTROL, ENABLE IRQ
  // just to Lock out writes to irq\dma register ...
  outpb(GF1_VOICE_SELECT,0);
  // enable output & irq, disable line & mic input
  mix_image|=0x09;
  outpb(GF1_MIX_CTRL,mix_image);
  // just to Lock out writes to irq\dma register ...
  outpb(GF1_VOICE_SELECT,0x0);
}

// GUS memory handling: ----------
void GUS_c::MemInit(void)
{ // nothin yet ...
}

// channel specific control: --------------------------------------------------

// GUS-sample-speed depends on number of active channels !
/* The formula for this table is:
	1,000,000 / (1.619695497 * # of active voices)

	The 1.619695497 is calculated by knowing that 14 voices
		gives exactly 44.1 Khz. Therefore,
		1,000,000 / (X * 14) = 44100
		X = 1.619695497
*/
static WORD freq_divisor[19] = {
	44100,          /* 14 active voices */
	41160,          /* 15 active voices */
	38587,          /* 16 active voices */
	36317,          /* 17 active voices */
	34300,          /* 18 active voices */
	32494,          /* 19 active voices */
	30870,          /* 20 active voices */
	29400,          /* 21 active voices */
	28063,          /* 22 active voices */
	26843,          /* 23 active voices */
	25725,          /* 24 active voices */
	24696,          /* 25 active voices */
	23746,          /* 26 active voices */
	22866,          /* 27 active voices */
	22050,          /* 28 active voices */
	21289,          /* 29 active voices */
	20580,          /* 30 active voices */
	19916,          /* 31 active voices */
	19293}          /* 32 active voices */
;

// set sample play frequency on active channel
static WORD GUS_SetFrequency(DWORD frequency,int voices)
{ WORD fc;
  DWORD temp;
  // FC is calculated based on the number of active channels(voices) ...
  temp=freq_divisor[voices-14];
  fc=(((frequency<<9L)+(temp>>1L))/temp);
  GF1OutW(SET_FREQUENCY,fc<<1);
  return (fc<<1);
}

static void GUS_FadeVolume(WORD start,WORD end) // thx mikmod
{ WORD temp;
  BYTE rate=0x3f; // calculating is not needed - make it as fast as possible
  BYTE mode=0;

  if(start==end) return;
  /*********************************************************************
  * If the start volume is greater than the end volume, flip them and
  * turn on decreasing volume. Note that the GF1 requires that the
  * programmed start volume MUST be less than or equal to the end
  * volume.
  *********************************************************************/
  /*if(start>end)
  { temp=start;start=end;end=temp;
    mode |= VC_DIRECT;              // decreasing volumes
  }*/
  /* looping below 64 or greater that 4032 can cause strange things */
  if(end>4032*16) end=4032*16;
  if(start<64*16) start=64*16;

  //GF1OutW(SET_VOLUME,start);

  GF1OutB(SET_VOLUME_RATE,rate);
  GF1OutB(SET_VOLUME_START,start>>8);
  GF1OutB(SET_VOLUME_END,end>>8);

  //we don't use rollover IRQs
  //BYTE vmode=GF1InB(GET_VOLUME_CONTROL);
  //if(vmode&VC_ROLLOVER) mode|=VC_ROLLOVER; // what ever...
  GF1OutB(SET_VOLUME_CONTROL,mode);
  GUS_delay();
  GF1OutB(SET_VOLUME_CONTROL,mode);
}

static void GUS_SetPanning(BYTE pan)
{ GF1OutB(SET_BALANCE,pan);
}

static inline void GUS_SetChannel(BYTE chn)
{ // Select channel:
  outpb(GF1_VOICE_SELECT,chn);
  outpb(GF1_VOICE_SELECT,chn);
  outpb(GF1_VOICE_SELECT,chn);
  GUS_delay();
  outpb(GF1_VOICE_SELECT,chn);
}

void GUS_StartChannel(BYTE vmode)
{ // start tha'voice
  /* NOTE: no irq's from the voice ... */
  GF1OutB(SET_CONTROL,vmode);
  GUS_delay();
  GF1OutB(SET_CONTROL,vmode);
}

static DWORD GUS_getsampleposition()
{ DWORD w1,w2;
  BYTE m;
  w1=GF1InW(GET_ACC_HIGH);
  w2=GF1InW(GET_ACC_LOW);
  m=GF1InB(GET_CONTROL);
  return physical_address(w1,w2,m);
}

static void GUS_setsampleposition(DWORD phys_startadr)
{
  GF1OutW(SET_ACC_LOW,ADDR_LOW(phys_startadr));
  GF1OutW(SET_ACC_HIGH,ADDR_HIGH(phys_startadr));
}
