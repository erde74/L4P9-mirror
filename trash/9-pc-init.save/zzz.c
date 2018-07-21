//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//% (C) Bell Lab
//% (C2) KM

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "boot.h"

#include  <l4all.h>   //%

char	cputype[64];
char	sys[2*64];
char 	reply[256];
int	printcol;
int	mflag;
int	fflag;
int	kflag;

char	*bargv[Nbarg];
int	bargc;

//%  static Method	*rootserver(char*);
//%  static void	swapproc(void);
//%  static void	kbmap(void);

//-----------------------------------------
int cd_start( ) 
{
    int  fd, rc;
    l4printf_g("cd / \n");
    chdir("/");

#if 1
    l4printf_g("bind -a #S /dev \n");
    bind("#S", "/dev", MAFTER);
#endif
    sleep(100); // 

    l4printf_g("mount -a /srv/9660 /t /dev/sdD0/data\n");
    fd = open("/srv/9660", ORDWR);
    if (fd < 0) {
        l4printf_g("cannot open /srv/9660\n");
        return -1;
    }

    rc = mount(fd, -1, "/t", MAFTER, "/dev/sdD0/data");
    return  rc;
}

void boot(int argc, char *argv[])
{
	int fd, afd;
	Method *mp;
	char *cmd, cmdbuf[64], *iargv[16];
	char rootbuf[64];
	int islocal, ishybrid;
	char *rp = nil, *rsp = nil;
	int iargc, n;
	char buf[32];
	AuthInfo *ai;
	int  rc;

	fmtinstall('r', errfmt);

	l4printf_b("bind  -b #c /dev\n"); //%
	bind("#c", "/dev", MBEFORE);

	open("/dev/cons", OREAD);
	open("/dev/cons", OWRITE);
	open("/dev/cons", OWRITE);

	// init() will reinitialize its namespace.
#if 1 //%----------------------------------------
	sleep(500); 

	if(chdir("/") < 0) fatal("chdir");
 
	l4printf_b("bind  #d /fd\n");
	rc = bind("#d", "/fd", 0);       // bind the DEVDUP+
	if (rc < 0) fatal("bind #d") ;

	l4printf_b("bind -c #e /env \n");
	rc = bind("#e", "/env", MBEFORE|MCREATE); // bind the DEVENV
	if (rc < 0) fatal("bind #c") ;

	l4printf_b("bind  #p /proc \n");
	rc = bind("#p", "/proc", 0);             // bind the DEVPROC+
	if (rc < 0) fatal("bind #p") ; 

	l4printf_b("bind -c #s /srv \n");
	rc = bind("#s", "/srv", MREPL|MCREATE);  // bind the DEVSRV
	if (rc < 0) fatal("bind #s") ;

	l4printf_b("bind -ac #R / \n");         // devrootfs
	rc = bind("#R", "/", MAFTER|MCREATE);   // bind the DEVROOTFS ?BEFORE
	if (rc < 0) fatal("bind #R") ;

	ack2hvm(0);
	sleep(500); 

	cd_start();
	sleep(200);
#else //%original-----------------------------
	// #ec gets us plan9.ini settings (*var variables).
	bind("#ec", "/env", MREPL);
	bind("#e", "/env", MBEFORE|MCREATE);
	bind("#s", "/srv", MREPL|MCREATE);
#endif //%-----------------------------------

	rp = "";

	{
	    int  rc;
	    l4printf_b("bind -a #I0 /net \n");
	    rc = bind("#I", "/net", MAFTER);
	    if (rc < 0) fatal("bind #I0") ;
                                                                   
	    l4printf_b("bind -a #l /net \n");
	    rc = bind("#l", "/net", MAFTER);
	    if (rc < 0) fatal("bind #l") ;
	}
	afd = -1;

	rsp = rp;
	rp = "/root";  //% ??

	if(bind(rp, "/", MAFTER|MCREATE) < 0){
		if(strncmp(rp, "/root", 5) == 0){
			fprint(2, "boot: couldn't bind $rootdir=%s to root: %r\n", rp);
			fatal("second bind /");
		}
		snprint(rootbuf, sizeof rootbuf, "/root/%s", rp);
		rp = rootbuf;
		if(bind(rp, "/", MAFTER|MCREATE) < 0){
			fprint(2, "boot: couldn't bind $rootdir=%s to root: %r\n", rp);

			if(strcmp(rootbuf, "/root//plan9") == 0){
				fprint(2, "**** warning: remove rootdir=/plan9 entry from plan9.ini\n");
				rp = "/root";
				if(bind(rp, "/", MAFTER|MCREATE) < 0)
					fatal("second bind /");
			}else
				fatal("second bind /");
		}
	}

	setenv("rootdir", rp);


	extern  void init_main(char*);
	init_main(nil);
}

