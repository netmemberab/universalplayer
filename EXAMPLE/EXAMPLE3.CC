/****************************************************************************
   example - an example player for SoundBlaster-Card (+compatibles)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 20.10.1996
****************************************************************************/
#include <stdio.h>
#include "player.h"
#include "registry.h"
#include "devices/dos_sb.h"
#include "modules/modfile.h"

// ### registering SB/MOD support:
REGISTER_DEVICE( USE_SB, SB_c );
REGISTER_MODULE( LOAD_MOD, MOD_c );

// ### The only one instance of PlayerControl:
PlayerControlClass player;

// ### setup options for SB Hardware:
Mixer_c * mixer=new Stereo13_c;
SB_HardwareOptionStruct opts = { 0x220,5,1,5,mixer };

void main(int argc,char *argv[])
{ DeviceID dev;
  ModuleID song;

  if (argc<2) { printf("%s MOD-filename\n",argv[0]);exit(0); }

 /**1**/
     dev = player.InstallDevice( USE_SB,(DeviceOptionsPtr)&opts); 
     if (dev==0) exit(1); // install went wrong

 /**2**/
     song = player.LoadModule(dev, LOAD_MOD, argv[1]);
     if (song==0) exit(1); // loading went wrong

  /**3**/
     if (player.StartPlay(dev,song)==NO_ERROR) 
     { printf("now playing...(press <return> to finish)\n");
       getchar();
     } else printf("start playing failed...\n");

  delete mixer;  
}
