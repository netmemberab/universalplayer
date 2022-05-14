/****************************************************************************
   ENDIAN - conversion routines
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 7.6.1995 // it's about 35 degrees celsius today, *uuuhh*
****************************************************************************/
/* some compilers (e.g. TC) do not have an endian-lib .. that's why I did
   wrote this lib optimal designed for my requirements 
*/

#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include "mtypes.h"

// the following 4 functions are used internally don't call them direct,
// use the macros (they are maschine depended defined) !
void rv_w(WORD *w);
void rv_dw(DWORD *w);
void rv_ws(WORD *w,int n);
void rv_dws(DWORD *w,int n);

// ------------------------------------------------------------------------
// THE FOLLOWING CONVERSION ROUTINES ARE USED IN MODULE LOADERS, NOT WHILE
// PLAYING ! SO WE DON'T REALLY CARE ABOUT IF THEY'RE HIGHLY OPTIMIZED IN
// EVERY DETAIL
// ------------------------------------------------------------------------
#ifndef  __BIG_ENDIAN__
// *****************************************************************************
// MACROS FOR LITTLE ENDIAN-MACHINES like intels x86, vax
// *****************************************************************************
  // standard is BIGENDIAN  (lo,hi)-format
  #define __LITTLE_ENDIAN__
  // empty because they don't have to do something and do not return anything
  #define lit2m_w(w)
  #define lit2m_dw(w)
  #define lit2m_ws(w,n)
  #define lit2m_dws(w,n)

  // macros do point to the reverse routines
  #define big2m_w   rv_w
  #define big2m_dw  rv_dw
  #define big2m_ws  rv_ws
  #define big2m_dws rv_dws

#else
// ****************************************************************************
// MACROS FOR BIG ENDIAN-MACHINES like sun-stations, 68000
// ****************************************************************************

  // for all those big endian maschines:
  // empty because they don't have to do something and do not return anything
  #define big2m_w(w)
  #define big2m_dw(w)
  #define big2m_ws(w,n)
  #define big2m_dws(w,n)

  // macros do point to the reverse routines
  #define lit2m_w   rv_w
  #define lit2m_dw  rv_dw
  #define lit2m_ws  rv_ws
  #define lit2m_dws rv_dws

#endif

#endif
