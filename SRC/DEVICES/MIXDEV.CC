/****************************************************************************
   MixDevice - Software mixer for all non wavetable soundcards
  --------------------------------------------------------------------------
  (c) copyright 1995 by A.Baresel

  created: 08.07.1996
  redesigned 18.07.1996
  ( mixing routines moved to separat classes on 24.7.1996 )
  reassigned for new player interface: 03.01.1997
****************************************************************************/

#include "player.h"
#include "module.h"
#include "devices/mixdev.h"
#include "instrmts/sample.h"
#include "mtypes.h"

#include <malloc.h>
#include <string.h>

#include "border.inc"

inline InstrumentID form_instrumentID(void *deviceptr,DWORD rscno,DWORD instno)
{ return ((DWORD)deviceptr&0xff00000)+(rscno<<13)+instno; }

inline DWORD get_resourceno(InstrumentID iid)
{ return (iid>>13)&0xf; }

inline DWORD get_instrumentno(InstrumentID iid)
{ return iid&0x1fff; }

inline ResourceID form_resourceID(void *deviceptr,DWORD rscno)
{ return (((DWORD)deviceptr)&~0xffU)+rscno; }

inline DWORD get_resourceno_RSC(ResourceID rid)
{ return rid&0xff; }

MixDevice_c::MixDevice_c():DeviceClass(),Mixer(NULL),Filter(NULL)
{ memset(&sample,0,sizeof(sample)); /* delete all entries in sampleinfo */
  trace("MixDevice initialized.\n");
}

MixDevice_c::~MixDevice_c(void)
{ trace("Mixdevice done.\n");
  /*
  for (j=1;j<MAX_RESOURCES;j++)
  if (samples[j]!=NULL
  for (i=1;i< ..... ;i++)
    free(sample[i].data);
  */
  delete Mixer;
}

Status MixDevice_c::Configure(DeviceSettingsPtr p)
{ MixerSettingsPtr mixset=(MixerSettingsPtr)p;
  SampleRate=mixset->MixFrequency;
  if (mixset->Mixer!=NULL) Mixer=mixset->Mixer;
  if (mixset->Filter!=NULL) Filter=mixset->Filter;

  BitMode=mixset->mixmode & BITMODE;
  StereoMode=mixset->mixmode & STEREOMODE;
  Format=mixset->mixmode;

  if (Mixer!=NULL)
  { trace("change mixer_c.");
    Mixer->setup(mixset->maxbuffer,NO_CHANGES,Format);
  }
}

void MixDevice_c::SetupChannel(BYTE chn,ChannelStruct&)
{
}

void MixDevice_c::SetupChannels(ChannelSettings&)
{
}

Status MixDevice_c::Start(ModuleClass* Module)
{ trace("start mixer.");
  if (Mixer==NULL) return NO_MIXER;
  SamplesLeft=0; // start with new notes !
  Module->StartModule();
  memset(&channel,0,sizeof(channel));
  #ifdef __DEBUGGING__
  trace("Start mixer ok.");getchar();
  #endif
  return NO_ERROR;
}

/* ------ =================================================== ------
   #####  Instrument handling: allocating, uploading, freeing  #####
   ------ =================================================== ------
*/
ResourceID MixDevice_c::AllocateInstruments(InstrumentIDs ins,int noi)
{ int i=0,j;
  ResourceID rid;
  InstrumentID iid;
  if (noi>MAX_SAMPLES_PER_RSC) { status=OUT_OF_RANGE;return NO_RESOURCE; }
  while (i<MAX_RESOURCES && sample[i]!=NULL) i++;
  if (i==MAX_RESOURCES) { status=NO_RESOURCES;return NO_RESOURCE; }
  sample[i]= new SampleResourceStruct;
  trace("got resource.\n");
  if (sample[i]==NULL) { status=NO_RESOURCES;return NO_RESOURCE; }
  (sample[i])->no_samples=0;
  if (((sample[i])->smpinfo=new SampleStruct[noi])==NULL) { status=NO_MEMORY;return NO_MEMORY; }
  if ((sample[i])->smpinfo==NULL) { status=NO_RESOURCES;return NO_RESOURCE; }
  (sample[i])->no_samples=noi;
  trace("got entries.\n");
  memset(sample[i]->smpinfo,0,noi*sizeof(SampleStruct));
  rid=form_resourceID(this,i);
  // generate IDs
    // Idea:
    //    resource= (id>>12)&0xff
    //    sampleno=  id&0x3fff

  trace("Device reserves:\n");
  for (j=0;j<noi;j++) 
  { *ins++=form_instrumentID(this,i,j);
    #ifdef __DEBUGGING__
      printf("%li:",*(ins-1));
    #endif
  }
  #ifdef __DEBUGGING__
    printf("rid: %i\n",rid);
  #endif
  return rid;
}

Status MixDevice_c::FreeInstruments(ResourceID id)
{ int i=get_resourceno_RSC(id);
  int j;
  if (sample[i]==NULL) return BAD_ID;
  if (j=(sample[i])->no_samples) delete ((sample[i])->smpinfo);
  delete (sample[i]);
  sample[i]=NULL;
  return NO_ERROR;
}

Status MixDevice_c::UploadInstrument(InstrumentID Iid,InstrumentLoaderClass& loader)
{ int rscno=get_resourceno(Iid);
  DWORD instno=get_instrumentno(Iid);
  Status st;
  #ifdef __DEBUGGING__
    printf("access resource %i\n",rscno);
  #endif
  if (sample[rscno]==NULL || instno>sample[rscno]->no_samples) return BAD_ID;
  trace("upload id ok\n");
  SampleStruct * smpptr= &sample[rscno]->smpinfo[ instno ];

  SampleType ss=SMP_UNSIGNED_8BIT;
  if ((st=loader.SetOutputFormat(IT_SAMPLE,&ss))!=NO_ERROR)
    return st; // instrument cannot be converted to the requested format...
  // if 16bit support is ready:
  /*
  *  SampleType s1=SMP_UNSIGNED_8BIT;
  *  SampleType s2=SMP_UNSIGNED_16BIT;
  *  InstrumentSpec *is=new InstrumentSpec[2];
  *  is[0]->UserTypePtr=&s1;is[0]->InstrumentType=IT_SAMPLE;
  *  is[1]->UserTypePtr=&s2;is[1]->InstrumentType=IT_SAMPLE;
  *  if (loader.SetPreferedOutputFormat(is,2)!=NO_ERROR)
  *  { // maybe SetPrefered is not implemted:
  *    if (loader.SetOutputFormat(IT_SAMPLE,&s1)!=NO_ERROR)
  *      if ((st=loader.SetOutputFormat(IT_SAMPLE,&s2))!=NO_ERROR)
  *        return st; // instrument cannot be converted to the requested format...
  *  }
  */

  // get sample specs ...
  SampleSpec sp;
  loader.GetInstrumentSpecs(Iid,(UserSpecPtr)&sp);
  #ifdef __DEBUGGING__
    printf("Sample: (%li) %li-%li\n",sp.size,sp.loopstart,sp.loopend);
  #endif
  if (sp.loopflag&LOOP_MODE && sp.loopend-sp.loopstart<500) getchar();
  smpptr->id=Iid;
  if (smpptr->data!=NULL) free(smpptr->data);
  if ((smpptr->data=(BYTE*)malloc(sp.size+1))==NULL) 
  { trace("Not enough memory.\n");
    return(status=NO_MEMORY);
  }
  memset(smpptr->data,128,sp.size+1);smpptr->endpos=sp.size;
  if (loader.Load(Iid,(DataPtr*)smpptr->data,sp.size))
    { trace("error reading sample.\n");return(ERRORLOADING); }
  smpptr->looptype=sp.loopflag;
  /*if (sp.type==SMP_UNSIGNED_8BIT)*/ smpptr->bits=8; 
  /* else smpptr->bits=16;*/
  if (sp.loopflag&LOOP_MODE)
  { smpptr->loopstart=sp.loopstart;
    smpptr->loopend=sp.loopend;
  } else
  { smpptr->loopstart=0;
    smpptr->loopend=0;
  }
  return NO_ERROR;
}

void  MixDevice_c::newnotes(DeviceCommandsStruct& devcmd)
{
  int i;

  for (i=0;i<devcmd.no_commands;i++)
  { SampleStruct * smpptr= &(sample[channel[i].instrsc]->smpinfo[channel[i].instno]);

    if (devcmd.Commands[i].Changes&CHG_STOPPLAY)
    { // stop playing instrument emmediatly, but store position where it stays
      // currently
      channel[i].mixchn=0; // stop mixing immediatly
    }

    if (devcmd.Commands[i].Changes&CHG_INSTRUMENT)    // set instrument data
    { // another instrument is used on this channel up to now
      // (stop playing current one, set internal instrument settings
      //  it will be used in next CHG_FREQUENCY)
      InstrumentID iid=devcmd.Commands[i].instrid;
      ResourceID irsc=get_resourceno(iid);
      DWORD ino=get_instrumentno(iid);
      if (iid!=NO_INSTRID)
      { channel[i].instrid = iid;
	channel[i].instrsc = irsc;
	channel[i].instno  = ino;

  	smpptr= &(sample[irsc]->smpinfo[ino]);
        channel[i].offset    = 0;
        channel[i].frac_ofs  = 0;
        channel[i].sampleend = smpptr->endpos;
        channel[i].loopstart = smpptr->loopstart;
        channel[i].loopend   = smpptr->loopend;
        channel[i].looptype  = smpptr->looptype;
        channel[i].volume    = 63;
        channel[i].ptr       = smpptr->data;
        channel[i].bits      = 8 /*smpptr->bits*/;
      } else devcmd.Commands[i].Changes&=~CHG_INSTRUMENT;
    }

    if (devcmd.Commands[i].Changes&CHG_SETOFFSET) // set offset
    { // start playing the current instrument from this position (given in samples)
      // requested sample offset is: devcmd.Commands[i].Offset=DWORD
      DWORD ofs=devcmd.Commands[i].Offset;
      if (ofs>channel[i].sampleend) channel[i].mixchn=0; // no mixing
      if (channel[i].looptype>0) ; // <- set pos mixdir checking
      channel[i].offset=ofs;
      channel[i].frac_ofs=0;
    }

    if (devcmd.Commands[i].Changes&CHG_PANNING) // panposition does change
    { 
      channel[i].panpos=devcmd.Commands[i].Panning;
    }

    if (devcmd.Commands[i].Changes&CHG_VOLUME)
    { BYTE volume=devcmd.Commands[i].Volume;
      if (volume>64) volume=64;
      channel[i].volume=volume;
    }

    if (devcmd.Commands[i].Changes&CHG_FREQUENCY)
    { // if it comes with a CHG_INSTRUMENT start this instrument with this pitch,
      // requested pitch as frequency: devcmd.Commands[i].frequency
      channel[i].frequency=devcmd.Commands[i].Frequency;
      if (devcmd.Commands[i].Changes&CHG_INSTRUMENT)
      { channel[i].direction=0; // will start running in positiv direction
        channel[i].mixchn = 1 ; // do mixing
      }
      channel[i].bits=8;

      channel[i].step_int=(DWORD)channel[i].frequency/(DWORD)SampleRate;

      // modified for extra high frequencies (>65536Hz)
      DWORD fr=(DWORD)channel[i].frequency%(DWORD)SampleRate;
      if (fr<0x10000LU)
        fr=(fr<<16)/(DWORD)SampleRate;
      else // floating point at high frequencies:
        fr=(DWORD)((float)fr/(float)SampleRate*65536.0);
      channel[i].step_frac=fr<<16;

    }

    if ((devcmd.Commands[i].Changes&CHG_RESTART) &&
	!channel[i].mixchn && (channel[i].step_int+channel[i].step_frac) && channel[i].instrid!=NO_INSTRID)
    { // do only restart if the instrument is currently not playing and
      // does not stay at sample end !
      channel[i].offset   =0;
      channel[i].frac_ofs =0;
      channel[i].direction=0;
      channel[i].mixchn   =1; // do mixing
    }

    if (devcmd.Commands[i].Changes&CHG_CONTINUE)
    { // continue playing at the position where it was stoped with CHG_STOPPLAY
      channel[i].mixchn=1; // do mixing
    }
  }
}

DWORD GetDistance(DWORD steps,DWORD stepint,DWORD stepfrac)
{ return steps*stepint+((steps*(stepfrac>>16))>>16);
}

DWORD CalcSteps(DWORD distance,DWORD stepint,DWORD stepfrac)
// distance in 16.16 bit fixed point format
{ DWORD l=((stepint<<16)+(stepfrac>>16));
  if (l==0) return 1;
  if (distance%l) return 1+distance/l; else return distance/l;
}

void MixDevice_c::mix_chunk(void * Buffer, DWORD nr_samples)
// -----------------> nr_samples number of samples not in bytes !!
//                    (e.g. in 16bit stereo mode one sample is 4 bytes, 
//		       16bit mono only 2 bytes)
{ void * mixbuf;
  BYTE chn;
  DWORD todo;

  { DWORD h=nr_samples; if (StereoMode==MIX_STEREO) h<<=1;
    Mixer->clear_buffer(h);
  }
  setborder(3);

  for (chn=0;chn<MAXCHANNELS;chn++)
  { if (channel[chn].mixchn==1)      /* do it for all active channels */
    { todo=nr_samples;
      mixbuf=Mixer->ptr2buf();
      while (todo>0)
      {
        if (channel[chn].direction==0 && (channel[chn].step_int+channel[chn].step_frac)>0)
        { 
	  SDWORD distance=GetDistance(todo-1,channel[chn].step_int,channel[chn].step_frac);
	  DWORD reallydo=todo;

	  // ---> check if we reach the sample loopend in the next "todo" steps

	  if ((channel[chn].looptype!=MIX_NO_LOOP) && channel[chn].offset+distance>=channel[chn].loopend)
	  { // yes we do! Do only mix until we reach this point !

	    distance=((channel[chn].loopend-channel[chn].offset)<<16)-(channel[chn].frac_ofs>>16);
	    reallydo=CalcSteps(distance,channel[chn].step_int,channel[chn].step_frac);
	    if (reallydo>todo) reallydo=todo;
	  } 
	  else // ---> no we don't, but maybe we reach the sample end ?

	  if ((channel[chn].looptype==MIX_NO_LOOP) && channel[chn].offset+distance>=channel[chn].sampleend)
	  { // yes we really do ! Do only mix until that point !

	    distance=((channel[chn].sampleend-channel[chn].offset)<<16)-(channel[chn].frac_ofs>>16)-1;
	    channel[chn].mixchn=0;       // --> stop mixing after that call
	    reallydo=CalcSteps(distance,channel[chn].step_int,channel[chn].step_frac);
	    if (reallydo>todo) reallydo=todo; else todo=reallydo;
	  }
	  todo-=reallydo;

          if (reallydo>5120L)
	  { printf("OH HOLY SHIT.\n");
	    fflush(stdout);exit(-1);
	  }

          /*  MIXING */ 
	  if (channel[chn].bits==8)
  	    mixbuf=Mixer->add_sample8(mixbuf,(BYTE*)channel[chn].ptr,&(channel[chn].offset),&(channel[chn].frac_ofs),
	           channel[chn].direction,channel[chn].step_int,channel[chn].step_frac,
                   reallydo,channel[chn].volume,channel[chn].panpos);
	  else
  	    mixbuf=Mixer->add_sample16(mixbuf,(WORD*)channel[chn].ptr,&(channel[chn].offset),&(channel[chn].frac_ofs),
	           channel[chn].direction,channel[chn].step_int,channel[chn].step_frac,
		   reallydo,channel[chn].volume,channel[chn].panpos);

	  if ((channel[chn].looptype!=MIX_NO_LOOP) &&
	      channel[chn].offset>=channel[chn].loopend) // reached loppend ?
	  { // what we gonna do depends on loop type:
	    if (channel[chn].looptype==MIX_BIDI_LOOP)
            { channel[chn].direction=1-channel[chn].direction; // reverse direction
              channel[chn].offset=
	        2*channel[chn].loopend-channel[chn].offset;
              channel[chn].frac_ofs=0x0ffffffffLU - channel[chn].frac_ofs;
            }
	    else // normal loop
	    { channel[chn].offset=
	        channel[chn].offset-channel[chn].loopend+channel[chn].loopstart;
	      // jump back to loopstart
	    }
	  }

	} else 
        if (channel[chn].direction==1 && channel[chn].step_int+channel[chn].step_frac>0)
        { // we are in a bidirectional loop in reverse direction
	  SDWORD distance=GetDistance(todo-1,channel[chn].step_int,channel[chn].step_frac);
	  DWORD reallydo=todo;

	  // check if we reach the sample loopstart in the next "todo" steps
	  if (channel[chn].offset<=channel[chn].loopstart+distance)
	  { // yes we do! Do only mix until we reach this point !

	    distance=((channel[chn].offset-channel[chn].loopstart)<<16)+(channel[chn].frac_ofs>>16);
	    reallydo=CalcSteps(distance,channel[chn].step_int,channel[chn].step_frac);
	  } 
	  if (reallydo>todo) reallydo=todo;
	  todo-=reallydo;

          /*  MIXING  */
	  if (channel[chn].bits==8)
  	    mixbuf=Mixer->add_sample8(mixbuf,(BYTE*)channel[chn].ptr,&(channel[chn].offset),&(channel[chn].frac_ofs),
	         channel[chn].direction,channel[chn].step_int,channel[chn].step_frac,
		 reallydo,channel[chn].volume,channel[chn].panpos);
	  else
  	    mixbuf=Mixer->add_sample16(mixbuf,(WORD*)channel[chn].ptr,&(channel[chn].offset),&(channel[chn].frac_ofs),
	         channel[chn].direction,channel[chn].step_int,channel[chn].step_frac,
		 reallydo,channel[chn].volume,channel[chn].panpos);

	  if (channel[chn].offset<=channel[chn].loopstart) // reached loopstart ?
	  { channel[chn].direction=1-channel[chn].direction; // reverse direction
            channel[chn].offset=
	      2*channel[chn].loopstart-channel[chn].offset;
              channel[chn].frac_ofs=0x0ffffffffLU - channel[chn].frac_ofs;
	  }
	} else todo=0; // step is 0 ??? --> fault tolerant computing ;)

      }
    }
  }
  setborder(4);
  { DWORD h=nr_samples; if (StereoMode==MIX_STEREO) h<<=1;
    Mixer->amplify_result(Buffer,h);
  }
  //if (Filter!=NULL) Filter->do_filter(Buffer,(long)nr_samples);
}

Status MixDevice_c::mixing(ModuleClass* Module,void * Buffer, DWORD buffersize)
{ DWORD samplesize;
  //memset(Buffer,0,buffersize);
  samplesize=(StereoMode==MIX_STEREO)?2:1;
  if (BitMode==MIX_16BIT) samplesize<<=1;

  while (buffersize>0)
  {  
    if ( samplesize * SamplesLeft>=buffersize)
      { mix_chunk(Buffer,buffersize/samplesize); 
	SamplesLeft-=buffersize/samplesize; 
	buffersize=0; 
      }
    else
    if (SamplesLeft>0)
    { mix_chunk(Buffer,SamplesLeft); 
      (BYTE *)Buffer+=samplesize*SamplesLeft;
      buffersize-=samplesize*SamplesLeft; 
      SamplesLeft=0;
    }
    else
    { setborder(1); 
      DeviceCommandsStruct devcmd;
      memset(&devcmd,0,sizeof(devcmd));
      devcmd.timerbase=SampleRate;
      Module->GetDeviceCommands(devcmd); // <- here you'll get the information what to do next !
      // first check through timing information
      SamplesLeft=devcmd.countervalue;
      // now do the settings...
      newnotes(devcmd); 
    }
  }
  return NO_ERROR;
}

/*
int MixDevice_c::Initialize_Filter(int type, int options)
{
  Filter=new Filter_c(this,type,options);
  return 0; // is for later error handling if filter was not
            // correctly initialized
}
*/