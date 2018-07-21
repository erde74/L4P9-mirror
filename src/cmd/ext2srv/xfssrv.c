//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "dat.h"
#include "fns.h"

#include "errstr.h"

#if 1 //%--------------------------------
#include  <l4all.h>

#define  PRN  l4printf_b

void USED(void* x, ...) { }
void SET(void* x, ...) { }

static uint stk[128];
void syncthread( )
{
    // print("ext2srv-sync<%x>\n", L4_Myself());
    while(1){ 
        L4_Sleep(L4_TimePeriod( 5000000UL)); // 5 sec
	syncbuf();
    }
}


#endif //%--------------------------------

#define  PRN  l4printf_b

int	errno;
int rdonly;
char	*srvfile;
char	*deffile;

extern void iobuf_init(void);
extern Srv ext2srv;

void
usage(void)
{
	fprint(2, "usage: %s [-v] [-s] [-r] [-D] [-p passwd] [-g group] [-f devicefile] [srvname]\n", argv0);
	exits("usage");
}

/*void handler(void *v, char *sig)
{
	USED(v,sig);
	syncbuf();
	noted(NDFLT);
}*/

#if 0 //%-----------------------------
void  main( ) 
{
        int  argc = 0; 
        char **argv = 0;
        int  stdio;
	stdio = 0;

	L4_Sleep(L4_TimePeriod(5000*1000));
	l4printf_g(">>>>>> ext2srv <<<<<< \n");

#else //---------------------------------
void
main(int argc, char **argv)
{
	int stdio;
	stdio = 0;
	//	int argc = 0;    //%
	//	char **argv =0;  //%

	l4printf_g(">>>>>> ext2srv <<<<<< \n");

	ARGBEGIN{
	case 'D':
		++chatty9p;
		break;
	case 'v':
		++chatty;
		break;
	case 'f':
		deffile = ARGF();
		break;
	case 'g':
		gidfile(ARGF());
		break;
	case 'p':
		uidfile(ARGF());
		break;
	case 's':
		stdio = 1;
		break;
	case 'r':
		rdonly = 1;
		break;
	default:
		usage();
	}ARGEND
#endif //%---------------------------

	if(argc == 0)
		srvfile = "ext2";
	else if(argc == 1)
		srvfile = argv[0];
	else
		usage();

	chat("ext2srv: argc=%d chatty=%d chatty9p=%d stdio=%d srvfile=%s \n",
	     argc, chatty, chatty9p, stdio, srvfile);
	iobuf_init();
	/*notify(handler);*/

	if(!chatty){
	  //%	close(2);
	  //%	open("#c/cons", OWRITE);
	}

#if 1 //%-----------------------------
	create_start_thread(syncthread, &stk[120]);
#endif //%--------------------------------

	if(stdio){
		srv(&ext2srv);
	}else{
	        // chat("%s %d: serving %s\n", argv0, getpid(), srvfile);
		postmountsrv(&ext2srv, srvfile, 0, 0);
	}
	//% exits(0);
	l4printf_r("Ext2srv exits\n");
}

char *
xerrstr(int e)
{
	if (e < 0 || e >= sizeof errmsg/sizeof errmsg[0])
		return "no such error";
	else
		return errmsg[e];
}
