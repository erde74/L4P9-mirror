#include <l4all.h>      // HK 20091031
#include <l_actobj.h> // HK 20091130
#include <u.h>
#include <libc.h>
#include <draw.h>
//#include <thread.h> // HK 20091130
#include <cursor.h>
#include <mouse.h>


#define DYY(n) {static int cnt=0; if((++cnt)%(1024*n)==0)print("\n %s:HEAVY-RUN\n", __FUNCTION__);}

void
moveto(Mousectl *m, Point pt)
{
	fprint(m->mfd, "m%d %d", pt.x, pt.y);
	m->_mouse.xy = pt;	// HK 20091130
}

void
closemouse(Mousectl *mc)
{
	L_mbuf *mbuf;		// HK 20091130

	if(mc == nil)
		return;

	postnote(PNPROC, mc->pid, "kill");

	// HK 20091130 begin

//	do; while(nbrecv(mc->c, &mc->_mouse) > 0);
	do {
		l_arecv0(mc->mbox, 0 /* no-wait */, &mbuf);
		if (mbuf != nil) free(mbuf);	// HK 20100131
	} while (mbuf != nil);

	l_thread_kill(&mc->inherit_thcb);
	close(mc->mfd);
	close(mc->cfd);
	free(mc->file);
	l_thread_kill(mc->mbox);
	l_thread_kill(mc->resizec);
	free(mc);
	return;

	// HK 20091130 end
}

int
readmouse(Mousectl *mc)
{
	L_mbuf *mbuf;	// HK 20091031
	int n;		// HK 20091130
	if(mc->image)
		flushimage(mc->image->display, 1);

	// HK 20091130 begin

//	if(recv(mc->c, &mc->_mouse) < 0){
//		fprint(2, "readmouse: %r\n");
//		return -1;
//	}

	l_arecv0(mc->mbox, INF, &mbuf);
	n = sizeof (mc->_mouse);
	l_getarg(mbuf, "s1", &mc->_mouse, &n);
	if (mbuf != nil) free(mbuf);	// HK 20100131
	if (n != sizeof (mc->_mouse)) {
		fprint(2, "readmouse: %r\n");
		return -1;
	}

	// HK 20091130 end
	return 0;
}

static
void
_ioproc(/* void *arg */)	// HK 20091130
{
	int n, nerr /* , one */;	// HK 20091031
	char buf[1+5*12];
	Mouse m;
	Mousectl *mc;
	L_mbuf *mbuf;	//% HK 20091130

	// HK 20091130 begin

//	mc = arg;
//	threadsetname("mouseproc");
//	one = 1;
	mc = L_MYOBJ(Mousectl *);
	l_thread_setname("Mousectl-actobj");
	mc->pid = getpid();
	memset(&m, 0, sizeof m);

	nerr = 0;
	for(;;){
DYY(1);
		n = read(mc->mfd, buf, sizeof buf);
		if(n != 1+4*12){
		//	yield();	/* if error is due to exiting, we'll exit here */	// HK 20091031
			l_yield(nil);	// HK 20091130
			if (n < 0)								// HK 20091231
				sleep(10);							// HK 20091231
			else									// HK 20091231
				fprint(2, "mouse: bad count %d not 49: %r\n", n);
			if(/* n<0 || */ ++nerr>10) {		// HK 20091231
			//	threadexits("read error");		// HK 20091130
				l_thread_exits("read error");	// HK 20091130
			}
			continue;
		}
		nerr = 0;
		switch(buf[0]){
		case 'r':
			// HK 20091130 begin
			// send(mc->resizec, &one);
	        	mbuf = (L_mbuf*) malloc(sizeof(L_mbuf)); // HK 20100131
		        l_putarg(mbuf, 16 /* mlabel */, "i1", 1); //%
			l_asend0(mc->mbox, 0 /* no-wait */, mbuf); //%
			// HK 20091130 end
			/* fall through */
		case 'm':
			m.xy.x = atoi(buf+1+0*12);
			m.xy.y = atoi(buf+1+1*12);
			m.buttons = atoi(buf+1+2*12);
			m.msec = atoi(buf+1+3*12);
			// HK 20091231 begin
			// send(mc->c, &m);
	        	mbuf = (L_mbuf*) malloc(sizeof(L_mbuf)); // HK 20100131
			l_putarg(mbuf, 17 /* mlabel */, "s1", &m, sizeof (m)); //%
			l_asend0(mc->mbox, 0 /* no-wait */, mbuf); //%
			// HK 20091231 end
			/*
			 * mc->Mouse is updated after send so it doesn't have wrong value if we block during send.
			 * This means that programs should receive into mc->Mouse (see readmouse() above) if
			 * they want full synchrony.
			 */
			mc->_mouse = m;	// HK 20091031
			break;
		}
	}
}

Mousectl*
initmouse(char *file, Image *i)
{
	Mousectl *mc;	// HK 20091130
	char *t = nil, *sl;

	// HK 20091231 begin

	if ((mc = malloc(sizeof (Mousectl))) != nil) {
		if(file == nil)
			file = "/dev/mouse";
		mc->mfd = open(file, ORDWR|OCEXEC);
		t = malloc(strlen(file)+16);
		mc->cfd = -1;
		mc->file = strdup(file);
		mc->mbox = l_mbox_create("mouse_mbox");
		mc->resizec = l_mbox_create("mouse_resize_mbox");
		if (mc->mfd < 0 && strcmp(file, "/dev/mouse") == 0) {
			bind("#m", "/dev", MAFTER);
			mc->mfd = open(file, ORDWR|OCEXEC);
		}
		if (mc->mfd >= 0 && t != nil && mc->file != nil && mc->mbox != nil && mc->resizec != nil) {
			strcpy(t, file);
			sl = utfrrune(t, '/');
			if (sl)
				strcpy(sl, "/cursor");
			else
				strcpy(t, "/dev/cursor");
			mc->cfd = open(t, ORDWR|OCEXEC);
			if (mc->cfd < 0)
				goto Error;
			memset(&mc->_mouse, 0, sizeof (Mouse));
			mc->image = i;
			if (l_thread_create(_ioproc, 4096, mc) == nil)
				goto Error;
		} else {
Error:
			if (mc->mfd >= 0)
				close(mc->mfd);
			if (mc->cfd >= 0)
				close(mc->cfd);
			if (mc->file != nil)
				free(mc->file);
			if (mc->mbox != nil)
				l_thread_kill(mc->mbox);
			if (mc->resizec != nil)
				l_thread_kill(mc->resizec);
			free(mc);
			mc = nil;
		}
	}
	if (t != nil)
		free(t);
	return mc;

//	mc->c = chancreate(sizeof(Mouse), 0);
//	mc->resizec = chancreate(sizeof(int), 2);
//	proccreate(_ioproc, mc, 4096);

	// HK 20091231 end
}

void
setcursor(Mousectl *mc, Cursor *c)
{
	char curs[2*4+2*2*16];

	if(c == nil)
		write(mc->cfd, curs, 0);
	else{
		BPLONG(curs+0*4, c->offset.x);
		BPLONG(curs+1*4, c->offset.y);
		memmove(curs+2*4, c->clr, 2*2*16);
		write(mc->cfd, curs, sizeof curs);
	}
}

