/****************************************************************************
   DOS no sound device
  --------------------------------------------------------------------------
  (c) copyright 1997 by A.Baresel

  created: 20.01.1997
****************************************************************************/

#include "player.h"
#include "module.h"
#include "devices/dos_nosd.h"
#include "arch/dos/dos_irq.h"
#include "instrmts/sample.h"

#include <string.h>
#include <dos.h>

#include "border.inc"

volatile static DWORD timercounter; // used for timing nosound->newnotes() calls
volatile static int playing;
void NoSound_IRQ_handler(UserData);   // timer for playing


NoSound_c::NoSound_c(DeviceOptionsPtr opts):DeviceClass()
{ trace("init ok.\n");
}

NoSound_c::~NoSound_c(void)
{ Stop();
}

void NoSound_c::SetupChannel(BYTE chn,ChannelStruct&)
{
}

Status NoSound_c::Configure(DeviceSettingsPtr)
{
}

void NoSound_c::SetupChannels(ChannelSettings&)
{
}

struct NoSound_IRQ_data { 
		NoSound_c * nosnd;
		ModuleClass * Module;
};

static int timerflag;

Status NoSound_c::Start(ModuleClass * Module)
{ Module->StartModule();
  trace("start nosound\n");

  // program timer:
  timerflag=0;
  { NoSound_IRQ_data * data = new NoSound_IRQ_data;
    data->nosnd=this;data->Module=Module;
    timerid=IRQ_Manager.ReserveTimer(1000,NoSound_IRQ_handler,data,0);
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
  trace("END.    nosound::Start\n");
  return NO_ERROR;
}

Status NoSound_c::Stop(void)
{ playing=0; // no more counting in IRQ
}

Status NoSound_c::Continue()
{ playing=1;
}

// --------------------------------------------------------------------
// Instrument allocation - simply load 'dem to nowhere
// --------------------------------------------------------------------
ResourceID NoSound_c::AllocateInstruments(InstrumentIDs ins,int noi)
{ ResourceID rid;
  InstrumentID iid;
  DWORD i,j;
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
  } trace("\n");
  return rid;
}

Status NoSound_c::FreeInstruments(ResourceID id)
{ return NO_ERROR;
}

// --------------------------------------------------------------------
// Uploading does load data to nowhere
// --------------------------------------------------------------------
Status NoSound_c::UploadInstrument(InstrumentID Iid,InstrumentLoaderClass& loader)
{ int rscno=(Iid>>12)&0xff;
  Status st;
  // set output format ...
  SampleType ss=SMP_UNSIGNED_8BIT;
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

  DWORD size=sp.size,len;
  BYTE SampleBuffer[16*1024U];

  while (size>0)
  { len=(size>16*1024U)?16*1024U:size;
    if (loader.Load(Iid,(DataPtr*)&SampleBuffer,len))
      { trace("error reading sample.\n");return(ERRORLOADING); }
    size-=len;
  }
  return NO_ERROR;
}

// flags for hardware changes (to set them only once per call !)

void  NoSound_c::newnotes(ModuleClass * Module)
{ int i;
  DeviceCommandsStruct devcmd;
  memset(&devcmd,0,sizeof(devcmd));

  devcmd.timerbase=1193181; // timer base in Hz
  Module->GetDeviceCommands(devcmd);

  DWORD counter=devcmd.countervalue;
  if (counter>65535) { trace("no-sound support error...big timer val...\n"); exit(1); }
  IRQ_Manager.ReprogramTimerCnt(timerid,counter);
}

void NoSound_IRQ_handler(UserData ud) // timer for playing
{ timerflag=1;
  /*
  NoSound_IRQ_data * data=(NoSound_IRQ_data*)ud;
  if (!playing) return;
  if (data->nosnd!=NULL) data->nosnd->newnotes(data->Module);
  */
}

