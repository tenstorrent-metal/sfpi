	.file	"regmov.cc"
	.option nopic
	.attribute arch, "rv32i2p0_y2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	1
	.globl	_Z11remove_add1v
	.type	_Z11remove_add1v, @function
_Z11remove_add1v:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L0, 4, 2
	SFPIADD	0, L1, L0, 4
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z11remove_add1v, .-_Z11remove_add1v
	.align	1
	.globl	_Z15remove_add2_nopv
	.type	_Z15remove_add2_nopv, @function
_Z15remove_add2_nopv:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L0, 4, 2
	SFPIADD	0, L1, L0, 4
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z15remove_add2_nopv, .-_Z15remove_add2_nopv
	.align	1
	.globl	_Z12replace_add3v
	.type	_Z12replace_add3v, @function
_Z12replace_add3v:
	SFPLOADI	L2, 4, 1
	SFPLOADI	L1, 4, 2
	SFPMOV	L1, L0, 0
	SFPNOP
	SFPNOP
	SFPIADD	0, L2, L0, 4
	SFPNOP
	SFPNOP
	SFPSTORE	L2, 2, 0
	SFPSTORE	L1, 2, 4
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z12replace_add3v, .-_Z12replace_add3v
	.align	1
	.globl	_Z12replace_sub1v
	.type	_Z12replace_sub1v, @function
_Z12replace_sub1v:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L2, 4, 2
	SFPMOV	L1, L0, 0
	SFPNOP
	SFPNOP
	SFPIADD	0, L2, L0, 6
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z12replace_sub1v, .-_Z12replace_sub1v
	.align	1
	.globl	_Z11remove_and1v
	.type	_Z11remove_and1v, @function
_Z11remove_and1v:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L0, 4, 2
	SFPAND	L1, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z11remove_and1v, .-_Z11remove_and1v
	.align	1
	.globl	_Z15remove_and2_nopv
	.type	_Z15remove_and2_nopv, @function
_Z15remove_and2_nopv:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L0, 4, 2
	SFPAND	L1, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z15remove_and2_nopv, .-_Z15remove_and2_nopv
	.align	1
	.globl	_Z12replace_and3v
	.type	_Z12replace_and3v, @function
_Z12replace_and3v:
	SFPLOADI	L2, 4, 1
	SFPLOADI	L1, 4, 2
	SFPMOV	L2, L0, 0
	SFPNOP
	SFPNOP
	SFPAND	L1, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L2, 2, 0
	SFPSTORE	L1, 2, 4
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z12replace_and3v, .-_Z12replace_and3v
	.align	1
	.globl	_Z10remove_or1v
	.type	_Z10remove_or1v, @function
_Z10remove_or1v:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L0, 4, 2
	SFPOR	L1, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z10remove_or1v, .-_Z10remove_or1v
	.align	1
	.globl	_Z14remove_or2_nopv
	.type	_Z14remove_or2_nopv, @function
_Z14remove_or2_nopv:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L0, 4, 2
	SFPOR	L1, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z14remove_or2_nopv, .-_Z14remove_or2_nopv
	.align	1
	.globl	_Z11replace_or3v
	.type	_Z11replace_or3v, @function
_Z11replace_or3v:
	SFPLOADI	L2, 4, 1
	SFPLOADI	L1, 4, 2
	SFPMOV	L2, L0, 0
	SFPNOP
	SFPNOP
	SFPOR	L1, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L2, 2, 0
	SFPSTORE	L1, 2, 4
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z11replace_or3v, .-_Z11replace_or3v
	.align	1
	.globl	_Z11remove_cmp1v
	.type	_Z11remove_cmp1v, @function
_Z11remove_cmp1v:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L0, 4, 2
	SFPIADD	0, L1, L0, 0
	SFPNOP
	SFPNOP
	SFPNOP
	SFPENCC	3, 10
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z11remove_cmp1v, .-_Z11remove_cmp1v
	.align	1
	.globl	_Z12replace_cmp2v
	.type	_Z12replace_cmp2v, @function
_Z12replace_cmp2v:
	SFPLOADI	L1, 4, 1
	SFPLOADI	L2, 4, 2
	SFPMOV	L1, L0, 0
	SFPNOP
	SFPNOP
	SFPIADD	0, L2, L0, 2
	SFPNOP
	SFPNOP
	SFPNOP
	SFPENCC	3, 10
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z12replace_cmp2v, .-_Z12replace_cmp2v
	.align	1
	.globl	_Z12replace_muliv
	.type	_Z12replace_muliv, @function
_Z12replace_muliv:
	SFPLOAD	L1, 1, 0
	SFPLOADI	L0, 0, 16384
	SFPMUL	L1, L0, L4, L0, 0
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 0, 0
	SFPSTORE	L0, 0, 0
	ret
	.size	_Z12replace_muliv, .-_Z12replace_muliv
	.align	1
	.globl	_Z12replace_addiv
	.type	_Z12replace_addiv, @function
_Z12replace_addiv:
	SFPLOAD	L1, 1, 0
	SFPLOADI	L0, 0, 16384
	SFPADD	L10, L1, L0, L0, 0
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 0, 0
	SFPSTORE	L0, 0, 0
	ret
	.size	_Z12replace_addiv, .-_Z12replace_addiv
	.align	1
	.globl	_Z12replace_shftv
	.type	_Z12replace_shftv, @function
_Z12replace_shftv:
	SFPLOADI	L1, 4, 1
	SFPMOV	L1, L0, 0
	SFPNOP
	SFPNOP
	SFPSHFT	1, L0, L0, 1
	SFPNOP
	SFPNOP
	SFPSTORE	L1, 2, 0
	SFPSTORE	L0, 2, 8
	ret
	.size	_Z12replace_shftv, .-_Z12replace_shftv
	.align	1
	.globl	_Z12replace_cregv
	.type	_Z12replace_cregv, @function
_Z12replace_cregv:
	SFPMOV	L13, L0, 0
	SFPNOP
	SFPNOP
	SFPSHFT	1, L0, L0, 1
	SFPNOP
	SFPNOP
	SFPSTORE	L0, 2, 0
	SFPMOV	L13, L0, 0
	SFPNOP
	SFPNOP
	SFPAND	L14, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L0, 2, 0
	SFPMOV	L14, L0, 0
	SFPNOP
	SFPNOP
	SFPOR	L13, L0
	SFPNOP
	SFPNOP
	SFPSTORE	L0, 2, 0
	ret
	.size	_Z12replace_cregv, .-_Z12replace_cregv
	.globl	_ZN7ckernel13instrn_bufferE
	.section	.sbss,"aw",@nobits
	.align	2
	.type	_ZN7ckernel13instrn_bufferE, @object
	.size	_ZN7ckernel13instrn_bufferE, 4
_ZN7ckernel13instrn_bufferE:
	.zero	4
	.ident	"GCC: (GNU) 10.2.0"