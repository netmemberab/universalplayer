/****************************************************************************
   example - an example player for NoSound-device +compatibles :)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 17.2.1997
  "don't wonder you won't hear anything!"
****************************************************************************/
#include <stdio.h>
#include "player.h"
#include "registry.h"
#include "devices/dos_nosd.h"
#include "modules/modfile.h"

// ### registering NoSouND/MOD support:
REGISTER_DEVICE( USE_NOSOUND, NoSound_c );
REGISTER_MODULE( LOAD_MOD, MOD_c );

// ### The only one instance of PlayerControl:
PlayerControlClass player;

void main(int argc,char *argv[])
{ DeviceID dev;
  ModuleID song;

  if (argc<2) { printf("%s MOD-filename\n",argv[0]);exit(0); }

  /* ###### Step 1 ##################### */
     dev = player.InstallDevice( USE_NOSOUND,(DeviceOptionsPtr)NULL); 
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
