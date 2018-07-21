/********************************************************
 *        LP49 QUASI SHELL for debugging                *
 *        A stupid shell for debugging purpose.         *
 *                                                      *  
 *        2008 (C)  KM, YS @NII                         *
 ********************************************************/

/* Debugging tool: sorry for very very dirty program. */

/*##### Grub-boot menu.lst #####
 * title = LP49 + PC + QSH + DOSSRV
 * kernel=/l4/kickstart-0507.gz
 * module=/l4/l4ka-0507.gz
 * module=/l4/sigma0-0507.gz
 * module=/boot/hvm.gz
 * module=/boot/pc.gz
 * module=/boot/qsh.gz
 * module=/boot/dossrv.gz
 * module=/bin/9660srv
 */

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>
#include  <u.h>
#include  <libc.h>

#define  INITPROC 1 
#define  _DBGFLG  0
#define  DBGPRN  if(_DBGFLG)l4printf_b
#define  DBGBRK  if(_DBGFLG)l4printgetc

#define  NULL  ((void*)0)
#define  LL2L(x)  (unsigned)(x)
#define  PR      print    // l4printf
#define l4printf print
#define l4printf_b print
#define l4printf_r print

#define  EPR(x)  if(x<0){ perror("qsh"); continue;}\
                 else l4printf("OK: %d \n", x);
#define  XPR(x)  if(x<0){ perror("qsh"); return;}\
                 else l4printf("OK: %d \n", x);

extern void  tar_main(int argc, char *argv[]);
extern long  _put(int arg1, void* arg2, long arg3);
extern long  _get(int arg1, void* arg2, long arg3);
extern long  _read(int arg1, void* arg2, long arg3);
extern long  _write(int arg1, void* arg2, long arg3);
extern long  _d(char *arg1, void *arg2);
extern int   spawn(char* arg1, char* arg2[]);
extern void  wait_preproc(int t);
extern void  post_nextproc(int t);
extern int   activate_server(L4_ThreadId_t tid, int ms);
extern int   proc2proc_copy(unsigned fromproc, unsigned fromadrs,
			    unsigned toproc, unsigned toadrs, int  size);
extern void  dbg_dump_mem(char *title, char *start, int size);
extern void  dbg_time(int which, char *msg) ;

extern int   give(int fd,  char* buf,  int  length);
extern int   take(int fd,  char* buf,  int  length);

extern int   qsh_cp(char *);
extern int   qsh_mkdir(char *);
extern void  req2printsrv(char *s, int len, int color);

//--------------------------------------
void  mkfullpath(char *in_path,  char * out_path);
int   ipsetup(char *IPadrs, char *GWadrs);
int   usbsetup( );
int   mbrpartition(int  fd);
char* vlong2str(vlong val, char *str);
char* vlong2a(vlong val);

static int   fd_start();
static void  nstart();
static void  credits(void);
static void  pi(void);
static int   copy(char *fromfile, char * tofile);
static void  help( );

//--------------------------------
int    debug = 0;
static char   pwdname[128];
int  fd0, fd1, fd2;

static char *autoinput = nil;

#if 1 // copy of libc/9sys/getwd.c -----------------------
char* getwd(char *buf, int nbuf)
{
    int n, fd;
    fd = open(".", OREAD);
    if(fd < 0) { perror("getwd");
        return nil;
    }
    L4_Sleep(L4_TimePeriod (100000UL /*microsec*/)); //100ms
    n = fd2path(fd, buf, nbuf);
    close(fd);
    if(n < 0) {  perror("fd2path");
        return nil;
    }
    return buf;
}
#endif //------------------------------------------------

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
	PR("Delta_time('%c')  \n", which);
	return;
    }
    PR("Delta_time('%c')  \t", which);
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
static uchar  getuchar()
{
	char cbuf[4]; 
	uchar  ch;

	/* HK 20100228 begin */
	if (autoinput == nil || *autoinput == '\0') {
		read(0, cbuf, 1);
		ch = cbuf[0];
	} else
		ch = *autoinput++;
	/* HK 20100228 end */
	return  ch;
}

void inputline(char *buf, int maxlen)
{
    int cpos = 0, len = 0,  i;
    uchar  ch;  
    unsigned  scrolldown = 0x0000000F; // Bad idea, but tentative. sorry.
    unsigned  scrollup   = 0x0000000E;

    buf[0] = 0;
    strcpy(buf, "_"); 
    PR("\rLP49[%s]: ", pwdname);

    do {
    Scrolled: ;

	ch = getuchar();
	switch (ch)  {
	case 0xEF:  // scroll in RAW mode
	              {
			  uchar ch2, ch3;
			  ch2 = getuchar();
			  ch3 = getuchar();
			  if (ch2 == 0x80 && ch3 == 0x8E) {
			      req2printsrv(&scrolldown, 1, 0);
			  }
			  else  if (ch2 == 0xA0 && ch3 == 0x80) {
			      req2printsrv(&scrollup, 1, 0);
			  }
			  goto Scrolled;
		      }

	case 127 : if (cpos > 0)  // DEL in RAW mode
	               {
			 strcpy(&buf[cpos-1], &buf[cpos]);
			 len--; cpos--;
		       }  
	              break;
	case  27 : cpos = 0;   //ESC in RAW mode
	              len = 0;
		      strcpy(buf, "_");
                      break;

	case  '\r' : 
	case  '\n' :  strcpy(&buf[cpos], &buf[cpos+1]);  // '\n' '\r'
	             // PR("\n"); //-------
                      break;

	default  : if (len < maxlen) {
                          for (i = len+1; i >= cpos; i--)
                            buf[i+1] = buf[i];
                          len++;
			  buf[cpos++] = ch;
		   } 
	           break;
	}
    } while (ch != '\r' && ch != '\n')  ;    // '\r'
    //   PR("`%s` \n",  buf);
}

//---------------------------------------------
static void pr_dir(Dir *db, int lc)
{
    ulong mode = db->mode;
    uint  rwx;
    char  daxm[8] = {0}; 

    if (lc){  // 
        char  c1 ;
	if (mode & DMDIR) 
	    c1 = '/';
	else  if (mode & DMEXCL)
	    c1 = '*';
	else 
	    c1 = ' ';

	PR("%s%c\t", db->name, c1);
	return ;
    }

    strcat(daxm, ((mode & DMDIR)?   "d":""));
    strcat(daxm, ((mode & DMAPPEND)?"a":""));
    strcat(daxm, ((mode & DMEXCL)?  "x":""));
    strcat(daxm, ((mode & DMMOUNT)? "m":""));
    if (daxm[0] == 0) strcat(daxm, "-");
    PR("%s ", daxm);

    rwx = mode >> 6;
    PR("%c%c%c", (rwx & DMREAD)?'r':'-',  (rwx & DMWRITE)?'w':'-', 
       (rwx & DMEXEC)?'x':'-');
    rwx = mode >> 3;
    PR("%c%c%c", (rwx & DMREAD)?'r':'-',  (rwx & DMWRITE)?'w':'-', 
       (rwx & DMEXEC)?'x':'-');
    rwx = mode >> 0;
    PR("%c%c%c", (rwx & DMREAD)?'r':'-',  (rwx & DMWRITE)?'w':'-', 
       (rwx & DMEXEC)?'x':'-');
    PR("\t%4d\t%s%c", LL2L(db->length), db->name, (db->qid.type & QTDIR)?'/':' ');
    PR("\ttdq<%c %d %d> ", db->type, db->dev, LL2L(db->qid.path));
    PR("\t<%s %s> \n", db->uid, db->gid);
}

int listdir(char * dirname, int lc)
{
    Dir  *db;
    int  fd, n, i;

    if (dirname == 0) dirname = ".";
    DBGBRK("> %s(%s) \n", __FUNCTION__, dirname);
    db = dirstat(dirname);
    if (db == nil) {
        PR("ls: err-1\n");
	return -1;
    }
  
    pr_dir(db, lc);
    if (db->qid.type & QTDIR) {
        free(db);
	fd = open(dirname, OREAD);
	if (fd == -1){
	    perror("ls");
	    return  -1;
	}
	n = dirreadall(fd, &db);
	if (n < 0) {
	    perror("ls-3");
	    return  -1;
	}
	PR(" ...this directory contains...\n");
	for (i = 0; i<n; i++) {
	    pr_dir(& db[i], lc);
	    if (lc && (i%6==5)) PR("\n");
	}
	close (fd);
	free (db);
	return  n;
    }
    else {
        PR("%s   \n", db->name);
	free (db);
	return  1;
    }
}

//--------------------------------
#define X2C(x)  (((x)<=9)?('0'+(x)):('a'+(x)-10))
static  char  ebuf[2048];
void print_sx(char *ss, int len)
{
    char  ch;
    int  i,j;
    for (i=0, j=0; i<len; i++) {
        if (j >= sizeof(ebuf)-4)
	    break;
        ch = ss[i];
        if ((ch >= ' ' && ch < '~') || (ch=='\n')) {
            ebuf[j++] = ch;
        }
        else{
            ebuf[j++] = '`';
            ebuf[j++] = X2C((ch>>4) & 0xF);
            ebuf[j++] = X2C(ch & 0xF);
        }
    }
    ebuf[j] = 0;
    print("%s", ebuf);
}

//-------------------------------------------------
#define ZZZLEN  8192  
char zzzbuf[ZZZLEN];
char xxxbuf[ZZZLEN];

static char * title =
  "      o o O O  <><> LP49 Quasi SHELL 2010-06-30 <><> \n"
  "     o     __     _________  _________  _________    \n"
  "   _[]----|##|__ | # # # # || # # # # || # # # # |   \n"
  "  ()______|__|__||_________||_________||_________|   \n"
  "  ##oo ()()() 0 0  ()   ()    ()   ()    ()  ()      \n"
  "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^  \n"; 

static char  SL[2048] =
  "      0 00 OO  9_9)  @_@)                             \n"
  "     0     __     _________  _________  _________     \n"
  "   _[]----|##|__ | # # # # || # # # # || # # # # |    \n"
  "  ()______|__|__||_________||_________||_________|    \n"
  "  ##oo ()()() 0 0  ()   ()    ()   ()    ()  ()       \n"
  "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^   \n" 
  "                                                      \n"
;

//---------------------------------------------------
void cat(char *name)
{
  int       fd, cnt;

    if ((fd = open(name, 0)) < 0)   {
        perror("cat"); 
	return;
    }  
    while (check_clock('t', ">read"),(cnt = read(fd, zzzbuf, sizeof zzzbuf)) > 0)
    {   
        check_clock('t', "<read");
        zzzbuf[cnt] = 0;
	print_sx(zzzbuf, cnt);
	zzzbuf[0] = 0;
    }
    print("\n");
    close(fd)  ;
}

//---------------------------------------------------------
static char * argv[32] = {0};
static  char  linebuf[100];
static  char  s[100];

typedef struct {
    char   inbuf[128];
    int    onum;
    char   option[16];
    int    vnum;
    char*  arg[16];
} param_t;

param_t  param;

static int isalpha(char c)
{
    if (((c >= 'A') & (c <= 'Z')) | ((c >= 'a') & (c <= 'z')))
        return 1;
    else 
        return  0;
}

static int isgraph(char c)
{
  if ((c >= '#') & (c <= 'z')) 
     return 1;
  else 
    return  0;
}

void parse_args(param_t  *_prm, char *_linebuf)
{
    char  *p, *q;

    strncpy(_prm->inbuf, linebuf, 128);
    _prm->onum = _prm->vnum = 0;
    for (p = _prm->inbuf; ; p++) {
        if ((*p == 0) | (*p == '\n')) 
	    break;
	while (*p == ' ') *p++ = 0;

	switch(*p) {
	case  '-':
	    while(isalpha(*(++p)))
	        _prm->option[_prm->onum++] = *p;
	    *p = 0;
	    break;

	case  '"':
	    q = ++p;
	    while(*p != '"') p++;
	    *p = 0;
	    _prm->arg[_prm->vnum++] = q ;
	    break;

	default:
	    q = p;
	    while(isgraph(*p)) p++;
	    *p = 0;
	    _prm->arg[_prm->vnum++] = q ;
	}
    }
}

//=========================================================
void  namespace1()
{
    int  rc;
    if (chdir("/") < 0) L4_KDB_Enter("chdir /");
    //    getwd(pwdname, sizeof(pwdname));

    l4printf_b("bind  #c /dev\n");
    if (bind("#c", "/dev", MAFTER) < 0) L4_KDB_Enter("bind") ;
 
    if ((fd0 = open("/dev/cons", OREAD)) < 0) L4_KDB_Enter("Open-err-fd0");
    if ((fd1 = open("/dev/cons", OWRITE)) < 0) L4_KDB_Enter("Open-err-fd1");
    if ((fd2 = open("/dev/cons", OWRITE)) < 0) L4_KDB_Enter("Open-err-fd2");
    sleep(50); //L4_Sleep(L4_TimePeriod(50000));  // 50ms
 
    l4printf_b("bind  #d /fd\n");
    rc = bind("#d", "/fd", 0);               // bind the DEVDUP  080920
    if (rc < 0) L4_KDB_Enter("bind #d") ;

    l4printf_b("bind -c #e /env \n");
    rc = bind("#e", "/env", MCREATE);        // bind the DEVENV  080920
    if (rc < 0) L4_KDB_Enter("bind #c") ;

    l4printf_b("bind  #p /proc \n");
    rc = bind("#p", "/proc", 0);             // bind the DEVDUP  080920
    if (rc < 0) L4_KDB_Enter("bind #p") ; 

    l4printf_b("bind -c #s /srv \n");
    rc = bind("#s", "/srv", MREPL|MCREATE);  // bind the DEVSRV
    if (rc < 0) L4_KDB_Enter("bind #s") ;

    l4printf_b("bind -ac #R / \n");         // devrootfs
    rc = bind("#R", "/", MAFTER|MCREATE);   // bind the DEVROOTFS
    if (rc < 0) L4_KDB_Enter("bind #R") ;
}

void  namespace2() {
     int  rc;
    l4printf_b("bind -a #I0 /net \n");
    rc = bind("#I", "/net", MAFTER);
    if (rc < 0) L4_KDB_Enter("bind #I0") ;
                                                                   
    l4printf_b("bind -a #l /net \n");
    rc = bind("#l", "/net", MAFTER);
    if (rc < 0) L4_KDB_Enter("bind #l") ;
}

//======= xxx_cmd functions ===============================

//----------  ls  [<dir>] -------------
void ls_cmnd()
{
            char *where = strtok(NULL, " &\n");
	    int  rc;

	    if (where != NULL)  {
                strcpy(s, where);
		rc = listdir(s, 0); 
	    } 
	    else {
	        listdir(pwdname, 0);  
	    }
} 

//----------  lc  [<dir>] -------------
void lc_cmnd()
{
            char *where = strtok(NULL, " &\n");
	    int  rc;
	    if (where != NULL)  {
                strcpy(s, where);
		rc = listdir(s, 1); 
	    } 
	    else 
	        listdir(pwdname, 1);  
} 
	
//-------- cd  [<dirname>] -------------
void cd_cmnd()
{	    
            int  rc;  char *p;
            p = strtok(NULL, " &\n");
	    if (p != NULL)   {  
                strcpy(s, p);
		 p = s;      
		 while (*p)  p++;
		 if (p != s) p--;
		 if (*p != '/')  // terminating by '/'
		    strcat(s, "/");
	    } 
	    else 
	        strcpy(s, "/");

	    if ((rc = chdir(s))==0) {
	        //!!    getwd(pwdname, sizeof(pwdname));
	        char fullpath[128];
		mkfullpath(s, fullpath);
		strcpy(pwdname, fullpath);
	    }
	    XPR(rc);
} 

       
//------------ pwd  ---------------------
void pwd_cmnd()
{
	    // getwd(pwdname, sizeof(pwdname));   //%
	    PR("%s\n", pwdname); 
}

//------ cat  <name>  -------------------
void cat_cmnd()
{
           cat(strtok(NULL, " &\n")); 
}

//------ dog file text -----------------------------
void dog_cmnd()
{
    int       fd, cnt;
    char  *fname = strtok(NULL, " ");
    char  *txt = strtok(NULL, " &\n");

    if ((fd = open(fname, 1)) < 0)   {
        perror("dog"); 
	return;
    }  
    cnt = write(fd, txt, strlen(txt));
    //print("dog(%d, %s, %d):%d\n", fd, txt, strlrn(txt), cnt);
    close(fd)  ;
}


//----- echo  <text> ----------------
void echo_cmnd()
{
           PR("%s\n", strtok(NULL, "&\n")); 
}

//------ create  [-rwWxc]  <name>  <perm> -----------
void create_cmnd()
{
	    unsigned  mode = 0;
	    char     *fname = param.arg[1];
	    unsigned  perm = atoi(param.arg[2]);
	    int  fd;
	    int  i;
	    for (i=0; i<param.onum; i++)
	        switch(param.option[i]) {
		case 'r':  mode |= 0x0; break;
		case 'W':  mode |= 0x01; break;
		case 'w':  mode |= 0x02; break;
		case 'x':  mode |= 0x03; break;
		case 't':  mode |= 0x10; break;
		case 'c':  mode |= 0x20; break;
		case 'd':  mode |= 0x40; break;
		case 'e':  mode |= 0x1000; break;
		default: ;
		}

	    fd = create(fname, mode, perm);    //PR(fd);
	    PR("create(%s, %x, %x)->%d \n", fname, mode, perm, fd);
}


//------ rm   <fname>  -----------
void rm_cmnd()
{
	    char *fname = strtok(NULL, " &\n");  
	    int  rc;
	    rc = remove(fname);    XPR(rc);
}

//-------- open  [-rwxt...]  <name>  <mode> --------------
// -r  OREAD   0  
// -W  OWRITE  1       
// -w  ORDWR   2       
// -x  OEXEC   3       
// -t  OTRUNC  16  or'ed in (except for exec), truncate file first 
// -c  OCEXEC  32  or'ed in, close on exec 
// -d  ORCLOSE 64  or'ed in, remove on close 
// -e  OEXCL   0x1000   or'ed in, exclusive use (create only) 

void open_cmnd()
{
	    unsigned  mode = 0;
	    char *fname = param.arg[1];
	    int  fd;
	    int  i;
	    for (i=0; i<param.onum; i++)
	        switch(param.option[i]) {
		case 'r':  mode |= 0x0; break;
		case 'W':  mode |= 0x01; break;
		case 'w':  mode |= 0x02; break;
		case 'x':  mode |= 0x03; break;
		case 't':  mode |= 0x10; break;
		case 'c':  mode |= 0x20; break;
		case 'd':  mode |= 0x40; break;
		case 'e':  mode |= 0x1000; break;
		default: ;
		}

	    fd = open(fname, mode);   XPR(fd);
	    PR("open(\"%s\", %x)->%d \n", fname, mode, fd);
}


//----- close  <fd>  ------------------
void close_cmnd()
{
	    int  fd = atoi(strtok(NULL, " &\n"));
	    int  rc;
	    rc = close(fd);  XPR(fd);
}


//-----  seek  <fd>  <offset>  <type> --------
void seek_cmnd()
{
	    int  fd = atoi(strtok(NULL, " "));
	    int  offset = atoi(strtok(NULL, " "));
	    int  whence = atoi(strtok(NULL, " &\n"));
	    vlong  seekpos;
	    seekpos = seek(fd, offset, whence);  XPR(seekpos);
}

//----  write  <fd>  <text> ----------
void write_cmnd()
{
	    int fd = atoi(strtok(NULL, " "));
	    char *txt = strtok(NULL, "&\n");
	    int   len;
	    int  rc;
	    // if (txt[0] == '$') txt++;
	    // if (strcmp(txt, "SL") == 0)  txt = SL;
	    len = strlen(txt);
	    // if (strcmp(txt, "pg") == 0){ txt = pg, len = sizeof(pg);}

	    rc = _write(fd,  txt, len);  XPR(rc);
}

//----- read  <fd>  -------------
void read_cmnd()
{
	    int fd = atoi(strtok(NULL, " &\n"));
	    int  rc;
	    rc = _read(fd, zzzbuf, 2048);  XPR(rc);
	    zzzbuf[rc] = 0;
	    print_sx(zzzbuf, rc);
	    //    print("\n%s \n", zzzbuf);
	    memset(zzzbuf, 0, sizeof(zzzbuf));
}
        
//----- pwrite  <fd>  <offset>  <text>  ------
void pwrite_cmnd()
{
	    int fd = atoi(strtok(NULL, " "));
	    unsigned long long offset = atoi(strtok(NULL, " "));
	    char *txt = strtok(NULL, "&\n");
	    int   len;
	    int  rc;
	    if (txt[0] == '$') txt++;
	    if (strcmp(txt, "SL") == 0)  txt = SL;
	    len = strlen(txt);

	    rc = pwrite(fd,  txt, len, offset);  XPR(rc);
}

//------- pread  <fd>  <offset> -------
void pread_cmnd()
{
	    int fd = atoi(strtok(NULL, " "));
	    unsigned long long offset = atoi(strtok(NULL, " &\n"));
	    int  rc;
	    rc = pread(fd, zzzbuf, 2048, offset);  XPR(rc);
	    zzzbuf[rc] = 0;
	    print_sx(zzzbuf, rc);
	    //    print("\n%s \n", zzzbuf);
	    memset(zzzbuf, 0, sizeof(zzzbuf));
}

//----- preadx  <fd>  <offset>  [<size>] ----------
void preadx_cmnd()
{
	    int  i, j, k;
	    unsigned char c;
	    char  buf[128];
	    char  elem[32];
	    int   size;
	    int  rc;
	    int fd = atoi(strtok(NULL, " "));
	    unsigned long long offset = atoi(strtok(NULL, " &\n"));
	    size = atoi(strtok(NULL, " &\n"));
	    if (size == 0) size = 2048;

	    rc = pread(fd, zzzbuf, size, offset);  XPR(rc);

	    for(i=0; i<rc; i+=16) {
	        buf[0] = 0;
		for (j=i; j<i+16; j+=4) {
		    if (j%4 == 0) strcat(buf, " ");
		    for(k=3; k>=0; k--) {
		        c = zzzbuf[j+k];
			l4snprintf(elem, 32, "%.2x", c);
			strcat(buf, elem);
		    }
		}
 
		strcat(buf, "  ");
		for (j=i; j<i+16; j++) {
		    c = zzzbuf[j];
		    if ( c >= ' ' && c < '~')
		        l4snprintf(elem, 32, "%c", c);
		    else
		        l4snprintf(elem, 32, ".");
		    strcat(buf, elem);
		}
		PR("%s\n", buf);
	    }
}
        
//----- cp  <from>...  <to> ----------------
void cp_cmnd()
{
	    char *argslist = strtok(NULL, "&\n"); 
	    qsh_cp(argslist);
}

//------- copy  <from>  <to> -------
void copy_cmnd()
{
            char *from = strtok(NULL, " ");
	    char *to   = strtok(NULL, "\n");
           
	    if (from == NULL)
               PR("copy: missing file names \n"); 
	    else
	      if (to == NULL)
		PR("copy: missing to-file name\n"); 
	      else
		copy(from, to);
} 

//------ mkdir <dirname>  --------------
void mkdir_cmnd()
{
            char *dirname = strtok(NULL, " &\n");
	    qsh_mkdir(dirname);
} 

//------- fd2path  <fd>  <buf>  <len> -----------
void fd2path_cmnd()
{
	    // TBD
            int  fd = atoi(strtok(NULL, " "));
            char *buf = strtok(NULL, " ");
            int  len  = atoi(strtok(NULL, " &\n"));
	    int  rc;
	    rc = fd2path(fd, buf, len);  XPR(rc);
} 

//------- sync ----------------------
void sync_cmnd()
{
	     PR("sync : to be defined\n");
} 

//------  help -----------------
void help_cmnd()
{
	    help( );
}

//------ bind  [-abcC]  <newpath>  <mountpoint> ------- 
//    MREPL   0x0000  
// -b MBEFORE 0x0001 
// -a MAFTER  0x0002 
// -c MCREATE 0x0004 
// -C MCACHE  0x0010 

void bind_cmnd()
{
	     char *new = param.arg[1]; 
	     char *mntpoint = param.arg[2];
	     unsigned  flag = 0;
	     int  i, rc;
	     for (i=0; i<param.onum; i++)
	         switch(param.option[i]) {
		 case 'a':  flag |= 0x02; break;
		 case 'b':  flag |= 0x01; break;
		 case 'c':  flag |= 0x04; break;
		 case 'C':  flag |= 0x10; break;
		 default: ;
		 }
           
	     rc = bind(new, mntpoint, flag);  XPR(rc);
} 

//----- mount  [-abcC]  <srvname>  <mountpoint>  <spec>  -----
void mount_cmnd()
{
            unsigned  flag = 0;
	    char *srvname = param.arg[1]; 
	    char *mntpoint  = param.arg[2]; 
	    char *spec = param.arg[3]; 
	    int  fd;
	    int  rc;
	    int  i;
	    for (i=0; i<param.onum; i++)
	       switch(param.option[i]) {
	       case 'a':  flag |= 0x02; break;
	       case 'b':  flag |= 0x01; break;
	       case 'c':  flag |= 0x04; break;
	       case 'C':  flag |= 0x10; break;
	       default: ;
	       }
           
	    if (spec == NULL) spec = "";
	    fd = open(srvname, ORDWR);
	    if(fd < 0){
	        PR("mount: can't open %s\n", srvname);
		return; // thnaks to mr Ueda.
	    }

	    //PR("mount %x <%s> <%s> <%s> \n", flag, srvname, mntpoint, spec);
	    rc = mount(fd, -1, mntpoint, flag, spec); 
	    XPR(rc);
}

//------- unmount  [<name>]  <mntpoint> -----
void unmount_cmnd()
{
            char *name = strtok(NULL, " ");
	    char *mntpoint = strtok(NULL, " \n");
	    int  rc;
	    PR("unmount %s %s\n", name, mntpoint);
           
	    if (name == NULL && mntpoint == NULL){
	        PR("umount: missing parameters\n");
		return;
	    }

	    if(mntpoint == NULL) {
	        mntpoint = name;
		name = NULL;
	    }
	    rc = unmount(name, mntpoint); 
	    XPR(rc);
} 
	 
//---------- tar --------------------  
void tar_cmnd()
{
	    char  argbuf[128];
	    char  *argv[16];
	    char  *cp;
	    int   argc;
	    strncpy(argbuf,  strtok(NULL, "&\n"), 128);
	   
	    argv[0] = "tar";
	    for (argc = 1; argc<16; argc++)    {
	        cp = argv[argc] =  (argc == 1) ? 
		    strtok(argbuf, " \n") : strtok(NULL, " \n");
		if (cp == NULL) break;
	    }
	    //	   tar_main(argc, argv);
}

//------ zip -f <zipfile>  <file> .... -------------  
void zip_cmnd()
{
	    char  *z = strtok(NULL, " ");
            char  *zipfile = strtok(NULL, " ");
	    char  *files = strtok(NULL, "&\n");
	    int qsh_zip(char *, char *);

	    if (strcmp(z, "-f")) { 
	        PR("zip -f zipfile  file... \n");
		return;
	    }
	   
	    if (files == (char*)0 || strcmp(files, "")) { 
	        PR("zip -f zipfile  file... \n");
		return;
	    }
	   
	    PR("zip -z %s   %s\n", zipfile, files);
	    qsh_zip(zipfile, files);
}

//------ unzip -f <zipfile>  --------------  
void unzip_cmnd()
{
	    char  *z = strtok(NULL, " ");
            char  *zipfile = strtok(NULL, "&\n ");
	    int  rc;
	    int qsh_unzip(char *);

	    if (strcmp(z, "-f")) { 
	        PR("unzip -f zipfile \n");
		return;
	    }
	   
	    PR("unzip -f %s \n", zipfile);
	    rc = qsh_unzip(zipfile);
	    XPR(rc);
}

//--------- process commands --------------------
#define  EBPCHK(s)  // l4printf_r("EBP-%s:%x \n", s, _EBP) 

#define  STKCHK(ss)  {register uint *stkptr  asm("%esp");   \
            register uint *frmptr  asm("%ebp");  \
            l4printf_r("%s<sp:%x,fp:%x,rtn:%x> ", ss, stkptr, frmptr, frmptr[1]); } 

//------ rfork  <flag>  --------
void rfork_cmnd()
{
	    unsigned  pid;
	    char *flagstr = strtok(NULL, " \n");

	    pid = rfork(atoi(flagstr));  
	    if (pid == 0) {
	      int i;
	      for (i = 0; i<3; i++){
		  PR("I am a forked child \n");
		  L4_Yield();
		  L4_Sleep(L4_TimePeriod (300000UL)); // 300ms
	      }

	      exec("/tmp/ex2", 0);
	    }
	    else {
	        PR("I am a parent process; forked child:%d \n",  pid);
	    }
}

//------- fork  [<progname>]----------------
void fork_cmnd()
{
	    unsigned  pid;
	    unsigned  flag = RFFDG | RFREND | RFPROC;  // 1<<2 | 1<<13 | 1<<4 
	    int  i;
	    char *progname = strtok(NULL, " \n");

	    STKCHK("A");
	    pid = rfork(flag);  
	      print("=== fork();%d \n", pid);    sleep(100);

	    if (pid == 0) {
	        PR("I am a forked child: %x. \n", L4_Myself());
		EBPCHK("fork-30");  //
		STKCHK("B");
		L4_Yield();
		L4_Sleep(L4_TimePeriod (400000UL)); // 400 ms

		if (progname) {
		    exec(progname, 0);
		}
		else {
		    // exec("/t/a/ex2", 0);
		    // never come here ...
		    for(i = 0; i<20;i++) {
		        l4printf("(~_~) ");
			L4_Sleep(L4_TimePeriod (1000000UL)); // 1000 ms
		    }
		    L4_Sleep(L4_Never); 
		}
	    }
	    else {
	        EBPCHK("fork-50"); //
	        STKCHK("C");
		PR("I am a parent; forked child is:%d \n",  pid);
		L4_Sleep(L4_TimePeriod (4000000UL)); // 4000 ms
	    }
}

//------- exec  <progname>  ----------------
void exec_cmnd()
{
	    char *name = strtok(NULL, " ");  
	    int  i, rc;
	    argv[0] = name;

	    for (i = 1; i<16 ; i++)   {
	        argv[i] = strtok(NULL, " &\n");
		if (argv[i] == NULL)
		    break;
	    }

	    PR("# exec("); 
	    for(i=0; i<16; i++) {
	        PR("%s ", argv[i]); 
		if(argv[i] == 0) break;
	    }
	    PR(")\n");
	    rc = exec(name, argv);
	    XPR(rc);
}

//------- spawn  <progname> ----------------
void spawn_cmnd()
{
	    char *name = strtok(NULL, " ");  
	    int  i, rc;
	    argv[0] = name;

	    for (i = 1; i<16 ; i++)  {
	        argv[i] = strtok(NULL, " &\n");
		if (argv[i] == NULL)
		   break;
	    }
#if 0
	    PR("# spawn("); 
	    for(i=0; i<16 && argv[i] ; i++){ PR("%s ", argv[i]);  }
	    PR(")\n");
#endif
	    rc = spawn(name, argv);
	    XPR(rc);
}

//------ pipe  ------------
void pipe_cmnd()
{
            int  fd[2], rc;
	    rc = pipe(fd);  XPR(rc);
	    PR("pipe fd[0]=%d fd[1]=%d \n", fd[0], fd[1]);
} 
        
//------ dup  <oldfd>  <newfd>   ------------
void dup_cmnd()
{
	    int  oldfd =  atoi(strtok(NULL, " "));
	    int  newfd =  atoi(strtok(NULL, " &\n"));
	    int  rc;
	    rc = dup(oldfd, newfd);  XPR(rc);
} 
        
//------ exits  <msg>   ------------
void exits_cmnd()
{
	    char  *msg =  strtok(NULL, " &\n");
	    exits(msg); 
} 
        
//---------- pi -------------
void pi_cmnd()
{
            pi(); 
}

//---------- credits ----------
void credits_cmnd()
{
           credits(); 
}

//------ putenv  <name>  <value> ------- 
void putenv_cmnd()
{
	    char *name = strtok(NULL, " ");
	    char *val  = strtok(NULL, "&\n");
	    int  rc;
	    rc = putenv(name, val);   XPR(rc);
}

//-------- getenv  <name>  ----------
void getenv_cmnd()
{
	    char  *cp;
	    char *name = strtok(NULL, " \n");
	    cp = getenv(name);    XPR(cp);
	    PR("%s:  %s \n",  name, cp);
}

//------- exit --------------
void exit_cmnd()
{
	   //	   exit(0);
}

//======= Setup commands =================================
//------- ipsetup  <IPaddress>  <GWaddress> ---------------	 
void ipsetup_cmnd()
{
	    char *IPadrs;
	    char *GWadrs;

	    IPadrs = strtok(NULL, " &\n");  
	    GWadrs = strtok(NULL, " &\n");  
	    if (IPadrs == 0) IPadrs = "10.0.0.2";
	    if (GWadrs == 0) GWadrs = "10.0.0.1";
	    ipsetup(IPadrs, GWadrs);
}

//------- usbsetup -------------------	 
void usbsetup_cmnd()
{
    int  rc;
	    rc = usbsetup();   XPR(rc);
}

//----- fdstart ------------------
void fdstart()
{
	    //l4printf("dossrv is running by default\n");
	    int  rc;
	    rc = fd_start();
	    // EXPR(rc);
}

//----- xga -------------------------
void  xga_cmnd( )
{
  int   pid, rc, fd;
    argv[0] = "/bin/aux/vga";
    argv[1] = "-l";
    argv[2] = "1024x768x8";
    argv[3] = nil;

    rc = spawn(argv[0], argv);
    if (rc<0) {
        PR("/bin/aux/vga: err=<%d> \n", rc);
        return;
    }
    else  {
	L4_Yield(); 
	pid = waitpid();
    }
    fd = open("/dev/mousectl", 0x02);
    if (fd<0)   return;

    _write(fd, "ps2", 3);
    sleep(10);
    close(fd);
}

//----- cga -------------------------
void  cga_cmnd( )
{
  int   pid, rc, fd;
    argv[0] = "/bin/aux/vga";
    argv[1] = "-l";
    argv[2] = "text";
    argv[3] = nil;

    rc = spawn(argv[0], argv);
    if (rc<0) {
        PR("/bin/aux/vga: err=<%d> \n", rc);
        return;
    }
    else  {
	L4_Yield(); 
	pid = waitpid();
    }
    sleep(10);
}




//======= Test commands ==================================
//------ SL -----------------------
void SL_cmnd()
{
	  print("\n%s  <%d>\n", SL, strlen(SL));
}
        
//------ putsl -----------------------
void putsl_cmnd()
{
	    //	    dbg_time(2, 0);
	    int  rc;
	    rc = _put(1, SL, strlen(SL));  
	    //	    dbg_time(2, "qsh:put");  XPR(rc);
}
        
//---- put <text> ----------------------
void put_cmnd()
{
	    char *txt = strtok(NULL, "&\n");
	    int   len, rc;

	    if (txt[0] == '$') txt++;
	    if (strcmp(txt, "SL") == 0)  txt = SL;
	    len = strlen(txt);
	    //	    dbg_time(2, 0);
	    rc = _put(1, txt, len);  
	    //	    dbg_time(2, "qsh:put");
	    XPR(rc);
}

//---- get  ----------------------
void get_cmnd()
{
	    int  rc;
	    //	    dbg_time(2, 0);
	    rc = _get(1, zzzbuf, 2048); 
	    //	    dbg_time(2, "qsh:get");
	    print("%s\n", zzzbuf);
	    memset(zzzbuf, 0, sizeof(zzzbuf));
	    XPR(rc);
}
      

//------ givesl -----------------------
void givesl_cmnd()
{
	    int  rc;
	    //	    dbg_time(2, 0);
	    rc = give(1, SL, strlen(SL));  
	    XPR(rc);
	    //	    dbg_time(2, "qsh:put");  
}
        
//---- give <text> ----------------------
void give_cmnd()
{
	    char *txt = strtok(NULL, "&\n");
	    int   len;
	    int  rc;
	    if (txt[0] == '$') txt++;
	    if (strcmp(txt, "SL") == 0)  txt = SL;
	    len = strlen(txt);
	    //	    dbg_time(2, 0);
	    rc = give(1, txt, len);  
	    //	    dbg_time(2, "qsh:put");
	    XPR(rc);
}

//---- take  ----------------------
void take_cmnd()
{
            int size, rc;
	    size = atoi(strtok(NULL, " &\n"));
	    if (size == 0) size = 2048;

	    //	    dbg_time(2, 0);
	    rc = take(1, zzzbuf, size); 
	    //	    dbg_time(2, "qsh:get");
	    print("%s\n", zzzbuf);
	    memset(zzzbuf, 0, sizeof(zzzbuf));
	    XPR(rc);
}

//---- dump  <procnr> <startadrs> <size>  ------
void dump_cmnd()
{
	    unsigned procnr = atoi(strtok(NULL, " "));
	    unsigned startadr = atoi(strtok(NULL, " "));
	    unsigned size = atoi(strtok(NULL, " &\n"));
	    int  n, i;
	    unsigned char  c, bb[256];
	    L4_ThreadId_t myself = L4_Myself();

	    if (size > 255) size = 256;
	    n = proc2proc_copy(procnr, startadr, TID2PROCNR(myself), (unsigned)bb, size);

	    for(i=0; i<size; i++) {
	        if ( i>0 && i%16 == 0) PR("\n");
		c = bb[i]; 
		if ( c >= ' ' && c < '~')
		    PR("%.2x:%c", c, c);
		else 
		    PR("%.2x", c);
	    }
	    PR("\n");
}
       
//---- maptest  <size> ----------------------
#define CHR(i)  ((i%128<'!')?('.'):((i%128>'}')?('~'):(i%128)))

void maptest_cmnd()
{
	    //  int   fd = atoi(strtok(NULL, " "));
	    int   size = atoi(strtok(NULL, " &\n"));
	    int   i;
	    int  rc;
	    if (size > ZZZLEN) size = ZZZLEN;

	    for (i=0; i<size; i++) zzzbuf[i] = CHR(i); 
	    rc = give(9, zzzbuf, size);  
	    XPR(rc);

	    for (i=0; i<size; i++) xxxbuf[i] = 0;

	    rc = take(9, xxxbuf, size); 

	    l4printf_b("data-qsh-sent/received:\n");
	    for (i=0; i<size; i+=300)  print("<%c%c>\t", zzzbuf[i], xxxbuf[i]);

	    XPR(rc);
}
      
//---- malloctest  <size> -------------------------
void malloctest_cmnd()
{
	    int   sz = atoi(strtok(NULL, " &\n"));
	    char  *p, *q;
	    int   i;
	    static  char *mtbl[32];
	    static  int  next = 0;

	    if (next==32) {
	        print("malloc tbl full\n"); 
	        return;
	    }
	    if (sz == 0){
	       for(i=0; i<next; i++) {
		  print("free(%x)  ", mtbl[i]);
		  free(mtbl[i]);
	       }
	       next = 0;
	       print("\n");
	       return ;
	    }

	    p = malloc(sz);
	    print("malloc(%d):%x  ", sz, p);
	    for (i=0; i<sz; i++) p[i]=CHR(i);
	    for (i=0; i<sz; i++)
	        if(p[i] != CHR(i))
		    l4printf("NG<i=%d>  ", i);

	    q = realloc(p, sz*2);
	    print("realloc(%d):%x  ", sz*2, q);
	    for (i=sz; i<sz*2; i++) q[i]=CHR(i);
	    for (i=0; i<sz*2; i++)
	        if(q[i] != CHR(i))
		    l4printf("RNG<i=%d>  ", i);

	    mtbl[next++] = q;

	    print("\n");
	    //XPR(rc);
}
      

//========== debug =============
void debug_cmnd()
{
            char *arg = strtok(NULL, " &\n");
	    if (arg != NULL)  {
                if (!strcmp(arg, "on")) debug=1;  
		if (!strcmp(arg, "off")) debug=0;
	    }  
	    PR("debug: ");
	    if (debug) PR("enabled\n"); else PR("disabled\n");
} 

//------ d  <table>  <adrs> -------------
void d_cmnd()
{
            char *tbl = strtok(NULL, " ");
	    char *which = strtok(NULL, " &\n");  // addres/procnr 
	    int  rc;
	    rc = _d(tbl, which);
	    XPR(rc);

	    if (!strcmp(tbl, "tm")) {
		pr_elapse(which[0], 1);   
	    }
	    else  if (!strcmp(tbl, "tmz") ) {
		pr_elapse(which[0], 0);   
	    }
} 

//------ getpid  ------------------
void getpid_cmnd()
{
	    int  rc;
            check_clock('g', ">getpid");
	    rc = getpid();
            check_clock('g', "<getpid");
	    XPR(rc);
} 

//------ ps  ------------------
void ps_cmnd()
{
	    _d("ps", 0); 
} 

//-------- kd ------------------
void kd_cmnd()
{
           L4_KDB_Enter("hit 'g' to continue"); 
}

//------- pgmap  <procnr> ----------------
void pgmap_cmnd()
{
	    int procnr = atoi(strtok(NULL, " "));  
	    int adrs = atoi(strtok(NULL, " "));  
	    int len = atoi(strtok(NULL, " &\n"));  
	    print("%x %x %x\n", procnr, adrs, len);

	    L4_ThreadId_t  mypager = L4_Pager();
	    L4_MsgTag_t    tag;

	    L4_LoadMR(1, procnr);
	    L4_LoadMR(2, adrs);
	    L4_LoadMR(3, len);
	    L4_LoadMR(0, CHK_PGMAP << 16 | 3);
	    tag = L4_Call(mypager);
}

//------- code  <string> ----------------
void code_cmnd()
{
	    char *cp =  strtok(NULL, " ");  
	    char  c1;
	    for(; c1 = *cp; cp++) {
	        PR("%x-", c1);
	    }
	    PR("\n");
}

//------- partition  <fdstr>  -----
void  partition_cmnd()
{
            char *fdstr = strtok(NULL, " \n");
	    int  fd = atoi(fdstr);
	    int  rc;
	    rc = mbrpartition(fd);
	    XPR(rc);
} 

//-------- clk --------------
unsigned int nops_microsec;

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

void delayloop(int n)
{
	unsigned long long  tt1, tt2, tt3;
	unsigned int dd;
	tt1 = L4_SystemClock().raw;
	loop_nops(n);
	tt2 = L4_SystemClock().raw;
	tt3 = tt2 - tt1;
	dd = tt3;
	PR("<%d> DD=%d usec\n", n, dd);
}

void microsec_delay(unsigned int micro_sec)
{
	unsigned long long  tt1, tt2, tt3;
	unsigned int dd;
	unsigned int nn;
	nn = micro_sec * nops_microsec;
	tt1 = L4_SystemClock().raw;
	loop_nops(nn);
	//	L4_SystemClock();
	tt2 = L4_SystemClock().raw;
	tt3 = tt2 - tt1;
	dd = tt3;
	PR("<%d usec> %d DD=%d usec\n", micro_sec, nn, dd);
}

int  calc_speed(int n)
{
	unsigned long long  tt1, tt2, tt3;
	unsigned int cycle_usec;

	// Warming up
	L4_SystemClock();  L4_SystemClock();
	L4_SystemClock();  L4_SystemClock(); 

	tt1 = L4_SystemClock().raw;
	loop_nops(n);
	tt2 = L4_SystemClock().raw;
	tt3 = tt2 - tt1;
	cycle_usec = n/(tt3-1);
	PR("cycle_usec = %d  %d\n", cycle_usec, tt3);
	return cycle_usec;
}

void clk_cmnd()
{
	    L4_Clock_t  clk;
	    static L4_Clock_t  lastclk;
	    unsigned long long  delta;
	    extern  L4_Clock_t  L4_SystemClock();
	    uint  sec, min, hour, day;

	    nops_microsec = calc_speed(10000);
	    nops_microsec = calc_speed(100000);

	    delayloop(1); 
	    delayloop(10); 
	    delayloop(100); 
	    delayloop(1000); 
	    delayloop(10000); 
	    delayloop(100000); 
	    delayloop(1000000); 
	    
	    microsec_delay(1);
	    microsec_delay(10);
	    microsec_delay(100);

	    clk = L4_SystemClock();  // micro-sec
	    delta = clk.raw - lastclk.raw;  
	    sec = clk.raw / 1000000ULL;
	    min = sec /60;
	    hour = min / 60;
	    day = hour / 24;

	    PR("microsec=%s delta=%d ", vlong2a(clk.raw), (unsigned)delta); 
	    PR("day=%d hour=%d min=%d sec=%d\n", day, hour % 60, min % 60, sec % 60);

	    lastclk = clk;
}

//------- sleep -----------
void sleep_cmnd()
{
	    int   millisec = atoi(strtok(NULL, " &\n"));
	    uvlong  usec1 , usec2, delta;

	    print(" sleep %d millisec\n",  millisec);
	    usec1 = L4_SystemClock().raw;
	    sleep(millisec);
	    usec2 = L4_SystemClock().raw;
	    delta = usec2 - usec1;
	    print(" usec1=%lld  usec2=%lld  delta=%lld \n",
		  usec1, usec2, delta);
}

//------- delay -----------
void delay_cmnd()
{
	    int   millisec = atoi(strtok(NULL, " &\n"));
	    sleep(millisec);
}


//======= External commands ==========================
//-------  <progname> [<argment>...] ----------------
void  progname_cmnd(char *cmd, int background)
{
            int  i, pid, rc;
	    argv[0] = cmd;

	    for (i = 1; i<16 ; i++)  {
	        argv[i] = strtok(NULL, " &\n");
		if (argv[i] == NULL)
		   break;
	    }
	    rc = spawn(cmd, argv);
	    if (rc<0) {
	        PR("spawn-err=<%d> \n", rc);
		return;
	    }
	    else
	    if (background == 0) {
	        // PR("spawn-wait\n");
		L4_Yield(); // sleep(10000);
	        pid = waitpid();
	        // PR("spawn=<%d,%d> \n", rc, pid);
	    }else{
	        PR("spawn=<%d> \n", rc);
	    }
}

//==========================================================
int main( )
{
    char  *cmd, *p;
    int   background = 0; // background operator: '&'  
    int   rc, c;
    int   fd;
    //    if (chdir("/") < 0) L4_KDB_Enter("qsh chdir /");
    //    getwd(pwdname, sizeof(pwdname));
    //    if (bind("#c", "/dev", MAFTER) < 0) L4_KDB_Enter("bind") ;

#ifdef  INITPROC   // If this qsh is forked by init process, 
    PR("QSH is started by INIT process.\n");
    //    sleep(500); 
    //    PR(".......................\n");
#else          // Qsh is spawned directly by HVM.
    PR("QSH is started by HVM process.\n");
    namespace1();
    sleep(1000);
    ack2hvm(0); //090330
    // Here, we must wait for dossrv and 9660srv to become ready.
    sleep(1000);
    PR("Which media ?  Hit  c for CD, f for Floppy: ");
    c = getuchar();
    if ((c == 'f') || (c =='F'))
        rc = fd_start();
    else
         cd_start();
#endif 
    sleep(100);

    // l4printf_b("bind -ac /t/bin /bin \n"); 
    // rc = bind("/t/bin", "/bin", MAFTER);   
    // if (rc < 0) L4_KDB_Enter("bind /t/bin /bin") ;

    namespace2();
    //    l4printf_c(30, "Wait a while...");      
    //    sleep(500);
    getwd(pwdname, sizeof(pwdname));
    //---------------------------------------------------------------
    l4printf_c(30, "\n%s\n", title);  

    l4printf_c(26, "\n Boot CD is mounted on '/t', and commands appear on '/t/bin'");
    l4printf_c(26, "\n Also, '/t/bin' is bound to '/bin'.                         \n");

//  l4printf_c(26, "\n  Or, mount the virtual disk on /c :              \n");
//  l4printf_c(26, "  LP40[/]: bind -a #S /dev                        \n");
//  l4printf_c(26, "  LP40[/]: mount -a /srv/dos /c /dev/sdC0/dos-0   \n");
//  l4printf_c(26, "  LP40[/]: cd /c/bin                              \n");

    do {
        int  len;
        L4_Yield();  
	inputline(linebuf, 80);
	{
	  len = strlen(linebuf);
	  background = (linebuf[len-1] == '&') ? 1 : 0;  
	}
	zzzbuf[0]= 0;
	parse_args(&param, linebuf);

	cmd = strtok(linebuf, " &");
	if (debug) PR("shell: executing command '%s'\n", cmd);

	//-----------------------------------------------
        if (!strcmp(cmd, "") || (cmd == 0) ) {
	    continue;
	}
	else

	//========== Buildin commands =================
        //----------  ls  [<dir>] -------------
        if (!strcmp(cmd, "dir") | !strcmp(cmd, "ls") | !strcmp(cmd, "list")) 
	  ls_cmnd();
        else    
	
        //----------  lc  [<dir>] -------------
        if (! strcmp(cmd, "lc"))  
	  lc_cmnd();
        else    
	
        //-------- cd  [<dirname>] -------------
        if (!strcmp(cmd, "cd"))    
	  cd_cmnd();
        else  
       
        //------------ pwd  ---------------------
        if (!strcmp(cmd, "pwd")) 
	  pwd_cmnd();
        else  

	//------ cat  <name>  -------------------
        if (!strcmp(cmd, "cat"))
	   cat_cmnd();
	else

	//------ dog  <name>  <text> ------------------
        if (!strcmp(cmd, "dog"))
	   dog_cmnd();
	else

	//----- echo  <text> ----------------
        if (!strcmp(cmd, "echo"))
	  echo_cmnd();
	else

        //------ create  [-rwWxc]  <name>  <perm> -----------
        if (!strcmp(cmd, "create")) 
	  create_cmnd();
	else

        //------ rm   <fname>  -----------
        if (!strcmp(cmd, "remove") || !strcmp(cmd, "rm")) 
	  rm_cmnd();
	else

        //-------- open  [-rwxt...]  <name>  <mode> --------------
	// -r  OREAD   0  
	// -W  OWRITE  1       
	// -w  ORDWR   2       
	// -x  OEXEC   3       
	// -t  OTRUNC  16  or'ed in (except for exec), truncate file first 
	// -c  OCEXEC  32  or'ed in, close on exec 
	// -d  ORCLOSE 64  or'ed in, remove on close 
	// -e  OEXCL   0x1000   or'ed in, exclusive use (create only) 

        if (!strcmp(cmd, "open"))  
	  open_cmnd();
	else

	//----- close  <fd>  ------------------
        if (!strcmp(cmd, "close")) 
	  close_cmnd();
        else

	//-----  seek  <fd>  <offset>  <type> --------
	if (!strcmp(cmd, "seek")) 
	  seek_cmnd();
        else

        //----  write  <fd>  <text> ----------
	if (!strcmp(cmd, "write"))  
	  write_cmnd();
        else

        //----- read  <fd>  -------------
	if (!strcmp(cmd, "read")) 
	  read_cmnd();
	else
        
        //----- pwrite  <fd>  <offset>  <text>  ------
        if (!strcmp(cmd, "pwrite"))  
	  pwrite_cmnd();
        else

        //------- pread  <fd>  <offset> -------
        if (!strcmp(cmd, "pread"))  
	  pread_cmnd();
        else

        //----- preadx  <fd>  <offset>  [<size>] ----------
        if (!strcmp(cmd, "preadx"))  
	  preadx_cmnd();
        else
        
	//----- cp  <from>...  <to> ----------------
	if (!strcmp(cmd, "cp")) 
	  cp_cmnd();
	else

	//------- copy  <from>  <to> -------
        if (!strcmp(cmd, "copy"))   
	  copy_cmnd();
        else

        //------ mkdir <dirname>  --------------
        if (!strcmp(cmd, "mkdir"))   
	  mkdir_cmnd();
        else

	//------- fd2path  <fd>  <buf>  <len> -----------
	if (!strcmp(cmd, "fd2path"))  
	  fd2path_cmnd();
        else  

	//------- sync ----------------------
        if (!strcmp(cmd, "sync"))  
	  {
	     PR("sync : to be defined\n");
	} 
	else  

	//------  help -----------------
	if (!strcmp(cmd, "help")) 
	    help( );
	else

        //------ bind  [-abcC]  <newpath>  <mountpoint> ------- 
	 //    MREPL   0x0000  
	 // -b MBEFORE 0x0001 
	 // -a MAFTER  0x0002 
	 // -c MCREATE 0x0004 
	 // -C MCACHE  0x0010 

	if (!strcmp(cmd, "bind")) 
	  bind_cmnd();
	else

	//----- mount  [-abcC]  <srvname>  <mountpoint>  <spec>  -----
        if (!strcmp(cmd, "mount")) 
	  mount_cmnd();
        else

        //------- unmount  [<name>]  <mntpoint> -----
        if (!strcmp(cmd, "unmount"))    
	  unmount_cmnd();
        else
	 
	//---------- tar --------------------  
        if (!strcmp(cmd, "tar"))  
	  tar_cmnd();
        else

	//------ zip -f <zipfile>  <file> .... -------------  
        if (!strcmp(cmd, "zip"))  
	  zip_cmnd();
        else

	//------ unzip -f <zipfile>  --------------  
        if (!strcmp(cmd, "unzip"))  
	  unzip_cmnd();
        else

	//--------- process commands --------------------
#define  EBPCHK(s)  // l4printf_r("EBP-%s:%x \n", s, _EBP) 

#define  STKCHK(ss)  {register uint *stkptr  asm("%esp");   \
            register uint *frmptr  asm("%ebp");  \
            l4printf_r("%s<sp:%x,fp:%x,rtn:%x> ", ss, stkptr, frmptr, frmptr[1]); } 

        //------ rfork  <flag>  --------
        if (!strcmp(cmd, "rfork")) 
	  rfork_cmnd();
        else

        //------- fork  [<progname>]----------------
        if (!strcmp(cmd, "fork")) 
	  fork_cmnd();
        else 

        //------- exec  <progname>  ----------------
        if (!strcmp(cmd, "exec")) 
	  exec_cmnd();
        else

        //------- spawn  <progname> ----------------
        if (!strcmp(cmd, "spawn")) 
	  spawn_cmnd();
        else

        //------ pipe  ------------
        if (!strcmp(cmd, "pipe"))  
	  pipe_cmnd();
        else    
        
        //------ dup  <oldfd>  <newfd>   ------------
        if (!strcmp(cmd, "dup"))  
	  dup_cmnd();
        else    
        
        //------ exits  <msg>   ------------
        if (!strcmp(cmd, "exits"))  
	  exits_cmnd();
        else    
        
	//---------- pi -------------
        if (!strcmp(cmd, "pi"))
            pi(); 
        else

        //---------- credits ----------
	if (!strcmp(cmd, "credits"))
           credits(); 
        else  


        //------ putenv  <name>  <value> ------- 
        if (!strcmp(cmd, "putenv"))	 
	  putenv_cmnd();
        else

        //-------- getenv  <name>  ----------
        if (!strcmp(cmd, "getenv")) 
	  getenv_cmnd();
        else


	//------- exit --------------
        if (!strcmp(cmd, "exit"))
	{
	   //	   exit(0);
	}
	else

        //======= Setup commands =================================
	//----------- ramfs  |  dossrv  |  ext2srv ---------------
        if (!strcmp(cmd, "ramfs") || !strcmp(cmd, "dossrv") || !strcmp(cmd, "ext2srv")) {
	    post_nextproc(0);
	}
	else
	 
        //------- ipsetup  <IPaddress>  <GWaddress> ---------------	 
        if (!strcmp(cmd, "ipsetup"))  	
	  ipsetup_cmnd();
        else

        //------- usbsetup -------------------	 
        if (!strcmp(cmd, "usbsetup"))  	
	  {
	    rc = usbsetup();   EPR(rc);
	  }
        else

        //----- fdstart ------------------
        if (!strcmp(cmd, "fdstart"))	 
	  {
	    //l4printf("dossrv is running by default\n");
	    rc = fd_start();
	    // EPR(rc);
	  }
        else

        //----- xga ------------------
        if (!strcmp(cmd, "xga"))	 
	  {
	    print("xga\n");
	    xga_cmnd();
	    // EPR(rc);
	  }
        else

        //----- cga ------------------
        if (!strcmp(cmd, "cga"))	 
	  {
	    print("cga\n");
	    cga_cmnd();
	    // EPR(rc);
	  }
        else


        //======= Test commands ==================================
        //------ SL -----------------------
	if (!strcmp(cmd, "SL")) 
	  {
	    print("\n%s  <%d>\n", SL, strlen(SL));
	  }
	else
        
        //------ putsl -----------------------
	if (!strcmp(cmd, "putsl")) {
	    //	    dbg_time(2, 0);
	    rc = _put(1, SL, strlen(SL));  
	    //	    dbg_time(2, "qsh:put");  EPR(rc);
	}
	else
        
        //---- put <text> ----------------------
        if (!strcmp(cmd, "put"))  
	  put_cmnd();
	else

        //---- get  ----------------------
        if (!strcmp(cmd, "get")) 
	  get_cmnd();
	else
      
        //------ givesl -----------------------
	if (!strcmp(cmd, "givesl")) 
	  {
	    //	    dbg_time(2, 0);
	    rc = give(1, SL, strlen(SL));  
	    EPR(rc);
	    //	    dbg_time(2, "qsh:put");  
	}
	else
        
        //---- give <text> ----------------------
        if (!strcmp(cmd, "give"))  
	  give_cmnd();
	else

        //---- take  ----------------------
        if (!strcmp(cmd, "take")) 
	  take_cmnd();
	else

        //---- dump  <procnr> <startadrs> <size>  ------
	if (!strcmp(cmd, "dump"))  
	  dump_cmnd();
        else
       
        //---- maptest  <size> ----------------------
#define CHR(i)  ((i%128<'!')?('.'):((i%128>'}')?('~'):(i%128)))

        if (!strcmp(cmd, "maptest"))  
	  maptest_cmnd();
	else
      
        //---- malloctest  <size> -------------------------
        if (!strcmp(cmd, "malloctest"))  
	  malloctest_cmnd();
	else
      

	//========== debug =============
	if (!strcmp(cmd, "debug"))  
	  debug_cmnd();
        else  

        //------ d  <table>  <adrs> -------------
        if (!strcmp(cmd, "d"))   
	  d_cmnd();
        else  

        //------ getpid  ------------------
        if (!strcmp(cmd, "getpid"))   
	  getpid_cmnd();
        else  

        //------ ps  ------------------
        if (!strcmp(cmd, "ps"))   
	  {
	    _d("ps", 0); 
	  } 
        else  

	//-------- kd ------------------
        if (!strcmp(cmd, "kd"))
           L4_KDB_Enter("hit 'g' to continue"); 
	else

        //------- pgmap  <procnr> ----------------
        if (!strcmp(cmd, "pgmap")) 
	  pgmap_cmnd();
        else

        //------- code  <string> ----------------
        if (!strcmp(cmd, "code"))	 
	  code_cmnd();
        else

        //------- partition  <fdstr>  -----
        if (!strcmp(cmd, "partition"))        
	  partition_cmnd();
        else

	//-------- clk --------------
        if (!strcmp(cmd, "clk")) 
	  clk_cmnd();
        else

	//----- sleep ----------
	if (!strcmp(cmd, "sleep"))
	  sleep_cmnd();
	else

	//-------- delay --------------
        if (!strcmp(cmd, "delay")) 
	  delay_cmnd();
        else

	

	//---- HK 20100228 begin-----
	if (!strcmp(cmd, "a"))
           autoinput = "cd bin\n delay 300\n xga\n delay 300\n rio &\n";
        else
	if (!strcmp(cmd, "b"))
           autoinput = "riotst\n";
        else
	if (!strcmp(cmd, "win"))
           autoinput = "cd bin\n delay 300\n xga\n delay 300\n rio -w \n";
        else
	if (!strcmp(cmd, "c"))
           autoinput = "cd /mnt/wsys\n";
        else
	if (!strcmp(cmd, "e"))
         autoinput = "dog cons ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
        else
	if (!strcmp(cmd, "f"))
         autoinput = "cd bin\n delay 300\n xga\n delay 300\n rio -i /bin/riostart\n";
        else
	// HK 20100228 end

        //======= External commands ==========================
        //-------  <progname> [<argment>...] ----------------
#if 1
	  progname_cmnd(cmd, background);
#else
        {
	    int  i, pid;
	    argv[0] = cmd;

	    for (i = 1; i<16 ; i++)  {
	        argv[i] = strtok(NULL, " &\n");
		if (argv[i] == NULL)
		   break;
	    }
	    rc = spawn(cmd, argv);
	    if (rc<0) {
	        PR("spawn-err=<%d> \n", rc);
		continue;
	    }
	    else
	    if (background == 0) {
	        // PR("spawn-wait\n");
		L4_Yield(); // sleep(10000);
	        pid = waitpid();
	        // PR("spawn=<%d,%d> \n", rc, pid);
	    }else{
	        PR("spawn=<%d> \n", rc);
	    }
	}
#endif

	//PR("<>  "); //
	//       if (background == 0) wait(&status);
    } while (1);
    return  0;
}

//------------------------------------------
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

char* vlong2a(vlong val)
{
    static  char str[64];
    vlong2str(val, str);
    return  str;
}


//------------------------------
static void help( )
{
    PR("Supported commands:\n");
    PR(" cat <file>         displays contents of <file>\n");

    PR(" ls <path>          lists contents of working directory\n");
    PR(" ls <path>          shows detailed information about directory\n");
    PR(" pwd                prints path to working directory\n");
    PR(" cd <path>          changes working directory to <path>\n");
    PR(" echo <txt>         echo back <txt>\n");
    
    PR(" mkdir <dirname>    make a new directory\n");
    PR(" rm  <path>         Delete a file\n");

    PR(" open [-rwx..] <filename>    open a file\n");
    PR(" wrie <fd>  <text>           Write file\n");
    PR(" pwrie <fd> <pos>  <text>    Write file at pos\n");
    PR(" read <fd>  <text>           Read file\n");
    PR(" pread <fd> <pos>            Read file at pos\n");
    PR(" preadx <fd> <pos> [size]    Dump file at pos\n");
    
    PR(" bind [-abcC] <#new> <mntpoint> \n"); 
    PR(" mount [-abcC] <srvname> <mntpoint> <spec>  \n");
    PR(" umount <fs>        unmounts the filesystem\n");
    
    PR(" debug [on|off]     enables or disables debugging\n");
    PR(" help               displays the message you're reading\n");
    PR(" kd                 enters kernel debugger\n");

    PR(" d <tblname> <adrs>  dump table\n");
    PR("  <tblname> ::= chan|ch|namec|dev|mount|mhead|mnt|pgrp|proc  \n");
    PR(" d {up | ns}       dump the current proc or namespace\n");

    PR(" putsl              _write $SL \n");
    PR(" put <txt>          _write <text>\n");
    PR(" get                displays contents _written\n");
    
    PR("--- Followings are not yet implemented -----\n");
    PR(" copy <from> <to>   copy file \n");
    PR(" tar [cxt][vo][F][f] <tarfile> [<files>...] \n");
    PR(" <execfile> <arg>...  spawn a newtask  \n");
    PR(" newfile <fname> <txt>  create a new file\n");
    PR("\n");
    PR("Syscall constants:\n");
    PR("MORDER  0x0003  \n");
    PR("MREPL   0x0000  \n");
    PR("MBEFORE 0x0001  \n");
    PR("MAFTER  0x0002  \n");
    PR("MCREATE 0x0004  \n");
    PR("MCACHE  0x0010  \n");
    PR("OREAD   0  \n");
    PR("OWRITE  1  \n");
    PR("ORDWR   2  \n");
    PR("OEXEC   3  \n");
    PR("OTRUNC  16 \n");
    PR("OCEXEC  32 \n");
    PR("ORCLOSE 64 \n");
    PR("OEXCL   0x1000  \n");
    PR("AEXIST  0  \n");
    PR("AEXEC   1  \n");
    PR("AWRITE  2  \n");
    PR("AREAD   4  \n");

    PR("QTDIR     0x80  \n");
    PR("QTAPPEND  0x40  \n");
    PR("QTEXCL    0x20  \n");
    PR("QTMOUNT   0x10  \n");
    PR("QTAUTH    0x08  \n");
    PR("DMDIR     0x80000000  \n"); /* mode bit for directories */
    PR("DMAPPEND  0x40000000  \n"); 
    PR("DMEXCL    0x20000000  \n");
    PR("DMMOUNT   0x10000000   \n");
    PR("DMAUTH    0x08000000  \n");
    PR("DMTMP     0x04000000  \n");
    PR("DMREAD    0x4  \n");
    PR("DMWRITE   0x2  \n");
    PR("DMEXEC    0x1  \n");
    PR("  \n");
} 

//-----------------------------
static void credits(void)
{
    PR("To be or not to be, that is a question.\n");
    PR("To be to be, ten made to be. !!\n");
    PR("LP49 contributors:\n");
    PR("  L4-ka: Late Prof. Liedtke, Karlsruhe Univ.\n");
    PR("  Plan9: Bell Lab.\n");
    PR("  LP49/*            Maruyama  \n");
    PR("  LP49/9/ip         Sato  \n");
    PR("  LP49/9/vga, etc.  Kawai  \n");
    PR("  LP49/cmd/simple   Ueki  \n");
    PR("  ether driver      Hayashi  \n");
} 


//-------------------------------
static void pi(void)
{
    PR("3.1415926535 8979323846 2643383279 5028841971 6939937510 5820974944\n");
}

//------------------------------------
static int copy(char *fromfile, char * tofile)
{
    char buf[130];
    int fd,outfd;
    int cnt;
    int omode = 2;
    ulong perm = 0;

    if ((fd = open(fromfile, 0x0)) < 0) {
        PR("%s: open error\n", fromfile);
	return -1;
  }  

    if ((outfd = create(tofile, omode, perm)) < 0) {  //?
        PR("%s: creat error\n", tofile);
	close(fd);
	return -1;
    }  

    cnt = read(fd,  buf, 128);
    while ((cnt > 0) && (cnt <= 128)) {
        write(outfd, buf, cnt);
	cnt = read(fd, buf, 128);
    }

    close(fd);
    close(outfd);
    //  sync();
    return 1;
}


//-----------------------------------------
int fd_start( ) 
{
    int  fd, rc;
    l4printf_g("cd / \n");
    chdir("/");

#if 1
    l4printf_g("bind -a #f /dev \n");
    bind("#f", "/dev", MAFTER);
#endif

    L4_Yield();
    L4_Sleep(L4_TimePeriod (300000UL )); // 300 milli-sec

    l4printf_g("dossrv\n");
    post_nextproc(0);
    L4_Yield();
    L4_Sleep(L4_TimePeriod (200000UL )); // 200 milli-sec
    
    l4printf_g("mount -a /srv/dos /t /dev/fd0disk\n");
    fd = open("/srv/dos", ORDWR);
    if (fd < 0) {
        PR("cannot open /srv/dos\n");
	return -1;
    }
    rc = mount(fd, -1, "/t", MAFTER, "/dev/fd0disk");
    return  rc;
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

    L4_Yield();
    //    post_nextproc(0);
    L4_Yield();
    sleep(100); // L4_Sleep(L4_TimePeriod (200000UL )); // 200 milli-sec

    l4printf_g("mount -a /srv/9660 /t /dev/sdD0/data\n");
    fd = open("/srv/9660", ORDWR);
    if (fd < 0) {
        PR("cannot open /srv/9660\n");
	return -1;
    }

    rc = mount(fd, -1, "/t", MAFTER, "/dev/sdD0/data");
    return  rc;
}


//---------------------------------------------------
int ipsetup(char *IPadrs, char *GWadrs)  
{
	  int fd;
	  char subcmnd[64];

	  l4printf_b("IPsetup: IPadrs=%s GWadrs%s\n", IPadrs, GWadrs);

#if 0	  
	  l4printf_b("bind -a #I0 /net \n");
	  bind("#I", "/net", MAFTER);

	  l4printf_b("bind -a #l /net \n");
	  bind("#l", "/net", MAFTER);
#endif
	  l4printf_b("open -w /net/ipifc/clone\n");
	  fd = open("/net/ipifc/clone", ORDWR);

	  snprint(subcmnd, 64, "bind ether /net/ether0");
	  l4printf_b("write %d %s\n", fd, subcmnd);
	  _write(fd, subcmnd, strlen(subcmnd));

	  snprint(subcmnd, 64, "add %s 255.255.255.0", IPadrs);
	  l4printf_b("write %d %s\n", fd, subcmnd);
	  _write(fd, subcmnd, strlen(subcmnd));

	  /* Don't close "/net/ipifc/clone."			*/
	  /* It's the same implementation as ipconfig on Plan 9 */

	  l4printf_b("open -w /net/iproute\n");
	  fd = open("/net/iproute", 2);

	  snprint(subcmnd, 64, "add 0 0 %s", GWadrs);
	  l4printf_b("write %d %s\n", fd, subcmnd);
	  _write(fd, subcmnd, strlen(subcmnd));

	  l4printf_b("close %d\n", fd);
	  close(fd);
#if 1 
	  l4printf_b("open -w /net/ndb\n");
	  fd = open("/net/ndb", ORDWR);

	  snprint(subcmnd, 64, "ip=%s  ipmask=255.255.255.0 ipgw=%s\n"
			"\tsys=lp49\n"
			"\tdom= lp49.r.nii.ac.jp\n"
			"\tdns=%s\n", IPadrs, GWadrs, GWadrs);

	  l4printf_b("write %d %s\n", fd, subcmnd);
	  _write(fd, subcmnd, strlen(subcmnd));

	  l4printf_b("close %d\n", fd);
	  close(fd);
#endif
	  return  0;
}

//---------------------------------------------------
int usbsetup( )  
{
          int    fd, rc;
	  char   *scall;

	  l4printf("USB setup => bind -a #U /dev \n");
	  rc = bind("#U", "/dev", 2);
	  sleep(100); //L4_Sleep(L4_TimePeriod(100000UL)); 

	  scall = argv[0] = "/t/bin/usbd";
	  argv[1] = 0;
	  l4printf_b("next: spawn %s \n", scall);
	  rc = spawn(scall, argv);
	  sleep(1000); // L4_Sleep(L4_TimePeriod(1000000UL));  //1000ms
	  if (rc < 0) return -1;

	  scall = argv[0] = "/t/bin/usbsfs";
	  argv[1] = 0;
	  l4printf_b("next: spawn %s \n", scall);
	  rc = spawn(scall, argv);
	  sleep(1000); //L4_Sleep(L4_TimePeriod(1000000UL));  // 1000ms
	  if (rc < 0) return -1;
#if 1
	  scall = argv[0] = "/t/bin/dos2";
	  argv[1] = "-f";
	  argv[2] = "/ums/data:32";
	  argv[3] = "usbs";
	  argv[4] = 0;
	  l4printf_b("next: spawn %s -f /ums/data:32 usbs\n", scall);
	  rc = spawn(scall, argv);
	  sleep(1000); //L4_Sleep(L4_TimePeriod(1000000UL));  // 1000ms
	  if (rc < 0) return -1;

	  l4printf_b("next: mount -ac /srv/usbs /work\n");
	  fd = open("/srv/usbs", OREAD);
	  rc = mount(fd, -1, "/work", MAFTER|MCREATE, "");
#endif
	  return  rc;
}


//----------------------------------------------
typedef struct Dospart  Dospart;
struct Dospart
{
  uchar flag;             /* active flag */
  uchar shead;            /* starting head */
  uchar scs[2];           /* starting cylinder/sector */
  uchar type;             /* partition type */
  uchar ehead;            /* ending head */
  uchar ecs[2];           /* ending cylinder/sector */
  uchar start[4];         /* starting sector */
  uchar len[4];           /* length in sectors */
};

#define FAT12   0x01
#define FAT16   0x04
#define EXTEND  0x05
#define FATHUGE 0x06
#define FAT32   0x0b
#define FAT32X  0x0c
#define EXTHUGE 0x0f
#define DMDDO   0x54
#define PLAN9   0x39
#define LEXTEND 0x85

#define   GLONG(p)  (((p)[3]<<24) | ((p)[2]<<16) | ((p)[1]<<8) | (p)[0])


//%------------------------------------------------
uchar mbrbuf[512];
int nbuf;

int
isdos(int t)
{
  return t==FAT12 || t==FAT16 || t==FATHUGE || t==FAT32 || t==FAT32X;
}

int
isextend(int t)
{
  return t==EXTEND || t==EXTHUGE || t==LEXTEND;
}

int mbrpartition(int  fd)
{
  Dospart *dp;
  ulong taboffset, start, end;
  ulong firstxpart, nxtxpart;
  int havedos, i, nplan9;
  char name[10];
  int  rc;

  taboffset = 0;
  dp = (Dospart*)&mbrbuf[0x1BE];
  rc = pread(fd, mbrbuf, 0, 512); 

  if (rc < 0)
      return -1;

  /*
   * Read the partitions, first from the MBR and then
   * from successive extended partition tables.
   */
  nplan9 = 0;
  havedos = 0;
  firstxpart = 0;
  for(;;) {
    if( (rc = pread(fd, mbrbuf, 512, 0)) <  0)
      return -1;
    l4printf("mbr-check<%x %x> \n", mbrbuf[510], mbrbuf[511]);

    nxtxpart = 0;
    for(i=0; i<4; i++) {
      start = taboffset + GLONG(dp[i].start);
      end = start + GLONG(dp[i].len);

      l4printf("type=%x start=%d end=%d len=%d \n", 
	       dp[i].type, start, end, GLONG(dp[i].len));

      if(havedos==0 && isdos(dp[i].type)){
	havedos = 1;
      }

      /* nxtxpart is relative to firstxpart (or 0), not taboffset */
      if(isextend(dp[i].type)){
	nxtxpart = start-taboffset+firstxpart;
	l4printf("link %x\n", nxtxpart);
      }
    }

    if(1 || !nxtxpart)
      break;
    if(!firstxpart)
      firstxpart = nxtxpart;
    taboffset = nxtxpart;
  }
  return rc;
}

//-----   cc/dd ==> /aa/bb/cc/dd ----------
void  mkfullpath(char *in_path,  char * out_path)
{
  char ss[100] ;
  char *pos, *p2, *path;

  strcpy(ss, in_path);
  path = ss;

  if (debug) PR("curdir=[%s] ss=[%s]\n", pwdname, path);

  if (*path == '/') {
    strcpy(out_path, "/");
    path++;
  }
  else   strcpy(out_path, pwdname);

  while (*path)  {
      pos = path;
      while ((*path) && (*path != '/'))  path++;
      if (*path == '/')  *path++ = 0;

      if (!strcmp(pos, "..")) {
          p2 = out_path;
          while (*(++p2));
          p2--;

          if (p2 != out_path) // in case this already is root
            while (*(--p2) != '/');

          *(++p2) = 0;
      }
      else
        if (strcmp(pos, "."))  {
            strcat(out_path, pos);strcat(out_path, "/");
	}
  }
  if (debug) PR(" fullpath [%s] => [%s]\n", in_path, out_path);
}



#if 0 //================================================

/*  aa/bb/cc/dd  ==> aa/bb/cc/    dd */
void splitpath(char *composite,  char *path,  char *fname)
{
  char *lastslash,  *p2;
  
  lastslash = NULL;
  p2 = composite;
  while (*p2)  {
      if (*p2 == '/') lastslash = p2;
      p2++;
    }
  
  if (lastslash != NULL)   {
      strncpy(path, composite, 1 + (int)(lastslash - composite));
      strcpy(fname, lastslash+1);
    } 
  else {
      strcpy(path, "");
      strcpy(fname, composite);
    }  
}           

void changedir(char *abspath)
{
  char      *next = abspath;
  char      newpath[100];

  strcpy(newpath, "/");
  if (*next == '/') 
    next++;
  next = strtok(next, "/");

  while (next != NULL)
    {
      strcat(newpath, next);
      strcat(newpath, "/");
      next = strtok(NULL, "/");
    }  
  strcpy(pwdname, newpath);
}


extern  int     mount(int, int, char*, int, char*);
extern  int     unmount(char*, char*);

void mountcmd(int flag, char *srvname,  char *old, char * spec)
{
  int  fd;
  
  fd = open(srvname, ORDWR);
  if (fd < 0) {
    PR("ERR open \n");
    return ;
  }

  mount(fd, -1, old, flag, spec);
}  

//------------------------------
void umountcmd(char *old)
{
  //
}  

//--------------------------------------
void newfile(char *fname, char *txt)
{
  int  fd, m;
  int omode = 2;
  ulong perm = 0;

  if ((fd = create(fname, omode, perm)) < 0) {  //?
    PR("Create-ERR   \n"); 
    return;
  }
  m = write(fd, txt, strlen(txt));

  //  if (sync() < 0) L4_KDB_Enter("Sync-ERR");
  if (close(fd) < 0) L4_KDB_Enter("Close-ERR");
}

uvlong note_clock(uvlong * tt, char *msg)  // 1st time: msg==nil
{
    uvlong  last, now, delta;
    uint  usec, msec, sec;

    now = L4_SystemClock().raw;  // usec in uvlong
    if (tt == nil)   return  0L;

    if (msg) {
        last = *tt;
	delta = now - last;
	usec = (uint)(delta % 1000ULL);
	msec = (uint)((delta / 1000ULL) % 1000ULL);
	sec =  (uint)((delta / 1000000ULL) % 1000ULL);
	PR("%s<%d sec. %d msec. %d usec.> \n", msg, sec, msec, usec);
    }
    now = L4_SystemClock().raw; 
    *tt = now;
    return now;
}


void dbg_dump_mem(char *title, char *zbuf, int size)
{
  int  i, j, k;
  unsigned char c;
  char  buf[128];
  char  elem[32];

  if (size == 0) size = 256;

  for(i=0; i<size; i+=16) {
    buf[0] = 0;
    if (i==0)
      strcpy(buf, title);

    for (j=i; j<i+16; j+=4) {
      if (j%4 == 0) strcat(buf, " ");
      for(k=3; k>=0; k--) {
	c = zbuf[j+k];
	l4snprintf(elem, 32, "%.2x", c);
	strcat(buf, elem);
      }
    }

    strcat(buf, "  ");
    for (j=i; j<i+16; j++) {
      c = zbuf[j];
      if ( c >= ' ' && c < '~')
	l4snprintf(elem, 32, "%c", c);
      else
	l4snprintf(elem, 32, ".");
      strcat(buf, elem);
    }
    PR("%s\n", buf);
  }
}

static char pg[] =
{
  0x00,			/* Virsion and header length	*/
  0x00,			/* tos				*/
  0x00, 0x00,		/* packet length	  60	*/
  0x00, 0x00,		/* Identification		*/
  0x00, 0x00,		/* fragment information		*/
  0x00,			/* ttl				*/
  0x00,			/* protocol	  ICMP		*/
  0x00, 0x00,		/* header checksum		*/
  0x00, 0x00, 0x00, 0x00,	/* IP source		*/
  0x00, 0x00, 0x00, 0x00,	/* IP destination	*/
  0x08,			/* type		  Echo Request	*/
  0x00,			/* code			        */
  0x00, 0x00,		/* checksum	  (auto calc)	*/
  0x00, 0x00,		/* identifier			*/
  0x00, 0x00,		/* sequence number		*/
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
  'u', 'v', 'w', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
  'h', 'i', 
};

//---------------------------------------------------
void nstart( )  
{
	  int fd;
#define LADDR	"192.168.94.2"
#define GWADDR	"192.168.94.1"
#define DNSADDR "192.168.94.1"
#define SMASK	"255.255.255.0"
#define SNAME	"lp49"
#define DNAME	"x.nii.ac.jp"
	  char *str;

	  l4printf_b("begin NetStart \n");
	  
	  l4printf_b("bind -a #I /net \n");
	  bind("#I", "/net", 2);

	  l4printf_b("bind -a #l /net \n");
	  bind("#l", "/net", 2);

	  l4printf_b("cd /net\n");
	  chdir("/net");

	  l4printf_b("open -w ipifc/clone\n");
	  fd = open("ipifc/clone", 2);

	  str = "bind ether /net/ether0";
	  l4printf_b("write %d %s\n", fd, str);
	  _write(fd, str, strlen(str));

	  str = "add " LADDR " " SMASK;
	  l4printf_b("write %d %s\n", fd, str);
	  _write(fd, str, strlen(str));

	  /* Don't close "/net/ipifc/clone."			*/
	  /* It's the same implementation as ipconfig on Plan 9 */

	  l4printf_b("open -w iproute\n");
	  fd = open("iproute", 2);

	  str = "add 0 0 " GWADDR;
	  l4printf_b("write %d %s\n", fd, str);
	  _write(fd, str, strlen(str));

	  l4printf_b("close %d\n", fd);
	  close(fd);

	  l4printf_b("open -w ndb\n");
	  fd = open("ndb", 2);

	  str = "ip=" LADDR " ipmask=" SMASK " ipgw=" GWADDR "\n"
			"\tsys=" SNAME "\n"
			"\tdom=" SNAME "." DNAME "\n"
			"\tdns=" DNSADDR "\n";
	  l4printf_b("write %d %s\n", fd, str);
	  _write(fd, str, strlen(str));

	  l4printf_b("close %d\n", fd);
	  close(fd);

	  l4printf_b("end   NetStart \n");
}
#endif  //=======================================

