/// DOES NOT WORK !!

/****************************************************************************
   Filter class for MixDevice in addition to MixDevice class
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 16.04.1996
****************************************************************************/

#include "filter.h"
#include "global.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Filter_c::Filter_c(class MixDevice_c *p_device, int type, int options)
{ 
  filter_len=5;
  recurse = 0; // simple FIR-filterI

  para[0]= - 32767 / 12;
  para[1]= + 32767 / 6;
  para[2]= + 32767 / 3;
  para[3]= + 32767 / 6;
  para[4]= - 32767 / 12;
  
  Mixer_p=p_device;
  reset();
  trace("Filter initialized.\n");
}

void Filter_c::reset(void)
{ memset(&in_buf, 0, sizeof(in_buf));
  memset(&out_buf, 0, sizeof(out_buf));
}

/* Memaccess optimized filter function: */

void Filter_c::do_filter(void * buffer, long size)
{ BYTE * mptr, *ptr, *out_ptr;
  short * parptr;
  long result; int out_c;
  int mcount=filter_len, count;
  out_ptr=(BYTE*)&out_buf;

  /* filter the first 'filter_len' values and store them in out_buf */
  while (mcount&&size--)
  { ptr=(BYTE*)&in_buf;count=mcount; parptr=&para[0];
    result=0;
/**/while (count) { result+=((long)*(parptr++))*((int)(*ptr++)-128); count--; }
    count=filter_len-mcount;ptr=(BYTE*)buffer; 
/**/while (count) { result+=((long)*(parptr++))*((int)(*ptr++)-128); count--; }
    *(out_ptr++)=(BYTE)((result/32767)+128);
    mcount--;
  }

  // use out_buf as ring buffer and move values to real output 'buffer'

  out_c=0;mptr=(BYTE*)buffer;
  if (size>0)
  while (size--)
  { ptr=mptr; count=filter_len;
    result=0; parptr=&para[0];

    while (count--) result+=((long)*(parptr++))*((int)(*ptr++)-128);
    if (!out_c)         // counter for ring buffer
    { out_c=filter_len;
      out_ptr=(BYTE*)&out_buf;
    } else { out_ptr++; out_c--; }
    *(mptr++)=*out_ptr;   // store old value to real output buffer
    *out_ptr=(BYTE)((result/32767)+128); 
  }

  
  /* store input values for next call, and copy the last values to real output
     'buffer' */
  memcpy(&in_buf,mptr,filter_len);
  count=filter_len;
  while (count--)
  { if (!out_c)         // counter for ring buffer
    { out_c=filter_len;
      out_ptr=(BYTE*)&out_buf;
    } else { out_ptr++; out_c--; }
    *mptr++=*out_ptr;
  }
}
