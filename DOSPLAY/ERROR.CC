
#include "stdio.h"

int plyerror=0;

const char * errormsgstr[] =
{ "No error\n",
  "%s: Error in command line. Use -h\n",
  "%s: Device failed. Check config.\n",
  "%s: Module loading failed. Check command line.\n"
};

void player_errormsg(char * head)
{ printf(errormsgstr[plyerror],head);
}

int player_error() { return plyerror; }
