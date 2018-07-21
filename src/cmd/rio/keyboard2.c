#include <l4all.h>      // HK 20091031
#include <lp49/l_actobj.h> // HK 20091130
#include <u.h>
#include <libc.h>
#include <draw.h>
//#include <thread.h> // HK 20091130
#include <keyboard.h>

#include <cursor.h>   //%
//#include <thread.h>   // HK 20100131%
#include <mouse.h>    //%
#include <frame.h>    //%
#include <fcall.h>    //%
#include "dat.h"      //%


#define SENDP(dest, mlabel, val)  l_send0(dest, INF, l_putarg(nil, mlabel, "i1", val))
// #define  DXX(n) {print("%s  %s:%d ", (n==0)?"\n":"", __FUNCTION__, n); l_yield(nil);}

void
closekeyboard(Keyboardctl *kc)
{
	if(kc == nil)
		return;

	postnote(PNPROC, kc->pid, "kill");

	l_thread_kill(&kc->inherit_thcb);	// HK 20091130
	close(kc->ctlfd);
	close(kc->consfd);
	free(kc->file);
	l_thread_kill(kc->mbox);
	free(kc);
	return;
}

#if 1 //%---------------------------------------------
static void _ioproc(Keyboardctl *kc)
{
	int    m;
	int    nx;  // 0 or 1
	char   buf[10];
	char   runebuf[10];
	Rune   r;
	int    i, j, k;
	L_msgtag  msgtag;

	l_thread_setname("Keyboardctl-actobj");
	kc->pid = getpid();
	l_post_ready(0); //%

	for(;;){
	        m = read(kc->consfd, buf, sizeof buf);
		if(m <= 0){
			l_yield(nil);	
			fprint(2, "keyboard read error: %r\n");
			l_thread_exits("error");
		}

		for (i=0, j=0; j<m; i++){
		    k = chartorune(&runebuf[i], &buf[j]);
		    j += k;
		}
		runebuf[i] = (Rune)L'\0';

print("%s", buf);

                if(input){
		    l_putarg(nil, WKey, "s1", runebuf, sizeof(runebuf));
		    msgtag = l_send0((L_thcb*)input, INF, nil);
		    if (L4_IpcFailed(msgtag))
		      print ("Keyboard2:send() failed:%d \n", L4_ErrorCode());
		}
	}
}

#else //%--------------------------------------------
static
void
_ioproc(Keyboardctl *kc /* void *arg */)	// HK 20091130
{
	int    m;
	int    nx;  // 0 or 1
	char   buf[2][20];
	char   runebuf[2][20];
	Rune   r;
	int    i, j, k;
	//%	Keyboardctl *kc;
	L_msgtag  msgtag;

//	kc = L_MYOBJ(Keyboardctl *);
	l_thread_setname("keyboard2thread");
	kc->pid = getpid();

	for(nx = 0;; nx ^= 1){
	        m = read(kc->consfd, buf[nx], sizeof buf[0]);
		if(m <= 0){
			l_yield(nil);	
			fprint(2, "keyboard read error: %r\n");
			l_thread_exits("error");
		}
		for (i=0, j=0; j<m; i++){
		    k = chartorune(&runebuf[nx][i], &buf[nx][j]);
		    j += k;
		}
		runebuf[nx][i] = L'\0';

print("KBD<%s>  ", buf[nx]);
for(j=0; j<i; j++) print("<%x:%c> ", input, runebuf[nx][j]);

                if(input){
		    print("Keyboard2:send(%x, WKey)  ", input); 

		    l_putarg(nil, WKey, "i1", &runebuf[nx]);
		    msgtag = l_send0((L_thcb*)input, INF, nil);
                    //  msgtag = SENDP((L_thcb*)input, WKey,  &runebuf[nx]);
		    if (L4_IpcFailed(msgtag))
		      print ("Keyboard2:send() failed:%d \n", L4_ErrorCode());
		    // l_putarg(nil, WKey, "i1", &buf[nx]);
		    // l_send0((L_thcb*)input, INF, nil);
		}
	}
}
#endif //%-------------------------------------------

Keyboardctl*
initkeyboard(char *file)
{
	Keyboardctl *kc;
	char *t = nil;
	L_thcb  *kbd_thcb;

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

			kbd_thcb = l_thread_create(_ioproc, 4096, kc);  //KM
			if (kbd_thcb){
			        l_yield(kbd_thcb);
				l_wait_ready(kbd_thcb);  //%
			}else
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

