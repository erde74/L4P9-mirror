#include  <l4all.h>      // HK 20091031
#include <lp49/l_actobj.h> // HK 20091231

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


#define  RECVP(varptr) {L4_ThreadId_t sender;	\
      l_recv0(&sender, INF, nil); l_getarg(nil, "i1", varptr); }
static int  dbg_mouse = 0; //%

/*
 *  WASHINGTON (AP) - The Food and Drug Administration warned
 * consumers Wednesday not to use ``Rio'' hair relaxer products
 * because they may cause severe hair loss or turn hair green....
 *    The FDA urged consumers who have experienced problems with Rio
 * to notify their local FDA office, local health department or the
 * company at 1‑800‑543‑3002.
 */

void		resize(void);
void		move(void);
void		delete(void);
void		hide(void);
void		unhide(int);
void		newtile(int);
Image	*sweep(void);
Image	*bandsize(Window*);
Image*	drag(Window*, Rectangle*);
void		refresh(Rectangle);
void		resized(void);
//Channel	*exitchan;	/* chan(int) */		// HK 20091031
//Channel	*winclosechan; /* chan(Window*); */	// HK 20091031
//L_tidx exitchan, winclosechan;			// HK 20091031
Rectangle	viewr;
int	threadrforkflag = 0; /* should be RFENVG but that hides rio from plumber */

void	mousethread();		// HK 20091031 (cut dummy arg)
void	keyboardthread();	// HK 20091031 (cut dummy arg)
void    winclosethread();	// HK 20091031 (cut dummy arg)
void    deletethread();		// HK 20091031 (cut dummy arg)
void	initcmd(void*);

char	*fontname;
int	mainpid;

L_thcb   *winclose_aobj, *delete_aobj;  //%

enum
{
	New,
	Reshape,
	Move,
	Delete,
	Hide,
	Exit,
};

enum
{
	Cut,
	Paste,
	Snarf,
	Plumb,
	Send,
	Scroll,
};

char		*menu2str[] = {
 [Cut]		"cut",
 [Paste]	"paste",
 [Snarf]	"snarf",
 [Plumb]	"plumb",
 [Send]		"send",
 [Scroll]	"scroll",
		nil
};

Menu menu2 =
{
	menu2str
};

int	Hidden = Exit+1;

char		*menu3str[100] = {
 [New]		"New",
 [Reshape]	"Resize",
 [Move]		"Move",
 [Delete]	"Delete",
 [Hide]		"Hide",
 [Exit]		"Exit",
		nil
};

Menu menu3 =
{
	menu3str
};

char *rcargv[] = { "rc", "-i", nil };
char *kbdargv[] = { "rc", "-c", nil, nil };

int errorshouldabort = 0;

void
derror(Display *dummy, char *errorstr)	// HK 20091031
{
	error(errorstr);
}

void
usage(void)
{
	fprint(2, "usage: rio [-w] [-f font] [-i initcmd] [-k kbdcmd] [-s]\n");
	exits("usage");
}


void main(int argc, char *argv[])	// HK 20091031	threadmain -> main
{
	char *initstr, *kbdin, *s;
	char buf[256];
	Image *i;
	Rectangle r;
	int  winflag = 0;
	L_thcb  *m_thcb;
//%	static void *arg[1];

	if(strstr(argv[0], ".out") == nil){
		menu3str[Exit] = nil;
		Hidden--;
	}
	initstr = nil;
	kbdin = nil;
	maxtab = 0;
	ARGBEGIN{
	case 'f':
		fontname = ARGF();
		if(fontname == nil)
			usage();
		break;
	case 'i':
		initstr = ARGF();
		if(initstr == nil)
			usage();
		break;
	case 'k':
		if(kbdin != nil)
			usage();
		kbdin = ARGF();
		if(kbdin == nil)
			usage();
		break;
	case 's':
		scrolling = TRUE;
		break;
        case 'w':  // test code
	        winflag = TRUE;
		break;
	}ARGEND

	mainpid = getpid();
	if(getwd(buf, sizeof buf) == nil)
		startdir = estrdup(".");
	else
		startdir = estrdup(buf);
	if(fontname == nil)
		fontname = getenv("font");
	if(fontname == nil)
		fontname = "/lib/font/bit/lucm/unicode.9.font";
	s = getenv("tabstop");
	if(s != nil)
		maxtab = strtol(s, nil, 0);
	if(maxtab == 0)
		maxtab = 4;
	free(s);
	/* check font before barging ahead */
	if(access(fontname, 0) < 0){
		fprint(2, "rio: can't access %s: %r\n", fontname);
		// HK 20091031 begin
		fontname = "/bin/unicode.9.font";
		if (access(fontname, 0) < 0) {
			fprint(2, "rio: can't access %s: %r\n", fontname);
			exits("font open");
		}
		// HK 20091031 end
	}
	putenv("font", fontname);

	snarffd = open("/dev/snarf", OREAD|OCEXEC);

	if(geninitdraw(nil, derror, nil, "rio", nil, Refnone) < 0){
		fprint(2, "rio: can't open display: %r\n");
		exits("display open");
	}

	iconinit();
	view = screen;
	viewr = view->r;

	mousectl = initmouse(nil, screen); // Mousectl actobj
	if(mousectl == nil)
		error("can't find mouse");
	mouse = &mousectl->_mouse;		// HK 20091130
//sleep(100); // wait for mouse setup  HK 20091231

        keyboardctl = initkeyboard(nil); // Keyboard actobj
	if(keyboardctl == nil)
		error("can't find keyboard");
//sleep(100); // wait for keyboard setup  HK 20091231

	wscreen = allocscreen(screen, background, 0);
	if(wscreen == nil)
		error("can't allocate screen");
	draw(view, viewr, background, nil, ZP);
	flushimage(display, 1);
	timerinit();

//	exitchan = chancreate(sizeof(int), 0);		// HK 20091031
//	winclosechan = chancreate(sizeof(Window*), 0);	// HK 20091031
//	deletechan = chancreate(sizeof(char*), 0);	// HK 20091031
//	threadcreate(keyboardthread, nil, STACK);	// HK 20091031
//	threadcreate(mousethread, nil, STACK);		// HK 20091031
//	threadcreate(winclosethread, nil, STACK);	// HK 20091031
//	threadcreate(deletethread, nil, STACK);		// HK 20091031
//      keyboard_aobj = l_thread_create(keyboardthread, STACK, nil); //%
//	aobj_mouse.mc = mousectl;
//	mouse_aobj    = l_thread_create(mousethread, STACK, &aobj_mouse); //%

	/*** Mousectl active object == mousethread ***/
	m_thcb = l_thread_create(mousethread, STACK, nil); //%  mousethread
	//	l_thread_create(mousethread, STACK, mousectl); //%
	l_yield(m_thcb);

	winclose_aobj = l_thread_create(winclosethread, STACK, nil); //%
	l_yield((L_thcb*)winclose_aobj);

	delete_aobj   = l_thread_create(deletethread, STACK, nil); //%
	l_yield((L_thcb*)delete_aobj);

	/*====== filsys active object is started. =======*/
	filsys = filsysinit();  //%   Filsys actobj

	if(filsys == nil)
		fprint(2, "rio: can't create file system server: %r\n");
	else{
	        errorshouldabort = 1; // suicide if there's trouble after this

		if(initstr){
		    int  pid;
		    pid = rfork(RFENVG|RFFDG|RFNOTEG|RFNAMEG | RFPROC);
		    switch (pid){
		    case -1:  
		        print("rio: Error fork !\n");
		        break;
		    case 0:
DXX(1010);
		        execl("/bin/rc", "rc", "-c", initstr, nil);
			fprint(2, "rio: exec failed: %r\n");
			exits("exec");
		    default:
		         ;
		    }
		    //% Cf.  proccreate(initcmd, initstr, STACK); //%
		}

		if(kbdin){
			kbdargv[2] = kbdin;
			r = screen->r;
			r.max.x = r.min.x+400; 
			r.max.y = r.min.y+180; 
			i = allocwindow(wscreen, r, Refbackup, DWhite);
			wkeyboard = new(i, FALSE, scrolling, 0, nil, "/bin/rc", kbdargv);
			if(wkeyboard == nil)
				error("can't create keyboard window");
		}

#if 1 // test code for -w paramete
                if(winflag){ // test code
		    int  pid = getpid();
		    char *wsys ;
		    char  attr[64];
		    int   srvfd;
		    int   cc;

		    if ((wsys = getenv("wsys")) == nil)  
		        exits(0);

		    srvfd = open(wsys, ORDWR);
		    free(wsys);
		    if (srvfd < 0)  
		        exits(0);
		    sprint(attr, "N %d 0 0 600 400", pid);

		    sleep(100);
		    pid = rfork(RFENVG|RFFDG|RFNAMEG | RFPROC);

		    switch (pid){
		    case -1:  
		        print("rio: Error fork !\n");
			break;

		  case 0:
sleep(100);		      //   DXX(1020);sleep(100);
		      cc = mount(srvfd, -1, "/mnt/wsys", 0, attr);
		      print(" rio:mount(%d, -1, /mnt/wsys, 0, '%s'):%d \n", 
			    srvfd, attr, cc);
		      sleep(50);
                         
		      cc = bind("/mnt/wsys", "/dev", MBEFORE);
		      print(" rio:bind(/mnt/wsys, /dev, MBEFORE):%d \n", cc);
		      sleep(50);
		      //Cf.      mount(srvfd, -1, "/dev", MBEFORE, attr); 

		      close(0);
		      if(open("/dev/cons", OREAD) < 0){
			  fprint(2, "can't open /dev/cons: %r\n");
		      }
		      close(1);
		      if(open("/dev/cons", OWRITE) < 0){
			  fprint(2, "can't open /dev/cons: %r\n");
		      }
		      close(2);
		      dup(1, 2);

		      //write(1, tt, strlen(tt)); 
		      write(1, "Window-start \n", 14);

		      print(" execl('/bin/rc', 'rc', '-i', nil) \n");
		      sleep(10);
		      execl("/bin/rc", "rc", "-i", nil);
		      //    execl("/bin/qsh", "qsh", nil);
		      print(" execl(...):Failed \n");

		      // will not come here.... 
		      exits(0);

		    default:
      sleep(200);
		      ;
		    }
		    //% Cf.  proccreate(initcmd, initstr, STACK); //%
                }
#endif 

l_thcbtbl_ls();


#if 0 //%---------------------------------------------------------
               while(1){
		    L4_Word_t sp, ip, flag;
		    L4_ThreadState_t  state;
		    L4_ThreadId_t  localtid;
		    localtid = /*L4_LocalIdOf*/(m_thcb->tid);

		    sleep(12000);
		    //if(dbg_mouse)print("\n DBG_MOUSE:%d \n", dbg_mouse);
		    print("\n  MOUSETHREAD<%x>", localtid);
		    state = L4_Stop_SpIpFlags(localtid, &sp, &ip, &flag);
		    print("[state=%x ip=%x, sp=%x, flag=%x]\n", state, ip, sp, flag);
		    L4_Start(localtid);
		}
#endif //%--------------------------------------------------------
		fprint(2, ".....rio is ready......\n"); //%%
		l_recv0(nil, INF, nil); 
	}
	l_thread_killall();
}

/*
 * /dev/snarf updates when the file is closed, so we must open our own
 * fd here rather than use snarffd
 */
void
putsnarf(void)
{
	int fd, i, n;

	if(snarffd<0 || nsnarf==0)
		return;
	fd = open("/dev/snarf", OWRITE);
	if(fd < 0)
		return;
	/* snarf buffer could be huge, so fprint will truncate; do it in blocks */
	for(i=0; i<nsnarf; i+=n){
		n = nsnarf-i;
		if(n >= 256)
			n = 256;
		if(fprint(fd, "%.*S", n, snarf+i) < 0)
			break;
	}
	close(fd);
}

void
getsnarf(void)
{
	int i, n, nb, nulls;
	char *sn, buf[1024];

	if(snarffd < 0)
		return;
	sn = nil;
	i = 0;
	seek(snarffd, 0, 0);
	while((n = read(snarffd, buf, sizeof buf)) > 0){
		sn = erealloc(sn, i+n+1);
		memmove(sn+i, buf, n);
		i += n;
		sn[i] = 0;
	}
	if(i > 0){
		snarf = runerealloc(snarf, i+1);
		cvttorunes(sn, i, snarf, &nb, &nsnarf, &nulls);
		free(sn);
	}
}

#if 0 //%
void
initcmd(void *arg)
{
	char *cmd;

	cmd = arg;
	rfork(RFENVG|RFFDG|RFNOTEG|RFNAMEG);
//%	procexecl(nil, "/bin/rc", "rc", "-c", cmd, nil); // HK 20091130 debug
	fprint(2, "rio: exec failed: %r\n");
	exits("exec");
}
#endif

char *oknotes[] =
{
	"delete",
	"hangup",
	"kill",
	"exit",
	nil
};

int
shutdown(void *dummy, char *msg)	// HK 20091031
{
	int i;

	killprocs();
	for(i=0; oknotes[i]; i++)
		if(strncmp(oknotes[i], msg, strlen(oknotes[i])) == 0)
			l_thread_exitsall(msg);	// HK 20091130
	fprint(2, "rio %d: abort: %s\n", getpid(), msg);

	abort();
	exits(msg);
	return 0;
}

void
killprocs(void)
{
	int i;

	for(i=0; i<nwindow; i++)
		postnote(PNGROUP, window[i]->pid, "hangup");
}


/*
 * Used by /dev/kbdin
 */
void
keyboardsend(char *s, int cnt)
{
	Rune *r;
	int i, nb, nr;

	r = runemalloc(cnt);
	/* BUGlet: partial runes will be converted to error runes */
	cvttorunes(s, cnt, r, &nb, &nr, nil);
	for(i=0; i<nr; i++){
	      l_putarg(nil, WKey, "i2", &r[i], 1);  //% To be refined
	      l_send0((L_thcb*)keyboardctl, INF, nil);
	      //% send(keyboardctl->c, &r[i]);
	}
	free(r);
}

int
portion(int x, int lo, int hi)
{
#if 1 
	if(x < lo + 20)
		return 0;
	if(x > hi-20)
		return 2;
	return 1;
#else //original
	x -= lo;
	hi -= lo;
	if(x < 20)
		return 0;
	if(x > hi-20)
		return 2;
	return 1;
#endif
}

int
whichcorner(Window *w, Point p)
{
	int i, j;
	
	i = portion(p.x, w->screenr.min.x, w->screenr.max.x);
	j = portion(p.y, w->screenr.min.y, w->screenr.max.y);
	return 3*j+i;
}

void
cornercursor(Window *w, Point p, int force)
{
	if(w!=nil && winborder(w, p))
		riosetcursor(corners[whichcorner(w, p)], force);
	else
		wsetcursor(w, force);
}

/* thread to allow fsysproc to synchronize window closing with main proc */
void
winclosethread(void* _x)  //%  winclose_aobj  
{
	Window *w;

	l_thread_setname("winclosethread");  //% 
	for(;;){
	        RECVP(&w); //%    w = recvp(winclosechan);  //%
		wclose(w);
	}
}

/* thread to make Deleted windows that the client still holds disappear offscreen after an interval */
void
deletethread(void* _x)  //%   delete_aobj
{
	char *s;
	Image *i;

	l_thread_setname("deletethread");  //%

	for(;;){
	        RECVP(&s); //%   s = recvp(deletechan);
DXX(0);
		i = namedimage(display, s);
		if(i != nil){
		    //move it off-screen to hide it, since client is slow in letting it go
		    originwindow(i, i->r.min, view->r.max);
		}
		freeimage(i);
		free(s);
	}
}

void
deletetimeoutproc(void *v, char *s)	// HK 20100312
{
	L_msgtag   msgtag;
//	char *s;	// HK 20100312
//	s = v;		// HK 20100312
	sleep(750); /* remove window from screen after 3/4 of a second */

	l_putarg(nil, 0, "i1", s);
	msgtag = l_send0(delete_aobj, INF, nil);
	//% sendp(deletechan, s); 
	if (L4_IpcFailed(msgtag)) IPCERR("deletetimeoutproc");
}

/*
 * Button 6 - keyboard toggle - has been pressed.
 * Send event to keyboard, wait for button up, send that.
 * Note: there is no coordinate translation done here; this
 * is just about getting button 6 to the keyboard simulator.
 */
void
keyboardhide(void)     //% -> wind.c: 301
{
        L_msgtag  msgtag;

	l_putarg(nil, WMouse, "i1", mouse);
	msgtag = l_send0((L_thcb*)wkeyboard, INF, nil);
	//%  send(wkeyboard->mc.c, mouse);
	if (L4_IpcFailed(msgtag)) IPCERR("keyboardhide");
	do
		readmouse(mousectl);
	while(mouse->buttons & (1<<5));

	l_putarg(nil, WMouse, "i1", mouse);
	msgtag = l_send0((L_thcb*)wkeyboard, INF, nil);
	//%  send(wkeyboard->mc.c, mouse);  
	if (L4_IpcFailed(msgtag)) IPCERR("keyboardhide");
}


/**********************************************************
 *  Mouse action active object / single                   *
 *       Data:     Mousectl type                          *
 *       Action:   mousethread() in rio.c                 *
 **********************************************************/


#define  MXX(n)  {dbg_mouse = (n);}
void
mousethread(/* struct Mousectl *self */)  /** self == mousectl **/
{
	int sending, inside, scrolling, moving, band;
	Window *oin, *w, *winput;
	Image *i;
	Rectangle r;
	Point xy;
	Mouse tmp;
	L_mbuf *mbuf = nil; //%
	int   len1;
	enum {
		MReshape = 16,
		MMouse,
		NALT
	};

	l_thread_setname("mousethread"); //%
	sending = FALSE;
	scrolling = FALSE;
	moving = FALSE;

//%static Alt alts[NALT+1];
//%alts[MReshape].c=mousectl->resizec; alts[MReshape].v=nil; alts[MReshape].op=CHANRCV;
//%alts[MMouse].c=mousectl->c;alts[MMouse].v=&mousectl->_mouse;alts[MMouse].op=CHANRCV;
//%alts[NALT].op=CHANEND;

	for(;;){
		L_msgtag  msgtag;
		if (mbuf) {
			free(mbuf);
			mbuf = nil; // HK 20100312
		}
	MXX(0);

		// msgtag = l_arecv0(self->mc->mbox, INF, &mbuf);
	msgtag = l_arecv0(mousectl->mbox, 5000/*INF*/, &mbuf); 
		if (msgtag.raw == 0) { //Time over
		    print("$");
		    continue;
		}

	        switch(MLABEL(msgtag)){    //% switch(alt(alts)){ 
		case MReshape:
		        //%  <-- libdraw:mouse(mc->resizedc)!one
        MXX(100);
			resized();
			break;

		case MMouse:
		        //%      <-- libdraw:mouse(mc->c)!message
		        len1 = sizeof(mousectl->_mouse);  // 100220
		        l_getarg(mbuf, "s1", &mousectl->_mouse, &len1); //% HK 20091231
	MXX(200);
			/** mouse == &mousectl->_mouse **/
			if(wkeyboard!=nil && (mouse->buttons & (1<<5))){
			    keyboardhide();
			    break;
			}

		Again:
	MXX(300);
	DZZ(1);
//if(mouse->buttons)print(" BTN<%x> ", mouse->buttons);
			winput = input;
			/* override everything for the keyboard window */
			if(wkeyboard!=nil && ptinrect(mouse->xy, wkeyboard->screenr)){
			    /* make sure it's on top; this call is free if it is */
			    wtopme(wkeyboard);
			    winput = wkeyboard;
			}

			if(winput!=nil && winput->i!=nil){
			    /* convert to logical coordinates */
			    xy.x = mouse->xy.x + 
			          (winput->i->r.min.x - winput->screenr.min.x);
			    xy.y = mouse->xy.y + 
				  (winput->i->r.min.y - winput->screenr.min.y);

			    // up/down scroll buttons do not subject to the usual rules
			    if((mouse->buttons & (8|16)) && !winput->mouseopen)
			        goto Sending;

			    inside = ptinrect(mouse->xy, 
					      insetrect(winput->screenr, Selborder));

			    if(winput->mouseopen)
			          scrolling = FALSE;
			    else if(scrolling)
			          scrolling = mouse->buttons;
			    else
			          scrolling = mouse->buttons && 
				              ptinrect(xy, winput->scrollr);

			    /* topped will be <= 0 if window has been bottomed */
			    if(sending == FALSE && !scrolling 
			                  && winborder(winput, mouse->xy) 
                                          && winput->topped>0) {
			        moving = TRUE;
			    }
			    else if(inside && 
			       (scrolling || winput->mouseopen || (mouse->buttons & 1)))
			        sending = TRUE;

			}else
			    sending = FALSE;


			if(sending){
			Sending:
	  MXX(400);
				if(mouse->buttons == 0){
					cornercursor(winput, mouse->xy, 0);
					sending = FALSE;
				}
				else
					wsetcursor(winput, 0);

				tmp = mousectl->_mouse; //% Mouse
				tmp.xy = xy;

				l_putarg(nil, WMouse, "s1", &tmp, sizeof(tmp));
				msgtag = l_send0((L_thcb*)winput, INF, nil);
				if (L4_IpcFailed(msgtag)) IPCERR("mousethread");
				//% send(winput->mc.c, &tmp);
				continue;
			}

	MXX(500);
			w = wpointto(mouse->xy);

			/* change cursor if over anyone's border */
			if(w != nil)
			    cornercursor(w, mouse->xy, 0);
			else
			    riosetcursor(nil, 0);

			if(moving && (mouse->buttons&7)){
	MXX(600);
			    oin = winput;
			    band = mouse->buttons & 3;
			    sweeping = 1;
			    if(band)
			        i = bandsize(winput);
			    else
			        i = drag(winput, &r);

			    sweeping = 0;
			    if(i != nil){
			        if(winput == oin){
				    if(band)
				        wsendctlmesg(winput, Reshaped, i->r, i);
				    else
				        wsendctlmesg(winput, Moved, r, i);
				    cornercursor(winput, mouse->xy, 1);
				}else
				    freeimage(i);
			    }
			}
	MXX(700);

			if(w != nil)
			    cornercursor(w, mouse->xy, 0);

			/* we're not sending the event, 
			 * but if button is down maybe we should */
			if(mouse->buttons){
	MXX(800);
			    /* w->topped will be <= 0 if window has been bottomed */
			    if(w==nil || (w == winput && w->topped > 0)){
			        if(mouse->buttons & 1){
				    // print("BTN#<%x, %x>   ", mouse->buttons, w);
                                    ;
				}
				else if(mouse->buttons & 2){
				    // print("BTN#<%x, %x>   ", mouse->buttons, w);
				    if(winput && !winput->mouseopen)
				        button2menu(winput);
				}
				else if(mouse->buttons & 4){
				    // print("BTN#<%x, %x>   ", mouse->buttons, w);
                                    button3menu();
				}
	MXX(899);
			    }
			    else{ //% Top the clicked window.
			        /* IF BUTTON-1 event in the window, 
				 * top the window and wait for button up. 
				 * ELSE top the window and pass the event on*/
	MXX(900);
	                        // print("BTN##<%x, %x>   ", mouse->buttons, w);//%??
			        if(wtop(mouse->xy) && 
				   (mouse->buttons!=1 || winborder(w, mouse->xy))){
				    print("#Again ");
				    goto Again;
				}
				print("#Drain ");
#if 1 //%
				do
				    readmouse(mousectl);
				while(mousectl->_mouse.buttons);  //% _mouse. 
				moving = FALSE;
				goto Again;     /* recalculate mouse position, cursor */
#else //original
				goto Drain; 
#endif

			    }
			}
			moving = FALSE;
			break;
#if 0 //% oriignal
		Drain:
	MXX(1000);
			do
				readmouse(mousectl);
			while(mousectl->_mouse.buttons);  //% _mouse.
			moving = FALSE;
			goto Again;	/* recalculate mouse position, cursor */
#endif //%
		}//switch
	}
}

void
resized(void)
{
	Image *im;
	int i, j, ishidden;
	Rectangle r;
	Point o, n;
	Window *w;

	if(getwindow(display, Refnone) < 0)
		error("failed to re-attach window");
	freescrtemps();
	view = screen;
	freescreen(wscreen);
	wscreen = allocscreen(screen, background, 0);
	if(wscreen == nil)
		error("can't re-allocate screen");
	draw(view, view->r, background, nil, ZP);
	o = subpt(viewr.max, viewr.min);
	n = subpt(view->clipr.max, view->clipr.min);
	for(i=0; i<nwindow; i++){
		w = window[i];
		if(w->deleted)
			continue;
		r = rectsubpt(w->i->r, viewr.min);
		r.min.x = (r.min.x*n.x)/o.x;
		r.min.y = (r.min.y*n.y)/o.y;
		r.max.x = (r.max.x*n.x)/o.x;
		r.max.y = (r.max.y*n.y)/o.y;
		r = rectaddpt(r, screen->clipr.min);
		ishidden = 0;
		for(j=0; j<nhidden; j++)
			if(w == hidden[j]){
				ishidden = 1;
				break;
			}
		if(ishidden)
			im = allocimage(display, r, screen->chan, 0, DWhite);
		else
			im = allocwindow(wscreen, r, Refbackup, DWhite);
		if(im)
			wsendctlmesg(w, Reshaped, r, im);
	}
	viewr = screen->r;
	flushimage(display, 1);
}

void
button3menu(void)
{DXX(0);
	int i;

	for(i=0; i<nhidden; i++)
		menu3str[i+Hidden] = hidden[i]->label;
	menu3str[i+Hidden] = nil;

	sweeping = 1;
	switch(i = menuhit(3, mousectl, &menu3, wscreen)){
	case -1:
		break;
	case New:
		new(sweep(), FALSE, scrolling, 0, nil, "/bin/rc", nil);
		break;
	case Reshape:
		resize();
		break;
	case Move:
		move();
		break;
	case Delete:
		delete();
		break;
	case Hide:
		hide();
		break;
	case Exit:
		if(Hidden > Exit){
		        //%%	send(exitchan, nil); //% send to main thread
			break;
		}
		/* else fall through */
	default:
		unhide(i);
		break;
	}
	sweeping = 0;
DXX(99);
}

void
button2menu(Window *w)
{
	if(w->deleted)
		return;
	incref(&w->_ref);  //% w
	if(w->scrolling)
		menu2str[Scroll] = "noscroll";
	else
		menu2str[Scroll] = "scroll";
	switch(menuhit(2, mousectl, &menu2, wscreen)){
	case Cut:
		wsnarf(w);
		wcut(w);
		wscrdraw(w);
		break;

	case Snarf:
		wsnarf(w);
		break;

	case Paste:
		getsnarf();
		wpaste(w);
		wscrdraw(w);
		break;

	case Plumb:
		wplumb(w);
		break;

	case Send:
		getsnarf();
		wsnarf(w);
		if(nsnarf == 0)
			break;
		if(w->rawing){
			waddraw(w, snarf, nsnarf);
			if(snarf[nsnarf-1]!='\n' && snarf[nsnarf-1]!='\004')
			  waddraw(w, (Rune*)L"\n", 1);  //% (Rune*)
		}else{
			winsert(w, snarf, nsnarf, w->nr);
			if(snarf[nsnarf-1]!='\n' && snarf[nsnarf-1]!='\004')
			  winsert(w, (Rune*)L"\n", 1, w->nr);  //% (Rune*)
		}
		wsetselect(w, w->nr, w->nr);
		wshow(w, w->nr);
		break;

	case Scroll:
		if(w->scrolling ^= 1)
			wshow(w, w->nr);
		break;
	}
	wclose(w);
	wsendctlmesg(w, Wakeup, ZR, nil);
	flushimage(display, 1);
}

Point
onscreen(Point p)
{
	p.x = max(screen->clipr.min.x, p.x);
	p.x = min(screen->clipr.max.x, p.x);
	p.y = max(screen->clipr.min.y, p.y);
	p.y = min(screen->clipr.max.y, p.y);
	return p;
}

Image*
sweep(void)
{
	Image *i, *oi;
	Rectangle r;
	Point p0, p;

	i = nil;
	menuing = TRUE;
	riosetcursor(&crosscursor, 1);
	while(mouse->buttons == 0)
		readmouse(mousectl);
	p0 = onscreen(mouse->xy);
	p = p0;
	r.min = p;
	r.max = p;
	oi = nil;
	while(mouse->buttons == 4){
		readmouse(mousectl);
		if(mouse->buttons != 4 && mouse->buttons != 0)
			break;
		if(!eqpt(mouse->xy, p)){
			p = onscreen(mouse->xy);
			r = canonrect(Rpt(p0, p));
			if(Dx(r)>5 && Dy(r)>5){ // Sweep grey-window with red-border 
				i = allocwindow(wscreen, r, Refnone, 0xEEEEEEFF); /* grey */
				freeimage(oi);
				if(i == nil)
					goto Rescue;
				oi = i;
				border(i, r, Selborder, red, ZP);
				flushimage(display, 1);
			}
		}
	}
	if(mouse->buttons != 0)
		goto Rescue;
	if(i==nil || Dx(i->r)<100 || Dy(i->r)<3*font->height)
		goto Rescue;
	oi = i;
	i = allocwindow(wscreen, oi->r, Refbackup, DWhite);
	freeimage(oi);
	if(i == nil)
		goto Rescue;
	border(i, r, Selborder, red, ZP);
	cornercursor(input, mouse->xy, 1);
	goto Return;

 Rescue:
	freeimage(i);
	i = nil;
	cornercursor(input, mouse->xy, 1);
	while(mouse->buttons)
		readmouse(mousectl);

 Return:
	moveto(mousectl, mouse->xy);	/* force cursor update; ugly */
	menuing = FALSE;
	return i;
}

void
drawedge(Image **bp, Rectangle r)
{
	Image *b = *bp;
	if(b != nil && Dx(b->r) == Dx(r) && Dy(b->r) == Dy(r))
		originwindow(b, r.min, r.min);
	else{
		freeimage(b);
		*bp = allocwindow(wscreen, r, Refbackup, DRed);
	}
}

void
drawborder(Rectangle r, int show)
{
	static Image *b[4];
	int i;
	if(show == 0){
		for(i = 0; i < 4; i++){
			freeimage(b[i]);
			b[i] = nil;
		}
	}else{
		r = canonrect(r);
		drawedge(&b[0], Rect(r.min.x, r.min.y, r.min.x+Borderwidth, r.max.y));
		drawedge(&b[1], Rect(r.min.x+Borderwidth, r.min.y, r.max.x-Borderwidth, r.min.y+Borderwidth));
		drawedge(&b[2], Rect(r.max.x-Borderwidth, r.min.y, r.max.x, r.max.y));
		drawedge(&b[3], Rect(r.min.x+Borderwidth, r.max.y-Borderwidth, r.max.x-Borderwidth, r.max.y));
	}
}

Image*
drag(Window *w, Rectangle *rp)
{
	Image *i, *ni;
	Point p, op, d, dm, om;
	Rectangle r;

	i = w->i;
	menuing = TRUE;
	om = mouse->xy;
	riosetcursor(&boxcursor, 1);
	dm = subpt(mouse->xy, w->screenr.min);
	d = subpt(i->r.max, i->r.min);
	op = subpt(mouse->xy, dm);
	drawborder(Rect(op.x, op.y, op.x+d.x, op.y+d.y), 1);
	flushimage(display, 1);
	while(mouse->buttons == 4){
		p = subpt(mouse->xy, dm);
		if(!eqpt(p, op)){
			drawborder(Rect(p.x, p.y, p.x+d.x, p.y+d.y), 1);
			flushimage(display, 1);
			op = p;
		}
		readmouse(mousectl);
	}
	r = Rect(op.x, op.y, op.x+d.x, op.y+d.y);
	drawborder(r, 0);
	cornercursor(w, mouse->xy, 1);
	moveto(mousectl, mouse->xy);	/* force cursor update; ugly */
	menuing = FALSE;
	flushimage(display, 1);
	if(mouse->buttons!=0 || (ni=allocwindow(wscreen, r, Refbackup, DWhite))==nil){
		moveto(mousectl, om);
		while(mouse->buttons)
			readmouse(mousectl);
		*rp = Rect(0, 0, 0, 0);
		return nil;
	}
	draw(ni, ni->r, i, nil, i->r.min);
	*rp = r;
	return ni;
}

Point
cornerpt(Rectangle r, Point p, int which)
{
	switch(which){
	case 0:	/* top left */
		p = Pt(r.min.x, r.min.y);
		break;
	case 2:	/* top right */
		p = Pt(r.max.x,r.min.y);
		break;
	case 6:	/* bottom left */
		p = Pt(r.min.x, r.max.y);
		break;
	case 8:	/* bottom right */
		p = Pt(r.max.x, r.max.y);
		break;
	case 1:	/* top edge */
		p = Pt(p.x,r.min.y);
		break;
	case 5:	/* right edge */
		p = Pt(r.max.x, p.y);
		break;
	case 7:	/* bottom edge */
		p = Pt(p.x, r.max.y);
		break;
	case 3:		/* left edge */
		p = Pt(r.min.x, p.y);
		break;
	}
	return p;
}

Rectangle
whichrect(Rectangle r, Point p, int which)
{
	switch(which){
	case 0:	/* top left */
		r = Rect(p.x, p.y, r.max.x, r.max.y);
		break;
	case 2:	/* top right */
		r = Rect(r.min.x, p.y, p.x, r.max.y);
		break;
	case 6:	/* bottom left */
		r = Rect(p.x, r.min.y, r.max.x, p.y);
		break;
	case 8:	/* bottom right */
		r = Rect(r.min.x, r.min.y, p.x, p.y);
		break;
	case 1:	/* top edge */
		r = Rect(r.min.x, p.y, r.max.x, r.max.y);
		break;
	case 5:	/* right edge */
		r = Rect(r.min.x, r.min.y, p.x, r.max.y);
		break;
	case 7:	/* bottom edge */
		r = Rect(r.min.x, r.min.y, r.max.x, p.y);
		break;
	case 3:		/* left edge */
		r = Rect(p.x, r.min.y, r.max.x, r.max.y);
		break;
	}
	return canonrect(r);
}

Image*
bandsize(Window *w)
{
	Image *i;
	Rectangle r, or;
	Point p, startp;
	int which, but;

	p = mouse->xy;
	
	which = whichcorner(w, p);
	p = cornerpt(w->screenr, p, which);
	wmovemouse(w, p);
	readmouse(mousectl);
	r = whichrect(w->screenr, p, which);
	drawborder(r, 1);
	or = r;
	startp = p;
	but = mouse->buttons;
	while(mouse->buttons == but){
		p = onscreen(mouse->xy);
		r = whichrect(w->screenr, p, which);
		if(!eqrect(r, or) && goodrect(r)){
			drawborder(r, 1);
			flushimage(display, 1);
			or = r;
		}
		readmouse(mousectl);
	}
	p = mouse->xy;
	drawborder(or, 0);
	flushimage(display, 1);
	wsetcursor(w, 1);
	if(mouse->buttons!=0 || Dx(or)<100 || Dy(or)<3*font->height){
		while(mouse->buttons)
			readmouse(mousectl);
		return nil;
	}
	if(abs(p.x-startp.x)+abs(p.y-startp.y) <= 1)
		return nil;
	i = allocwindow(wscreen, or, Refbackup, DWhite);
	if(i == nil)
		return nil;
	border(i, r, Selborder, red, ZP);
	return i;
}

Window*
pointto(int wait)
{
	Window *w;

	menuing = TRUE;
	riosetcursor(&sightcursor, 1);
	while(mouse->buttons == 0)
		readmouse(mousectl);
	if(mouse->buttons == 4)
		w = wpointto(mouse->xy);
	else
		w = nil;
	if(wait){
		while(mouse->buttons){
			if(mouse->buttons!=4 && w !=nil){	/* cancel */
				cornercursor(input, mouse->xy, 0);
				w = nil;
			}
			readmouse(mousectl);
		}
		if(w != nil && wpointto(mouse->xy) != w)
			w = nil;
	}
	cornercursor(input, mouse->xy, 0);
	moveto(mousectl, mouse->xy);	/* force cursor update; ugly */
	menuing = FALSE;
	return w;
}

void
delete(void)
{
	Window *w;

	w = pointto(TRUE);
	if(w)
		wsendctlmesg(w, Deleted, ZR, nil);
}

void
resize(void)
{
	Window *w;
	Image *i;

	w = pointto(TRUE);
	if(w == nil)
		return;
	i = sweep();
	if(i)
		wsendctlmesg(w, Reshaped, i->r, i);
}

void
move(void)
{
	Window *w;
	Image *i;
	Rectangle r;

	w = pointto(FALSE);
	if(w == nil)
		return;
	i = drag(w, &r);
	if(i)
		wsendctlmesg(w, Moved, r, i);
	cornercursor(input, mouse->xy, 1);
}

int
whide(Window *w)
{
	Image *i;
	int j;

	for(j=0; j<nhidden; j++)
		if(hidden[j] == w)	/* already hidden */
			return -1;
	i = allocimage(display, w->screenr, w->i->chan, 0, DWhite);
	if(i){
		hidden[nhidden++] = w;
		wsendctlmesg(w, Reshaped, ZR, i);
		return 1;
	}
	return 0;
}

int
wunhide(int h)
{
	Image *i;
	Window *w;

	w = hidden[h];
	i = allocwindow(wscreen, w->i->r, Refbackup, DWhite);
	if(i){
		--nhidden;
		memmove(hidden+h, hidden+h+1, (nhidden-h)*sizeof(Window*));
		wsendctlmesg(w, Reshaped, w->i->r, i);
		return 1;
	}
	return 0;
}

void
hide(void)
{
	Window *w;

	w = pointto(TRUE);
	if(w == nil)
		return;
	whide(w);
}

void
unhide(int h)
{
	Window *w;

	h -= Hidden;
	w = hidden[h];
	if(w == nil)
		return;
	wunhide(h);
}

//% this is called from  wctl.c::wctlnew()
Window*
new(Image *i, int hideit, int scrollit, int pid, char *dir, char *cmd, char **argv)
{DXX(0);
	Window *w;
	Mousectl *mc;
	void **arg;
	int    pid2, xx;
	L_thcb  *w_thcb;
	//%	Channel *cm, *ck, *cctl, *cpid;

	if(i == nil)
		return nil;

//% cm = chancreate(sizeof(Mouse), 0); ck = chancreate(sizeof(Rune*), 0); 
//% cctl = chancreate(sizeof(Wctlmesg), 4); cpid = chancreate(sizeof(int), 0);
//% if(cm==nil || ck==nil || cctl==nil)	error("new: channel alloc failed");

	mc = emalloc(sizeof(Mousectl));
	*mc = *mousectl;
	mc->image = i;
	//%	mc->c = cm;

	w = wmk(i, mc, scrollit); //% w = wmk(i, mc, ck, cctl, scrollit);  

	free(mc);	/* wmk copies *mc */
	window = erealloc(window, ++nwindow*sizeof(Window*));
	window[nwindow-1] = w;
	if(hideit){
		hidden[nhidden++] = w;
		w->screenr = ZR;
	}

	incref(&w->_ref); //?
	w_thcb = l_thread_create(winctl, 4096, w); //% Winctl (Window) actobject
	l_yield(w_thcb);
	l_wait_ready(w_thcb); //%

sleep(100);

	if(!hideit)
		wcurrent(w);

	flushimage(display, 1);

	if(pid == 0){
		arg = emalloc(5*sizeof(void*));
		arg[0] = w;
		arg[1] = 0;   //% arg[1] = cpid;
		arg[2] = cmd;
		if(argv == nil)
			arg[3] = rcargv;
		else
			arg[3] = argv;
		arg[4] = dir;

                //% Trial code ----------------------------------------
		pid2 = rfork(RFNAMEG | RFFDG | RFENVG | RFPROC);

		switch(pid2){
		case 0:
DXX(100);
		      if(filsysmount(filsys, w->id) < 0){
			  fprint(2, "mount failed: %r\n");
			  //%%    sendul(pidc, 0); 
			  l_thread_exits("mount failed");  //%   
		      }

		      close(0);
		      if(open("/dev/cons", OREAD) < 0){
			  fprint(2, "can't open /dev/cons: %r\n");
			  //%%    sendul(pidc, 0);  
			  l_thread_exits("/dev/cons");  //%
		      }

		      close(1);
		      if(open("/dev/cons", OWRITE) < 0){
			  fprint(2, "can't open /dev/cons: %r\n");
			  //%%    sendul(pidc, 0);  
			  l_thread_exits("open");  //%  BUG? was terminate() 
		      }

		      print("==== new->Window start ====\n");
		      if(wclose(w) == 0 ){ // remove extra ref hanging from creation.
			  //% notify(nil);
			  close(2); //??
			  dup(1, 2);
			  if(dir)
			      chdir(dir);

                          if (argv == nil)  
			      argv = rcargv;
			  print(" rio:new:exec('/bin/rc', 'rc', '-i', nil) \n");
			  sleep(100);  //??
                         
			  execl("/bin/rc", "rc", "-i", nil); // exec(cmd, argv); 
			  print("\n?? rio:new(): exec failed !!\n");
			  //%%    _exits("exec failed");  
		      }
		      else {
			  print("\n?? rio:new():wclose() != 0  !!\n");
		      }
		    break;

		case -1:
		    print("rio:new(): Error fork !!\n");
		    break;

		default:
sleep(100);
DXX(200);
                    free(arg);
		    wsetpid(w, pid2, 1);
		    wsetname(w);
		    if(dir)
		        w->dir = estrdup(dir);
 DXX(299);//print("\n new window created: %x\n", w);
                     return w;
		}//switch
		//%     l_thread_create_args(winshell, 8192, nil, 1, arg); //091228

                RECVP(&xx);
		/*%-----plan9 original---------------------------------
		 * proccreate(winshell, arg, 8192);  //%
		 * pid = recvul(cpid); //%
                 */
		free(arg);
		//% end of trial code --------
	}
DXX(50);
	if(pid == 0){
		/* window creation failed */
		wsendctlmesg(w, Deleted, ZR, nil);
		//%	chanfree(cpid);  //%
		return nil;
	}
	wsetpid(w, pid, 1);
	wsetname(w);
	if(dir)
		w->dir = estrdup(dir);

	//%	chanfree(cpid);
DXX(99);
	return w;
}


#if 0 //% keyboardthread and libdraw/keyboard.c can be combined.-----------
void
keyboardthread(void* _x)  //%
{
	Rune buf[2][20], *rp;
	int n, i;
	L_msgtag  msgtag; //%
	L_mbuf   *mbuf; //%

	l_thread_setname("keyboardthread");  //%
	n = 0;
	for(;;){
		rp = buf[n];
		n = 1-n;
		
		if (mbuf) free(mbuf);
		msgtag = l_arecv0(keyboardctl->mbox, INF, &mbuf);
		l_getarg(mbuf,  "i1", &rp);
		//%%  recv(keyboardctl->c, rp);  //% To Be Modified
		//%% for(i=1; i<nelem(buf[0])-1; i++)
		//%%  if(nbrecv(keyboardctl->c, rp+i) <= 0)  //% TBM
		//%%		break;
		rp[i] = L'\0';
		if(input != nil){
		    l_putarg(nil, WKey, "i1", rp);
		    l_send0((L_thcb*)input, INF, nil);
		    // SENDP((L_thcb*)input, WKey, rp); //% sendp(input->ck, rp);
		}
	}
}
#endif //%---------------------------------------------
