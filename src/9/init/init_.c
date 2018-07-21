//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <authsrv.h>

#if 1 //%----------------------------------------
int passtokey(char *key, char *p)
{
  l4printf_r("passtokey(%x,%s): Not yet implemented\n", key, p);
  return  0;
}
#endif //%---------------------------------------

char*	readenv(char*);
static void	setenv(char*, char*);  //%
void	cpenv(char*, char*);
void	closefds(void);
void	fexec(void(*)(void));
void	rcexec(void);
void	qshexec(void);
void	cpustart(void);
void	pass(int);

char	*service = "terminal";
char	*cmd;
char	*cpu;
char	*systemname;
int	manual = 0;
int	iscpu;


//-----------------------
static uchar  getuchar()
{
    char cbuf[4]; 
    uchar  ch;
    read(0, cbuf, 1);  
    ch = cbuf[0];
    return  ch;
}


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

void main(int argc, char *argv[])
{
	char *user;
	int fd;
	char ctl[128];
	int  rc;

//%	closefds();

	service = "terminal";
	manual = 0;
	ARGBEGIN{
	case 'c':
		service = "cpu";
		break;
	case 'm':
		manual = 1;
		break;
	case 't':
		service = "terminal";
		break;
	}ARGEND
	cmd = *argv;

#if 0 //%
	snprint(ctl, sizeof(ctl), "#p/%d/ctl", getpid());
	fd = open(ctl, OWRITE);
	if(fd < 0)
		print("init: warning: can't open %s: %r\n", ctl);
	else
		if(write(fd, "pri 10", 6) != 6)
			print("init: warning: can't set priority: %r\n");
	close(fd);
#endif //%

#if 0 //%
	cpu = readenv("#e/cputype");
	setenv("#e/objtype", cpu);
	setenv("#e/service", service);
	cpenv("/adm/timezone/local", "#e/timezone");
	user = readenv("#c/user");
	systemname = readenv("#c/sysname");
#endif //%

        fmtinstall('r', errfmt);

        l4printf_b("bind  -b #c /dev\n"); //%
        bind("#c", "/dev", MBEFORE);

	open("/dev/cons", OREAD);
	open("/dev/cons", OWRITE);
	open("/dev/cons", OWRITE);

#if 1 
        sleep(500); 

        if(chdir("/") < 0) fatal("chdir");
 
        l4printf_b("bind  #d /fd\n");
        if (bind("#d", "/fd", 0) < 0)       // bind the DEVDUP
            fatal("bind #d") ;

        l4printf_b("bind -c #e /env \n");
        if (bind("#e", "/env", MBEFORE|MCREATE) < 0) // bind the DEVENV
            fatal("bind #c") ;

        l4printf_b("bind  #p /proc \n");
        if (bind("#p", "/proc", 0) < 0)             // bind the DEVPROC
            fatal("bind #p") ; 

        l4printf_b("bind -c #s /srv \n");
        if (bind("#s", "/srv", MREPL|MCREATE) < 0)  // bind the DEVSRV
            fatal("bind #s") ;

        l4printf_b("bind -ac #R / \n");         // devrootfs
        if (bind("#R", "/", MAFTER|MCREATE) < 0) // bind the DEVROOTFS ?BEFORE
            fatal("bind #R") ;

	// HK 20091031 begin
	l4printf_b("bind -a #m /dev \n");	// devmouse
	if (bind("#m", "/dev", MAFTER) < 0)
		fatal("bind #m");

	l4printf_b("bind -a #i /dev \n");	// devdraw
	if (bind("#i", "/dev", MAFTER) < 0)
		fatal("bind #i");
	// HK 20091031 end
#else
	user = "eve";    //%
	newns(user, 0);
#endif

	ack2hvm(0);
        sleep(1000); 

        cd_start();
        sleep(200);

        l4printf_b("bind -ac /t/bin /bin \n");  
        if (bind("/t/bin", "/bin", MAFTER) < 0)   
            fatal("bind /t/bin /bin") ;

#if 0 //%
	{
	    int pid;
	    char *argv[32];
	    argv[0] = "rc";
	    argv[1] = 0;
	    rc = spawn("/t/bin/rc", 0);
	    if (rc<0) fatal("spawn");
	    fatal("10");
	    pid = waitpid();
	    fatal("20");
	    sleep(100000);
	}
#else //%
//%	iscpu = strcmp(service, "cpu")==0;
//%	if(iscpu && manual == 0) fexec(cpustart);

	for(;;){
	    int  c; 
	    print("Which shell ?   rc: hit 'r';  qsh RETURN\n"); 
	    c = getuchar();
	    if ((c == 'r') || (c =='R')){
	        print("\ninit: starting /bin/rc\n");
	        fexec(rcexec);
	    }
	    else {
	        print("\ninit: starting /bin/qsh\n");
	        fexec(qshexec);
	    }

	    manual = 1;
	    cmd = 0;
	    sleep(1000000);
	}
#endif //%
}

void
pass(int fd)
{
	char key[DESKEYLEN];
	char typed[32];
	char crypted[DESKEYLEN];
	int i;

	for(;;){
		print("\n%s password:", systemname);
		for(i=0; i<sizeof typed; i++){
			if(read(0, typed+i, 1) != 1){
				print("init: can't read password; insecure\n");
				return;
			}
			if(typed[i] == '\n'){
				typed[i] = 0;
				break;
			}
		}
		if(i == sizeof typed)
			continue;
		if(passtokey(crypted, typed) == 0)
			continue;
		seek(fd, 0, 0);
		if(read(fd, key, DESKEYLEN) != DESKEYLEN){
			print("init: can't read key; insecure\n");
			return;
		}
		if(memcmp(crypted, key, sizeof key))
			continue;
		/* clean up memory */
		memset(crypted, 0, sizeof crypted);
		memset(key, 0, sizeof key);
		return;
	}
}

static int gotnote;

void
pinhead(void* _x, char *msg)
{
	gotnote = 1;
	fprint(2, "init got note '%s'\n", msg);
	noted(NCONT);
}


void
fexec(void (*execfn)(void))
{
	Waitmsg *w;
	int pid;

	switch(pid=fork()){
	case 0:
		rfork(RFNOTEG);
		(*execfn)();
		print("init: exec error: %r\n");
		exits("exec");

	case -1:
		print("init: fork error: %r\n");
		exits("fork");

	default:
	casedefault:
	        //% notify(pinhead);
		gotnote = 0;

		sleep(1000); //%
		w = wait();
		if(w == nil){
			if(gotnote)
				goto casedefault;
			print("init: wait error: %r\n");
			break;
		}
		if(w->pid != pid){
			free(w);
			goto casedefault;
		}
		if(strstr(w->msg, "exec error") != 0){
			print("init: exit string %s\n", w->msg);
			print("init: sleeping because exec failed\n");
			free(w);
			for(;;)
				sleep(1000);
		}
		if(w->msg[0])
			print("init: rc exit status: %s\n", w->msg);
		free(w);
		break;
	}
}


void rcexec(void)
{
#if 1  //%
	  execl("/t/bin/rc", "rc", nil);   //% /t
#else  //original
	if(cmd)
	  execl("/t/bin/rc", "rc", "-c", cmd, nil);  //%  /t
	else if(manual || iscpu)
	  execl("/t/bin/rc", "rc", nil);   //% /t
	else if(strcmp(service, "terminal") == 0)
	  execl("/t/bin/rc", "rc", "-c",    //% /t
		      ". /rc/bin/termrc; home=/usr/$user; cd; . lib/profile", nil);
	else
#endif //%
}

void qshexec(void)
{
	  execl("/t/bin/qsh", "qsh", nil);   //% /t
}


void cpustart(void)
{
	execl("/bin/rc", "rc", "-c", "/rc/bin/cpurc", nil);
}


char* readenv(char *name)
{
	int f, len;
	Dir *d;
	char *val;

	f = open(name, OREAD);
	if(f < 0){
		print("init: can't open %s: %r\n", name);
		return "*unknown*";	
	}
	d = dirfstat(f);
	if(d == nil){
		print("init: can't stat %s: %r\n", name);
		return "*unknown*";
	}
	len = d->length;
	free(d);
	if(len == 0)	/* device files can be zero length but have contents */
		len = 64;
	val = malloc(len+1);
	if(val == nil){
		print("init: can't malloc %s: %r\n", name);
		return "*unknown*";
	}
	len = read(f, val, len);
	close(f);
	if(len < 0){
		print("init: can't read %s: %r\n", name);
		return "*unknown*";
	}else
		val[len] = '\0';
	return val;
}


static void setenv(char *var, char *val)
{
	int fd;

	fd = create(var, OWRITE, 0644);
	if(fd < 0)
		print("init: can't open %s\n", var);
	else{
		fprint(fd, val);
		close(fd);
	}
}

void
cpenv(char *file, char *var)
{
	int i, fd;
	char buf[8192];

	fd = open(file, OREAD);
	if(fd < 0)
		print("init: can't open %s\n", file);
	else{
		i = read(fd, buf, sizeof(buf)-1);
		if(i <= 0)
			print("init: can't read %s: %r\n", file);
		else{
			close(fd);
			buf[i] = 0;
			setenv(var, buf);
		}
	}
}

/*
 *  clean up after /boot
 */
void
closefds(void)
{
	int i;

	for(i = 3; i < 30; i++)
		close(i);
}
