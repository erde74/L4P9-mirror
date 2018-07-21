//%
#include  <l4all.h>          //%
#include  <lp49/l_actobj.h>  //%

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
#include <complete.h>

#include "dat.h"
#include "fns.h"


#define MOVEIT if(0)

enum
{
	HiWater	= 64000,	/* max size of history */
	LoWater	= 40000,	/* min size of history after max'ed */
	MinWater= 20000,	/* room to leave available when reallocating */
};

static	int	topped;
static	int	id;

static	Image	*cols[NCOL];
static	Image	*grey;
static	Image	*darkgrey;
static	Cursor	*lastcursor;
static	Image	*titlecol;
static	Image	*lighttitlecol;
static	Image	*holdcol;
static	Image	*lightholdcol;
static	Image	*paleholdcol;


Window*
wmk(Image *i, Mousectl *mc, int scrolling)
//% wmk(Image *i, Mousectl *mc, Channel *ck, Channel *cctl, int scrolling)
{
	Window *w;
	Rectangle r;

	if(cols[0] == nil){
		/* greys are multiples of 0x11111100+0xFF, 14* being palest */
		grey = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0xEEEEEEFF);
		darkgrey = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x666666FF);
		cols[BACK] = display->white;
		cols[HIGH] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0xCCCCCCFF);
		cols[BORD] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x999999FF);
		cols[TEXT] = display->black;
		cols[HTEXT] = display->black;
		titlecol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DGreygreen);
		lighttitlecol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DPalegreygreen);
		holdcol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DMedblue);
		lightholdcol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DGreyblue);
		paleholdcol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DPalegreyblue);
	}
	w = emalloc(sizeof(Window));
	if (w == nil){
	    print("\n  Out of memory in wmk \n");
	    exits(0);
	}
	w->screenr = i->r;
	r = insetrect(i->r, Selborder+1);
	w->i = i;
	w->mc = *mc;
	w->cursorp = nil;
//%	w->ck = ck;	w->cctl = cctl;  w->conswrite = chancreate(sizeof(Conswritemesg), 0); w->consread = chancreate(sizeof(Consreadmesg), 0); w->mouseread =  chancreate(sizeof(Mousereadmesg), 0); w->wctlread =  chancreate(sizeof(Consreadmesg), 0);
	w->scrollr = r;
	w->scrollr.max.x = r.min.x+Scrollwid;
	w->lastsr = ZR;
	r.min.x += Scrollwid+Scrollgap;
	frinit(&w->_frame, r, font, i, cols);  //% w
	w->_frame.maxtab = maxtab*stringwidth(font, "0"); //%
	w->topped = ++topped;
	w->id = ++id;
	w->notefd = -1;
	w->scrolling = scrolling;
	w->dir = estrdup(startdir);
	w->label = estrdup("<unnamed>");
	r = insetrect(w->i->r, Selborder);
	draw(w->i, r, cols[BACK], nil, w->_frame.entire.min);  //%
	wborder(w, Selborder);
	wscrdraw(w);
	incref(&w->_ref); //% ref will be removed after mounting; avoids delete before ready to be deleted 
	return w;
}


void
wsetname(Window *w)
{
	int i, n;
	char err[ERRMAX];
sleep(20);
//DXX(0);	
	n = sprint(w->name, "window.%d.%d", w->id, w->namecount++);
	for(i='A'; i<='Z'; i++){
		if(nameimage(w->i, w->name, 1) > 0)
			return;
		errstr(err, sizeof err);
		if(strcmp(err, "image name in use") != 0){
			break;
		}
		w->name[n] = i;
		w->name[n+1] = 0;
	}
	w->name[0] = 0;
	fprint(2, "?? rio: setname failed: %s\n", err);
}

void
wresize(Window *w, Image *i, int move)
{
	Rectangle r, or;

	or = w->i->r;
	if(move || (Dx(or)==Dx(i->r) && Dy(or)==Dy(i->r)))
		draw(i, i->r, w->i, nil, w->i->r.min);
	freeimage(w->i);
	w->i = i;
	wsetname(w);
	w->mc.image = i;
	r = insetrect(i->r, Selborder+1);
	w->scrollr = r;
	w->scrollr.max.x = r.min.x+Scrollwid;
	w->lastsr = ZR;
	r.min.x += Scrollwid+Scrollgap;
	if(move)
	        frsetrects(&w->_frame, r, w->i); //% w
	else{
	        frclear(&w->_frame, FALSE);  //%
		frinit(&w->_frame, r, w->_frame.font, w->i, cols);  //%
		wsetcols(w);
		w->_frame.maxtab = maxtab*stringwidth(w->_frame.font, "0"); //%
		r = insetrect(w->i->r, Selborder);
		draw(w->i, r, cols[BACK], nil, w->_frame.entire.min); //%
		wfill(w);
		wsetselect(w, w->q0, w->q1);
		wscrdraw(w);
	}
	wborder(w, Selborder);
	w->topped = ++topped;
	w->resized = TRUE;
	w->mouse.counter++;
}

void
wrefresh(Window *w, Rectangle _z)  //%	
{
	/* BUG: rectangle is ignored */
	if(w == input)
		wborder(w, Selborder);
	else
		wborder(w, Unselborder);
	if(w->mouseopen)
		return;
	draw(w->i, insetrect(w->i->r, Borderwidth), w->_frame.cols[BACK], nil, w->i->r.min); //%
	w->_frame.ticked = 0; //%
	if(w->_frame.p0 > 0) //%
	        frdrawsel(&w->_frame, frptofchar(&w->_frame, 0), 0, w->_frame.p0, 0); //%
	if(w->_frame.p1 < w->_frame.nchars) //%
	        frdrawsel(&w->_frame, frptofchar(&w->_frame, w->_frame.p1), w->_frame.p1, w->_frame.nchars, 0); //%
	frdrawsel(&w->_frame, frptofchar(&w->_frame, w->_frame.p0), w->_frame.p0, w->_frame.p1, 1); //%
	w->lastsr = ZR;
	wscrdraw(w);
}

int
wclose(Window *w)
{
	int i;

	i = decref(&w->_ref);  //% w
	if(i > 0)
		return 0;
	if(i < 0)
		error("negative ref count");
	if(!w->deleted)
		wclosewin(w);
	wsendctlmesg(w, Exited, ZR, nil);
	return 1;
}

//% added --------------------------------------------------------
static char  *ctlmsgname[] =
  {
    "Wakeup",
    "Reshaped",
    "Moved",
    "Refresh",
    "Movemouse",
    "Rawon",
    "Rawoff",
    "Holdon",
    "Holdoff",
    "Deleted",
    "Exited",
  };


//% GATE: WCread_ready(): TO BE PROGRAMMED
//% if(w->holding)  return 0;
//% else if(npart || (w->rawing && w->nraw>0)) return 1;
//% else {
//%   for(i=w->qh; i<w->nr; i++){
//%     c = w->r[i];
//%     if(c=='\n' || c=='\004'){ return  1;}
//%   }
//%   return 0;
//% }

int WCread_ready(Window *w)
{
        int  i, c;
	for(i = w->qh; i < w->nr; i++){
	    c = w->r[i];
	    if (c == '\n' || c == '\004')
	        return  1;
	}
	return  0;
}

int WMouseread_ready(Window *w)
{
        if(w->mouseopen && w->mouse.counter != w->mouse.lastcounter)
	      return 1;
	else  return 0;
}

int WCwrite_ready(Window *w)
{
        if(!w->scrolling && !w->mouseopen && w->qh>w->org+w->_frame.nchars)
	      return  0;
	else  return  1;
}

int WWread_ready(Window *w)
{
        if(w->deleted || !w->wctlready)
	      return  0;
	else  return  1;
}


/**********************************************************
 *  Window active object / per window                     *
 *       Data:     Window type                            *
 *       Action:   winctl() in wind.c                     *
 **********************************************************/
#if 1 //%-------------------------------------------------------------------
void
winctl(void *arg)  //% arg: self Window structure 
{
	Rune *rp, *bp, *tp, *up, *kbdr;
	Rune  runebuf[10];  //%
	uint qh;
	int nr, nb, c, wid, i, npart, initial, lastb;
	char *s, *t, part[3];
	char *t2;   int  nb2;
	Window *w;
	Mousestate *mp, m;
	Stringpair pair;
	Wctlmesg wcm;
	char buf[4*12+1];
	L_msgtag       msgtag;
	int            mlabel;
	int            len1, len2, len3;
	L4_ThreadId_t  from;
	L4_ThreadId_t  WCread_client, WMouseread_client, WWread_client;
	int            WCread_request = 0, WMouseread_request = 0, WWread_request = 0;

	w = arg;
	snprint(buf, sizeof buf, "winctl-id%d", w->id);
	l_thread_setname(buf);

	l_thcb_ls((L_thcb*)w);
 
	npart = 0;
	lastb = -1;
	l_post_ready(0); //%

	for(;;){
		/******************************************
		 *   Request phase                        *
		 ******************************************/
		msgtag = l_recv0(&from, 5000, nil); 
		if (L4_IpcFailed(msgtag)){ 
		    print("!"); //print("?? winctl-recv0(%d)  ", L4_ErrorCode());
                    continue;
		}
		mlabel = MLABEL(msgtag);

		switch(mlabel){      //%
		case WKey: //%  THIS <== WKey(Runebuf) === keyboardthread, rio.c:346

		        len1 = sizeof(runebuf);
                        l_getarg(nil, "s1", runebuf, &len1); //%
DXX(100);
                        for(i=0; runebuf[i]!=L'\0'; i++){//print("@%c  ", runebuf[i]);
				wkeyctl(w, runebuf[i]); 
			}
			break;


		case WMouse:  //%  THIS <=== WMouse ===  rio.c::mousethread  

		        len3 = sizeof(w->mc._mouse);
		        l_getarg(nil, "s1", &w->mc._mouse, &len3); //%
DXX(200);
			if(w->mouseopen) {
				w->mouse.counter++;

				/* queue click events */
				if(!w->mouse.qfull && lastb != w->mc._mouse.buttons) { //% add to ring
					mp = &w->mouse.queue[w->mouse.wi];
					if(++w->mouse.wi == nelem(w->mouse.queue))
						w->mouse.wi = 0;
					if(w->mouse.wi == w->mouse.ri)
						w->mouse.qfull = TRUE;
					mp->_mouse = w->mc._mouse; //% Mouse
					mp->counter = w->mouse.counter;
					lastb = w->mc._mouse.buttons;  //%
				}
			} else
				wmousectl(w);
			break;

		case WMouseread:  //%  THIS <=== WMouseread ===  xfid.c::xfidread()
		        //% GATE: if(w->mouseopen && w->mouse.counter != w->mouse.lastcounter)

			/* Send a queued event or, 
			 *      if the queue is empty, the current state 
			 * If the queue has filled, 
			 *      we discard all the events it contained. 
			 * The intent is to discard frantic clicking by the user 
			 *      during long latencies. 
			 */
#if 1 //%--------------------------------------------------------------
		        WMouseread_request = 1;
			WMouseread_client = L4_GlobalIdOf(from);
			if (WMouseread_ready(w))  break;
			else                      continue;
#else //%---------------------------------------------------------------
		        {// moved to the reply phase.
			    w->mouse.qfull = FALSE;
			    if(w->mouse.wi != w->mouse.ri) {
				m = w->mouse.queue[w->mouse.ri];
				if(++w->mouse.ri == nelem(w->mouse.queue))
					w->mouse.ri = 0;
			    } else
			        m = (Mousestate){w->mc._mouse, w->mouse.counter}; //% Mouse

			    w->mouse.lastcounter = m.counter;
			    l_putarg(nil, WMouseread, "s1", &m._mouse, sizeof(m._mouse));
			    l_reply0(from, nil);
			    //% send(mrm.cm, &m._mouse);//%(mrm.cm)!(m._mouse)==>mousethread  
			}
			continue;
#endif //%------------------------------------------------------------------

		case WCtl: //%  THIS <=== WCtl(Wctlmesg) ===  wsendctlmesg() 

		        len2 = sizeof(wcm);
		        l_getarg(nil, "s1", &wcm, &len2); //% 100218

DXX(400);print("WCtl<%s:%x:%x>   ", ctlmsgname[wcm.type], wcm.r.max.y, wcm.image); sleep(300);
			if(wctlmesg(w, wcm.type, wcm.r, wcm.image) == Exited){
			        l_thread_exits(nil);  //%
			}
			continue;


		case WCwrite:  //% THIS <=== WCwrite(addr, len) === xfid.c::xfidwrite()
		        //% GATE: if(w->scrolling || w->mouseopen || w->qh <= w->org+w->nchars) 

			l_getarg(nil, "i2", &rp, &nr); 
DXX(500);
			bp = rp;
			for(i=0; i<nr; i++)
				if(*bp++ == '\b'){
					--bp;
					initial = 0;
					tp = runemalloc(nr);
					runemove(tp, rp, i);
					up = tp+i;
					for(; i<nr; i++){
						*up = *bp++;
						if(*up == '\b')
							if(up == tp)
								initial++;
							else
								--up;
						else
							up++;
					}
					if(initial){
						if(initial > w->qh)
							initial = w->qh;
						qh = w->qh-initial;
						wdelete(w, qh, qh+initial);
						w->qh = qh;
					}
					free(rp);
					rp = tp;
					nr = up-tp;
					rp[nr] = 0;
					break;
				}
			w->qh = winsert(w, rp, nr, w->qh)+nr;
			if(w->scrolling || w->mouseopen)
				wshow(w, w->qh);
			wsetselect(w, w->q0, w->q1);
			wscrdraw(w);
			free(rp);
//DXX(505);
			break;

		case WCread:   //% THIS <=== WCread(addr, len) === xfid.c::xfidread()

		        l_getarg(nil, "i2", &t2, &nb2);
DXX(600);
#if 1 //% trial code ------------------------------------------------------------
                        WCread_request = 1;
			WCread_client = L4_GlobalIdOf(from);  //??	 WCread_client = from; 
                        if (WCread_ready(w))
			    break;
			else
			    continue;
#endif //-------------------------------------------------

		case WWread:  //% THIS <==== WWread ====== xfid.c::xfidread()

		        //% GATE: TO BE PROGRAMMED
		        //% if(! w->deleted && w->wctlready)

		        l_getarg(nil, "i2", &pair.s, &pair.ns); //%
#if 1 //%--------------------------------------------------------------
		        WWread_request = 1;
			WWread_client = L4_GlobalIdOf(from);
			if (WWread_ready(w))  break;
			else                  continue;
#else //%---------------------------------------------------------------
			{// To be moved to the reply phase.
			    w->wctlready = 0;
			    //%	recv(cwrm.c1, &pair); //% *** --> cwrm.c1 ! (pair)
			    if(w->deleted || w->i==nil)
				pair.ns = sprint(pair.s, "");
			    else{
				s = "visible";
				for(i=0; i<nhidden; i++)
					if(hidden[i] == w){
						s = "hidden";
						break;
					}
				t = "notcurrent";
				if(w == input)
					t = "current";
				pair.ns = snprint(pair.s, pair.ns, "%11d %11d %11d %11d %s %s ",
					w->i->r.min.x, w->i->r.min.y, w->i->r.max.x, w->i->r.max.y, t, s);
			    }
			    //% send(cwrm.c2, &pair);  //% *** <-- cwrm.c2 ! (pair)
			    l_putarg(nil, 0, "i2", pair.s, pair.ns);
			    l_reply0(from, nil);
			}
			continue;
#endif //%---------------------------------------------------

		default: 
		       print("\n ?========= WINCTL:MLABEL:%d<-%x (%x) ============\n",
			     mlabel, from, L4_GlobalIdOf(from));
		}//switch

#if 1 //% trialcode: respond to the read request. --------------------------------------
		/*****************************************
		 *     Reply phase                       *
		 *****************************************/
		if (WCread_request && WCread_ready(w))
		{ 
// DXX(660);
			WCread_request = 0; 
			i = npart;
			npart = 0;
			if(i)
				memmove(t2, part, i);
			while(i < nb2 && (w->qh < w->nr || w->nraw > 0)){
				if(w->qh == w->nr){
					wid = runetochar(t2+i, &w->raw[0]);
					w->nraw--;
					runemove(w->raw, w->raw+1, w->nraw);
				}else
					wid = runetochar(t2+i, &w->r[w->qh++]);
				c = t2[i];	/* knows break characters fit in a byte */
				i += wid;
				if(!w->rawing && (c == '\n' || c=='\004')){
					if(c == '\004')
						i--;
					break;
				}
			}
			if(i==nb2 && w->qh < w->nr && w->r[w->qh]=='\004')
				w->qh++;
			if(i > nb2){
				npart = i-nb2;
				memmove(part, t2+nb2, npart);
				i = nb2;
			}

DXX(665);print("replyto:%x  ", WCread_client);
			l_putarg(nil, CRdata, "i2", t2, i);
			msgtag = l_reply0(WCread_client, nil);
			// msgtag = l_send0(TID2THCB(WCread_client), 10000, nil);
			if (L4_IpcFailed(msgtag)) 
			     print("winctl:REPLY-ERR:%x \n", msgtag);
DXX(666);
		}

		if (WMouseread_request && WMouseread_ready(w)) {
		        WMouseread_request = 0;

			w->mouse.qfull = FALSE;
			if(w->mouse.wi != w->mouse.ri) {
				m = w->mouse.queue[w->mouse.ri];
				if(++w->mouse.ri == nelem(w->mouse.queue))
					w->mouse.ri = 0;
			} else
			        m = (Mousestate){w->mc._mouse, w->mouse.counter}; //% Mouse

			w->mouse.lastcounter = m.counter;
			l_putarg(nil, WMouseread, "s1", &m._mouse, sizeof(m._mouse));
			l_reply0(WMouseread_client, nil);
		}

		if (WWread_request && WWread_ready(w)) {
		        WWread_request = 0;

			w->wctlready = 0;
			//%	recv(cwrm.c1, &pair); //% *** --> cwrm.c1 ! (pair)
			if(w->deleted || w->i==nil)
				pair.ns = sprint(pair.s, "");
			else{
				s = "visible";
				for(i=0; i<nhidden; i++)
					if(hidden[i] == w){
						s = "hidden";
						break;
					}
				t = "notcurrent";
				if(w == input)
					t = "current";
				pair.ns = snprint(pair.s, pair.ns, "%11d %11d %11d %11d %s %s ",
					w->i->r.min.x, w->i->r.min.y, w->i->r.max.x, w->i->r.max.y, t, s);
			}
			l_putarg(nil, 0, "i2", pair.s, pair.ns);
			l_reply0(WWread_client, nil);
		}
#endif //%---------------------------------------------------------------
		/*****************************************
		 *     Post phase                        *
		 *****************************************/
		if(!w->deleted)
			flushimage(display, 1);
	}
}

#else  //%plan9 -------------------------------------------------------
void
winctl(void *arg)
{
    Rune *rp, *bp, *tp, *up, *kbdr;
    uint qh;
    int nr, nb, c, wid, i, npart, initial, lastb;
    char *s, *t, part[3];
    Window *w;
    Mousestate *mp, m;
    enum { WKey, WMouse, WMouseread, WCtl, WCwrite, WCread, WWread, NWALT };
    Alt alts[NWALT+1];
    Mousereadmesg mrm;
    Conswritemesg cwm;
    Consreadmesg crm;
    Consreadmesg cwrm;
    Stringpair pair;
    Wctlmesg wcm;
    char buf[4*12+1];
    
    w = arg;
    snprint(buf, sizeof buf, "winctl-id%d", w->id);
    threadsetname(buf);

    mrm.cm = chancreate(sizeof(Mouse), 0);
    cwm.cw = chancreate(sizeof(Stringpair), 0);
    crm.c1 = chancreate(sizeof(Stringpair), 0);
    crm.c2 = chancreate(sizeof(Stringpair), 0);
    cwrm.c1 = chancreate(sizeof(Stringpair), 0);
    cwrm.c2 = chancreate(sizeof(Stringpair), 0);
        
    alts[WKey].c = w->ck;
    alts[WKey].v = &kbdr;
    alts[WKey].op = CHANRCV;
    alts[WMouse].c = w->mc.c;
    alts[WMouse].v = &w->mc.Mouse;
    alts[WMouse].op = CHANRCV;
    alts[WMouseread].c = w->mouseread;
    alts[WMouseread].v = &mrm;
    alts[WMouseread].op = CHANSND;
    alts[WCtl].c = w->cctl;
    alts[WCtl].v = &wcm;
    alts[WCtl].op = CHANRCV;
    alts[WCwrite].c = w->conswrite;
    alts[WCwrite].v = &cwm;
    alts[WCwrite].op = CHANSND;
    alts[WCread].c = w->consread;
    alts[WCread].v = &crm;
    alts[WCread].op = CHANSND;
    alts[WWread].c = w->wctlread;
    alts[WWread].v = &cwrm;
    alts[WWread].op = CHANSND;
    alts[NWALT].op = CHANEND;

    npart = 0;
    lastb = -1;
    for(;;){
        if(w->mouseopen && w->mouse.counter != w->mouse.lastcounter)
	  alts[WMouseread].op = CHANSND;
	else
	  alts[WMouseread].op = CHANNOP;
	if(!w->scrolling && !w->mouseopen && w->qh>w->org+w->nchars)
	  alts[WCwrite].op = CHANNOP;
	else
	  alts[WCwrite].op = CHANSND;
	if(w->deleted || !w->wctlready)
	  alts[WWread].op = CHANNOP;
	else
	  alts[WWread].op = CHANSND;
	/* this code depends on NL and EOT fitting in a single byte */
	/* kind of expensive for each loop; worth precomputing? */
	if(w->holding)
	  alts[WCread].op = CHANNOP;
	else if(npart || (w->rawing && w->nraw>0))
	  alts[WCread].op = CHANSND;
	else{
	  alts[WCread].op = CHANNOP;
	  for(i=w->qh; i<w->nr; i++){
	    c = w->r[i];
	    if(c=='\n' || c=='\004'){
	      alts[WCread].op = CHANSND;
	      break;
	    }
	  }
	}

	switch(alt(alts)){
	case WKey:
	  for(i=0; kbdr[i]!=L'\0'; i++)
	    wkeyctl(w, kbdr[i]);
	  //                      wkeyctl(w, r);
	  ///                     while(nbrecv(w->ck, &r))
	  //                              wkeyctl(w, r);
	  break;

	case WMouse:
	  if(w->mouseopen) {
	    w->mouse.counter++;
	  
	    /* queue click events */
	    if(!w->mouse.qfull && lastb != w->mc.buttons) { /* add to ring */
	      mp = &w->mouse.queue[w->mouse.wi];
	      if(++w->mouse.wi == nelem(w->mouse.queue))
		w->mouse.wi = 0;
	      if(w->mouse.wi == w->mouse.ri)
		w->mouse.qfull = TRUE;
	      mp->Mouse = w->mc;
	      mp->counter = w->mouse.counter;
	      lastb = w->mc.buttons;
	    }
	  } else
	    wmousectl(w);
	  break;

	case WMouseread:
	  /* send a queued event or, if the queue is empty, the current state */
	  /* if the queue has filled, we discard all the events it contained. */
	  /* the intent is to discard frantic clicking by the user during long latencies. */
	  w->mouse.qfull = FALSE;
	  if(w->mouse.wi != w->mouse.ri) {
	    m = w->mouse.queue[w->mouse.ri];
	    if(++w->mouse.ri == nelem(w->mouse.queue))
	      w->mouse.ri = 0;
	  } else
	    m = (Mousestate){w->mc.Mouse, w->mouse.counter};

	  w->mouse.lastcounter = m.counter;
	  send(mrm.cm, &m.Mouse);
	  continue;

	case WCtl:
	  if(wctlmesg(w, wcm.type, wcm.r, wcm.image) == Exited){
	    chanfree(crm.c1);
	    chanfree(crm.c2);
	    chanfree(mrm.cm);
	    chanfree(cwm.cw);
	    chanfree(cwrm.c1);
	    chanfree(cwrm.c2);
	    threadexits(nil);
	  }
	  continue;

	case WCwrite:
	  recv(cwm.cw, &pair);
	  rp = pair.s;
	  nr = pair.ns;
	  bp = rp;
	  for(i=0; i<nr; i++)
	    if(*bp++ == '\b'){
	      --bp;
	      initial = 0;
	      tp = runemalloc(nr);
	      runemove(tp, rp, i);
	      up = tp+i;
	      for(; i<nr; i++){
		*up = *bp++;
		if(*up == '\b')
		  if(up == tp)
		    initial++;
		  else
		    --up;
		else
		  up++;
	      }
	      if(initial){
		if(initial > w->qh)
		  initial = w->qh;
		qh = w->qh-initial;
		wdelete(w, qh, qh+initial);
		w->qh = qh;
	      }
	      free(rp);
	      rp = tp;
	      nr = up-tp;
	      rp[nr] = 0;
	      break;
	    }
	  w->qh = winsert(w, rp, nr, w->qh)+nr;
	  if(w->scrolling || w->mouseopen)
	    wshow(w, w->qh);
	  wsetselect(w, w->q0, w->q1);
	  wscrdraw(w);
	  free(rp);
	  break;

	case WCread:
	  recv(crm.c1, &pair);
	  t = pair.s;
	  nb = pair.ns;
	  i = npart;
	  npart = 0;
	  if(i)
	    memmove(t, part, i);
	  while(i<nb && (w->qh<w->nr || w->nraw>0)){
	    if(w->qh == w->nr){
	      wid = runetochar(t+i, &w->raw[0]);
	      w->nraw--;
	      runemove(w->raw, w->raw+1, w->nraw);
	    }else
	      wid = runetochar(t+i, &w->r[w->qh++]);
	    c = t[i];       /* knows break characters fit in a byte */
	    i += wid;
	    if(!w->rawing && (c == '\n' || c=='\004')){
	      if(c == '\004')
		i--;
	      break;
	    }
	  }
	  if(i==nb && w->qh<w->nr && w->r[w->qh]=='\004')
	    w->qh++;
	  if(i > nb){
	    npart = i-nb;
	    memmove(part, t+nb, npart);
	    i = nb;
	  }
	  pair.s = t;
	  pair.ns = i;
	  send(crm.c2, &pair);
	  continue;

	case WWread:
	  w->wctlready = 0;
	  recv(cwrm.c1, &pair);
	  if(w->deleted || w->i==nil)
	    pair.ns = sprint(pair.s, "");
	  else{
	    s = "visible";
	    for(i=0; i<nhidden; i++)
	      if(hidden[i] == w){
		s = "hidden";
		break;
	      }
	    t = "notcurrent";
	    if(w == input)
	      t = "current";
	    pair.ns = snprint(pair.s, pair.ns, "%11d %11d %11d %11d %s %s ",
			    w->i->r.min.x, w->i->r.min.y, w->i->r.max.x, w->i->r.max.y, t, s);
	  }
	  send(cwrm.c2, &pair);
	  continue;
	}
	if(!w->deleted)
	  flushimage(display, 1);
    }
}
#endif //%-------------------------------------------------------------

void
waddraw(Window *w, Rune *r, int nr)
{
	w->raw = runerealloc(w->raw, w->nraw+nr);
	runemove(w->raw+w->nraw, r, nr);
	w->nraw += nr;
}

/*
 * Need to do this in a separate proc because if process we're interrupting
 * is dying and trying to print tombstone, kernel is blocked holding p->debug lock.
 */
void
interruptproc(void *v)
{
	int *notefd;

	notefd = v;
	write(*notefd, "interrupt", 9);
	free(notefd);
}

int
windfilewidth(Window *w, uint q0, int oneelement)
{
	uint q;
	Rune r;

	q = q0;
	while(q > 0){
		r = w->r[q-1];
		if(r<=' ')
			break;
		if(oneelement && r=='/')
			break;
		--q;
	}
	return q0-q;
}

void
showcandidates(Window *w, Completion *c)
{
	int i;
	Fmt f;
	Rune *rp;
	uint nr, qline, q0;
	char *s;

	runefmtstrinit(&f);
	if (c->nmatch == 0)
		s = "[no matches in ";
	else
		s = "[";
	if(c->nfile > 32)
		fmtprint(&f, "%s%d files]\n", s, c->nfile);
	else{
		fmtprint(&f, "%s", s);
		for(i=0; i<c->nfile; i++){
			if(i > 0)
				fmtprint(&f, " ");
			fmtprint(&f, "%s", c->filename[i]);
		}
		fmtprint(&f, "]\n");
	}
	/* place text at beginning of line before host point */
	qline = w->qh;
	while(qline>0 && w->r[qline-1] != '\n')
		qline--;

	rp = runefmtstrflush(&f);
	nr = runestrlen(rp);

	q0 = w->q0;
	q0 += winsert(w, rp, runestrlen(rp), qline) - qline;
	free(rp);
	wsetselect(w, q0+nr, q0+nr);
}

Rune*
namecomplete(Window *w)
{
	int nstr, npath;
	Rune *rp, *path, *str;
	Completion *c;
	char *s, *dir, *root;

	/* control-f: filename completion; works back to white space or / */
	if(w->q0<w->nr && w->r[w->q0]>' ')	/* must be at end of word */
		return nil;
	nstr = windfilewidth(w, w->q0, TRUE);
	str = runemalloc(nstr);
	runemove(str, w->r+(w->q0-nstr), nstr);
	npath = windfilewidth(w, w->q0-nstr, FALSE);
	path = runemalloc(npath);
	runemove(path, w->r+(w->q0-nstr-npath), npath);
	rp = nil;

	/* is path rooted? if not, we need to make it relative to window path */
	if(npath>0 && path[0]=='/'){
		dir = malloc(UTFmax*npath+1);
		sprint(dir, "%.*S", npath, path);
	}else{
		if(strcmp(w->dir, "") == 0)
			root = ".";
		else
			root = w->dir;
		dir = malloc(strlen(root)+1+UTFmax*npath+1);
		sprint(dir, "%s/%.*S", root, npath, path);
	}
	dir = cleanname(dir);

	s = smprint("%.*S", nstr, str);
	c = complete(dir, s);
	free(s);
	if(c == nil)
		goto Return;

	if(!c->advance)
		showcandidates(w, c);

	if(c->advance)
		rp = runesmprint("%s", c->string);

  Return:
	//%%	freecompletion(c);
	free(dir);
	free(path);
	free(str);
	return rp;
}

void
wkeyctl(Window *w, Rune r)
{
	uint q0 ,q1;
	int n, nb, nr;
	Rune *rp;
	int *notefd;

	if(r == 0)
		return;
	if(w->deleted)
		return;
	/* navigation keys work only when mouse is not open */
	if(!w->mouseopen)
		switch(r){
		case Kdown:
		        n = w->_frame.maxlines/3; //%
			goto case_Down;
		case Kscrollonedown:
		        n = mousescrollsize(w->_frame.maxlines);  //%
			if(n <= 0)
				n = 1;
			goto case_Down;
		case Kpgdown:
		        n = 2*w->_frame.maxlines/3;  //%
		case_Down:
			q0 = w->org+frcharofpt(&w->_frame, Pt(w->_frame.r.min.x, w->_frame.r.min.y+n*w->_frame.font->height)); //% Frame
			wsetorigin(w, q0, TRUE);
			return;
		case Kup:
		        n = w->_frame.maxlines/3;  //%
			goto case_Up;
		case Kscrolloneup:
		        n = mousescrollsize(w->_frame.maxlines);  //%
			if(n <= 0)
				n = 1;
			goto case_Up;
		case Kpgup:
		        n = 2*w->_frame.maxlines/3;  //%
		case_Up:
			q0 = wbacknl(w, w->org, n);
			wsetorigin(w, q0, TRUE);
			return;
		case Kleft:
			if(w->q0 > 0){
				q0 = w->q0-1;
				wsetselect(w, q0, q0);
				wshow(w, q0);
			}
			return;
		case Kright:
			if(w->q1 < w->nr){
				q1 = w->q1+1;
				wsetselect(w, q1, q1);
				wshow(w, q1);
			}
			return;
		case Khome:
			wshow(w, 0);
			return;
		case Kend:
			wshow(w, w->nr);
			return;
		case 0x01:	/* ^A: beginning of line */
			if(w->q0==0 || w->q0==w->qh || w->r[w->q0-1]=='\n')
				return;
			nb = wbswidth(w, 0x15 /* ^U */);
			wsetselect(w, w->q0-nb, w->q0-nb);
			wshow(w, w->q0);
			return;
		case 0x05:	/* ^E: end of line */
			q0 = w->q0;
			while(q0 < w->nr && w->r[q0]!='\n')
				q0++;
			wsetselect(w, q0, q0);
			wshow(w, w->q0);
			return;
		}
	if(w->rawing && (w->q0==w->nr || w->mouseopen)){
		waddraw(w, &r, 1);
		return;
	}
	if(r==0x1B || (w->holding && r==0x7F)){	/* toggle hold */
		if(w->holding)
			--w->holding;
		else
			w->holding++;
		wrepaint(w);
		if(r == 0x1B)
			return;
	}
	if(r != 0x7F){
		wsnarf(w);
		wcut(w);
	}
	switch(r){
	case 0x7F:		/* send interrupt */
		w->qh = w->nr;
		wshow(w, w->qh);
		notefd = emalloc(sizeof(int));
		*notefd = w->notefd;
		print("creating interruptproc ..."); //%???
		l_thread_create(interruptproc, 4096, notefd); //%
		//%%	proccreate(interruptproc, notefd, 4096);
		return;
	case 0x06:	/* ^F: file name completion */
	case Kins:		/* Insert: file name completion */
		rp = namecomplete(w);
		if(rp == nil)
			return;
		nr = runestrlen(rp);
		q0 = w->q0;
		q0 = winsert(w, rp, nr, q0);
		wshow(w, q0+nr);
		free(rp);
		return;
	case 0x08:	/* ^H: erase character */
	case 0x15:	/* ^U: erase line */
	case 0x17:	/* ^W: erase word */
		if(w->q0==0 || w->q0==w->qh)
			return;
		nb = wbswidth(w, r);
		q1 = w->q0;
		q0 = q1-nb;
		if(q0 < w->org){
			q0 = w->org;
			nb = q1-q0;
		}
		if(nb > 0){
			wdelete(w, q0, q0+nb);
			wsetselect(w, q0, q0);
		}
		return;
	}
	/* otherwise ordinary character; just insert */
	q0 = w->q0;
	q0 = winsert(w, &r, 1, q0);
	wshow(w, q0+1);
}

void
wsetcols(Window *w)
{
	if(w->holding)
		if(w == input)
		        w->_frame.cols[TEXT] = w->_frame.cols[HTEXT] = holdcol; //%
		else
		        w->_frame.cols[TEXT] = w->_frame.cols[HTEXT] = lightholdcol;  //%
	else
		if(w == input)
		  w->_frame.cols[TEXT] = w->_frame.cols[HTEXT] = display->black;  //%
		else
		  w->_frame.cols[TEXT] = w->_frame.cols[HTEXT] = darkgrey;  //%
}

void
wrepaint(Window *w)
{
	wsetcols(w);
	if(!w->mouseopen){
		if(font->maxdepth > 1)
		        draw(w->_frame.b, w->_frame.r, cols[BACK], nil, ZP); //%
		_frredraw(&w->_frame, w->_frame.r.min); //%
	}
	if(w == input){
		wborder(w, Selborder);
		wsetcursor(w, 0);
	}else
		wborder(w, Unselborder);
}

int
wbswidth(Window *w, Rune c)
{
	uint q, eq, stop;
	Rune r;
	int skipping;

	/* there is known to be at least one character to erase */
	if(c == 0x08)	/* ^H: erase character */
		return 1;
	q = w->q0;
	stop = 0;
	if(q > w->qh)
		stop = w->qh;
	skipping = TRUE;
	while(q > stop){
		r = w->r[q-1];
		if(r == '\n'){		/* eat at most one more character */
			if(q == w->q0)	/* eat the newline */
				--q;
			break; 
		}
		if(c == 0x17){
			eq = isalnum(r);
			if(eq && skipping)	/* found one; stop skipping */
				skipping = FALSE;
			else if(!eq && !skipping)
				break;
		}
		--q;
	}
	return w->q0-q;
}

void
wsnarf(Window *w)
{
	if(w->q1 == w->q0)
		return;
	nsnarf = w->q1-w->q0;
	snarf = runerealloc(snarf, nsnarf);
	snarfversion++;	/* maybe modified by parent */
	runemove(snarf, w->r+w->q0, nsnarf);
	putsnarf();
}

void
wcut(Window *w)
{
	if(w->q1 == w->q0)
		return;
	wdelete(w, w->q0, w->q1);
	wsetselect(w, w->q0, w->q0);
}

void
wpaste(Window *w)
{
	uint q0;

	if(nsnarf == 0)
		return;
	wcut(w);
	q0 = w->q0;
	if(w->rawing && q0==w->nr){
		waddraw(w, snarf, nsnarf);
		wsetselect(w, q0, q0);
	}else{
		q0 = winsert(w, snarf, nsnarf, w->q0);
		wsetselect(w, q0, q0+nsnarf);
	}
}

void
wplumb(Window *w)
{
#if 0 //%----------------------------------
	Plumbmsg *m;
	static int fd = -2;
	char buf[32];
	uint p0, p1;
	Cursor *c;

	if(fd == -2)
		fd = plumbopen("send", OWRITE|OCEXEC);
	if(fd < 0)
		return;
	m = emalloc(sizeof(Plumbmsg));
	m->src = estrdup("rio");
	m->dst = nil;
	m->wdir = estrdup(w->dir);
	m->type = estrdup("text");
	p0 = w->q0;
	p1 = w->q1;
	if(w->q1 > w->q0)
		m->attr = nil;
	else{
		while(p0>0 && w->r[p0-1]!=' ' && w->r[p0-1]!='\t' && w->r[p0-1]!='\n')
			p0--;
		while(p1<w->nr && w->r[p1]!=' ' && w->r[p1]!='\t' && w->r[p1]!='\n')
			p1++;
		sprint(buf, "click=%d", w->q0-p0);
		m->attr = plumbunpackattr(buf);
	}
	if(p1-p0 > messagesize-1024){
		plumbfree(m);
		return;	/* too large for 9P */
	}
	m->data = runetobyte(w->r+p0, p1-p0, &m->ndata);
	if(plumbsend(fd, m) < 0){
		c = lastcursor;
		riosetcursor(&query, 1);
		sleep(300);
		riosetcursor(c, 1);
	}
	plumbfree(m);
#endif  //%--------------------------------
}

int
winborder(Window *w, Point xy)
{
	return ptinrect(xy, w->screenr) && !ptinrect(xy, insetrect(w->screenr, Selborder));
}

void
wmousectl(Window *w)
{
	int but;

	if( w->mc._mouse.buttons == 1)  //%
		but = 1;
	else if( w->mc._mouse.buttons == 2) //%
		but = 2;
	else if( w->mc._mouse.buttons == 4) //%
		but = 3;
	else{
	  if( w->mc._mouse.buttons == 8)  //%
			wkeyctl(w, Kscrolloneup);
	  if( w->mc._mouse.buttons == 16)  //%
			wkeyctl(w, Kscrollonedown);
		return;
	}

	incref(&w->_ref); //% w  hold up window while we track
	if(w->deleted)
		goto Return;
	if(ptinrect(w->mc._mouse.xy, w->scrollr)){  //%
		if(but)
			wscroll(w, but);
		goto Return;
	}
	if(but == 1)
		wselect(w);
	/* else all is handled by main process */
   Return:
	wclose(w);
}

void
wdelete(Window *w, uint q0, uint q1)
{
	uint n, p0, p1;

	n = q1-q0;
	if(n == 0)
		return;
	runemove(w->r+q0, w->r+q1, w->nr-q1);
	w->nr -= n;
	if(q0 < w->q0)
		w->q0 -= min(n, w->q0-q0);
	if(q0 < w->q1)
		w->q1 -= min(n, w->q1-q0);
	if(q1 < w->qh)
		w->qh -= n;
	else if(q0 < w->qh)
		w->qh = q0;
	if(q1 <= w->org)
		w->org -= n;
	else if(q0 < w->org+w->_frame.nchars){  //%
		p1 = q1 - w->org;
		if(p1 > w->_frame.nchars)  //%
		        p1 = w->_frame.nchars;  //%
		if(q0 < w->org){
			w->org = q0;
			p0 = 0;
		}else
			p0 = q0 - w->org;
		frdelete(&w->_frame, p0, p1); //%
		wfill(w);
	}
}


static Window	*clickwin;
static uint	clickmsec;
static Window	*selectwin;
static uint	selectq;

/*
 * called from frame library
 */
void
framescroll(Frame *f, int dl)
{
  if(f != &selectwin->_frame)  //% Frame
		error("frameselect not right frame");
	wframescroll(selectwin, dl);
}

void
wframescroll(Window *w, int dl)
{
	uint q0;

	if(dl == 0){
		wscrsleep(w, 100);
		return;
	}
	if(dl < 0){
		q0 = wbacknl(w, w->org, -dl);
		if(selectq > w->org+w->_frame.p0)  //%
		  wsetselect(w, w->org+w->_frame.p0, selectq); //%
		else
		  wsetselect(w, selectq, w->org+w->_frame.p0); //%
	}else{
	        if(w->org+w->_frame.nchars == w->nr) //%
			return;
		q0 = w->org+frcharofpt(&w->_frame, Pt(w->_frame.r.min.x, w->_frame.r.min.y+dl*w->_frame.font->height));  //%
		if(selectq >= w->org+w->_frame.p1) //%
		  wsetselect(w, w->org+w->_frame.p1, selectq); //%
		else
		  wsetselect(w, selectq, w->org+w->_frame.p1);  //%
	}
	wsetorigin(w, q0, TRUE);
}

void
wselect(Window *w)
{
	uint q0, q1;
	int b, x, y, first;

	first = 1;
	selectwin = w;
	/*
	 * Double-click immediately if it might make sense.
	 */
	b =  w->mc._mouse.buttons;  //%
	q0 = w->q0;
	q1 = w->q1;
	selectq = w->org+frcharofpt(&w->_frame, w->mc._mouse.xy);  //%

	if(clickwin==w && w->mc._mouse.msec-clickmsec<500) //%
	if(q0==q1 && selectq==w->q0){
		wdoubleclick(w, &q0, &q1);
		wsetselect(w, q0, q1);
		flushimage(display, 1);
		x = w->mc._mouse.xy.x; //%
		y = w->mc._mouse.xy.y; //%
		/* stay here until something interesting happens */
		do
			readmouse(&w->mc);
		while( w->mc._mouse.buttons==b && abs(w->mc._mouse.xy.x-x)<3 && abs(w->mc._mouse.xy.y-y)<3); //%
		w->mc._mouse.xy.x = x;	//% /* in case we're calling frselect */
		w->mc._mouse.xy.y = y; //%
		q0 = w->q0;	/* may have changed */
		q1 = w->q1;
		selectq = q0;
	}

	if( w->mc._mouse.buttons == b){  //%
	        w->_frame.scroll = framescroll; //%
		frselect(&w->_frame, &w->mc);     //%
		/* horrible botch: while asleep, may have lost selection altogether */
		if(selectq > w->nr)
		        selectq = w->org + w->_frame.p0;  //%
		w->_frame.scroll = nil;  //%
		if(selectq < w->org)
			q0 = selectq;
		else
		        q0 = w->org + w->_frame.p0;  //%
		if(selectq > w->org+w->_frame.nchars)  //%
			q1 = selectq;
		else
		        q1 = w->org+w->_frame.p1;  //%
	}

	if(q0 == q1){
		if(q0==w->q0 && clickwin==w && w->mc._mouse.msec-clickmsec<500){
			wdoubleclick(w, &q0, &q1);
			clickwin = nil;
		}else{
			clickwin = w;
			clickmsec = w->mc._mouse.msec;
		}
	}else
		clickwin = nil;

	wsetselect(w, q0, q1);
	flushimage(display, 1);
	while( w->mc._mouse.buttons){
		w->mc._mouse.msec = 0;
		b =  w->mc._mouse.buttons;
		if(b & 6){
			if(b & 2){
				wsnarf(w);
				wcut(w);
			}else{
				if(first){
					first = 0;
					getsnarf();
				}
				wpaste(w);
			}
		}
		wscrdraw(w);
		flushimage(display, 1);
		while( w->mc._mouse.buttons == b)
			readmouse(&w->mc);
		clickwin = nil;
	}
}

void
wsendctlmesg(Window *w, int type, Rectangle r, Image *image)
{
	Wctlmesg wcm;
	L_msgtag msgtag;

//	DXX(0);print("WCM{%x:%x,%x,%x}  ", &wcm, type, r.max.y, image);
	wcm.type = type;
	wcm.r = r;
	wcm.image = image;

	l_putarg(nil, WCtl, "s1", &wcm, sizeof(wcm));
	msgtag = l_send0((L_thcb*)w, INF, nil);
	//%  send(w->cctl, &wcm);

	if (L4_IpcFailed(msgtag)) IPCERR("wsendctlmesg");
}

extern L_thcb *delete_aobj;
void msg2delete_aobj(char *ss)  //%%%%%
{
    L_msgtag  msgtag;
    sleep(750); /* remove window from screen after 3/4 of a second */
    l_putarg(nil, 0, "i1", ss);
    msgtag = l_send0(delete_aobj, INF, nil);
    if (L4_IpcFailed(msgtag)) IPCERR("deletetimeoutproc");
}

int
wctlmesg(Window *w, int m, Rectangle r, Image *i)
{
	char buf[64];
//DXX(0);
	switch(m){
	default:
		error("unknown control message");
		break;
	case Wakeup:
		break;
	case Moved:
	case Reshaped:
		if(w->deleted){
			freeimage(i);
			break;
		}
		w->screenr = r;
		strcpy(buf, w->name);
		wresize(w, i, m==Moved);
		w->wctlready = 1;
// HK 20100312 begin
                print("msg2delete_aobj(%s) ", w->name); //%%% 
		msg2delete_aobj(estrdup(buf));
		// l_thread_create_args(deletetimeoutproc, 4096, nil, 1, estrdup(buf));
// HK 20100312 end
		l_yield(nil); //%
		//%	proccreate(deletetimeoutproc, estrdup(buf), 4096);
		if(Dx(r) > 0){
			if(w != input)
				wcurrent(w);
		}else if(w == input)
			wcurrent(nil);
		flushimage(display, 1);
		break;
	case Refresh:
		if(w->deleted || Dx(w->screenr)<=0 || !rectclip(&r, w->i->r))
			break;
		if(!w->mouseopen)
			wrefresh(w, r);
		flushimage(display, 1);
		break;
	case Movemouse:
		if(sweeping || !ptinrect(r.min, w->i->r))
			break;
		wmovemouse(w, r.min);
	case Rawon:
		break;
	case Rawoff:
		if(w->deleted)
			break;
		while(w->nraw > 0){
			wkeyctl(w, w->raw[0]);
			--w->nraw;
			runemove(w->raw, w->raw+1, w->nraw);
		}
		break;
	case Holdon:
	case Holdoff:
		if(w->deleted)
			break;
		wrepaint(w);
		flushimage(display, 1);
		break;
	case Deleted:
		if(w->deleted)
			break;
		write(w->notefd, "hangup", 6);
                print("msg2delete_aobj(%s) ", w->name); //%%% 
		msg2delete_aobj(estrdup(w->name));
		//l_thread_create_args(deletetimeoutproc, 4096, nil, 1, estrdup(w->name));	// HK 20100312
		//%	proccreate(deletetimeoutproc, estrdup(w->name), 4096);
		wclosewin(w);
		break;
	case Exited:
 DXX(100);
		frclear(&w->_frame, TRUE);
		close(w->notefd);
		//% chanfree(w->mc.c);	chanfree(w->ck);
		//% chanfree(w->cctl);	chanfree(w->conswrite);
		//% chanfree(w->consread); chanfree(w->mouseread);
		//% chanfree(w->wctlread);
		free(w->raw);
		free(w->r);
		free(w->dir);
		free(w->label);
		free(w);
		break;
	}
	return m;
}

/*
 * Convert back to physical coordinates
 */
void
wmovemouse(Window *w, Point p)
{
	p.x += w->screenr.min.x-w->i->r.min.x;
	p.y += w->screenr.min.y-w->i->r.min.y;
	moveto(mousectl, p);
}

void
wborder(Window *w, int type)
{
	Image *col;

	if(w->i == nil)
		return;
	if(w->holding){
		if(type == Selborder)
			col = holdcol;
		else
			col = paleholdcol;
	}else{
		if(type == Selborder)
			col = titlecol;
		else
			col = lighttitlecol;
	}

	border(w->i, w->i->r, Selborder, col, ZP);
}

Window*
wpointto(Point pt)
{
	int i;
	Window *v, *w;

	w = nil;
	for(i=0; i<nwindow; i++){
		v = window[i];
		if(ptinrect(pt, v->screenr))
		if(!v->deleted)
		if(w==nil || v->topped>w->topped)
			w = v;
	}
	return w;
}

void
wcurrent(Window *w)
{
	Window *oi;

	if(wkeyboard!=nil && w==wkeyboard)
		return;

	oi = input;
	input = w;
	if(oi!=w && oi!=nil)
		wrepaint(oi);

	if(w !=nil){
		wrepaint(w);
		wsetcursor(w, 0);
	}

	if(w != oi){
		if(oi){
			oi->wctlready = 1;
			//?KM? wsendctlmesg(oi, Wakeup, ZR, nil);
		}
		if(w){
			w->wctlready = 1;
			//?KM? wsendctlmesg(w, Wakeup, ZR, nil);
		}
	}
}

void
wsetcursor(Window *w, int force)
{
	Cursor *p;

	if(w==nil || /*w!=input || */ w->i==nil || Dx(w->screenr)<=0)
		p = nil;
	else if(wpointto(mouse->xy) == w){
		p = w->cursorp;
		if(p==nil && w->holding)
			p = &whitearrow;
	}else
		p = nil;
	if(!menuing)
		riosetcursor(p, force && !menuing);
}

void
riosetcursor(Cursor *p, int force)
{
	if(!force && p==lastcursor)
		return;
	setcursor(mousectl, p);
	lastcursor = p;
}

Window*
wtop(Point pt)
{
	Window *w;
//DXX(0);
	w = wpointto(pt);

	if(w){
	        if(w->topped == topped){ DXX(20);
			return nil;
		}

		topwindow(w->i);
		wcurrent(w);
//DXX(50);print("display<%x>  ",display);
		flushimage(display, 1);
		w->topped = ++topped;
	}
//DXX(99);
	return w;
}

void
wtopme(Window *w)
{
	if(w!=nil && w->i!=nil && !w->deleted && w->topped!=topped){
		topwindow(w->i);
		flushimage(display, 1);
		w->topped = ++ topped;
	}
}

void
wbottomme(Window *w)
{
	if(w!=nil && w->i!=nil && !w->deleted){
		bottomwindow(w->i);
		flushimage(display, 1);
		w->topped = - ++topped;
	}
}

Window*
wlookid(int id)
{
	int i;

	for(i=0; i<nwindow; i++)
		if(window[i]->id == id)
			return window[i];
	return nil;
}

void
wclosewin(Window *w)
{
	Rectangle r;
	int i;

	w->deleted = TRUE;
	if(w == input){
		input = nil;
		wsetcursor(w, 0);
	}
	if(w == wkeyboard)
		wkeyboard = nil;

	for(i=0; i<nhidden; i++)
		if(hidden[i] == w){
			--nhidden;
			memmove(hidden+i, hidden+i+1, (nhidden-i)*sizeof(hidden[0]));
			break;
		}

	for(i=0; i<nwindow; i++)
		if(window[i] == w){
			--nwindow;
			memmove(window+i, window+i+1, (nwindow-i)*sizeof(Window*));
			w->deleted = TRUE;
			r = w->i->r;
			/* move it off-screen to hide it, in case client is slow in letting it go */
			MOVEIT originwindow(w->i, r.min, view->r.max);
			freeimage(w->i);
			w->i = nil;
			return;
		}
	error("unknown window in closewin");
}

void
wsetpid(Window *w, int pid, int dolabel)
{
	char buf[128];
	int fd;

	w->pid = pid;
	if(dolabel){
		sprint(buf, "rc %d", pid);
		free(w->label);
		w->label = estrdup(buf);
	}
	sprint(buf, "/proc/%d/notepg", pid);
	fd = open(buf, OWRITE|OCEXEC);
	if(w->notefd > 0)
		close(w->notefd);
	w->notefd = fd;
}

void
winshell(void *self, void *args)  //% (void *args)
{DXX(0);
	Window *w;
	//%	Channel *pidc;
	void **arg;
	char *cmd, *dir;
	char **argv;

	arg = args;
	w = arg[0];
	//%%	pidc = arg[1];
	cmd = arg[2];
	argv = arg[3];
	dir = arg[4];

#if 1 //%--------------------------------------
        int pid;
        pid = rfork(RFNAMEG|RFFDG|RFENVG|RFPROC);

        if (pid == 0){
	  DXX(10);
	  if(filsysmount(filsys, w->id) < 0){
	    fprint(2, "mount failed: %r\n");
	    //%%    sendul(pidc, 0);
	    l_thread_exits("mount failed");  //%
	  }
	  close(0);
	  DXX(20);
	  if(open("/dev/cons", OREAD) < 0){
	    fprint(2, "can't open /dev/cons: %r\n");
	    //%%    sendul(pidc, 0);
	    l_thread_exits("/dev/cons");  //%
	  }
	  close(1);
	  DXX(30);
	  if(open("/dev/cons", OWRITE) < 0){
	    fprint(2, "can't open /dev/cons: %r\n");
	    //%%    sendul(pidc, 0);
	    l_thread_exits("open");  //%  /* BUG? was terminate() */
	  }
	  if(wclose(w) == 0){ /* remove extra ref hanging from creation */
	    notify(nil);
	    dup(1, 2);
	    if(dir)
	      chdir(dir);
	    print("winshell<dir=%s cmd:%s> \n", dir, cmd);
	    exec(cmd, argv);
	    //%%    _exits("exec failed");
	  }
        }

#else //%original---------------------------------------

	rfork(RFNAMEG|RFFDG|RFENVG);

	if(filsysmount(filsys, w->id) < 0){
		fprint(2, "mount failed: %r\n");
		//%%	sendul(pidc, 0);
		l_thread_exits("mount failed");  //%
	}
	close(0);

	if(open("/dev/cons", OREAD) < 0){
		fprint(2, "can't open /dev/cons: %r\n");
		//%%	sendul(pidc, 0);
		l_thread_exits("/dev/cons");  //%
	}
	close(1);

	if(open("/dev/cons", OWRITE) < 0){
		fprint(2, "can't open /dev/cons: %r\n");
		//%%	sendul(pidc, 0);
		l_thread_exits("open");	 //%  /* BUG? was terminate() */
	}
	if(wclose(w) == 0){	/* remove extra ref hanging from creation */
		notify(nil);
		dup(1, 2);
		if(dir)
			chdir(dir);
		//%%	procexec(pidc, cmd, argv);
		//%%	_exits("exec failed");
	}
DXX(99);
#endif //%--------------------------------------------
}

static Rune left1[] =  { L'{', L'[', L'(', L'<', L'«', 0 };
static Rune right1[] = { L'}', L']', L')', L'>', L'»', 0 };
static Rune left2[] =  { L'\n', 0 };
static Rune left3[] =  { L'\'', L'"', L'`', 0 };

Rune *left[] = {
	left1,
	left2,
	left3,
	nil
};
Rune *right[] = {
	right1,
	left2,
	left3,
	nil
};

void
wdoubleclick(Window *w, uint *q0, uint *q1)
{
	int c, i;
	Rune *r, *l, *p;
	uint q;

	for(i=0; left[i]!=nil; i++){
		q = *q0;
		l = left[i];
		r = right[i];
		/* try matching character to left, looking right */
		if(q == 0)
			c = '\n';
		else
			c = w->r[q-1];
		p = strrune(l, c);
		if(p != nil){
			if(wclickmatch(w, c, r[p-l], 1, &q))
				*q1 = q-(c!='\n');
			return;
		}
		/* try matching character to right, looking left */
		if(q == w->nr)
			c = '\n';
		else
			c = w->r[q];
		p = strrune(r, c);
		if(p != nil){
			if(wclickmatch(w, c, l[p-r], -1, &q)){
				*q1 = *q0+(*q0<w->nr && c=='\n');
				*q0 = q;
				if(c!='\n' || q!=0 || w->r[0]=='\n')
					(*q0)++;
			}
			return;
		}
	}
	/* try filling out word to right */
	while(*q1<w->nr && isalnum(w->r[*q1]))
		(*q1)++;
	/* try filling out word to left */
	while(*q0>0 && isalnum(w->r[*q0-1]))
		(*q0)--;
}

int
wclickmatch(Window *w, int cl, int cr, int dir, uint *q)
{
	Rune c;
	int nest;

	nest = 1;
	for(;;){
		if(dir > 0){
			if(*q == w->nr)
				break;
			c = w->r[*q];
			(*q)++;
		}else{
			if(*q == 0)
				break;
			(*q)--;
			c = w->r[*q];
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}


uint
wbacknl(Window *w, uint p, uint n)
{
	int i, j;

	/* look for start of this line if n==0 */
	if(n==0 && p>0 && w->r[p-1]!='\n')
		n = 1;
	i = n;
	while(i-->0 && p>0){
		--p;	/* it's at a newline now; back over it */
		if(p == 0)
			break;
		/* at 128 chars, call it a line anyway */
		for(j=128; --j>0 && p>0; p--)
			if(w->r[p-1]=='\n')
				break;
	}
	return p;
}

void
wshow(Window *w, uint q0)
{
	int qe;
	int nl;
	uint q;

	qe = w->org+w->_frame.nchars;
	if(w->org<=q0 && (q0<qe || (q0==qe && qe==w->nr)))
		wscrdraw(w);
	else{
		nl = 4*w->_frame.maxlines/5;
		q = wbacknl(w, q0, nl);
		/* avoid going backwards if trying to go forwards - long lines! */
		if(!(q0>w->org && q<w->org))
			wsetorigin(w, q, TRUE);
		while(q0 > w->org+w->_frame.nchars)
			wsetorigin(w, w->org+1, FALSE);
	}
}

void
wsetorigin(Window *w, uint org, int exact)
{
	int i, a, fixup;
	Rune *r;
	uint n;

	if(org>0 && !exact){
		/* org is an estimate of the char posn; find a newline */
		/* don't try harder than 256 chars */
		for(i=0; i<256 && org<w->nr; i++){
			if(w->r[org] == '\n'){
				org++;
				break;
			}
			org++;
		}
	}
	a = org-w->org;
	fixup = 0;
	if(a>=0 && a<w->_frame.nchars){
		frdelete(&w->_frame, 0, a);
		fixup = 1; /* frdelete can leave end of last line in wrong selection mode; it doesn't know what follows */
	}else if(a<0 && -a<w->_frame.nchars){
		n = w->org - org;
		r = runemalloc(n);
		runemove(r, w->r+org, n);
		frinsert(&w->_frame, r, r+n, 0);
		free(r);
	}else
		frdelete(&w->_frame, 0, w->_frame.nchars);
	w->org = org;
	wfill(w);
	wscrdraw(w);
	wsetselect(w, w->q0, w->q1);
	if(fixup && w->_frame.p1 > w->_frame.p0)
		frdrawsel(&w->_frame, frptofchar(&w->_frame, w->_frame.p1-1), w->_frame.p1-1, w->_frame.p1, 1);
}

void
wsetselect(Window *w, uint q0, uint q1)
{
	int p0, p1;

	/* w->_frame.p0 and w->_frame.p1 are always right; w->q0 and w->q1 may be off */
	w->q0 = q0;
	w->q1 = q1;
	/* compute desired p0,p1 from q0,q1 */
	p0 = q0-w->org;
	p1 = q1-w->org;
	if(p0 < 0)
		p0 = 0;
	if(p1 < 0)
		p1 = 0;
	if(p0 > w->_frame.nchars)
		p0 = w->_frame.nchars;
	if(p1 > w->_frame.nchars)
		p1 = w->_frame.nchars;
	if(p0==w->_frame.p0 && p1==w->_frame.p1)
		return;
	/* screen disagrees with desired selection */
	if(w->_frame.p1<=p0 || p1<=w->_frame.p0 || p0==p1 || w->_frame.p1==w->_frame.p0){
		/* no overlap or too easy to bother trying */
		frdrawsel(&w->_frame, frptofchar(&w->_frame, w->_frame.p0), w->_frame.p0, w->_frame.p1, 0);
		frdrawsel(&w->_frame, frptofchar(&w->_frame, p0), p0, p1, 1);
		goto Return;
	}
	/* overlap; avoid unnecessary painting */
	if(p0 < w->_frame.p0){
		/* extend selection backwards */
		frdrawsel(&w->_frame, frptofchar(&w->_frame, p0), p0, w->_frame.p0, 1);
	}else if(p0 > w->_frame.p0){
		/* trim first part of selection */
		frdrawsel(&w->_frame, frptofchar(&w->_frame, w->_frame.p0), w->_frame.p0, p0, 0);
	}
	if(p1 > w->_frame.p1){
		/* extend selection forwards */
		frdrawsel(&w->_frame, frptofchar(&w->_frame, w->_frame.p1), w->_frame.p1, p1, 1);
	}else if(p1 < w->_frame.p1){
		/* trim last part of selection */
		frdrawsel(&w->_frame, frptofchar(&w->_frame, p1), p1, w->_frame.p1, 0);
	}

    Return:
	w->_frame.p0 = p0;
	w->_frame.p1 = p1;
}

uint
winsert(Window *w, Rune *r, int n, uint q0)
{
	uint m;

	if(n == 0)
		return q0;
	if(w->nr+n>HiWater && q0>=w->org && q0>=w->qh){
		m = min(HiWater-LoWater, min(w->org, w->qh));
		w->org -= m;
		w->qh -= m;
		if(w->q0 > m)
			w->q0 -= m;
		else
			w->q0 = 0;
		if(w->q1 > m)
			w->q1 -= m;
		else
			w->q1 = 0;
		w->nr -= m;
		runemove(w->r, w->r+m, w->nr);
		q0 -= m;
	}
	if(w->nr+n > w->maxr){
		/*
		 * Minimize realloc breakage:
		 *	Allocate at least MinWater
		 * 	Double allocation size each time
		 *	But don't go much above HiWater
		 */
		m = max(min(2*(w->nr+n), HiWater), w->nr+n)+MinWater;
		if(m > HiWater)
			m = max(HiWater+MinWater, w->nr+n);
		if(m > w->maxr){
			w->r = runerealloc(w->r, m);
			w->maxr = m;
		}
	}
	runemove(w->r+q0+n, w->r+q0, w->nr-q0);
	runemove(w->r+q0, r, n);
	w->nr += n;
	/* if output touches, advance selection, not qh; works best for keyboard and output */
	if(q0 <= w->q1)
		w->q1 += n;
	if(q0 <= w->q0)
		w->q0 += n;
	if(q0 < w->qh)
		w->qh += n;
	if(q0 < w->org)
		w->org += n;
	else if(q0 <= w->org+w->_frame.nchars)
		frinsert(&w->_frame, r, r+n, q0-w->org);
	return q0;
}

void
wfill(Window *w)
{
	Rune *rp;
	int i, n, m, nl;

	if(w->_frame.lastlinefull) //%
		return;
	rp = malloc(messagesize);
	do{
		n = w->nr-(w->org+w->_frame.nchars);
		if(n == 0)
			break;
		if(n > 2000)	/* educated guess at reasonable amount */
			n = 2000;
		runemove(rp, w->r+(w->org+w->_frame.nchars), n);
		/*
		 * it's expensive to frinsert more than we need, so
		 * count newlines.
		 */
		nl = w->_frame.maxlines - w->_frame.nlines;
		m = 0;
		for(i=0; i<n; ){
			if(rp[i++] == '\n'){
				m++;
				if(m >= nl)
					break;
			}
		}
		frinsert(&w->_frame, rp, rp+i, w->_frame.nchars);
	}while(w->_frame.lastlinefull == FALSE); //%
	free(rp);
}

char*
wcontents(Window *w, int *ip)
{
	return runetobyte(w->r, w->nr, ip);
}
