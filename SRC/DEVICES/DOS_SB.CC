/****************************************************************************
   DOS - SoundBlaster support
   models: SB1.0,SB2.0,SBPRO,SBPRO2,SB16,SBAWE (SB16-mode)
   ( 8 bit, mono, max. 44/24 kHz )
  --------------------------------------------------------------------------
  (c) copyright 1995 by A.Baresel

  created: 09.01.1996
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>

#include "devices/dos_sb.h"
#include "arch/dos/dma.h"

#ifdef __GNUC__
#include <pc.h>
#define inp(a)  inportb(a)
#define outp(a,b) outportb(a,b)
#endif

/* Prototypes for hardware access */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static void (*initialize_play)(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bits);
static void SB_reset();
static int  SB_init(SWORD base, short irq, short dma, short dsp);
static void SB_halt(int base);
static void SB_close(int base,int irq);
static void SB_setsamplerate(DWORD *SR);
static WORD SB_GetDSPversion();
static void SB1x_set(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bitmode);
static void SB2x_set(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bitmode);
static void SBPRO_set(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bitmode);
static void SB16_set(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bitmode);
static void SB_out(unsigned char v);
void SB_IRQ_handler(UserData);

#define SB_speakeron  SB_out(0xD1)
#define SB_speakeroff SB_out(0xD3);

static void SB_getenv(SWORD *Base,short *DSPver,short *Irq,short *Dma8,short *Dma16);


// Model specific HARDWARE DATA ------------------------------------------------
struct t_SBdata { int autoinit;
		  int stereo;
		  int stereomult;
		  int bits;
		  WORD  maxmnSR;
		  WORD  maxstSR;
		  int datatype;    // signed/unsigned data
		  void (*initplay)(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bits);
		} SBdata[5]={{0,0,0,0,0,0,0,NULL },
			     {0,0,1,8, 22222 ,    1 ,SB_UNSIGNED,SB1x_set},    // SB 1.0 & 1.5
			     {1,0,1,8, 45454u,    1 ,SB_UNSIGNED,SB2x_set},    // SB 2.0 & 2.5
			     {1,1,2,8, 45454u,22222 ,SB_UNSIGNED,SBPRO_set},   // SB PRO ,PRO2,PRO3
			     {1,1,1,16,45454u,45454u,SB_BOTH    ,SB16_set}};   // SB 16,16ASP,AWE,32
// -----------------------------------------------------------------------------

static int temp_base; /* temporary baseadress - changed by SB_c members */

struct SB_IRQ_datastruct { SB_c * sb;
			   ModuleClass * module;
			 };

#define BUFFERPART (45454/70-3)
//10*1024l
#define BUFFERSIZE BUFFERPART*2

SB_c::SB_c(DeviceOptionsPtr p):MixDevice_c()
{ SB_HardwareOptionsPtr sbopts=(SB_HardwareOptionsPtr)p;
  if (status) return; /* -> error initializing Software-Mixer <- */

  // SB_getenv(&Base,&DSP_version,&IRQ,&DMA8,&DMA16);

  Card_Base=sbopts->base;
  Card_IRQ=sbopts->irq;
  Card_DMA8=sbopts->dma8;
  Card_DMA16=sbopts->dma16;
 
  trace("Check & initialize SoundBlaster compatible soundcard.\n");
  temp_base=Card_Base;
  if (SB_init(Card_Base,Card_IRQ,Card_DMA8,DSP_version)) 
    { status=DEVICE_FAILED; return; }
  DSP_version=SB_GetDSPversion();
  if (DSP_version>4) DSP_version=4;

  #ifdef __DEBUGGING__
    printf("Base: %d, IRQ: %d, DMA8: %d, DMA16: %d, DSP: %d\n",Card_Base,Card_IRQ,Card_DMA8,Card_DMA16,DSP_version);
    getchar();
  #endif

  if (dmabuf_alloc(BUFFERPART,2))
    { status=NO_MEMORY; trace("dmabuf failed.\n");getchar();return; } // allocating buffer failed

  // default settings: (mono, to the max)
  emulAI=!SBdata[DSP_version].autoinit;
  DWORD tmp_SR=45454 /*SBdata[DSP_version].maxmnSR*/;
  SB_setsamplerate(&tmp_SR); // setup and return real used value (got from SB-hardware)

  // defaults to mixer:
  { MixerSettingsStruct set;

    set.MixFrequency=tmp_SR;
    set.mixmode=MIX_STEREO | MIX_UNSIGNED | MIX_8BIT; // defaults
    set.maxbuffer=BUFFERPART;
    set.Mixer=sbopts->Mixer;
    set.Filter=NULL;

    MixDevice_c::Configure(&set);
    #ifdef __DEBUGGING__
    trace("sb konf.done.");getchar();
    #endif
  }

  SB_speakeron;
}

SB_c::~SB_c(void)
{
  trace("halt SoundBlaster.\n");
  SB_close(Card_Base,Card_IRQ);   // halt and close sounddevice
  // stop dma channel and free used memory
  dma_stop();
  dmabuf_free();
}

static inline void setborder(BYTE color)
{ outp(0x3c0,0x31);
  outp(0x3c0,color);
}

static void waitraster()
{ while (!(inp(0x3da)&0x08));
  while (inp(0x3da)&0x08);
}

static int playflag;
unsigned int cur_buf=0;      // for x-buffering

Status SB_c::Start(ModuleClass *module)
{ void *ptr;
  Status s;

  if (s=MixDevice_c::Start(module)) return s;

  // initialize interupt:
  { SB_IRQ_datastruct *d=new SB_IRQ_datastruct;
    d->sb=this;
    d->module=module;
    IRQ_Manager.ReserveIRQ(Card_IRQ,SB_IRQ_handler,d);
  }

  ptr=dmabuf_getptr(0);
  mixing(module,ptr,BUFFERPART);
  dmabuf_copy(ptr,0);

  ptr=dmabuf_getptr(1);
  mixing(module,ptr,BUFFERPART);
  dmabuf_copy(ptr,1);
  cur_buf=0;

  dma_start((GetBitMode()==MIX_8BIT)?Card_DMA8:Card_DMA16);
  initialize_play(BUFFERPART,!emulAI,GetStereoMode(),GetBitMode());
  trace("playing...\n");

  playflag=0;
  while(1)
  { if (playflag)
    { playflag=0;
      setborder(4);
      fillbuffer(module);
      setborder(0);
    }    
  }
  return NO_ERROR;
}

Status SB_c::Stop(void)
{ return(0);
}

Status SB_c::Continue(void)
{ return(0);
}

Status SB_c::Configure(DeviceSettingsPtr p)
{ SB_SettingsPtr sbset = (SB_SettingsPtr)p;

  /* you have to stop playing before changing settings ! */
  MixerSettingsStruct mixset;

  mixset.Mixer=sbset->Mixer;
  mixset.Filter=sbset->Filter;

  BYTE output_format;
  if (SBdata[DSP_version].bits==8) sbset->bits=8;
  if (SBdata[DSP_version].stereo==0) sbset->stereo=SB_MONO;
  if (SBdata[DSP_version].datatype==SB_UNSIGNED) sbset->sign=SB_UNSIGNED;

  if (sbset->bits==8)
    mixset.mixmode=(sbset->sign==SB_UNSIGNED)?MIX_UNSIGNED|MIX_8BIT:MIX_SIGNED|MIX_8BIT;
  else
    mixset.mixmode=(sbset->sign==SB_UNSIGNED)?MIX_UNSIGNED|MIX_16BIT:MIX_SIGNED|MIX_16BIT;
  mixset.MixFrequency=sbset->MixFrequency;
  mixset.maxbuffer=BUFFERPART;

  { DWORD SR=mixset.MixFrequency*((sbset->stereo==SB_STEREO)?StereoMultiply2:1);
    /* we did'nt check temp_base since SB_init() */
    SB_setsamplerate(&SR); // setup and return real used value (got from SB-hardware)
    sbset->MixFrequency=mixset.MixFrequency=(sbset->stereo==2)?SR/StereoMultiply2:SR;
  }
  return MixDevice_c::Configure(&mixset);
}

// ##################################################
// called by soundblaster hardware interrupt request:
// ##################################################
void SB_IRQ_handler(UserData u)
{ SB_IRQ_datastruct * data=(SB_IRQ_datastruct*)u;
  playflag=1;
  inp(temp_base+0x0e); // ackknowedge "8bit-IRQ" (if there was one)
  inp(temp_base+0x0f); // ackknowedge "16bit-IRQ" (if there was one)
  /*
  setborder(4);
  data->sb->fillbuffer(data->module);
  setborder(0);
  */
}

// ___________________________________________________________________________
// ++++++++ HARDWARE FUNCTIONS +++++++++++++++++++++++++++++++++++++++++++++++
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define SB_inp(p) inp(temp_base+p)

static unsigned char SB_in()
{ int c=32000;
  while (!(SB_inp(0xE)&0x80) && c--);
  return SB_inp(0xA);
}

#define SB_outp(p,v) outp(temp_base+p,v)

static void SB_out(unsigned char v)
{ int c=32000;
  while (SB_inp(0xC)&0x80 && c--); // no endless loop if SB somehow does not work correct
  SB_outp(0xC,v);
}

static void SB_reset()
{
  SB_outp(0x06,1);
  delay(50);
  SB_inp(0x6);
  SB_outp(0x6,0);
  delay(50);
}

static char checkreset()
{ unsigned int i;
  SB_reset();
  // 60000 loops for very slow SB1.x (and compatibles)
  for (i=0; i<60000u && !(SB_inp(0xE)&0x80); i++);  
  return(SB_inp(0xA)==0xAA);
}

int testflag;

void sb_testirq(UserData p)
{ int * i=(int*)p;
  *i=1;
  // ackknowledge "8bit"-IRQ on SB:
  SB_inp(0x0e);
}

// resets the SB and checks if settings ok
// (?? is it also correct for SB compatibles ??)

int  SB_init(SWORD base, short irq, short dma, short dsp)
{ WORD tmp_dsp; 
  temp_base=base;
  SB_speakeron;
  trace("speaker on.");
  if (!checkreset()) return(1); // baseadress not correct
  trace("check reset.");
  // play a silent block to check IRQ
  IRQ_Manager.ReserveIRQ(irq,sb_testirq,&testflag);
  trace("irq.");
  testflag=0;
  SB_inp(0x0e);  // ackknowledge "8bit"-DMA
  SB_inp(0x0f);  // ackknowledge "16bit"-DMA
  outp(0x20,0x20); // ackknowledge IRQC1
  outp(0xA0,0x20); // ackknowledge IRQC2
  SB_out(0x80);     // setup a silent part
  SB_out(0x03);
  SB_out(0x00);
  #ifdef __DEBUGGING__
    printf("testing...%i",irq);
  #endif
  delay(50);
  //check if 'our' IRQ was called :
  if (!testflag) { IRQ_Manager.FreeIRQ(irq); return(1); } // IRQ is not correct
  trace("irq done.");
  testflag=0;
  /* DMA channel testing */
  SB_out(0x14);  // 8bit,mono,nonhighspeed DMA transfer (possible on all SB models)
  SB_out(0x10);
  SB_out(0x00);
  dma_start(1); // 8bit dma...
  delay(100);
  if (!testflag)
  { //reset DMAtransfer
    SB_out(0x80);
    SB_out(0x03);
    SB_out(0x00);
    delay(50);
    IRQ_Manager.FreeIRQ(irq);
    return(1); 
  }
  trace("dma done.");
  //reset DMAtransfer
  SB_out(0x80);
  SB_out(0x03);
  SB_out(0x00);
  delay(50);
  SB_inp(0x0e);  // ackknowledge "8bit"-DMA
  SB_inp(0x0f);  // ackknowledge "16bit"-DMA
  outp(0x20,0x20); // ackknowledge IRQC1
  outp(0xA0,0x20); // ackknowledge IRQC2
  IRQ_Manager.FreeIRQ(irq);

  trace("dma2 done.");

  // get DSP version: (to check model)
  SB_out(0xE1);
  tmp_dsp=0x100*SB_in()+SB_in();

  // if (*dsp && (tmp_dsp>>8)<*dsp) return(-4);    // wrong DSP-chip version (some extensions not available)
  // if (!*dsp) *dsp=tmp_dsp>>8;

  #ifdef __DEBUGGING__
    printf("%i\n",tmp_dsp);
  #endif
  SB_speakeroff;
  initialize_play=SBdata[tmp_dsp>>8].initplay;  // the play routine for the _required_ dsp-version is set

  return(0);
}

WORD SB_GetDSPversion()
{ SB_out(0xE1);
  WORD w=SB_in(); // high by of version
  SB_in(); // low byte of version
  return w;
}

void SB_halt(int base)
{ temp_base=base;SB_out(0xd0);SB_out(0xd9);SB_out(0xd0); }

void SB_close(int base,int irq)
{ temp_base=base;
  SB_speakeroff;
  SB_halt(base);
  // reset SB:
  IRQ_Manager.ReserveIRQ(irq,sb_testirq,&testflag);
  SB_speakeron;
  SB_inp(0x0e);  // ackknowledge "8bit"-DMA
  SB_inp(0x0f);  // ackknowledge "16bit"-DMA
  outp(0x20,0x20); // ackknowledge IRQC1
  outp(0xA0,0x20); // ackknowledge IRQC2
  SB_out(0x80); // program a silent block to stop DMA transfers
  SB_out(0x03);
  SB_out(0x00);
  delay(50);
  IRQ_Manager.FreeIRQ(irq);
  // well done, bye,bye
}

void SB_setsamplerate(DWORD *SR)
{ unsigned char tc;
  tc=256-1000000l/ *SR;
  SB_out(0x40);
  SB_out(tc);
  *SR=1000000l/(256-tc);
}

void SB1x_set(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bitmode)
{
  SB_out(0x48);
  SB_out(len-1);
  SB_out((len-1)>>8);
  SB_out(0x1C);
}

void SB2x_set(unsigned int len,unsigned char autoinit,unsigned char stereo,unsigned char bitmode)
{
  SB_out(0x48);
  SB_out(len-1);
  SB_out((len-1)>>8);
  SB_out(0x90);
}

void SBPRO_set(unsigned int len,unsigned char autoinit,unsigned char stereomode,unsigned char bitmode)
{
  if (stereomode==MIX_STEREO)
  { //setmixer: (filter off + stereo on)
    outp(temp_base+4,0x0e);
    outp(temp_base+5,inp(temp_base+5)|0x22);
  } else outp(temp_base+5,inp(temp_base+5)|0x20); // filter off
  SB_out(0x48);
  SB_out(len-1);
  SB_out((len-1)>>8);
  SB_out(0x90);
}

void SB16_set(unsigned int len,unsigned char autoinit,unsigned char stereomode,unsigned char bitmode)
{ 
  if (bitmode==MIX_8BIT) 
  { SB_out(0xc2|((autoinit&1)<<2));
    SB_out((stereomode==MIX_STEREO)?1<<5:0);  // bit 5 ... stereo & bit 4 = 1... signed data
    SB_out(len-1);
    SB_out((len-1)>>8);
  } else // 16 bit transfer
  { SB_out(0xb2|((autoinit&1)<<2));
    SB_out((stereomode==MIX_STEREO)?1<<5:0);  // bit 5 ... stereo & bit 4 = 1... signed data
    SB_out((len>>1)-1);
    SB_out(((len>>1)-1)>>8);
  }
}

// ++++++++++ PLAY FUNCTIONS +++++++++++++++++++++++++++++++++++++++++++++++++

int inside=0;

void SB_c::fillbuffer(ModuleClass * module)
{ // while (inside) ;
  inside=1;
  temp_base=Card_Base;
  // emulate the Autoinit (for SB1.x models)
  // if (emulAI) initialize_play(BUFFERPART,0,0,8);
  // calculate the next data:
  mixing(module,(BYTE*)dmabuf_getptr(cur_buf),BUFFERPART);
  setborder(1);
  dmabuf_copy(dmabuf_getptr(cur_buf),cur_buf);
  cur_buf=1-cur_buf;
  inside=0;
}

/****************************************************************************/
// ++++++++++ Detect routines ++++++++++++++++++++++++++++++++++++++++++++++++

void SB_getenv(SWORD *Base,short *DSPver,short *Irq,short *Dma8,short *Dma16)
{ char * s=getenv("BLASTER");
  long h;
  if (!s) return;
  while (1)
  {
    while (isspace(*s)) s++;
    if (!*s) break; // end of string
    switch (*s++)
    {
    case 'a': case 'A':
      h=strtoul(s, 0, 16);
      if (*Base==-1) *Base=h;
      break;
    case 'i': case 'I':
      h=strtoul(s, 0, 10);
      if (*Irq==-1) *Irq=h;
      break;
    case 'd': case 'D':
      h=strtoul(s, 0, 10);
      if (*Dma8==-1) *Dma8=h;
      break;
    case 'h': case 'H':
      h=strtoul(s, 0, 10);
      if (*Dma16==-1) *Dma16=h;
      break;
    case 't': case 'T':
      { BYTE conv[8]={2,1,3,2,3,3,4,4};  
        h=conv[strtoul(s, 0, 10)];
        if (*DSPver==-1) *DSPver=h;
      }
      break;
    }
    while (!isspace(*s)&&*s) s++;
  }
  return;
}
