/******************************************************************************
   player error handler
  ----------------------------------------------------------------------------
   copyright 1997 by A.Baresel
 *****************************************************************************/

#define PLY_NO_ERROR      0
#define PLY_BAD_COMMANDLN 1
#define PLY_DEVICE_FAILED 2
#define PLY_MODLOADFAIL   3

extern int plyerror;

extern void player_errormsg(char *);
inline int player_error() { return plyerror; }
