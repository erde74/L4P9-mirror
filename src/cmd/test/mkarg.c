#include <stdio.h>

#define PR printf

void dump_mem(char *title, unsigned char *start, unsigned size)
{
  int  i, j, k;
  unsigned char c;
  char  buf[128];
  char  elem[32];

  printf("* dump mem <%s>\n", title);
  for(i=0; i<size; i+=16) {
    buf[0] = 0;
    for (j=i; j<i+16; j+=4) {
      if (j%4 == 0) strcat(buf, " ");

      for(k=3; k>=0; k--) {
        c = start[j+k];
        snprintf(elem, 32, "%.2x", c);
        strcat(buf, elem);
      }
    }

    strcat(buf, "  ");
    for (j=i; j<i+16; j++) {
      c = start[j];
      if ( c >= ' ' && c < '~')
        snprintf(elem, 32, "%c", c);
      else
        snprintf(elem, 32, ".");
      strcat(buf, elem);
    }
    PR("%s\n", buf);
  }
}
/*  +--------+
 *  |  size  |
 *  +--------+
 *  | argc   |
 *  +========+
 *  | argv0  |  argv[] pointer to argument  
 *  +--------+
 *  | argv1  |
 *  +--------+
 *  |  NULL  |
 *  +========+
 *  | argstr |
 *  |        |
 *  |        |
 *  +--------+
 */     



// Don't forget to free the malloced memory
char * mkargtbl(char *argv[])
{
  char  *hp;
  char  **ap;
  char  *p;
  int  argc = 0, size = 0, i;
  char *argtbl = NULL;

  while (argv[argc]) 
    argc++;

  if (argc == 0) return NULL;

  size = (argc + 3) * sizeof(char*);
  for(i = 0; i<argc; i++) 
    size += strlen(argv[i]) + 1;
  // size of argtbl 

  argtbl = malloc(size);

  ap = (char**)argtbl;
  hp = &argtbl[(argc + 3) * sizeof(char*)];
  // [ argc | arg0 | arg1 | arg2 | 0 | arg0str | arg1str | ... ]
  *ap++ = size;
  *ap++ = (char*)argc;

  for (i = 0; i < argc; i++) {
    *ap++ = (char*)(hp - argtbl);
    *ap = NULL;
    p = *argv++;    
    printf("<%d:%s>" , hp-argtbl, p);
    while ((*hp++ = *p++) != 0)
      ;
  }
  // ASSERT( (size == hp-argtbl));
  printf("{%d, %d}\n", size, hp-argtbl);
  return  argtbl;
}

void xxx(char *argtbl)
{
  char  *p;
  int  argc;
  int  i, size;
  unsigned *up;

  up = (unsigned*) argtbl;
  size = *up++;
  argc = *up++;


  for(i=0; i<argc; i++, up++) {
    p = *up + (unsigned)argtbl;
    printf("[%d] \"%s\"\n", i, p);
  }
}

char*  mkargtbl2(char *arg, ...)
{
  return  mkargtbl(&arg);
}


main( )
{
  int  i;
  char  *tp;

  tp = mkargtbl2(0);
  if (tp) {
    xxx(tp);
    dump_mem("1", tp, 64);
  }

  tp = mkargtbl2("a", 0);
  xxx(tp);dump_mem("1", tp, 64);  free(tp);

  tp = mkargtbl2("a", "bb", 0);
  xxx(tp);  dump_mem("1", tp, 64); free(tp);

  tp = mkargtbl2("a", "bb", "ccc", 0);
  xxx(tp);  dump_mem("1", tp, 64); free(tp);

  tp = mkargtbl2("a", "bb", "ccc", "dddd", 0);
  xxx(tp);  dump_mem("1", tp, 64);  free(tp);

  tp = mkargtbl2("a", "bb", "ccc", "dddd", "eeeee", 0);
  xxx(tp);  dump_mem("1", tp, 64);  free(tp);

#if 1
  char *a[] = {"aa",  0};
  char *b[] = {"aa", "bbb", 0};
  char *c[] = {"aa", "bbb", "cccc", 0};
  char *d[] = {"aa", "bbb", "cccc", "ddddd", 0};
  char *e[] = {"aa", "bbb", "cccc", "ddddd", "eeeeee", 0};

  tp = mkargtbl(a);
  xxx(tp);  dump_mem("a", tp, 64);

  tp = mkargtbl(b);
  xxx(tp);  dump_mem("b", tp, 64);

   tp = mkargtbl(c);
   xxx(tp);  dump_mem("c", tp, 64);

   tp = mkargtbl(d);
   xxx(tp);  dump_mem("d", tp, 64);

   tp = mkargtbl(e);
   xxx(tp);  dump_mem("e", tp, 64);
#endif
}
