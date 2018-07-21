#ifndef _DAT_S_H_ 
#define _DAT_S_H_

//---- port/portdat.h -----------------
typedef struct Alarms   Alarms;
typedef struct Block    Block;
typedef struct Chan     Chan;
typedef struct Cmdbuf   Cmdbuf;
typedef struct Cmdtab   Cmdtab;
//%  typedef struct Confmem     Confmem;   //! L4p9
typedef struct Dev      Dev;
typedef struct Dirtab   Dirtab;

#ifndef _EDF_T_
#define _EDF_T_
typedef struct Edf      Edf;
#endif
typedef struct Egrp     Egrp;
typedef struct Evalue   Evalue;
typedef struct Fgrp     Fgrp;
typedef struct DevConf  DevConf;
typedef struct Image    Image;
typedef struct Log      Log;
typedef struct Logflag  Logflag;
typedef struct Mntcache Mntcache;
typedef struct Mount    Mount;
typedef struct Mntrpc   Mntrpc;
typedef struct Mntwalk  Mntwalk;
typedef struct Mnt      Mnt;
typedef struct Mhead    Mhead;
typedef struct Note     Note;
typedef struct Page     Page;
typedef struct Path     Path;
typedef struct Palloc   Palloc;
typedef struct Pallocmem        Pallocmem;
typedef struct Perf     Perf;
typedef struct PhysUart PhysUart;
typedef struct Pgrp     Pgrp;
typedef struct Physseg  Physseg;
typedef struct Proc     Proc;
typedef struct Pte      Pte;
typedef struct QLock    QLock;
typedef struct Queue    Queue;
typedef struct Ref      Ref;
typedef struct Rendez   Rendez;
typedef struct Rgrp     Rgrp;
typedef struct RWlock   RWlock;
typedef struct Sargs    Sargs;
typedef struct Schedq   Schedq;
typedef struct Segment  Segment;
typedef struct Sema     Sema;
typedef struct Timer    Timer;
typedef struct Timers   Timers;
typedef struct Uart     Uart;
typedef struct Waitq    Waitq;
typedef struct Walkqid  Walkqid;
typedef struct Watchdog Watchdog;
typedef int    Devgen(Chan* c, char* name, Dirtab* d, int n, int i, Dir* dir);

//------ pc/data.h -------------------
typedef struct Conf     Conf;
typedef struct Confmem  Confmem;
typedef struct FPsave   FPsave;
typedef struct ISAConf  ISAConf;
typedef struct Label    Label;
typedef struct Lock     Lock;
typedef struct MMU      MMU;
typedef struct Mach     Mach;
typedef struct Notsave  Notsave;
typedef struct PCArch   PCArch;
typedef struct Pcidev   Pcidev;
typedef struct PCMmap   PCMmap;
typedef struct PCMslot  PCMslot;
//typedef struct Page   Page;
typedef struct PMMU     PMMU;
//typedef struct Proc     Proc;
typedef struct Segdesc  Segdesc;
typedef struct Ureg     Ureg;
typedef struct Vctl     Vctl;


//------- pc/io.h ----------------
//typedef struct Vctl Vctl;  //%
typedef struct Pcisiz Pcisiz;
//typedef struct Pcidev Pcidev;
typedef struct SMBus SMBus;
//typedef struct PCMslot          PCMslot;
typedef struct PCMconftab       PCMconftab;


#endif 
