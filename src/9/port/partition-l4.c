#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../pc/io.h"
#include "ureg.h"
#include "../port/error.h"
#include "../port/sd.h"

#include  "../errhandler-l4.h"

//%begin-----------------------
#define  _DBGFLG   0
#include <l4/_dbgpr.h>
#define  PRN  l4printf_g
//%end--------------------------


//----------------------------------------------
typedef struct Dospart  Dospart;
struct Dospart
{
  uchar flag;             /* active flag */
  uchar shead;            /* starting head */
  uchar scs[2];           /* starting cylinder/sector */
  uchar type;             /* partition type */
  uchar ehead;            /* ending head */
  uchar ecs[2];           /* ending cylinder/sector */
  uchar start[4];         /* starting sector */
  uchar len[4];           /* length in sectors */
};

#define FAT12   0x01
#define FAT16   0x04
#define EXTEND  0x05
#define FATHUGE 0x06
#define FAT32   0x0b
#define FAT32X  0x0c
#define EXTHUGE 0x0f
#define DMDDO   0x54
#define PLAN9   0x39
#define LEXTEND 0x85
#define EXT2    0x83

#define   GLONG(p)  (((p)[3]<<24) | ((p)[2]<<16) | ((p)[1]<<8) | (p)[0])


//%------------------------------------------------
uchar mbrbuf[512];
int nbuf;

int isdos(int t)
{
  return t==FAT12 || t==FAT16 || t==FATHUGE || t==FAT32 || t==FAT32X;
}

int isextend(int t)
{
  return t==EXTEND || t==EXTHUGE || t==LEXTEND;
}

int mbrpartition(SDunit  *unit)
{
    Dospart *dp;
    ulong taboffset, type, start, len, end;
    ulong firstxpart, nxtxpart;
    int   i, ndos = 0, next2 = 0, nplan9 = 0;
    char name[10];
    int  rc;

    taboffset = 0;
    dp = (Dospart*)&mbrbuf[0x1BE];

    // Read the partitions, first from the MBR and then
    // from successive extended partition tables.

    firstxpart = 0;
    for(;;) {
        if((rc = unit->dev->ifc->bio(unit, 0, 0, mbrbuf, 1, 0)) <  0)
            return -1;

	if (mbrbuf[510] != 0x55 || mbrbuf[511] != 0xAA) {
	    l4printf("No partition table set in MBR\n");
	    return -2;
	}

	l4printf_c(63, "MBR-check<%x %x> \n", mbrbuf[510], mbrbuf[511]);

	nxtxpart = 0;
	for(i=0; i<4; i++) {
	    type = dp[i].type;
	    start = taboffset + GLONG(dp[i].start);
	    len = GLONG(dp[i].len);
	    end = start + len;

	    l4printf_c(63, "type=%x start=%d end=%d len=%d \n", type, start, end, len);

	    if (type == PLAN9){
	        sprint(name, "plan9-%d", nplan9++);
		sdaddpart(unit, name, start, end);
	    }
	    else  if (type == EXT2){
	        sprint(name, "ext2-%d", next2++);
		sdaddpart(unit, name, start, end);
	    }
	    else if (isdos(type)){
	        sprint(name, "dos-%d", ndos++);
		sdaddpart(unit, name, start, end);
	    }

	    /* nxtxpart is relative to firstxpart (or 0), not taboffset */
	    if(isextend(dp[i].type)){
	        nxtxpart = start-taboffset+firstxpart;
		// l4printf_c(63, " link %x\n", nxtxpart);
	    }
	}

	if(1 || !nxtxpart)
	    break;
	if(!firstxpart)
	    firstxpart = nxtxpart;
	taboffset = nxtxpart;
    }
    return rc;
}
