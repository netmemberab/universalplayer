/****************************************************************************
   rm_dma: DMA accessing routines for DOS in realmode
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel
  created: 27.05.1996  rewritten: 21/22.06.1996 (interface redesigned)
****************************************************************************/

#include <mem.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include "dma.h"

#define outpb(a,b) outp(a,b)
#define inpb(a)    inp(a)

//
// Attention: implemenation of real and protected mode differ in some
//            important details !
//

struct t_dmabuf_data { void * realbuf;   // real buffer where dma buffer is located
                       void * dmabuffer; // ptr to start of dma buffer
                                         // somewhere inside realbuf
                       unsigned long linaddr; // linear adress of dma buffer
                       int partlen;   // length of one dma buffer part == size of buffer
                       int parts;     // number of dma buffer parts
                     } dmabuf;

int dmabuf_alloc(int partlen,int parts)
/*
 * Allocates an DMA-buffer of size: partlen*bufparts bytes
 * real mode:
 * realbuf is located in data seg, dmabuffer is located in realbuf, but
 * start might differ for page override reasons...
 *
 * returns errorcode (0: OK)
 */
{ dmabuf.partlen=partlen;
  dmabuf.parts=parts;
  if ((long)partlen*parts>65535l) return -1; // maximum size of dmabuffer is 64KB
  // allocate realbuf with size = 2*parts*partlen
  if ((dmabuf.realbuf=malloc(partlen*parts+15))==NULL) return -1;
  // calculate linear adress out of dos seg:ofs adress
  dmabuf.linaddr=(((unsigned long)FP_SEG(dmabuf.realbuf)<<4))+FP_OFF(dmabuf.realbuf);
  dmabuf.dmabuffer=dmabuf.realbuf;
  // move to next offset 0:
  if (dmabuf.linaddr&0x0000fl)
    dmabuf.linaddr=(dmabuf.linaddr&0xffffff0l)+0x10;
  // check for page overides: (we do not allow 64KB overides, also in case
  // of 16bit transfers where only 128KB overides are not allowed...
  // (in fact we do not know here, if we'll later use 16bit or 8bit transfers)
  if ((dmabuf.linaddr&0xffff)+parts*partlen>0xffff) // OVERRIDE! move to next (page:0)
    dmabuf.linaddr=(dmabuf.linaddr&0xff0000l)+0x10000l;
  dmabuf.dmabuffer=MK_FP((unsigned)(dmabuf.linaddr>>4),0);
  return 0;
}

void * dmabuf_getptr(int part)
/*
 * this routine returns a ptr to the specified dma buffer part (0..parts)
 */
{ part%=dmabuf.parts;
  return (void *)((char *)dmabuf.dmabuffer+dmabuf.partlen*part);
}

void dmabuf_copy(void *buf,int part)
/*
 * copy buffer to dma buffer part (do nothin if they're equal)
 * (that's for compatiblity with PM_DMA
 */
{ void *ptr;
  if (buf==(ptr=dmabuf_getptr(part))) return;
  else
    memcpy(ptr,buf,dmabuf.partlen);
}

void dmabuf_free(void)
/*
 *  free dos-memory and buffer in data-seg
 */
{
  free(dmabuf.realbuf);
}

/* DMAC-Hardware data/acessing-routines: */

static unsigned char DMA_Ports[8][6]=
  {{0x00, 0x01, 0x0A, 0x0B, 0x0C, 0x87},
   {0x02, 0x03, 0x0A, 0x0B, 0x0C, 0x83},
   {0x04, 0x05, 0x0A, 0x0B, 0x0C, 0x81},
   {0   , 0   , 0   , 0   , 0   , 0   }, // channel 4 is not possible !
   {0xC0, 0xC2, 0xD4, 0xD6, 0xD8, 0x8F},
   {0xC4, 0xC6, 0xD4, 0xD6, 0xD8, 0x8B},
   {0xC8, 0xCA, 0xD4, 0xD6, 0xD8, 0x89},
   {0xCC, 0xCE, 0xD4, 0xD6, 0xD8, 0x8A}};

#define SINGLEMODE    0x40
#define DMAREAD       0x08
#define AUTOINIT      0x10

struct t_dmac_data { int dma_chn; // -1 if no active
                     unsigned char * dma_hwi; // pointer to hardware information
                     unsigned char dmapage;
                     unsigned int  dmaofs;
                     unsigned int  size;
                   } dmac;

void dma_start(int channel)
/*
 * start dma "channel" to send data (looping in dma-buffer)
 */
{
  dmac.dma_chn=channel&7;
  dmac.dma_hwi=DMA_Ports[dmac.dma_chn];

  if (dmac.dma_chn&4)  //if 16bit DMA channel
  { // DMA adress (128KB pages)
    dmac.dmapage=((dmabuf.linaddr>>16)&0xfe);
    dmac.dmaofs =(unsigned int)((dmabuf.linaddr&0x1fff0l)>>1);
    dmac.size=((dmabuf.parts*dmabuf.partlen)>>1)-1; // buflen in words -1
  } else // 8bit DMA
  { // DMA adress (64KB pages)
    dmac.dmapage=((dmabuf.linaddr>>16)&0xff);
    dmac.dmaofs =(unsigned int)((dmabuf.linaddr&0xfff0));
    dmac.size=dmabuf.parts*dmabuf.partlen-1;        // buflen in bytes -1
  }
  disable();                       // disable interrupts
  outpb(dmac.dma_hwi[2],dmac.dma_chn|4); // disable DMA for programming
  outpb(dmac.dma_hwi[4],0);              // clear FlipFlop
  //setup DMA-transfer mode:
  outpb(dmac.dma_hwi[3],(SINGLEMODE|AUTOINIT|DMAREAD|(dmac.dma_chn&3)));
  outpb(dmac.dma_hwi[0],(char)dmac.dmaofs);
  outpb(dmac.dma_hwi[0],(char)(dmac.dmaofs>>8));
  outpb(dmac.dma_hwi[5],(char)dmac.dmapage);
  outpb(dmac.dma_hwi[1],(char)dmac.size);
  outpb(dmac.dma_hwi[1],(char)(dmac.size>>8));   // DMA buffer length
  outpb(dmac.dma_hwi[2],dmac.dma_chn&3);   // enable DMA
  enable();                       // enable interrupts
}

void dma_stop(void)
/*
 * stop dma "channel" to send data
 */
{
  if (dmac.dma_chn==-1) return;
  outp(dmac.dma_hwi[2],dmac.dma_chn|4); // disable DMA
  dmac.dma_chn=-1;  // no active dma anymore
}
