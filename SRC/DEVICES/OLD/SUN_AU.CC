/****************************************************************************
   SOLARIS - Audio support
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 06.06.1996
****************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sun/audioio.h>
#include "sun_au.h"
#include "../global.h"
#include "device.h"



// delete this :
#include <curses.h>
#include "../display/d_curses.h"

void Audio_c::playing(void)
{ 
  for (;;) /* yes - endless ! may be song end control here (later) */
  { module_wprint();
    mixdevice_wprint();
    refresh(); // curses scr-refresh -<- we have to clean up this routine anyway
    mixing(buffer,BUFSIZE);printf(",");
    write(Audio,buffer,BUFSIZE);
  }

}

short Audio_c::StartPlay(void)
{ 
  Module->startmodule();
  MixDevice_c::StartPlay();

  /* forking here ? but, for the first do not return ... simply call the 
     endless loop in: 
	Audio_c::playing(); 
  */

  playing();
  return(0);
}

short Audio_c::StopPlay(void)
{ /* take advantage of things in StartPlay() */
  return(0);
}

short Audio_c::LoadInstrument(short no,DWORD size,BYTE bits,BYTE loopflag,DWORD loopbeg)
{
  return(LoadInstrument_Mem(no,size,bits,loopflag,loopbeg));
}

struct audio_info stuff;
int dev;
struct audio_info info;

Audio_c::Audio_c():MixDevice_c()
{
  Audio = open("/dev/audio", O_WRONLY);

  if (Audio == -1)
  {
    perror("Error opening audio device");
    status=NO_DEVICE;
  }
  else
  {     
     ioctl(Audio, AUDIO_GETINFO, &stuff);
     ioctl(Audio, AUDIO_GETDEV, &dev);
     switch (dev)
     { case AUDIO_DEV_UNKNOWN: /* unknown - error */ break;
       case AUDIO_DEV_AMD    : 
       case AUDIO_DEV_SPEAKERBOX:
       case AUDIO_DEV_CODEC: 
	{
		AUDIO_INITINFO(&info);
		info.play.sample_rate = 22050;
		info.play.precision = 8;
		info.play.encoding = AUDIO_ENCODING_NONE;
		AudioChannels = 1;
		info.play.channels = 1;
		if (ioctl(Audio, AUDIO_SETINFO, &info) != 0)
		     { status=NO_DEVICE; return; }
	}
      } 
    }

    SampleRate= info.play.sample_rate;



    return;

}

