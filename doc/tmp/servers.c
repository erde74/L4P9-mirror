////////// plan9/ramfs.c ////////////////////////
void
usage(void)
{
    fprint(2, "usage: %s [-Dips] [-m mountpoint] [-S srvname]\n", argv0);
    exits("usage");
}
~

void
main(int argc, char *argv[])
{
    Ram *r;
    char *defmnt;
    int p[2];
    int fd;
    int stdio = 0;
    char *service;

    service = "ramfs";
    defmnt = "/tmp";
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

    if(pipe(p) < 0)
       error("pipe failed");
    if(!stdio){
        mfd[0] = p[0];
	mfd[1] = p[0];
	if(defmnt == 0){
	    char buf[64];
	    snprint(buf, sizeof buf, "#s/%s", service);
	    fd = create(buf, OWRITE|ORCLOSE, 0666);
	    if(fd < 0)
	        error("create failed");
	    sprint(buf, "%d", p[1]);
	    if(write(fd, buf, strlen(buf)) < 0)
	        error("writing service file");
	}
    }

    user = getuser();
    notify(notifyf);
    nram = 1;
    r = &ram[0];
    r->busy = 1;
    r->data = 0;
    r->ndata = 0;
    r->perm = DMDIR | 0775;
    r->qid.type = QTDIR;
    r->qid.path = 0LL;
    r->qid.vers = 0;
    r->parent = 0;
    r->user = user;
    r->group = user;
    r->muid = user;
    r->atime = time(0);
    r->mtime = r->atime;
    r->name = estrdup(".");

    if(debug)
        fmtinstall('F', fcallfmt);
    switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){
    case -1:
      error("fork");
    case 0:
      close(p[1]);
      io();
      break;
    default:
      close(p[0]);    /* don't deadlock if child fails */
      if(defmnt && mount(p[1], -1, defmnt, MREPL|MCREATE, "") < 0)
	error("mount failed");
    }
    exits(0);
}



///////////////// plan9/dossrv.c ////////////////////////////////
void  usage(void)
{
    fprint(2, "usage: %s [-v] [-s] [-f devicefile] [srvname]\n", argv0);
    exits("usage");
}


void main(int argc, char **argv)
{
    int stdio, srvfd, pipefd[2];

    rep = malloc(sizeof(Fcall));
    req = malloc(Reqsize);
    if(rep == nil || req == nil)
      panic("out of memory");
    stdio = 0;
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
	 strcpy(srvfile, "#s/dos");
     else if(argc == 1)
         snprint(srvfile, sizeof srvfile, "#s/%s", argv[0]);
     else
         usage();

    if(stdio){
        pipefd[0] = 0;
	pipefd[1] = 1;
    }else{
        close(0);
	close(1);
	open("/dev/null", OREAD);
	open("/dev/null", OWRITE);
	if(pipe(pipefd) < 0)
	  panic("pipe");
	srvfd = create(srvfile, OWRITE|ORCLOSE, 0600);
	if(srvfd < 0)
	  panic(srvfile);
	fprint(srvfd, "%d", pipefd[0]);
	close(pipefd[0]);
	atexit(rmservice);
	fprint(2, "%s: serving %s\n", argv0, srvfile);
    }
    srvfd = pipefd[1];

    switch(rfork(RFNOWAIT|RFNOTEG|RFFDG|RFPROC|RFNAMEG)){
    case -1:
        panic("fork");
    default:
        _exits(0);
    case 0:
        break;
    }

    iotrack_init();

    if(!chatty){
        close(2);
	open("#c/cons", OWRITE);
    }

    io(srvfd);
    exits(0);
}

void io(int srvfd)
{
    int n, pid;

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
      if(n < 0)
	break;
      if(n == 0)
	continue;
      if(convM2S(mdata, n, req) == 0)
	continue;

      if(chatty)
	fprint(2, "dossrv %d:<-%F\n", pid, req);

      errno = 0;
      if(!fcalls[req->type])
	errno = Ebadfcall;
      else
	(*fcalls[req->type])();
      if(errno){
	rep->type = Rerror;
	rep->ename = xerrstr(errno);
      }else{
	rep->type = req->type + 1;
	rep->fid = req->fid;
      }
      rep->tag = req->tag;
      if(chatty)
	fprint(2, "dossrv %d:->%F\n", pid, rep);
      n = convS2M(rep, mdata, sizeof mdata);
      if(n == 0)
	panic("convS2M error on write");
      if(write(srvfd, mdata, n) != n)
	panic("mount write");
    }
    chat("server shut down");
}



///////////// plan9/ext2srv /////////////////////////////
void usage(void)
{
    fprint(2, "usage: %s [-v] [-s] [-r] [-p passwd] [-g group] [-f devicefile] [srvname]\n", argv0);
    exits("usage");
}


void main(int argc, char **argv)
{
    int stdio;

    stdio = 0;
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

    if(argc == 0)
       srvfile = "ext2";
    else if(argc == 1)
       srvfile = argv[0];
    else
       usage();

    iobuf_init();
    /*notify(handler);*/

    if(!chatty){
      close(2);
      open("#c/cons", OWRITE);
    }
    if(stdio){
      srv(&ext2srv);
    }else{
      chat("%s %d: serving %s\n", argv0, getpid(), srvfile);
      postmountsrv(&ext2srv, srvfile, 0, 0);
    }
    exits(0);
}



/////////// plan9:usb/usbsfs //////////////////////////////////////

void usage(void)
{
    fprint(2, "Usage: %s [-rsD] [-m mountpoint] [-l lun] [ctrno id]\n", argv0);
    exits("Usage");
}

void main(int argc, char **argv)
{
    char *srvname = "ums";  //%
    //%     char *srvname = nil;
    char *mntname = "/n/ums";
    char *p;
    int stdio = 0;
    int epin, epout;
    int ctlrno = 0, id = 0;
    int lun = 0;
    Biobuf *f;
    Device *d;
    char buf[64];
    extern int debug;

    ARGBEGIN{
    case 'd':
      debug = Dbginfo;
      break;
    case 's':
      ++stdio;
      break;
    case 'm':
      mntname = ARGF();
      break;
    case 'D':
      ++chatty9p;
      break;
    case 'r':
      ++noreset;
      break;
    case 'l':
      lun = atoi(EARGF(usage()));
      break;
    default:
      usage();
    }ARGEND

    if (argc == 0) {
        for (ctlrno = 0; ctlrno < 16; ctlrno++) {
	  for (id = 1; id < 128; id++) {
	    sprint(buf, "/dev/usb%d/%d/status", ctlrno, id);
	    f = Bopen(buf, OREAD);
	    if (f == 0)
	      break;
	    while ((p = Brdline(&f->_Biobufhdr, '\n')) != 0) {    //%
	      p[Blinelen(&f->_Biobufhdr)-1] = '\0';         //%
	      if (strstr(p, "0x500508") != 0 || strstr(p, "0x500608") != 0) {
		Bterm(&f->_Biobufhdr);     //%
		goto found;
	      }
	    }
	    Bterm(&f->_Biobufhdr);    //%
	  }
	}
	sysfatal("No usb storage device found");
    } else if (argc == 2 && isdigit(argv[0][0]) && isdigit(argv[1][0])) {
      ctlrno = atoi(argv[0]);
      id = atoi(argv[1]);
    } else
      usage();
 found:

    d = opendev(ctlrno, id);
    if (describedevice(d) < 0)
        sysfatal("%r");
    if (findendpoints(d, &epin, &epout) < 0)
        sysfatal("bad usb configuration");

    sprint(buf, "/dev/usb%d/%d", ctlrno, id);
    if (umsinit(&ums, buf, epin, epout, lun) < 0)
        sysfatal("device error");
    starttime = time(0);
    owner = getuser();
    if (stdio)
        srv(&usbssrv);
    else
        //% postmountsrv(&usbssrv, srvname, mntname, 0);
        postmountsrv(&usbssrv, srvname, mntname, MREPL|MCREATE);
    exits(0);
}



//////////// lp49: ramfs ///////////////////////

void
main(int argc, char *argv[])
{
    Ram *r;
    //%     char *defmnt = 0;
    //%     int p[2];
    int fd;
    int stdio = 0;
    char *service;
    int  i;

#if 0 //%------------------------------------------------
    extern void wait_preproc(int);
    wait_preproc(0);

    PRN(">ramfs:main( ) \n");
    service = "ramfs";
    defmnt = "/tmp";

    debug = 0;
#else  //%----------------------------------------------
    service = "ramfs";
    defmnt = "/tmp";

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
    }ARGEND ;
#endif  //%--------------------------------------------------------

        // l4printf_s("debug=%d defmnt=%s stdio=%d service=%s private=%d\n",
        //       debug, defmnt, stdio, service, private);

        /*
         * default-option ==>  mount the pipe p[1] onto /tmp.
         *        defmnt = "/tmp";
         * -s option   ==>  Post the pipe p[1] on /srv/ramfs
         *        defmnt = 0;    service = "ramfs";
         * -S XXX option  ==> Post the pipe p[1] on /srv/XXX
         */

    if(pipe(p) < 0)
        error("ramfs: pipe failed");
    DBGPRN(">ramfs:pipe[0]=%d pipe[1]=%d ", p[0], p[1]);

    if(!stdio){
        mfd[0] = p[0];
	mfd[1] = p[0];
	if(defmnt == 0){
          char buf[64];
	  snprint(buf, sizeof buf, "#s/%s", service);
	  fd = create(buf, OWRITE|ORCLOSE, 0666);
	  if(fd < 0)
	    error("create failed");
	  
	  L4_Yield();
	  sprint(buf, "%d", p[1]);
	  if(write(fd, buf, strlen(buf)) < 0)
	    error("writing service file");
	  
	  PRN("ramfs starting: \"/srv/%s\" pipe<%d %d> debug=%d \n",
	      service, p[0], p[1], debug);
	}
    }
                                                                                                 
    user = getuser();
    //%     notify(notifyf);
    nram = 1;
    r = &ram[0];
    r->busy = 1;
    r->data = 0;
    r->ndata = 0;
    r->perm = DMDIR | 0775;
    r->qid.type = QTDIR;
    r->qid.path = 0LL;
    r->qid.vers = 0;
    r->parent = 0;
    r->user = user;
    r->group = user;
    r->muid = user;
    r->atime = time(0);
    r->mtime = r->atime;
    r->name = estrdup(".");
    
    PRN(">ramfs starting... ");

    if(debug)
        fmtinstall('F', fcallfmt);

#if 1 //%-------------------------------------------
    if (defmnt)  //  /tmp
      {
	static unsigned  mystack[128];
	L4_ThreadId_t    tid;

	void mount_thread();
	extern L4_ThreadId_t create_start_thread(unsigned pc, unsigned stkp);

	tid = create_start_thread((unsigned)mount_thread, &mystack[120]);

	io();
      }
    else {   // -s option:  /srv/ramfs  ----------------------

      extern post_nextproc(int);
      io( );
      close(mfd[0]);
      close(mfd[1]);
    }

#else //original-----------------------------------
    switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){
    case -1:
        error("fork");
    case 0:
        close(p[1]);
	io();
	break;
    default:
        close(p[0]);    /* don't deadlock if child fails */
	if(defmnt && mount(p[1], -1, defmnt, MREPL | MCREATE, "") < 0)
	    error("mount failed");
    }
    exits(0);
#endif //%------------------------------------------
}

//%
static  void mount_thread()
{
    L4_Yield();
    L4_Sleep(L4_TimePeriod(1000000));
    DBGPRN(">%s() ", __FUNCTION__);

    if(defmnt && mount(p[1], -1, defmnt, MREPL|MCREATE, "") < 0)
      error("mount failed");

    L4_Sleep(L4_Never);
}



//////////////////// lp49; dossrv ///////////////////// 
void
main(int argc, char **argv)
{
    int stdio, srvfd, pipefd[2];
 
    rep = malloc(sizeof(Fcall));
    req = malloc(Reqsize);
    if(rep == nil || req == nil)
      panic("out of memory");
    stdio = 0;
 
#if 1   //%-----------------------------------------------
    extern wait_preproc(int);
    extern post_nextproc(int);
 
    wait_preproc(0);
    l4printf_g("<> DOSSRV [%x] <>\n", L4_Myself().raw);
 
    DBGPRN("> %s()\n", __FUNCTION__);
 
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
       strcpy(srvfile, "#s/dos");
    else if(argc == 1)
       snprint(srvfile, sizeof srvfile, "#s/%s", argv[0]);
    else
      usage();

    if(stdio){
        pipefd[0] = 0;
	pipefd[1] = 1;
    }else{
        close(0);
	close(1);
	open("/dev/null", OREAD);
	open("/dev/null", OWRITE);
	if(pipe(pipefd) < 0)
	  panic("pipe");
	srvfd = create(srvfile, OWRITE|ORCLOSE, 0600);
	if(srvfd < 0)
	  panic(srvfile);
	fprint(srvfd, "%d", pipefd[0]);
	close(pipefd[0]);
	atexit(rmservice);
	fprint(2, "%s: serving %s\n", argv0, srvfile);
    }
    srvfd = pipefd[1];

    switch(rfork(RFNOWAIT|RFNOTEG|RFFDG|RFPROC|RFNAMEG)){
    case -1:
      panic("fork");
    default:
      _exits(0);
    case 0:
      break;
    }

    iotrack_init();

    if(!chatty){
      close(2);
      open("#c/cons", OWRITE);
    }

    post_nextproc(0); //%

    io(srvfd);
    exits(0);
#endif //------------------------------------------------
}

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

    if(chatty)
      fprint(2, "dossrv %d:<-%F\n", pid, req);

    errno = 0;

    if(_DBGFLG) print9pmsg(req);  //%%

    if(!fcalls[req->type])
      errno = Ebadfcall;
    else
      (*fcalls[req->type])();

    if(errno){
      rep->type = Rerror;
      rep->ename = xerrstr(errno);
      l4printf_c(63, "!dossrv:%s.\n", rep->ename); //%%
    }else{
      rep->type = req->type + 1;
      rep->fid = req->fid;
    }
    rep->tag = req->tag;
    if(chatty)
      fprint(2, "dossrv %d:->%F\n", pid, rep);

    if(_DBGFLG) print9pmsg(rep);  //%%

    n = convS2M(rep, mdata, sizeof mdata);
    _DXX_("dossrv -", n);

    if(n == 0)
      panic("convS2M error on write");

    L4_Yield();  //% ??????

    if( (m = write(srvfd, mdata, n)) != n){
      l4printf_b(" # %s\n", printable(mdata, (n<80)?n:80));
      l4printf("mount-write n=%d m=%d\n", n, m);
      // panic("mount write");
    }
  }
#        l4printf_b("dossrv:9precv:err => please reboot ! exit() not yet implemented.\n");
  //      close(srvfd);
  L4_Sleep(L4_Never);
#else //original-----------------
  chat("server shut down");
#endif //%---------------------
}



//////////////// lp49: ext2srv ///////////////////

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
    //      int argc = 0;    //%
    //      char **argv =0;  //%

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
      //%   close(2);
      //%   open("#c/cons", OWRITE);
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


//---------------------------------------
void
  postmountsrv(Srv *s, char *name, char *_mtpt, int _flag)
  {
    int fd[2];

    if(pipe(fd) < 0) {
      l4printf_r("pipe: \n");
      return ;
    }
 
    s->infd = s->outfd = fd[1];
    s->srvfd = fd[0];
 
    chat("pipe=<%d %d>\n", fd[0], fd[1]);
  
    {
      int fd;
      char buf[32], buf2[8];
  
      snprint(buf, sizeof buf, "/srv/%s", name);
      chat("postfd %s\n", buf);
 
      fd = create(buf, OWRITE|ORCLOSE|OCEXEC, 0666);  //0600
 
      if(fd < 0){
	l4printf_b("create \"%s\" failed\n", buf);
	return ;
      }
 
 
      sprint(buf2, "%d", s->srvfd);
      if(write(fd, buf2, strlen(buf2)) < 0){
	if(chatty)
	  l4printf_r("write fails: \n");
	close(fd);
	return ;
      }
 
      chat("ext2srv: \"%s\"(fd=%d) created, and pipe[0](fd=%s) is postfd.\n",
	   buf, fd, buf2);
    }
 
    chat(">>> ext2srv() >>>\n");
    srv(s);
    chat("<<< srv <<<\n");
 
    close(s->infd);
    if(s->infd != s->outfd)
      close(s->outfd);

    close(s->srvfd);
  }



//////// plan9: lib9p/postmountsrv /////////////////////

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <auth.h>

static void postproc(void*);

void
_postmountsrv(Srv *s, char *name, char *mtpt, int flag)
{
	int fd[2];

	if(!s->nopipe){
		if(pipe(fd) < 0)
			sysfatal("pipe: %r");
		s->infd = s->outfd = fd[1];
		s->srvfd = fd[0];
	}
	if(name)
		if(postfd(name, s->srvfd) < 0)
			sysfatal("postfd %s: %r", name);

	if(_forker == nil)
		sysfatal("no forker");
	_forker(postproc, s, RFNAMEG);

	/*
	 * Normally the server is posting as the last thing it does
	 * before exiting, so the correct thing to do is drop into
	 * a different fd space and close the 9P server half of the
	 * pipe before trying to mount the kernel half.  This way,
	 * if the file server dies, we don't have a ref to the 9P server
	 * half of the pipe.  Then killing the other procs will drop
	 * all the refs on the 9P server half, and the mount will fail.
	 * Otherwise the mount hangs forever.
	 *
	 * Libthread in general and acme win in particular make
	 * it hard to make this fd bookkeeping work out properly,
	 * so leaveinfdopen is a flag that win sets to opt out of this
	 * safety net.
	 */
	if(!s->leavefdsopen){
		rfork(RFFDG);
		rendezvous(0, 0);
		close(s->infd);
		if(s->infd != s->outfd)
			close(s->outfd);
	}

	if(mtpt){
		if(amount(s->srvfd, mtpt, flag, "") == -1)
			sysfatal("mount %s: %r", mtpt);
	}else
		close(s->srvfd);
}

static void
postproc(void *v)
{
	Srv *s;

	s = v;
	if(!s->leavefdsopen){
		rfork(RFNOTEG);
		rendezvous(0, 0);
		close(s->srvfd);
	}
	srv(s);
}


//// rfork.c -------------------------
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

static void
rforker(void (*fn)(void*), void *arg, int flag)
{
	switch(rfork(RFPROC|RFMEM|RFNOWAIT|flag)){
	case -1:
		sysfatal("rfork: %r");
	default:
		return;
	case 0:
		fn(arg);
		_exits(0);
	}
}

void
listensrv(Srv *s, char *addr)
{
	_forker = rforker;
	_listensrv(s, addr);
}

void
postmountsrv(Srv *s, char *name, char *mtpt, int flag)
{
	_forker = rforker;
	_postmountsrv(s, name, mtpt, flag);
}



//////////////////// lp49: usbsfs/pmsrv-l4.c //////////

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>
 
#define   _DBGFLG  1
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b
 
//  char *print9pmsg(Fcall *fp);
extern  L4_ThreadId_t create_start_thread(uint , uint );
extern  void srv(Srv *srv);

static  uint  stackarea[1024];
static  int srvthread_create(void (*f)(Srv *arg), Srv*  arg)
{
    uint  *sp = &stackarea[1020];
    L4_ThreadId_t  tid;

    *(--sp) = 0; // no meaning
    *(--sp) = (unsigned)arg;
    *(--sp) = 0; // Return address area

    tid = create_start_thread((unsigned)f, (unsigned)sp);
    return  tid.raw;
}


 
static int _postfd(char *srvname, int pfd);
 
void postmountsrv(Srv *s, char *srvname, char *mntpt, int flag)
{
      int fd[2];
      int rv;
                                                                                   
      if(pipe(fd) < 0) {
	  l4printf_r("pipe: \n");
	  return ;
      }

      s->infd = s->outfd = fd[1];
      s->srvfd = fd[0];
      DBGPRN("pipe=<%d %d>\n", fd[0], fd[1]);
 

      if (srvname && (mntpt==nil)) {
	  if (_postfd(srvname, s->srvfd) < 0)
	      sysfatal("postfd %s: %r", srvname); 

	  DBGPRN(">>> srv(\"/srv/%s\") -> mount plese >>>\n", srvname);
	  srv(s);

	  close(s->infd);
	  if(s->infd != s->outfd)
	      close(s->outfd);

	  close(s->srvfd);
      }
      else

      if (mntpt && (srvname==nil)) {
	
	  DBGPRN(">>> srv(mntpt=%s) >>>\n", mntpt);
          srvthread_create(srv, s);

	  L4_Sleep(L4_TimePeriod(100000));
          fprint(2, "mount(fd:%d, afd:-1, mntpt:%s, flag:%x, aname:%s)\n", 
		 s->srvfd, mntpt, flag, "");
	  rv = mount(s->srvfd, -1, mntpt, flag, "");
	  fprint(2, "<mount():%d\n", rv);
	  if (rv == -1){
	      perror("pmsrv-l4:mount");
	      sysfatal("mountsrv %s: %r", srvname);
	  }
      }
      else

      if (srvname && mntpt) {
	  if (_postfd(srvname, s->srvfd) < 0)
	      sysfatal("postfd %s: %r", srvname); 

	  DBGPRN(">>> srv(mntpt=%s) >>>\n", mntpt);
          srvthread_create(srv, s);

          fprint(2, "mount(fd:%d, afd:-1, mntpt:%s, flag:%x, aname:%s)\n", 
		 s->srvfd, mntpt, flag, "");
	  rv = mount(s->srvfd, -1, mntpt, flag, "");
	  fprint(2, "<mount():%d\n", rv);
	  if (rv == -1){
	      perror("pmsrv-l4:mount");
	      sysfatal("mountsrv %s: %r", srvname);
	  }
      }

      else {
	sysfatal("postmntsrv: neither srvname nor mntpt specified");
      }
}


static int _postfd(char *srvname, int pfd)
{
    int fd;
    char buf[80];

    snprint(buf, sizeof buf, "/srv/%s", srvname);
    //    if(chatty9p)
        fprint(2, "postfd %s\n", buf);
	fd = create(buf, OWRITE|ORCLOSE|OCEXEC, 0666);  // 0600
    if(fd < 0){
        if(chatty9p)
	    fprint(2, "create fails: %r\n");
	return -1;
    }
    if(fprint(fd, "%d", pfd) < 0){
        if(chatty9p)
	    fprint(2, "write fails: %r\n");
	close(fd);
	return -1;
    }
    if(chatty9p)
        fprint(2, "postfd successful\n");
    return 0;
}


void dump_mem(char *title, unsigned char *start, unsigned size)
{
    int  i, j, k;
    unsigned char c;
    char  buf[128];
    char  elem[32];

    l4printf_b("* dump mem <%s>\n", title);
    for(i=0; i<size; i+=16) {
        buf[0] = 0;  
	for (j=i; j<i+16; j+=4) {
	  if (j%4 == 0) strcat(buf, " ");
	  for(k=3; k>=0; k--) {
	      c = start[j+k];
	      l4snprintf(elem, 32, "%.2x", c);
	      strcat(buf, elem);
	  }
	}

	strcat(buf, "  ");
	for (j=i; j<i+16; j++) {
	    c = start[j];
	    if ( c >= ' ' && c < '~')
	        l4snprintf(elem, 32, "%c", c);
	    else
	        l4snprintf(elem, 32, ".");
	    strcat(buf, elem);
	}
	l4printf_b("%s\n", buf);
    }
}
                                                                                   
