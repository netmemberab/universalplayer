/****************************************************************************
   ENDIAN - conversion routines
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 7.6.1995 // it's about 35øC today, *uuuhh*
****************************************************************************/
#include "player.h"
#include "arch\xendian.h"

//
// function for complete reverse order - THE Difference -
//

void rv_w(WORD *w)
  { BYTE b;
    b=*(((BYTE*)w)+1);
    *((BYTE*)w+1)=*(BYTE*)w;
    *(BYTE*)w=b;
  }

void rv_dw(DWORD *w)
  { WORD ew;
    big2m_w((WORD*)w+1);
    big2m_w((WORD*)w);
    ew=*((WORD*)w+1);
    *((WORD*)w+1)=*((WORD*)w);
    *(WORD*)w=ew;
  }

void rv_ws(WORD *w, int n)
  { BYTE b;
    BYTE *u0=(BYTE*)w,*u1=((BYTE*)w)+1;
    while (n-->0)
    { b=*u1;
      *u1=*u0;
      *u0=b; u0+=2;u1+=2;
    }
 }

void rv_dws(DWORD *w, int n)
  { BYTE b;
    BYTE *u0=(BYTE*)w,*u2=((BYTE*)w)+2;
    while (n-->0)
    { b=*u0; *u0=*(u2+1); *(u2+1)=b;
      b=*u2; *u2=*(u0+1); *(u0+1)=b; u2+=4; u0+=4;
    }
  }

#ifndef __BIGENDIAN__
// *****************************************************************************
// FUNCTIONS FOR LITTLE-ENDIAN-MACHINES like intels x86 series, VAX
// *****************************************************************************


#else
// *****************************************************************************
// FUNCTIONS FOR LITTLEENDIAN-MACHINES like sun-stations
// *****************************************************************************

void lit2m_w(WORD *w)
  { BYTE b;
    b=*(((BYTE*)w)+1);
    *((BYTE*)w+1)=*(BYTE*)w;
    *(BYTE*)w=b;
  }

void lit2m_dw(DWORD *w)
  { WORD ew;
    big2m_w((WORD*)w+1);
    big2m_w((WORD*)w);
    *((WORD*)w+1)=*((WORD*)w);
    *(WORD*)w=ew;
  }

void lit2m_ws(WORD *w, int n)
  { BYTE b;
    BYTE *u0=(BYTE*)w,*u1=((BYTE*)w)+1;
    while (n-->0)
    { b=*u1;
      *u1=*u0;
      *u0=b; u0+=2;u1+=2;
    }
 }

void lit2m_dws(DWORD *w, int n)
  { BYTE b;
    BYTE *u0=(BYTE*)w,*u2=((BYTE*)w)+2;
    while (n-->0)
    { b=*u0; *u0=*(u2+1); *(u2+1)=b;
      b=*u2; *u2=*(u0+1); *(u0+1)=b; u2+=4; u0+=4;
    }
  }

#endif
