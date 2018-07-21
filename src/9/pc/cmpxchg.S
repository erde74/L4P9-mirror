#      cmpxchg 
#     (C) HKawai
#
#    int cmpxchg1(void *p, int t, int s);
#    int cmpxchg2(void *p, int t, int s);
#    int cmpxchg4(void *p, int t, int s);
#    int cmpxchg8(void *p, int t0, int t1, int s0, int s1);
#
#        t: data for test, s: data for store
#        return 0: not-equal, not-stored
#               1: equal, stored
# 
	
	.file       "cmpxchg.S"
        .text
        .global     cmpxchg1
        .type       cmpxchg1,@function
	
cmpxchg1:
        movl        4(%esp),%ecx
        movl        8(%esp),%eax
        movl        12(%esp),%edx
        lock cmpxchgb  %al,(%ecx)
        setzb       %al
        andl        $1,%eax
        ret

        .global     cmpxchg2
        .type       cmpxchg2,@function

cmpxchg2:
        movl        4(%esp),%ecx
        movl        8(%esp),%eax
        movl        12(%esp),%edx
        lock cmpxchgw  %ax,(%ecx)
        setzb       %al
        andl        $1,%eax
        ret

        .global     cmpxchg4
        .type       cmpxchg4,@function

cmpxchg4:
        movl        4(%esp),%ecx
        movl        8(%esp),%eax
        movl        12(%esp),%edx
        lock cmpxchgl  %eax,(%ecx)
        setzb       %al
        andl        $1,%eax
        ret

        .global     cmpxchg8
        .type       cmpxchg8,@function

cmpxchg8:
        pushl       esi
        pushl       ebx
        movl        12(%esp),%esi
        movl        16(%esp),%eax
        movl        20(%esp),%edx
        movl        24(%esp),%ebx
        movl        28(%esp),%ecx
        lock cmpxchg8b  (%esi)
        setzb       %al
        andl        $1,%eax
        popl        ebx
        popl        esi
        ret
