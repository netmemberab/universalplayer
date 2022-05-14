/****************************************************************************
   player with userfriendly interface for dos
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 20.10.1996
****************************************************************************/
#include <stdio.h>
#include "player.h"
#include "registry.h"
#include "devices/dos_awe.h"
#include "devices/dos_sb.h"
#include "devices/dos_gus.h"
#include "modules/modfile.h"
// local headers:
#include "keyb.h"
#include "display.h"
#include "error.h"

// ### registering SoundBlasterAWE/SB/GUS/MOD support:
REGISTER_DEVICE( USE_SBAWE, SBAWEclass );
REGISTER_DEVICE( USE_SB, SB_c );
REGISTER_DEVICE( USE_GUS, GUS_c );
REGISTER_MODULE( LOAD_MOD, MOD_c );

// ### The only one instance of PlayerControl:
PlayerControlClass player;

Mixer_c* mixer=new Mono13_c;

SBAWE_HardwareOptionStruct aweopts={ 0x220,0x620,AWE32_DRAM_START,DETECT_AWE32_MEM };
GUS_HardwareOptionStruct   gusopts={ 0x240,3,3,7,7 };
SB_HardwareOptionStruct    sbopts={ 0x220,5,1,5,mixer };

DeviceID choose_device(void)
{ DeviceID dev;
  // modify this:
  dev = player.InstallDevice( USE_SB,(DeviceOptionsPtr)&sbopts);
  if (dev==0) 
  { plyerror=PLY_DEVICE_FAILED;exit(1); }
  return dev;
}

void main(int argc,char *argv[])
{ DeviceID dev;
  ModuleID song;

  if (argc<2) { plyerror=PLY_BAD_COMMANDLN;exit(0); }

  dev=choose_device();

  /* ##### Load the Song ###### */
  song = player.LoadModule(dev, LOAD_MOD, argv[1]);
  if (song==0) { plyerror=PLY_MODLOADFAIL;exit(1); } // loading went wrong 

  /* ####### initialize text interface ######## */
  if (display.failed()) exit(1); // initializing display went wrong

  /* ###### Start Playing ##################### */
  if (player.StartPlay(dev,song)==NO_ERROR)
  { printf("now playing...(press <return> to finish)\n");
    while (!keypressed(KB_escape)) ;
  } else printf("start playing failed...\n");

  // close'n'destoy will happen automaticly
}
