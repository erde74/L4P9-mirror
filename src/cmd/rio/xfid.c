//%%%%
#include  <l4all.h>      //%
#include  <lp49/l_actobj.h> //%

#include <u.h>
#include <libc.h>
#include <draw.h>
//#include <thread.h>	// HK 20100131
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>

#include "dat.h"
#include "fns.h"

#if 1 //%--------------------
#define SENDP(dest, mlabel, val) l_send0(dest, INF, l_putarg(nil, mlabel, "i1", val)) 
#define SEND(dest, mlabel, val)  l_send0(dest, INF, l_putarg(nil, mlabel, "i1", val)) 
#define  RECVP(varptr) {L4_ThreadId_t sender;   \
    l_recv0(&sender, INF, nil); l_getarg(nil, "i1", varptr); }
#endif //%-------------------


#define	MAXSNARF	100*1024

char Einuse[] =		"file in use";
char Edeleted[] =	"window deleted";
char Ebadreq[] =	"bad graphics request";
char Etooshort[] =	"buffer too small";
char Ebadtile[] =	"unknown tile";
char Eshort[] =		"short i/o request";
char Elong[] = 		"snarf buffer too long";
char Eunkid[] = 	"unknown id in attach";
char Ebadrect[] = 	"bad rectangle in attach";
char Ewindow[] = 	"cannot make window";
char Enowindow[] = 	"window has no image";
char Ebadmouse[] = 	"bad format on /dev/mouse";
char Ebadwrect[] = 	"rectangle outside screen";
char Ebadoffset[] = 	"window read not on scan line boundary";
extern char Eperm[];

static	Xfid	*xfidfree = nil;
static	Xfid	*xfid = nil;
//%  static	Channel	*cxfidalloc;	/* chan(Xfid*) */
//%  static	Channel	*cxfidfree;	/* chan(Xfid*) */
static  int     xfidlock = 0;

static	char	*tsnarf;
static	int	ntsnarf;

#if 1 //%-----------------------------------
void pr_freelist()
{
    Xfid  *xp;
    print(" FLIST{");
    for(xp=xfidfree; xp; xp=xp->free)print("%x ", xp);
    print("} ");
}

Xfid *
allocxfid()
{
	Xfid *x;
	L_thcb  *thcbp;
//DXX(0);//pr_freelist(); sleep(50);
	l_lock(&xfidlock);

	x = xfidfree;
	if(x)
	      xfidfree = x->free;
	else{
	      x = emalloc(sizeof(Xfid));
	      x->flushtag = -1;
	      x->next = xfid;
	      x->free = nil;
	      xfid = x;
	      thcbp = l_thread_create(xfidctl, 16384, x); //%
	      l_yield(thcbp);
	      l_wait_ready(thcbp); //%
	      sleep(100);
	}

	if(x->_ref.ref != 0){  //%
	      fprint(2, "%p incref %ld\n", x, x->_ref.ref);  //%
	      l_unlock(&xfidlock);
	      error("incref");
	}
	if(x->flushtag != -1) {
	      l_unlock(&xfidlock);
	      error("flushtag in allocate");
	}
	incref(&x->_ref);  //% x
	l_unlock(&xfidlock);
print(" Xalloced[%x] ", ((L_thcb*)x)->tid);sleep(50);
	return  x;
}

void freexfid(Xfid *x)
{
  //print(" FREEX[%x] ", x); sleep(100);
      l_lock(&xfidlock);
      if(x->_ref.ref != 0){  //%
          fprint(2, "%p decref %ld\n", x, x->_ref.ref); //%
	  l_unlock(&xfidlock);
	  error("decref");
      }
      if(x->flushtag != -1) {
	  l_unlock(&xfidlock);
	  error("flushtag in free");
      }
      x->free = xfidfree;
      xfidfree = x;
//pr_freelist();sleep(50);
	  l_unlock(&xfidlock);
}

#else  //% plan9 original--------------------------
void xfidallocthread(void* _z)
{
	Xfid *x;
	enum { Alloc, Free, N };
	static Alt alts[N+1];

	alts[Alloc].c = cxfidalloc; //%
	alts[Alloc].v = nil;
	alts[Alloc].op = CHANRCV;
	alts[Free].c = cxfidfree;
	alts[Free].v = &x;
	alts[Free].op = CHANRCV;
	alts[N].op = CHANEND;
	for(;;){
	    switch(alt(alts)){ //%
		case Alloc:
			x = xfidfree;
			if(x)
				xfidfree = x->free;
			else{
				x = emalloc(sizeof(Xfid));
				//%  x->c = chancreate(sizeof(void(*)(Xfid*)), 0); //%
				//%  x->flushc = chancreate(sizeof(int), 0);	
				x->flushtag = -1;
				x->next = xfid;
				xfid = x;
				threadcreate(xfidctl, x, 16384); //%
			}
			if(x->_ref.ref != 0){  //%
			        fprint(2, "%p incref %ld\n", x, x->_ref.ref);  //%
				error("incref");
			}
			if(x->flushtag != -1)
				error("flushtag in allocate");
			incref(&x->_ref);  //% x
			//%	sendp(cxfidalloc, x); //%
			break;
		case Free:
		        if(x->_ref.ref != 0){  //%
			        fprint(2, "%p decref %ld\n", x, x->_ref.ref); //%
				error("decref");
			}
			if(x->flushtag != -1)
				error("flushtag in free");
			x->free = xfidfree;
			xfidfree = x;
			break;
		}
	}
}

Channel*
xfidinit(void)
{
        cxfidalloc = chancreate(sizeof(Xfid*), 0); //%
	cxfidfree = chancreate(sizeof(Xfid*), 0);  //%
	threadcreate(xfidallocthread, nil, STACK); //%
	return cxfidalloc;
}
#endif //%---------------------------------------------------------


/*****************************************************************
 *  Xfid active object / to process concurrent request messges   *
 *       Data:     Xfid type                                     *
 *       Action:   xfidctl()                                     *
 *****************************************************************/
void
xfidctl(void *arg)  //%  arg :  Self Xfid data structure
{
	Xfid *x;
	void (*f)(Xfid*);
	char buf[64];

	x = arg;
	snprint(buf, sizeof buf, "xfid.%p", x);
	l_thread_setname(buf); //%
	l_thcb_ls((L_thcb*)x); //%
	l_post_ready(0); //%

	for(;;){
	        // print("  XREC[%x] ", L4_Myself());
	        // RECVP(&f); //%  f = recvp(x->c);  //%
                L4_ThreadId_t  from; 
                l_recv0(&from, INF, nil); 
                l_getarg(nil, "i1", &f); 
	         
		(*f)(x);
		if(decref(&x->_ref) == 0) //% x
		        freexfid(x);  //% sendp(cxfidfree, x); //%
	}
}

void
xfidflush(Xfid *x)
{
	Fcall t;
	Xfid *xf;

	for(xf=xfid; xf; xf=xf->next)
	        if(xf->flushtag == x->_fcall.oldtag){  //%
			xf->flushtag = -1;
			xf->flushing = TRUE;
			incref(&xf->_ref); //% to hold data structures up at tail of synchronization 
			if(xf->_ref.ref == 1) //%
				error("ref 1 in flush");
			if(canqlock(&xf->active)){
				qunlock(&xf->active);
				SENDP((L_thcb*)xf, CWflush, 0);  //%sendul(xf->flushc, 0);
			}else{
				qlock(&xf->active);	/* wait for him to finish */
				qunlock(&xf->active);
			}
			xf->flushing = FALSE;
			if(decref(&xf->_ref) == 0)  //%
			      freexfid(xf);  //% sendp(cxfidfree, xf); //%
			break;
		}
	filsysrespond(x->fs, x, &t, nil);
}

void
xfidattach(Xfid *x)
{
	Fcall t;
	int id, hideit, scrollit;
	Window *w;
	char *err, *n, *dir, errbuf[ERRMAX];
	int pid, newlymade;
	Rectangle r;
	Image *i;

	t.qid = x->f->qid;
	qlock(&all);
	w = nil;
	err = Eunkid;
	newlymade = FALSE;
	hideit = 0;

	if(x->_fcall.aname[0] == 'N'){	//% N pid, 100,100,200,200 - old syntax 
	        n = x->_fcall.aname+1;  //%
		pid = strtoul(n, &n, 0);
		if(*n == ',')
			n++;
		r.min.x = strtoul(n, &n, 0);
		if(*n == ',')
			n++;
		r.min.y = strtoul(n, &n, 0);
		if(*n == ',')
			n++;
		r.max.x = strtoul(n, &n, 0);
		if(*n == ',')
			n++;
		r.max.y = strtoul(n, &n, 0);
  Allocate:
		if(!goodrect(r))
			err = Ebadrect;
		else{
			if(hideit)
				i = allocimage(display, r, screen->chan, 0, DWhite);
			else
				i = allocwindow(wscreen, r, Refbackup, DWhite);
			if(i){
				border(i, r, Selborder, display->black, ZP);
				if(pid == 0)
					pid = -1; /* make sure we don't pop a shell! - UGH */
				w = new(i, hideit, scrolling, pid, nil, nil, nil);
				flushimage(display, 1);
				newlymade = TRUE;
			}else
				err = Ewindow;
		}
	}
	else if(strncmp(x->_fcall.aname, "new", 3) == 0){  /* new -dx -dy - new syntax, as in wctl */
		pid = 0;
		if(parsewctl(nil, ZR, &r, &pid, nil, &hideit, &scrollit, &dir, x->_fcall.aname, errbuf) < 0) //%
			err = errbuf;
		else
			goto Allocate;
	}
	else{
	        id = atoi(x->_fcall.aname); //%
		w = wlookid(id);
	}

//print("\n xfidattach<x:%x  w:%x>\n", x, w);  
	x->f->w = w;
	if(w == nil){
		qunlock(&all);
		x->f->busy = FALSE;
		filsysrespond(x->fs, x, &t, err);
		return;
	}
	if(!newlymade)	/* counteract dec() in winshell() */
	        incref(&w->_ref);  //% w
	qunlock(&all);
	filsysrespond(x->fs, x, &t, nil);
}

void
xfidopen(Xfid *x)
{
	Fcall t;
	Window *w;

	w = x->f->w;
	if(w->deleted){
		filsysrespond(x->fs, x, &t, Edeleted);
		return;
	}
	switch(FILE(x->f->qid)){
	case Qconsctl:
		if(w->ctlopen){
			filsysrespond(x->fs, x, &t, Einuse);
			return;
		}
		w->ctlopen = TRUE;
		break;
	case Qkbdin:
		if(w !=  wkeyboard){
			filsysrespond(x->fs, x, &t, Eperm);
			return;
		}
		break;
	case Qmouse:
		if(w->mouseopen){
			filsysrespond(x->fs, x, &t, Einuse);
			return;
		}
		/*
		 * Reshaped: there's a race if the appl. opens the
		 * window, is resized, and then opens the mouse,
		 * but that's rare.  The alternative is to generate
		 * a resized event every time a new program starts
		 * up in a window that has been resized since the
		 * dawn of time.  We choose the lesser evil.
		 */
		w->resized = FALSE;
		w->mouseopen = TRUE;
		break;
	case Qsnarf:
	        if(x->_fcall.mode==ORDWR || x->_fcall.mode==OWRITE){  //%
			if(tsnarf)
				free(tsnarf);	/* collision, but OK */
			ntsnarf = 0;
			tsnarf = malloc(1);
		}
		break;
	case Qwctl:
	        if(x->_fcall.mode==OREAD || x->_fcall.mode==ORDWR){  //%
			/*
			 * It would be much nicer to implement fan-out for wctl reads,
			 * so multiple people can see the resizings, but rio just isn't
			 * structured for that.  It's structured for /dev/cons, which gives
			 * alternate data to alternate readers.  So to keep things sane for
			 * wctl, we compromise and give an error if two people try to
			 * open it.  Apologies.
			 */
			if(w->wctlopen){
				filsysrespond(x->fs, x, &t, Einuse);
				return;
			}
			w->wctlopen = TRUE;
			w->wctlready = 1;
			wsendctlmesg(w, Wakeup, ZR, nil);
		}
		break;
	}
	t.qid = x->f->qid;
	t.iounit = messagesize-IOHDRSZ;
	x->f->open = TRUE;
	x->f->mode = x->_fcall.mode;  //%
	filsysrespond(x->fs, x, &t, nil);
}

void
xfidclose(Xfid *x)
{
	Fcall t;
	Window *w;
	int nb, nulls;

	w = x->f->w;
	switch(FILE(x->f->qid)){
	case Qconsctl:
		if(w->rawing){
			w->rawing = FALSE;
			wsendctlmesg(w, Rawoff, ZR, nil);
		}
		if(w->holding){
			w->holding = FALSE;
			wsendctlmesg(w, Holdoff, ZR, nil);
		}
		w->ctlopen = FALSE;
		break;
	case Qcursor:
		w->cursorp = nil;
		wsetcursor(w, FALSE);
		break;
	case Qmouse:
		w->resized = FALSE;
		w->mouseopen = FALSE;
		if(w->i != nil)
			wsendctlmesg(w, Refresh, w->i->r, nil);
		break;
	/* odd behavior but really ok: replace snarf buffer when /dev/snarf is closed */
	case Qsnarf:
		if(x->f->mode==ORDWR || x->f->mode==OWRITE){
			snarf = runerealloc(snarf, ntsnarf+1);
			cvttorunes(tsnarf, ntsnarf, snarf, &nb, &nsnarf, &nulls);
			free(tsnarf);
			tsnarf = nil;
			ntsnarf = 0;
		}
		break;
	case Qwctl:
		if(x->f->mode==OREAD || x->f->mode==ORDWR)
			w->wctlopen = FALSE;
		break;
	}
	wclose(w);
	filsysrespond(x->fs, x, &t, nil);
}

void
xfidwrite(Xfid *x)
{
	Fcall fc;
	int c, cnt, qid, nb, off, nr;
	char buf[256], *p;
	Point pt;
	Window *w;
	Rune *r;
	Stringpair pair;
	//%	Conswritemesg cwm;
	//%	enum { CWdata, CWflush, NCW };
	//%	Alt alts[NCW+1];
	L_msgtag   msgtag;   //%
	int        mlabel;   //%
	int        len1;
	//DXX(0);
	w = x->f->w;
	if(w->deleted){
		filsysrespond(x->fs, x, &fc, Edeleted);
		return;
	}
	qid = FILE(x->f->qid);
	cnt = x->_fcall.count;  //%
	off = x->_fcall.offset;  //%
	x->_fcall.data[cnt] = 0; //%

	switch(qid){
	case Qcons: //%  THIS ==== WCwrite =====> Window actobj (winctl) 
#if 1 //%--------------------------------------
		nr = x->f->nrpart;
		if(nr > 0){
		        memmove(x->_fcall.data+nr, x->_fcall.data, cnt); 
			//%  there's room: see malloc in filsysproc
		        memmove(x->_fcall.data, x->f->rpart, nr);  //%
			cnt += nr;
			x->f->nrpart = 0;
		}
		r = runemalloc(cnt);
		cvttorunes(x->_fcall.data, cnt-UTFmax, r, &nb, &nr, nil); //%
		/* approach end of buffer */
		while(fullrune(x->_fcall.data+nb, cnt-nb)){  //%
			c = nb;
			nb += chartorune(&r[nr], x->_fcall.data+c);  //%
			if(r[nr])
				nr++;
		}
		if(nb < cnt){
		        memmove(x->f->rpart, x->_fcall.data+nb, cnt-nb); //%
			x->f->nrpart = cnt-nb;
		}
		x->flushtag = x->_fcall.tag;  //%


		/* received data */
		x->flushtag = -1;

		qlock(&x->active);
		pair.s = r;
		pair.ns = nr;

		l_putarg(nil, WCwrite, "i2", r, nr);
		msgtag = l_send0((L_thcb*)w, INF, nil); //% send(cwm.cw, &pair);
		if (L4_IpcFailed(msgtag)) {
		  print("\n ?? xfidwrite:send2window:%d \n", L4_ErrorCode());
		}
		fc.count = x->_fcall.count;  //%
		filsysrespond(x->fs, x, &fc, nil);
		qunlock(&x->active);
		return;
#else //% plan9 --------------------------------------
                nr = x->f->nrpart;
                if(nr > 0){
		    memmove(x->data+nr, x->data, cnt); 
		    memmove(x->data, x->f->rpart, nr);
		    cnt += nr;
		    x->f->nrpart = 0;
                }
                r = runemalloc(cnt);
                cvttorunes(x->data, cnt-UTFmax, r, &nb, &nr, nil);
                /* approach end of buffer */
                while(fullrune(x->data+nb, cnt-nb)){
		    c = nb;
		    nb += chartorune(&r[nr], x->data+c);
		    if(r[nr])    nr++;
                }
                if(nb < cnt){
		    memmove(x->f->rpart, x->data+nb, cnt-nb);
		    x->f->nrpart = cnt-nb;
                }
                x->flushtag = x->tag;

     alts[CWdata].c = w->conswrite;  alts[CWdata].v = &cwm; alts[CWdata].op = CHANRCV;
     alts[CWflush].c = x->flushc; alts[CWflush].v = nil; alts[CWflush].op = CHANRCV;
     alts[NCW].op = CHANEND;
                switch(alt(alts)){
                case CWdata:
		    break;
                case CWflush:
		    filsyscancel(x);
		    return;
                }
                /* received data */
                x->flushtag = -1;
                if(x->flushing){
		    recv(x->flushc, nil);  /* wake up flushing xfid */
		    pair.s = runemalloc(1);
		    pair.ns = 0;
		    send(cwm.cw, &pair); /* wake up window with empty data */
		    filsyscancel(x);
		    return;
                }
                qlock(&x->active);
                pair.s = r;
                pair.ns = nr;
                send(cwm.cw, &pair);
                fc.count = x->count;
                filsysrespond(x->fs, x, &fc, nil);
                qunlock(&x->active);
                return;
#endif //%----------------------------------------

	case Qconsctl:
	  if(strncmp(x->_fcall.data, "holdon", 6)==0){  //%
			if(w->holding++ == 0)
				wsendctlmesg(w, Holdon, ZR, nil);
			break;
		}
	  if(strncmp(x->_fcall.data, "holdoff", 7)==0 && w->holding){  //%
			if(--w->holding == FALSE)
				wsendctlmesg(w, Holdoff, ZR, nil);
			break;
		}
	  if(strncmp(x->_fcall.data, "rawon", 5)==0){  //%
			if(w->holding){
				w->holding = FALSE;
				wsendctlmesg(w, Holdoff, ZR, nil);
			}
			if(w->rawing++ == 0)
				wsendctlmesg(w, Rawon, ZR, nil);
			break;
		}
	  if(strncmp(x->_fcall.data, "rawoff", 6)==0 && w->rawing){ //%
			if(--w->rawing == 0)
				wsendctlmesg(w, Rawoff, ZR, nil);
			break;
		}
		filsysrespond(x->fs, x, &fc, "unknown control message");
		return;

	case Qcursor:
		if(cnt < 2*4+2*2*16)
			w->cursorp = nil;
		else{
		        w->cursor.offset.x = BGLONG(x->_fcall.data+0*4); //%
			w->cursor.offset.y = BGLONG(x->_fcall.data+1*4); //%
			memmove(w->cursor.clr, x->_fcall.data+2*4, 2*2*16); //%
			w->cursorp = &w->cursor;
		}
		wsetcursor(w, !sweeping);
		break;

	case Qlabel:
		if(off != 0){
			filsysrespond(x->fs, x, &fc, "non-zero offset writing label");
			return;
		}
		free(w->label);
		w->label = emalloc(cnt+1);
		memmove(w->label, x->_fcall.data, cnt); //%
		w->label[cnt] = 0;
		break;

	case Qmouse:
		if(w!=input || Dx(w->screenr)<=0)
			break;
		if(x->_fcall.data[0] != 'm'){ //%
			filsysrespond(x->fs, x, &fc, Ebadmouse);
			return;
		}
		p = nil;
		pt.x = strtoul(x->_fcall.data+1, &p, 0); //%
		if(p == nil){
			filsysrespond(x->fs, x, &fc, Eshort);
			return;
		}
		pt.y = strtoul(p, nil, 0);
		if(w==input && wpointto(mouse->xy)==w)
			wsendctlmesg(w, Movemouse, Rpt(pt, pt), nil);
		break;

	case Qsnarf:
		/* always append only */
		if(ntsnarf > MAXSNARF){	/* avoid thrashing when people cut huge text */
			filsysrespond(x->fs, x, &fc, Elong);
			return;
		}
		tsnarf = erealloc(tsnarf, ntsnarf+cnt+1);	/* room for NUL */
		memmove(tsnarf+ntsnarf, x->_fcall.data, cnt);  //%
		ntsnarf += cnt;
		snarfversion++;
		break;

	case Qwdir:
		if(cnt == 0)
			break;
		if(x->_fcall.data[cnt-1] == '\n'){  //%
			if(cnt == 1)
				break;
			x->_fcall.data[cnt-1] = '\0';  //%
		}
		/* assume data comes in a single write */
		/* Problem: programs like dossrv, ftp produce illegal UTF;
		 * we must cope by converting it first.
		 */
		snprint(buf, sizeof buf, "%.*s", cnt, x->_fcall.data); //%
		if(buf[0] == '/'){
			free(w->dir);
			w->dir = estrdup(buf);
		}else{
			p = emalloc(strlen(w->dir) + 1 + strlen(buf) + 1);
			sprint(p, "%s/%s", w->dir, buf);
			free(w->dir);
			w->dir = cleanname(p);
		}
		break;

	case Qkbdin:
	  keyboardsend(x->_fcall.data, cnt); //%
		break;

	case Qwctl:
		if(writewctl(x, buf) < 0){
			filsysrespond(x->fs, x, &fc, buf);
			return;
		}
		flushimage(display, 1);
		break;

	default:
		fprint(2, buf, "unknown qid %d in write\n", qid);
		sprint(buf, "unknown qid in write");
		filsysrespond(x->fs, x, &fc, buf);
		return;
	}
	fc.count = cnt;
	filsysrespond(x->fs, x, &fc, nil);
}

int
readwindow(Image *i, char *t, Rectangle r, int offset, int n)
{
	int ww, y;

	offset -= 5*12;
	ww = bytesperline(r, screen->depth);
	r.min.y += offset/ww;
	if(r.min.y >= r.max.y)
		return 0;
	y = r.min.y + n/ww;
	if(y < r.max.y)
		r.max.y = y;
	if(r.max.y <= r.min.y)
		return 0;
	return unloadimage(i, r, (uchar*)t, n);
}

void
xfidread(Xfid *x)
{
	Fcall fc;
	int n, off, cnt, c;
	uint qid;
	char buf[128], *t;
	char cbuf[30];
	Window *w;
	Mouse ms;
	Rectangle r;
	Image *i;
	int  len1;
	L_msgtag  msgtag;
	Stringpair pair;
	L4_ThreadId_t  tid;
//% Channel *c1, *c2;	/* chan (tuple(char*, int)) */
//% Consreadmesg crm;	Mousereadmesg mrm; Consreadmesg cwrm;
//% enum { CRdata, CRflush, NCR }; enum { MRdata, MRflush, NMR };
//% enum { WCRdata, WCRflush, NWCR }; Alt alts[NCR+1];

	w = x->f->w;
	if(w->deleted){
		filsysrespond(x->fs, x, &fc, Edeleted);
		return;
	}
	qid = FILE(x->f->qid);
	off = x->_fcall.offset; //%
	cnt = x->_fcall.count;  //%

	switch(qid){
	case Qcons:  //%  THIS ==== WCread ===> Window (winctl) actobj 
#if 1 //%--------------------------------------------------
 DXX(600);
	        x->flushtag = x->_fcall.tag; //%

		t = malloc(cnt+UTFmax+1);	/* room to unpack partial rune plus */
		pair.s = t;
		pair.ns = cnt;

		l_putarg(nil, WCread, "i2", t, cnt);
		/* Cf. msgtag = l_call0((L_thcb*)w, INF, INF, nil);  */
		msgtag = l_send0((L_thcb*)w, INF, nil); 
		if (L4_IpcFailed(msgtag)) print(" ?xfidread:send0:TBP \n");

		//? qlock(&x->active);
		msgtag = l_recv0(nil, INF, nil);  // CRdata
		if (L4_IpcFailed(msgtag)){
		    print(" ?xfidread:recv0:%x:TBP  \n", msgtag);
		    //memcpy(t, "pwd\n", 5); fc.data = t; fc.count = 5;
		    //filsysrespond(x->fs, x, &fc, nil); free(t);
		    //? qunlock(&x->active);
		    //break;
		}
		x->flushtag = -1;

		switch(MLABEL(msgtag)){
		case CRflush:
		    msgtag = l_recv0(nil, INF, nil); // CRdata -- discard 
		    free(t);
		    filsyscancel(x);
		    return;

		case CRdata:
		    l_getarg(nil, "i2", &fc.data, &fc.count);
sleep(100);
//DXX(605);//print("\n #<%d># ", MLABEL(msgtag));
		    filsysrespond(x->fs, x, &fc, nil);
		    free(t);
		    break;
		default: 
		  ;// error
		}
		//? qunlock(&x->active);
		break;
#else //%------------------------------------------------------
	    //% [Mate] wincth thread  <> Myself
	    //% (w->consread) ! (crm) <> (w->consread)?(crm)
	    //%  (crm.c1) ? (pair)    <> (crm.c1) ! (pair)
	    //%  (crm.c2) ! (pair)    <> (crm.c2) ? (pair)
	        x->flushtag = x->_fcall.tag; //%
    alts[CRdata].c = w->consread; alts[CRdata].v = &crm; alts[CRdata].op = CHANRCV; 
    alts[CRflush].c = x->flushc; alts[CRflush].v = nil;	alts[CRflush].op = CHANRCV;
    alts[NMR].op = CHANEND;

                switch(alt(alts)){  
		case CRdata:
			break;
		case CRflush:
			filsyscancel(x);
			return;
		}
		/* received data */
		x->flushtag = -1;
		c1 = crm.c1;	c2 = crm.c2;
		t = malloc(cnt+UTFmax+1);	/* room to unpack partial rune plus */
		pair.s = t;
		pair.ns = cnt;
		send(c1, &pair);
		if(x->flushing){
		        recv(x->flushc, nil);	/* wake up flushing xfid */
			recv(c2, nil);		/* wake up window and toss data */
			free(t);
			filsyscancel(x);
			return;
		}
		qlock(&x->active);
		recv(c2, &pair); //%
		fc.data = pair.s;
		fc.count = pair.ns;
		filsysrespond(x->fs, x, &fc, nil);
		free(t);
		qunlock(&x->active);
		break;
#endif //%--------------------------------------------------

	case Qlabel:
		n = strlen(w->label);
		if(off > n)
			off = n;
		if(off+cnt > n)
			cnt = n-off;
		fc.data = w->label+off;
		fc.count = cnt;
		filsysrespond(x->fs, x, &fc, nil);
		break;

	case Qmouse:  //% THIS ==== WMouseread ===> Window (winctl) actobj
#if 1 //%--------------------------------------------------
	        x->flushtag = x->_fcall.tag; //%

		len1 = sizeof(ms);
		l_putarg(nil, WMouseread, 0);

		msgtag = l_send0((L_thcb*)w, INF, nil); 
		if (L4_IpcFailed(msgtag)) print(" ?xfidread:send0:TBP \n");

		//? qlock(&x->active);
		msgtag = l_recv0(nil, INF, nil);  // CRdata
		if (L4_IpcFailed(msgtag)){
		    print(" ?xfidread:recv0:%x:TBP  \n", msgtag);
		    //...............
		}
		x->flushtag = -1;

		switch(MLABEL(msgtag)){
		case CRflush:
		    msgtag = l_recv0(nil, INF, nil); // CRdata -- discard 
		    //? qunlock(&x->active);
		    free(t);
		    filsyscancel(x);
		    return;

		case CRdata:
		    l_getarg(nil, "i2", &fc.data, &fc.count);
sleep(100);
                    c = 'm';
		    if(w->resized)
		        c = 'r';
		    n = sprint(buf, "%c%11d %11d %11d %11ld ", c, 
			       ms.xy.x, ms.xy.y, ms.buttons, ms.msec);
		    w->resized = 0;
		    fc.data = buf;
		    fc.count = min(n, cnt);
		    filsysrespond(x->fs, x, &fc, nil);
		    qunlock(&x->active);
		    break;

		default: 
		  ;// error
		}
		//? qunlock(&x->active);
		break;

#else //% plan9-----------------------------------------------------------
	    //% [Mate] winctl thread   <> Myself
	    //% (w->mouseread) ! (mrm) <> (w->mouseread) ? (mrm)
	    //% .....                      ....
	    //% mrm.cm ! (m._mouse);   <>  (mrm.cm) ? (ms) 

	        x->flushtag = x->_fcall.tag;  
    alts[MRdata].c = w->mouseread; alts[MRdata].v = &mrm; alts[MRdata].op = CHANRCV;
    alts[MRflush].c = x->flushc; alts[MRflush].v = nil; alts[MRflush].op = CHANRCV;
    alts[NMR].op = CHANEND;

		switch(alt(alts)){ 
		case MRdata:
			break;
		case MRflush:
			filsyscancel(x);
			return;
		}
		/* received data */
		x->flushtag = -1;
		if(x->flushing){
		        recv(x->flushc, nil); /* wake up flushing xfid */
			recv(mrm.cm, nil); /* wake up window and toss data */
			filsyscancel(x);
			return;
		}
		qlock(&x->active);
		recv(mrm.cm, &ms);
		c = 'm';
		if(w->resized)
			c = 'r';
		n = sprint(buf, "%c%11d %11d %11d %11ld ", c, 
			   ms.xy.x, ms.xy.y, ms.buttons, ms.msec);
		w->resized = 0;
		fc.data = buf;
		fc.count = min(n, cnt);
		filsysrespond(x->fs, x, &fc, nil);
		qunlock(&x->active);
		break;
#endif //%---------------------------------------------------------------

	case Qcursor:
		filsysrespond(x->fs, x, &fc, "cursor read not implemented");
		break;

	/* The algorithm for snarf and text is expensive but easy and rarely used */
	case Qsnarf:
		getsnarf();
		if(nsnarf)
			t = runetobyte(snarf, nsnarf, &n);
		else {
			t = nil;
			n = 0;
		}
		goto Text;

	case Qtext:
		t = wcontents(w, &n);
		goto Text;

	Text:
		if(off > n){
			off = n;
			cnt = 0;
		}
		if(off+cnt > n)
			cnt = n-off;
		fc.data = t+off;
		fc.count = cnt;
		filsysrespond(x->fs, x, &fc, nil);
		free(t);
		break;

	case Qwdir:
		t = estrdup(w->dir);
		n = strlen(t);
		goto Text;

	case Qwinid:
		n = sprint(buf, "%11d ", w->id);
		t = estrdup(buf);
		goto Text;


	case Qwinname:
		n = strlen(w->name);
		if(n == 0){
			filsysrespond(x->fs, x, &fc, "window has no name");
			break;
		}
		t = estrdup(w->name);
		goto Text;

	case Qwindow:
		i = w->i;
		if(i == nil || Dx(w->screenr)<=0){
			filsysrespond(x->fs, x, &fc, Enowindow);
			return;
		}
		r = w->screenr;
		goto caseImage;

	case Qscreen:
		i = display->image;
		if(i == nil){
			filsysrespond(x->fs, x, &fc, "no top-level screen");
			break;
		}
		r = i->r;
		/* fall through */

	caseImage:
		if(off < 5*12){
			n = sprint(buf, "%11s %11d %11d %11d %11d ",
				chantostr(cbuf, screen->chan),
				i->r.min.x, i->r.min.y, i->r.max.x, i->r.max.y);
			t = estrdup(buf);
			goto Text;
		}
		t = malloc(cnt);
		fc.data = t;
		n = readwindow(i, t, r, off, cnt);	/* careful; fc.count is unsigned */
		if(n < 0){
			buf[0] = 0;
			errstr(buf, sizeof buf);
			filsysrespond(x->fs, x, &fc, buf);
		}else{
			fc.count = n;
			filsysrespond(x->fs, x, &fc, nil);
		}
		free(t);
		return;

	case Qwctl:  //% THIS ===  WWread  ===> Window (winctl) actobj
           	/* read returns rectangle, hangs if not resized */

#if 1 //%---------------------------------------------

		if(cnt < 4*12){
			filsysrespond(x->fs, x, &fc, Etooshort);
			break;
		}
		x->flushtag = x->_fcall.tag;  //%

		t = malloc(cnt+1);	/* be sure to have room for NUL */
		//% pair.s = t;	 pair.ns = cnt+1;  send(c1, &pair);  //%

		l_putarg(nil, WWread, "i2", t, cnt+1);

		msgtag = l_send0((L_thcb*)w, INF, nil); 
                if (L4_IpcFailed(msgtag)) print(" ?xfidread:send0:TBP \n");

		//? qlock(&x->active);
                msgtag = l_recv0(nil, INF, nil);  // CRdata
                if (L4_IpcFailed(msgtag)){
		  print(" ?xfidread:recv0:%x:TBP  \n", msgtag);
		  //...............
                }
                x->flushtag = -1;

                switch(MLABEL(msgtag)){
                case WCRflush:
		    msgtag = l_recv0(nil, INF, nil); // CRdata -- discard 
		    //? qunlock(&x->active);
		    free(t);
		    filsyscancel(x);
		    return;

                case WCRdata:
		    l_getarg(nil, "i2", &fc.data, &fc.count);
		    if (fc.count > cnt) 
		        fc.count = cnt;
		    filsysrespond(x->fs, x, &fc, nil);
		    free(t);
		    break;

		default: // Never come here
		  ;
		}
		// qunlock(&x->active);
		break;

#else //% plan9 -----------------------------------------------
                //% [Mate] winctl thread   <> Myself
	        //% (w->wctlread) ! (cwrm) <> (w->wctlread) ? (cwrm)
                //% (cwrm.c1) ? (pair)     <> (cwrm.c1) ! (pair)  
                //% (cwrm.c2) ! (pair)     <> (cwrm.c2) ? (pair)

		if(cnt < 4*12){
			filsysrespond(x->fs, x, &fc, Etooshort);
			break;
		}
		x->flushtag = x->_fcall.tag;  
    alts[WCRdata].c = w->wctlread; alts[WCRdata].v = &cwrm; alts[WCRdata].op = CHANRCV;
    alts[WCRflush].c = x->flushc; alts[WCRflush].v = nil; alts[WCRflush].op = CHANRCV;
    alts[NMR].op = CHANEND;

		switch(alt(alts)){ 
		case WCRdata:
			break;
		case WCRflush:
			filsyscancel(x);
			return;
		}

		/* received data */
		x->flushtag = -1;
		c1 = cwrm.c1;
		c2 = cwrm.c2;
		t = malloc(cnt+1);	/* be sure to have room for NUL */
		pair.s = t;
		pair.ns = cnt+1;
		send(c1, &pair);  
		if(x->flushing){
		        recv(x->flushc, nil);	/* wake up flushing xfid */
		        recv(c2, nil);		/* wake up window and toss data */
			free(t);
			filsyscancel(x);
			return;
		}
		qlock(&x->active);
		recv(c2, &pair);   
		fc.data = pair.s;
		if(pair.ns > cnt)	pair.ns = cnt;
		fc.count = pair.ns;
		filsysrespond(x->fs, x, &fc, nil);
		free(t);
		qunlock(&x->active);
		break;
#endif //%------------------------------------------------------

	default:
		fprint(2, "unknown qid %d in read\n", qid);
		sprint(buf, "unknown qid in read");
		filsysrespond(x->fs, x, &fc, buf);
		break;
	}
}
