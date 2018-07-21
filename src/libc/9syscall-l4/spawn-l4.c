#include  "sys.h"
#include  <u.h>
#include  <libc.h>

#include  "ipc-l4.h"

//---------------------------------------------------
/*  argtbl:
 *  +--------+
 *  |  size  |
 *  +--------+
 *  | argc   |   ex. =2
 *  +--------+
 *  | argv   |   == 12
 *  +--------+
 *  | argp[0]|  argv[] pointer to argument
 *  +--------+
 *  | argp[1]|
 *  +--------+
 *  |  NULL  |
 *  +--------+
 *  | argstr |   argp[argc+1]
 *  |        |
 *  |        |
 *  +--------+
 */

typedef struct {
  int       size;
  int       argc;
  void*     argv;
  void*     argp[0];
} argtbl_t;

//#define DBGPRN if(0)print
static void _dump_argtbl(int n, char *xp)
{ 
  int  i, size;
  argtbl_t *ap = (argtbl_t *)xp;
  char *cp = &ap->argp[0];
  char ch;
  size = ap->size;
  DBGPRN("ARGTBL:%d %x [%x:%d:%d] {", n, ap, ap->size, ap->argc, ap->argv);
  size -= 12;
  if (size>32) size = 32; 
  for(i = 0; i < size; i++)
    { ch = cp[i]; if (ch<32 || ch>126) ch = '.'; DBGPRN("%c", ch);  }
  DBGPRN("}\n");
}


// Don't forget to free the malloced memory
argtbl_t * mkargtbl(char *argv[])
{
  char  *hp;
  char  *p;
  int    argc = 0, size = 0, i;
  argtbl_t *argtbl = nil;

  while (argv[argc])
    argc++;

  if (argc == 0) return nil;

  size = sizeof(argtbl_t) + (argc+1)*sizeof(void*);
  for(i = 0; i<argc; i++)
    size += strlen(argv[i]) + 1;
  // size == total size of argment table.

  argtbl = (argtbl_t*)malloc(size);

  argtbl->size = size;
  argtbl->argc = argc;
  argtbl->argv = (void*)((unsigned)(&argtbl->argp[0]) - (unsigned)argtbl);

  hp = (char*)&argtbl->argp[argc+1] ;
  // [ argc | argv | arg0 | arg1 | arg2 | 0 | arg0str | arg1str | ... ]

  for (i = 0; i < argc; i++) {
    argtbl->argp[i] = (void*)((unsigned)hp - (unsigned)argtbl);
    p = argv[i];
    while ((*hp++ = *p++) != 0)
      ;
  }
  argtbl->argp[argc] = 0;
  // ASSERT( (size == hp-argtbl));
  // _dump_argtbl(0, argtbl); //%

  return  argtbl;
}
//-------------------------------------


int      exec(char* arg1, char * arg2[])       // s?
{
#if 1 //---------------------------------------
  argtbl_t* argtbl;
  int   size, *sizep;
  int   rc;
  argtbl = mkargtbl(arg2);

  if (argtbl == nil)
    rc = syscall_sai(EXEC, arg1, 0, 0);
  else {
    sizep = (int*)argtbl; // the first 4 byte contains the size.
    size = *sizep;
    rc = syscall_sai(EXEC, arg1, argtbl, size);
    free (argtbl);
  }
  return rc;

#else //---------------------------------
  return syscall_sa(EXEC, arg1, (char*)arg2); 
#endif //--------------------------------
}



int      spawn(char* arg1, char * arg2[])	// s?
{   
#if 1 //---------------------------------------
    argtbl_t* argtbl;
    int   size, *sizep; 
    int   rc;

    argtbl = mkargtbl(arg2);

    if (argtbl == nil)
        rc = syscall_sai(SPAWN, arg1, 0, 0);
    else {
        sizep = (int*)argtbl; // the first 4 byte contains the size.
	size = *sizep;
	rc = syscall_sai(SPAWN, arg1, argtbl, size);
	free (argtbl);
    }
    return rc;
#else //---------------------------------
    return syscall_sa(SPAWN, arg1, (char*)arg2); 
#endif //-------------------------------
}


// long     _read(int arg1, void* arg2, long arg3)	// iai
// {   return syscall_iai(_READ, arg1, arg2, arg3); }


