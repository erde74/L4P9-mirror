#include "mem.h"
#include "/sys/src/boot/pc/x16.h"
#undef DELAY

#define PADDR(a)	((a) & ~KZERO)
#define KADDR(a)	(KZERO|(a))

/*
 * Some machine instructions not handled by 8[al].
 */
#define OP16		BYTE $0x66
#define DELAY		BYTE $0xEB; BYTE $0x00	/* JMP .+2 */
#define CPUID		BYTE $0x0F; BYTE $0xA2	/* CPUID, argument in AX */
#define WRMSR		BYTE $0x0F; BYTE $0x30	/* WRMSR, argument in AX/DX (lo/hi) */
#define RDTSC 		BYTE $0x0F; BYTE $0x31	/* RDTSC, result in AX/DX (lo/hi) */
#define RDMSR		BYTE $0x0F; BYTE $0x32	/* RDMSR, result in AX/DX (lo/hi) */
#define HLT		BYTE $0xF4
#define INVLPG	BYTE $0x0F; BYTE $0x01; BYTE $0x39	/* INVLPG (%ecx) */

/*
 * Macros for calculating offsets within the page directory base
 * and page tables. Note that these are assembler-specific hence
 * the '<<2'.
 */
#define PDO(a)		(((((a))>>22) & 0x03FF)<<2)
#define PTO(a)		(((((a))>>12) & 0x03FF)<<2)


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
 * Save registers.
 */
TEXT saveregs(SB), $0
	/* appease 8l */
	SUBL $32, SP
	POPL AX
	POPL AX
	POPL AX
	POPL AX
	POPL AX
	POPL AX
	POPL AX
	POPL AX
	
	PUSHL	AX
	PUSHL	BX
	PUSHL	CX
	PUSHL	DX
	PUSHL	BP
	PUSHL	DI
	PUSHL	SI
	PUSHFL

	XCHGL	32(SP), AX	/* swap return PC and saved flags */
	XCHGL	0(SP), AX
	XCHGL	32(SP), AX
	RET

TEXT restoreregs(SB), $0
	/* appease 8l */
	PUSHL	AX
	PUSHL	AX
	PUSHL	AX
	PUSHL	AX
	PUSHL	AX
	PUSHL	AX
	PUSHL	AX
	PUSHL	AX
	ADDL	$32, SP
	
	XCHGL	32(SP), AX	/* swap return PC and saved flags */
	XCHGL	0(SP), AX
	XCHGL	32(SP), AX

	POPFL
	POPL	SI
	POPL	DI
	POPL	BP
	POPL	DX
	POPL	CX
	POPL	BX
	POPL	AX
	RET

/*
 * Assumed to be in protected mode at time of call.
 * Switch to real mode, execute an interrupt, and
 * then switch back to protected mode.  
 *
 * Assumes:
 *
 *	- no device interrupts are going to come in
 *	- 0-16MB is identity mapped in page tables
 *	- realmode() has copied us down from 0x100000 to 0x8000
 *	- can use code segment 0x0800 in real mode
 *		to get at l.s code
 *	- l.s code is less than 1 page
 */
#define RELOC	(RMCODE-KTZERO)

TEXT realmodeidtptr(SB), $0
	WORD	$(4*256-1)
	LONG	$0

TEXT realmode0(SB), $0
	CALL	saveregs(SB)

	/* switch to low code address */
	LEAL	physcode-KZERO(SB), AX
	JMP *AX

TEXT physcode(SB), $0

	/* switch to low stack */
	MOVL	SP, AX
	MOVL	$0x7C00, SP
	PUSHL	AX

	/* change gdt to physical pointer */
	MOVL	m0rgdtptr-KZERO(SB), GDTR

	/* load IDT with real-mode version*/
	MOVL	realmodeidtptr-KZERO(SB), IDTR

	/* edit INT $0x00 instruction below */
	MOVL	$(RMUADDR-KZERO+48), AX	/* &rmu.trap */
	MOVL	(AX), AX
	MOVB	AX, realmodeintrinst+(-KZERO+1+RELOC)(SB)

	/* disable paging */
	MOVL	CR0, AX
	ANDL	$0x7FFFFFFF, AX
	MOVL	AX, CR0
	/* JMP .+2 to clear prefetch queue*/
	BYTE $0xEB; BYTE $0x00

	/* jump to 16-bit code segment */
/*	JMPFAR	SELECTOR(KESEG16, SELGDT, 0):$again16bit(SB) /**/
	 BYTE	$0xEA
	 LONG	$again16bit-KZERO(SB)
	 WORD	$SELECTOR(KESEG16, SELGDT, 0)

TEXT again16bit(SB), $0
	/*
	 * Now in 16-bit compatibility mode.
	 * These are 32-bit instructions being interpreted
	 * as 16-bit instructions.  I'm being lazy and
	 * not using the macros because I know when
	 * the 16- and 32-bit instructions look the same
	 * or close enough.
	 */

	/* disable protected mode and jump to real mode cs */
	OPSIZE; MOVL CR0, AX
	OPSIZE; XORL BX, BX
	OPSIZE; INCL BX
	OPSIZE; XORL BX, AX
	OPSIZE; MOVL AX, CR0

	/* JMPFAR 0x0800:now16real */
	 BYTE $0xEA
	 WORD	$now16real-KZERO(SB)
	 WORD	$0x0800

TEXT now16real(SB), $0
	/* copy the registers for the bios call */
	LWI(0x0000, rAX)
	MOVW	AX,SS
	LWI(RMUADDR, rBP)
	
	/* offsets are in Ureg */
	LXW(44, xBP, rAX)
	MOVW	AX, DS
	LXW(40, xBP, rAX)
	MOVW	AX, ES

	OPSIZE; LXW(0, xBP, rDI)
	OPSIZE; LXW(4, xBP, rSI)
	OPSIZE; LXW(16, xBP, rBX)
	OPSIZE; LXW(20, xBP, rDX)
	OPSIZE; LXW(24, xBP, rCX)
	OPSIZE; LXW(28, xBP, rAX)

	CLC

TEXT realmodeintrinst(SB), $0
	INT $0x00

	/* save the registers after the call */

	LWI(0x7bfc, rSP)
	OPSIZE; PUSHFL
	OPSIZE; PUSHL AX

	LWI(0, rAX)
	MOVW	AX,SS
	LWI(RMUADDR, rBP)
	
	OPSIZE; SXW(rDI, 0, xBP)
	OPSIZE; SXW(rSI, 4, xBP)
	OPSIZE; SXW(rBX, 16, xBP)
	OPSIZE; SXW(rDX, 20, xBP)
	OPSIZE; SXW(rCX, 24, xBP)
	OPSIZE; POPL AX
	OPSIZE; SXW(rAX, 28, xBP)

	MOVW	DS, AX
	OPSIZE; SXW(rAX, 44, xBP)
	MOVW	ES, AX
	OPSIZE; SXW(rAX, 40, xBP)

	OPSIZE; POPL AX
	OPSIZE; SXW(rAX, 64, xBP)	/* flags */

	/* re-enter protected mode and jump to 32-bit code */
	OPSIZE; MOVL $1, AX
	OPSIZE; MOVL AX, CR0
	
/*	JMPFAR	SELECTOR(KESEG, SELGDT, 0):$again32bit(SB) /**/
	 OPSIZE
	 BYTE $0xEA
	 LONG	$again32bit-KZERO(SB)
	 WORD	$SELECTOR(KESEG, SELGDT, 0)

TEXT again32bit(SB), $0
	MOVW	$SELECTOR(KDSEG, SELGDT, 0),AX
	MOVW	AX,DS
	MOVW	AX,SS
	MOVW	AX,ES
	MOVW	AX,FS
	MOVW	AX,GS

	/* enable paging and jump to kzero-address code */
	MOVL	CR0, AX
	ORL	$0x80000000, AX
	MOVL	AX, CR0
	LEAL	again32kzero(SB), AX
	JMP*	AX

TEXT again32kzero(SB), $0
	/* breathe a sigh of relief - back in 32-bit protected mode */

	/* switch to old stack */	
	PUSHL	AX	/* match popl below for 8l */
	MOVL	$0x7BFC, SP
	POPL	SP

	/* restore idt */
	MOVL	m0idtptr(SB),IDTR

	/* restore gdt */
	MOVL	m0gdtptr(SB), GDTR

	CALL	restoreregs(SB)
	RET


/*
 * Read/write various system registers.
 * CR4 and the 'model specific registers' should only be read/written
 * after it has been determined the processor supports them
 */
TEXT lgdt(SB), $0				/* GDTR - global descriptor table */
	MOVL	gdtptr+0(FP), AX
	MOVL	(AX), GDTR
	RET

TEXT lidt(SB), $0				/* IDTR - interrupt descriptor table */
	MOVL	idtptr+0(FP), AX
	MOVL	(AX), IDTR
	RET

TEXT ltr(SB), $0				/* TR - task register */
	MOVL	tptr+0(FP), AX
	MOVW	AX, TASK
	RET

TEXT getcr0(SB), $0				/* CR0 - processor control */
	MOVL	CR0, AX
	RET

TEXT getcr2(SB), $0				/* CR2 - page fault linear address */
	MOVL	CR2, AX
	RET

TEXT getcr3(SB), $0				/* CR3 - page directory base */
	MOVL	CR3, AX
	RET

TEXT putcr3(SB), $0
	MOVL	cr3+0(FP), AX
	MOVL	AX, CR3
	RET

TEXT getcr4(SB), $0				/* CR4 - extensions */
	MOVL	CR4, AX
	RET

TEXT putcr4(SB), $0
	MOVL	cr4+0(FP), AX
	MOVL	AX, CR4
	RET

TEXT invlpg(SB), $0
	/* 486+ only */
	MOVL	va+0(FP), CX
	INVLPG
	RET

TEXT _cycles(SB), $0				/* time stamp counter */
	RDTSC
	MOVL	vlong+0(FP), CX			/* &vlong */
	MOVL	AX, 0(CX)			/* lo */
	MOVL	DX, 4(CX)			/* hi */
	RET

TEXT rdmsr(SB), $0				/* model-specific register */
	MOVL	index+0(FP), CX
	RDMSR
	MOVL	vlong+4(FP), CX			/* &vlong */
	MOVL	AX, 0(CX)			/* lo */
	MOVL	DX, 4(CX)			/* hi */
	RET
	
TEXT wrmsr(SB), $0
	MOVL	index+0(FP), CX
	MOVL	lo+4(FP), AX
	MOVL	hi+8(FP), DX
	WRMSR
	RET


/*
 */
TEXT splhi(SB), $0
shi:
	PUSHFL
	POPL	AX
	TESTL	$0x200, AX
	JZ	alreadyhi
	MOVL	$(MACHADDR+0x04), CX 		/* save PC in m->splpc */
	MOVL	(SP), BX
	MOVL	BX, (CX)
alreadyhi:
	CLI
	RET

TEXT spllo(SB), $0
slo:
	PUSHFL
	POPL	AX
	TESTL	$0x200, AX
	JNZ	alreadylo
	MOVL	$(MACHADDR+0x04), CX		/* clear m->splpc */
	MOVL	$0, (CX)
alreadylo:
	STI
	RET

TEXT splx(SB), $0
	MOVL	s+0(FP), AX
	TESTL	$0x200, AX
	JNZ	slo
	JMP	shi

TEXT spldone(SB), $0
	RET

TEXT islo(SB), $0
	PUSHFL
	POPL	AX
	ANDL	$0x200, AX			/* interrupt enable flag */
	RET

/*
 * Test-And-Set
 */
TEXT tas(SB), $0
	MOVL	$0xDEADDEAD, AX
	MOVL	lock+0(FP), BX
	XCHGL	AX, (BX)			/* lock->key */
	RET

TEXT _xinc(SB), $0				/* void _xinc(long*); */
	MOVL	l+0(FP), AX
	LOCK;	INCL 0(AX)
	RET

TEXT _xdec(SB), $0				/* long _xdec(long*); */
	MOVL	l+0(FP), BX
	XORL	AX, AX
	LOCK;	DECL 0(BX)
	JLT	_xdeclt
	JGT	_xdecgt
	RET
_xdecgt:
	INCL	AX
	RET
_xdeclt:
	DECL	AX
	RET

TEXT mb386(SB), $0
	POPL	AX				/* return PC */
	PUSHFL
	PUSHL	CS
	PUSHL	AX
	IRETL

TEXT mb586(SB), $0
	XORL	AX, AX
	CPUID
	RET

TEXT xchgw(SB), $0
	MOVL	v+4(FP), AX
	MOVL	p+0(FP), BX
	XCHGW	AX, (BX)
	RET

TEXT cmpswap486(SB), $0
	MOVL	addr+0(FP), BX
	MOVL	old+4(FP), AX
	MOVL	new+8(FP), CX
	LOCK
	BYTE $0x0F; BYTE $0xB1; BYTE $0x0B	/* CMPXCHGL CX, (BX) */
	JNZ didnt
	MOVL	$1, AX
	RET
didnt:
	XORL	AX,AX
	RET

