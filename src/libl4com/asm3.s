	.file	"l4-tidutcb.c"
	.local	th_num_base.0
	.comm	th_num_base.0,4,4
	.local	version.1
	.comm	version.1,4,4
	.data
	.align 4
	.type	th_num_indx.2,@object
	.size	th_num_indx.2,4
th_num_indx.2:
	.long	0
	.local	utcb_size.3
	.comm	utcb_size.3,4,4
	.local	utcb_base.4
	.comm	utcb_base.4,4,4
	.align 4
	.type	kipage.5,@object
	.size	kipage.5,4
kipage.5:
	.long	0
	.local	myself.6
	.comm	myself.6,4,4
	.section	.rodata.str1.32,"aMS",@progbits,1
	.align 32
.LC1:
	.string	"==== <utcbsize=%d utcbbase=%x thnumbase=%x >====\n"
	.text
.globl get_new_tid_and_utcb
	.type	get_new_tid_and_utcb,@function
get_new_tid_and_utcb:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$12, %esp
	cmpl	$0, kipage.5
	jne	.L103
#APP
	/* __L4_X86_Utcb() */			
	mov %gs:0, %eax			

#NO_APP
	movl	-60(%eax), %ebx
	movl	%ebx, myself.6
#APP
	/* L4_KernelInterface() */ 			
	lock; nop				

#NO_APP
	movl	%eax, kipage.5
	movzwl	168(%eax), %edx
	andl	$1023, %edx
	movb	169(%eax), %cl
	shrb	$2, %cl
	sall	%cl, %edx
	movl	%edx, utcb_size.3
#APP
	/* __L4_X86_Utcb() */			
	mov %gs:0, %ecx			

#NO_APP
	movl	%edx, %eax
	negl	%eax
	andl	%eax, %ecx
	movl	%ecx, utcb_base.4
	movl	%ebx, %eax
	shrl	$14, %eax
	movl	%eax, th_num_base.0
	andl	$16383, %ebx
	movl	%ebx, version.1
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	pushl	$.LC1
	call	l4printf_g
	addl	$16, %esp
.L103:
	subl	$12, %esp
	pushl	$th_num_indx.2
	call	_increment
	addl	$16, %esp
	cmpl	$31, th_num_indx.2
	jbe	.L116
#APP
	/* L4_KDB_Enter() */		
	int	$3		
	jmp	2f		
	mov	$1f, %eax	
.section .rodata		
1:	.ascii "Num of threads >= 16: to be extended"	
	.byte 0			
.previous			
2:				

#NO_APP
	movl	$0, %eax
	jmp	.L101
.L116:
	movl	th_num_base.0, %eax
	addl	th_num_indx.2, %eax
	sall	$14, %eax
	andl	$16383, %edi
	orl	%eax, %edi
	movl	version.1, %eax
	andl	$16383, %eax
	andl	$-16384, %edi
	orl	%eax, %edi
	movl	utcb_size.3, %edx
	imull	th_num_indx.2, %edx
	addl	utcb_base.4, %edx
	movl	8(%ebp), %eax
	movl	%edi, (%eax)
	movl	12(%ebp), %eax
	movl	%edx, (%eax)
	movl	$1, %eax
.L101:
	leal	-12(%ebp), %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
.Lfe1:
	.size	get_new_tid_and_utcb,.Lfe1-get_new_tid_and_utcb
	.section	.note.GNU-stack,"",@progbits
	.ident	"GCC: (GNU) 3.2.3 20030502 (Red Hat Linux 3.2.3-20)"
