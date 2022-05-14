/****************************************************************************
   Player fixed types (maschine/compiler depended macros)
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 15.10.1996
****************************************************************************/

#ifndef __MTYPES_H__
#define __MTYPES_H__

// --------------------------------------------------------------------------
// fixed-types for all compilers:
// --------------------------------------------------------------------------
// BYTE - unsigned 8bit
// SBYTE - signed 8bit
// WORD - unsigned 16bit
// SWORD - signed 16bit
// DWORD - unsigned 32bit
// SDWORD - signed 32bit

#define BYTE  unsigned char
#define SBYTE signed char

#if defined(__GNUC__)||defined(__SUN__)||defined(__SOLARIS__)
  #define WORD unsigned short
  #define SWORD short
  #define DWORD unsigned long
  #define SDWORD long
#elif defined(__DOS__)
  #define WORD unsigned int
  #define SWORD int
  #define DWORD unsigned long
  #define SDWORD long
#endif

#ifdef __GNUC__
  #define PACKSTRUC __attribute__ ((packed))
#else
  #define PACKSTRUC
#endif

#if defined(__DEBUGGING__)
#define trace(a) { printf(a);fflush(stdout); }
#else
#define trace(a) ;
#endif

#endif
