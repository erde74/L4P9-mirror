//%%%%%%% print9pmsg.c %%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>


#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>

#define   _DBGFLG  0
#define  DBGPRN  if(_DBGPRN)print

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
   l4snprintf(qidbuf, 32, "qid<%d-%d-%x> ", (int)q.path, q.vers, q.type);
   return  qidbuf;
}

static char *qidlist(int nqid, Qid  *q[])  
{
   static char qbuf[64];
   int  i;
   qbuf[0] = 0;
   l4snprintf(qbuf, 32, "nqid=%d=", nqid);

   for (i=0; i<nqid; i++){
     strcat(qbuf, qidf((*q)[i]));
   } 

   return  qbuf;
}

static char *namelist(int n, char *names[])  
{
  static char nbuf[64];
  int  i;
  nbuf[n] = 0;
  for (i = 0; i < n; i++) {
    strcat(nbuf, names[i]);
    strcat(nbuf, ",");
  }
  return  nbuf;
}


void  print9pmsg(Fcall *fp)
{
    char  buf1[64], buf2[128];
    
    buf1[0] = 0;
    buf2[0] = 0;
  
    l4snprintf(buf1, 64, "ramfs %s fid:%d tag:%d ", p9msgname(fp->type), fp->fid, fp->tag);

    switch(fp->type) {
    case Tversion: 
    case Rversion: l4snprintf(buf2, 128, "m=%d v=%s ", fp->msize, fp->version); 
                   break; 

    case Tflush: l4snprintf(buf2, 128, "otag=%d ", fp->oldtag);  break;  

    case Rerror: l4snprintf(buf2, 128, "\"%s\" ",  fp->ename);  break;

    case Rattach:
    case Ropen:
    case Rcreate:  l4snprintf(buf2, 128, "%s iounit=%d ", qidf(fp->qid), fp->iounit);  break;

    case Rauth:  l4snprintf(buf2, 128, "qid=%s ",  qidf(fp->qid));  break;

    case Tauth:
    case Tattach:  l4snprintf(buf2, 128, "afid=%d uname=%s aname=%s ", 
			      fp->afid, fp->uname, fp->aname);  break;

    case Topen:
    case Tcreate:  l4snprintf(buf2, 128, "name=%s perm=%x mode=%x ", 
			      fp->name, fp->perm, fp->mode );  break;

    case Twalk:  l4snprintf(buf2, 128, "nfid=%d nname=%d names={%s} ", 
			    fp->newfid, fp->nwname, namelist(fp->nwname, fp->wname) );  break;

    case Rwalk:  l4snprintf(buf2, 128, "%s ", qidlist(fp->nwqid, fp->wqid));
                  break;

    case Tread:
    case Twrite:  l4snprintf(buf2, 128, "offset=%d  count=%d  data=%s ", 
			     fp->offset, fp->count, fp->data );  break;
    }

    l4printf("# %s %s\n", buf1, buf2); 
}


