//%%%% Debugging %%%%
// extern int DBGFLG;
// #define  DBGON  (DBGFLG = 1)
// #define  DBGOFF  (DBGFLG = 0)
  
#if 0
int l4printf_b(const char*, ...);
char l4printgetc();
  #define  DBGPRN  l4printf_b
  #define  DBGBRK  l4printgetc
#else
  #define  DBGPRN  if(0) l4printf_b
  #define  DBGBRK  if(0) l4printgetc
#endif
