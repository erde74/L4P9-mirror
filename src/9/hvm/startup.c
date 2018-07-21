/**********************************************************************
 *      mm/startup.c                                                  *
 *                                                                    *
 *   (C) 2005 NII                                                     *
 *   (c) 2000 University of Karlsruhe                                 *
 **********************************************************************/

#include  <l4all.h>
#include  <l4/l4io.h>
#include  "mb_info.h"
#include  "elf.h"


#define   _DBGFLG 0
#include  <l4/_dbgpr.h>

#define NULL (void*)0 

#define  MAX_THD_NUM  32

// KM(02-07-28) Cf. ELF address: 0x08048000 ===========================
// #define  ARGBLOCK_ADRS  0x08046000 //Fixed address for task parameter passing


L4_ThreadId_t   new_task_id;  // defined in exec.c

int  NEWTASKNR = 0;

static  L4_ThreadId_t   myself, sigma0; 
extern  L4_ThreadId_t   mx_pager;

extern  int  _start;
extern  int  _edata;
extern  int  _end;

extern  unsigned int  __bootflag;   // defined in crt0-x86.S
extern  unsigned int  __bootinfop;  // defined in crt0-x86.S

extern int get_new_tid_and_utcb (L4_ThreadId_t *tid, unsigned int *utcb);
extern int get_taskid(int tasknr, int localnr, L4_ThreadId_t *taskid,
                      unsigned *kip, unsigned *utcb);
extern void mm_start_thread(L4_ThreadId_t target, void *ip,
                            void *sp, L4_ThreadId_t pager);
extern int mm_create_start_task(L4_ThreadId_t task_tid, int max_thd_num,
                                L4_Word_t kip, L4_Word_t utcb_base,
                                L4_ThreadId_t  pager,  void  *ip, void *sp);
extern int mm_create_task(L4_ThreadId_t task_tid, int max_thd_num,
                          L4_Word_t kip, L4_Word_t utcb_base, L4_ThreadId_t  pager);
extern int set2task(char val, int tasknr, unsigned  to, int size);
extern int copy2task(char *from, int tasknr, unsigned  to, int size);
extern int create_mxpager();
extern int get_module_info( L4_Word_t index, char** cmdline,
                            L4_Word_t *haddr_start, L4_Word_t *size );
extern void print_modules_list();
extern void load_segment(L4_ThreadId_t target, int toadrs,
                         int fromadrs,  int size);

extern int get_module_total();
extern void dbg_pagemap(int procnr);


#if 0 //----------------------------
static char * strncpy(char *ret, const char *s2, int n)
{
  char *s1 = ret;
  if (n>0) {
    while((*s1++ = *s2++) && --n > 0)  /* EMPTY */ ;
    if ((*--s2 == '\0') && --n > 0) {
      do {
	*s1++ = '\0';
      } while(--n > 0);
    }
  }
  return ret;
}
#endif 

static int strcmp(const char *s1, const char *s2)
{
  while (*s1 == *s2++) {
    if (*s1++ == '\0') {
      return 0;
    }
  }
  if (*s1 == '\0') return -1;
  if (*--s2 == '\0') return 1;
  return (unsigned char) *s1 - (unsigned char) *s2;
}

//----------------------------------------------------
int elf_section_describe(L4_Word_t file_start, 	char *search_name,
				L4_Word_t *addr, L4_Word_t *size )
{
  Elf32_Ehdr *ehdr = (Elf32_Ehdr *)file_start;
  Elf32_Shdr *shdr, *shdr_list, *str_hdr;
  L4_Word_t s_cnt;
  char *section_name;
 
  shdr_list = (Elf32_Shdr *)(file_start + ehdr->e_shoff);
  str_hdr = &shdr_list[ ehdr->e_shstrndx ];
  for( s_cnt = 0; s_cnt < ehdr->e_shnum; s_cnt++ )
    {
      shdr = &shdr_list[ s_cnt ];
      section_name = (char *)(shdr->sh_name + file_start + str_hdr->sh_offset);
      if( !strcmp(section_name, search_name) )
        {
	  *addr = shdr->sh_addr;
	  *size = shdr->sh_size;
	  return 1;
        }
    }
  return 0;
}

//------------------------------------------------------------------------
void touch_pages()
{
  int   foo;
  int   i, j;
  char *cmdline;
  L4_Word_t  module_start, module_size;
 
  // Touch all pages used by this task to protect them
  //  from being handed out by sigma0 before we access them.
  for (i = (int)&_start; i <= (int)&_end;  i += 4096) 
    foo = *((volatile int*)i);

  // for (i = 0xB8000; i < 0x100000; i += 4096)     // Video memory area 
  //    foo = *((volatile int*)i);
  /*  Touch all pages which contain load modules. */
  for (i=0; i < get_module_total(); i++) 
    {
      int  start_page;
      int  end_page;

      get_module_info(i, &cmdline, &module_start, &module_size);
      start_page = module_start/4096;
      end_page   = (module_start + module_size)/4096;
 
      l4printf("Module[%d] at [%x - %x]  %s \n", 
	     i, module_start, (module_start + module_size), cmdline);
        for (j = start_page; j <= end_page; j++)
          foo = *(volatile int*)(j * 4096);
    }
}

#if 0 //-------------------------------------------------------------------
static char*  strchr(char *s, int c)
{
  char c0 = c;
  char c1;

  if(c == 0) {
    while(*s++)  ;
    return s-1;
  }

  while((c1 = *s++))
    if(c1 == c0)   return s-1;
  return 0;
}

static char*  strstr(char *s1, char *s2)
{
  char *p, *pa, *pb;
  int c0, c;

  c0 = *s2;
  if(c0 == 0)   return s1;
  s2++;
  for(p=strchr(s1, c0); p; p=strchr(p+1, c0)) {
    pa = p;
    for(pb=s2;; pb++) {
      c = *pb;
      if(c == 0)     return p;
      if(c != *++pa) break;
    }
  }
  return 0;
}
#endif

//---------------------------------------------------
static void print_code(unsigned  start, unsigned size, unsigned vadrs)
{
  unsigned char *p;
  unsigned char c0, c1, c2, c3;
  l4printf_b("-- Grub loaded module: sampled contents --\n");
  for ( p = (unsigned char*)start; p < (unsigned char*)(start+size); 
	p += 4096, vadrs += 4096)
    {
      c0 = *p;  c1 = *(p+1); c2 = *(p+2); c3 = *(p+3);
      l4printf_b("%x:%.2x %.2x %.2x %.2x  ", vadrs, c0, c1, c2, c3);
    }
  l4printf("\n");
}



//------------------------------------------------------------------
int  startup( )
{
  Elf32_Phdr         *phdr;
  int                i, j; 

  // Initialize global data
  myself = L4_Myself();  
  sigma0 = L4_Pager();
  DBGBRK("\n<> HVM =%X  SIGMA0=  %X <>\n", myself.raw, sigma0.raw);
  //  check_multiboot_header(mb_magic, &mbi);
  //  l4printf("Multiboot header found at %X\n", (dword_t) mbi);


  //=============== bootup sequence ========================
  for (j = 0; j < get_module_total(); j++)
    {
      Elf32_Ehdr *elf_hdr;
      char *cmdline;
      L4_Word_t  module_start, module_size;

      get_module_info(j, &cmdline, &module_start, &module_size);

      elf_hdr = (Elf32_Ehdr *)module_start;

      if (elf_hdr -> e_ident[EI_MAG0] !=  ELFMAG0  || 
	  elf_hdr -> e_ident[EI_MAG1] !=  ELFMAG1  || 
	  elf_hdr -> e_ident[EI_MAG2] !=  ELFMAG2  || 
	  elf_hdr -> e_ident[EI_MAG3] !=  ELFMAG3) 
        {    
	  // not ELF file ------------------
	  l4printf_r("Booter: Non ELF file: %s\n", cmdline); //KM
          continue;
        } 

      if (elf_hdr -> e_type != ET_EXEC)    L4_KDB_Enter("unexpected e_type");
      if (elf_hdr -> e_machine != EM_386)  L4_KDB_Enter("not an intel binary");
      if (elf_hdr -> e_version != EV_CURRENT)  L4_KDB_Enter("version mismatch?");
      if (elf_hdr -> e_flags != 0)   L4_KDB_Enter("unexpected flags?");
      if (elf_hdr -> e_phnum <= 0)   L4_KDB_Enter("No loadable program sections");

      //--- create a new task.----- 
      { 
	unsigned  kip;
	unsigned  utcb;

	NEWTASKNR++;
	get_taskid (NEWTASKNR, 0, &new_task_id, &kip, &utcb);
	mm_create_task(new_task_id, MAX_THD_NUM, kip, utcb, mx_pager); 
	DBGBRK("<> NEW PROCESS= %X \n", new_task_id);
      }

      //      set_mproc_tbl(new_task_id,  cmdline);
      //      strncpy(commandline.command, cmdline, 110);
      l4printf_g("Loading task '%s':%X  pager:%X hvm:%X sigma0:%X\n",
		 cmdline, new_task_id.raw, mx_pager.raw, myself.raw, sigma0.raw);

      //------- parse program headers -------------------------------
      phdr = (Elf32_Phdr *)(elf_hdr->e_phoff + (unsigned int)elf_hdr);
      for (i = 0; i < elf_hdr->e_phnum; i++) 
	{
	  if (phdr[i].p_type == PT_LOAD) 
	    {
		unsigned  from, to, size;
		from = (int)elf_hdr + phdr[i].p_offset;
		to =  (int)phdr[i].p_vaddr;
		size = (int)phdr[i].p_filesz;

#if _DBGFLG
#define  ZZ  phdr[i]
	      l4printf("PHDR typ=%X ofst=0x%X v-adr=0x%X"
		      " f/m-sz=0x%X/0x%X flag=%X a=0x%X\n",
		     ZZ.p_type, ZZ.p_offset, ZZ.p_vaddr, 
		     ZZ.p_filesz, ZZ.p_memsz, ZZ.p_flags, ZZ.p_align);  
	      // L4_KDB_Enter("startup-3");
#endif 
	      
		copy2task((void*)from, NEWTASKNR, to, size);
		DBGPRN(" Copying segment size: 0x%X from: 0x%X to: 0x%X \n",
		       size, from , to );

	      //---- Clear the BSS segment --------------------------------
	      if (phdr[i].p_memsz > phdr[i].p_filesz)
		{ 
		  int zero_base = phdr[i].p_vaddr + phdr[i].p_filesz;
		  int zero_size = phdr[i].p_memsz - phdr[i].p_filesz;
		  DBGPRN(" Erasing zone at %X, size %d \n", zero_base, zero_size);
		  set2task(0, NEWTASKNR, zero_base, zero_size);
		}
	    } // if (phdr[i].p_type == PT_LOAD) 
	} //for (i = 0; i < phnum; i++) 

      DBGPRN(" Jumping to program entry point at %X \n", elf_hdr->e_entry);
      dbg_pagemap(NEWTASKNR);  //%%%


      mm_start_thread(new_task_id, (void*)elf_hdr->e_entry, 
		     (void*) 0 /* (void*)0x200000 */, mx_pager);

      { //090330
	  L4_Time_t       timeout;
	  L4_MsgTag_t     tag;
	  int             errcode;

	  timeout = L4_Never;
	  // timeout = L4_TimePeriod((long long)(100 * 1000000));

	  tag = L4_Receive_Timeout(new_task_id, timeout);
	  if (L4_IpcFailed(tag)){
	      errcode = L4_ErrorCode();
	      l4printf_g(" Err: hvm:wait_ack():%d\n", errcode);
	  }
	  // l4printf_g("ack received...\n");  L4_KDB_Enter("");
      }


      //--- Argument passing ---------------
      //  load_segment(new_task_id, ARGBLOCK_ADRS, (int)&commandline, 128); 
 
      L4_ThreadSwitch(new_task_id);
    } // (j = 0; j < get_module_total(); j++)

  return 1;
}
