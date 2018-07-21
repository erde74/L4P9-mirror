#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>
#include  <u.h>
#include  <libc.h>

//---------------------------------------------------

#define  PR  print
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

void print_clocks(char  which, int diff)
{
    int  i;
    uvlong delta;  // unsigned long long 
    uint   stime, usec, msec, sec;
    static firsttime = 1;
    
    if (firsttime) {
        what = which;
	elapse_inx = 0;
	firsttime = 0;
	PR("DeltaTime('%c')  \n", which);
	return;
    }
    PR("DeltaTime('%c')  \t", which);
    for (i = 0; i < elapse_inx; i++){
        if(diff == 1){
	    if (i == 0) {
	        // PR("{0:0:0} \t%s \t", elapse[i].msg); 
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

