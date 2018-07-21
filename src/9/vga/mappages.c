
// HK 20090930 begin

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../pc/io.h"
#include "../port/error.h"

static L4_ThreadId_t  sigma0;

static int get_pages_from_sigma0(L4_Word_t sigma0_adrs, L4_Word_t map_to, int log2pages)
{
    L4_Fpage_t  request_fp, result_fp, recv_fp;
    L4_MsgTag_t tag;
    L4_Msg_t msg;

    sigma0.raw = 0x000C0001;
    request_fp = L4_FpageLog2(sigma0_adrs, 12+log2pages);
    recv_fp = L4_FpageLog2(map_to, 12+log2pages);

    L4_Accept(L4_MapGrantItems (recv_fp));
    L4_MsgClear(&msg);
    L4_MsgAppendWord(&msg, request_fp.raw); //MR1
    L4_MsgAppendWord(&msg, 0);              //MR2 attribute
    L4_Set_MsgLabel(&msg, (L4_Word_t) -6UL << 4); //MR0-label
    L4_MsgLoad(&msg);

    tag = L4_Call(sigma0);
    if (L4_IpcFailed (tag)) {
//        DBGPRN("< get_page_from_sigma0: err tag=%x\n", tag.raw);
	return 0;
    }
    L4_MsgStore(tag, &msg);

    result_fp.raw = L4_MsgWord(&msg, 1); // L4_StoreMR(2, &result_fp.raw);

    if( L4_IsNilFpage(result_fp) )
        return 0;
    return 1;
}

signed char lp49_unmappages(unsigned int size, unsigned int laddr)
/*  0:success, 1:error, -1:abort */
{
	int done = 0;
	for (; size > done; done += 4096) {
		return 1;
	}
	return 0;
}

signed char lp49_mappages(unsigned int size, unsigned int laddr, unsigned int paddr)
/* ret: 0:success, 1:error, -1:abort */
{
	int done;
	for (done = 0; size > done; done += 4096) {
		if (get_pages_from_sigma0(paddr + done, laddr + done, 1) == 0) {
			if (lp49_unmappages(done, laddr) == 0)
				return 1;
			return -1;
		}
	}
	return 0;
}

// HK 20090930 end

