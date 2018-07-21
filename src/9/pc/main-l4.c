//%%%%%%%%%%%% main-l4.c %%%%%%%%%%%%%%%%%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../pc/io.h"
#include	"ureg.h"
//%  #include	"init.h"
#include	"pool.h"
//%  #include	"reboot.h"

#include  <l4all.h>     //%
#include  <l4/l4io.h>   //%
#include  <lp49/lp49.h> //%

#include  "../errhandler-l4.h"

#define print  l4printf_r  //%

static int  _DBGFLG = 0;
#include   <l4/_dbgpr.h>

extern  L4_ThreadId_t p9_irq_init();
extern  Proc*   newproc_at(int);
extern  Proc*   procnr2pcb(int);
extern void coreproc_setup(int procnr, int parent_procnr, char* pname);

int  core_process_nr;

Mach *m;

/*
 * Where configuration info is left for the loaded programme.
 * This will turn into a structure as more is done by the boot loader
 * (e.g. why parse the .ini file twice?).
 * There are 3584 bytes available at CONFADDR.
 */

#if 1 //%----------------------------------------------------
extern  char __bootline[64];
extern  char __bootargs[2048];

#define BOOTLINE	((char*)__bootline)
#define BOOTLINELEN	64
#define BOOTARGS	((char*)__bootargs)
#define	BOOTARGSLEN	(1023)
#define	MAXCONF		64

#else //original---------------------------------------------
  #define BOOTLINE	((char*)CONFADDR)
  #define BOOTLINELEN	64
  #define BOOTARGS	((char*)(CONFADDR+BOOTLINELEN))
  #define	BOOTARGSLEN	(4096-0x200-BOOTLINELEN)
  #define	MAXCONF		64

  char bootdisk[KNAMELEN];
#endif //%---------------------------------------------------



#if 1 //% Tentative value setting  --------------------------
Conf conf = 
  {  .ialloc =    4*1024*1024,  //%
     .pipeqsize = 32*1024,      //%
     .monitor =   1,            //%
     .nproc = 63,              //%  
  } ;

#else //original
  Conf conf;
#endif //-----------------------------------------------------

char *confname[MAXCONF];
char *confval[MAXCONF];
int nconf;
//%  uchar *sp;	/* user stack of init proc */
//%  int delaylink;

static void options(void)
{
	long i, n;
	char *cp, *line[MAXCONF], *p, *q;

	//  parse configuration args from dos file plan9.ini
	cp = BOOTARGS;	/* where b.com leaves its config */
	cp[BOOTARGSLEN-1] = 0;

	// Strip out '\r', change '\t' -> ' '.
	p = cp;
	for(q = cp; *q; q++){
		if(*q == '\r')
			continue;
		if(*q == '\t')
			*q = ' ';
		*p++ = *q;
	}
	*p = 0;

	n = getfields(cp, line, MAXCONF, 1, "\n");
	for(i = 0; i < n; i++){
		if(*line[i] == '#')
			continue;
		cp = strchr(line[i], '=');
		if(cp == nil)
			continue;
		*cp++ = '\0';
		confname[nconf] = line[i];
		confval[nconf] = cp;
		nconf++;
	}
}

//% extern void mmuinit0(void);
//% extern void (*i8237alloc)(void);

#if 1 //%----------------------------------------------
void dbg_pagemap(char *title)
{
    L4_ThreadId_t  mypager = L4_Pager();
    L4_MsgTag_t    tag;
    int procnr = 1;
    l4printf_r("%s ", title);
    L4_LoadMR(1, procnr);
    L4_LoadMR(2, 0);
    L4_LoadMR(3, 0);
    L4_LoadMR(0, CHK_PGMAP << 16 | 3);
    tag = L4_Call(mypager);
}
#endif //%--------------------------------------------

#if 1 //--------------------------------
static int nops_microsec = 100;

void loop_nops(int n)
{
  __asm__ __volatile__ (
        "0:              \n"
        "   nop         \n"
        "   loop      0b \n"
	:
	: "c" (n)
			);
}


uint  calc_speed()
{
    unsigned long long  tt1, tt2, tt3;
    int cycle_usec;
    int  nn = 10000;

    // Warming up 
    L4_SystemClock();  L4_SystemClock();
    L4_SystemClock();  L4_SystemClock();

    tt1 = L4_SystemClock().raw;
    loop_nops(nn);
    tt2 = L4_SystemClock().raw;
    tt3 = tt2 - tt1; // must not be 0
    cycle_usec = nn/tt3;
    DBGPRN("cycle_usec = %d  %d\n", cycle_usec, tt3);
    return cycle_usec;
}


void microdelay(int micro_sec) //9/pc/i8253.c:291:
{
    unsigned long long  tt1, tt2, tt3;
    int nn;
    nn = micro_sec * nops_microsec;
    tt1 = L4_SystemClock().raw;
    loop_nops(nn);
    tt2 = L4_SystemClock().raw;
    tt3 = tt2 - tt1;
    DBGPRN("<%d usec> %d delta=%d usec\n", micro_sec, nn, tt3);
}


void delay(int millisecs)
{
    L4_Word64_t microsecs ;
    if (millisecs == 0)
        microsecs = 100;
    else
        microsecs = millisecs * 500;  // 1000                                             
    L4_Sleep(L4_TimePeriod(microsecs));
}


long seconds(void)  // tod.c                                                              
{
    // DBGPRN("! %s \n", __FUNCTION__);                                                   
    L4_Clock_t  clock;
    uvlong  x;
    uint    sec;

    clock = L4_SystemClock();
    x = clock.raw / 1000000UL; // micro-sec --> sec.                                      
    sec = x;
    return  sec;
}

#endif //%--------------------------------------------



void main(void)
{
	l4printf_g("\n>>>>>>>>>>>>>>  LP49  <<<<<<<<<<<<<<<\n");

	core_process_nr = TID2PROCNR(L4_Myself());

        //%	mach0init();
        options();
	nops_microsec = calc_speed(); //%
        ioinit();
	i8250console();   // pc/uarti8250.c	// HK 20091031
	quotefmtinstall();
	screeninit();     // pc/cga.c // HK 20091031
	p9_irq_init();	  //% libl4/l4-p9-irq.c. create IRQ-thread
	//%	trapinit0();
	//%	mmuinit0();
	kbdinit();
	//%	i8253init();   // 8253 timer
	//%	cpuidentify();
	//%	meminit();
	//%	confinit();
	//%	archinit();
	xinit();
	//%	if(i8237alloc != nil)
	//%	   i8237alloc();  // dma.c.  allocate DMA area 
	//%	trapinit();
	printinit();   //% in devcons.c
	//%	cpuidprint();
	//%	mmuinit();
	//%	if(arch->intrinit) /* launches other processors on an mp */
	//%		arch->intrinit();
	timersinit();  // port/pportclock.c, create timer_thread.
	//%	mathinit();
	kbdenable();   // pc/kbd.c
	//%	if(arch->clockenable)
	//%	        arch->clockenable();
	procinit0();  //% proc-l4.c: 367
	//%	initseg();
	//%	if(delaylink){
	//%		bootlinks();
	//%		pcimatch(0, 0, 0);
	//%	}else

	links();     //% config-l4.c: 109

	chandevreset();   //% port/chan.c: 242
	chandevinit();   //% port/chan.c: 242

	//%	pageinit();
	//%	i8253link();
	//%	swapinit();
	//%	userinit();
	//%	active.thunderbirdsarego = 1;
	//%	schedinit();

	L4_Set_UserDefinedHandle((L4_Word_t)&cjob);

	coreproc_setup(1, 0, "*pc*");     //%  pc [1000002]  proc-l4c: 93
	coreproc_setup(2, 0, "*qsh*");    //%  qsh [2000002]
	coreproc_setup(3, 2, "*dossrv*"); //%  dossrv  Inherit FDs etc. from qsh=2  
	coreproc_setup(4, 2, "*9660srv*");  //%  9660srv

extern void service_loop();
	service_loop(); // syssrv-l4.c
}


#if 1 //%-------------------------------------------------------
// "coreproc"; a process loaded by GRUB and started by hvm/startup().
void coreproc_setup(int procnr, int parent_procnr, char *pname)  
{
  Proc *p;
  Proc *parent;

  p = newproc_at(procnr);

  if (parent_procnr == 0) {
    p->pgrp = newpgrp();

    p->egrp = smalloc(sizeof(Egrp));
    p->egrp->_ref.ref = 1;  //%     p->egrp->ref = 1;
    p->fgrp = dupfgrp(nil);
    p->rgrp = newrgrp();

  }
  else {  // fork effect
    parent = procnr2pcb(parent_procnr);
    p->pgrp = parent->pgrp;

    p->egrp = parent->egrp;
    p->fgrp = parent->fgrp;
    p->rgrp = parent->rgrp;
  }

  p->procmode = 0640;

  p->user = eve;
  kstrdup(&p->text, pname); 

  p->slash = namec("#/", Atodir, 0, 0);
  pathclose(up->slash->path);
  p->slash->path = newpath("/");
  p->dot = cclone(up->slash);

  up = p;
  DBGPRN("< %s(%d) \n", __FUNCTION__, procnr);
}


#endif //%----------------------------------------------------


char* getconf(char *name)
{
	int i;
	//%	if (strcmp(name, "sdC0part") == 0)              //% DBG
	//%        return "dos 63 1000/plan9 1001 2000";
	for(i = 0; i < nconf; i++)
	        if(cistrcmp(confname[i], name) == 0) {
		        DBGBRK("getconf(%s): %s   \n", name, confval[i]); //%
			return confval[i];
		}
	return 0;
}


void exit(int ispanic)
{
  DBGBRK("! %s() \n", __FUNCTION__);
#if 0
	shutdown(ispanic);
	arch->reset();
#endif
}


int isaconfig(char *class, int ctlrno, ISAConf *isa)
{
	char cc[32], *p;
	int i;

	snprint(cc, sizeof cc, "%s%d", class, ctlrno);
	p = getconf(cc);
	if(p == nil)
		return 0;

	isa->type = "";
	isa->nopt = tokenize(p, isa->opt, NISAOPT);
	for(i = 0; i < isa->nopt; i++){
		p = isa->opt[i];
		if(cistrncmp(p, "type=", 5) == 0)
			isa->type = p + 5;
		else if(cistrncmp(p, "port=", 5) == 0)
			isa->port = strtoul(p+5, &p, 0);
		else if(cistrncmp(p, "irq=", 4) == 0)
			isa->irq = strtoul(p+4, &p, 0);
		else if(cistrncmp(p, "dma=", 4) == 0)
			isa->dma = strtoul(p+4, &p, 0);
		else if(cistrncmp(p, "mem=", 4) == 0)
			isa->mem = strtoul(p+4, &p, 0);
		else if(cistrncmp(p, "size=", 5) == 0)
			isa->size = strtoul(p+5, &p, 0);
		else if(cistrncmp(p, "freq=", 5) == 0)
			isa->freq = strtoul(p+5, &p, 0);
	}
	return 1;
}


int cistrcmp(char *a, char *b)
{
	int ac, bc;

	for(;;){
		ac = *a++;
		bc = *b++;
	
		if(ac >= 'A' && ac <= 'Z')
			ac = 'a' + (ac - 'A');
		if(bc >= 'A' && bc <= 'Z')
			bc = 'a' + (bc - 'A');
		ac -= bc;
		if(ac)
			return ac;
		if(bc == 0)
			break;
	}
	return 0;
}


int cistrncmp(char *a, char *b, int n)
{
	unsigned ac, bc;

	while(n > 0){
		ac = *a++;
		bc = *b++;
		n--;

		if(ac >= 'A' && ac <= 'Z')
			ac = 'a' + (ac - 'A');
		if(bc >= 'A' && bc <= 'Z')
			bc = 'a' + (bc - 'A');

		ac -= bc;
		if(ac)
			return ac;
		if(bc == 0)
			break;
	}

	return 0;
}



#if 0 //%============================================================
//%  Plan9 functions .

void mach0init(void);
void machinit(void);
static void mathnote(void);
static void matherror(Ureg *ur, void* _x);  
static void mathemu(Ureg *ureg, void* _x);  
static void mathover(Ureg* _x, void* _y);
void mathinit(void);
void procsetup(Proc*p);
void procrestore(Proc *p);
void procsave(Proc *p);


void init0(void)
{
	int i;
	char buf[2*KNAMELEN];

	up->nerrlab = 0;
	spllo();

	// These are o.k. because rootinit is null.
	// Then early kproc's will have a root and dot.
	up->slash = namec("#/", Atodir, 0, 0);
	pathclose(up->slash->path);
	up->slash->path = newpath("/");
	up->dot = cclone(up->slash);

	chandevinit();
	if(!WASERROR()){   //%
	        //% snprint(buf, sizeof(buf), "%s %s", arch->id, conffile);
		snprint(buf, sizeof(buf), "%s %s", "pc", conffile);
		ksetenv("terminal", buf, 0);
		ksetenv("cputype", "386", 0);
		if(cpuserver)
			ksetenv("service", "cpu", 0);
		else
			ksetenv("service", "terminal", 0);
		for(i = 0; i < nconf; i++){
			if(confname[i][0] != '*')
				ksetenv(confname[i], confval[i], 0);
			ksetenv(confname[i], confval[i], 1);
		}
		POPERROR();   //%
	}
	//%	kproc("alarm", alarmkproc, 0);
	//%	touser(sp);
}

void userinit(void)
{
	void *v;
	Proc *p;
	Segment *s;
	Page *pg;

	p = newproc();
	p->pgrp = newpgrp();
	p->egrp = smalloc(sizeof(Egrp));
	p->egrp->_ref.ref = 1;  //%	p->egrp->ref = 1;
	p->fgrp = dupfgrp(nil);
	p->rgrp = newrgrp();
	p->procmode = 0640;

	kstrdup(&eve, "");
	kstrdup(&p->text, "*init*");
	kstrdup(&p->user, eve);

	p->fpstate = FPinit;
	fpoff();
	// Kernel Stack
	// N.B. make sure there's enough space for syscall to check
	//	for valid args and 4 bytes for gotolabel's return PC
	p->sched.pc = (ulong)init0;
	p->sched.sp = (ulong)p->kstack+KSTACK-(sizeof(Sargs)+BY2WD);
	// User Stack
	//* N.B. cannot call newpage() with clear=1, because pc kmap
	// requires up != nil.  use tmpmap instead.
	s = newseg(SG_STACK, USTKTOP-USTKSIZE, USTKSIZE/BY2PG);
	p->seg[SSEG] = s;
	pg = newpage(0, 0, USTKTOP-BY2PG);
	v = tmpmap(pg);
	memset(v, 0, BY2PG);
	segpage(s, pg);
	bootargs(v);
	tmpunmap(v);
	// Text
	s = newseg(SG_TEXT, UTZERO, 1);
	s->flushme++;
	p->seg[TSEG] = s;
	pg = newpage(0, 0, UTZERO);
	memset(pg->cachectl, PG_TXTFLUSH, sizeof(pg->cachectl));
	segpage(s, pg);
	v = tmpmap(pg);
	memset(v, 0, BY2PG);
	//% memmove(v, initcode, sizeof initcode);  //%%%% ??
	tmpunmap(v);
	ready(p);
}


static int writeconf(void)  //% void<-int ONERR
{
	char *p, *q;
	int n;

	p = getconfenv();

	if(WASERROR()) {   //%
	_ERR_1:
		free(p);
		NEXTERROR_RETURN(ONERR);   //% nexterror();
	}

	/* convert to name=value\n format */
	for(q=p; *q; q++) {
		q += strlen(q);
		*q = '=';
		q += strlen(q);
		*q = '\n';
	}
	n = q - p + 1;
	if(n >= BOOTARGSLEN)
	        ERROR_GOTO("kernel configuration too large", _ERR_1);
	memset(BOOTLINE, 0, BOOTLINELEN);
	memmove(BOOTARGS, p, n);
	POPERROR();    //%
	free(p);
	return  1;     //%
}


void confinit(void)
{
	DBGPRN("! %s \n", __FUNCTION__);
	char *p;
	int i, userpcnt;
	ulong kpages;

	if((p = getconf("*kernelpercent")))  //%
		userpcnt = 100 - strtol(p, 0, 0);
	else
		userpcnt = 0;

	conf.npage = 0;
	for(i=0; i<nelem(conf.mem); i++)
		conf.npage += conf.mem[i].npage;

	conf.nproc = 100 + ((conf.npage*BY2PG)/MB)*5;
	if(cpuserver)
		conf.nproc *= 3;
	if(conf.nproc > 2000)
		conf.nproc = 2000;
	conf.nimage = 200;
	conf.nswap = conf.nproc*80;
	conf.nswppo = 4096;

	if(cpuserver) {
		if(userpcnt < 10)
			userpcnt = 70;
		kpages = conf.npage - (conf.npage*userpcnt)/100;

		/*
		 * Hack for the big boys. Only good while physmem < 4GB.
		 * Give the kernel fixed max + enough to allocate the
		 * page pool.
		 * This is an overestimate as conf.upages < conf.npages.
		 * The patch of nimage is a band-aid, scanning the whole
		 * page list in imagereclaim just takes too long.
		 */
		if(kpages > (64*MB + conf.npage*sizeof(Page))/BY2PG){
			kpages = (64*MB + conf.npage*sizeof(Page))/BY2PG;
			conf.nimage = 2000;
			kpages += (conf.nproc*KSTACK)/BY2PG;
		}
	} else {
		if(userpcnt < 10) {
			if(conf.npage*BY2PG < 16*MB)
				userpcnt = 40;
			else
				userpcnt = 60;
		}
		kpages = conf.npage - (conf.npage*userpcnt)/100;

		/*
		 * Make sure terminals with low memory get at least
		 * 4MB on the first Image chunk allocation.
		 */
		if(conf.npage*BY2PG < 16*MB)
		  imagmem->minarena = 4*1024*1024;  //% alloc.c
	}

	/*
	 * can't go past the end of virtual memory
	 * (ulong)-KZERO is 2^32 - KZERO
	 */
	if(kpages > ((ulong)-KZERO)/BY2PG)
		kpages = ((ulong)-KZERO)/BY2PG;

	conf.upages = conf.npage - kpages;
	conf.ialloc = (kpages/2)*BY2PG;

	/*
	 * Guess how much is taken by the large permanent
	 * datastructures. Mntcache and Mntrpc are not accounted for
	 * (probably ~300KB).
	 */
	kpages *= BY2PG;
	kpages -= conf.upages*sizeof(Page)
		+ conf.nproc*sizeof(Proc)
		+ conf.nimage*sizeof(Image)
		+ conf.nswap
		+ conf.nswppo*sizeof(Page);
	mainmem->maxsize = kpages;   //% alloc.c 
	if(!cpuserver){
		/*
		 * give terminals lots of image memory, too; the dynamic
		 * allocation will balance the load properly, hopefully.
		 * be careful with 32-bit overflow.
		 */
	         imagmem->maxsize = kpages;   //% 
	}
}


uchar * pusharg(char *p)
{
	int n;

	n = strlen(p)+1;
	sp -= n;
	memmove(sp, p, n);
	return sp;
}

void bootargs(void *base)
{
 	int i, ac;
	uchar *av[32];
	uchar **lsp;
	char *cp = BOOTLINE;
	char buf[64];

	sp = (uchar*)base + BY2PG - MAXSYSARG*BY2WD;

	ac = 0;
	av[ac++] = pusharg("/386/9dos");

	/* when boot is changed to only use rc, this code can go away */
	cp[BOOTLINELEN-1] = 0;
	buf[0] = 0;
	if(strncmp(cp, "fd", 2) == 0){
		sprint(buf, "local!#f/fd%lddisk", strtol(cp+2, 0, 0));
		av[ac++] = pusharg(buf);
	} else if(strncmp(cp, "sd", 2) == 0){
		sprint(buf, "local!#S/sd%c%c/fs", *(cp+2), *(cp+3));
		av[ac++] = pusharg(buf);
	} else if(strncmp(cp, "ether", 5) == 0)
		av[ac++] = pusharg("-n");

	/* 4 byte word align stack */
	sp = (uchar*)((ulong)sp & ~3);

	/* build argc, argv on stack */
	sp -= (ac+1)*sizeof(sp);
	lsp = (uchar**)sp;
	for(i = 0; i < ac; i++)
		*lsp++ = av[i] + ((USTKTOP - BY2PG) - (ulong)base);
	*lsp = 0;
	sp += (USTKTOP - BY2PG) - (ulong)base - sizeof(ulong);
}


static void shutdown(int ispanic);
{
	int ms, once;

	lock(&active._lock);  //% ?? 	lock(&active); 
	if(ispanic)
		active.ispanic = ispanic;
	else if(m->machno == 0 && (active.machs & (1<<m->machno)) == 0)
		active.ispanic = 0;
	once = active.machs & (1<<m->machno);
	active.machs &= ~(1<<m->machno);
	active.exiting = 1;
	unlock(&active._lock);  //% ??   (&active)

	if(once)
		print("cpu%d: exiting\n", m->machno);
	spllo();
	for(ms = 5*1000; ms > 0; ms -= TK2MS(2)){
		delay(TK2MS(2));
		if(active.machs == 0 && consactive() == 0)
			break;
	}

	if(getconf("*debug"))
		delay(5*60*1000);

	if(active.ispanic){
		if(!cpuserver)
			for(;;)
				halt();
		delay(10000);
	}else
		delay(1000);
}

void
reboot(void *entry, void *code, ulong size)
{
	void (*f)(ulong, ulong, ulong);
	ulong *pdb;

	writeconf();
	shutdown(0);

	// should be the only processor running now

	print("shutting down...\n");
	delay(200);

	splhi();

	/* turn off buffered serial console */
	serialoq = nil;

	/* shutdown devices */
	chandevshutdown();

	/*
	 * Modify the machine page table to directly map the low 4MB of memory
	 * This allows the reboot code to turn off the page mapping
	 */
	pdb = m->pdb;
	pdb[PDX(0)] = pdb[PDX(KZERO)];
	mmuflushtlb(PADDR(pdb));

	/* setup reboot trampoline function */
	f = (void*)REBOOTADDR;
	//% memmove(f, rebootcode, sizeof(rebootcode));  //%%%%%%%

	print("rebooting...\n");

	/* off we go - never to return */
	(*f)(PADDR(entry), PADDR(code), size);
}


/*
 *  put the processor in the halt state if we've no processes to run.
 *  an interrupt will get us going again.
 */
void
idlehands(void)
{
	if(conf.nmach == 1)
		halt();
}
#endif //%============================================================
