struct Dev
{
  int     dc;
  char*   name;

  void    (*reset)(void);
  void    (*init)(void);
  void    (*shutdown)(void);


  Chan*   (*attach)(char* spec_bogus);   

  Walkqid*(*walk)(Chan* ch, Chan* newch, char** names, int nnames);

  int     (*stat)(Chan* ch, uchar*, int);

  Chan*   (*open)(Chan* ch, int omode);

  Chan*   (*create)(Chan* ch, char* name, int omode, ulong perm);  
          //% nil:ERR <- void

  int     (*close)(Chan* ch);             //% ONERR <- void

  long    (*read)(Chan* ch, void* buf, long len, vlong offset);

  Block*  (*bread)(Chan*, long, ulong);   // Dev-only

  long    (*write)(Chan* ch, void* buf, long len, vlong offset);

  long    (*bwrite)(Chan*, Block*, ulong);    // Dev-only

  int     (*remove)(Chan* ch);    //% ONERR <- void

  int     (*wstat)(Chan* ch, uchar*, int);


  void    (*power)(int);  /* power mgt: power(1) => on, power(0) => off */
  int     (*config)(int, char*, DevConf*); //% ONERR<-void
};


//-------------------------------------------------------------
struct Srv {
  Tree*   tree;
  void            (*destroyfid)(Fid*);
  void            (*destroyreq)(Req*);
  void            (*end)(Srv*);
  void*   aux;


  void            (*attach)(Req*);

  void            (*auth)(Req*);   // Srv, 9P

  void            (*open)(Req*);

  void            (*create)(Req*);

  void            (*read)(Req*);

  void            (*write)(Req*);

  void            (*remove)(Req*);

  void            (*flush)(Req*);  // Srv, 9P

  void            (*stat)(Req*);

  void            (*wstat)(Req*);

  void            (*walk)(Req*);


  char*           (*clone)(Fid*, Fid*);        // Srv, 9P
  char*           (*walk1)(Fid*, char*, Qid*); // Srv, 9P

  int             infd;
  int             outfd;
  int             nopipe;
  int             srvfd;
  int             leavefdsopen;   /* magic for acme win */
  char*   keyspec;

  /* below is implementation-specific; don't use */
  Fidpool*        fpool;
  Reqpool*        rpool;
  uint            msize;

  uchar*  rbuf;
  QLock   rlock;
  uchar*  wbuf;
  QLock   wlock;
         
  char*   addr;
};

//----------------------------------------------------
9P message

 MESSAGES


 size [4] Tversion  tag [2] msize [4] version [ s ] 
 size [4] Rversion tag [2] msize [4] version [ s ]


 size [4] Tauth tag [2] afid [4] uname [ s ] aname [ s ]
 size [4] Rauth tag [2] aqid [13]
 

 size [4] Rerror tag [2] ename [ s ]
 

 size [4] Tflush tag [2] oldtag [2]
 size [4] Rflush tag [2]
 

 size [4] Tattach tag [2] fid [4] afid [4] uname [ s ] aname [ s ]
 size [4] Rattach tag [2] qid [13]
 

 size [4] Twalk tag [2] fid [4] newfid [4] nwname [2] nwname *( wname [ s ])
 size [4] Rwalk tag [2] nwqid [2] nwqid *( wqid [13])
 

 size [4] Topen tag [2] fid [4] mode [1]
 size [4] Ropen tag [2] qid [13] iounit [4]
 

 size [4] Tcreate tag [2] fid [4] name [ s ] perm [4] mode [1]
 size [4] Rcreate tag [2] qid [13] iounit [4]
 

 size [4] Tread tag [2] fid [4] offset [8] count [4]
 size [4] Rread tag [2] count [4] data [ count ] 

 size [4] Twrite tag [2] fid [4] offset [8] count [4] data [ count ]
 size [4] Rwrite tag [2] count [4]
 

 size [4] Tclunk tag [2] fid [4]
 size [4] Rclunk tag [2]
 

 size [4] Tremove tag [2] fid [4]
 size [4] Rremove tag [2]
 

 size [4] Tstat tag [2] fid [4]
 size [4] Rstat tag [2] stat [ n ]
 

 size [4] Twstat tag [2] fid [4] stat [ n ]
 size [4] Rwstat tag [2]




struct  Fcall
{
  uchar   type;
  u32int  fid;
  ushort  tag;
  union {
    struct {
      u32int  msize;          /* Tversion, Rversion */
      char    *version;       /* Tversion, Rversion */
    } ;
    struct {
      ushort  oldtag;         /* Tflush */
    } ;
    struct {
      char    *ename;         /* Rerror */
    } ;
    struct {
      Qid     qid;            /* Rattach, Ropen, Rcreate */
      u32int  iounit;         /* Ropen, Rcreate */
    } ;
    struct {
      Qid     aqid;           /* Rauth */
    } ;
    struct {
      u32int  afid;           /* Tauth, Tattach */
      char    *uname;         /* Tauth, Tattach */
      char    *aname;         /* Tauth, Tattach */
    } ;
    struct {
      u32int  perm;           /* Tcreate */
      char    *name;          /* Tcreate */
      uchar   mode;           /* Tcreate, Topen */
    } ;
    struct {
      u32int  newfid;         /* Twalk */
      ushort  nwname;         /* Twalk */
      char    *wname[MAXWELEM];       /* Twalk */
    } ;
    struct {
      ushort  nwqid;          /* Rwalk */
      Qid     wqid[MAXWELEM];         /* Rwalk */
    } ;
    struct {
      vlong   offset;         /* Tread, Twrite */
      u32int  count;          /* Tread, Twrite, Rread */
      char    *data;          /* Twrite, Rread */
    } ;
    struct {
      ushort  nstat;          /* Twstat, Rstat */
      uchar   *stat;          /* Twstat, Rstat */
    } ;
  };
} Fcall;
