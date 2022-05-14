/****************************************************************************
   sample - SampleMemoryLoaderClass (sub class of InstrumentLoaderClass) 
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 29.10.1996
****************************************************************************/

// known bug: lsb/msb conversion is not done yet - so it will fail on
//            little endian maschines
// ---> ioformat setting is not very nice designed...

#include "player.h"
#include "modules/sample.h"

/*
   SampleLoaderClass can manage conversion between the different sample types
     8 bit signed/unsigned
    16 bit signed/unsigned lsb/msb-order 
*/

Status SampleLoaderClass::RegisterSample(InstrumentID Aid,SampleType spec,istream& A_input,DataSize size,DWORD lstart,DWORD lend,int lflag)
{ id=Aid;
  switch (inputtype=spec)
  { case SMP_UNSIGNED_8BIT:
	LoadFunc=Load_U8; break;
    case SMP_SIGNED_8BIT:
      	LoadFunc=Load_S8;break;
    case SMP_UNSIGNED_16BIT:
	LoadFunc=Load_U16;break;
    case SMP_SIGNED_16BIT:
	LoadFunc=Load_S16;break;
    default:
	LoadFunc=Load_Unknown;return BAD_PARAMETER; break;
  }
  input=&A_input;
  SamplesLeft=SampleSize=size;
  loopstart=lstart; loopend=lend; loopflag=lflag;
  return NO_ERROR;
}

Status SampleLoaderClass::GetInstrumentSpecs(InstrumentID Aid, UserSpecPtr up)
{ if (Aid!=id) return BAD_ID;
  SampleSpecPtr sp=(SampleSpecPtr)up;
  sp->type=outputtype;
  sp->size=SampleSize;
  sp->loopflag=loopflag;
  sp->loopstart=loopstart;
  sp->loopend=loopend;
  return NO_ERROR;
}

Status SampleLoaderClass::SetOutputFormat(InstrumentType id,UserTypePtr p)
{ SampleTypePtr sp=(SampleTypePtr)p;
  if (id!=IT_SAMPLE) { outputtype=SMP_UNDEFINED; return NO_SUPPORT; }
  if (inputtype==SMP_COPY)
  { outputtype=inputtype;
    return NO_ERROR;
  }
  switch (*sp)
  { case SMP_UNSIGNED_8BIT:
    case SMP_SIGNED_8BIT:
    case SMP_UNSIGNED_16BIT:
    case SMP_SIGNED_16BIT:
		outputtype=*sp;
		return NO_ERROR;
    default: 
		outputtype=SMP_UNDEFINED;
		return NO_SUPPORT;
  }
}

Status SampleLoaderClass::Load(InstrumentID Aid,DataPtr p,DataSize l)
{ if (Aid!=id) return BAD_ID;
  return LoadFunc(p,l);
}

/******************************************************************************
 *  input stream are 8bit signed samples				      *
 *  - convert d'em to output format ...					      *
 ******************************************************************************/
Status SampleLoaderClass::Load_S8(DataPtr data,DataSize size)
{
  switch (outputtype)
  { case SMP_SIGNED_8BIT: // 'copy'
      { input->read(data,size); 
        return (!*input)?ERRORLOADING:NO_ERROR; 
        break;
      }
    case SMP_UNSIGNED_8BIT: // +128 !
      { BYTE *h=(BYTE*)data;
	input->read(data,size);
        while (size--) { (*h++)+=128; }
	return (!*input)?ERRORLOADING:NO_ERROR;
	break;
      }
    case SMP_SIGNED_16BIT: // shift d'em
      { SWORD *h=(SWORD*)data;size>>=1;
	SBYTE b;
        while (size--&& !(!*input)) { input->read(&b,1);(*h++)=((SWORD)b)<<8; }
	return (!*input)?ERRORLOADING:NO_ERROR;
	break;
      }
    case SMP_UNSIGNED_16BIT: // +128 and shift d'em
      { WORD *h=(WORD*)data;size>>=1;
	SBYTE b;
        while (size-- && !(!*input) ) { input->read(&b,1);(*h++)=128U+b<<8; }
	return (!*input)?ERRORLOADING:NO_ERROR;
	break;
      }
  }
}

/******************************************************************************
 *  input stream are 8bit unsigned samples				      *
 *  - convert d'em to output format ...					      *
 ******************************************************************************/
Status SampleLoaderClass::Load_U8(DataPtr data,DataSize size)
{ 
  switch (outputtype)
  { case SMP_UNSIGNED_8BIT: // 'copy'
      { input->read(data,size); 
	return (!*input)?ERRORLOADING:NO_ERROR; 
	break;
      }
    case SMP_SIGNED_8BIT: // -128 !
      { SBYTE *h=(SBYTE*)data;
	input->read(data,size);
        while (size--) { (*h++)-=128; }
	return (!*input)?ERRORLOADING:NO_ERROR;
	break;
      }
    case SMP_UNSIGNED_16BIT: // shift d'em
      { WORD *h=(WORD*)data;size>>=1;
	BYTE b;
        while (size-- && !(!*input) ) { input->read(&b,1);(*h++)=((WORD)b)<<8; }
	return (!*input)?ERRORLOADING:NO_ERROR;
	break;
      }
    case SMP_SIGNED_16BIT: // -128 and shift d'em
      { SWORD *h=(SWORD*)data;size>>=1;
	BYTE b;
        while (size-- && !(!*input) ) { input->read(&b,1);(*h++)=(SWORD(b-128))<<8; }
	return (!*input)?ERRORLOADING:NO_ERROR;
	break;
      }
  }
}

/******************************************************************************
 *  input stream are 16bit signed samples				      *
 *  - convert d'em to output format ...					      *
 ******************************************************************************/
Status SampleLoaderClass::Load_S16(DataPtr data,DataSize size)
{ return NO_SUPPORT;
}

/******************************************************************************
 *  input stream are 16bit unsigned samples				      *
 *  - convert d'em to output format ...					      *
 ******************************************************************************/
Status SampleLoaderClass::Load_U16(DataPtr data,DataSize size)
{ return NO_SUPPORT;
}

Status SampleLoaderClass::SetPreferedFormat(InstrumentSpec*,int)
{ return NO_SUPPORT;
}
