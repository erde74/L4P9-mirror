
#include <stdio.h>

/*   Stack frame in Proc-call
 *
 * int  A, B, C;
 * int foo(int *x, int *y)
 * {
 *   int  a, b;
 *    a = A;
 *    b = B;
 *    *x = a;
 *    *y = b;
 *    return 7;
 *  }
 *      |            |
 *      |------------| -8  <-- SP
 *      |     b      |
 *      |------------| -4
 *      |     a      |
 *      |------------| <-- FP 
 *      | rtnFP=ebp  |
 *      |------------| 
 *      | rtnIP=eip  |
 *      |------------| +4
 *      |     x      |
 *      |------------| +8
 *      |     y      |
 *      |------------| +12
 *      |            |
 */

typedef struct Label Label;
struct Label
{
  unsigned   sp;
  unsigned   pc;
  unsigned   fp;
  //  unsigned   dmy;
};

#define  NERR   32
#define  ERRMAX 128

struct Proc
{
  int     nerrlab;
  Label   errlab[NERR];
  char    *syserrstr; 
  char    *errstr;    
  char    errbuf0[ERRMAX];
  char    errbuf1[ERRMAX];
  char    genbuf[128];
}  pcb;
struct Proc  *up = &pcb;

#if 0
TEXT gotolabel(SB), $0
        MOVL    label+0(FP), AX
        MOVL    0(AX), SP    /* restore sp */
        MOVL    4(AX), AX    /* put return pc on the stack */
        MOVL    AX, 0(SP)
        MOVL    $1, AX       /* return 1 */
        RET
#endif

extern int gotolabel(Label *label);

#if 0 
TEXT setlabel(SB), $0
      MOVL    label+0(FP), AX
      MOVL    SP, 0(AX)        /* store sp */
      MOVL    0(SP), BX        /* store return pc */
      MOVL    BX, 4(AX)
      MOVL    $0, AX           /* return 0 */
      RET
#endif

extern  int setlabel(Label *label);

void prn(){ printf("# %d %\n", pcb.errlab[pcb.nerrlab-1]); }

#define  waserror()   (pcb.nerrlab++, setlabel(&pcb.errlab[pcb.nerrlab-1]))

#define  poperror()   pcb.nerrlab--

void nexterror(void)
{
  gotolabel(&pcb.errlab[--pcb.nerrlab]);
}

void error(char *err)
{
  printf("> %s \n", err);
  setlabel(&pcb.errlab[NERR-1]);
  nexterror();
}

pr_save()
{
    int  i;
    printf("< errlab[] nerrlab=%d\n", pcb.nerrlab);
    for(i = 0; i <= pcb.nerrlab-1; i++)
    printf(" label=%x sp:%x ip=%x fp=%x  \n", 
	   &pcb.errlab[i], pcb.errlab[i].sp, 
	   pcb.errlab[i].pc, pcb.errlab[i].fp);
}


void alpha(int x)
{
  int  a,b,c;
  a = 1;
  
  if (x) error("Oh my mistake !");

}

void beta(int x)
{
  int  a,b,c;
  alpha(x);
}


int main()
{
  pr_save();
  if (!waserror() ) {
      printf("Hello \n");
      pr_save();
      beta(1); //error("XXX");

      printf("World \n");
      poperror();
  }
  else {
        printf("Goodby \n");
	pr_save();
  }
  printf("-------\n");
  pr_save();
  if (!waserror() ) {
      printf("Hello \n");
      pr_save();
      beta(0);  //   error("XXX");

      printf("World \n");
      poperror();
      pr_save();
  }
  else {
        printf("Goodby \n");
	pr_save();
  }
  return 0;
}
