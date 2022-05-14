/****************************************************************************
    Mixer16 - mix routines for 16bit lookup
  --------------------------------------------------------------------------
  (c) copyright 1996 by A.Baresel

  created: 2.8.1996
****************************************************************************/

/***************************************************************************
 *  Mixer16_c    equal routines for stereo and mono mixing 
 *		 (the reset is abstract)
 ***************************************************************************/
class Mixer16_c:public Mixer_c {
	protected:
		 WORD buffersize;
                 WORD * MixBuffer;
		 WORD * posttable;
		 BYTE interpolate;
		 BYTE outformat;

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

	         virtual WORD * mix_8( WORD * dest, BYTE * src,SDWORD *ofs,SBYTE dir,
                         SDWORD delta, WORD todo, BYTE vol, BYTE panpos)=0;
                 virtual WORD * mix_8i( WORD * dest, BYTE * src,SDWORD *ofs,SBYTE dir,
                         SDWORD delta, WORD todo, BYTE vol, BYTE panpos)=0;
		 void calc_posttable(BYTE mainvolume);

	public:
		  Mixer16_c(DWORD _buffersize,BYTE _interpolate,BYTE ouput_format);
		  virtual ~Mixer16_c(void);
                  void * ptr2buf(void);
                  void clear_buffer(void);
		  void amplify_result(void *Dest);
                  void * add_sample8( void * dest, BYTE * src,SDWORD *offset,WORD *frac,
                                      SDWORD step, WORD todo, BYTE vol, BYTE panpos);
                  void * add_sample16( void * dest, WORD * src,SDWORD *offset,WORD *frac,
                                      SDWORD step, WORD todo, BYTE vol, BYTE panpos);
};

/*****************************************************************************
 *  Mono16_c    routines for mixing samples to 8/16 bit mono data
 ****************************************************************************/
class Mono16_c:public Mixer16_c {
	     public:
		 Mono16_c(DWORD buffersize,BYTE _interpolate,BYTE ouput_format);

              protected:
                 WORD no_samples;
                 WORD * mix_8( WORD * dest, BYTE * src,SDWORD *ofs,SBYTE dir,
                               SDWORD delta, WORD todo, BYTE vol, BYTE panpos);
                 WORD * mix_8i( WORD * dest, BYTE * src,SDWORD *ofs,SBYTE dir,
                                SDWORD delta, WORD todo, BYTE vol, BYTE panpos);
};

/*****************************************************************************
 *  Stereo16_c    routines for mixing samples to 8/16 bit stereo data
 ****************************************************************************/
class Stereo16_c:public Mixer16_c {
             public:
		 Stereo16_c(DWORD buffersize,BYTE _interpolate,BYTE output_format);

              protected:
                 WORD no_samples;
                 WORD * mix_8( WORD * dest, BYTE * src,SDWORD *ofs,SBYTE dir,
                               SDWORD delta, WORD todo, BYTE vol, BYTE panpos);
                 WORD * mix_8i( WORD * dest, BYTE * src,SDWORD *ofs,SBYTE dir,
                               SDWORD delta, WORD todo, BYTE vol, BYTE panpos);
};
