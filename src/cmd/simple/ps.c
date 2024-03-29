#include <u.h>
#include <libc.h>
#include <bio.h>

#define DBGPRN  if(1)print

void	ps(char*);
void	error(char*);
int	cmp(void*, void*);

Biobuf	bout;
int	pflag;
int	aflag;

void
main(int argc, char *argv[])
{
	int fd, i, tot, none = 1;
	Dir *dir, **mem;


	ARGBEGIN {
	case 'a':
		aflag++;
		break;
	case 'p':
		pflag++;
		break;
	} ARGEND;
	Binit(&bout, 1, OWRITE);
	if(chdir("/proc")==-1)
		error("/proc");
	fd=open(".", OREAD);
	if(fd<0)
		error("/proc");

	tot = dirreadall(fd, &dir);
	if(tot <= 0){
		fprint(2, "ps: empty directory /proc\n");
		exits("empty");
	}

	mem = malloc(tot*sizeof(Dir*));
	for(i=0; i<tot; i++)
		mem[i] = dir++;

	qsort(mem, tot, sizeof(Dir*), cmp);
	for(i=0; i<tot; i++){
		ps(mem[i]->name);
		none = 0;
	}

	if(none)
		error("no processes; bad #p");
	exits(0);
}

void
ps(char *s)
{
	ulong utime, stime, size;
	int argc, basepri, fd, i, n, pri;
	char args[256], *argv[16], buf[64], pbuf[8], status[4096];
//DBGPRN("%s(%s)\n", __FUNCTION__, s);

	sprint(buf, "%s/status", s);
	fd = open(buf, OREAD);
	if(fd<0)
		return;
	n = read(fd, status, sizeof status-1);
	close(fd);
	if(n <= 0)
		return;
	status[n] = '\0';

	argc = tokenize(status, argv, nelem(argv)-1);
	//DBGPRN("PS[%d: %s %s %s ]\n", argc, argv[0], argv[1], argv[2]);
	if(argc  < 6)  // 6 <- 12
		return;
	argv[argc] = 0;

	/*
	 * 0  text
	 * 1  user
	 * 2  state
	 * 3  cputime[6]
	 * 9  memory
	 * 10 basepri
	 * 11 pri
	 */
#if 1 //%-------------------------------------
	print("%3s %-16s %-10s %-10s 0x%s  %3s\n",  
	      s, argv[0], argv[1], argv[2], argv[3], argv[4]);
#else //%-------------------------------------
	utime = strtoul(argv[3], 0, 0)/1000;
	stime = strtoul(argv[4], 0, 0)/1000;
	size  = strtoul(argv[9], 0, 0);
	if(pflag){
		basepri = strtoul(argv[10], 0, 0);
		pri = strtoul(argv[11], 0, 0);
		sprint(pbuf, " %2d %2d", basepri, pri);
	} else
		pbuf[0] = 0;
	Bprint(&bout, "%-10s %8s %4lud:%.2lud %3lud:%.2lud%s %7ludK %-8.8s ",
			argv[1],
			s,
			utime/60, utime%60,
			stime/60, stime%60,
			pbuf,
			size,
			argv[2]);

	if(aflag == 0){
    Noargs:
		Bprint(&bout, "%s\n", argv[0]);
		return;
	}

	sprint(buf, "%s/args", s);
	fd = open(buf, OREAD);
	if(fd < 0)
		goto Badargs;
	n = read(fd, args, sizeof args-1);
	close(fd);
	if(n < 0)
		goto Badargs;
	if(n == 0)
		goto Noargs;
	args[n] = '\0';
	for(i=0; i<n; i++)
		if(args[i] == '\n')
			args[i] = ' ';
	Bprint(&bout, "%s\n", args);
	return;

    Badargs:
	Bprint(&bout, "%s ?\n", argv[0]);
#endif //%----------------------------------------
}

void
error(char *s)
{
	fprint(2, "ps: %s: ", s);
	perror("error");
	exits(s);
}

int
cmp(void *va, void *vb)
{
	Dir **a, **b;

	a = va;
	b = vb;
	return atoi((*a)->name) - atoi((*b)->name);
}
