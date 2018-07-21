//% (C) Bell Lab.  (C2) KM
//%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "../dossrv/iotrack.h"  //%
#include "../dossrv/dat.h"      //%
#include "../dossrv/dosfs.h"    //%
#include "../dossrv/fns.h"      //%

#include "../dossrv/errstr.h"

#if 1 //%------------------------------------------------------
#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>

#define  CORE  1
#define   _DBGFLG  0
#include  <l4/_dbgpr.h>

 extern void dbg9p(Fcall *fp);

 long time(long *t) {  return 0; }

 void USED(void * _x, ...){ }
 void SET(void* _x, ...){ }
 int getpid( ) { return 10; }  // devproc

void error(char *s)
{
    l4printf("dossrv-err: %s => please reboot! exit() not yet implemented.\n", s);
    L4_Sleep(L4_Never);
}

char * printable(char * ss, int len)
{
    static char pbuf[128];
    int i, j = 0;
    char  ch;

    for (i = 0; i<len; i++) {
        ch = ss[i];
	if ((ch >= ' ') && (ch <= '~')) 
	    pbuf[j++] = ch;
	if (j > 80)  break;
    }
    pbuf[j] = 0;
    return  pbuf;
}
#endif //%----------------------------------------------------

#define	Reqsize	(sizeof(Fcall)+Maxfdata)
Fcall	*req;
Fcall	*rep;

uchar	mdata[Maxiosize];
char	repdata[Maxfdata];
uchar	statbuf[STATMAX];
int	errno;
char	errbuf[ERRMAX];
void	rmservice(void);
char	srvfile[64];
char	*deffile;
int	doabort;
int	trspaces;

void	(*fcalls[])(void) = {
	[Tversion]	rversion,
	[Tflush]	rflush,
	[Tauth]	        rauth,
	[Tattach]	rattach,
	[Twalk]		rwalk,
	[Topen]		ropen,
	[Tcreate]	rcreate,
	[Tread]		rread,
	[Twrite]	rwrite,
	[Tclunk]	rclunk,
	[Tremove]	rremove,
	[Tstat]		rstat,
	[Twstat]	rwstat,
};

void
usage(void)
{
	fprint(2, "usage: %s [-v] [-s] [-f devicefile] [srvname]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int stdio, srvfd, pipefd[2];

	rep = malloc(sizeof(Fcall));
	req = malloc(Reqsize);
	if(rep == nil || req == nil)
		panic("out of memory");
	stdio = 0;

#ifdef CORE  //%-----------------------------------------------
	extern ack2hvm(int);

	l4printf_g("<> DOSSRV:%x <>\n", L4_Myself().raw);
	//	sleep(100); //

        if(pipe(pipefd) < 0)
	    error("pipe failed");

	{
	    char  buf[64];
	    int   fd;

	    snprint(buf, sizeof buf, "#s/%s", "dos");
	    fd = create(buf, OWRITE|ORCLOSE, 0666);
	    if(fd < 0)
	      error("create failed");

	    sprint(buf, "%d", pipefd[1]);
	    DBGPRN("dossrv pipefd[0]=%d pipefd[1]=%d fd=%d buf=\"%s\" \n",
		   pipefd[0], pipefd[1], fd, buf);

	    if(write(fd, buf, strlen(buf)) < 0) 
	      error("writing service file");
        }

	iotrack_init();

	ack2hvm(0);  //090330

	io(pipefd[0]);
  
#else  //original------------------------------------------
	ARGBEGIN{
	case ':':
		trspaces = 1;
		break;
	case 'r':
		readonly = 1;
		break;
	case 'v':
		++chatty;
		break;
	case 'f':
		deffile = ARGF();
		break;
	case 's':
		stdio = 1;
		break;
	case 'p':
		doabort = 1;
		break;
	default:
		usage();
	}ARGEND


	if(argc == 0)
	        strcpy(srvfile, "#s/dos2");  //% dos
	else if(argc == 1)
		snprint (srvfile, sizeof srvfile, "#s/%s", argv[0]);
	else
		usage();

	fprint(2, ">>dos2{srvfile:%s deffile:%s stdio:%d r:%d} \n",
	       srvfile, deffile, stdio, readonly);

	if(stdio){
		pipefd[0] = 0;
		pipefd[1] = 1;
	}else{
	        //%	close(0);
	        //%	close(1);
	        //%	open("/dev/null", OREAD);
	        //%	open("/dev/null", OWRITE);

		if(pipe(pipefd) < 0)
			panic("pipe");
		srvfd = create(srvfile, OWRITE|ORCLOSE, 0600);
		if(srvfd < 0)
			panic(srvfile);

		fprint(srvfd, "%d", pipefd[0]);
		//%	close(pipefd[0]);
		//%	atexit(rmservice);
		fprint(2, "%s: serving %s\n", argv0, srvfile);
	}
	srvfd = pipefd[1];

#if  0  //%
	switch(rfork(RFNOWAIT|RFNOTEG|RFFDG|RFPROC|RFNAMEG)){
	case -1:
		panic("fork");
	default:
	        //%		_exits(0);
	        L4_Sleep(L4_Never);  // _exit() not yet implemented.
	case 0:
		break;
	}
#endif //%

	iotrack_init();

	if(!chatty){
	  //%	close(2);
	  //%	open("#c/cons", OWRITE);
	}


	io(srvfd);
	exits(0);
#endif //------------------------------------------------
}


static void  _DXX_(char *ss, int len)
{
  int  i,j,k=0;
  char  qbuf[128], ch;

  if (len<=0) {
    l4printf_b("%s:%d\n", ss, len);
    return;
  }

  for(i=0;i<len;i++) ch = mdata[i];
/*
  for(i=0;i<len;i++)
    if(mdata[i]=='T'){
      for(j=i, k=0;j<len && k<77; j++) {
	ch = mdata[j];
	if (ch >= ' ' && ch <= '}')qbuf[k++] = ch;
      }
      qbuf[k] = 0;
      l4printf_b("%s%s\n", ss, qbuf);
      return;
    }
*/
/*
  { int  i,j;
    l4printf_r("\n");
    for(i=0;i<sizeof(mdata);i++)
    if(mdata[i]=='T'){
      for(j=i+1;mdata[j]=='T' && j<sizeof(mdata);j++);
      l4printf_b("$T:%d ", j-i);
      i = j;
    }
  }
*/

}

#define GBIT32(p)  ((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))  //%

void
io(int srvfd)
{
	int  n, pid, m;

	pid = getpid();

	fmtinstall('F', fcallfmt);
	for(;;){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads.
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error.
		 */
		n = read9pmsg(srvfd, mdata, sizeof mdata);
		_DXX_("dossrv read9pmsg", n);

		if(n < 0)
			break;

		if(n == 0)
			continue;
		if(convM2S(mdata, n, req) == 0)
			continue;

		if(chatty)   //%
			fprint(2, "dossrv %d:<-%F\n", pid, req);

		errno = 0;

		if(_DBGFLG) dbg9p(req);  //%%

		if(!fcalls[req->type])
			errno = Ebadfcall;
		else
			(*fcalls[req->type])();

		if(errno){
			rep->type = Rerror;
			rep->ename = xerrstr(errno);
			//l4printf_c(63, "!dossrv:%s.\n", rep->ename); //%%
		}else{
			rep->type = req->type + 1;
			rep->fid = req->fid;
		}
		rep->tag = req->tag;
		if(chatty)  //%
			fprint(2, "dossrv %d:->%F\n", pid, rep);

		if(_DBGFLG) dbg9p(rep);  //%%

		n = convS2M(rep, mdata, sizeof mdata);
		_DXX_("dossrv -", n);

		if(n == 0)
			panic("convS2M error on write");

		L4_Yield();  //% ??????

		if(0 & n>0x2000){   //%
		    int  i;
		    uchar  *cp = mdata;
		    for(i=0;i<32;i++)print("%02x ", mdata[i]);
		    print("dossrv:reply(%x,%x,%x)\n", mdata, n, GBIT32(mdata)); //%
		}

		if((m = write(srvfd, mdata, n)) != n){
		         l4printf_b(" # %s\n", printable(mdata, (n<80)?n:80));
		         l4printf("mount-write n=%d m=%d\n", n, m);
			 // panic("mount write");
		}
		//if(n>8000){int i;for(i=0;i<16;i++)print("%02x ",mdata[i]);}
	}
#if 1 //%----------------------
	l4printf_b("dossrv:9precv:err => please reboot ! exit() not yet implemented.\n");
	//	close(srvfd);
	L4_Sleep(L4_Never);
#else //original-----------------
	chat("server shut down");
#endif //%---------------------
}

void
rmservice(void)
{
	remove(srvfile);
}

char *
xerrstr(int e)
{
	if (e < 0 || e >= sizeof errmsg/sizeof errmsg[0])
		return "no such error";
	if(e == Eerrstr){
		errstr(errbuf, sizeof errbuf);
		return errbuf;
	}
	return errmsg[e];
}

int
eqqid(Qid q1, Qid q2)
{
	return q1.path == q2.path && q1.type == q2.type && q1.vers == q2.vers;
}
