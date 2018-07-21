//%%%%%%% dbg9p.c %%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>


#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>

//------------------------------------------
void _backtrace_(char *caption)
{
    typedef struct frame{
        struct frame *ebp;
      void   *ret;
    } frame;
    register void * _EBP_ asm("%ebp");
 
    if (caption == 0) caption = "";
    frame  *ebp = _EBP_;
    fprint(2, " %s btrace: ", caption);
    while (ebp) {
        fprint(2, "0x%x ", ebp->ret);
	ebp = ebp->ebp;
    }
    fprint(2, "\n");
}

//------------------------------------------------
static char *p9msgname(int typ)
{
    switch(typ) {
    case 100: return "Tversion"; 
    case 101: return "Rversion"; 
    case 102: return "Tauth"; 
    case 103: return "Rauth";
    case 104: return "Tattach";
    case 105: return "Rattach";
    case 106: return "Terror";
    case 107: return "Rerror";
    case 108: return "Tflush";
    case 109: return "Rflush";
    case 110: return "Twalk";
    case 111: return "Rwalk";
    case 112: return "Topen";
    case 113: return "Ropen";
    case 114: return "Tcreate";
    case 115: return "Rcreate";
    case 116: return "Tread";
    case 117: return "Rread";
    case 118: return "Twrite";
    case 119: return "Rwrite";
    case 120: return "Tclunk";
    case 121: return "Rclunk";
    case 122: return "Tremove";
    case 123: return "Rremove";
    case 124: return "Tstat";
    case 125: return "Rstat";
    case 126: return "Twstat";
    case 127: return "Rwstat";
    default: return "T--";
    }
}


// non-reentrant
static char *qidf(Qid  q)  
{
   static char qidbuf[32];
   snprint(qidbuf, 32, "qid<%d-%d-%x> ", (int)q.path, q.vers, q.type);
   return  qidbuf;
}

static char *qidlist(int nqid, Qid  *q)  
{
   static char qbuf[64];
   int  i;
   qbuf[0] = 0;
   snprint(qbuf, 32, "nqid:%d ", nqid);

   for (i=0; i<nqid; i++){
     strcat(qbuf, qidf(q[i]));
   } 

   return  qbuf;
}

static char *namelist(int n, char *names[])  
{
  static char nbuf[64];
  int  i;
  nbuf[0] = 0;
  for (i = 0; i < n; i++) {
    strcat(nbuf, names[i]);
    strcat(nbuf, ",");
  }
  return  nbuf;
}


void dbg9p(Fcall *fp)
{
    char  buf1[64], buf2[128];
    
    buf1[0] = 0;
    buf2[0] = 0;
  
    snprint(buf1, 64, "[9P] %s fid:%d tag:%d ", p9msgname(fp->type), fp->fid, fp->tag);

    switch(fp->type) {
    case Tversion: 
    case Rversion: snprint(buf2, 128, "m:%d v:%s ", fp->msize, fp->version); 
                   break; 

    case Tflush: snprint(buf2, 128, "otag:%d ", fp->oldtag);  break;  

    case Rerror: snprint(buf2, 128, "\"%s\" ",  fp->ename);  break;

    case Rattach: snprint(buf2, 128, "%s ", qidf(fp->qid));  break;

    case Ropen:
    case Rcreate:  snprint(buf2, 128, "%s iounit:%d ", qidf(fp->qid), fp->iounit);  break;

    case Rauth:  snprint(buf2, 128, "aqid:%s ",  qidf(fp->aqid));  break;

    case Tauth:
    case Tattach:  snprint(buf2, 128, "afid:%d uname:%s aname:%s ", 
			      fp->afid, fp->uname, fp->aname);  break;

    case Topen:  snprint(buf2, 128, "mode:%x ",	 fp->mode );  break;

    case Tcreate:  snprint(buf2, 128, "name:%s perm:%x mode:%x ", 
			      fp->name, fp->perm, fp->mode );  break;

    case Twalk:  snprint(buf2, 128, "nfid:%d nname:%d names={%s} ", 
			    fp->newfid, fp->nwname, namelist(fp->nwname, fp->wname) );  
                 break;

    case Rwalk:  snprint(buf2, 128, "%s ", qidlist(fp->nwqid, fp->wqid));
                  break;

    case Tread:  snprint(buf2, 128, "offset:%d  count:%d ", 
			  (int)fp->offset, fp->count);  break;

    case Twrite:  snprint(buf2, 128, "offset:%d  count:%d  data:%.12s", 
			  (int)fp->offset, fp->count, fp->data );  break;

    case Rread:  snprint(buf2, 128, "count:%d  data:%.12s", 
			  fp->count, fp->data );  break;

    case Twstat:
    case Rstat: snprint(buf2, 128, "nstat:%d  stat:* ",
			fp->nstat, fp->stat );  break;

    default: snprint(buf2, 128, "");
    }

    l4printf_b("    %s %s\n", buf1, buf2); // fprint(2, "    %s %s\n", buf1, buf2); 
}

//------------------------------------------------
void dbg_memory(char *title, unsigned char *start, unsigned size)
{
    int  i, j, k;
    unsigned char c;
    char  buf[128];
    char  elem[32];

    fprint(2, " dump_mem<%s %x %d>\n", title, start, size);

    if (size>2048) size = 1024;

    for(i=0; i<size; i+=16) {
        buf[0] = 0;
	for (j=i; j<i+16; j+=4) {
	    if (j%4 == 0) strcat(buf, " ");

	    for(k=3; k>=0; k--) {
	        c = start[j+k];
		snprint(elem, 32, "%.2x", c);
		strcat(buf, elem);
	    }
	}

	strcat(buf, "  ");
	for (j=i; j<i+16; j++) {
	    c = start[j];
	    if ( c >= ' ' && c < '~')
	        snprint(elem, 32, "%c", c);
	    else
	      snprint(elem, 32, ".");
	    strcat(buf, elem);
	}
	fprint(2, "%s\n", buf);
    }
}

//------------------------------------------------
void dbg_backtrace()
{
    typedef struct frame{
        struct frame *ebp;
        void   *ret;
    } frame;
    register  void * _EBP_ asm("%ebp");

    frame  *ebp = _EBP_;
    fprint(2, " dbg_backtrace: ");
    while (ebp) {
        fprint(2, "0x%x ", ebp->ret);
	ebp = ebp->ebp;
    }
    fprint(2, "\n");
}
