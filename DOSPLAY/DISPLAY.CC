/******************************************************************************
  Display Driver Class - implementation
 -----------------------------------------------------------------------------
 copyright 1997 by A.Baresel
******************************************************************************/

#include "display.h"
#include <conio.h>
#include "error.h"

Display display;

Display::Display() // open display and set defaults
{ 
  gppconio_init();
  textcolor(WHITE);
  textbackground(BLUE);
  clrscr();
  window(1,1,80,10);
  textbackground(WHITE);
  textcolor(BLACK);
  clrscr();
  printf(" ** DOS PLAY by Andr‚ Baresel **\n");
}

Display::~Display() // close display, clear screen, output copyrights
{ textcolor(LIGHTGRAY);
  textbackground(BLACK);
  clrscr();
  if (player_error()) player_errormsg("ERROR:");
  printf("DOS-PLAY by Andr‚ Baresel copyright 1997\n");
}

void Display::ChangeView(int no) // switch to new output screen
{ viewno=no;
  // do prepare view ...
}

void Display::RefreshChannelView()
{
}

void Display::RefreshNoteView()
{
}
	
void Display::RefreshInstrumentView()
{
}

void Display::DoRefresh()
{
}

void RefreshDisplay_call(UserData p)
{ Display * dsp=(Display*)p;
  dsp->DoRefresh();
}
