//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*  (c) 
 *  (c2)  KM
 */


#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "scsireq.h"
#include "../lib/usb.h"    //% "usb.h"

//%----------------------------------------------
#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>

#define   _DBGFLG  1
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b

static void SET(void* _x, ...) { }     
static void USED(void* _x, ...) { }     

void
sysfatal(char *fmt, ...)
{
#if 1
        PRN("sysfatal:%s \n", fmt);
#else
        va_list arg;
 	va_start(arg, fmt);
	(*_sysfatal)(fmt, arg);
	va_end(arg);
#endif
}


//%---------------------------------------------

#define GET_MAX_LUN_T	RD2H | Rclass | Rinterface
#define GET_MAX_LUN		0xFE
#define UMS_RESET_T		RH2D | Rclass | Rinterface
#define UMS_RESET		0xFF

#define PATH(type, n)		((type)|((n)<<8))
#define TYPE(path)			((int)(path) & 0xFF)
#define NUM(path)			((uint)(path)>>8)

enum {
	Qdir = 0,
	Qctl,
	Qn,
	Qraw,
	Qdata,

	CMreset = 1,

	Pcmd = 0,
	Pdata,
	Pstatus,
};

typedef struct Dirtab Dirtab;
struct Dirtab {
	char	*name;
	int	mode;
};
Dirtab dirtab[] = {  //%
	{".",	DMDIR|0555},
	{"ctl",	0640},
	{nil,	DMDIR|0640},
	{"raw",	0640},
	{"data", 0640},
};

Cmdtab cmdtab[] = {  //%
	{CMreset,	"reset",	1},
};

//% typedef struct Umsc Umsc;
struct Umsc {
        ScsiReq _scsireq;   //%
	ulong	blocks;
	vlong	capacity;
	uchar 	rawcmd[10];
	uchar	phase;
};

typedef struct Ums Ums;
struct Ums {
	Umsc	lun[256];
	uchar	maxlun;
	int		fd2;
	int		fd;
	int		setupfd;
	int		ctlfd;
	uchar	epin;
	uchar	epout;
	char	dev[64];
};

Ums ums;
long starttime;
char *owner;
extern int debug;

static void umsreset(Ums *umsc, int doinit);

/*
 * USB transparent SCSI devices
 */
typedef struct Cbw Cbw;			// command block wrapper
struct Cbw {
	char	signature[4];		// "USBC"
	long	tag;
	long	datalen;
	uchar	flags;
	uchar	lun;
	uchar	len;
	char	command[16];
};

typedef struct Csw Csw;			// command status wrapper
struct Csw {
	char	signature[4];		// "USBS"
	long	tag;
	long	dataresidue;
	uchar	status;
};

enum {
	CbwLen		= 31,
	CbwDataIn	= 0x80,
	CbwDataOut	= 0x00,
	CswLen		= 13,
	CswOk		= 0x00,
	CswFailed	= 0x01,
	CswPhaseErr	= 0x02,
};

void
statuscmd(int fd, int type, int req, int value, int index, char *data, int count)
{
	char *wp;

	wp = emalloc9p(8 + count);

	wp[0] = type;
	wp[1] = req;
	PUT2(wp + 2, value);
	PUT2(wp + 4, index);
	PUT2(wp + 6, count);
	if(data != nil)
		memmove(wp + 8, data, count);
	if(write(fd, wp, 8 + count) != 8 + count)
		sysfatal("statuscmd: %r");
}

void
statusread(int fd, char *buf, int count)
{
        char   path[32];  //%
	fd2path(fd, path, 32);  //%
	DBGPRN(">statusread{fd2path=%s} ", path); //%

	if(read(fd, buf, count) < 0)
		sysfatal("statusread: %r");
	DBGPRN("<statusread{} \n"); //%
}

void
getmaxlun(Ums *ums)
{
	uchar max;
	DBGPRN(">getmaxlun() ");

	statuscmd(ums->setupfd, GET_MAX_LUN_T, GET_MAX_LUN, 0, 0, nil, 0);
	DBGPRN("getmaxlun#2  ");

	statusread(ums->setupfd, (char *)&max, 1);

	ums->maxlun = max;

	DBGPRN("<getmaxlun() \n");
	return;
}

int
umsinit(Ums *ums, int epin, int epout)
{
	uchar data[8], i;
	char fin[128];
	DBGPRN("umsinit(epin=%d epout=%d)\n", epin, epout);

	if(ums->ctlfd == -1) {
	        snprint(fin, sizeof(fin), "%s/ctl", ums->dev);
		if((ums->ctlfd = open(fin, OWRITE)) == -1)
			return -1;
		if(epin == epout) {
			if(fprint(ums->ctlfd, "ep %d bulk rw 64 16", epin) < 0)
				return -1;
		} else {
			if(fprint(ums->ctlfd, "ep %d bulk r 64 16", epin) < 0)
				return -1;
			if(fprint(ums->ctlfd, "ep %d bulk w 64 16", epout) < 0)
				return -1;
		}
		snprint(fin, sizeof(fin), "%s/ep%ddata", ums->dev, epin);
		if((ums->fd = open(fin, OREAD)) == -1)
			return -1;
		snprint(fin, sizeof(fin), "%s/ep%ddata", ums->dev, epout);
		if((ums->fd2 = open(fin, OWRITE)) == -1)
			return -1;
		snprint(fin, sizeof(fin), "%s/setup", ums->dev);
		if ((ums->setupfd = open(fin, ORDWR)) == -1)
			return -1;
		{  //%
		   // char   path[32];  //%
		   // fd2path(ums->setupfd, path, 32);  //%
		   // DBGPRN("umsinit{ums->setupfd=%d fd2path=%s} ", 
		   //	   ums->setupfd, path); //%
		}  //%
	}
	//  DBGPRN("umsinit#2  \n");

	ums->epin = epin;
	ums->epout = epout;
	umsreset(ums, 0);
	//  DBGPRN("umsinit#3  \n");

	getmaxlun(ums);
	DBGPRN("umsinit#4  \n");

	for(i = 0; i < (ums->maxlun & 0xFF); i++) {
	        ums->lun[i]._scsireq.lun = i & 0xFF;   //% _scsireq
		ums->lun[i]._scsireq.umsc = &ums->lun[i];   //% _..
		ums->lun[i]._scsireq.flags = Fopen | Fusb | Frw10;   //% _...
		if(SRinquiry(&ums->lun[i]._scsireq) == -1)  //% _...
				return -1;
		SRstart(&ums->lun[i]._scsireq, 1);  //% _...
		if(SRrcapacity(&ums->lun[i]._scsireq, data) == -1 &&   //% _...
		   SRrcapacity(&ums->lun[i]._scsireq, data) == -1) {   //% _... 
				ums->lun[i].blocks = 0;
				ums->lun[i].capacity = 0;
				ums->lun[i]._scsireq.lbsize = 0;  //% _...
		} else {
		        ums->lun[i]._scsireq.lbsize = 
			    (data[4]<<28)|(data[5]<<16)|(data[6]<<8)|data[7];  //% _..
			ums->lun[i].blocks = 
			    (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
			ums->lun[i].blocks++;	// SRcapacity returns LBA of last block
			ums->lun[i].capacity = 
			    (vlong)ums->lun[i].blocks * ums->lun[i]._scsireq.lbsize;  //% _..
		}
		DBGPRN("umsinit#5  \n");

	}

		DBGPRN("<umsinit  \n");
	return 0;
}

static void
unstall(Ums *ums, int ep)
{
	if(fprint(ums->ctlfd, "unstall %d", ep & 0xF) < 0)
		fprint(2, "ctl write failed\n");
	if(fprint(ums->ctlfd, "data %d 0", ep & 0xF) < 0)
		fprint(2, "ctl write failed\n");

	statuscmd(ums->setupfd, RH2D | Rstandard | Rendpt, CLEAR_FEATURE, 0, (0 << 8) | ep, nil, 0);
}

static void
umsreset(Ums *umsc, int doinit)
{
	statuscmd(umsc->setupfd, UMS_RESET_T, UMS_RESET, 0, 0, nil, 0);

	unstall(umsc, umsc->epin | 0x80);
	unstall(umsc, umsc->epout);

	if(doinit && umsinit(&ums, umsc->epin, umsc->epout) < 0)
		sysfatal("device error");
}

long
umsrequest(Umsc *umsc, ScsiPtr *cmd, ScsiPtr *data, int *status)
{
	Cbw cbw;
	Csw csw;
	static int seq = 0;
	int n;

	memcpy(cbw.signature, "USBC", 4);
	cbw.tag = ++seq;
	cbw.datalen = data->count;
	cbw.flags = data->write ? CbwDataOut : CbwDataIn;
	cbw.lun = umsc->_scsireq.lun;  //% _
	cbw.len = cmd->count;
	memcpy(cbw.command, cmd->p, cmd->count);
	memset(cbw.command + cmd->count, 0, sizeof(cbw.command) - cmd->count);
	
	if(debug) {
		fprint(2, "cmd:");
		for (n = 0; n < cbw.len; n++)
			fprint(2, " %2.2x", cbw.command[n]&0xFF);
		fprint(2, " datalen: %ld\n", cbw.datalen);
	}
	if(write(ums.fd2, &cbw, CbwLen) != CbwLen){
		fprint(2, "usbscsi: write cmd: %r\n");
		goto reset;
	}
	if(data->count != 0) {
		if(data->write)
			n = write(ums.fd2, data->p, data->count);
		else
			n = read(ums.fd, data->p, data->count);
		if(n == -1){
			if(debug)
				fprint(2, "usbscsi: data %sput: %r\n", data->write? "out" : "in");
			if(data->write)
				unstall(&ums, ums.epout);
			else
				unstall(&ums, ums.epin | 0x80);
		}
	}
	n = read(ums.fd, &csw, CswLen);
	if(n == -1){
		unstall(&ums, ums.epin | 0x80);
		n = read(ums.fd, &csw, CswLen);
	}
	if(n != CswLen || strncmp(csw.signature, "USBS", 4) != 0){
		fprint(2, "usbscsi: read status: %r\n");
		goto reset;
	}
	if(csw.tag != cbw.tag) {
		fprint(2, "usbscsi: status tag mismatch\n");
		goto reset;
	}
	if(csw.status >= CswPhaseErr){
		fprint(2, "usbscsi: phase error\n");
		goto reset;
	}
	if(debug) {
		fprint(2, "status: %2.2ux residue: %ld\n", csw.status, csw.dataresidue);
		if(cbw.command[0] == ScmdRsense) {
			fprint(2, "sense data:");
			for (n = 0; n < data->count - csw.dataresidue; n++)
				fprint(2, " %2.2x", data->p[n]&0xFF);
			fprint(2, "\n");
		}
	}

	if(csw.status == CswOk)
		*status = STok;
	else
		*status = STcheck;
	return data->count - csw.dataresidue;

reset:
	umsreset(&ums, 0);
	*status = STharderr;
	return -1;
}

void
rattach(Req *r)
{
	r->ofcall.qid.path = PATH(Qdir, 0);
	r->ofcall.qid.type = dirtab[Qdir].mode >> 24;
	r->fid->qid = r->ofcall.qid;
	respond(r, nil);
}

char*
rwalk1(Fid *fid, char *name, Qid *qid)
{
	int i, n;
	char buf[32];
	ulong path;

	path = fid->qid.path;
	if(!(fid->qid.type & QTDIR))
		return "walk in non-directory";

	if(strcmp(name, "..") == 0){
		switch(TYPE(path)) {
		case Qn:
			qid->path = PATH(Qn, NUM(path));
			qid->type = dirtab[Qn].mode >> 24;
			return nil;
		case Qdir:
			return nil;
		default:
			return "bug in rwalk1";
		}
	}

	i = TYPE(path)+1;
	for(; i < nelem(dirtab); i++) {
		if(i==Qn){
			n = atoi(name);
			snprint(buf, sizeof buf, "%d", n);
			if(n < (ums.maxlun & 0xFF) && strcmp(buf, name) == 0){
				qid->path = PATH(i, n);
				qid->type = dirtab[i].mode>>24;
				return nil;
			}
			break;
		}
		if(strcmp(name, dirtab[i].name) == 0) {
			qid->path = PATH(i, NUM(path));
			qid->type = dirtab[i].mode >> 24;
			return nil;
		}
		if(dirtab[i].mode & DMDIR)
			break;
	}
	return "directory entry not found";
}

void
dostat(int path, Dir *d)
{
	Dirtab *t;

	memset(d, 0, sizeof(*d));
	d->uid = estrdup9p(owner);
	d->gid = estrdup9p(owner);
	d->qid.path = path;
	d->atime = d->mtime = starttime;
	t = &dirtab[TYPE(path)];
	if(t->name)
		d->name = estrdup9p(t->name);
	else {
		d->name = smprint("%ud", NUM(path));
		if(d->name == nil)
			sysfatal("out of memory");
	}
	if(TYPE(path) == Qdata)
		d->length = ums.lun[NUM(path)].capacity;
	d->qid.type = t->mode>>24;
	d->mode = t->mode;
}

static int
dirgen(int i, Dir *d, void* _x)  //%
{
	i += Qdir + 1;
	if(i <= Qn) {
		dostat(i, d);
		return 0;
	}
	i -= Qn;
	if(i < (ums.maxlun & 0xFF)) {
		dostat(PATH(Qn, i), d);
		return 0;
	}
	return -1;
}

static int
lungen(int i, Dir *d, void *aux)
{
	int *c;

	c = aux;
	i += Qn + 1;
	if(i <= Qdata){
		dostat(PATH(i, NUM(*c)), d);
		return 0;
	}
	return -1;
}

void
rstat(Req *r)
{
	dostat((long)r->fid->qid.path, &r->d);
	respond(r, nil);
}

void
ropen(Req *r)
{
	ulong path;

	path = r->fid->qid.path;
	switch(TYPE(path)) {
	case Qraw:
		ums.lun[NUM(path)].phase = Pcmd;
		break;
	}
	respond(r, nil);
}

void
rread(Req *r)
{
	char buf[8192], *p;
	uchar i;
	ulong path;
	int bno, nb, len, offset, n;

	path = r->fid->qid.path;
	switch(TYPE(path)) {
	case Qdir:
		dirread9p(r, dirgen, 0);
		break;
	case Qn:
		dirread9p(r, lungen, &path);
		break;
	case Qctl:
		n = 0;
		for(i = 0; i < ums.maxlun; i++) {
			n += snprint(buf + n, sizeof(buf) - n, "%d: ", (int)(i & 0xFF));
			if(ums.lun[i]._scsireq.flags & Finqok)  //% _..
				n += snprint(buf + n, sizeof(buf) - n, "inquiry %.48s ", 
					     (char *)ums.lun[i]._scsireq.inquiry + 8);  //% _..
			if(ums.lun[i].blocks > 0)
				n += snprint(buf + n, sizeof(buf) - n, "geometry %ld %ld", 
					     ums.lun[i].blocks, ums.lun[i]._scsireq.lbsize);  //% _..
			n += snprint(buf + n, sizeof(buf) - n, "\n");
		}
		readbuf(r, buf, n);
		break;
	case Qraw:
	  if(ums.lun[NUM(path)]._scsireq.lbsize <= 0) {  //% _..
			respond(r, "no media on this lun");
			return;
		}
		switch(ums.lun[NUM(path)].phase) {
		case Pcmd:
			respond(r, "phase error");
			return;
		case Pdata:
		  ums.lun[NUM(path)]._scsireq.data.p = (uchar*)r->ofcall.data;  //% _..
		  ums.lun[NUM(path)]._scsireq.data.count = r->ifcall.count;  //% _..
		  ums.lun[NUM(path)]._scsireq.data.write = 0;  //% _..
		  n = umsrequest(&ums.lun[NUM(path)], &ums.lun[NUM(path)]._scsireq.cmd,  //% _..
				 &ums.lun[NUM(path)]._scsireq.data, &ums.lun[NUM(path)]._scsireq.status); //% _ _
			ums.lun[NUM(path)].phase = Pstatus;
			if (n == -1) {
				respond(r, "IO error");
				return;
			}
			r->ofcall.count = n;
			break;
		case Pstatus:
		  n = snprint(buf, sizeof(buf), "%11.0ud ", ums.lun[NUM(path)]._scsireq.status); //% _
			if (r->ifcall.count < n)
				n = r->ifcall.count;
			memmove(r->ofcall.data, buf, n);
			r->ofcall.count = n;
			ums.lun[NUM(path)].phase = Pcmd;
			break;
		}
		break;
	case Qdata:
	  if(ums.lun[NUM(path)]._scsireq.lbsize <= 0) {  //% _
			respond(r, "no media on this lun");
			return;
		}
	  bno = r->ifcall.offset / ums.lun[NUM(path)]._scsireq.lbsize;  //% _
	  nb = (r->ifcall.offset + r->ifcall.count + ums.lun[NUM(path)]._scsireq.lbsize - 1)  //% _
	    / ums.lun[NUM(path)]._scsireq.lbsize - bno;  //% _
		if(bno + nb > ums.lun[NUM(path)].blocks)
			nb = ums.lun[NUM(path)].blocks - bno;
		if(bno >= ums.lun[NUM(path)].blocks || nb == 0) {
			r->ofcall.count = 0;
			break;
		}
		if(nb * ums.lun[NUM(path)]._scsireq.lbsize > MaxIOsize)  //% _
		  nb = MaxIOsize / ums.lun[NUM(path)]._scsireq.lbsize;  //% _
		p = malloc(nb * ums.lun[NUM(path)]._scsireq.lbsize);  //% _
		if (p == 0) {
			respond(r, "no mem");
			return;
		}
		ums.lun[NUM(path)]._scsireq.offset = r->ifcall.offset / ums.lun[NUM(path)]._scsireq.lbsize; //% _ _
		n = SRread(&ums.lun[NUM(path)]._scsireq, p, nb * ums.lun[NUM(path)]._scsireq.lbsize);  //% _ _
		if(n == -1) {
			free(p);
			respond(r, "IO error");
			return;
		}
		len = r->ifcall.count;
		offset = r->ifcall.offset % ums.lun[NUM(path)]._scsireq.lbsize;  //% _
		if(offset + len > n)
			len = n - offset;
		r->ofcall.count = len;
		memmove(r->ofcall.data, p + offset, len);
		free(p);
		break;
	}
	respond(r, nil);
}

void
rwrite(Req *r)
{
	int n;
	char *p;
	int bno, nb, len, offset;
	Cmdbuf *cb;
	Cmdtab *ct;
	ulong path;

	n = r->ifcall.count;
	r->ofcall.count = 0;
	path = r->fid->qid.path;
	switch(TYPE(path)) {
	case Qctl:
		cb = parsecmd(r->ifcall.data, n);
		ct = lookupcmd(cb, cmdtab, nelem(cmdtab));
		if(ct == 0) {
			respondcmderror(r, cb, "%r");
			return;
		}
		switch(ct->index) {
		case CMreset:
			umsreset(&ums, 1);
		}
		break;
	case Qraw:
	  if(ums.lun[NUM(path)]._scsireq.lbsize <= 0) {  //% _
			respond(r, "no media on this lun");
			return;
		}
		n = r->ifcall.count;
		switch(ums.lun[NUM(path)].phase) {
		case Pcmd:
			if(n != 6 && n != 10) {
				respond(r, "bad command length");
				return;
			}
			memmove(ums.lun[NUM(path)].rawcmd, r->ifcall.data, n);
			ums.lun[NUM(path)]._scsireq.cmd.p = ums.lun[NUM(path)].rawcmd;  //% _
			ums.lun[NUM(path)]._scsireq.cmd.count = n;  //% _
			ums.lun[NUM(path)]._scsireq.cmd.write = 1;  //% _
			ums.lun[NUM(path)].phase = Pdata;
			break;
		case Pdata:
		        ums.lun[NUM(path)]._scsireq.data.p = (uchar*)r->ifcall.data;  //% _
			ums.lun[NUM(path)]._scsireq.data.count = n;  //% _
			ums.lun[NUM(path)]._scsireq.data.write = 1;  //% _
			n = umsrequest(&ums.lun[NUM(path)], &ums.lun[NUM(path)]._scsireq.cmd,  //% _
				       &ums.lun[NUM(path)]._scsireq.data, &ums.lun[NUM(path)]._scsireq.status);  //% _ _
			ums.lun[NUM(path)].phase = Pstatus;
			if(n == -1) {
				respond(r, "IO error");
				return;
			}
			break;
		case Pstatus:
			ums.lun[NUM(path)].phase = Pcmd;
			respond(r, "phase error");
			return;
		}
		break;	
	case Qdata:
	  if(ums.lun[NUM(path)]._scsireq.lbsize <= 0) {  //% _
			respond(r, "no media on this lun");
			return;
		}
	  bno = r->ifcall.offset / ums.lun[NUM(path)]._scsireq.lbsize;  //% _
	  nb = (r->ifcall.offset + r->ifcall.count + ums.lun[NUM(path)]._scsireq.lbsize-1)  //% _
	    / ums.lun[NUM(path)]._scsireq.lbsize - bno;  //% _
		if(bno + nb > ums.lun[NUM(path)].blocks)
			nb = ums.lun[NUM(path)].blocks - bno;
		if(bno >= ums.lun[NUM(path)].blocks || nb == 0) {
			r->ofcall.count = 0;
			break;
		}
		if(nb * ums.lun[NUM(path)]._scsireq.lbsize > MaxIOsize)  //% _
		  nb = MaxIOsize / ums.lun[NUM(path)]._scsireq.lbsize; //% _
		p = malloc(nb * ums.lun[NUM(path)]._scsireq.lbsize);  //% _
		if(p == 0) {
			respond(r, "no mem");
			return;
		}
		offset = r->ifcall.offset % ums.lun[NUM(path)]._scsireq.lbsize;  //% _
		len = r->ifcall.count;
		if(offset || (len % ums.lun[NUM(path)]._scsireq.lbsize)) {  //% _
		        ums.lun[NUM(path)]._scsireq.offset = r->ifcall.offset / ums.lun[NUM(path)]._scsireq.lbsize; //% _ _ 
			n = SRread(&ums.lun[NUM(path)]._scsireq, p, nb * ums.lun[NUM(path)]._scsireq.lbsize); //% _ _
			if(n == -1) {
				free(p);
				respond(r, "IO error");
				return;
			}
			if(offset + len > n)
				len = n - offset;
		}
		memmove(p+offset, r->ifcall.data, len);
		ums.lun[NUM(path)]._scsireq.offset = r->ifcall.offset / ums.lun[NUM(path)]._scsireq.lbsize;  //% _ _
		n = SRwrite(&ums.lun[NUM(path)]._scsireq, p, nb * ums.lun[NUM(path)]._scsireq.lbsize);  //% _ _
		if(n == -1) {
			free(p);
			respond(r, "IO error");
			return;
		}
		if(offset+len > n)
			len = n - offset;
		r->ofcall.count = len;
		free(p);
		break;
	}
	r->ofcall.count = n;
	respond(r, nil);
}

Srv usbssrv = {
	.attach = rattach,
	.walk1 = rwalk1,
	.open = ropen,
	.read = rread,
	.write = rwrite,
	.stat = rstat,
};

int
findendpoints(Device *d, int *epin, int *epout)
{
	Endpt *ep;
	ulong csp;
	int i, addr, nendpt;

	*epin = *epout = -1;
	nendpt = 0;
	if(d->nconf < 1)
		return -1;
	for(i=0; i<d->nconf; i++) {
		if (d->config[i] == nil)
			d->config[i] = mallocz(sizeof(*d->config[i]),1);
		loadconfig(d, i);
	}
	for(i = 0; i < Nendpt; i++){
		if((ep = d->ep[i]) == nil)
			continue;
		nendpt++;
		csp = ep->csp;
		if(!(Class(csp) == CL_STORAGE && (Proto(csp) == 0x50)))
			continue;
		if(ep->type == Ebulk) {
			addr = ep->addr;
			if(addr & 0x80) {
				if(*epin == -1)
					*epin = addr&0xF;
			} else {
				if(*epout == -1)
					*epout = addr;
			}
		}
	}
	if(nendpt == 0) {
		if(*epin == -1)
			*epin = *epout;
		if(*epout == -1)
			*epout = *epin;
	}
	if (*epin == -1 || *epout == -1)
		return -1;
	return 0;	
}

void (*dprinter[])(Device *, int, ulong, void *b, int n) = {
	[STRING] pstring,
	[DEVICE] pdevice,
};

void
usage(void)
{
	fprint(2, "Usage: %s [-Dd] [-m mountpoint] [-s srvname] [ctrno id]\n", argv0);
	exits("Usage");
}

void
main(int argc, char **argv)
{
	char *srvname, *mntname, buf[64];
	int epin, epout, ctlrno, id, fd;
	Device *d;
	extern int debug;

	mntname = "/n/ums";
	srvname = nil;
	ctlrno = 0;
	id = 1;

	ARGBEGIN{
	case 'd':
		debug = Dbginfo;
		break;
	case 'm':
		mntname = EARGF(usage());
		break;
	case 's':
		srvname = EARGF(usage());
		break;
	case 'D':
		++chatty9p;
		break;
	default:
		usage();
	}ARGEND

	if(argc == 0) {
		for(ctlrno = 0; ctlrno < 16; ctlrno++) {
			for(id = 1; id < 128; id++) {
				sprint(buf, "/dev/usb%d/%d/status", ctlrno, id);
				fd = open(buf, OREAD);
				if(fd < 0)
					break;
				read(fd, buf, sizeof(buf));
				buf[63] = '\0';
				close(fd);
				if(strstr(buf, "0x500508") != nil || strstr(buf, "0x500608") != nil)
					goto found;
			}
		}
		sysfatal("No usb storage device found");
	} else if(argc == 2 && isdigit(argv[0][0]) && isdigit(argv[1][0])) {
		ctlrno = atoi(argv[0]);
		id = atoi(argv[1]);
	} else
		usage();
found:

	DBGPRN("umsfs{srvname=%s mntname=%s ctlrno=%d id=%d\n", 
	       srvname, mntname, ctlrno, id);
	
	d = opendev(ctlrno, id);
	if(describedevice(d) < 0)
		sysfatal("%r");
	if(findendpoints(d, &epin, &epout) < 0)
		sysfatal("bad usb configuration");
	closedev(d);
	//	DBGPRN("#1# ");
	
	ums.ctlfd = -1;
	snprint(ums.dev, sizeof(ums.dev), "/dev/usb%d/%d", ctlrno, id);
	if(umsinit(&ums, epin, epout) < 0)
		sysfatal("device error");
	DBGPRN("#2# ");

	starttime = time(0);
	DBGPRN("#2# ");

	owner = getuser();

	DBGPRN("postmntsrv\n");
	postmountsrv(&usbssrv, srvname, mntname, 0);

	exits(0);
}

