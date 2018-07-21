
#include  <l4all.h>
#include  <l4/l4io.h>
#include "../port/_dbg.h" 

#define  INLINE  static inline


extern L4_ThreadId_t  SrvManager;
static   L4_ThreadId_t  _server;
#define  L4_Call(to)  L4_ReplyWait(to, &_server)

extern L4_MapItem_t covering_fpage_map(void *adrs, uint size, uint perm);

//----- NULL  ----------------------------------  
INLINE int syscall_(int syscallnr)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x0; //  pattern("");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0);  //MR1
    return result;
}

//----- a ----------------------------------  
INLINE int syscall_a(int syscallnr, void* arg0)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x2; //  pattern("a");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, (int)arg0);    //arg0 a
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- aa ----------------------------------  
INLINE int syscall_aa(int syscallnr, void* arg0, void* arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x22; //  pattern("aa");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, (int)arg0);    //arg0 a
    L4_MsgAppendWord(&msgreg, (int)arg1);    //arg1 a
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- ai ----------------------------------  
INLINE int syscall_ai(int syscallnr, void* arg0, int arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x12; //  pattern("ai");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, (int)arg0);    //arg0 a
    L4_MsgAppendWord(&msgreg, arg1);         //arg1 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- i ----------------------------------  
INLINE int syscall_i(int syscallnr, int arg0)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x1; //  pattern("i");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);    //arg0 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag)) return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- ia ----------------------------------  
INLINE int syscall_ia(int syscallnr, int arg0, void* arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x21; //  pattern("ia");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);       //arg0 i
    L4_MsgAppendWord(&msgreg, (int)arg1);  //arg1 a
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- iai ----------------------------------  
INLINE int syscall_iai(int syscallnr, int arg0, void* arg1, int arg2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x121; //  pattern("iai");
    L4_ThreadId_t  actualsender;

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);       //arg0 i
    L4_MsgAppendWord(&msgreg, (int)arg1);  //arg1 a
    L4_MsgAppendWord(&msgreg, arg2);       //arg2 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);

    if (L4_IpcPropagated(tag)) 
        actualsender = L4_ActualSender();
    else
        actualsender = L4_nilthread;
    //  l4printf_r("syscall<%x,%x,%x> ", SrvManager, _server, actualsender); 

    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- aim ----------------------------------  
INLINE int syscall_aim(int syscallnr, void* arg0, int arg1, L4_MapItem_t mapitem)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x412; //  pattern("aim");
    L4_ThreadId_t  actualsender;

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, (int)arg0);  //arg1 a
    L4_MsgAppendWord(&msgreg, arg1);       //arg1 i

    L4_MsgAppendMapItem(&msgreg, mapitem);  //map
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);

    if (L4_IpcPropagated(tag)) 
        actualsender = L4_ActualSender();
    else
        actualsender = L4_nilthread;
    //  l4printf_r("syscall<%x,%x,%x> ", SrvManager, _server, actualsender); 

    L4_UnmapFpage(L4_MapItemSndFpage(mapitem)); //?

    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- iaim ----------------------------------  
INLINE int syscall_iaim(int syscallnr, int arg0, void* arg1, int arg2, L4_MapItem_t mapitem)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x4121; //  pattern("iaim");
    L4_ThreadId_t  actualsender;

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);       //arg0 i
    L4_MsgAppendWord(&msgreg, (int)arg1);  //arg1 a
    L4_MsgAppendWord(&msgreg, arg2);       //arg2 i

    L4_MsgAppendMapItem(&msgreg, mapitem);  //map
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);

    if (L4_IpcPropagated(tag)) 
        actualsender = L4_ActualSender();
    else
        actualsender = L4_nilthread;
    //  l4printf_r("syscall<%x,%x,%x> ", SrvManager, _server, actualsender); 

    L4_UnmapFpage(L4_MapItemSndFpage(mapitem)); //?

    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- iaii ----------------------------------  
INLINE int syscall_iaii(int syscallnr, int arg0, void* arg1, int arg2, int arg3)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x1121; //  pattern("iaii");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);       //arg0 i
    L4_MsgAppendWord(&msgreg, (int)arg1);  //arg1 a
    L4_MsgAppendWord(&msgreg, arg2);       //arg2 i
    L4_MsgAppendWord(&msgreg, arg3);       //arg3 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- iaiii ----------------------------------  
INLINE int syscall_iaiii(int syscallnr, int arg0, void* arg1, int arg2, int arg3, int arg4)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x11121; //  pattern("iaiii");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);       //arg0 i
    L4_MsgAppendWord(&msgreg, (int)arg1);  //arg1 a
    L4_MsgAppendWord(&msgreg, arg2);       //arg2 i
    L4_MsgAppendWord(&msgreg, arg3);       //arg3 i
    L4_MsgAppendWord(&msgreg, arg4);       //arg4 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- iaiiim ----------------------------------  
INLINE int syscall_iaiiim(int syscallnr, int arg0, void* arg1, int arg2, int arg3, int arg4,
		   L4_MapItem_t mapitem)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x411121; //  pattern("iaiiim");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);       //arg0 i
    L4_MsgAppendWord(&msgreg, (int)arg1);  //arg1 a
    L4_MsgAppendWord(&msgreg, arg2);       //arg2 i
    L4_MsgAppendWord(&msgreg, arg3);       //arg3 i
    L4_MsgAppendWord(&msgreg, arg4);       //arg4 i
 
    L4_MsgAppendMapItem(&msgreg, mapitem);  //map

    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_UnmapFpage(L4_MapItemSndFpage(mapitem)); //?

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- ii ----------------------------------  
INLINE int syscall_ii(int syscallnr, int arg0, int arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x11; //  pattern("ii");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);    //arg0 i
    L4_MsgAppendWord(&msgreg, arg1);    //arg1 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}

//----- iii ----------------------------------  
INLINE int syscall_iii(int syscallnr, int arg0, int arg1, int arg2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x111; //  pattern("iii");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);  //arg0 i
    L4_MsgAppendWord(&msgreg, arg1);  //arg1 i
    L4_MsgAppendWord(&msgreg, arg2);  //arg2 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}


//----- iisi ----------------------------------  
INLINE int syscall_iisi(int syscallnr, int arg0, int arg1, char* arg2, int arg3)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x1311; //  pattern("iisi");
    L4_StringItem_t  sitem;
    int len2;

    if (arg2)
      len2 = strlen(arg2)+1;
    else
      len2 = 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len2;
    sitem.X.str.string_ptr = arg2;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);    //arg0 i
    L4_MsgAppendWord(&msgreg, arg1);    //arg1 i
    L4_MsgAppendWord(&msgreg, len2);    //arg2 s
    L4_MsgAppendWord(&msgreg, arg3);    //arg3 i             

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))   return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- iisis ----------------------------------  
INLINE int syscall_iisis(int syscallnr, 
		  int arg0, int arg1, char* arg2, int arg3, char *arg4)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x31311; //  pattern("iisis");
    L4_StringItem_t  sitem[2];
    int len2;
    int len4;

    if (arg2)
      len2  = strlen(arg2)+1;
    else
      len2 = 0;
    if (arg4)
      len4  = strlen(arg4)+1;
    else
      len4 = 0;

    memset(sitem, 0, sizeof(sitem));
    sitem[0].X.string_length = len2;
    sitem[0].X.str.string_ptr = arg2;
    sitem[0].X.c = 1;    //compound string
    sitem[0].X.C = 1;  
    sitem[1].X.string_length = len4;
    sitem[1].X.str.string_ptr = arg4;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);    //arg0 i
    L4_MsgAppendWord(&msgreg, arg1);    //arg1 i
    L4_MsgAppendWord(&msgreg, len2);    //arg2 s
    L4_MsgAppendWord(&msgreg, arg3);    //arg3 i             
    L4_MsgAppendWord(&msgreg, len4);    //arg4 s

    L4_MsgAppendStringItem(&msgreg, &sitem[0]);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- is ----------------------------------  
INLINE int syscall_is(int syscallnr, int arg0, char* arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x31; //  pattern("is");
    L4_StringItem_t  sitem;
    int len1 ;

    len1 = (arg1)? (strlen(arg1)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len1;
    sitem.X.str.string_ptr = arg1;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);    //arg0 i
    L4_MsgAppendWord(&msgreg, len1);    //arg1 s

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag)) return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- isai ----------------------------------  
INLINE int syscall_isai(int syscallnr, int arg0, char* arg1, void* arg2, int arg3)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x1231; //  pattern("isai");
    L4_StringItem_t  sitem;
    int len1;

    len1 = (arg1)? (strlen(arg1)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len1;
    sitem.X.str.string_ptr = arg1;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);        //arg0 i
    L4_MsgAppendWord(&msgreg, len1);        //arg1 s
    L4_MsgAppendWord(&msgreg, (int)arg2);   //arg2 a
    L4_MsgAppendWord(&msgreg, arg3);        //arg3 i

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag)) return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- isi ----------------------------------  
INLINE int syscall_isi(int syscallnr, int arg0, char* arg1, int arg2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x131; //  pattern("isi");
    L4_StringItem_t  sitem;
    int   len1 ;

    len1 = (arg1)? (strlen(arg1)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len1;
    sitem.X.str.string_ptr = arg1;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);    //arg0 i
    L4_MsgAppendWord(&msgreg, len1);    //arg1 s
    L4_MsgAppendWord(&msgreg, arg2);    //arg2 i

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag)) return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- isis ----------------------------------  
INLINE int syscall_isis(int syscallnr, 
		 int arg0, char* arg1, int arg2, char *arg3)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x3131; //  pattern("isis");
    L4_StringItem_t  sitem[2];
    int   len1;
    int   len3;

    len1 = (arg1)? (strlen(arg1)+1) : 0;
    len3 = (arg3)? (strlen(arg3)+1) : 0;

    memset(sitem, 0, sizeof(sitem));
    sitem[0].X.string_length = len1;
    sitem[0].X.str.string_ptr = arg1;
    sitem[0].X.c = 1;    //compound string
    sitem[0].X.C = 1;  
    sitem[1].X.string_length = len3;
    sitem[1].X.str.string_ptr = arg3;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);    //arg0 i
    L4_MsgAppendWord(&msgreg, len1);    //arg1 s
    L4_MsgAppendWord(&msgreg, arg2);    //arg2 i             
    L4_MsgAppendWord(&msgreg, len3);    //arg3 s

    L4_MsgAppendStringItem(&msgreg, &sitem[0]);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))    return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- s ----------------------------------  
INLINE int syscall_s(int syscallnr,  char *arg0)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x3; //  pattern("s");
    L4_StringItem_t  sitem;
    int len0 ;

    len0 = (arg0)? (strlen(arg0)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len0;
    sitem.X.str.string_ptr = arg0;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);    //arg0 s

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))   return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- sa ----------------------------------  
INLINE int syscall_sa(int syscallnr,  char *arg0, void* arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x23; //  pattern("sa");
    L4_StringItem_t  sitem;
    int          len0;

    len0 = (arg0)? (strlen(arg0)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len0;
    sitem.X.str.string_ptr = arg0;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);        //arg0 s
    L4_MsgAppendWord(&msgreg, (int)arg1);   //arg1 a

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))    return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 1); 

    return result;
}

//----- sai ----------------------------------  
INLINE int syscall_sai(int syscallnr,  char *arg0, void* arg1, int arg2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x123; //  pattern("sai");
    L4_StringItem_t  sitem;
    int          len0;

    len0 = (arg0)? (strlen(arg0)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len0;
    sitem.X.str.string_ptr = arg0;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);      //arg0 s
    L4_MsgAppendWord(&msgreg, (int)arg1); //arg1 a
    L4_MsgAppendWord(&msgreg, arg2);      //arg2 i

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))    return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}


//----- saim ----------------------------------  
INLINE int syscall_saim(int syscallnr,  char *arg0, void* arg1, int arg2, L4_MapItem_t mapitem)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x4123; //  pattern("saim");
    L4_StringItem_t  sitem;
    int         len0;

    len0 = (arg0)? (strlen(arg0)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len0;
    sitem.X.str.string_ptr = arg0;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);         //arg0 s
    L4_MsgAppendWord(&msgreg, (int)arg1);    //arg1 a
    L4_MsgAppendWord(&msgreg, arg2);         //arg2 i

    L4_MsgAppendStringItem(&msgreg, &sitem);

    L4_MsgAppendMapItem(&msgreg, mapitem);  //map
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))    return -1; //XXX

    L4_UnmapFpage(L4_MapItemSndFpage(mapitem)); //?

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}


//----- si ----------------------------------  
INLINE int syscall_si(int syscallnr,  char *arg0, int arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x13; //  pattern("si");
    L4_StringItem_t  sitem;
    int          len0;

    len0 = (arg0)? (strlen(arg0)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len0;
    sitem.X.str.string_ptr = arg0;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);    //arg0 s
    L4_MsgAppendWord(&msgreg, arg1);    //arg0 i

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))   return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}


//----- sii ----------------------------------  
INLINE int syscall_sii(int syscallnr,  char *arg0, int arg1, int arg2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x113; //  pattern("sii");
    L4_StringItem_t  sitem;
    int         len0;

    len0 = (arg0)? (strlen(arg0)+1) : 0;

    memset(&sitem, 0, sizeof(sitem));
    sitem.X.string_length = len0;
    sitem.X.str.string_ptr = arg0;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);    //arg0 s
    L4_MsgAppendWord(&msgreg, arg1);    //arg1 i
    L4_MsgAppendWord(&msgreg, arg2);    //arg2 i

    L4_MsgAppendStringItem(&msgreg, &sitem);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))    return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}


//----- ss ----------------------------------  
INLINE int syscall_ss(int syscallnr, char *arg0, char *arg1)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x33; //  pattern("ss");
    L4_StringItem_t  sitem[2];
    int          len0;
    int          len1;

    len0 = (arg0)? (strlen(arg0)+1) : 0;
    len1 = (arg1)? (strlen(arg1)+1) : 0;

    memset(sitem, 0, sizeof(sitem));
    sitem[0].X.string_length = len0;
    sitem[0].X.str.string_ptr = arg0;
    sitem[0].X.c = 1;    //compound string
    sitem[0].X.C = 1;  
    sitem[1].X.string_length = len1;
    sitem[1].X.str.string_ptr = arg1;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);    //arg0 s
    L4_MsgAppendWord(&msgreg, len1);    //arg1 s

    L4_MsgAppendStringItem(&msgreg, &sitem[0]);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag)) return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}

//----- ssi ----------------------------------  
INLINE int syscall_ssi(int syscallnr, char *arg0, char *arg1, int arg2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x133; //  pattern("ssi");
    L4_StringItem_t  sitem[2];
    int          len0;
    int          len1;

    len0 = (arg0)? (strlen(arg0)+1) : 0;
    len1 = (arg1)? (strlen(arg1)+1) : 0;

    memset(sitem, 0, sizeof(sitem));
    sitem[0].X.string_length = len0;
    sitem[0].X.str.string_ptr = arg0;
    sitem[0].X.c = 1;    //compound string
    sitem[0].X.C = 1;  
    sitem[1].X.string_length = len1;
    sitem[1].X.str.string_ptr = arg1;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, len0);    //arg0 s
    L4_MsgAppendWord(&msgreg, len1);    //arg1 s
    L4_MsgAppendWord(&msgreg, arg2);    //arg2 i

    L4_MsgAppendStringItem(&msgreg, &sitem[0]);
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag)) return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}




// int mount2(int fd, int afd, char *old, int flag, char *aname)
// {
//   return syscall_iisis(MOUNT, fd, afd, old, flag,  aname);
// }


//----------------------------------------
INLINE int syscall_i3s2(int syscallnr, int arg0, int arg1, int arg2,
		 char *str1, char *str2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, arg0);   //arg0
    L4_MsgAppendWord(&msgreg, arg1 );  //arg1
    L4_MsgAppendWord(&msgreg, arg2 );  //arg2
             
    L4_MsgAppendSimpleStringItem(&msgreg,
				 L4_StringItem(sizeof(str1)+1, (void*)str1));
    L4_MsgAppendSimpleStringItem(&msgreg,
				 L4_StringItem(sizeof(str2)+1, (void*)str2));
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))    return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 

    return result;
}


//----- iiii ----------------------------------  
INLINE vlong syscall_iiii_v(int syscallnr, int arg0, int arg1, int arg2, int arg3)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    //    int          rlow, rhigh;
    //    vlong        result;
    union {
      vlong  v;
      ulong  u[2];
    } uv;
    unsigned     patrn = 0x1111; //  pattern("iiii");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);  //arg0 i
    L4_MsgAppendWord(&msgreg, arg1);  //arg1 i
    L4_MsgAppendWord(&msgreg, arg2);  //arg2 i
    L4_MsgAppendWord(&msgreg, arg3);  //arg3 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    uv.u[0] = L4_MsgWord(&msgreg, 0); 
    uv.u[1] = L4_MsgWord(&msgreg, 1); 
    return uv.v;
}



// int mount1(int fd, int afd, char *old, int flag, char *aname)
// {
//   return syscall_i3s2(MOUNT, fd, afd, flag, old, aname);
// }



//----- iiim ----------------------------------
INLINE int syscall_iiim(int syscallnr, int arg0, int  arg1, int arg2, L4_MapItem_t mapitem)
{
  L4_Msg_t     msgreg;
  L4_MsgTag_t  tag;
  int          result;
  unsigned     patrn = 0x4111; //  pattern("iiim");

  L4_MsgClear(&msgreg);
  L4_Set_MsgLabel(&msgreg, syscallnr);
  L4_MsgAppendWord(&msgreg, patrn);
  L4_MsgAppendWord(&msgreg, arg0);  //arg0 i
  L4_MsgAppendWord(&msgreg, arg1);  //arg1 i
  L4_MsgAppendWord(&msgreg, arg2);  //arg2 i

  L4_MsgAppendMapItem(&msgreg, mapitem);  //map

  L4_MsgLoad(&msgreg);
  tag = L4_Call(SrvManager);
  if (L4_IpcFailed(tag))  return -1; //XXX

  L4_UnmapFpage(L4_MapItemSndFpage(mapitem)); //?

  L4_MsgStore(tag, &msgreg);
  result = L4_MsgWord(&msgreg, 0);
  return result;
}

