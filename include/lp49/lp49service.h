#ifndef __LP49SERVICE_H__
#define __LP49SERVICE_H__ 

#include  <u.h>
#include  <l4all.h> 
#include  <lp49/lp49.h>


/* Stack area
 *
 *    +---------+-----------++----------------------------------------+-+
 *    | L4_atcb |   udata   ||                           stack  <---- | |
 *    +---------+-----------++----------------------------------------+-+
 *    :                      :                                        :
 *                         stklimit                                   stktop
 *
 *     
 */


typedef struct L4_atcb L4_atcb;
struct   L4_atcb{
    L4_ThreadId_t  tid;
    char  * name;
    uint    stklimit;
    uint    stktop;
    L4_atcb  * next;
    char    state;
    char    malloced;
    char    _x;
    char    _y;
};


enum {
  L4TH_NONE   = 0,
  L4TH_ASIGND  = 1,
}; 

//--------------------------------------------------------------

L4_atcb* l4_thread_create(char *name, void *stkarea, int stkareasize, int udsize, 
		      void (*func)());
L4_atcb* l4_thread_create_args(char *name, void *stkarea, int stkareasize, int udsize, 
			   void (*func)(), int argc, ...);

int  l4_thread_kill(L4_ThreadId_t  tid);
void l4_thread_exits(char *exitstr);
void l4_thread_killall();
void l4_thread_exitsall(char *exitstr);

void l4_sleep_ms(unsigned  ms);

int  l4_stkmargin0(uint stklimit);
int  l4_stkmargin() ;

int  my_procnr();

//---------------------------------------------------------------

int  l4_send_intn(L4_ThreadId_t dest, int label, int argcnt, ...);
// Ex. rc = l4_send_intn(tid, LABEL, 3, i, j, k);

int  l4_call_intn(L4_ThreadId_t dest, int *reply, 
		 int label, int argcnt, ...);
// Ex. rc = l4_call_intn(tid, &r, LABEL, 3, i, j, k);

int  l4_recvfrom_intn(L4_ThreadId_t from, int *label, int argcnt, ...);
// Ex. rc = l4_recvfrom(tid, &lbl, 2, &x, &y);

int  l4_recv_intn(L4_ThreadId_t *from, int *label, int argcnt, ...);
// Ex. rc = l4_recv_intn(&tid, &lbl, 2, &x, &y);

int  l4_send_str_intn(L4_ThreadId_t dest, int label, 
		     char *buf, int len, int argcnt, ...);
// Ex. rc = l4_send_str_intn(tid, LABEL, buf, len, 3, i, j, k);

int  l4_recv_str_intn(L4_ThreadId_t *from, int *label, 
		     char *buf, int len, int argcnt, ...);
// Ex. rc = l4_recv_str_intn(&tid, &lbl, 2, &x, &y);

int  l4_recvfrom_str_intn(L4_ThreadId_t from, int *label, 
			 char *buf, int len, int argcnt, ...);
// Ex. rc = l4_recv_str_intn(&tid, &lbl, 2, &x, &y);


#endif // __LP49SERVICE_H__ 
