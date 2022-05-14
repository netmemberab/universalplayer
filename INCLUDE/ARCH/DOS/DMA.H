/****************************************************************************
   DMA accessing routines for DOS (protected/real)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel
  created: 21.06.1996 (now designed for rm and pm)
****************************************************************************/

#ifndef __DOS_DMA_H__
#define __DOS_DMA_H__

/* interface description: */

int dmabuf_alloc(int partlen,int bufparts);
  /* Allocates an DMA-buffer of size: partlen*bufparts bytes 
     for reasons of optimal code only buffersizes divisible with 4 are 
     used correct (no code to check this!!)
  */

extern void * dmabuf_getptr(int part);
  /* returns a ptr to store your data for the dmabuffer */

void dmabuf_copy(void *buf,int part);
  /* put the data into the dmabuffer (if they are not allready there...RM/PM) */

void dmabuf_free(void);
  /* free buffers */

void dma_start(int channel);
  /* start sending data with dma channel */

void dma_stop(void);
  /* stop sending */

/* differences of implementation in real and protected mode:

dmabuf_alloc() -> allocate an DMA buffer
      real mode: buffer is located in the heap
      protected mode: buffer is located in dos memory (no direct access possible)
         and an extra buffer in data segment is allocated for temporary data
         storage of data for the dma buffer

dmabuf_getptr() -> get a ptr where you can copy the data for dma transfer
      real mode: ptr is really located in the requested part of the buffer
           (you can directly write to the dmabuffer)
      protected mode: allways the same ptr in dataseg for all parts
           (no direct dmabuffer access possible)

dmabuf_copy()   -> copy buffer to dma buffer part
      real mode: if the ptr does point to the specified dmabuffer part (dmabuf_getptr())
          it does nothing, if not - memcpy() is used
      protected mode: copies data from buffer to the dmabuffer located in
          dos-memory (kind of farcopy but in protected mode)

HOW TO USE: (for real and protected mode!!)

  allocate buffer.                         dmabuf_alloc()
  to put data into the dma buffer:
    get the ptr to write to                ptr=dmabuf_getptr()
    write your data to the returned location
    call dmabuf_copy(ptr)
      --> in rm nothing is done (in fact data is allready at the required location!)
      --> in pm a kind of farcopy for pm is used (my first AS-code >(;9)
*/

#endif
