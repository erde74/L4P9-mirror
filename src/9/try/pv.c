/*
 * Semaphore, worker pool
 */

#include <l4all.h>
#include <l4/l4io.h>
#include  <u.h>

//============================================
void l4_send_arg_0(unsigned short label)
{
  L4_MsgTag_t    tag;

  tag.raw = 0; tag.X.label = label; tag.X.u = 0;
  L4_LoadMR(0, tag.raw);
}

void l4_send_arg_1(unsigned short label, int arg)
{
  L4_MsgTag_t    tag;

  tag.raw = 0; tag.X.label = label; tag.X.u = 1;
  L4_LoadMR(0, tag.raw);
  L4_LoadMR(1, arg);
}

void l4_send_args(unsigned short label, int num, ...)
{
  L4_MsgTag_t    tag;
  va_list  ap;
  int   val, i;

  va_start(ap, num);
  tag.raw = 0; tag.X.label = label; tag.X.u = num;
  L4_LoadMR(0, tag.raw);
  for (i = 0; i<num; i++) {
    val = va_arg(ap, int);
    L4_LoadMR(1+i, val);
  }
  va_end(ap);
}

//------------------------
int l4_recv_arg_1(L4_MsgTag_t tag, int *arg)
{
  if (L4_UntypedWords(tag) < 1)  
    return 0;
  
  L4_StoreMR(1, (L4_Word_t*)*arg);
  return 1;
}

int l4_recv_arg_2(L4_MsgTag_t tag, int *arg1, int *arg2)
{
  if (L4_UntypedWords(tag) < 2)  return 0;

  L4_StoreMR(1, (L4_Word_t*)*arg1);
  L4_StoreMR(2, (L4_Word_t*)*arg2);
  return 1;
}

int l4_recv_args(L4_MsgTag_t tag, int num, ...)
{
  va_list  ap;
  L4_Word_t *adrs;
  int   i;

  if (num >= L4_UntypedWords(tag))  return 0;

  va_start(ap, num);
  for (i = 0; i<num; i++) {
    adrs = va_arg(ap, L4_Word_t *);
    L4_StoreMR(1+i, adrs);
  }
  va_end(ap);
  return 1;
}


//----------- Semaphore -----------------------
#define  P_op  1
#define  V_op  2 
#define  GO    3
#define  N     8

typedef struct Semaphore {
  int  cnt;
  L4_ThreadId_t  que[64];
  int  inx, outx, num;
} Semaphore_t;

void sem_init(Semaphore_t  *sem, int n)
{
  sem->cnt = n;
  sem->inx = sem->outx = sem->num = 0;
}

void sema_thread( )
{
  Semaphore_t  *sem;
  int  op;
  L4_ThreadId_t  client;
  L4_MsgTag_t    tag;

  while(1){
      tag = L4_Wait(&client);
      op = L4_Label(tag);
      l4_recv_args(tag, 1, (L4_Word_t*)&sem);

      switch(op) {
      case P_op: 
	sem->cnt --;
	if (sem->cnt > 0) {
	    l4_send_args(GO, 0);
	    L4_Reply(client);
	}
	else {
	    sem->que[sem->inx++] = client;
	    sem->inx = sem->inx % 64;
	    sem->num++;
	    if (sem->num == 64) 
	        L4_KDB_Enter("!!!!!");
	}
	continue;

      case V_op:
	sem->cnt ++;
	if (sem->cnt < 0) {
	  client = sem->que[sem->outx++];
	  sem->outx = sem->outx % 64;
	  sem->num--;
	  l4_send_args(GO, 0);
	  L4_Reply(client);
	}
	continue;
      }
  }
}


/******************
Semaphore_t   sem1, sem2;

sem_init(&sem1, 4);

l4_send_args(P_op, 1, sem1);
L4_Call(semthread);

l4_send_args(V_op, 1, sem1);
L4_Call(semthread);

**************************/

//================================================
#define WORKERALLOC  0
#define WORKERFREE   1

typedef struct Worker Worker_t;
struct Worker {
  unsigned       stk[1024-4-10-128];
  unsigned       stktop[4];
  //- - - - - - - - -
  Worker_t      *next;
  L4_ThreadId_t  thid;
  L4_ThreadId_t  client;
  unsigned       pattern;
  int            args[6];
  char           string[512];
} ;


typedef struct Workerpool Workerpool_t;
struct Workerpool{
  Worker_t  *head;
  Worker_t  *tail;
  int  inx, outx, num;
  L4_ThreadId_t  que[64];

} workerpool = {0, 0, 0, 0, 0 } ;

void workerpool_init(Workerpool_t *wpool)
{
  wpool->head = wpool->tail = (Worker_t*)0;
  wpool->inx = wpool->outx = wpool->num = 0;
}


void workeralloc_thread( )
{
  Workerpool_t  *wpool;
  Worker_t      *w;
  int            op;
  L4_ThreadId_t  client;
  L4_MsgTag_t    tag;

  while(1){
      tag = L4_Wait(&client);

      op = L4_Label(tag);

      switch(op) {
      case WORKERALLOC: 
	l4_recv_args(tag, 1, (int*)&wpool);

	if (wpool->head) {
	  w = wpool->head;
	  wpool->head = w->next;
	  if (wpool->head == 0)
            wpool->tail = 0;

	  l4_send_args(GO, 1, (int)w);
	  L4_Reply(client);
	}
	else { 
	  wpool->que[wpool->inx++] = client;
	  wpool->inx = wpool->inx % 64;
	  wpool->num++;
	  if (wpool->num == 64) 
	    L4_KDB_Enter("!!!!!");
	}
	continue;

      case WORKERFREE:
	l4_recv_args(tag, 2, (int*)&wpool, (int*)&w);
	if (wpool->num > 0) {
	    client = wpool->que[wpool->outx++];
	    wpool->outx = wpool->outx % 64;
	    wpool->num--;
	    l4_send_args(GO, 1, w);
	    L4_Reply(client);
	}
	else {
	    w->next = 0;
	    if (wpool->head == 0)
	       wpool->head = w;
	    else
	        wpool->tail->next = w;
	    wpool->tail = w;
	}

	continue;
      }
  }
}

/************************************

  Workerpool_t wpool_1;

  workerpool_init(&wpool_1);

  l4_send_args(WORKERALLOC, 0);
  tag = L4_Call(workeralloc_thread);
  l4_recv_args(tag, 1, &wp);


  l4_send_args(WORKERFREE, 0);
  L4_Call(workeralloc_thread);
                                                                               
************************************/


//================================================
int main()
{
    int  i;
    l4printf_r("==== Guten morgen! [%X] ====\n", L4_Myself().raw);


    for (i = 0; i<50; i++) {

	L4_Sleep(L4_TimePeriod(1000000));


	L4_Sleep(L4_TimePeriod(1000000));

	L4_Sleep(L4_TimePeriod(1000000));
    }
    l4printf_r("\n");

    L4_KDB_Enter ("Hello debugger!");
    //	tag = L4_Send (new_thread->tid);
    for (;;);
};










#if 0 //============================================
void l4_loadMRs(unsigned short label, char fmt[], ...)
{
  L4_MsgTag_t    tag;
  int  n = (len + 3)/4;
  int  i, inx = 1, len;
  unsigned  pattern = 0;
  char ch;
  int  pos = 0;
  va_list  ap;

  va_start(ap, char*);
  
  for (i = 0; ; i++) {
    ch = fmt[i];   
    if (ch == '\0') break;
    
    switch(ch) {
    case 'i': case 'd': 
      pattern |= 1 << pos;
      pos += 4;
      val = va_arg(ap, int);
      L4_LoadMR(inx++, val);
      break;

    case 'a':
      pattern |= 2 << pos;
      pos += 4;
      val = va_arg(ap, int);
      L4_LoadMR(inx++, val);
      break;

    case 's':
      pattern |= 3 << pos;
      pos += 4;
      cp = va_arg(ap, char*);
      len = strlen(cp) + 1;
      L4_LoadMR(inx++, val);

      break;

    case 't':
      len = fmt[i++]*16 + fmt[i++];
      pattern |= 4 << pos;
      pos += 4;
      pattern |= len << pos;
      pos +=8;
      break;

    default:
      l4_printf("ERR l4_loadMRs \n");
    }

  }

  tag.raw = 0; tag.X.label = label; tag.X.u = 4;
  L4_LoadMR(0, tag.raw);
  L4_LoadMRs(1, n, data);
}

int getMRs(int len, int *actuallen, int data[])
{
  L4_MsgTag_t    tag;
  int  n;
  L4_LoadMR(0, (L4_Word_t*)&tag.raw);
  n = tag.X.u;
  if (n*4 > len) n = (len+3)/4;
  *actuallen = n*4;
  L4_StoreMRs(1, n, (L4_Word_t*)data);
  return  tag.X.label;
}
#endif


#if 0 //===============================================
int l4_call_4_4(L4_ThreadId_t to, int label, 
		int  in1, int in2, int in3, int in4,
		int* out1, int* out2, int* out3, int* out4)
{
    L4_MsgTag_t    tag;

    tag.raw = 0; tag.X.label = label; tag.X.u = 4;
    L4_LoadMR(0, tag.raw);
    L4_LoadMR(1, in1);
    L4_LoadMR(2, in2);
    L4_LoadMR(3, in3);
    L4_LoadMR(4, in4);
    tag = L4_Call(to);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    L4_StoreMR(1, (L4_Word_t*)out1);
    L4_StoreMR(2, (L4_Word_t*)out2);
    L4_StoreMR(3, (L4_Word_t*)out3);
    L4_StoreMR(4, (L4_Word_t*)out4);
    return tag.X.label;
}

int l4_call_m_n(L4_ThreadId_t to, int label, 
		int  m, int in[], 
		int  n, int *o, int *out[])
{
    L4_MsgTag_t    tag;
    tag.raw = 0; tag.X.label = label; tag.X.u = m;
    L4_LoadMR(0, tag.raw);
    L4_LoadMRs(1, m, (L4_Word_t*)in);
    tag = L4_Call(to);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    *o = tag.X.u; 
    if (tag.X.u < n) 
        n = tag.X.u;
    L4_StoreMRs(1, n, (L4_Word_t*)out);
    return tag.X.label;
}

int l4_call_1_0(L4_ThreadId_t to, int label, int in1)
{
    L4_MsgTag_t    tag;
    tag.raw = 0; tag.X.label = label; tag.X.u = 1;
    L4_LoadMR(0, tag.raw);
    L4_LoadMR(1, in1);
    tag = L4_Call(to);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    return tag.X.label;
}

//==========================================
int l4_send_4(L4_ThreadId_t to, int label,
	      int  in1, int in2, int in3, int in4)
{
    L4_MsgTag_t    tag;
    tag.raw = 0; tag.X.label = label; tag.X.u = 4;
    L4_LoadMR(0, tag.raw);
    L4_LoadMR(1, in1);
    L4_LoadMR(2, in2);
    L4_LoadMR(3, in3);
    L4_LoadMR(4, in4);
    tag = L4_Send(to);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    else 
        return  1;
}

int l4_send_0(L4_ThreadId_t to, int label)
{
    L4_MsgTag_t    tag;
    tag.raw = 0; tag.X.label = label; tag.X.u = 0;
    L4_LoadMR(0, tag.raw);
    tag = L4_Send(to);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    else 
        return  1;
}


int l4_send_1(L4_ThreadId_t to, int label,  int in1)
{
    L4_MsgTag_t    tag;
    tag.raw = 0; tag.X.label = label; tag.X.u = 1;
    L4_LoadMR(0, tag.raw);
    L4_LoadMR(1, in1);
    tag = L4_Send(to);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    else 
        return  1;
}

//=====================================
int l4_recfrom_0(L4_ThreadId_t from)
{
    L4_MsgTag_t    tag;
    tag.raw = 0;  tag.X.u = 0;
    L4_LoadMR(0, tag.raw);
    tag = L4_Receive(from);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    return tag.X.label;
}

int l4_recfrom_1(L4_ThreadId_t from, int* out1)
{
    L4_MsgTag_t    tag;
    tag.raw = 0;  tag.X.u = 1;
    L4_LoadMR(0, tag.raw);
    tag = L4_Receive(from);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    L4_StoreMR(1, (L4_Word_t*)out1);
    return tag.X.label;
}

int l4_recfrom_4(L4_ThreadId_t from, 
		int* out1, int* out2, int* out3, int* out4)
{
    L4_MsgTag_t    tag;
    tag.raw = 0;  tag.X.u = 4;
    L4_LoadMR(0, tag.raw);
    tag = L4_Receive(from);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    L4_StoreMR(1, (L4_Word_t*)out1);
    L4_StoreMR(2, (L4_Word_t*)out2);
    L4_StoreMR(3, (L4_Word_t*)out3);
    L4_StoreMR(4, (L4_Word_t*)out4);
    return tag.X.label;
}

//----------------------------
int l4_receive_0(L4_ThreadId_t *from)
{
    L4_MsgTag_t    tag;
    tag.raw = 0;  tag.X.u = 0;
    L4_LoadMR(0, tag.raw);
    tag = L4_Wait(from);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    return tag.X.label;
}

int l4_receive_1(L4_ThreadId_t *from, int* out1)
{
    L4_MsgTag_t    tag;
    int            result;
    tag.raw = 0;  tag.X.u = 1;
    L4_LoadMR(0, tag.raw);
    tag = L4_Wait(from);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    result = tag.X.label;
    L4_StoreMR(1, (L4_Word_t*)out1);
    return result;
}

int l4_receive_2(L4_ThreadId_t *from, 
		int* out1, int* out2)
{
    L4_MsgTag_t    tag;
    int            result;
    tag.raw = 0;  tag.X.u = 2;
    L4_LoadMR(0, tag.raw);
    tag = L4_Wait(from);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    result = tag.X.label;
    L4_StoreMR(1, (L4_Word_t*)out1);
    L4_StoreMR(2, (L4_Word_t*)out2);
    return result;
}

int l4_receive_4(L4_ThreadId_t *from, 
		int* out1, int* out2, int* out3, int* out4)
{
    L4_MsgTag_t    tag;
    int            result;
    tag.raw = 0;  tag.X.u = 4;
    L4_LoadMR(0, tag.raw);
    tag = L4_Wait(from);
    if (tag.raw & 0x00008000)  //Error bit
        return -1;
    result = tag.X.label;
    L4_StoreMR(1, (L4_Word_t*)out1);
    L4_StoreMR(2, (L4_Word_t*)out2);
    L4_StoreMR(3, (L4_Word_t*)out3);
    L4_StoreMR(4, (L4_Word_t*)out4);
    return result;
}

#endif //===============================================
