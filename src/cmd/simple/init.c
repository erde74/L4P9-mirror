//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <authsrv.h>

#if 1 //%------------------
#include  <l4all.h>
#define   DBGPRN  if(1)print
static void  SET(void* _x, ...){}
static void  USED(void* _x, ...){}

int passtokey(char *key, char *p)
{
  l4printf_r("passtokey(%x,%s): Not yet implemented\n", key, p);
  return  0;
}
#endif //%---------------------

char*	readenv(char*);
void	setenv(char*, char*);
void	cpenv(char*, char*);
void	closefds(void);
void	fexec(void(*)(void));
void	rcexec(void);
void	cpustart(void);
void	pass(int);

char	*service;
char	*cmd;
char	*cpu;
char	*systemname;
int	manual;
int	iscpu;

void
main(int argc, char *argv[])
{
	char *user;
	int fd;
	char ctl[128];

//L4_KDB_Enter("init-0");
	//	closefds();

	service = "terminal";  //orig: service = "cpu";
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

#if 0 //%-test----------------------
	snprint(ctl, sizeof(ctl), "#p/%d/ctl", getpid());
	fd = open(ctl, OWRITE);
	if(fd < 0)
		print("init: warning: can't open %s: %r\n", ctl);
	else
		if(write(fd, "pri 10", 6) != 6)
			print("init: warning: can't set priority: %r\n");
	close(fd);
#endif //%-----------------------------------

#if 1 //--Test--------------------------------
	user = "eve";
#else //original---------------------------
	cpu = readenv("#e/cputype");
	setenv("#e/objtype", cpu);
	setenv("#e/service", service);
	cpenv("/adm/timezone/local", "#e/timezone");
	user = readenv("#c/user");
	systemname = readenv("#c/sysname");
#endif //%------------------------------

#if 0 //%-------------------------
	{
	    int   fd;
	   open("/dev/cons", OREAD);
	   open("/dev/cons", OWRITE);
           open("/dev/cons", OWRITE);

	   if ((fd = open("/dev/consctl", OWRITE)) < 0) exits("Open-err-fd");
	   write(fd, "rawon", 5);  // raw-mode
	}
#else //%-------------------------
//L4_KDB_Enter("init-10");
	newns(user, 0);
//L4_KDB_Enter("init-20");
#endif //------------------------
	iscpu = strcmp(service, "cpu")==0;

	if(iscpu && manual == 0)
		fexec(cpustart);

	for(;;){
//L4_KDB_Enter("init-30");
		print("\ninit: starting /bin/rc\n");
		fexec(rcexec);
		manual = 1;
		cmd = 0;
		sleep(1000*1000);  //% <- 1000
	}
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

DBGPRN(">%s()\n", __FUNCTION__);
	switch(pid=fork()){
	case 0:
	  //%		rfork(RFNOTEG);

		(*execfn)();
		print("init: exec error: %r\n");
		exits("exec");
	case -1:
		print("init: fork error: %r\n");
		exits("fork");
	default:
	casedefault:
	  //%		notify(pinhead);
		gotnote = 0;
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

void
rcexec(void)
{
DBGPRN(">%s()\n", __FUNCTION__);
	if(cmd)
	        execl("/t/bin/rc", "rc", "-c", cmd, nil);  //% /bin/rc
	else if(manual || iscpu)
		execl("/t/bin/rc", "rc", nil);  //% /bin/rc
#if 0 //%
	else if(strcmp(service, "terminal") == 0)
		execl("/bin/rc", "rc", "-c", 
		      ". /rc/bin/termrc; home=/usr/$user; cd; . lib/profile", nil);
#endif 
	else
	  execl("/t/bin/rc", "rc", nil);   //% /bin/rc
}

void
cpustart(void)
{
	execl("/bin/rc", "rc", "-c", "/rc/bin/cpurc", nil);
}

char*
readenv(char *name)
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

void
setenv(char *var, char *val)
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