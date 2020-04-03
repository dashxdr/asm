stacksize:	equ	1024

zero:	dc.l	0
	dc.l	__entry-zero
	ds.b	$70-(*-zero)
	dc.l	hbint-zero
	ds.b	$78-(*-zero)
	dc.l	vbint-zero
	ds.b	$100-(*-zero)

	dc.l	'SEGA'
__data:	dc.l	_topcode
	public	__data
	public	_topcode

	bss	__temp,128
	bss	__stack,stacksize
	bss	__clock,2
	public	__entry
__entry:	lea	__stack+stacksize,a7
	move.b	$a10001,d0
	andi.b	#$0f,d0
	beq.s	skip
	move.l	#$53454741,$a14000
skip:	move.l	#_topbss,d0
	andi.l	#$fff0,d0
	subi	#16,d0
	lea	$ff0000,a0
	clr.l	(a0)
	clr.l	4(a0)
	move.l	d0,8(a0)
	clr.l	12(a0)
	move	#$8174,$c00004
	move	#$8f02,$c00004
	andi	#$f8ff,sr
	jsr	main
hlt:	illegal

__main:	rts

vbint:	addq	#1,__clock
	rte
hbint:	rte
	public	vb
vb:	move	__clock,d0
vbw:	cmp	__clock,d0
	beq.s	vbw
	rts

printf:	move.l	4(a7),a1 ;control string
	lea	8(a7),a0 ;where to fetch data
	movem.l	a2-a4/d2-d7,-(a7)
	lea	cod0(pc),a2
	move.l	a0,a3
	move.l	a1,a4
	bsr	anyprintf
	movem.l	(a7)+,a2-a4/d2-d7
	rts
	public	__main
	public	printf
	public	_topbss
	public	main
	public	strcpy
strcpy:	move.l	4(a7),a0
	move.l	8(a7),a1
strcpylp:	move.b	(a1)+,(a0)+
	bne.s	strcpylp
	move.l	4(a7),a0
	rts
xr0:	equ	$440000 ;$300000
xr1:	equ	xr0+2
xr2:	equ	xr0+4
xr3:	equ	xr0+6
	public	co,ci
ci:	moveq	#0,d0
cilp:	btst	#2,xr2
	bne.s	cilp
	move.b	xr1,d0
	rts
cod0:	move.b	d0,xr0
cod0lp:	btst	#1,xr2
	beq.s	cod0lp
	rts
co:	move.b	5(a7),xr0
colp:	btst	#1,xr2
	beq.s	colp
	rts
	public	puts
puts:	move.l	4(a7),a0
	bra.s	putsenter
putslp:	bsr.s	cod0
putsenter:	move.b	(a0)+,d0
	bne.s	putslp
	moveq	#10,d0
	bsr.s	cod0
	rts
;a2=co routine, prints a char in d0
;a3=where to fetch data
;a4=control string
anyprintfdone:	rts
anyprintflp:	jsr	(a2)
anyprintf:	move.b	(a4)+,d0
	beq.s	anyprintfdone
	cmpi.b	#'%',d0
	bne.s	anyprintflp
	move.b	(a4)+,d0
	beq.s	anyprintfdone
	cmpi.b	#'s',d0
	beq.s	astring
	cmpi.b	#'t',d0
	beq.s	atab
	cmpi.b	#'n',d0
	beq.s	anewline
	cmpi.b	#'c',d0
	beq.s	achar
	cmpi.b	#'l',d0
	beq.s	longsizes
	cmpi.b	#'x',d0
	beq.s	hexwordout
	cmpi.b	#'d',d0
	beq.s	decwordout
	cmpi.b	#'u',d0
	beq.s	unsignedwordout
	bra.s	anyprintflp
longsizes:	move.b	(a4)+,d0
	beq.s	anyprintfdone
	cmpi.b	#'x',d0
	beq.s	hexlongout
	cmpi.b	#'d',d0
	beq.s	declongout
	cmpi.b	#'u',d0
	beq.s	unsignedlongout
	bra.s	anyprintflp
astring:	move.l	(a3)+,a0
astringlp:	move.b	(a0)+,d0
	beq.s	anyprintf
	jsr	(a2)
	bra.s	astringlp
achar:	move	(a3)+,d0
	bra.s	anyprintflp
anewline:	moveq	#10,d0
	bra.s	anyprintflp
atab:	moveq	#9,d0
	bra.s	anyprintflp
hexwordout:	moveq	#0,d0
	move	(a3)+,d0
	bra.s	hexnolead0
hexdigits:	dc.b	'0123456789abcdef'
hexlongout:	move.l	(a3)+,d0
hexnolead0:	lea	__temp+128,a0
	clr.b	-(a0)
hlp:	move	d0,d1
	andi	#15,d1
	move.b	hexdigits(pc,d1),-(a0)
	lsr.l	#4,d0
	bne.s	hlp
	bra.s	astringlp
unsignedwordout:	moveq	#0,d0
	move	(a3)+,d0
	bra.s	decnolead0
decwordout:	move	(a3)+,d0
	ext.l	d0
	bra.s	decnolead0
unsignedlongout:
declongout:	move.l	(a3)+,d0
decnolead0:	lea	__temp+128,a0
	clr.b	-(a0)
	tst.l	d0
	bpl.s	positive
	neg.l	d0
	bsr.s	pdeclp
	move.b	#'-',-(a0)
	bra.s	astringlp
positive:	bsr.s	pdeclp
	bra.s	astringlp	
pdeclp:	move.l	d0,d1
	clr	d1
	swap	d1
	divu	#10,d1
	swap	d1
	swap	d0
	move	d1,d0
	swap	d0
	divu	#10,d0
	move	d0,d1
	swap	d0
	addi	#'0',d0
	move.b	d0,-(a0)
	move.l	d1,d0
	bne.s	pdeclp
	rts

	public	malloc
malloc:	moveq	#0,d0
	move	4(a7),d0
	addi	#31,d0
	andi	#$fff0,d0
	move.l	#$ff0000,d1
ma1:	move.l	d1,a0
	btst	#0,12(a0)
	bne.s	ma1next
	cmp.l	8(a0),d0
	bne.s	ma1next
	move.l	#$01000000,12(a0)
	lea	16(a0),a0
	move.l	a0,d0
	bra.s	madone
ma1next:	move.l	(a0),d1
	bne.s	ma1
mafind:	btst	#0,12(a0)
	bne.s	manext
	cmp.l	8(a0),d0
	bcc.s	manext
	move.l	a0,a1
	sub.l	d0,8(a0)
	adda.l	8(a0),a1
	move.l	d0,8(a1)
	move.l	(a0),d1
	move.l	a1,(a0)
	move.l	d1,(a1)
	move.l	a0,4(a1)
	move.l	#$01000000,12(a1)
	move.l	a1,d0
	addi.l	#16,d0
	tst.l	d1
	beq.s	madone
	move.l	d1,a0
	move.l	a1,4(a0)
	bra.s	madone
manext:	move.l	4(a0),a0
	move.l	a0,d1
	bne.s	mafind
	moveq	#0,d0
madone:	move.l	d0,a0
	rts

	public	free
free:	move.l	4(a7),d0
	beq.s	freedone
	move.l	d0,a0
	lea	-16(a0),a0
	bclr	#0,12(a0)
	move.l	4(a0),d0
	beq.s	fnoprev
	move.l	d0,a1
	btst	#0,12(a1)
	bne.s	fnoprev
	move.l	8(a0),d0
	add.l	d0,8(a1)
	move.l	(a0),d0
	move.l	d0,(a1)
	move.l	a1,a0
	beq.s	fnoprev
	move.l	d0,a1
	move.l	a0,4(a1)
fnoprev:	move.l	(a0),d0
	beq.s	freedone
	move.l	d0,a1
	btst	#0,12(a1)
	bne.s	freedone
	move.l	8(a1),d0
	add.l	d0,8(a0)
	move.l	(a1),d0
	move.l	d0,(a0)
	beq.s	freedone
	move.l	d0,a1
	move.l	a0,4(a1)
freedone:	rts

	public	porta

porta:	lea	$a10000,a0
	move.b	#$40,9(a0)
	move.b	#0,3(a0)
	moveq	#10,d1
pa1:	dbra	d1,pa1
	move.b	3(a0),d0
	lsl	#2,d0
	andi	#$c0,d0
	move.b	#$40,3(a0)
	moveq	#10,d1
pa2:	dbra	d1,pa2
	move.b	3(a0),d1
	andi	#$3f,d1
	or	d1,d0
	not.b	d0
	move	d0,a0
	rts
bcopy:	move.l	4(a7),a0
	move.l	8(a7),a1
	move	12(a7),d0
bcopylp:	move.b	(a0)+,(a1)+
	subq	#1,d0
	bne.s	bcopylp
	rts
	public bcopy
putchar:	move	4(a7),d0
	bra	co
	public	putchar
debug:	move	sr,-(a7)
	move.l	$10.w,-(a7)
	rts
	public	debug
	public	ldiv
ldiv:	move.l	d2,-(a7)
	moveq	#0,d2
	move.l	8(a7),d1
	beq.s	devzero
	bpl.s	d1pos
	neg.l	d1
	not	d2
d1pos:	tst.l	d0
	bpl.s	d0pos
	neg.l	d0
	not	d2
d0pos:	move	d2,a1
	moveq	#1,d2
	suba.l	a0,a0
shl:	cmp.l	d1,d0
	bcs.s	divloop
	rol.l	#1,d1
	rol.l	#1,d2
	bra.s	shl
divloop:	lsr.l	#1,d1
	lsr.l	#1,d2
	beq.s	divdone
	cmp.l	d1,d0
	bcs.s	divloop
	sub.l	d1,d0
	adda.l	d2,a0
	bra.s	divloop
divdone:	move.l	a0,d0
	move	a1,d1
	bpl.s	nonegd0
	neg.l	d0
nonegd0:
devzero:	move.l	(a7)+,d2
	rts

	public	lmul
lmul:	move	6(a7),d1
	mulu	d0,d1
	move.l	d1,a0
	move	4(a7),d1
	mulu	d0,d1
	swap	d1
	clr	d1
	adda.l	d1,a0
	swap	d0
	mulu	6(a7),d0
	swap	d0
	clr	d0
	add.l	a0,d0
	rts

	
