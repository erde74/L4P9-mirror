#include  <u.h>
#include  <libc.h>
#include  <l4all.h>

#define   _DBGFLG  1
#include  <l4/_dbgpr.h>

void  echo_thread(int lcfd, char* adir_0, char *ldir_0)
{
    int  dfd;
    char adir[40], ldir[40];
    int  n;
    char buf[256];

    print("========= echosrvthread (%d %s %s)=======\n", lcfd, adir_0,ldir_0);
    strncpy(adir, adir_0, 40);
    strncpy(ldir, ldir_0, 40);

    DBGPRN(">accept(%d, \"%s\")\n", lcfd, ldir);
    dfd = accept(lcfd, ldir); 
    DBGPRN("<accept(%d, \"%s\"):%d\n", lcfd, ldir, dfd);
    if (dfd<0) L4_Sleep(L4_Never);

    while((n = read(dfd, buf, sizeof(buf))) > 0){
	  l4printf("%s\n", buf);
	  write(dfd, buf, n);
    }
    // terminating ----
}

extern L4_ThreadId_t create_start_thread(unsigned pc, unsigned stacktop);

void create_echo_thread(int lcfd, char* adir_0, char * ldir_0)
{
    uint * stk, *sp;
    L4_ThreadId_t  tid;

    stk = malloc(256*4);
    sp = &stk[250];

    *(--sp) = (uint)ldir_0;
    *(--sp) = (uint)adir_0;
    *(--sp) = (uint)lcfd;
    *(--sp) = 0;  // return address area
    tid = create_start_thread((uint)echo_thread, (uint)sp);
}

int  main(void)
{
    int  afd, dfd, lcfd;
    char adir[40], ldir[40];
    int  n;
    char buf[256];

    l4printf("<><> echosrv <><>\n");

    afd = announce("tcp!*!7", adir); 
    DBGPRN("<announce(\"tcp!*!7\"); afd=%d adir=%s\n", afd, adir);
    if(afd < 0)   exits(0);

    for(;;){
        /* listen for a call */
        lcfd = listen(adir, ldir);  
	DBGPRN("<listen(\"%s\", \"%s\"):%d\n", adir, ldir, lcfd);
	if(lcfd < 0)   exits(0);

#if 0 // multi thread server ------------------------
	create_echo_thread(lcfd, adir, ldir);
#else // sigle thread server -----------------------

	DBGPRN(">accept(%d, \"%s\")\n", lcfd, ldir);
	dfd = accept(lcfd, ldir); 
	DBGPRN("<accept(%d, \"%s\"):%d\n", lcfd, ldir, dfd);
	if(dfd < 0)
	    continue;

	while((n = read(dfd, buf, sizeof(buf))) > 0){
	    l4printf("%s\n", buf);
	    write(dfd, buf, n);
	}
	continue;
#endif //----------------------------------
    }
}


