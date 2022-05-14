/******************************************************************************
  Keyboard Driver - header
 -----------------------------------------------------------------------------
 copyright 1997 by A.Baresel
******************************************************************************/

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

typedef int KEY;

#define KB_escape 27

class Keyboard {
  public:
	Keyboard();
	~Keyboard();

  friend int keypressed(KEY);
};

extern Keyboard keyb;

#endif
