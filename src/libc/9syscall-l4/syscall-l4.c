#include  "sys.h"
#include  <u.h>
#include  <libc.h>

#define USE_MAPPING  1  

#include  "ipc-l4.h"


//#  bind
int      bind(char* name, char* old, int flag)	// ssi
{   
    return syscall_ssi(BIND, name, old, flag); 
}

//# chdir
int      chdir(char* dirname)		// s
{   
    return syscall_s(CHDIR, dirname); 
}

//# close
int      close(int arg1)		// i
{   
    return syscall_i(CLOSE, arg1); 
}

//# dup
int      dup(int oldfd, int newfd)	// ii
{   
    return syscall_ii(DUP, oldfd, newfd); 
}

//# alarm
long     alarm(ulong arg1)		// i
{   
    return syscall_i(ALARM, arg1); 
}

//# exits
void     exits(char* msg)		// s
{   
    syscall_s(EXITS, msg); 
}

//# open
int      open(char* file, int omode)	// si
{   
    return syscall_si(OPEN, file, omode); 
}

//# _read
long     _read(int fd, void* buf, long len)	// iai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, len, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(_READ, fd, buf, len, map); 
#else
    return syscall_iai(_READ, fd, buf, len); 
#endif //---------------------------------------------

}


//# _write
long     _write(int fd, void* buf, long len)	// iai
{ 
    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, len, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(_WRITE, fd, buf, len, map); 
#else
    return syscall_iai(_WRITE, fd, buf, len); 
#endif //---------------------------------------------
}

//# pipe
int      pipe(int fd[2])	// 
{
#if 1 //------------------------------
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x0; //  pattern("");
                                                                                
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, PIPE);
    L4_MsgAppendWord(&msgreg, patrn);
                                                                                
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX
                                                                                
    L4_MsgStore(tag, &msgreg);
    result= L4_MsgWord(&msgreg, 0);  //MR1 
    fd[0] = L4_MsgWord(&msgreg, 1);  //MR2 carries fd[0]
    fd[1] = L4_MsgWord(&msgreg, 2);  //MR3 carries fd[1]
    //  l4printf_r("pipe:<%d %d>\n", fd[0], fd[1]);
    return  result;
#else //------------------------------
    return syscall_a(PIPE, fd); 
#endif //--------------------------------
}


//# create
int      create(char* file, int omode, ulong perm)	// sii
{   
    return syscall_sii(CREATE, file, omode, perm); 
}

//# fd2path
int      fd2path(int fd, char* buf, int nbuf)	// iai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, nbuf, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(FD2PATH, fd, buf, nbuf, map); 
#else
    return syscall_iai(FD2PATH, fd, buf, nbuf); 
#endif //-----------------------------------------
}


//# brk_
int      brk_(void* arg1)	// a
{   
    return syscall_a(BRK_, arg1); 
}


//# remove
int      remove(char* arg1)	// s
{   
    return syscall_s(REMOVE, arg1); 
}

//# notify
int      notify(void(*arg1)(void*, char*))	// a
{   
    return syscall_a(NOTIFY, arg1); 
}

//# noted
int      noted(int arg1)			// i
{   
    return syscall_i(NOTED, arg1); 
}


//# rendezvous
void*    rendezvous(void* arg1, void* arg2)	// aa
{   
    return (void*)syscall_aa(RENDEZVOUS, arg1, arg2); 
}

//# unmount
int      unmount(char* name, char* old)		// ss
{   
    return syscall_ss(UNMOUNT, name, old); 
}

//# _wait
Waitmsg* _wait(void)				//
{
   return (Waitmsg*)syscall_(_WAIT); 
}


//# seek
vlong    seek(int fd, vlong n, int type)	// iii
{   
    union {
      vlong   v;
      ulong   u[2];
    } uv;
    uv.v = n;
    return syscall_iiii_v(SEEK, fd, uv.u[0], uv.u[1], type); 
}

//# fversion
int      fversion(int arg1, int arg2, char* arg3, int arg4)	// iisi
{   
    return syscall_iisi(FVERSION, arg1, arg2, arg3, arg4); 
}


//# errstr
int      errstr(char* err, uint nerr)		// si
     // {   return syscall_si(ERRSTR, arg1, arg2); }
{   
    return syscall_ai(ERRSTR, err, nerr); 
}  

//# stat
int      stat(char* name, uchar* edir, int nedir)	// s?i
{  
    L4_MapItem_t  map; 
    map = covering_fpage_map(edir, nedir, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_saim(STAT, name, edir, nedir, map); 
#else
    return syscall_sai(STAT, name, edir, nedir); 
#endif //-------------------------------------------
}  

//# fstat
int      fstat(int fd, uchar* edir, int nedir)	// iai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(edir, nedir, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(FSTAT, fd, edir, nedir, map); 
#else
    return syscall_iai(FSTAT, fd, edir, nedir); 
#endif //------------------------------------------
}  


//# wstat
int      wstat(char* name, uchar* edir, int nedir)	// sai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(edir, nedir, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_saim(WSTAT, name, edir, nedir, map); 
#else
    return syscall_sai(WSTAT, name, edir, nedir); 
#endif //----------------------------------------
}   


//# fwstat
int      fwstat(int fd, uchar* edir, int nedir)	// iai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(edir, nedir, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(FWSTAT, fd, (char*)edir, nedir, map); 
#else
    return syscall_iai(FWSTAT, fd, (char*)edir, nedir);  
#endif //------------------------------------------
}   

//# mount
int      mount(int fd, int afd, char* old, int flag, char* aname) // iisis
{   
    return syscall_iisis(MOUNT, fd, afd, old, flag, aname); 
}

//# await
int      await(char* buf, int len)		// ai
{   
//l4printf_b(">await(%x %x)\n", buf, len);
#if  USE_MAPPING //--------------------------------------
    L4_MapItem_t  map;
    int  rc;
    map = covering_fpage_map(buf, len, L4_Readable|L4_Writable);
    rc = syscall_aim(AWAIT, buf, len, map); 
//l4printf_b("<await(%x %x)\n", buf, len);
    return rc;
#else 
    return syscall_si(AWAIT, buf, len);     //NG
#endif //-----------------------------------------------
}

//# pread
long     pread(int fd, void* buf, long nbytes, vlong offset)	// iaiii
{ 
    union {
      vlong   v;
      ulong   u[2];
    } uv;
    uv.v = offset;
    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, nbytes, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaiiim(PREAD, fd, buf, nbytes, uv.u[0], uv.u[1], map);  // VLONG ?
#else
    return syscall_iaiii(PREAD, fd, buf, nbytes, uv.u[0], uv.u[1]);    // VLONG ?
#endif //--------------------------------------------
}

//# pwrite
long     pwrite(int fd, void* buf, long nbytes, vlong offset)	// iaiii
{
    union {
      vlong   v;
      ulong   u[2];
    } uv;
    uv.v = offset;   

    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, nbytes, L4_Readable|L4_Writable);

    // l4printf("> pwrite(%d %x %x %x-%x %Lx)\n", 
    //	     fd, buf, nbytes, uv.u[0], uv.u[1], offset);
#if USE_MAPPING //--------------------------------------
    return syscall_iaiiim(PWRITE, fd, buf, nbytes, uv.u[0], uv.u[1], map);  
#else
    return syscall_iaiii(PWRITE, fd, buf, nbytes, uv.u[0], uv.u[1]);    // VLONG ?
#endif //--------------------------------------
}



//# _errstr
int      _errstr(char* arg1)		// s
{   
    return syscall_s(_ERRSTR, arg1);   
}

//# _fsession
int      _fsession(char* arg1, void* arg2, int arg3)	// sai
{   
    return syscall_sai(_FSESSION, arg1, arg2, arg3); 
}

//# fauth
int      fauth(int fd, char* aname)	// is
{   
    return syscall_is(FAUTH, fd, aname); 
}

//# _fstat
int      _fstat(int arg1, char* arg2)	// is
{   
    return syscall_is(_FSTAT, arg1, arg2); 
}

//# segbrk
void*    segbrk(void* arg1, void* arg2)	// aa
{   
    return (void*)syscall_aa(SEGBRK, arg1, arg2); 
}

//# _mount
int      _mount(int arg1, char* arg2, int arg3, char* arg4)	// isis
{   
    return syscall_isis(_MOUNT, arg1, arg2, arg3, arg4); 
}

//# oseek
long     oseek(int arg1, long arg2, int arg3)		// iii
{   
    return syscall_iii(OSEEK, arg1, arg2, arg3); 
}

//# _stat
int      _stat(char* arg1, char* arg2)	// ss
{   
    return syscall_ss(_STAT, arg1, arg2); 
}

//# _wstat
int      _wstat(char* arg1, char* arg2)		// ss
{   
    return syscall_ss(_WSTAT, arg1, arg2); 
}

//# _fwstat
int      _fwstat(int arg1, char* arg2)		// is
{   
    return syscall_is(_FWSTAT, arg1, arg2); 
}

//# segattach
void* segattach(int arg1, char* arg2, void* arg3, ulong arg4) // isai
{   
    return (void*)syscall_isai(SEGATTACH, arg1, arg2, arg3, arg4); 
}

//# segdetach
int      segdetach(void* arg1)			// a
{   
    return syscall_a(SEGDETACH, arg1); 
}

//# segfree
int      segfree(void* arg1, ulong arg2)	// ai
{   
    return syscall_ai(SEGFREE, arg1, arg2); 
}

//# segflush
int      segflush(void* arg1, ulong arg2)	// ai
{   
    return syscall_ai(SEGFLUSH, arg1, arg2); 
}

//# semacquire
int      semacquire(long* arg1, int arg2)	// ai
{   
    return syscall_ai(SEMACQUIRE, arg1, arg2); 
}

//# semrelease
long     semrelease(long* arg1, long arg2)	// ai
{   
    return syscall_ai(SEMRELEASE, arg1, arg2); 
}

