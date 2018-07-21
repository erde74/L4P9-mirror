#include <u.h>
#include <libc.h>
// #include <auth.h>
// #include <fcall.h>

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

// ------[1] MM (Mem-proc) Server ----------------
#define  MM_SERVER    0xC8001        // [0, 50, 1]
#define  MX_PAGER     0xCC001        // [0, 51, 1]
                                                               
static void USED(void * _x, ...){ }
static void SET(void* _x, ...){ }

int debug = 0;
int private;
char *defmnt = 0;
int p[2];
int fd;
int  mfd[2];
int stdio = 0;
char *service;
int  private;

void
usage(void)
{
  l4printf("usage: ex3 [-Dips] [-m mountpoint] [-S srvname]\n", argv0);
}



void
main(int argc, char *argv[] )
{
  int  i;
                                                                     
  l4printf("usage: ex3 [-Dips] [-m mountpoint] [-S srvname]\n");

  for (i = 0; i<argc; i++)
    l4printf("argv[%d]= \"%s\" \n", i, argv[i]);  

 
  ARGBEGIN{
  case 'D':
    debug = 1;
    break;
  case 'i':
    defmnt = 0;
    stdio = 1;
    mfd[0] = 0;
    mfd[1] = 1;
    break;
  case 's':
    defmnt = 0;
    break;
  case 'm':
    defmnt = EARGF(usage());
    break;
  case 'p':
    private++;
    break;
  case 'S':
    defmnt = 0;
    service = EARGF(usage());
    break;
  default:
    usage();
  }ARGEND

     ;


  l4printf("debug=%d defmnt=%s stdio=%d service=%s private=%d\n",
	   debug, defmnt, stdio, service, private);

}


