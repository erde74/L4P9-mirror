//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*  (C)
 *  (C2) KM
 */

#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <bio.h>
#include "scsireq.h"
#include "../lib/usb.h"  //%

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

enum {
	Qdir = 0,
	Qctl,
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
	Qid		qid;
	int		mode;
};
Dirtab dirtab[] = {  //%
       {".",	{Qdir,0,QTDIR},	DMDIR|0555},
       {"ctl",	{Qctl},		0640},
       {"raw",	{Qraw},		0640},
       {"data",	{Qdata},	0640},
};

Cmdtab cmdtab[] = {   //%
       {CMreset,	"reset",	1},
};

typedef struct Ums Ums;
struct Ums {
        ScsiReq  _scsireq;   //%
	ulong	blocks;
	vlong	capacity;
	int		fd2;
	int		setupfd;
	int		ctlfd;
	uchar	rawcmd[10];
	uchar	phase;
	uchar	epin;
	uchar	epout;
};

Ums ums;
long starttime;
char *owner;
static int noreset;		/* flag: if true, drive freaks out if reset */
extern int debug;

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

static void
unstall(Ums *ums, int ep)
{
	uchar req[16];
	int n;

	n = snprint((char*)req, sizeof req, "unstall %d", ep&0xF);
	if (write(ums->ctlfd, req, n) != n) fprint(2, "ctl write failed\n");
	n = snprint((char*)req, sizeof req, "data %d 0", ep&0xF);
	if (write(ums->ctlfd, req, n) != n) fprint(2, "ctl write failed\n");

	req[0] = 2;				// RH2D | Rstandard | Rendpt
	req[1] = 1;				// CLEAR_FEATURE
	req[2] = req[3] = 0;	// ENDPOINT_HALT
	req[4] = ep; req[5] = 0;	// endpoint
	req[6] = req[7] = 0;	// count
	if (write(ums->setupfd, req, 8) != 8) fprint(2, "setup write failed\n");
}

static void
umsreset(Ums *ums)
{
	uchar req[8];

	req[0] = (1<<5)|1;		// RH2D | Rclass | Rinterface
	req[1] = 0xFF;			// bulk-only reset
	req[2] = req[3] = 0;	// value
	req[4] = req[5] = 0;	// index = interface number?
	req[6] = req[7] = 0;	// count
	/*
	 * umsreset freaks out some thumb drives (e.g., geek squad
	 * a.k.a. san disk sdcz2 cruzer mini flash drive), and once
	 * they freak out, they're unusable until you remove them and
	 * plug them in again, so it seems to be impossible to recover
	 * from a reset in software.
	 */
	if (!noreset && write(ums->setupfd, req, 8) != 8)
		fprint(2, "umsreset setup write failed\n");

	unstall(ums, ums->epin|0x80);
	unstall(ums, ums->epout);
}

long
umsrequest(void *u, ScsiPtr *cmd, ScsiPtr *data, int *status)
{
	Cbw cbw;
	Csw csw;
	static int seq = 0;
	int n;
	Ums *ums;

	ums = u;
	memcpy(cbw.signature, "USBC", 4);
	cbw.tag = ++seq;
	cbw.datalen = data->count;
	cbw.flags = data->write ? CbwDataOut : CbwDataIn;
	cbw.lun = ums->_scsireq.lun;  //%
	cbw.len = cmd->count;
	memcpy(cbw.command, cmd->p, cmd->count);
	memset(cbw.command+cmd->count, 0, sizeof(cbw.command)-cmd->count);
	
	if (debug) {
		fprint(2, "cmd:");
		for (n = 0; n < cbw.len; n++)
			fprint(2, " %2.2x", cbw.command[n]&0xFF);
		fprint(2, " datalen: %ld\n", cbw.datalen);
	}
	if(write(ums->fd2, &cbw, CbwLen) != CbwLen){
		fprint(2, "usbscsi: write cmd: %r\n");
		goto reset;
	}
	if(data->count != 0) {
		if(data->write)
			n = write(ums->fd2, data->p, data->count);
		else
		  n = read(ums->_scsireq.fd, data->p, data->count);  //%
		if(n == -1){
			fprint(2, "usbscsi: data %sput: %r\n", data->write? "out" : "in");
			if(data->write)
				unstall(ums, ums->epout);
			else
				unstall(ums, ums->epin|0x80);
		}
	}
	n = read(ums->_scsireq.fd, &csw, CswLen);   //%
	if(n == -1){
		unstall(ums, ums->epin|0x80);
		n = read(ums->_scsireq.fd, &csw, CswLen);  //%
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
	if (debug) {
		fprint(2, "status: %2.2ux residue: %ld\n", csw.status, csw.dataresidue);
		if (cbw.command[0] == ScmdRsense) {
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
	umsreset(ums);
	*status = STharderr;
	return -1;
}

int
umsinit(Ums *ums, char *usbdir, int epin, int epout, int lun)
{
	uchar data[8];
	char fin[128];

	memset(ums, 0, sizeof(*ums));
	snprint(fin, sizeof fin, "%s/ctl", usbdir);
	if ((ums->ctlfd = open(fin, OWRITE)) == -1)
		return -1;
	if (epin == epout) {
		if (fprint(ums->ctlfd, "ep %d bulk rw 64 16", epin) < 0)
			return -1;
	} else {
		if (fprint(ums->ctlfd, "ep %d bulk r 64 16", epin) < 0)
			return -1;
		if (fprint(ums->ctlfd, "ep %d bulk w 64 16", epout) < 0)
			return -1;
	}
	snprint(fin, sizeof fin, "%s/ep%ddata", usbdir, epin);
	if((ums->_scsireq.fd = open(fin, OREAD)) == -1)  //%
		return -1;
	snprint(fin, sizeof fin, "%s/ep%ddata", usbdir, epout);
	if((ums->fd2 = open(fin, OWRITE)) == -1)
		return -1;
	snprint(fin, sizeof fin, "%s/setup", usbdir);
	if ((ums->setupfd = open(fin, ORDWR)) == -1)
		return -1;
	ums->_scsireq.flags = Fopen|Fusb|Frw10;   //%
	ums->epin = epin;
	ums->epout = epout;
	ums->_scsireq.ums = ums;	//%	// to get from ScsiReq to enclosing Ums
	ums->_scsireq.lun = lun;        //%
	umsreset(ums);
	if (SRinquiry(&ums->_scsireq) == -1)  //%
			return -1;
	SRstart(&ums->_scsireq, 1);          //%
	if (SRrcapacity(&ums->_scsireq, data) == -1 && SRrcapacity(&ums->_scsireq, data) == -1)   //%
			return -1;
	ums->_scsireq.lbsize = (data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7];    //%
	ums->blocks = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
	ums->blocks++;		// SRcapacity returns LBA of last block
	ums->capacity = (vlong)ums->blocks * ums->_scsireq.lbsize;    //%
	return 0;
}

void
rattach(Req *r)
{
	r->ofcall.qid = dirtab[Qdir].qid;
	r->fid->qid = r->ofcall.qid;
	respond(r, nil);
}

char*
rwalk1(Fid *fid, char *name, Qid *qid)
{
	int i;

	for (i = 0; i < nelem(dirtab); i++) {
		if (!strcmp(name, dirtab[i].name)) {
			*qid = dirtab[i].qid;
			fid->qid = *qid;
			return 0;
		}
	}
	return "file does not exist";
}

void
dostat(int n, Dir *dir)
{
	Dirtab *d;

	d = &dirtab[n];
	dir->qid = d->qid;
	dir->mode = d->mode;
	dir->atime = dir->mtime = starttime;
	dir->name = estrdup9p(d->name);
	dir->uid = estrdup9p(owner);
	dir->gid = estrdup9p(owner);
	if (n == Qdata)
		dir->length = ums.capacity;
}

int
dirgen(int n, Dir *dir, void* _x)  //%
{
	if (++n >= nelem(dirtab))
		return -1;
	dostat(n, dir);
	return 0;
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
	switch ((long)r->fid->qid.path ){
	case Qraw:
		ums.phase = Pcmd;
		break;
	}
	respond(r, nil);
}

void
rread(Req *r)
{
	char buf[128];
	int n;
	char *p;
	int bno, nb, len, offset;

	switch ((long)r->fid->qid.path) {
	case Qdir:
		dirread9p(r, dirgen, 0);
		break;
	case Qctl:
		n = 0;
		if (ums._scsireq.flags & Finqok)    //%
		        n = snprint(buf, sizeof buf, "inquiry %.48s\n", (char*)ums._scsireq.inquiry+8); //%
		n += snprint(buf+n, sizeof buf-n, "geometry %ld %ld\n", ums.blocks, ums._scsireq.lbsize); //%
		readbuf(r, buf, n);
		break;
	case Qraw:
		switch (ums.phase) {
		case Pcmd:
			respond(r, "phase error");
			return;
		case Pdata:
		        ums._scsireq.data.p = (uchar*)r->ofcall.data;   //%
			ums._scsireq.data.count = r->ifcall.count;   //%
			ums._scsireq.data.write = 0;           //%
			n = umsrequest(&ums, &ums._scsireq.cmd, &ums._scsireq.data, &ums._scsireq.status);  //%
			ums.phase = Pstatus;
			if (n == -1) {
				respond(r, "IO error");
				return;
			}
			r->ofcall.count = n;
			break;
		case Pstatus:
		        n = sprint(buf, "%11.0ud ", ums._scsireq.status);   //%
			if (r->ifcall.count < n)
				n = r->ifcall.count;
			memmove(r->ofcall.data, buf, n);
			r->ofcall.count = n;
			ums.phase = Pcmd;
			break;
		}
		break;
	case Qdata:
	        bno = r->ifcall.offset/ums._scsireq.lbsize;     //%
		nb = (r->ifcall.offset+r->ifcall.count+ums._scsireq.lbsize-1)/ums._scsireq.lbsize - bno;   //%
		if (bno+nb > ums.blocks)
			nb = ums.blocks - bno;
		if (bno >= ums.blocks || nb == 0) {
			r->ofcall.count = 0;
			break;
		}
		if (nb*ums._scsireq.lbsize > MaxIOsize)    //%
		        nb = MaxIOsize / ums._scsireq.lbsize;   //%
		p = malloc(nb * ums._scsireq.lbsize);      //%
		if (p == 0) {
			respond(r, "no mem");
			return;
		}
		ums._scsireq.offset = r->ifcall.offset / ums._scsireq.lbsize;   //%
		n = SRread(&ums._scsireq, p, nb*ums._scsireq.lbsize);           //%
		if (n == -1) {
			free(p);
			respond(r, "IO error");
			return;
		}
		len = r->ifcall.count;
		offset = r->ifcall.offset % ums._scsireq.lbsize;    //%
		if (offset+len > n)
			len = n - offset;
		r->ofcall.count = len;
		memmove(r->ofcall.data, p+offset, len);
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

	n = r->ifcall.count;
	r->ofcall.count = 0;
	switch ((long)r->fid->qid.path) {
	case Qctl:
		cb = parsecmd(r->ifcall.data, n);
		ct = lookupcmd(cb, cmdtab, nelem(cmdtab));
		if (ct == 0) {
			respondcmderror(r, cb, "%r");
			return;
		}
		switch (ct->index) {
		case CMreset:
			umsreset(&ums);
		}
		break;
	case Qraw:
		n = r->ifcall.count;
		switch (ums.phase) {
		case Pcmd:
			if ( n != 6 && n != 10) {
				respond(r, "bad command length");
				return;
			}
			memmove(ums.rawcmd, r->ifcall.data, n);
			ums._scsireq.cmd.p = ums.rawcmd;    //%
			ums._scsireq.cmd.count = n;         //%
			ums._scsireq.cmd.write = 1;         //%
			ums.phase = Pdata;
			break;
		case Pdata:
		        ums._scsireq.data.p = (uchar*)r->ifcall.data;   //%
			ums._scsireq.data.count = n;     //%
			ums._scsireq.data.write = 1;     //%
			n = umsrequest(&ums, &ums._scsireq.cmd, &ums._scsireq.data, &ums._scsireq.status);   //%
			ums.phase = Pstatus;
			if (n == -1) {
				respond(r, "IO error");
				return;
			}
			break;
		case Pstatus:
			ums.phase = Pcmd;
			respond(r, "phase error");
			return;
		}
		break;	
	case Qdata:
	        bno = r->ifcall.offset/ums._scsireq.lbsize;   //%
		nb = (r->ifcall.offset+r->ifcall.count+ums._scsireq.lbsize-1)/ums._scsireq.lbsize - bno;  //%
		if (bno+nb > ums.blocks)
			nb = ums.blocks - bno;
		if (bno >= ums.blocks || nb == 0) {
			r->ofcall.count = 0;
			break;
		}
		if (nb*ums._scsireq.lbsize > MaxIOsize)    //%
		  nb = MaxIOsize / ums._scsireq.lbsize;    //%
		p = malloc(nb * ums._scsireq.lbsize);      //%
		if (p == 0) {
			respond(r, "no mem");
			return;
		}
		offset = r->ifcall.offset % ums._scsireq.lbsize;   //%
		len = r->ifcall.count;
		if (offset || (len%ums._scsireq.lbsize)) {
		  ums._scsireq.offset = r->ifcall.offset / ums._scsireq.lbsize;   //%
		  n = SRread(&ums._scsireq, p, nb*ums._scsireq.lbsize);     //%
			if (n == -1) {
				free(p);
				respond(r, "IO error");
				return;
			}
			if (offset+len > n)
				len = n - offset; 
		}
		memmove(p+offset, r->ifcall.data, len);
		ums._scsireq.offset = r->ifcall.offset / ums._scsireq.lbsize;   //%
		n = SRwrite(&ums._scsireq, p, nb*ums._scsireq.lbsize);     //%
		if (n == -1) {
			free(p);
			respond(r, "IO error");
			return;
		}
		if (offset+len > n)
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
	int i, addr;

	*epin = *epout = -1;
	if (d->nconf < 1)
		return -1;
	for(i=0; i<d->nconf; i++) {
		if (d->config[i] == nil)
			d->config[i] = mallocz(sizeof(*d->config[i]),1);
		loadconfig(d, i);
	}
	for(i = 0; i < Nendpt; i++){
		if ((ep = d->ep[i]) == nil)
			continue;
		csp = ep->csp;
		if(!(Class(csp) == CL_STORAGE && Proto(csp) == 0x50))
			continue;
		if(ep->type == Ebulk) {
			addr = ep->addr;
			if (addr&0x80) {
				if(*epin == -1)
					*epin = addr&0xF;
			} else {
				if(*epout == -1)
					*epout = addr;
			}
		}
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
	fprint(2, "Usage: %s [-rsD] [-m mountpoint] [-l lun] [ctrno id]\n", argv0);
	exits("Usage");
}

void
main(int argc, char **argv)
{
        //      char *srvname = "ums";  //%
	char *srvname = nil;
	char *mntname = "/ums";    //% "/n/ums"
	char *p;
	int stdio = 0;
	int epin, epout;
	int ctlrno = 0, id = 0;
	int lun = 0;
	Biobuf *f;
	Device *d;
	char buf[64];
	extern int debug;

	ARGBEGIN{
	case 'd':
		debug = Dbginfo;
		break;
	case 's':
		++stdio;
		break;
	case 'm':
		mntname = ARGF();
		break;
	case 'D':
		++chatty9p;
		break;
	case 'r':
		++noreset;
		break;
	case 'l':
		lun = atoi(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	if (argc == 0) {
		for (ctlrno = 0; ctlrno < 16; ctlrno++) {
			for (id = 1; id < 128; id++) {
				sprint(buf, "/dev/usb%d/%d/status", ctlrno, id);
				f = Bopen(buf, OREAD);
				if (f == 0)
					break;
				while ((p = Brdline(&f->_Biobufhdr, '\n')) != 0) {    //%
				         p[Blinelen(&f->_Biobufhdr)-1] = '\0';         //%
					if (strstr(p, "0x500508") != 0 || strstr(p, "0x500608") != 0) {
					        Bterm(&f->_Biobufhdr);     //%
						goto found;
					}
				}
				Bterm(&f->_Biobufhdr);    //%
			}
		}
		sysfatal("No usb storage device found");
	} else if (argc == 2 && isdigit(argv[0][0]) && isdigit(argv[1][0])) {
		ctlrno = atoi(argv[0]);
		id = atoi(argv[1]);
	} else
		usage();
found:

	fprint(2, "usbsfs {ctlrno:%d, id:%d, /dev/usb%d/%d}\n", 
	       ctlrno, id, ctlrno, id);  //%
	d = opendev(ctlrno, id);
	if (describedevice(d) < 0)
		sysfatal("%r");
	if (findendpoints(d, &epin, &epout) < 0)
		sysfatal("bad usb configuration");

	sprint(buf, "/dev/usb%d/%d", ctlrno, id);
	if (umsinit(&ums, buf, epin, epout, lun) < 0)
		sysfatal("device error");
	starttime = time(0);
	owner = getuser();
	if (stdio)
		srv(&usbssrv);
	else
	        postmountsrv(&usbssrv, nil, mntname, MAFTER | MCREATE);
	        // postmountsrv(&usbssrv, srvname, mntname, 0);
	        //% postmountsrv(&usbssrv, srvname, mntname, MAFTER | MCREATE);

	//%	exits(0);
	L4_Sleep(L4_Never);  //%
}

