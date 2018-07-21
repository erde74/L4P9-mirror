//%% debug dump %% 

#include        "u.h"
#include        "../port/lib.h"
#include        "../pc/mem.h"
#include        "../pc/dat.h"
#include        "../pc/fns.h"
#include        "../port/error.h"

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

#define   _DBGFLG  1
#include  <l4/_dbgpr.h>

#define PR  print   // l4printf
#define PR2 print   //  l4printf_b

//---------------------------------------------------
#define  TMAX   256
#define  LENMAX  12
static struct {
      L4_Clock_t  clk;
      char        msg[LENMAX];
} elapse[TMAX];

static  int elapse_inx = 0;
static  int what = 0; 
extern  L4_Clock_t  L4_SystemClock();


void check_clock(int  which, char *msg)  // which : 'a', 'b',,,,
{
    int          i;
    char        *cp;        

    if (which != what) 
        return;
    if (elapse_inx >= TMAX) 
        return;

    elapse[elapse_inx].clk = L4_SystemClock();
    
    cp = elapse[elapse_inx].msg;
    for(i=0; i<(LENMAX-1)  && (*cp++ = *msg++) ; i++); 
    *cp = 0;
    elapse_inx++;
}


static void pr_elapse(char  which, int diff)
{
    int  i;
    uvlong delta;  // unsigned long long                                                   
    uint   stime, usec, msec, sec;
    static firsttime = 1;

    if (firsttime) {
        what = which;
	elapse_inx = 0;
	firsttime = 0;
	PR("elapsed_time('%c')  \n", which);
	return;
    }
    PR("delta_time('%c')  \t", which);
    for (i = 0; i < elapse_inx; i++){

        if(diff == 1){
	    if (i == 0) {
	        PR("{0:0:0} \t%s \t", elapse[i].msg);
		continue;
	    }

	    delta = elapse[i].clk.raw - elapse[i-1].clk.raw;
	    usec = (uint)(delta % 1000ULL);
	    msec = (uint)((delta / 1000ULL) % 1000ULL);
	    sec =  (uint)((delta / 1000000ULL) % 1000ULL);
	    PR("[%d.%d.%d] %s  \t", sec, msec, usec, elapse[i].msg);
	}
	else {
	    stime = (uint)elapse[i].clk.raw; // lower 32 bits; enough for our purpose  
	    usec = (uint)(stime % 1000);
	    msec = (uint)((stime / 1000) % 1000);
	    sec =  (uint)((stime / 1000000) % 1000);
	    PR("{%d.%d.%d} %s  \t", sec, msec, usec, elapse[i].msg);
	}
    }
    PR("\n");
    what = which;
    elapse_inx = 0;
}


//---------------------------------------------------
void _prncpy(char *to, char *from, int len)
{
    int  i;
    char  ch;
    for(i = 0; i<len; i++) {
      ch = from[i];
      if (ch >= ' ' && ch <= '~')
	  to[i] = ch;
      else
	  to[i] = '^';
    }
    to[len] = 0;
}

//----------------------------------------------------
void _backtrace_()
{
  typedef struct frame{
    struct frame *ebp;
    void   *ret;
  } frame;
  register void * _EBP_ asm("%ebp");

  frame  *ebp = _EBP_;
  l4printf(" Backtrace: "); 
  while (ebp) {
    l4printf("0x%x ", ebp->ret);
    ebp = ebp->ebp;
  }
  l4printf("\n"); 
}

//-------------------------------------------------
void dump_mem(char *title, unsigned char *start, unsigned size)
{
  int  i, j, k;
  unsigned char c;
  char  buf[128];
  char  elem[32];

  PR2(" dump_mem<%s %x %d>\n", title, start, size);
  //  L4_KDB_Enter("");   //!!
  if (size>2048) size = 1024;

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
    PR("%s\n", buf);
  }
}

void dump_proc_mem(char *title, int procnr, uint adrs, int len)
{
    L4_ThreadId_t  mypager = L4_Pager();
    L4_MsgTag_t    tag;
    l4printf_g("dump_proc_mem:%s(%d %x %d)\n", title, procnr, adrs, len);
    L4_LoadMR(1, procnr);
    L4_LoadMR(2, adrs);
    L4_LoadMR(3, len);
    L4_LoadMR(0, CHK_PGMAP << 16 | 3);
    tag = L4_Call(mypager);
}


char* vlong2str(vlong val, char *str)
{
    char  ss[32];
    int   i;
    uvlong  x;
    // printf("%lld \t", val);  // not supported
 
    if (str == nil)  return  nil;
    for (i=0; i<32; i++)  ss[i]=0;
    if (val > 0L) {
        x = val;
	str[0] = 0;
    }
    else{
        x = 0 - val;
	str[0] = '-';
	str[1] = 0;
    }
 
    for (i=0; i<32; i++) {
        ss[30-i] = (x % 10) + '0';
	x = x / 10;
	if (x < 1) break;
    }
 
    strcat(str,  &ss[30-i]);
    return  str;
}
 
// Non-reentrant. Debug-use-only
char* vlong2a(vlong val)
{
  static  char str[64];
  vlong2str(val, str);
  return  str;
}




/*  
 *  Qid.path is "unsigned long long", however 
 *  l4printf() does not support long long integer(64 bits).
 *  l4printf("%x ", qid.path) prints the least 32bits, 
 *  provided that it is the last argument.
 */

static void pr_names(char *names[], int num)
{
  int  i;
  char *sp;
  char  buf1[64];
  buf1[0] = 0;

  for (i = 0; i < num; i++) {
    sp = names[i];
    strcat(buf1, (sp)?"sp":"-");
    strcat(buf1, ",");
  }
  PR("{%s}", buf1);
}

struct ChanAlloc {
  Lock  _lock;
  int   fid;
  Chan  *free;
  Chan  *list;
};

extern struct ChanAlloc  chanalloc;

static void pr_chan_mnt(void *tp)
{
  Chan   *cp = tp;
  Mount  *m;
  Path   *path;
  int    i;
  char   buf1[64], buf2[64];
  char   *strp;
  
  buf1[0] = buf2[0] = 0;

  if ((cp->umh)) {
    strcat(buf1, "Umh{");
    for(m = cp->umh->mount; m; m = m->next) {
      strp = m->to->path->s;
      strcat(buf1, (strp)?strp:"-");
      strcat(buf1, ",");
    }
    strcat(buf1, "}");
  }

  if ((path = cp->path)) {
    strcat(buf2, "Path{");
    for(i = 0; i < path->mlen; i++) {
      strp = path->mtpt[i]->path->s;
      strcat(buf2, (strp)?strp:"-");
      strcat(buf2, ",");
    }
    strcat(buf2, "}");
  }
  PR("%s %s\n", buf1, buf2);
}

void pr_chan(void* tp)
{
  Chan *p = tp;
  if (p == nil) { PR("Chan = 0  XXX\n"); return; }
  if (p->fid == 0) { PR("Chan.fid = 0  XXX\n"); return; }
  PR("Chan:%d(%s) off=%d Tdq<%c %d %d> md=%x", 
          p->fid, (p->path) ? p->path->s : "<nil>", (ulong)p->offset, 
     devtab[p->type]->dc, p->dev, (ulong)p->qid.path, p->mode);
  PR("umh=%x umc=%x mux=%x mch=%x",  
     p->umh, p->umc, p->mux, p->mchan);
  pr_chan_mnt(p);
}

void DBG_CH(void* tp, char* note)
{
  Chan *p = tp;
  if (p == nil) { PR("Chan = 0  XXX\n"); return; }
  if (p->fid == 0) { PR("Chan.fid = 0  XXX\n"); return; }
  PR("Chan:%d(%s) off=%d Tdq<%c %d %d> md=%x", 
          p->fid, (p->path) ? p->path->s : "<nil>", (ulong)p->offset, 
     devtab[p->type]->dc, p->dev, (ulong)p->qid.path, p->mode);
  PR("umh=%x umc=%x mux=%x mch=%s <> %s",  
     p->umh, p->umc, p->mux, (p->mchan)?p->mchan->path->s:"-", note);
  l4printgetc("\n");
}

static void pr_chan1(int fid)
{
  Chan  *p;
  if (fid == 0) { PR("Chan.fid = 0  XXX\n"); return; }
  for (p = chanalloc.list; p; p = p->link) 
      if (p->fid == fid) break;
  if (p == nil) { PR("Chan<%d> = 0  XXX\n", fid); return; } 

  PR("Chan:%d(%s) off=%d Tdq<%c %d %d> md=%x", 
          p->fid, (p->path) ? p->path->s : "<nil>", (ulong)p->offset, 
          devtab[p->type]->dc, p->dev, (ulong)p->qid.path, p->mode);
  PR("umh=%x umc=%x mux=%x mch=%x\n",  
     p->umh, p->umc, p->mux, p->mchan);
  pr_chan_mnt(p);
}

static void pr2_chan(void* tp)
{
  Chan *p = tp;
  if (p == nil) { PR("Chan = 0  XXX\n"); return; }
  if (p->fid == 0) { PR("Chan.fid = 0  XXX\n"); return; }
  PR("<Chan:%d(%s) Tdq<%c %d %d>",
     p->fid, (p->path) ? p->path->s : "<nil>", devtab[p->type]->dc, p->dev, (ulong)p->qid.path);
  pr_chan_mnt(p);
}


static void pr_dev(void *tp)
{
  Dev *p = tp;
  if (p == nil) { PR("Dev = 0  XXX\n"); return; }
  PR("Dev dc=%x<%c> name=%s \n", p->dc, p->dc, p->name);
}

static void pr_mount(void *tp)
{
  Mount  *p = tp;
  if (p == nil) { PR("Mount = 0\n"); return; }
  PR("Mount mntid=%d next=%x head=%x to=%x spec=%s\n", 
	     p->mountid, p->next, p->head, p->head, p->to, p->spec);
}

static void pr2_mount(void *tp)
{
  Mount  *p = tp;
  if (p == nil) { PR("Mount = 0  XXX\n"); return; }
  for (p=tp; p; p=p->next) {
    PR("<Mount to=");
    pr2_chan(p->to);
    PR(">");
  }
}

static void pr_mhead(void *tp)
{
  Mhead *p = tp;
  if (p == nil) { PR("Mhead = 0  XXX\n"); return; }
  PR("Mhead from=%x mount=%x hash=%x \n", 
	     p->from, p->mount, p->hash);
}

static void pr2_mhead(void *tp)
{
  Mhead *p;
  if (tp == nil) { PR("Mhead = 0  XXX\n"); return; }
  for(p = tp; p; p = p->hash){
    PR("<Mhead from="); 
    pr2_chan(p->from);
    PR("mount=");
    pr2_mount(p->mount);
    PR(">\n");
  }    
}

static void pr_mnt(void *tp)
{
  Mnt *p = tp;
  if (p == nil) { PR("Mnt = 0  XXX\n"); return; }
  PR("Mnt chan=%x  \n",  p->c);
}


static void pr_pgrp(void *tp)
{ 
  Pgrp *p = tp;
  int  i;
  if (p == nil) { PR("Pgrp = 0  XXX\n"); return; }
  PR("Pgrp noattach=%x pgrpid=%x  mnthash ", 
	     p->noattach, p->pgrpid);
  for(i=0; i<32; i++) 
    if (p->mnthash[i]) l4printf("[%x]=%x ", i, p->mnthash[i]);
  l4printf("\n");
}

static void pr2_pgrp(void *tp)
{ 
  Pgrp *p = tp;
  int  i;
  if (p == nil) { PR("Pgrp = 0  XXX\n"); return; }
  PR("Pgrp{");
  for(i=0; i<32; i++) 
    if (p->mnthash[i]) pr2_mhead(p->mnthash[i]);
  PR(" }\n");
}


static void pr_fgrp(void *tp)
{
  Fgrp *p = tp;
  // Chan **c = p->fd ;  // Chan **fd;
  //  Chan *cp = *c;
  int   nfd = p->nfd;
  int   i; 
  if (p == nil) { PR("Fgrp = 0  XXX\n"); return; }
  PR("Fgrp nfd=%d :: ", p->nfd);
  for (i = 0; i < nfd; i++) {
    Chan *c = p->fd[i];
    PR("%x<%s> ", c,  (c)?(devtab[c->type]->name):"-" );
  }
  PR("  \n");
}


static void pr_proc(void *tp)
{
    Proc *p = tp;
    int i;
    if (p == nil) { PR("Proc = 0  XXX\n"); return; }
    PR2("[%d %x]  %s  %s  %x  ",
       TID2PROCNR(p->thread), p->thread, p->text, p->user, p->pid);
    PR("%s  %s(%d)  %s(%x)  fd{",
       p->genbuf, p->slash->path->s, p->slash->fid, p->dot->path->s, p->dot->fid);
    for(i=0; i<=p->fgrp->maxfd && p->fgrp->fd[i]; i++) 
         PR("%d:%s ", i, p->fgrp->fd[i]->path->s);
    PR("} env{");
    for(i=0; i<p->egrp->nent && p->egrp->ent[i]; i++) 
      PR("<%s:%s>", p->egrp->ent[i]->name, p->egrp->ent[i]->value);
    PR("}\n");
}

static void pr_up(void *tp)
{
    pr_proc(up);
}

extern Proc * procnr2pcb(int procnr);

static void pr_ps(int procnr)
{
    Proc *pcb;

    if (procnr) {
        pcb = procnr2pcb(procnr);
	if (pcb->text != nil && pcb->thread.raw != 0) pr_proc(pcb);
    }
    else
        for(procnr = 1; procnr<PROCNRMAX; procnr++){
	    pcb = procnr2pcb(procnr);
	    if (pcb->thread.raw != 0) pr_proc(pcb);
	} 
}



static void pr_ns(void *tp)
{
  Proc  *procp;

  if (tp == 0)
    pr2_pgrp(up->pgrp);
  else {
    procp = procnr2pcb((int)tp);
    pr2_pgrp(procp->pgrp);
  }
}


static void pr_fds(void *tp)
{
    Proc  *procp;
    int i;

    procp = procnr2pcb((int)tp);
    // check proctbl
    if (procp == nil) { PR("Proc = 0  XXX\n"); return; }

    PR("Proc[%d] prog=\"%s\" usr=%s pid=%x ",
       TID2PROCNR(procp->thread), procp->text, procp->user, procp->pid);
    PR(" root=\"%s\":%d pwd=\"%s\":%x  FD{",
       procp->slash->path->s, procp->slash->fid, procp->dot->path->s, procp->dot->fid);
    for(i=0; i<=procp->fgrp->maxfd && procp->fgrp->fd[i]; i++) 
      PR("%d:%s ", i, procp->fgrp->fd[i]->path->s);
    PR("}\n");
}


static void pr_dirtab(void *tp)
{
  Dirtab *p = tp;
  PR("Dirtab name=%s qid.path=%X  \n", 
	     p->name, (ulong)p->qid.path);
}

static void pr_walkqid(void *tp)
{
  Walkqid * p = tp;
  int  i;
  PR("Walkqid cloneC:%d(%s) nqid=%d qid{", 
     p->clone->fid, p->clone->path->s, p->nqid);
  for (i=0; i<p->nqid; i++) PR("%d:", (ulong)p->qid[i].path);
  PR("}\n");
}

//---------------------------------------------
static char linebuf[128];
static void inputline(char *buf, int maxlen)
{
  int cpos = 0, len = 0, ch, i;

  buf[0] = 0;
  strcpy(buf, "_");
  do {
    l4printf("\rInput path-name: %s ",  buf);  //-----
    ch = l4getc();
    switch (ch)
      {
      case 127 : if (cpos > 0)  // DEL
	  {
	    strcpy(&buf[cpos-1], &buf[cpos]);
	    len--; cpos--;
	  }
	break;
      case  27 : cpos = 0;   //ESC
	len = 0;
	strcpy(buf, "_");
	break;
      case  '\r' : strcpy(&buf[cpos], &buf[cpos+1]);  // '\r'
	l4printf("\n"); //-------
	break;
      default  : if (len < maxlen)
	  {
	    for (i = len+1; i >= cpos; i--)
	      buf[i+1] = buf[i];
	    len++;
	    buf[cpos++] = ch;
	  }
	break;
      }
  } while (ch != '\r')  ;    // '\r'
}


//----------------------------------
int sys_d(ulong  *arg, Clerkjob_t *myjob)
{
  char *tbl = (char*)arg[0];
  char *param = (char*)arg[1];
  void *tp  = (void*)atoi(param);
  //  PR("_d tbl=%s param=<%s, %x> \n", tbl, param, tp); 

  static void* _chan;
  static void* _dev;
  static void* _mount;
  static void* _mhead;
  static void* _mnt; 
  static void* _pgrp;
  static void* _fgrp;
  static void* _proc;
  static void* _dirtab;
  static void* _walkqid;


  if ( !strcmp(tbl, "chan") )
    {
      if (tp == nil) tp = _chan;
      pr_chan(tp);  
      _chan = tp;
    }
  else if ( !strcmp(tbl, "ch") )
    {
      pr_chan1((int)tp);  
    }
  else if ( !strcmp(tbl, "namec") )
    {
      Chan  *cp;
      do {
	inputline(linebuf, 80);
      }
      while (!linebuf[0]);

      cp = namec(linebuf, Aopen, 0, 0);

      pr_chan(cp);  
    }
  else if ( !strcmp(tbl, "dev") )
    {
      if (tp == nil) tp = _dev;
      pr_dev(tp);
      _dev = tp;   
    }
  else if ( !strcmp(tbl, "mount") ) 
    {
      if (tp == nil) tp = _mount;
      pr_mount(tp);  
      _mount = tp;
    }
  else if ( !strcmp(tbl, "mhead") ) 
    {
      if (tp == nil) tp = _mhead;
      pr_mhead(tp);  
      _mhead = tp;
    }
  else if ( !strcmp(tbl, "mnt") )   
    {
      if (tp == nil) tp = _mnt;
      pr_mnt(tp);
      _mnt = tp;   
    }
  else if ( !strcmp(tbl, "pgrp") )  
    {
      if (tp == nil) tp = _pgrp;
      pr_pgrp(tp);  
      _pgrp = tp;
    }
  else if ( !strcmp(tbl, "fgrp") )
    {
      if (tp == nil) tp = _fgrp;
      pr_fgrp(tp);  
      _fgrp = tp;
    }
  else  if ( !strcmp(tbl, "proc") ) 
    {
      if (tp == nil) tp = _proc;
      pr_proc(tp);  
      _proc = tp;
    }
  else if ( !strcmp(tbl, "up") ) 
    {
      pr_up(tp); 
    }
  else if ( !strcmp(tbl, "ps") ) 
    {
      pr_ps((int)tp); 
    }
  else if ( !strcmp(tbl, "ns") ) 
    {
      pr_ns(tp); 
    }
  else if ( !strcmp(tbl, "fds") ) 
    {
      pr_fds(tp); 
    }
  else if ( !strcmp(tbl, "nss") ) 
    {
      dumpmount(); 
    }
  else  if ( !strcmp(tbl, "dirtab") )
    {
      if (tp == nil) tp = _dirtab;
      pr_dirtab(tp);    
      _dirtab = tp;
    }
  else  if ( !strcmp(tbl, "walkqid") ) 
    {
      if (tp == nil) tp = _walkqid;
      pr_walkqid(tp);   
      _walkqid = tp;
    }
  else  if ( !strcmp(tbl, "tm") ) 
    {
      char  which = param[0];
      pr_elapse(which, 1);   
    }
  else  if ( !strcmp(tbl, "tmz") ) 
    {
      char  which = param[0];
      pr_elapse(which, 0);   
    }
  else  if ( !strcmp(tbl, "getpid") )
    {
      int pid;
      check_clock('g', ">getpid");
      pid = myjob->pcb->pid;
      check_clock('g', "<getpid");
      return  pid;
    }
  else  if ( !strcmp(tbl, "vga") )
    {
      extern void vga_test();
      vga_test();
    }
  else
    PR ("tbl name<%s> err\n", tbl);

  return  0;
}

