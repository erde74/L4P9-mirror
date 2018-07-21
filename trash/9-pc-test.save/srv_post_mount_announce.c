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


 
static int post_srvfd(char *srvname, int pfd);
 
void int post_mount_srv(Srv *s, char *srvname, char *mntpt, int flag)
{
      int fd[2];
      int rv;
                                                                                   
      if(pipe(fd) < 0) {
	  l4printf_r("pipe: \n");
	  return -1;
      }

      s->infd = s->outfd = fd[1];   // this fd is mounted or posted 
      s->srvfd = fd[0];             // this fd 

      if (srvname && ((mntpt==nil) || (*mntpt==0))) {
	  if (post_srvfd(srvname, s->srvfd) < 0)
	      sysfatal("postfd %s: %r", srvname); 

	  DBGPRN(">>> srv(\"/srv/%s\") -> mount plese >>>\n", srvname);
	  srv(s);

	  close(s->infd);
	  if(s->infd != s->outfd)
	      close(s->outfd);

	  close(s->srvfd);
      }
      else

      if (mntpt && ((srvname==nil) || (*srvname==0))) {
	
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
	  if (post_srvfd(srvname, s->srvfd) < 0)
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


void int  mount_srv(Srv *s, char *mntpt, int flag)
{
      int fd[2];
      int rv;
                                                                                   
      if(pipe(fd) < 0) {
	  l4printf_r("pipe: \n");
	  return ;
      }

      s->infd = s->outfd = fd[1];
      s->srvfd = fd[0];

      if (mntpt == nil)
	  return ;

      srvthread_create(srv, s);

      L4_Sleep(L4_TimePeriod(100000));
      fprint(2, "mount(fd:%d, afd:-1, mntpt:%s, flag:%x, aname:%s)\n", 
	     s->srvfd, mntpt, flag, "");

      rv = mount(s->srvfd, -1, mntpt, flag, "");
      fprint(2, "<mount():%d\n", rv);
      if (rv == -1){
	      perror("pmsrv-l4:mount");
	      // sysfatal("mountsrv %s: %r", srvname);
      }
      return  rv;
}




static int post_srvfd(char *srvname, int pfd)
{
    int fd;
    char buf[80];

    snprint(buf, sizeof buf, "/srv/%s", srvname);
    if(chatty9p)
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
                                                                                   
