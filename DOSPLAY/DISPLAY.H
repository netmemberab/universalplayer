/******************************************************************************
  Display Driver Class - header
 -----------------------------------------------------------------------------
 copyright 1997 by A.Baresel
******************************************************************************/

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "arch\dos\dos_irq.h"

#define CHANNEL_VIEW    0
#define NOTE_VIEW       1
#define INSTRUMENT_VIEW 2

class Display {
  public:
        Display();   // open display and set defaults
        ~Display();  // close display, clear screen, output copyrights

        void ChangeView(int no); // switch to new output screen
	int  failed(void) { return 0; }

  friend void RefreshDisplay_call(UserData);

  private:
	int viewno;
	void RefreshChannelView();
	void RefreshNoteView();
	void RefreshInstrumentView();
	void DoRefresh();
};

extern Display display;

#endif
