/****************************************************************************
   LINUX - Audio support (any SampleDevices supported by LinuxSoundSystem)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 13.01.1996
****************************************************************************/

#include "linux_au.h"
#include "global.h"
#include "mixdev.h"
#include "mixer.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <stdlib.h>
#include "display.h"

#define BUFFERSIZE 8192

void Audio_c::playing(void)
{
  for (;domix;) 
  { module_wprint();
    mixdevice_wprint();
    refresh_scr();
    mixing(buffer,BUFFERSIZE);
    write(Audio,buffer,BUFFERSIZE);
  }
}

short Audio_c::StartPlay(void)
{ 
  Module->startmodule();
  MixDevice_c::StartPlay();
  domix=1;
  playing();
  return(0);
}

short Audio_c::StopPlay(void)
{ 
  domix=0;
  return(0);
}

Audio_c::Audio_c( long _SampleRate, short _AudioBits, short _AudioChannels,
                  short _AudioFormat, short _AudioMode ):MixDevice_c()
{
  Audio = open("/dev/dsp", O_WRONLY, 0);

  if (Audio == -1)
  {
    perror("Error opening audio device");
    status=NO_DEVICE;
  }
  else
  { int stereo_f=(_AudioChannels==2);
    if (_AudioChannels!=1&&_AudioChannels!=2)
    { printf("Error: Linux-Audio sample devices does only support 1 or 2 Audiochannels (mono/stereo).\n");
      status=-1;
      return;
    }
    trace("Initialize audiodevice.\n");

    /* nice feature of linux audio device: */
    { int frag = 0x00060008;  /* 6 fragments of 2^8=256 bytes */
      ioctl(Audio, SNDCTL_DSP_SETFRAGMENT, &frag);
    } 

    /* check samplesize */
    AudioBits=_AudioBits;
    if (ioctl(Audio, SNDCTL_DSP_SAMPLESIZE, &AudioBits) == -1)
      AudioBits=8;
    #ifdef __DEBUGGING__
    printf("Audiobits: %u\n",AudioBits);
    #endif

    /* check stereo/mono */
    AudioChannels=_AudioChannels;
    if (ioctl(Audio, SNDCTL_DSP_STEREO, &stereo_f) == -1)
    {
      perror("Error setting stereo/mono");
      status=-1;
      return;
    }
    AudioChannels=(stereo_f)?2:1;
    #ifdef __DEBUGGING__
    printf("Audiochannels: %u\n",AudioChannels);
    #endif

    /* check samplerate */
    SampleRate=_SampleRate;
    if (ioctl(Audio, SNDCTL_DSP_SPEED, &SampleRate) == -1)
    {
      perror("Error setting frequency");
      status=-1; 
      return;
    }
    #ifdef __DEBUGGING__
    printf("Samplerate: %d\n",(int)_SampleRate);
    #endif

    if ((buffer=(char *)malloc(BUFFERSIZE)) == NULL ) { status=NO_MEM; return; }

    BYTE output_format=(AudioBits==8)?UNSIGNED_8BIT:UNSIGNED_16BIT;
    if (AudioChannels==1)
       Mixer=new Mono13_c(BUFFERSIZE,_AudioMode,output_format);
    else
       Mixer=new Stereo13_c(BUFFERSIZE,_AudioMode,output_format);

  }  
}

