/*   ===== _dgbpr.h =====
 * A user of this file must define _DBGFLG in advance:
 *  #define   _DBGFLG 1      //OR*  static int _DBGFLG = 1; 
 *  #include  "_dbgpr.h"
 */  

extern int  l4printf_b(const char*, ...);
extern char l4printgetc(const char*, ...);

#define  DBGPRN  if(_DBGFLG) l4printf_b
#define  DBGBRK  if(_DBGFLG) l4printgetc
#define  DD(x)   if(_DBGFLG) l4printf_b("%s: %x \n", __FUNCTION__, x);

#define  DPR     l4printf_r
#define  DPG     l4printf_g
#define  DPB     l4printf_b

#define TBD tobedefined(__FUNCTION__)
