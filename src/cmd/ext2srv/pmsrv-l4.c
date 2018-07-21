 
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "dat.h"
#include "fns.h"

//%------------------------------------------------------

#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>
 
#define   _DBGFLG  1
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b
 
char *print9pmsg(Fcall *fp);
 
//%----------------------------------------------------
 
 
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
                                                                                   
