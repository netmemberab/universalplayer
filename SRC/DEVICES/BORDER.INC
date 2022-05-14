#ifdef __GNUC__
#include <pc.h>
#define inp(a)  inportb(a)
#define outp(a,b) outportb(a,b)
#endif

static inline void setborder(BYTE color)
{ outp(0x3c0,0x31);
  outp(0x3c0,color);
}
