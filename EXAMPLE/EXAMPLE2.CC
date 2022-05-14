/****************************************************************************
   example - an example player for GUS-Card (simple enough ?)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 20.10.1996
****************************************************************************/
#include <stdio.h>
#include "player.h"
#include "registry.h"
#include "devices/dos_gus.h"
#include "modules/modfile.h"

// ### registering GUS/MOD support:
REGISTER_DEVICE( USE_GUS, GUS_c );
REGISTER_MODULE( LOAD_MOD, MOD_c );

// ### The only one instance of PlayerControl:
PlayerControlClass player;

// ### setup options for GUS Hardware:
GUS_HardwareOptionStruct opts={ 0x240,3,3,7,7 };

void main(int argc,char *argv[])
{ DeviceID dev;
  ModuleID song;

  if (argc<2) { printf("%s MOD-filename\n",argv[0]);exit(0); }

  /* ###### Step 1 ##################### */
     dev = player.InstallDevice( USE_GUS,(DeviceOptionsPtr)&opts); 
     if (dev==0) exit(1); // install went wrong

  /* ###### Step 2 ##################### */
     song = player.LoadModule(dev, LOAD_MOD, argv[1]);
     if (song==0) exit(1); // loading went wrong

  /* ###### Step 3 ##################### */
     if (player.StartPlay(dev,song)==NO_ERROR) 
     { printf("now playing...(press <return> to finish)\n");
       getchar();
     } else printf("start playing failed...\n");
  
}
