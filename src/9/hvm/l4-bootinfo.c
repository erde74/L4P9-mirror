/**************************************************************
 *   l4bootinfo.c                                             *
 *   Self-contained package                                   *
 *   (C) 2005 Karlsruhe Univ., NII                            *
 **************************************************************/

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <l4/bootinfo.h>

//-------------------------------------------
static void locate_kickstart_data();
static L4_BootRec_t * get_bootrec( L4_Word_t index );


#define  NULL  ((void*)0)
static  void *bootinfo = NULL;
static  int  total = 0;

typedef enum {
  bootloader_memdesc_undefined=   (0<<4) + L4_BootLoaderSpecificMemoryType,
  bootloader_memdesc_init_table=  (1<<4) + L4_BootLoaderSpecificMemoryType,
  bootloader_memdesc_init_server= (2<<4) + L4_BootLoaderSpecificMemoryType,
  bootloader_memdesc_boot_module= (3<<4) + L4_BootLoaderSpecificMemoryType,
} bootloader_memdesc_e;
 
#define  PAGE_BITS   12
#define  PAGE_SIZE   4096
#define  PAGE_MASK   (~(PAGE_SIZE -1))

//-----------------------------------------------------
static L4_Fpage_t L4_Sigma0_GetPage_RcvWindow 
  (L4_ThreadId_t s0,  L4_Fpage_t f,  L4_Fpage_t RcvWindow)
{
  L4_MsgTag_t tag;
  L4_Msg_t msg;
 
  if (L4_IsNilThread (s0))
    {
      L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *)
	L4_GetKernelInterface ();
      s0 = L4_GlobalId (kip->ThreadInfo.X.UserBase, 1);
    }

  L4_MsgClear (&msg);
  L4_MsgAppendWord (&msg, f.raw);
  L4_MsgAppendWord (&msg, 0);
  L4_Set_MsgLabel (&msg, (L4_Word_t) -6UL << 4);
  L4_MsgLoad (&msg);
  L4_Accept (L4_MapGrantItems (RcvWindow));
         
  tag = L4_Call (s0);
  if (L4_IpcFailed (tag))
    return L4_Nilpage;
 
  L4_StoreMR (2, &f.raw);
  return f;
}

//------------------------------------------------
static int sigma0_request_region( L4_Word_t start, L4_Word_t end )
{
  L4_Fpage_t request_fp, result_fp;
  L4_Word_t page;

  for( page = start & PAGE_MASK; page < end; page += PAGE_SIZE )
    {
      request_fp = L4_FpageLog2(page, PAGE_BITS);
      result_fp = L4_Sigma0_GetPage_RcvWindow( L4_Pager(), request_fp, request_fp );
      if( L4_IsNilFpage(result_fp) )
	return 0;
    }
  return 1;
}

//-----------------------------------------------------
int get_module_total()
{
    return total;
}

//-----------------------------------------------------
int get_module_info( L4_Word_t index,
	char** cmdline, L4_Word_t *haddr_start, L4_Word_t *size )
{
    if( !bootinfo || (index >= get_module_total()) )	return 0;

    L4_BootRec_t *rec = get_bootrec(index);

    *cmdline = L4_Module_Cmdline( rec );
    *haddr_start = L4_Module_Start( rec );
    *size = L4_Module_Size( rec );
    return 1;
}

//-----------------------------------------------------
int bootinfo_init()
{
    if( bootinfo )
	return 1;

    locate_kickstart_data();
    if( bootinfo )
    {
      int  i;
	total = 0;
	L4_BootRec_t *rec = L4_BootInfo_FirstEntry( bootinfo );
	for( i = 0; i < L4_BootInfo_Entries(bootinfo); i++ )
	{
	    if( L4_BootRec_Type(rec) == L4_BootInfo_Module )
		total++;
	    rec = L4_BootRec_Next(rec);
	}
	return 1;
    }
    return 0;
}

//-----------------------------------------------------
static L4_BootRec_t * get_bootrec( L4_Word_t index )
    // Index must be valid!!
{
    L4_Word_t which = 0;
    L4_BootRec_t *rec = L4_BootInfo_FirstEntry( bootinfo );

    while( 1 )
    {
	if( L4_BootRec_Type(rec) == L4_BootInfo_Module )
	{
	    if( which == index ) return rec;
	    which++;
	}
	rec = L4_BootRec_Next(rec);
    }
}

//-----------------------------------------------------
static void locate_kickstart_data()
{
    int  idx;
    L4_KernelInterfacePage_t *kip =
	(L4_KernelInterfacePage_t *)L4_GetKernelInterface();

    // Search for bootloader data in the memory descriptors.
    L4_Word_t num_mdesc = L4_NumMemoryDescriptors( kip );

    for( idx = 0; idx < num_mdesc; idx++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc( kip, idx );
	L4_Word_t start = L4_MemoryDescLow(mdesc);
	L4_Word_t end   = L4_MemoryDescHigh(mdesc);

	if( ! bootinfo && 
		(L4_MemoryDescType(mdesc) == bootloader_memdesc_init_table) )
	{
	    if( !sigma0_request_region(start, end) )
	    {
		l4printf("Unable to request the bootinfo region from sigma0.\n");
		return;
	    }
	    bootinfo = (void *)L4_MemoryDescLow(mdesc);
	    if( L4_BootInfo_Valid(bootinfo) )
		l4printf("Mapped bootinfo region %X-%X\n", start, end );
	    else
		bootinfo = NULL;
	}

	else if( L4_MemoryDescType(mdesc) == bootloader_memdesc_boot_module )
	{
	    if( !sigma0_request_region(start, end) )
	    {
		l4printf("Unable to request a bootloader module from sigma0.\n");
		return;
	    }
	    l4printf("Mapped bootloader module %X-%X\n", start, end );
	}
    }
}

void print_modules_list()
{
  L4_Word_t mod_index;

  l4printf("Boot modules:\n");
  for( mod_index = 0; mod_index < get_module_total(); mod_index++ )
    {
      L4_Word_t haddr_start, size;
      char *cmdline;

      get_module_info( mod_index, &cmdline, &haddr_start, &size );
      l4printf("Module[%d] at %X, size %X,  cmd-line: %s \n" ,
	     mod_index, haddr_start, size, cmdline)  ;
    }
}

#if 0 //TEST ==============================================
int main( )
{
  if (bootinfo_init() == 0)  L4_KDB_Enter("bootinfo::init()");
  print_modules_list();
}
#endif //=================================================
