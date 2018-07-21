//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <auth.h>
//==================================================
#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>

void
incref(Ref *r)   // Tentative: Must be redesigned. 
{
    r->ref++; //_xinc(&r->ref);
}

long
decref(Ref *r)  // Tentative: Must be redesigned. 
{
    return  --r->ref ; //_xdec(&r->ref);
}

void 
final_l4()   // Tentative: Must be rededesigned. 
{
    L4_Sleep(L4_Never);
}


//  char *print9pmsg(Fcall *fp);
extern  L4_ThreadId_t create_start_thread(uint , uint );
extern  void srv(Srv *srv);

static  L4_ThreadId_t l4thread_create(void (*f)(Srv *arg), Srv*  arg, int stksize)
{
    uint  *stk = (uint*) malloc(stksize);
    uint  *sp = &stk[stksize/4 - 2];
    L4_ThreadId_t  tid;

    *(--sp) = 0; // no meaning
    *(--sp) = (unsigned)arg;
    *(--sp) = 0; // Return address area

    tid = create_start_thread((unsigned)f, (unsigned)sp);
    return  tid;
}

//=================================================
static void service1_thread(Srv*);
L4_ThreadId_t  main_tid;

void postmountsrv_l4(Srv *s, char *name, char *mtpt, int flag)
{
	int fd[2];
	L4_ThreadId_t  srv1_tid;
	L4_MsgTag_t    tag;

	if(!s->nopipe){
		if(pipe(fd) < 0)
			sysfatal("pipe: %r");
		s->infd = s->outfd = fd[1];
		s->srvfd = fd[0];
	}
	if(name)
		if(postfd(name, s->srvfd) < 0)
			sysfatal("postfd %s: %r", name);

	main_tid = L4_Myself();
	srv1_tid = l4thread_create(service1_thread, s, 4096);
	tag = L4_Receive(srv1_tid); // synchronize-a

	if(mtpt){
	        if(mount(s->srvfd, -1, mtpt, flag, "") == -1) 
			sysfatal("mount %s: %r", mtpt);
	}else
		close(s->srvfd);
}

static void service1_thread(Srv *s)
{
	L4_LoadMR(0, 0);
	L4_Send(main_tid);  // synchronize-a
	srv(s);
}

/*
 *    postmountsrv_l4(Srv *srv, char *name, char *mtpt, int flag)
 *    {   
 *        int  fd[2];
 *           :
 *        pipe(fd);  // fd[0]: To be posted as /srv/SRVNAME
 *                   // fd[1]: This side is accessed by the Server thread 
 *           :
 *        if (name){
 *             "post /srv/SRVNAME";
 *        } 
 *           :
 *        "Create the server thread and run 'service1_thread'"  <-----> []---------------+
 *           :                                                           |service1_thread|        
 *                                                                       |               |
 *        if (mtpt){                                                     +---------------+
 *           "Mount the  fd[0] onto the mount point 'mtpt'.
 *           "Here, we communicate with the above-created service1_thread.
 *        }
 *        :
 *    }
 *
 */




//=================================================
static void listen_thread(Srv*);
static void service2_thread(Srv*);
static char *getremotesys(char*);

void listensrv_l4(Srv *os, char *addr)
{
	Srv *s;
	L4_ThreadId_t   listen_tid;
	L4_MsgTag_t     tag;

	s = emalloc9p(sizeof *s);
	*s = *os;
	s->addr = estrdup9p(addr);

	main_tid = L4_Myself();
	listen_tid = l4thread_create(listen_thread, s, 4096);
	tag = L4_Receive(listen_tid); // synchronize-a
}

static void listen_thread(Srv *os)
{
	char ndir[NETPATHLEN], dir[NETPATHLEN];
	int ctlfd, datafd, nctl;
	Srv  *s;
	
	L4_LoadMR(0, 0);
	L4_Send(main_tid);  // synchronize-a

	ctlfd = announce(os->addr, dir);
	print("  announce(%s, %s):%d\n", os->addr, dir, ctlfd); //%

	if(ctlfd < 0){
		fprint(2, "%s: announce %s: %r", argv0, os->addr);
		return;
	}

	for(;;){
		nctl = listen(dir, ndir);
		print("  listen(%s, %s):%d\n", dir, ndir, nctl); //%
		if(nctl < 0){
			fprint(2, "%s: listen %s: %r", argv0, os->addr);
			break;
		}
		
		datafd = accept(ctlfd, ndir);
		print("  accept(%d, %s):%d\n", ctlfd, ndir, datafd); //%
		if(datafd < 0){
			fprint(2, "%s: accept %s: %r\n", argv0, ndir);
			continue;
		}

		s = emalloc9p(sizeof *s);
		*s = *os;
		s->addr = getremotesys(ndir);
		s->infd = s->outfd = datafd;
		s->fpool = nil;
		s->rpool = nil;
		s->rbuf = nil;
		s->wbuf = nil;

		l4thread_create(service2_thread, s, 4096);
	}
	free(os->addr);
	free(os);
}

static void service2_thread(Srv *s)
{
  print("  >service2_thread()\n");   //%
	srv(s);
	close(s->infd);
	free(s->addr);
	free(s);
}

static char* getremotesys(char *ndir)
{
	char buf[128], *serv, *sys;
	int fd, n;

	snprint(buf, sizeof buf, "%s/remote", ndir);
	sys = nil;
	fd = open(buf, OREAD);
	if(fd >= 0){
		n = read(fd, buf, sizeof(buf)-1);
		if(n>0){
			buf[n-1] = 0;
			serv = strchr(buf, '!');
			if(serv)
				*serv = 0;
			sys = estrdup9p(buf);
		}
		close(fd);
	}
	if(sys == nil)
		sys = estrdup9p("unknown");
	return sys;
}

/*
 *    listensrv_l4(Srv *os, char *addr)
 *    {   
 *        int  fd[2];
 *           :
 *        "Create the listen thread <--> listen_thread
 *                                   []-----------------+
 *                                    |                 |
 *                                    | anounce(...)    |
 *                                    |   :             |
 *                                    | for(;;){        |
 *                                    |    listen(...); |
 *                                    |       :         |
 *                                    |    accept(...)  |
 *                                    |       :         |
 *                                    |    "Create serve thread"  <--->
 *                                    |       :         |            []--------------+
 *                                    | }               |             | serv2_thread |
 *                                    |                 |             |              |
 *                                    +-----------------+             +--------------+
 *        return;
 *    }
 *
 */

