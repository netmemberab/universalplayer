/****************************************************************************
   Mixer13 - fast ix86 assembler code header
 --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel
****************************************************************************/

extern "C" {

extern WORD volumetab[65][256];
extern WORD posttab[4096];
extern DWORD mixclocks;
extern DWORD mixloops;

extern void _I86_mix13_m8(WORD *Dest,BYTE * Sourc,SDWORD* ofs,DWORD *frac,DWORD dir,
		  DWORD delta_int,DWORD delta_frac, DWORD todo,DWORD vol,
		  DWORD panpos);
extern void _I86_mix13_s8(WORD *Dest,BYTE * Sourc,SDWORD* ofs,DWORD *frac,DWORD dir,
		  DWORD delta_int,DWORD delta_frac, DWORD todo,DWORD vol,
		  DWORD panpos);
extern void _I86_amp13_8(void *Dest,void * Source,DWORD count);
extern void _I86_clrbuf13(void *Buffer,DWORD value,DWORD count);
};
