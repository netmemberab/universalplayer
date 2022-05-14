/****************************************************************************
   Filter class for MixDevice in addition to MixDevice class
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 16.04.1996
****************************************************************************/

#if !defined(__FILTER_H__)
#define __FILTER_H__

#include "mtypes.h"
#include "device.h"

#define MAX_FILTER_LEN 256

class Filter_c {
   protected:
      short in_buf[MAX_FILTER_LEN];
      short out_buf[MAX_FILTER_LEN];
      short para[2*MAX_FILTER_LEN]; // parameter for in/out_buf 
      short filter_len;
      short recurse;                // flag if outbuffer is used to recurse io (IIR-filter) 
      class MixDevice_c * Mixer_p;
   public:
      Filter_c(class MixDevice_c *p_device, int type=0, int options=0);
      void reset(void);
      void do_filter(void * buffer, long size);
};

#endif
