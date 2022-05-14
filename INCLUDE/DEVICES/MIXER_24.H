/****************************************************************************
    Mixer16 - mix routines for 16bit lookup
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 2.8.1996
****************************************************************************/

/***************************************************************************
 *  Mixer24_c    equal routines for stereo and mono mixing 
 ***************************************************************************/
class Mixer24_c:public Mixer_c {
	protected:
		 WORD buffersize;
                 SDWORD * MixBuffer;		 
		 BYTE interpolate;
                 BYTE quadtab[256];
		 BYTE outformat;
		 BYTE mainvolume;

                 #if defined(__DOS__) && !defined(__GNUC__)
		 // dos realmode compilers:
		   #define VOLTB_BASE SWORD
		 #else
		    // all other
		    #define VOLTB_BASE SDWORD
		  #endif
  		  typedef VOLTB_BASE voltb_t[65][256];
                  voltb_t *volumetable; // optimization -> a table for all volumes
 		                        // and all possible sample values

	         virtual SDWORD * mix_8( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                         DWORD delta, WORD todo, BYTE vol, BYTE panpos)=0;
                 virtual SDWORD * mix_8i( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                         DWORD delta, WORD todo, BYTE vol, BYTE panpos)=0;
                 virtual SDWORD * mix_8qi( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                         DWORD delta, WORD todo, BYTE vol, BYTE panpos)=0;
		 //void calc_posttable(BYTE mainvolume);

	public:
		  Mixer24_c(DWORD _buffersize,BYTE _interpolate,BYTE ouput_format);
		  virtual ~Mixer24_c(void);
                  void * ptr2buf(void);
                  void clear_buffer(WORD count);
		  void amplify_result(void *Dest,WORD count);
                  void * add_sample8( void * dest, BYTE * src,DWORD *offset,WORD *frac,
                                      BYTE dir, DWORD step, WORD todo, BYTE vol, BYTE panpos);
                  void * add_sample16( void * dest, WORD * src,DWORD *offset,WORD *frac,
                                      BYTE dir, DWORD step, WORD todo, BYTE vol, BYTE panpos);
};

/*****************************************************************************
 *  Mono24_c    routines for mixing samples to 8/16 bit mono data
 ****************************************************************************/
class Mono24_c:public Mixer24_c {
	     public:
		 Mono24_c(DWORD buffersize,BYTE _interpolate,BYTE ouput_format);

              protected:
                 WORD no_samples;
                 SDWORD * mix_8( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                               DWORD delta, WORD todo, BYTE vol, BYTE panpos);
                 SDWORD * mix_8i( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                                DWORD delta, WORD todo, BYTE vol, BYTE panpos);
                 SDWORD * mix_8qi( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                                DWORD delta, WORD todo, BYTE vol, BYTE panpos);
};

/*****************************************************************************
 *  Stereo24_c    routines for mixing samples to 8/16 bit stereo data
 ****************************************************************************/
class Stereo24_c:public Mixer24_c {
             public:
		 Stereo24_c(DWORD buffersize,BYTE _interpolate,BYTE output_format);

              protected:
                 WORD no_samples;
                 SDWORD * mix_8( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                               DWORD delta, WORD todo, BYTE vol, BYTE panpos);
                 SDWORD * mix_8i( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                               DWORD delta, WORD todo, BYTE vol, BYTE panpos);
                 SDWORD * mix_8qi( SDWORD * dest, BYTE * src,SDWORD *ofs,BYTE dir,
                                DWORD delta, WORD todo, BYTE vol, BYTE panpos);
};
