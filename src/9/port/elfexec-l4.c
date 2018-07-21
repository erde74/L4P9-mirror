/****************************************************
 *        elfexec-l4.c                              *
 *                                                  *
 *  Copyright (c) 2000 University of Karlsruhe,     *
 *  (C) 2008 NII KM                                 *
 ****************************************************/

#include  "u.h"
#include  "../port/lib.h"
#include  "../pc/mem.h"
#include  "../pc/dat.h"

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>
#include  "elf.h"
#include  "../errhandler-l4.h"

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>

#define   MAX_THD_NUM  32

static L4_ThreadId_t  _mx_pager;
static L4_ThreadId_t  _myself;
static char    segbuf[8192];

#define  IS_ELF   -7  

#define PRN_ERR_RTN(txt) {return  ONERR;}
//#define PRN_ERR_RTN(txt) {l4printf_r("Err: %s\n", txt); return ONERR;}

extern void dump_mem(char *title, unsigned char *start, unsigned size);
extern void dump_proc_mem(char *title, int procnr, uint adrs, int len);
extern Chan*  namec(char*, int, int, ulong);
static int  exec_mkarg(L4_ThreadId_t from_tid, unsigned from_argtbl, int argtbl_size,
			L4_ThreadId_t to_tid, unsigned to_argtbl);
static int shell_mkarg(L4_ThreadId_t from_tid, unsigned from_argtbl, int argtbl_size,
		       char  *arglist2[],
		       L4_ThreadId_t to_tid, unsigned to_argtbl);

typedef struct {
	  int       size;
	  int       argc;
	  void*     argv;
	  void*     argp[0];  // To be refined.
}  argtbl_t;

#define PRN l4printf_g
void dump_argtbl(char *title, char *xp)
{ 
    int  i, size;
    argtbl_t *ap = (argtbl_t *)xp;
    char *cp = &ap->argp[0];
    char ch;
    size = ap->size;
    l4printf_g("ARGTBL:%s %x [%x:%d:%d] {", title, ap, ap->size, ap->argc, ap->argv);
    size -= 12;
    if (size>32) size = 32; 
    for(i = 0; i < size; i++)
      { ch = cp[i]; if (ch<32 || ch>126) ch = '.'; l4printf_g("%c", ch);  }
    l4printf_g("}\n");
}

static int _pread(Chan *chan, char *buf, int size, vlong pos)
{
    return devtab[chan->type]->read(chan, buf, size, pos);
} 

static char *_strcpy(char *ret, const char *s2)
{
    char *s1 = ret;
    while ((*s1++ = *s2++))  /* EMPTY */ ;
    return ret;
}

//---------------------------------------------------
typedef  struct {
  int         text_size;
  int         data_size;
  Elf32_Addr  bss_base;
  int         bss_size;
  int         total_size;
  Elf32_Addr  entry;
  int         seg_num; 
  Elf32_Phdr  segment[10];
} load_info_t;

load_info_t  load_info;

//----------------------------------------------------
void print_filehdr(Elf32_Ehdr  file_hdr)
{
  char * etype;
  switch(file_hdr.e_type) {
    case 0: etype = "No-file-type";  break;
    case 1: etype = "Relocatable-file";  break;
    case 2: etype = "Executable-file";   break;
    case 3: etype = "Shared-object-file";  break;
    case 4: etype = "Core-file";  break;
    case 5: etype = "Num-of-define-types"; break;
    default: etype = "File-?";
  }
  l4printf("ELFinfo type:%s entry=%x phoff=%x shoff=%x  \n"
	   "        flags=%x phnum=%x shnum=%x \n",
	 etype, file_hdr.e_entry, file_hdr.e_phoff, file_hdr.e_shoff,
	 file_hdr.e_flags, file_hdr.e_phnum, file_hdr.e_shnum); 
}

//-------------------------------------------------------
void print_phdr(Elf32_Phdr  phdr) 
{	
  char   pflag[4], *ptype;
  switch(phdr.p_type)  {
    case 0:  ptype = "PT_NULL";  break;
    case 1:  ptype = "PT_LOAD";  break;
    case 2:  ptype = "PT_DYNAMIC";  break;
    case 3:  ptype = "PT_INTERP";  break;
    case 4:  ptype = "PT_NOTE";  break;
    case 5:  ptype = "PT_SHLIB";  break;
    case 6:  ptype = "PT_PHDR";  break;
    case 7:  ptype = "PT_NUM";  break;
    case 0x70000000:  ptype = "PT_LOPROC";  break;
    case 0x7fffffff:  ptype = "PT_HIPROC";  break;
    default: ptype = "PT_?";
  }

  _strcpy(pflag, "---");
  if (phdr.p_flags & 0x01)   pflag[0] = 'X';
  if (phdr.p_flags & 0x02)   pflag[1] = 'W';
  if (phdr.p_flags & 0x04)   pflag[2] = 'R';
  l4printf("> %s %s offset=%x v/p-addr=[%x,%x] f/m-size=[%x,%x] align=%x\n",
	 pflag,  ptype,  phdr.p_offset,  phdr.p_vaddr,
	 phdr.p_paddr,  phdr.p_filesz, phdr.p_memsz, phdr.p_align);
  
  //----- Zero clear BSS segments --------------------
  if (phdr.p_memsz > phdr.p_filesz) { 
      int zero_base = phdr.p_vaddr + phdr.p_filesz;
      int zero_size = phdr.p_memsz - phdr.p_filesz;
      l4printf("       data=[%x, %x]   bss=[%x, %x]\n",
	     phdr.p_vaddr, phdr.p_filesz, zero_base, zero_size);
  } //if
} 


//----------------------------------------------------------
/*  Returns
 *   ONERR < 0: error
 *   IS_ELF< 0: ELF file heade
 *   n > 0    : length of "#!/bin/rc...."
 */

int elf_header(Chan *chan, load_info_t * linfo, char *line)  // ONERR
{ 
    Elf32_Ehdr   file_hdr; 
    Elf32_Phdr   phdr; 
    //    Elf32_Shdr   shdr;
    int          size, i, n; 
    vlong        pos = 0;

    DBGPRN("> %s(%x %x)\n", __FUNCTION__, chan, linfo);
    linfo->text_size = linfo->data_size = linfo->bss_size = 0;
    linfo->total_size = linfo->seg_num = 0;
    linfo->entry = 0;

    //----- File hdr --------------------------------
    size = sizeof(file_hdr); 
    n = _pread(chan, (char*)&file_hdr, size, pos);
    if (n < 2)  PRN_ERR_RTN("cannot read file header"); 
    // dump_mem("ehdr", &file_hdr, 128);  //DBGBRK("pos=%x\n", pos);  

    if (line) {
        if (strncmp((char*)&file_hdr, "#!", 2) == 0){
	    memmove(line,  (char*)&file_hdr, size); 
	    return  n;
	}
    }

    if (n < size)  PRN_ERR_RTN("cannot read file header"); 
    
    if (file_hdr.e_ident[0] !=  0x7f   || file_hdr.e_ident[1] !=  'E' 
        || file_hdr.e_ident[2] !=  'L' || file_hdr.e_ident[3] !=  'F')  
      PRN_ERR_RTN("this is NOT an ELF file");   
    
    if (file_hdr.e_type != ET_EXEC)  PRN_ERR_RTN("unexpected e_type"); 
    if (file_hdr.e_machine != EM_386) PRN_ERR_RTN("not an intel binary"); 
    if (file_hdr.e_version != EV_CURRENT)  PRN_ERR_RTN("version mismatch?"); 
    if (file_hdr.e_flags != 0)  PRN_ERR_RTN("unexpected flags?"); 
    if (file_hdr.e_phnum <= 0)  PRN_ERR_RTN("No loadable program sections"); 

    //----- Analyze ELF headers ----------------------
    //  print_filehdr(file_hdr);  
    linfo->entry = file_hdr.e_entry;

    //---------- Program HDR ---------------------
    for (i = 0; i < file_hdr.e_phnum; i++)  { 
	int  n;

	pos = file_hdr.e_phoff + i*sizeof(phdr);
        size = sizeof(phdr); 
        if ((n=_pread(chan, (char*)&phdr, size, pos))<size) PRN_ERR_RTN("can't read phdr"); 
	// dump_mem("phdr", &phdr, 128);  //DBGBRK("offset=%x\n", pos);  //
	pos += n;

	// print_phdr(phdr);  //--------------------

        if (phdr.p_type == PT_LOAD)  {	
	    int indx = linfo->seg_num ++;
	    if (indx == 10)  PRN_ERR_RTN("ELF seg-num overflow");
 
	    linfo->segment[indx] = phdr;
	    if (phdr.p_flags & PF_X) { // Executable
		linfo->text_size += phdr.p_memsz;
		linfo->total_size += phdr.p_memsz;
	    }
	    else 
	    if ((phdr.p_flags & PF_W) || (phdr.p_flags & PF_R))  {
		if (phdr.p_memsz > phdr.p_filesz) { //incl. BSS
		    linfo->data_size += phdr.p_memsz;
		    linfo->bss_base = phdr.p_vaddr + phdr.p_filesz;
		    linfo->bss_size = phdr.p_memsz - phdr.p_filesz;
		    linfo->total_size += phdr.p_memsz;
		}
		else {
		    linfo->data_size += phdr.p_memsz;
		    linfo->total_size += phdr.p_memsz;
		}
	    }
	} //if
    }	//for

#if 0  //---------- Section HDR ---------------------
    for (i = 0; i < file_hdr.e_shnum; i++) { 
	int  n;
	if (seek(fd, file_hdr.e_shoff + i*sizeof(shdr), 0) < 0) 
	l4printf(" P2 seek <%x> fail\n", file_hdr.e_shoff + i*sizeof(shdr)); 
	pos = file_hdr.e_shoff + i*sizeof(shdr);
        size = sizeof(shdr); 
        if ((n=_pread(fd, &shdr, size, pos)) < size) PRN_ERR_RTN("P2 cannot read shdr"); 
	pos += n;
        if ( 1 /* shdr.sh_type == PT_LOAD */ )  {	 
            size = (int)shdr.sh_size; 
	    if (seek(fd, shdr.sh_offset, 0) < 0)
	          l4printf(" P3 seek <%x> fail \n", shdr.sh_offset);
	    pos = shdr.sh_offset;
	    l4printf(" Section-HDR[%d] name=%x  type= %d  flag=%x  addr=%x \n"
		   "  offset= %x  size= %d  link= %x  entsz= %d  \n\n",
		   i, shdr.sh_name,  shdr.sh_type,  shdr.sh_flags,
		   shdr.sh_addr,  shdr.sh_offset, shdr.sh_size,
		   shdr.sh_link,  shdr.sh_entsize);
	} //if
    } //for
#endif   //----------------------------------------
    DBGPRN("< %s()\n", __FUNCTION__);
    return  IS_ELF;
}


/*====================================================*
 |    start_prog( )                                   |
 *====================================================*/
static int _create_task(L4_ThreadId_t tasktid, int max_thd_num, 
			L4_Word_t kip, L4_Word_t utcb, L4_ThreadId_t  pager)
{ 
    L4_ThreadId_t scheduler = pager;
    L4_Fpage_t kip_fp = L4_Fpage((unsigned int)kip, 4096);    
    int    utcb_size = (max_thd_num * 0x200 + 4095) & 0xFFFFF000;
    L4_Fpage_t utcb_fp = L4_Fpage(utcb, utcb_size);

    if(requestThreadControl(tasktid, tasktid, scheduler, L4_nilthread, (~0UL))==0)
        PRN_ERR_RTN("start_prog 1");
    if(requestSpaceControl(tasktid, 0, kip_fp, utcb_fp, L4_nilthread) == 0)
        PRN_ERR_RTN("start_prog 2");
    if(requestThreadControl(tasktid, tasktid, scheduler, pager, utcb)==0)
        PRN_ERR_RTN("start_prog 3");
    return 1;
}


int start_prog(Proc * pcb, L4_ThreadId_t tasktid, char *fname, 
	       L4_ThreadId_t client, 
	       unsigned  argtbl_adrs, int argtbl_size, int isSpawn) // ONERR
{ 
    Elf32_Phdr   phdr; 
    int         size,   i; 
    unsigned    pos;
    unsigned    kip = KIP_BASE;  
    unsigned    utcb = UTCB_BASE;
    unsigned    argtbl_base;
    Chan      * chan;
    int         rc, n;
    char        line[sizeof(Elf32_Ehdr)];
    char      * progarg[sizeof(Elf32_Ehdr)], *elem = nil, progelem[64];
    int         indirect = 0;

    _mx_pager = L4_Pager();
    _myself = L4_Myself();

    DBGPRN(">%s(%x '%s' %x %x %x %x)\n", __FUNCTION__,
	   tasktid, fname, client.raw, argtbl_adrs, argtbl_size, isSpawn);

    for(;;){
        chan = namec(fname, Aopen, OEXEC, 0);
	if (chan == nil ) {
	  //l4printgetc("! start_prog(): cannot open '%s'\n", fname);
	    PRN_ERR_RTN("#30");
	}
	DBGPRN("chan=%s \n", chan->path->s); 

	//---- Read in and analyze ELF-header ---------
	if (!indirect){
	    kstrdup(&elem, up->genbuf);  // To be reconsidered
	    //	    print("  elem='%s'\n", elem);
	}

        rc = elf_header(chan, & load_info, line);
	if (rc == ONERR) { 
	    PRN_ERR_RTN("#40"); 
	}
	else if (rc == IS_ELF)  // ELF binary file
	    break;
	else {  // shell script
	    // "#!/bin/rc ........"
	    n = shargs(line, rc, progarg);
	    if (n == 0)  return ONERR;
	    indirect = 1;
	    progarg[n++] = fname;
	    progarg[n] = 0;
	    fname = progarg[0];
	    if (strlen(elem) >= sizeof progelem)
	      PRN_ERR_RTN("#50");
	    strcpy(progelem, elem);
	    progarg[0] = progelem;
	    // l4printgetc("!-20 start_prog(): fname '%s'\n", fname);
	    cclose(chan);
	    //for(i=0; i<n; i++) print("--arg[%d]:'%s'\n", i, progarg[i]);
	}
    }//for

    if(isSpawn) {  // spawn
      if (_create_task(tasktid, MAX_THD_NUM, kip, utcb, _mx_pager) == ONERR)
	  PRN_ERR_RTN("#69"); 
    }
    else {  // exec
        rc = requestThreadControl(tasktid, L4_nilthread, 
				  L4_nilthread, L4_nilthread, -1);
	if (rc == 0) PRN_ERR_RTN("start_prog");
	if (_create_task(tasktid, MAX_THD_NUM, kip, utcb, _mx_pager) == ONERR)
	    PRN_ERR_RTN("#70"); 
    }

//dump_proc_mem("X2", TID2PROCNR(client), argtbl_adrs, argtbl_size); //%

    argtbl_base = (unsigned)(load_info.entry - 8192) & 0xfffff800; 
    // aligned at 2K boundary; Optimization is left for your work.

    if (indirect)
        shell_mkarg(client, argtbl_adrs, argtbl_size, progarg, tasktid, argtbl_base);
    else
        exec_mkarg(client, argtbl_adrs, argtbl_size, tasktid, argtbl_base);

    kstrdup(&pcb->text, elem); //! To be reconsidered

    for (i = 0; i < load_info.seg_num; i++) { 
	phdr = load_info.segment[i];
	// print_phdr(phdr); 

	size = (int)phdr.p_filesz; 
	pos = phdr.p_offset;

	{ 
	    int  remain = size;
	    int  p_vaddr = (int)phdr.p_vaddr;
	    int  n, sz;
	    while (remain > 0) {
		sz =  (remain >= sizeof(segbuf)) ? sizeof(segbuf) : remain;
		n = _pread(chan, segbuf, sz, pos);
		if (n < 0)  PRN_ERR_RTN("P3 read section");

		// dump_mem("prog-1", segbuf, 128); //%%
		pos += n;

		// Copy program/data to the target task.
		proc2proc_copy(TID2PROCNR(_myself), segbuf, 
			       TID2PROCNR(tasktid), p_vaddr, sz);
		DBGPRN(" p2p_copy tid=%x adrs=%x sz=%d\n", tasktid, p_vaddr, sz);

		p_vaddr += sz;
		remain -= sz;
	    }//while
	}
	//dump_proc_mem("X3", TID2PROCNR(client), argtbl_adrs, argtbl_size); //%  

	//--- Zero clear BSS segments ----
	if (phdr.p_memsz > phdr.p_filesz) { 
	    int zero_base = phdr.p_vaddr + phdr.p_filesz;
	    int zero_size = phdr.p_memsz - phdr.p_filesz;
	    //l4printf("BSS zero clear: %X, size=%d \n", zero_base, zero_size);

	    proc_memset(TID2PROCNR(tasktid), zero_base, 0, zero_size);
	    DBGPRN("proc_memset tid=%x base=%x val=0 sz=%d\n", tasktid, zero_base, zero_size);
	} //if
    }	//for

    // ***** start the program ******
    {
        unsigned  ip, sp;
	ip = (L4_Word_t)load_info.entry;
	sp = argtbl_base;
	DBGBRK("<%s(): tid=%x ip=%x sp=%x pager=%x\n", 
	       __FUNCTION__, tasktid, ip, sp, _mx_pager);

	// L4_Start_SpIp(tasktid, sp, ip);
	start_via_pager(_mx_pager, tasktid, sp, ip);
	L4_ThreadSwitch(tasktid);

	//l4printf_b(" <start_prog(): %s \n", (isSpawn)?"Spawn()":"Exec()");
    }
    return  1;
}


//---- Build argments of the execed proces ---------------------
    /* 
     *
     *                         | LocalVar|   Stack area
     *                         +---------+ 
     * argtbl:                 |  *FP*   |
     *  +--------+ <-argtblp   +---------+ <-- to_argtbl ?
     *  |  size  | = segbuf    |  *PC*   |
     *  +--------+             +---------+
     *  | argc   |             |  argc   |
     *  +--------+             +---------+
     *  | argv   |             |  argv   |
     *  +--------+             +---------+
     *  | argp[0]|             | argp[0] |
     *  +--------+             +---------+
     *  | argp[1]|             | argp[1] |
     *  +--------+             +---------+
     *  |  NULL  |             |         |
     *  +--------+             +---------+
     *  | argstr |             | argstr  |
     *  |        |             |         |
     *  |        |             |         |
     *  +--------+             +---------+
     *
     *                         +---------+ Text area
     *                         |         |
     *
     */

static int  exec_mkarg(L4_ThreadId_t from_tid, unsigned from_argtbl, int argtbl_size,
		 L4_ThreadId_t to_tid, unsigned to_argtbl)
{ 
    int        argc, i, size;
    argtbl_t  *argtblp;

    proc2proc_copy(TID2PROCNR(from_tid), from_argtbl,
		   TID2PROCNR(_myself), segbuf, argtbl_size);

    //dump_proc_mem("X5", TID2PROCNR(from_tid), from_argtbl, argtbl_size); //%
    DBGPRN("p2p_copy(%x %x -> %x %x %d)\n",TID2PROCNR(from_tid), from_argtbl,
	   TID2PROCNR(_myself), segbuf, argtbl_size);
    //dump_argtbl("20", (char*)segbuf); //%

    argtblp = (argtbl_t*) segbuf;
    size = argtblp->size;
    argc = argtblp->argc;

    argtblp->argv = (void*)&((argtbl_t*)to_argtbl)->argp;

    for(i=0; i<argc; i++) {
        DBGPRN("arg[%d] %x \"%s\"\n", i, 
	       ((uint)argtblp->argp[i] + (uint)to_argtbl),
	       ((uint)argtblp->argp[i] + (uint)argtblp));
	argtblp->argp[i] += (uint)to_argtbl;
    }

    proc2proc_copy(TID2PROCNR(_myself), argtblp,
		   TID2PROCNR(to_tid), to_argtbl, argtbl_size);
    DBGPRN("p2pcopy <%d %x> -> <%d %x> size:%d\n", TID2PROCNR(_myself), argtblp,
	   TID2PROCNR(to_tid), to_argtbl, argtbl_size);

  //DBGBRK("arguments\n");
  return  1;
} 

static int shell_mkarg(L4_ThreadId_t from_tid, unsigned from_argtbl, int argtbl_size,
		       char  *arglist2[], // arguments from shell script
		       L4_ThreadId_t to_tid, unsigned to_argtbl)
{ 
    argtbl_t * argtbl1;   // arguments from command
    int        argc1, size1;
    int        argc2 = 0; // arguments from shell script
    int        nbytes = 0,  size2;
    argtbl_t * argtbl3;   // arguments to exec-ed process
    int        totalargc, totalsize;
    int        i, j, k;
    char    ** argp, *ap;
    char     * xp, * yp;

    argp = arglist2;
    while (*argp) {
        ap = *argp++;
	nbytes += strlen(ap) + 1;
	argc2++;
    }
    size2 = 4*(argc2+1) + (nbytes+3) & 0xFFFFFFFC ; // make word boundary

    proc2proc_copy(TID2PROCNR(from_tid), from_argtbl,
		   TID2PROCNR(_myself), segbuf, argtbl_size);

    DBGPRN("p2p_copy(%x %x -> %x %x %d)\n",TID2PROCNR(from_tid), from_argtbl,
	   TID2PROCNR(_myself), segbuf, argtbl_size);

    argtbl1 = (argtbl_t*) segbuf;  // arguments from command
    size1 = argtbl1->size;
    argc1 = argtbl1->argc;

    argtbl1->argv = (void*)&((argtbl_t*)to_argtbl)->argp; // 

    for(i=0; i<argc1; i++) {
        DBGPRN("Xarg[%d] %x \"%s\"\n", i, 
	       ((uint)argtbl1->argp[i] + (uint)to_argtbl),
	       ((uint)argtbl1->argp[i] + (uint)argtbl1));
	argtbl1->argp[i] += (uint)segbuf;
    }

    totalargc =  argc2 + argc1 -1;
    totalsize = (size1 + size2 + 3) & 0xFFFFFFFC; // make word boundary
    argtbl3 = (argtbl_t*) malloc(totalsize);

    argtbl3->size = totalsize;
    argtbl3->argc = totalargc;
    argtbl3->argv = (void*)&((argtbl_t*)to_argtbl)->argp[0]; // =12

    xp = & argtbl3->argp[totalargc+1]; // area for argument string

    for(i = 0; i < argc2; i++){  // arguments from shell script
	yp = arglist2[i];
        argtbl3->argp[i] = xp;
        while ((*xp++ = *yp++) != 0) 
	    ;
    }
    for(j = 1; j < argc1; i++, j++){ // arguments from command
	yp = argtbl1->argp[j];
        argtbl3->argp[i] = xp;
        while ((*xp++ = *yp++) != 0) 
	    ;
    }
    argtbl3->argp[i] = nil;
    // for(i = 0; i < totalargc; i++)
    //    print("  arg2pass[%d]:'%s'\n", i, argtbl3->argp[i]);

    for(i = 0; i < totalargc; i++){
        argtbl3->argp[i] -= (uint)argtbl3; 
	argtbl3->argp[i] += (uint)to_argtbl;
    }

    proc2proc_copy(TID2PROCNR(_myself),  argtbl3,
		   TID2PROCNR(to_tid), to_argtbl, totalsize);
    DBGPRN("p2pcopy <%d %x> -> <%d %x> size:%d\n", TID2PROCNR(_myself), argtbl3,
	   TID2PROCNR(to_tid), to_argtbl, totalsize);
    free(argtbl3);

  //DBGBRK("arguments\n");
  return  1;
} 

