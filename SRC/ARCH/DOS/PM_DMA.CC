/****************************************************************************
   pm_dma: DMA accessing routines for Intelx86 in protected mode
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel
  created: 21.06.1996

  THIS IS Intelx86 code only !
  currently DOS only - check the alloc'n'free functions
****************************************************************************/

#include <mem.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/movedata.h>
#include <pc.h>
#include "player.h"
#include "arch\dos\dma.h"

#define DMA_CONTROL             0x41
#define SET_DMA_ADDRESS         0x42
#define SET_DRAM_LOW            0x43
#define SET_DRAM_HIGH           0x44

#ifdef __GNUC__
#define outpb(a,b) outportb(a,b)
#define inpb(a)    inportb(a)
#endif

//
// Attention: implemenation of real and protected mode differs in some
//            important details !
//

struct t_dmabuf_data { int selector; // selector for dma buffer in dosmem
		       int segment;  // dos segment of dma buffer
                       unsigned long linaddr;
                         // physical adress of dma buffer is send with dmac
                         // (sometimes not the real start of dma buffer -
                         //  keep page overides in mind...)
                       void * buffer; // temporary buffer in data segement
		       int bufstart;  // !=0 in case of page overides
                       int partlen;   // length of one dma buffer part == size of buffer
                       int parts;     // number of dma buffer parts
                       int paras;     // size in paragraphs
                     } dmabuf;

/* --- this routine is DOS only ---- */
/* for LINUX you need to implement a different routine which
   allows you to allocate low memory (<1MB) for DMA transfers 
*/
int dmabuf_alloc(int partlen,int parts)
/*
 * Allocates an DMA-buffer of size: partlen*bufparts bytes
 * protected mode only: (a different implemtation in real mode!)
 * that buffer is located somewhere in DOS-memory, for that reason an
 * extra buffer is created in the data segment, you have to call
 * dmabuf_copy() to copy the data from dataseg to the DMA-buffer.
 *
 * returns errorcode (0: OK)
 */
{ trace("Allocate DMA buffers...\n");
  dmabuf.partlen=partlen;
  dmabuf.parts=parts;
  if (partlen*parts>65535) return -1; // maximum size of dmabuffer is 64KB
  // allocate temporary data buffer
  if ((dmabuf.buffer=malloc(partlen))==NULL) return -1; // not enough memory for buffers
  // allocate dos memory:
  dmabuf.paras=(partlen*parts*2)>>4; // (2*size) in paragraphs
  #ifdef __DEBUGGING__
    printf("request for %i pages.\n",dmabuf.paras);
  #endif
  /* __dpmi_allocate_dos_memory() does return the dos segment
    (found in DJGPP-FAQ) 
  */
  if ((dmabuf.segment=__dpmi_allocate_dos_memory(dmabuf.paras,&dmabuf.selector))==-1)
  { // failed
    trace("dpmi_alloc_dosmem failed\n");
    free(dmabuf.buffer);dmabuf.buffer=NULL;
    return -1;
  }
  dmabuf.linaddr=((unsigned long)dmabuf.segment)<<4;
  #ifdef __DEBUGGING__
    printf("dma buffer in DOS segment: %i\n",dmabuf.segment);
    printf("linear adress: %lu\n",dmabuf.linaddr);
    printf("Nice - well done, all buffers allocated.\n");
    getchar();
  #endif
  // check for page overides: (we do not allow 64KB overides, also in case
  // of 16bit transfers where only 128KB overides are not allowed...
  // (in fact we do not know here, if we'll later use 16bit or 8bit transfers)
  if ((dmabuf.linaddr&0xffff)+parts*partlen>0xffff) // OVERRIDE !!
  { // move start to the beginning of the next page...
    dmabuf.linaddr&=0xff0000; // clear ofs
    dmabuf.linaddr+=0x010000; // move to next page
    getchar();
  }
  return 0;
}

void * dmabuf_getptr(int part)
/*
 * this routine returns allways the pointer to the
 * buffer in dataseg. NOT to the real dmabuffer in DOS-mem !
 */
{ return dmabuf.buffer;
}

void dmabuf_copy(void *buf,int part)
/*
 * copies data from buf to a special "part" of the DMAbuffer in low-memory
 */
{ unsigned long p_ofs=dmabuf.partlen*(part%dmabuf.parts);

  /* assembler is the optimal way here */
  #ifdef __INTEL_INLINE_ASM__
  asm { push es
	mov  ax,dmabuf.selector
	mov  es,ax
	mov  edi,p_ofs
	mov  esi,buf
	mov  cx,dmabuf.partlen
	shr  cx,2
	rep  movsd
    }
  #elif __GNUC__
	__asm__  ("push %%es\n" : : );
        __asm__  ("movw	%0,%%es"   :  : "g" (dmabuf.selector));
        __asm__  ("movl %0,%%edi"  :  : "g" (p_ofs));
        __asm__  ("movl %0,%%esi"  :  : "g" (buf));
        __asm__  ("movl	%0,%%ecx"  :  : "g" ((DWORD)dmabuf.partlen));
	__asm__  ("movl %%ecx,%%eax" :  : );
        __asm__  ("shrl $2,%%ecx"  :  : );
	__asm__	 ("and	$3,%%eax" : : );
        __asm__  ("rep\n movsl %%es:(%%edi),%%ds:(%%esi)" : : );
	__asm__  ("mov  %%eax,%%ecx" :  : );
	__asm__  ("rep\n movsb %%es:(%%edi),%%ds:(%%esi)" : : );
        __asm__  ("pop  %%es" : : );
    /* --> you want it PENTIUM optimized ? Just use your FP-unit for copying
           32bytes per loop
    */
  #else
    #error "dmabuf_copy(): memory copy function not defined."
  #endif
  /*
    You need to find out a way to support 
    memory copy (pmode-dataseg:ofs=buf) to (dosseg=dmabuf.segment:ofs=0)

    e.g. under DJGPP you can:
      1. uses dmabuffer with its selector (like I did in asm)
	    movedata(_my_ds(),(int)buf,dmabuf.selector,p_ofs,dmabuf.partlen);
      2. uses the hacker style via linaddr
  	    movedata(_my_ds(),(int)buf,_dos_ds,dmabuf.linaddr+p_ofs,dmabuf.partlen);
      3. that's internaly equal to:
	    dosmemput(buf,dmabuf.partlen,dmabuf.linaddr+p_ofs);
      4. direct copy with ASM code ... look above
  */
}

void dmabuf_free(void)
/*
 *  free dos-memory and buffer in data-seg
 */
{ trace("free DMA buffers.\n");
  // free temporary data buffer:
  free(dmabuf.buffer);
  // free dos memory
  __dpmi_free_dos_memory(dmabuf.selector);
}

/* DMAC-Hardware data/acessing-routines: */

static unsigned int DMA_Ports[8][6]=
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
                     unsigned int * dma_hwi; // pointer to hardware information
                     unsigned char dmapage;
                     unsigned short dmaofs;
                     unsigned short size;
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
    dmac.dmaofs =((dmabuf.linaddr&0x1fff0)>>1);
    dmac.size=((dmabuf.parts*dmabuf.partlen)>>1)-1; // buflen in words -1
  } else // 8bit DMA
  { // DMA adress (64KB pages)
    dmac.dmapage=((dmabuf.linaddr>>16)&0xff);
    dmac.dmaofs =dmabuf.linaddr&0xfff0;
    dmac.size=dmabuf.parts*dmabuf.partlen-1;        // buflen in bytes -1
  }
  disable();                       // disable interrupts
  outpb(dmac.dma_hwi[2],dmac.dma_chn|4); // disable DMA for programming
  outpb(dmac.dma_hwi[4],0);              // clear FlipFlop
  //setup DMA-transfer mode:
  outpb(dmac.dma_hwi[3],(SINGLEMODE|AUTOINIT|DMAREAD|(dmac.dma_chn&3)));
  outpb(dmac.dma_hwi[0],(char)dmac.dmaofs);
  outpb(dmac.dma_hwi[0],(char)(dmac.dmaofs>>8));
  outpb(dmac.dma_hwi[5],dmac.dmapage);
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
  outpb(dmac.dma_hwi[2],dmac.dma_chn|4); // disable DMA
  dmac.dma_chn=-1;  // no active dma anymore
}
