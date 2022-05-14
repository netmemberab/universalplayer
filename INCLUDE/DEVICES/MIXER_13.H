/****************************************************************************
    Mixer13 - mix routines for 13bit loopup
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 1.8.1996
****************************************************************************/

/***************************************************************************
 *  Mixer13_c    equal routines for stereo and mono mixing 
 ***************************************************************************/
class Mixer13_c:public Mixer_c {
	protected:
		 DWORD buffersize;
                 WORD * MixBuffer;
		 #ifdef __I86_CODE__
		   WORD *posttable;
		 #else
		   WORD posttable[2*2048+1];
		 #endif
		 BYTE interpolate;
		 BYTE quadtab[256];
		 BYTE outformat;

  		 typedef WORD voltb_t[65][256];
                 voltb_t * volumetable;

	         virtual WORD * mix_8( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                         DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos)=0;
                 virtual WORD * mix_8i( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                         DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos)=0;
                 virtual WORD * mix_8qi( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                         DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos)=0;
		 void calc_posttable(BYTE mainvolume);

	public:
		  Mixer13_c();
		  ~Mixer13_c(void);
                  void setup(DWORD _size,BYTE _interpolate,BYTE _outputformat);
                  void * ptr2buf(void);
                  void clear_buffer(WORD count);
		  void amplify_result(void *Dest,WORD count);
                  void * add_sample8( void * dest, BYTE * src,DWORD *offset,DWORD *frac,
                                      BYTE dir, DWORD step_int,DWORD step_frac, WORD todo, BYTE vol, BYTE panpos);
                  void * add_sample16( void * dest, WORD * src,DWORD *offset,DWORD *frac,
                                      BYTE dir, DWORD step_int,DWORD step_frac, WORD todo, BYTE vol, BYTE panpos);
};

/*****************************************************************************
 *  Mono13_c    routines for mixing samples to 8/16 bit mono data
 ****************************************************************************/
class Mono13_c:public Mixer13_c {
	     public:
                 void setup(DWORD _size,BYTE _interpolate,BYTE _outputformat);

              protected:
                 WORD no_samples;
                 WORD * mix_8( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                               DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos);
                 WORD * mix_8i( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                                DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos);
                 WORD * mix_8qi( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                                 DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos);
};

/*****************************************************************************
 *  Stereo13_c    routines for mixing samples to 8/16 bit stereo data
 ****************************************************************************/
class Stereo13_c:public Mixer13_c {
             public:
                 void setup(DWORD _size,BYTE _interpolate,BYTE _outputformat);

              protected:
                 WORD no_samples;
                 WORD * mix_8( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                               DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos);
                 WORD * mix_8i( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                                DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos);
                 WORD * mix_8qi( WORD * dest, BYTE * src,SDWORD *ofs,DWORD *frac,BYTE dir,
                                 DWORD delta_int,DWORD delta_frac, WORD todo, BYTE vol, BYTE panpos);
};
