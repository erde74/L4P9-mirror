	.file	"l4-tidutcb.c"
	.data
	.align 4
	.type	next_th_num.3433, @object
	.size	next_th_num.3433, 4
next_th_num.3433:
	.long	1
	.text
	.type	get_th_num, @function
get_th_num:
	pushl	%ebp
	movl	%esp, %ebp
	movl	next_th_num.3433, %eax
	leal	1(%eax), %edx
	movl	%edx, next_th_num.3433
	leave
	ret
	.size	get_th_num, .-get_th_num
	.local	myself.3448
	.comm	myself.3448,4,4
	.local	kipage.3447
	.comm	kipage.3447,4,4
	.local	utcb_base.3446
	.comm	utcb_base.3446,4,4
	.local	utcb_size.3445
	.comm	utcb_size.3445,4,4
	.local	th_num_indx.3444
	.comm	th_num_indx.3444,4,4
	.local	version.3443
	.comm	version.3443,4,4
	.local	th_num_base.3442
	.comm	th_num_base.3442,4,4
	.section	.rodata.str1.4,"aMS",@progbits,1
	.align 4
.LC0:
	.string	">>new_tid(indx=%d kip=%x utcbbase=%x utcbsz=%d thnumbase=%x)\n"
	.align 4
.LC1:
	.string	"==== <utcb_size=%d utcb_base=%x thnum_base=%x >====\n"
	.align 4
.LC2:
	.string	"<<new_tid(indx=%d tid=%x utcb=%x) \n"
	.text
.globl get_new_tid_and_utcb
	.type	get_new_tid_and_utcb, @function
get_new_tid_and_utcb:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%ebx
	subl	$8, %esp
	pushl	th_num_base.3442
	pushl	utcb_size.3445
	pushl	utcb_base.3446
	pushl	kipage.3447
	pushl	th_num_indx.3444
	pushl	$.LC0
	call	l4printf_r
	addl	$32, %esp
	cmpl	$0, kipage.3447
	jne	.L4
#APP
	/* __L4_X86_Utcb() */			
	mov %gs:0, %eax			

#NO_APP
	movl	-60(%eax), %ebx
	movl	%ebx, myself.3448
#APP
	/* L4_KernelInterface() */ 			
	lock; nop				

#NO_APP
	movl	%eax, kipage.3447
	movw	168(%eax), %dx
	andl	$1023, %edx
	movzwl	%dx, %edx
	movb	169(%eax), %cl
	shrb	$2, %cl
	sall	%cl, %edx
	movl	%edx, utcb_size.3445
#APP
	/* __L4_X86_Utcb() */			
	mov %gs:0, %ecx			

#NO_APP
	movl	%edx, %eax
	negl	%eax
	andl	%eax, %ecx
	movl	%ecx, utcb_base.3446
	movl	%ebx, %eax
	shrl	$14, %eax
	movl	%eax, th_num_base.3442
	andl	$16383, %ebx
	movl	%ebx, version.3443
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	pushl	$.LC1
	call	l4printf_g
	addl	$16, %esp
.L4:
	call	get_th_num
	movl	%eax, %ecx
	movl	%eax, th_num_indx.3444
	cmpl	$31, %eax
	jle	.L6
#APP
	/* L4_KDB_Enter() */		
	int	$3		
	jmp	2f		
	mov	$1f, %eax	
.section .rodata		
1:	.ascii "Num of threads >= 32: to be extended"	
	.byte 0			
.previous			
2:				

#NO_APP
	movl	$0, %eax
	jmp	.L8
.L6:
	addl	th_num_base.3442, %eax
	sall	$14, %eax
	andl	$16383, %ebx
	orl	%eax, %ebx
	movzwl	version.3443, %eax
	andl	$16383, %eax
	andl	$-16384, %ebx
	orl	%eax, %ebx
	movl	%ecx, %edx
	imull	utcb_size.3445, %edx
	addl	utcb_base.3446, %edx
	movl	8(%ebp), %eax
	movl	%ebx, (%eax)
	movl	12(%ebp), %eax
	movl	%edx, (%eax)
	pushl	%edx
	pushl	%ebx
	pushl	%ecx
	pushl	$.LC2
	call	l4printf_r
#APP
	/* L4_KDB_Enter() */		
	int	$3		
	jmp	2f		
	mov	$1f, %eax	
.section .rodata		
1:	.ascii ""	
	.byte 0			
.previous			
2:				

#NO_APP
	movl	$1, %eax
	addl	$16, %esp
.L8:
	leal	-8(%ebp), %esp
	popl	%ebx
	popl	%esi
	leave
	ret
	.size	get_new_tid_and_utcb, .-get_new_tid_and_utcb
	.ident	"GCC: (GNU) 4.0.0 20050519 (Red Hat 4.0.0-8)"
	.section	.note.GNU-stack,"",@progbits
