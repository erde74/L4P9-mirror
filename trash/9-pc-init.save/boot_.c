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

#ifdef DEBUG
	print("argc=%d\n", argc);
	for(fd = 0; fd < argc; fd++)
		print("%lux %s ", argv[fd], argv[fd]);
	print("\n");
#endif 

	ARGBEGIN{
	case 'k':
		kflag = 1;
		break;
	case 'm':
		mflag = 1;
		break;
	case 'f':
		fflag = 1;
		break;
	}ARGEND

#if 1 //%--------------------------------------------
	rp = "";
#else //% original-----------------------------------
	readfile("#e/cputype", cputype, sizeof(cputype));

	/**  pick a method and initialize it */
	if(method[0].name == nil)
		fatal("no boot methods");

	mp = rootserver(argc ? *argv : 0);
	(*mp->config)(mp);
	islocal = strcmp(mp->name, "local") == 0;
	ishybrid = strcmp(mp->name, "hybrid") == 0;

	/**  load keymap if its there */
	kbmap();

	/**  authentication agent */
	authentication(cpuflag);

	/**  connect to the root file system */
	fd = (*mp->connect)();
	if(fd < 0)
		fatal("can't connect to file server");
	if(getenv("srvold9p"))
		fd = old9p(fd);
	if(!islocal && !ishybrid){
		if(cfs)
			fd = (*cfs)(fd);
	}

	print("version...");
	buf[0] = '\0';
	n = fversion(fd, 0, buf, sizeof buf);
	if(n < 0)
		fatal("can't init 9P");
	srvcreate("boot", fd);

	/**  create the name space, mount the root fs */
	if(bind("/", "/", MREPL) < 0)
		fatal("bind /");
	rp = getenv("rootspec");
	if(rp == nil)
		rp = "";
#endif //%---------------------------------------------

#if 1 //%--------------------------------
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
#else //%original -----------------------	
	afd = fauth(fd, rp);
	if(afd >= 0){
		ai = auth_proxy(afd, auth_getkey, "proto=p9any role=client");
		if(ai == nil)
			print("authentication failed (%r), trying mount anyways\n");
	}

	if(mount(fd, afd, "/root", MREPL|MCREATE, rp) < 0)
		fatal("mount /");
	rsp = rp;
	rp = getenv("rootdir");
	if(rp == nil)
		rp = rootdir;
#endif //%------------------------------------------
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
//%	close(fd);
	setenv("rootdir", rp);

//%	settime(islocal, afd, rsp);
//%	if(afd > 0)
//%		close(afd);

//%	swapproc();


#if 1 //%---------------------------------------
	extern  void init_main(char*);
	init_main(nil);
#else //%original-------------------------------
	cmd = getenv("init");
	if(cmd == nil){
		sprint(cmdbuf, "/%s/init -%s%s", cputype,
			cpuflag ? "c" : "t", mflag ? "m" : "");
		cmd = cmdbuf;
	}
	iargc = tokenize(cmd, iargv, nelem(iargv)-1);
	cmd = iargv[0];

	/* make iargv[0] basename(iargv[0]) */
	if((iargv[0] = strrchr(iargv[0], '/')))
		iargv[0]++;
	else
		iargv[0] = cmd;

	iargv[iargc] = nil;

	exec(cmd, iargv);
	fatal(cmd);
#endif //%---------------------------------------
}

