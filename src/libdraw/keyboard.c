#include <l4all.h>      // HK 20091031
#include <u.h>
#include <libc.h>
#include <draw.h>
//#include <thread.h> // HK 20091130
#include <l_actobj.h> // HK 20091130
#include <keyboard.h>

void
closekeyboard(Keyboardctl *kc)
{
	if(kc == nil)
		return;

	postnote(PNPROC, kc->pid, "kill");

#ifdef BUG
	/* Drain the channel */
	while(?kc->c)
		<-kc->c;
#endif

	l_thread_kill(&kc->inherit_thcb);	// HK 20091130
	close(kc->ctlfd);
	close(kc->consfd);
	free(kc->file);
	l_thread_kill(kc->mbox);
	free(kc);
	return;
}

static
void
_ioproc(/* void *arg */)	// HK 20091130
{
	int m, n;
	char buf[20];
	Rune r;
	Keyboardctl *kc;

	// HK 20091130 begin

//	kc = arg;
//	threadsetname("kbdproc");
	kc = L_MYOBJ(Keyboardctl *);
	l_thread_setname("kbdproc");
	kc->pid = getpid();

	n = 0;
	for(;;){
		L_mbuf mbuf;	// HK 20091130
		while(n>0 && fullrune(buf, n)){
			m = chartorune(&r, buf);
			n -= m;
			memmove(buf, buf+m, n);
			// HK 20091130 begin
			// send(kc->c, &r);
			l_putarg(&mbuf, 16 /* mlabel */, "s1", &r, sizeof (r));
			l_asend0(kc->mbox, 0 /* no-wait */, &mbuf);
			// HK 20091130 end
		}
		m = read(kc->consfd, buf+n, sizeof buf-n);
		if(m <= 0){
			// yield();	/* if error is due to exiting, we'll exit here */	// HK 20091130
			l_yield(nil);		// HK 20091130
			fprint(2, "keyboard read error: %r\n");
			l_thread_exits("error");	// HK 20091130
		}
		n += m;
	}
}

Keyboardctl*
initkeyboard(char *file)
{
	Keyboardctl *kc;
	char *t = nil;

	// HK 20091231 begin

	if ((kc = malloc(sizeof (Keyboardctl))) != nil) {
		if (file == nil)
			file = "/dev/cons";
		kc->consfd = open(file, ORDWR|OCEXEC);
		kc->ctlfd = -1;
		kc->file = nil;
		t = malloc(strlen(file) + 16);
		kc->file = strdup(file);
		kc->mbox = l_mbox_create("kbd_mbox");
		if (kc->consfd >= 0 && t != nil && kc->file != nil && kc->mbox != nil) {
			sprint(t, "%sctl", file);
			kc->ctlfd = open(t, OWRITE|OCEXEC);
			if (kc->ctlfd < 0) {
				fprint(2, "initkeyboard: can't open %s: %r\n", t);
				goto Error;
			}
			if (write(kc->ctlfd, "rawon", 5) < 0) {
				fprint(2, "initkeyboard: can't turn on raw mode on %s: %r\n", t);
				goto Error;
			}
			if (l_thread_create(_ioproc, 4096, kc) == nil)
				goto Error;
		} else {
Error:
			if (kc->consfd >= 0)
				close(kc->consfd);
			if (kc->ctlfd >= 0)
				close(kc->ctlfd);
			if (kc->file != nil)
				free(kc->file);
			if (kc->mbox != nil)
				l_thread_kill(kc->mbox);
			free(kc);
			kc = nil;
		}
	}
	if (t != nil)
		free(t);
	return kc;

	// HK 20091231 end
}

int
ctlkeyboard(Keyboardctl *kc, char *m)
{
	return write(kc->ctlfd, m, strlen(m));
}

