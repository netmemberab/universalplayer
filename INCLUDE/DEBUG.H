/****************************************************************************
   Debugging Info Class Header
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 15.10.1996
****************************************************************************/
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <iostream.h>
/*
   if symbol __DEBUGGING__ is defined, this header declares a class for
   formated debugging output.
   if no symbol __DEBUGGING__ is defined, all methods will be empty inlines
   (no code at all!!)

   DebugInfoClass is a simple form of 'ostream' (only some features).

*/

#ifdef __DEBUGGING__

// use 'start' to start a debug line with.
// use 'type_on/off' to enable/disable type output for all values.
class DebugInfoClass;
typedef DebugInfoClass& (*__dbgmanip)(DebugInfoClass&);

class DebugInfoClass {
        private:
                ostream debug;
                int print_type;
        public:
                DebugInfoClass()           // debuginfo to stdout
                  { debug=cout;print_type=0; }
                DebugInfoClass(FILE);      // debuginfo to FILE
                DebugInfoClass(ofstream&o) // debuginfo to stream
                  { debug=o;print_type=0; }
                inline void enable_print_type() { print_type=1; }
                inline void disable_print_type() { print_type=0; }

                inline DebugInfoClass& operator < (char*s) { debug << s; return *this; }

                inline void TYPE(char*s) { if (print_type) debug < s; };

                inline DebugInfoClass& operator << (char*s) { TYPE("(char*)");debug << s; return *this; }
                inline DebugInfoClass& operator << (unsigned char*b) { TYPE("(BYTE)");debug << (int)b; return *this; }
                inline DebugInfoClass& operator << (signed char*p) { *this << ((char*)p); }
                inline DebugInfoClass& operator << (char&c) { TYPE("(char)");debug << c; return *this; }
                inline DebugInfoClass& operator << (unsigned char&);
                inline DebugInfoClass& operator << (signed char&p) { operator>>((char)p); }
                inline DebugInfoClass& operator << (short&);
                inline DebugInfoClass& operator << (int&);
                inline DebugInfoClass& operator << (long&);
                inline DebugInfoClass& operator << (long long&);
                inline DebugInfoClass& operator << (float);
                inline DebugInfoClass& operator << (double);
                inline DebugInfoClass& operator << (long double);
                inline DebugInfoClass& operator << (__dbgmanip func) { (*func)(debug); }

                inline friend DebugInfoClass& start(DebugInfoClass& dbg)
                  { return dbg < "DEBUG:"; }
                inline friend DebugInfoClass& type_on(DebugInfoClass& dbg)
                  { dbg.enable_print_type(); return dbg; }
                inline friend DebugInfoClass& type_off(DebugInfoClass& dbg)
                  { dbg.disable_print_type(); return dbg; }
};

#else

// empty code: (will be removed due compiler optimizations)
class DebugInfoClass {
        public:
                inline DebugInfoClass() { }
                inline DebugInfoClass(FILE) { }
                inline DebugInfoClass(ofstream&) { }
                inline ~DebugInfoClass() { }

                inline DebugInfoClass& operator << (char*) { return *this; }
                inline DebugInfoClass& operator << (unsigned char*) { return *this; }
                inline DebugInfoClass& operator << (signed char*) { return *this; }
                inline DebugInfoClass& operator << (char&) { return *this; }
                inline DebugInfoClass& operator << (unsigned char&) { return *this; }
                inline DebugInfoClass& operator << (signed char&) { return *this; }
                inline DebugInfoClass& operator << (short&) { return *this; }
                inline DebugInfoClass& operator << (int&) { return *this; }
                inline DebugInfoClass& operator << (long&) { return *this; }
                inline DebugInfoClass& operator << (long long&) { return *this; }
                inline DebugInfoClass& operator << (float) { return *this; }
                inline DebugInfoClass& operator << (double) { return *this; }
                inline DebugInfoClass& operator << (long double) { return *this; }
                inline DebugInfoClass& operator << (__omanip func) { return *this; }

                inline friend DebugInfoClass& start(DebugInfoClass& dbg)
                  { return dbg; }
};

#endif /* DEBUGGING */

#endif /* DEBUG_H   */
