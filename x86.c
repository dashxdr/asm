#include "asm.h"

uchar mode32;
int x86reg(void);

void needbyte(void) {error2("Operand must be byte size");}
void needword(void) {error2("Operand must be word size");}
void needsize(void) {error2("Must have size specifier");}
void incompat(void) {error2("Objects are of incompatable size");}

struct x86ea
{
	int x86flags;
	uchar x86disp;
	uchar x86type;
	uchar x86modrm; /* if type is register, this contains register # */
	uchar x86sib;
	uchar x86alen; /* 0=none spec or implied,1=short,2=long */
	uchar x86olen; /* 0 = none spec or implied, 1=byte,2=word,4=long */
	uchar x86seg; /* 0 = no seg override, 1=cs:, 2=ss:, 3=ds:, 4=es:, 5=fs:, 6=gs: */
	uchar x86reg;
	long x86value;
	struct sym *x86ref; /* if !=0, there was a reference to this symbol */

} x86ea1,x86ea2,x86ea3,*ea1,*ea2,*ea3;

/* types */
#define X86REG 0
#define X86INDIRECT 1
#define X86IMMED 2
/* ea flags */
#define X86ADDROF 1
#define X86SIB 2
#define X86MODRM 4
#define X86FORCE 8
#define X86PREVENT 16
#define X86SHORT 32
#define X86REL 64 /* part of a relative instruction (jcc, jmp, call) */
#define X86BRACES 128 /* if [] */

uchar x86regsizes[]=
{
2,2,2,2,2,2,2,2,
4,4,4,4,4,4,4,4,
1,1,1,1,1,1,1,1,
2,2,2,2,2,2
};

/*
 0   1   2   3   4   5   6   7
 ax, cx, dx, bx, sp, bp, si, di,

 8   9   10  11  12  13  14  15
eax,ecx,edx,ebx,esp,ebp,esi,edi,

 16  17  18  19  20  21  22  23
 al, cl, dl, bl, ah, ch, dh, bh,

 24  25  26  27  28  29
 es  cs  ss  ds  fs  gs

32-39  cr0-cr7
40-47  dr0-dr7
48-55  tr0-tr7
*/


char *getword(char *str) {
	char ch;
	char *new,*save;

	save=inpoint;
	while((isoksym((ch=*inpoint))))
		*str++=*inpoint++;
	*str=0;
	new=inpoint;
	inpoint=save;
	return new;
}

/* type=0 for ref, !=0 for rel */
void addref86(struct sym *asym, int size, int type) {
	addref(asym,(type ? 8 : 0) | (size==2 ? 0x93 : 0x91));
}

void x86outimm(struct x86ea *anea) {
	long val=anea->x86value;
	struct sym *ref;
	if(pass && (ref=anea->x86ref))
	{
		if(ref->symflags&APUBLIC && ~ref->symflags&ADEF)
			addref86(ref,anea->x86olen,anea->x86flags&X86REL);
		else
			if(~anea->x86flags & X86REL)
				addreloc(anea->x86olen);
	}
	switch(anea->x86olen)
	{
	case 0:
		error2("x86outimm() olen=0");
	case 1:
		bout((int)val);
		break;
	case 2:
		outw((int)val);
		break;
	case 4:
		outl(val);
	}
}

void x86opover(int n) {
	if(n!=mode32)
		bout(0x66);
}
void x86addrover(int n) {
	if(n!=mode32)
		bout(0x67);
}

uchar segoverbytes[]={0x26,0x2e,0x36,0x3e,0x64,0x65};
void x86outea(struct x86ea *anea, int opbyte1, int opbyte2, int subst) {
	uchar olen,type,alen,seg,disp;
	int flags;
	long val;
	struct sym *ref;

	type=anea->x86type;
	olen=anea->x86olen;
	alen=anea->x86alen;
	seg=anea->x86seg;
	disp=anea->x86disp;
	flags=anea->x86flags;
	val=anea->x86value;
	if(~flags&X86PREVENT)
	{
		if(flags&X86FORCE)
			bout(0x66);
		else
			if(olen!=1)
				x86opover(olen>2);
	}
	if(seg) bout(segoverbytes[seg-1]);
	if(type!=X86IMMED && alen) x86addrover(alen>2);
	bout(opbyte1);
	if(opbyte2>=0) bout(opbyte2);
	if(type!=X86IMMED)
	{
		if(flags&X86MODRM) bout(anea->x86modrm | (subst<<3));
		if(flags&X86SIB)
			bout(anea->x86sib);
		if(disp)
		{
			if(pass && (ref=anea->x86ref))
			{
				if(ref->symflags & APUBLIC && ~ref->symflags & ADEF)
					addref86(ref,disp,0);
				else
					if(~anea->x86flags&X86REL)
						addreloc(disp);
			}
			if(disp==1)
				bout((int)val);
			else if(disp==2)
				outw((int)val);
			else
				outl(val);
		}
	} else
	{
		if(pass && (ref=anea->x86ref))
		{
			if(ref->symflags & APUBLIC && ~ref->symflags & ADEF)
				addref86(ref,olen,0);
			else
				addreloc(olen);
		}
		switch(olen)
		{
		case 0: /* should never happen */
			error2("x86emitea(), immediate type w/ olen=0");
		case 1:
			bout((int)val);
			break;
		case 2:
			outw((int)val);
			break;
		case 4:
			outl(val);
		}
	}
}



uchar x86singleind1[]={
255,255,255,7,255,255,4,5,0,1,2,3,4,255,6,7
};
uchar x86singleind2[]={
255,255,255,0x47,255,0x46,0x44,0x45
};

int x86ea(struct x86ea *anea) {
	uchar reg,reg1,reg2,reg3,scale;
	char ch;
	char name[64];
	char *new;
	struct sym *asym;

	bzero(anea,sizeof(struct x86ea));
	anea->x86flags|=X86MODRM;
	while((isokfirst((ch=skipwhite()))))
	{
		new=getword(name);
		if(!strcmp(name,"byte"))
		{
			anea->x86olen=1;
			inpoint=new;
		} else if(!strcmp(name,"word"))
		{
			anea->x86olen=2;
			inpoint=new;
		} else if(!strcmp(name,"long"))
		{
			anea->x86olen=4;
			inpoint=new;
		} else if(!strcmp(name,"near"))
		{
			anea->x86alen=mode32 ? 4 : 2;
			inpoint=new;
		} else if(!strcmp(name,"short"))
		{
			anea->x86flags|=X86SHORT;
			inpoint=new;
		} else if(!strcmp(name,"offset"))
		{
			anea->x86flags|=X86ADDROF;
			inpoint=new;
/*
		} else if(!strcmp(name,"ptr"))
		{
			inpoint=new;
		} else if(!strcmp(name,"dword"))
		{
			anea->x86olen=4;
			inpoint=new;
*/
		} else break;
	}

	reg=x86reg();
	if(reg!=255)
	{
		if(reg>=24 && reg<=29 && at()==':') /* segment */
		{
			forward1();
			anea->x86seg=reg-23;
			ch=at();
		} else
		{
			anea->x86reg=reg;
			if(reg<24)
				anea->x86modrm=(reg&7) | 0xc0;
			anea->x86type=X86REG;
			anea->x86olen=x86regsizes[reg];
			return X86REG;
		}
	}
	if(ch=='[')
	{
		anea->x86flags|=X86BRACES;
		forward1();
		skipwhite();
		reg=x86reg();
		if(reg==255)
		{
			rexpr();
			if(at()!=']') syntaxerr();
			else forward1();
			if(exprtype==ABSTYPE && !anea->x86alen)
				anea->x86alen=mode32 ? 4 : 2;
			goto exprind;
		}
		reg1=reg;scale=0;reg2=reg3=255;
		for(;;)
		{
			skipwhite();
			if((ch=at())=='*')
			{
				if(reg2==255) {reg2=reg1;reg1=255;}
				if(scale) badmode();
				forward1();
				ch=get();
				if(ch=='2') scale=2;
				else if(ch=='4') scale=3;
				else if(ch=='8') scale=4;
				else {badmode();back1();}
			} else if(ch=='+')
			{
				forward1();
				ch=skipwhite();
				reg=x86reg();
				if(reg!=255)
				{
					if(reg2!=255) badmode();
					else reg2=reg;
				} else break;
			} else break;
		}
		if(ch!=']')
			rexpr();
		else exprtype=NOTHING;
		if(at()==']') forward1();
		else syntaxerr();
		if(scale)
			if(reg2<8 || reg2>15) badmode();
		if(reg2!=255)
		{
			if((reg1!=255 && (reg1>15 || reg2>15 || (reg1^reg2)&8)) || (reg1==255 && (reg2<8 || reg2>15))) badmode();
			else if(reg1!=255 && reg1<8)
			{
				if((reg1!=3 && reg1!=4) || (reg2!=6 && reg2!=7)) badmode();
				else
				{
					anea->x86alen=2;
					anea->x86modrm=(reg1==3 ? 0 : 2) + (reg2-6);
					if(exprtype!=NOTHING)
					{
						if(exprtype==RELTYPE)
						{
							anea->x86ref=(void *)exprval;
							exprval=((struct sym *)exprval)->symvalue+((struct sym *)exprval)->symdelta;
							anea->x86value=exprval;
							anea->x86modrm+=0x80;
							anea->x86disp=2;
						} else
						{
							anea->x86value=exprval;
							if(exprval>=-0x80 && exprval<=0x7f)
							{
								anea->x86modrm+=0x40;
								anea->x86disp=1;
							}

							else
							{
								anea->x86modrm+=0x80;
								anea->x86disp=2;
							}
						}
					}
					anea->x86alen=2;
					anea->x86type=X86INDIRECT;
					return X86INDIRECT;
				}
			} else
			{
				anea->x86flags |= X86SIB;
				if(exprtype!=NOTHING)
				{
					if(exprtype==RELTYPE)
					{
						anea->x86ref=(void *)exprval;
						exprval=((struct sym *)exprval)->symvalue+((struct sym *)exprval)->symdelta;
						anea->x86value=exprval;
						anea->x86modrm=0x84;
						anea->x86disp=4;
					} else if(exprtype==ABSTYPE)
					{
						anea->x86value=exprval;
						if(exprval<-0x80 || exprval>0x7f || reg1==255)
						{
							anea->x86modrm=0x84;
							anea->x86disp=4;
						}
						else
						{
							anea->x86modrm=0x44;
							anea->x86disp=1;
						}
					} else badmode();
				} else
					anea->x86modrm=0x04;
				if(!scale) scale=1;
				if(reg1==255)
				{
					reg1=13;
					anea->x86modrm=0x04;
					if(exprtype==NOTHING) anea->x86disp=4;
				}
				anea->x86sib=((scale-1)<<6) | ((reg2-8)<<3) | (reg1-8);
				if(reg2==12) badmode(); /* esp */
				anea->x86alen=4;
				anea->x86type=X86INDIRECT;
				return X86INDIRECT;
			}
			return 0; /* choke */
		} else if(reg1<16)
		{
			if(exprtype==NOTHING)
			{
				if(x86singleind1[reg1]==255) badmode();
				else anea->x86modrm=x86singleind1[reg1];
				if(reg1<8)
					anea->x86alen=2;
				else
					anea->x86alen=4;
				if(reg1==12) /* [esp] */
				{
					anea->x86flags|=X86SIB;
					anea->x86sib=0x24;
				}
			} else
			{
				if(exprtype==RELTYPE)
				{
					anea->x86ref=(void *)exprval;
					exprval=((struct sym *)exprval)->symvalue+((struct sym *)exprval)->symdelta;
					anea->x86value=exprval;
				}
				anea->x86value=exprval;
				if(reg1<8)
				{
					anea->x86alen=2;
					if(x86singleind2[reg1]==255) badmode();
					else anea->x86modrm=x86singleind2[reg1];
				} else
				{
					anea->x86alen=4;
					if(reg1==12) /* [esp+#] */
					{
						anea->x86sib=0x24;
						anea->x86flags|=X86SIB;
					}
					anea->x86modrm=0x40-8+reg1;
				}
				if(exprtype==RELTYPE || exprval<-0x80 || exprval>0x7f)
				{
					anea->x86modrm+=0x40;
					if(reg1<8)
						anea->x86disp=2;
					else
						anea->x86disp=4;
				} else
					anea->x86disp=1;
			}
			anea->x86type=X86INDIRECT;
			return X86INDIRECT;
		} else badmode();
		return 0; /* choke */
	}
	rexpr();
	if(at()=='[') syntaxerr();
exprind:
	if(exprtype==RELTYPE)
	{
		asym=(void *)exprval;
		anea->x86ref=asym;
		exprval=asym->symvalue+asym->symdelta;
		if(!anea->x86alen && ~anea->x86flags & X86ADDROF)
			anea->x86alen=mode32 ? 4 : 2;
	}
	anea->x86value=exprval;
	if(anea->x86alen && ~anea->x86flags & X86ADDROF)
	{
		if(anea->x86alen==2)
		{
			anea->x86modrm=6;
			anea->x86disp=2;
		} else
		{
			anea->x86modrm=5;
			anea->x86disp=4;
		}
		anea->x86type=X86INDIRECT;
		return X86INDIRECT;
	}
	if(exprtype!=ABSTYPE && ~anea->x86flags & X86ADDROF)
		badmode();
	if(!anea->x86olen)
	{
		if(exprval>=-0x80 && exprval<=0x7f && ~anea->x86flags & X86ADDROF)
			anea->x86olen=1;
		else if(anea->x86flags&X86ADDROF)
		{
			if(mode32 || exprval<-0x8000 || exprval>0x7fff)
				anea->x86olen=4;
			else
				anea->x86olen=2;
		}
	}
	return anea->x86type=X86IMMED;
}
/*
 0   1   2   3   4   5   6   7
 ax, cx, dx, bx, sp, bp, si, di,

 8   9   10  11  12  13  14  15
eax,ecx,edx,ebx,esp,ebp,esi,edi,

 16  17  18  19  20  21  22  23
 al, cl, dl, bl, ah, ch, dh, bh,

 24  25  26  27  28  29
 es  cs  ss  ds  fs  gs
*/


void x86init(void) {
	mode32=1;
	ea1=&x86ea1;
	ea2=&x86ea2;
	ea3=&x86ea3;
}


void x86nop(void) {bout(0x90);}
void x86das(void) {bout(0x2f);}
void x86pushf(void) {x86opover(0);bout(0x9c);}
void x86pushfd(void) {x86opover(1);bout(0x9c);}
void x86popf(void) {x86opover(0);bout(0x9d);}
void x86popfd(void) {x86opover(1);bout(0x9d);}
void x86pusha(void) {x86opover(0);bout(0x60);}
void x86pushad(void) {x86opover(1);bout(0x60);}
void x86popa(void) {x86opover(0);bout(0x61);}
void x86popad(void) {x86opover(1);bout(0x61);}
void x86hlt(void) {bout(0xf4);}
void x86cmc(void) {bout(0xf5);}
void x86clc(void) {bout(0xf8);}
void x86cld(void) {bout(0xfc);}
void x86cli(void) {bout(0xfa);}
void x86stc(void) {bout(0xf9);}
void x86std(void) {bout(0xfd);}
void x86sti(void) {bout(0xfb);}
void x86cbw(void) {x86opover(0);bout(0x98);}
void x86cwde(void) {x86opover(1);bout(0x98);}
void x86cwd(void) {x86opover(0);bout(0x99);}
void x86cdq(void) {x86opover(1);bout(0x99);}
void x86aaa(void) {bout(0x37);}
void x86aas(void) {bout(0x3f);}
void x86wait(void) {bout(0x9b);}
void x86leave(void) {bout(0xc9);}
void x86sahf(void) {bout(0x9e);}
void x86lahf(void) {bout(0x9f);}
void x86movsb(void) {bout(0xa4);}
void x86movsw(void) {x86opover(0);bout(0xa5);}
void x86movsd(void) {x86opover(1);bout(0xa5);}
void x86cmpsb(void) {bout(0xa6);}
void x86cmpsw(void) {x86opover(0);bout(0xa7);}
void x86cmpsd(void) {x86opover(1);bout(0xa7);}
void x86stosb(void) {bout(0xaa);}
void x86stosw(void) {x86opover(0);bout(0xab);}
void x86stosd(void) {x86opover(1);bout(0xab);}
void x86lodsb(void) {bout(0xac);}
void x86lodsw(void) {x86opover(0);bout(0xad);}
void x86lodsd(void) {x86opover(1);bout(0xad);}
void x86scasb(void) {bout(0xae);}
void x86scasw(void) {x86opover(0);bout(0xaf);}
void x86scasd(void) {x86opover(1);bout(0xaf);}
void x86into(void) {bout(0xce);}
void x86iret(void) {x86opover(0);bout(0xcf);}
void x86iretd(void) {x86opover(1);bout(0xcf);}
void x86xlat(void) {bout(0xd7);}
void x86lock(void) {bout(0xf0);}
void x86use16(void) {mode32=0;}
void x86use32(void) {mode32=1;}

void x86rets(int num) {
	expr();
	if(exprtype==NOTHING) bout(num);
	else
	{
		bout(num-1);
		outw((short) exprval);
	}
}
void x86retf(void) {x86rets(0xcb);}
void x86ret(void) {x86rets(0xc3);}

int x86comma(void) {
	int err;
	err=skipwhite()!=',';
	if(err) syntaxerr();
	else forward1();
	return err;
}

void x86dstrings(uchar op) {
	uchar type1,alen1,olen1,modrm1,seg1;
	uchar type2,alen2,olen2,modrm2,seg2;
	type1=x86ea(ea1);
	if(type1==X86IMMED || type1==X86REG) {badmode();return;}
	if(x86comma()) return;
	type2=x86ea(ea2);
	if(type2==X86IMMED || type2==X86REG) {badmode();return;}
	alen1=ea1->x86alen;
	alen2=ea2->x86alen;
	olen1=ea1->x86olen;
	olen2=ea2->x86olen;
	if(alen1!=alen2 || (olen1 && olen2 && olen1!=olen2)) {incompat();return;}
	seg1=ea1->x86seg;
	seg2=ea2->x86seg;
	if(seg1 && seg1!=1) {error2("Cannot override segment on second operand");return;}
	modrm1=ea1->x86modrm;
	modrm2=ea2->x86modrm;
	if(alen1==2)
	{
		if(modrm1!=5 || modrm2!=4) {badmode();return;}
		if(olen1>1) x86opover(olen1==4);
		x86addrover(0);
		if(seg2 && seg2!=4) bout(segoverbytes[seg2-1]);
		bout(olen1==1 ? op : op+1);
	} else
	{
		if(modrm1!=7 || modrm2!=6) {badmode();return;}
		if(olen1>1) x86opover(olen1==4);
		x86addrover(1);
		if(seg2 && seg2!=4) bout(segoverbytes[seg2-1]);
		bout(olen1==1 ? op : op+1);
	}
}
void x86movs(void) {x86dstrings(0xa4);}
void x86cmps(void) {x86dstrings(0xa6);}

void x86sstrings(uchar op, uchar side) {
	uchar type,alen,olen,modrm,seg;
	type=x86ea(ea1);
	if(type==X86IMMED || type==X86REG) {badmode();return;}
	alen=ea1->x86alen;
	olen=ea1->x86olen;
	modrm=ea1->x86modrm;
	if(!olen) {needsize();return;}
	seg=ea1->x86seg;
	if(alen==2)
	{
		if(modrm!=side+4) {badmode();return;}
		x86opover(olen==4);
		x86addrover(0);
		if(seg && seg!=(side ? 1 : 4)) bout(segoverbytes[seg-1]);
		bout(olen==1 ? op : op+1);
	} else
	{
		if(modrm!=side+6) {badmode();return;}
		x86opover(olen==4);
		x86addrover(1);
		if(seg && seg!=(side ? 1 : 4)) bout(segoverbytes[seg-1]);
		bout(olen==1 ? op : op+1);
	}
}
void x86lods(void) {x86sstrings(0xac,0);}
void x86scas(void) {x86sstrings(0xae,1);}
void x86stos(void) {x86sstrings(0xaa,1);}


void x86arrith(uchar field) {
	uchar type1,reg1,len1;
	uchar type2,reg2,len2;
	uchar op;

	type1=x86ea(ea1);
	if(x86comma()) return;
	type2=x86ea(ea2);
	op=field<<3;
	switch(type1)
	{
	case X86REG:
		reg1=ea1->x86reg;
		if(reg1>=24) badreg();
		else if(type2==X86IMMED && (reg1==0 || (reg1==8 && ea2->x86olen>1) || reg1==16))
		{
			ea2->x86olen=ea1->x86olen;
			x86outea(ea2,reg1<16 ? op+5 : op+4,-1,0);
			return;
		} else if(type2!=X86IMMED)
		{
			if(ea2->x86olen && ea1->x86olen!=ea2->x86olen) {incompat();return;}
			ea2->x86olen=ea1->x86olen;
			x86outea(ea2,reg1<16 ? op+3 : op+2,-1,reg1&7);
			return;
		} /* fall through to x86indirect */
	case X86INDIRECT:
		if(type2==X86IMMED)
		{
			if(!(len1=ea1->x86olen)) {needsize();return;}
			if((len2=ea2->x86olen)!=1) len2=ea2->x86olen=len1;
			if(len1==len2)
				x86outea(ea1,len1==1 ? 0x80 : 0x81,-1,field);
			else
			{
				if(len2!=1) {error2("Must be byte size for immediate operand");ea2->x86olen=1;}
					x86outea(ea1,0x83,-1,field);
			}
			x86outimm(ea2);
		} else if(type2==X86REG)
		{
			reg2=ea2->x86reg;
			if(reg2>=24) {badreg();return;}
			if(type2!=X86REG) {badmode();return;}
			len2=ea2->x86olen;
			if((len1=ea1->x86olen) && len1!=len2) {incompat();return;}
			ea1->x86olen=len2;
			x86outea(ea1,len2==1 ? op : op+1,-1, reg2&7);
		} else badmode();
		break;
	case X86IMMED:
		badmode();
		break;
	}
}
void x86add(void) {x86arrith(0);}
void x86or(void)  {x86arrith(1);}
void x86adc(void) {x86arrith(2);}
void x86sbb(void) {x86arrith(3);}
void x86and(void) {x86arrith(4);}
void x86sub(void) {x86arrith(5);}
void x86xor(void) {x86arrith(6);}
void x86cmp(void) {x86arrith(7);}

void x86test(void) {
	uchar type1,reg1,len1;
	uchar type2,len2;

	type1=x86ea(ea1);
	if(x86comma()) return;
	type2=x86ea(ea2);
	len1=ea1->x86olen;
	len2=ea2->x86olen;
	if(type1==X86REG)
	{
		reg1=ea1->x86reg;
		if(reg1>=24) badreg();
		else if(type2==X86IMMED && !(reg1&7))
		{
			ea2->x86olen=ea1->x86olen;
			x86outea(ea2,reg1<16 ? 0xa9 : 0xa8,-1,0);
			return;
		}
	}
	if(type2==X86INDIRECT)
		badmode();
	else if(type2==X86IMMED)
	{
		if(!len1) {needsize();return;}
		ea2->x86olen=len1;
		x86outea(ea1,len1<2 ? 0xf6 : 0xf7,-1,0);
		x86outimm(ea2);
	} else /* X86REG */
	{
		if(len1 && len1!=len2) {incompat();return;}
		ea1->x86olen=len2;
		x86outea(ea1,len2<2 ? 0x84 : 0x85,-1,ea2->x86reg & 7);
	}
}

void x86int(void) {
	int num;

	expr();
	num=exprval;
	if(num<0 || num>255) outofrange();
	bout(0xcd);
	bout(num);
}


int x86reg(void) {
	char *save;
	char tt[4],*p,ch;
	int val,i;

	save=inpoint;
	p=tt;
	i=0;
	while((isoksym((ch=get()))))
	{
		*p++=tolower(ch);
		i++;
		if(i==4)
		{
			inpoint=save;
			return 255;
		}
	}
	back1();
	*p=0;
	if(i<2)
	{
		inpoint=save;
		return 255;
	}
	if(i==3)
	{
		switch(*tt)
		{
		case 'e':
			val=tt[1]*256+tt[2];
			switch(val)
			{
			case 'a'*256+'x':
				return 8;
			case 'c'*256+'x':
				return 9;
			case 'd'*256+'x':
				return 10;
			case 'b'*256+'x':
				return 11;
			case 's'*256+'p':
				return 12;
			case 'b'*256+'p':
				return 13;
			case 's'*256+'i':
				return 14;
			case 'd'*256+'i':
				return 15;
			}
			break;
		case 'c':
			if(tt[1]=='r' && tt[2]>='0' && tt[2]<='7')
				return tt[2]+(32-'0');
			break;
		case 'd':
			if(tt[1]=='r' && tt[2]>='0' && tt[2]<='7')
				return tt[2]+(40-'0');
			break;
		case 't':
			if(tt[1]=='r' && tt[2]>='0' && tt[2]<='7')
				return tt[2]+(48-'0');
			break;
		}
		inpoint=save;
		return 255;
	}
	val=tt[0]*256+tt[1];
	switch(val)
	{
	case 'a'*256+'x':
		return 0;
	case 'c'*256+'x':
		return 1;
	case 'd'*256+'x':
		return 2;
	case 'b'*256+'x':
		return 3;
	case 's'*256+'p':
		return 4;
	case 'b'*256+'p':
		return 5;
	case 's'*256+'i':
		return 6;
	case 'd'*256+'i':
		return 7;
	case 'a'*256+'l':
		return 16;
	case 'c'*256+'l':
		return 17;
	case 'd'*256+'l':
		return 18;
	case 'b'*256+'l':
		return 19;
	case 'a'*256+'h':
		return 20;
	case 'c'*256+'h':
		return 21;
	case 'd'*256+'h':
		return 22;
	case 'b'*256+'h':
		return 23;
	case 'e'*256+'s':
		return 24;
	case 'c'*256+'s':
		return 25;
	case 's'*256+'s':
		return 26;
	case 'd'*256+'s':
		return 27;
	case 'f'*256+'s':
		return 28;
	case 'g'*256+'s':
		return 29;
	default:
		inpoint=save;
		return 255;
	}
	return 255;
}

char * x86regnames[]={
"ax","cx","dx","bx","sp","bp","si","di",
"eax","ecx","edx","ebx","esp","ebp","esi","edi",
"al","cl","dl","bl","ah","ch","dh","bh",
"es","cs","ss","ds","fs","gs"
};

void listea(struct x86ea *anea) {
	switch(anea->x86type)
	{
	case X86REG:
		printf("Register %s, olen=%d\n",x86regnames[anea->x86reg],anea->x86olen);
		break;
	case X86IMMED:
		printf("Immediate %ld olen=%d\n",anea->x86value,anea->x86olen);
		break;
	case X86INDIRECT:
		printf("Indirect %ld modrm=%x,sib=%x,alen=%d,olen=%d,seg=%d,ref=%p\n",anea->x86value,anea->x86modrm,anea->x86sib,
				anea->x86alen,anea->x86olen,anea->x86seg,anea->x86ref);
		break;
	}
}

uchar x86pushbytes[]={0x06,0x0e,0x16,0x1e,0xa0,0xa8};
void x86push(void) {
	int type,reg,len;
	char ch;

	while((ch=skipwhite()))
	{
		if(ch=='\n' || ch==';') break;
		type=x86ea(ea1);
		if(type==X86REG)
		{
			reg=ea1->x86reg;
			if(reg<16)
			{
				x86opover(reg>=8);
				bout(0x50+(reg&7));
			} else if(reg>=24 && reg<=29)
			{
				if(reg>=28)
					bout(0x0f);
				bout(x86pushbytes[reg-24]);
			} else
				badreg();
		} else if(type==X86IMMED)
		{
			if(!(len=ea1->x86olen))
				len=ea1->x86olen=mode32 ? 4 : 2;
			if(len!=1) {x86opover(len==4);bout(0x68);}
			else bout(0x6a);
			x86outimm(ea1);
		} else if(type==X86INDIRECT)
		{
			if(!ea1->x86olen)
				needsize();
			x86outea(ea1,0xff,-1,6);
		}
	}
}

void x86pop(void) {
	int type,reg;
	char ch;
	while((ch=skipwhite()))
	{
		if(ch=='\n' || ch==';') break;
		type=x86ea(ea1);
		if(type==X86REG)
		{
			reg=ea1->x86reg;
			if(reg<16)
			{
				x86opover(reg>=8);
				bout(0x58+(reg&7));
			} else if(reg>=24 && reg<=29)
			{
				if(reg>=28)
					bout(0x0f);
				bout(x86pushbytes[reg-24]+1);
			} else
				badreg();
		} else if(type==X86IMMED)
		{
			badmode();
		} else if(type==X86INDIRECT)
		{
			if(!ea1->x86olen)
				needsize();
			x86outea(ea1,0x8f,-1,0);
		}
	}
}

void x86reps(int op) {
	char repcode[64];
	void (*func)(void);

	bout(op);
	gather(repcode);
	skipwhite();
	func=scan(repcode,currentlist);
	if(func) func();
}
void x86rep(void) {x86reps(0xf3);}
void x86repe(void) {x86reps(0xf3);}
void x86repz(void) {x86reps(0xf3);}
void x86repne(void) {x86reps(0xf2);}
void x86repnz(void) {x86reps(0xf2);}

void x86incdec(uchar isdec) {
int type;
uchar reg,olen;
	type=x86ea(ea1);
	if(type==X86IMMED)
		badmode();
	else if(type==X86REG)
	{
		reg=ea1->x86reg;
		if(reg<16)
		{
			x86opover(reg>=8);
			bout(0x40+(reg&7) + (isdec<<3));
		} else goto incrm;
	} else /* X86INDIRECT */
	{
incrm:
		olen=ea1->x86olen;
		if(!olen)
			needsize();
		else x86outea(ea1,olen==1 ? 0xfe : 0xff,-1,isdec);
	}
}
void x86inc(void) {x86incdec(0);}
void x86dec(void) {x86incdec(1);}
void x86oneop(uchar field) {
uchar type,len;

	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
	len=ea1->x86olen;
	if(!len) {needsize();return;}
	x86outea(ea1,len==1 ? 0xf6 : 0xf7,-1,field);
}
void x86not(void) {x86oneop(2);}
void x86neg(void) {x86oneop(3);}
void x86mul(void) {x86oneop(4);}
void x86div(void) {x86oneop(6);}
void x86idiv(void) {x86oneop(7);}
void x86imul(void) {
	uchar type1,len1,reg1;
	uchar type2;
	uchar type3;
	type1=x86ea(ea1);
	if(skipwhite()!=',')
	{
		if(type1==X86IMMED) {badmode();return;}
		len1=ea1->x86olen;
		if(!len1) {needsize();return;}
		x86outea(ea1,len1==1 ? 0xf6 : 0xf7,-1,5);
		return;
	}
	forward1();
	type2=x86ea(ea2);
	if(type1!=X86REG) {badmode();return;}
	reg1=ea1->x86reg;
	if(reg1>=24) {badreg();return;}
	if(type2!=X86IMMED)
	{
		if(skipwhite()!=',')
		{
			len1=ea1->x86olen;
			if(len1==1) {badreg();return;}
			if(ea2->x86olen && ea2->x86olen!=len1) {incompat();return;}
			ea2->x86olen=len1;
			x86outea(ea2,0x0f,0xaf,reg1&7);
			return;
		}
		forward1();
		type3=x86ea(ea3);
		if(type3!=X86IMMED) {badmode();return;}
	} else
	{
		bcopy(ea2,ea3,sizeof(struct x86ea));
		bcopy(ea1,ea2,sizeof(struct x86ea));
	}
	if(reg1>=16) {badreg();return;}
/* ea1=16 or 32 bit reg, ea2=ea, ea3=immed */
	if(ea2->x86olen && ea2->x86olen!=ea1->x86olen) {incompat();return;}
	ea2->x86olen=ea1->x86olen;
	if(ea3->x86olen==1)
	{
		x86outea(ea2,0x6b,-1,reg1&7);
		x86outimm(ea3);
	} else
	{
		if(ea3->x86olen && ea3->x86olen!=ea1->x86olen) {incompat();return;}
		ea3->x86olen=ea1->x86olen;
		x86outea(ea2,0x69,-1,reg1&7);
		x86outimm(ea3);
	}
}


void x86movx(int op) {
	uchar reg,len,type;

	reg=x86reg();
	if(reg==255) {badmode();return;}
	if(reg>=16) {badreg();return;}
	if(x86comma()) return;
	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
	len=ea1->x86olen;
	if(!len) {
		if(reg<8)
			len=ea1->x86olen=1;
		else {needsize();return;}
	}
	if(len==4) {error2("movsx/movzx cannot move from long size operand");return;}
	if(mode32 != (reg>=8)) ea1->x86flags|=X86FORCE;
	else ea1->x86flags|=X86PREVENT;
	x86outea(ea1,0x0f,len==1 ? op : op+1,reg&7);

/*	mode32save=mode32;
	if(len==1)
	{
		if(mode32 != (reg>=8)) ea1->x86flags|=X86FORCE;
		x86outea(ea1,0x0f,op,reg&7);
	}
	else
	{
		mode32=0;
		x86outea(ea1,0x0f,op+1,reg&7);
		mode32=mode32save;
	}*/
	
}
void x86movsx(void) {x86movx(0xbe);}
void x86movzx(void) {x86movx(0xb6);}

void x86rots(uchar field) {
	uchar type1,len1;
	uchar type2,reg2;
	uchar val;
	type1=x86ea(ea1);
	if(x86comma()) return;
	type2=x86ea(ea2);
	if(type2==X86INDIRECT) {badmode();return;}
	if(type2==X86REG && (reg2=ea2->x86reg)!=17) {badreg();return;}
	len1=ea1->x86olen;
	if(!len1) {needsize();return;}
	if(type1==X86REG && ea1->x86reg>=24) {badreg();return;}
	if(type2==X86REG)
		x86outea(ea1,len1!=1 ? 0xd3 : 0xd2,-1,field);
	else
	{
		val=ea2->x86value;
		if(val==1)
			x86outea(ea1,len1!=1 ? 0xd1 : 0xd0,-1,field);
		else
		{
			x86outea(ea1,len1!=1 ? 0xc1 : 0xc0,-1,field);
			bout(val);
		}
	}
}
void x86rol(void) {x86rots(0);}
void x86ror(void) {x86rots(1);}
void x86rcl(void) {x86rots(2);}
void x86rcr(void) {x86rots(3);}
void x86sal(void) {x86rots(4);}
void x86shl(void) {x86rots(4);}
void x86shr(void) {x86rots(5);}
void x86sar(void) {x86rots(7);}

void x86enter(void) {
	int val1,val2;
	expr();
	val1=exprval;
	if(x86comma()) return;
	expr();
	val2=exprval;
	bout(0xc8);
	outw(val1);
	bout(val2);
}

void x86lea(void) {
	uchar reg;
	uchar type1;
	reg=x86reg();
	if(reg==255 || reg>=16) {badreg();return;}
	if(x86comma()) return;
	type1=x86ea(ea1);
	if(type1==X86REG || type1==X86IMMED) {badmode();return;}
	ea1->x86olen=1;
	if(mode32 != (reg>=8)) ea1->x86flags|=X86FORCE;
	x86outea(ea1,0x8d,-1,reg&7);
}

void x86xchg(void) {
	struct x86ea *e1,*e2;
	uchar type1,len1,reg1;
	uchar type2,reg2;
	type1=x86ea(ea1);
	if(x86comma()) return;
	type2=x86ea(ea2);
	if(type1!=X86REG && type2==X86REG)
	{
		e1=ea2;
		e2=ea1;
	} else
	{
		e1=ea1;
		e2=ea2;
	}
	if(e1->x86type!=X86REG) {badmode();return;}
	reg1=e1->x86reg;
	if(reg1>=24) {badreg();return;}
	type2=e2->x86type;
	if(type2==X86IMMED) {badmode();return;}
	len1=e1->x86olen;
	if(e2->x86olen && e2->x86olen!=len1) {incompat();return;}
	e2->x86olen=len1;
	if(type2==X86REG)
	{
		reg2=e2->x86reg;
		if(reg1==0 || reg1==8)
		{
			x86opover(reg1==8);
			bout(0x90+(reg2&7));
			return;
		} else if(reg2==0 || reg2==8)
		{
			x86opover(reg2==8);
			bout(0x90+(reg1&7));
			return;
		}
	}
	x86outea(e2,len1==1 ? 0x86 : 0x87,-1,reg1&7);
}

uchar controlops[]={0x22,0x23,0x26};

void x86mov(void) {
	uchar type1,len1,reg1,modrm1;
	uchar type2,len2,reg2,modrm2;
	uchar op;

	type1=x86ea(ea1);
	if(x86comma()) return;
	type2=x86ea(ea2);
	len1=ea1->x86olen;
	len2=ea2->x86olen;
	if(type1==X86IMMED) {badmode();return;}
	if(type2==X86IMMED)
	{
		if(!len1) {needsize();return;}
		ea2->x86olen=len1;
		if(type1==X86REG && (reg1=ea1->x86reg)<24)
		{
			if(len1>1)
			{
				x86opover(len1==4);
				bout(0xb8+(reg1&7));
			} else
				bout(0xb0+(reg1&7));
		} else
			x86outea(ea1,len1==1 ? 0xc6 : 0xc7,-1,0);
		x86outimm(ea2);
		return;
	}
	reg1=ea1->x86reg;
	reg2=ea2->x86reg;
	if(type1==X86REG && type2==X86INDIRECT && reg1<24 && !(reg1&7))
	{
		modrm2=ea2->x86modrm;
		if((mode32 && modrm2==5) || (!mode32 && modrm2==6))
		{
			ea2->x86flags&=~X86MODRM;
			ea2->x86olen=ea1->x86olen;
			x86outea(ea2,reg1==16 ? 0xa0 : 0xa1,-1,0);
			return;				
		}
	}
	if(type2==X86REG && type1==X86INDIRECT && reg2<24 && !(reg2&7))
	{
		modrm1=ea1->x86modrm;
		if((mode32 && modrm1==5) || (!mode32 && modrm1==6))
		{
			ea1->x86flags&=~X86MODRM;
			ea1->x86olen=ea2->x86olen;
			x86outea(ea1,reg2==16 ? 0xa2 : 0xa3,-1,0);
			return;				
		}
	}

	if(type1==X86REG && type2==X86REG)
	{
		if(reg1>=32 && reg1<=55)
		{
			if(reg2<8 || reg2>15) {badreg();return;}
			reg2&=7;
			bout(0x0f);
			op=controlops[(reg1-32)>>3];
			reg1&=7;
			bout(op);
			bout(0xc0 | (reg1<<3) | reg2);
			return;
		}
		if(reg2>=32 && reg2<=55)
		{
			if(reg1<8 || reg1>15) {badreg();return;}
			reg1&=7;
			bout(0x0f);
			op=controlops[(reg2-32)>>3]-2;
			reg2&=7;
			bout(op);
			bout(0xc0 | (reg2<<3) | reg1);
			return;
		}
	}

	if(type1==X86REG && reg1>=24 && reg1<=29)
	{
		if(!len2) ea2->x86olen=2;
		else if(len2!=2) {incompat();return;}
		ea2->x86flags|=X86PREVENT;
		x86outea(ea2,0x8e,-1,reg1-24);
		return;
	}
	if(type2==X86REG && reg2>=24 && reg2<=29)
	{
		if(!len1) ea1->x86olen=2;
		else if(len1!=2) {incompat();return;}
		ea1->x86flags|=X86PREVENT;
		x86outea(ea1,0x8c,-1,reg2-24);
		return;
		}
	if(type1==X86REG)
	{
		if(len2 && len2!=len1) {incompat();return;}
		ea2->x86olen=len1;
		x86outea(ea2,len1==1 ? 0x8a : 0x8b,-1,reg1&7);
		return;
	}
	if(type2==X86REG)
	{
		if(len1 && len1!=len2) {incompat();return;}
		ea1->x86olen=len2;
		x86outea(ea1,len2==1 ? 0x88 : 0x89,-1,reg2&7);
		return;
	} else badmode();
}
void x86doubleshifts(uchar op) {
	uchar type1,type2,type3;
	uchar reg2;

	type1=x86ea(ea1);
	if(x86comma()) return;
	type2=x86ea(ea2);
	if(x86comma()) return;
	type3=x86ea(ea3);
	if(type2!=X86REG || (reg2=ea2->x86reg)>=16) {error2("Second parameter must be R16/R32");return;}
	if(type3!=X86IMMED)
		if(type3!=X86REG || ea3->x86reg!=17)
			{error2("Third parameter must be immediate or cl");return;}
	if(type1==X86IMMED) {badmode();return;}
	if(ea1->x86olen && ea1->x86olen!=ea2->x86olen) {incompat();return;}
	ea1->x86olen=ea2->x86olen;
	x86outea(ea1,0x0f,type3==X86IMMED ? op : op+1,reg2&7);
	if(type3==X86IMMED)
	{
		ea3->x86olen=1;
		x86outimm(ea3);
	}
}
void x86shrd(void) {x86doubleshifts(0xac);}
void x86shld(void) {x86doubleshifts(0xa4);}

void x86calljmp(uchar op) {
	uchar type,modrm,alen;
	long val;

	type=x86ea(ea1);
	if(type==X86IMMED || (type==X86REG && ea1->x86reg>=16)) {badmode();return;}
	modrm=ea1->x86modrm;
	if(op && ea1->x86flags&X86SHORT)
	{
		if((mode32 && modrm!=5) || (!mode32 && modrm!=6)) {badmode();return;}
		bout(0xeb);
		val=ea1->x86value-pcount-1;
		if(val<-0x80 || val>0x7f) outofrange();
		bout((int)val);
		return;
	}
	if(!ea1->x86olen && ~ea1->x86flags&X86BRACES)
	{
		if((mode32 && modrm==5) || (!mode32 && modrm==6))
		{
			alen=ea1->x86alen;
			ea1->x86olen=alen;
			bout(0xe8+op);
			ea1->x86value-=pcount+alen;
			ea1->x86flags|=X86REL;
			x86outimm(ea1);
		} else
			needsize();
		return;
	}
/*	ea1->x86flags|=X86PREVENT;*/
	x86outea(ea1,0xff,-1,2<<op); /*ea1->x86olen==2 ? (2<<op) : (2<<op)+1);*/
}
void x86call(void) {x86calljmp(0);}
void x86jmp(void) {x86calljmp(1);}

void x86fcalljmp(uchar field) {
	uchar type;
	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
/*	if(mode32) ea1->x86flags|=X86PREVENT;
	else ea1->x86flags|=X86FORCE;*/
	x86outea(ea1,0xff,-1,field);
}
void x86fjmp(void) {x86fcalljmp(5);}
void x86fcall(void) {x86fcalljmp(3);}

void x86jumps(uchar op) {
	uchar type,alen,modrm;
	long val;

	type=x86ea(ea1);
	alen=ea1->x86alen;
	if(type==X86IMMED || type==X86REG || !alen || (alen==2 && mode32) ||
			(alen==4 && !mode32))
		 {badmode();return;}
	modrm=ea1->x86modrm;
	if((mode32 && modrm!=5) || (!mode32 && modrm!=6)) {badmode();return;}
	if(op!=0xe3 && ~ea1->x86flags&X86SHORT)
	{
/*
		val=ea1->x86value-pcount-2;
		if(val<0 && val>=-0x80)
		{
			bout(op);
			bout((int)val);
			return;
		}
*/
		bout(0x0f);
		bout(op+0x10);
		ea1->x86olen=alen;
		ea1->x86value-=pcount+alen;
		ea1->x86flags|=X86REL;
		x86outimm(ea1);
	} else
	{
		bout(op);
		val=ea1->x86value-pcount-1;
		if(val<-0x80 || val>0x7f) outofrange();
		bout((int)val);
	}
}
void x86ja(void) {x86jumps(0x77);}
void x86jae(void) {x86jumps(0x73);}
void x86jb(void) {x86jumps(0x72);}
void x86jbe(void) {x86jumps(0x76);}
void x86jc(void) {x86jumps(0x72);}
void x86jcxz(void) {x86jumps(0xe3);}
void x86jecxz(void) {x86jumps(0xe3);}
void x86je(void) {x86jumps(0x74);}
void x86jg(void) {x86jumps(0x7f);}
void x86jge(void) {x86jumps(0x7d);}
void x86jl(void) {x86jumps(0x7c);}
void x86jle(void) {x86jumps(0x7e);}
void x86jna(void) {x86jumps(0x76);}
void x86jnae(void) {x86jumps(0x72);}
void x86jnb(void) {x86jumps(0x73);}
void x86jnbe(void) {x86jumps(0x77);}
void x86jnc(void) {x86jumps(0x73);}
void x86jne(void) {x86jumps(0x75);}
void x86jng(void) {x86jumps(0x7e);}
void x86jnge(void) {x86jumps(0x7c);}
void x86jnl(void) {x86jumps(0x7d);}
void x86jnle(void) {x86jumps(0x7f);}
void x86jno(void) {x86jumps(0x71);}
void x86jnp(void) {x86jumps(0x7b);}
void x86jns(void) {x86jumps(0x79);}
void x86jnz(void) {x86jumps(0x75);}
void x86jo(void) {x86jumps(0x70);}
void x86jp(void) {x86jumps(0x7a);}
void x86jpe(void) {x86jumps(0x7a);}
void x86jpo(void) {x86jumps(0x7b);}
void x86js(void) {x86jumps(0x78);}
void x86jz(void) {x86jumps(0x74);}

void x86loops(uchar op) {
	struct sym *asym;

	rexpr();
	if(exprtype==RELTYPE)
	{
		asym=(void *)exprval;
		if(pass && asym->symflags & APUBLIC && ~asym->symflags & ADEF)
			{badmode();return;}
		exprval=asym->symvalue+asym->symdelta;
	}
	bout(op);
	exprval-=pcount+1;
	if(exprval<-0x80 || exprval>0x7f)
		outofrange();
	bout((int)exprval);
}
void x86loopne(void) {x86loops(0xe0);}
void x86loopnz(void) {x86loops(0xe0);}
void x86loope(void) {x86loops(0xe1);}
void x86loopz(void) {x86loops(0xe1);}
void x86loop(void) {x86loops(0xe2);}

void x86lar(void) {
	uchar type,reg,olen;

	type=x86ea(ea1);
	if(x86comma()) return;
	if(type!=X86REG) {badmode();return;}
	reg=ea1->x86reg;
	olen=ea1->x86olen;
	if(reg>=16) {badreg();return;}
	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
	if(ea1->x86olen && ea1->x86olen!=olen) {incompat();return;}
	ea1->x86olen=olen;
	x86outea(ea1,0x0f,0x02,reg&7);
}

void x86in(void) {
	uchar reg1;
	uchar reg2;

	reg1=x86reg();
	if(reg1==255) {syntaxerr();return;}
	if(reg1>=24 || reg1&7) {badreg();return;}
	if(x86comma()) return;
	reg2=x86reg();
	if(reg2==2)
	{
		if(reg1==16) bout(0xec);
		else {x86opover(reg1==8);bout(0xed);}
		return;
	}
	if(reg2!=255) {badreg();return;}
	expr();
	if(exprtype!=ABSTYPE) {badmode();return;}
	if(exprval<0 || exprval>255) outofrange();
	if(reg1==16) bout(0xe4);
	else {x86opover(reg1==8);bout(0xe5);}
	bout((int)exprval);
}

void x86out(void) {
	uchar reg1;
	uchar reg2;

	reg1=x86reg();
	if(reg1==2)
	{
		if(x86comma()) return;
		reg2=x86reg();
		if(reg2==255) {syntaxerr();return;}
		if(reg2>=24 || reg2&7) {badreg();return;}
		if(reg2==16) bout(0xee);
		else {x86opover(reg2==8);bout(0xef);}
		return;
	}
	expr();
	if(exprtype!=ABSTYPE) {badmode();return;}
	if(exprval<0 || exprval>255) outofrange();
	if(x86comma()) return;
	reg2=x86reg();
	if(reg2==255) {syntaxerr();return;}
	if(reg2>=24 || reg2&7) {badreg();return;}
	if(reg2==16) bout(0xe6);
	else {x86opover(reg2==8);bout(0xe7);}
	bout((int)exprval);
}

void x86loadfulls(uchar op) {
	uchar reg,type,olen;
	reg=x86reg();
	if(reg==255) {badmode();return;}
	if(reg>=16) {badreg();return;}
	if(x86comma()) return;
	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
	olen=reg>=8 ? 4 : 2;
	if(ea1->x86olen && ea1->x86olen!=olen) {incompat();return;}
	ea1->x86olen=olen;
	x86outea(ea1,op<0xc4 ? 0x0f : op, op<0xc4 ? op : -1,reg&7);
}
void x86lds(void) {x86loadfulls(0xc5);}
void x86les(void) {x86loadfulls(0xc4);}
void x86lfs(void) {x86loadfulls(0xb4);}
void x86lgs(void) {x86loadfulls(0xb5);}
void x86lss(void) {x86loadfulls(0xb2);}

void x86dt(uchar field) {
	uchar type;
	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
	ea1->x86flags|=X86PREVENT;
	x86outea(ea1,0x0f,0x01,field);
}
void x86sgdt(void) {x86dt(0);}
void x86sidt(void) {x86dt(1);}
void x86lidt(void) {x86dt(3);}
void x86lgdt(void) {x86dt(2);}

void x86allsets(uchar op) {
	uchar type;
	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
	if(ea1->x86olen && ea1->x86olen!=1) {needbyte();return;}
	ea1->x86olen=1;
	x86outea(ea1,0x0f,op,0);
}
void x86seta(void) {x86allsets(0x97);}
void x86setae(void) {x86allsets(0x93);}
void x86setb(void) {x86allsets(0x92);}
void x86setbe(void) {x86allsets(0x96);}
void x86setc(void) {x86allsets(0x92);}
void x86setcxz(void) {x86allsets(0xe3);}
void x86setecxz(void) {x86allsets(0xe3);}
void x86sete(void) {x86allsets(0x94);}
void x86setg(void) {x86allsets(0x9f);}
void x86setge(void) {x86allsets(0x9d);}
void x86setl(void) {x86allsets(0x9c);}
void x86setle(void) {x86allsets(0x9e);}
void x86setna(void) {x86allsets(0x96);}
void x86setnae(void) {x86allsets(0x92);}
void x86setnb(void) {x86allsets(0x93);}
void x86setnbe(void) {x86allsets(0x97);}
void x86setnc(void) {x86allsets(0x93);}
void x86setne(void) {x86allsets(0x95);}
void x86setng(void) {x86allsets(0x9e);}
void x86setnge(void) {x86allsets(0x9c);}
void x86setnl(void) {x86allsets(0x9d);}
void x86setnle(void) {x86allsets(0x9f);}
void x86setno(void) {x86allsets(0x91);}
void x86setnp(void) {x86allsets(0x9b);}
void x86setns(void) {x86allsets(0x99);}
void x86setnz(void) {x86allsets(0x95);}
void x86seto(void) {x86allsets(0x90);}
void x86setp(void) {x86allsets(0x9a);}
void x86setpe(void) {x86allsets(0x9a);}
void x86setpo(void) {x86allsets(0x9b);}
void x86sets(void) {x86allsets(0x98);}
void x86setz(void) {x86allsets(0x94);}

void x86dt2(uchar op,uchar field) {
	uchar type;
	type=x86ea(ea1);
	if(type==X86IMMED) {badmode();return;}
	if(ea1->x86olen && ea1->x86olen!=2) {needword();return;}
	x86outea(ea1,0x0f,op,field);
}
void x86sldt(void) {x86dt2(0,0);}
void x86lldt(void) {x86dt2(0,2);}
void x86smsw(void) {x86dt2(1,4);}
void x86lmsw(void) {x86dt2(1,6);}

void x86bits(uchar op, uchar field) {
	uchar type1;
	uchar type2,reg2,olen2;

	type1=x86ea(ea1);
	if(type1==X86IMMED) {badmode();return;}
	if(x86comma()) return;
	type2=x86ea(ea2);
	if(type2!=X86REG && type2!=X86IMMED) {badmode();return;}
	olen2=ea2->x86olen;
	if(type2==X86REG)
	{
		reg2=ea2->x86reg;
		if(ea1->x86olen && ea1->x86olen!=olen2) {incompat();return;}
		if(reg2>=16) {badreg();return;}
		x86outea(ea1,0x0f,op,0);
	} else
	{
		if(olen2 && olen2!=1) {needbyte();return;}
		if(ea1->x86olen<2) {badmode();return;}
		x86outea(ea1,0x0f,0xba,field);
		bout((int)ea2->x86value);
	}
}
void x86bt(void) {x86bits(0xa3,4);}
void x86bts(void) {x86bits(0xab,5);}
void x86btr(void) {x86bits(0xb3,6);}
void x86btc(void) {x86bits(0xbb,7);}


struct anopcode x86codes[]={
{"aaa",x86aaa},
{"aas",x86aas},
{"adc",x86adc},
{"add",x86add},
{"and",x86and},
{"bt",x86bt},
{"btc",x86btc},
{"btr",x86btr},
{"bts",x86bts},
{"call",x86call},
{"cbw",x86cbw},
{"cdq",x86cdq},
{"clc",x86clc},
{"cld",x86cld},
{"cli",x86cli},
{"cmc",x86cmc},
{"cmp",x86cmp},
{"cmps",x86cmps},
{"cmpsb",x86cmpsb},
{"cmpsd",x86cmpsd},
{"cmpsw",x86cmpsw},
{"cwd",x86cwd},
{"cwde",x86cwde},
{"das",x86das},
{"db",dodcb},
{"dec",x86dec},
{"div",x86div},
{"enter",x86enter},
{"fcall",x86fcall},
{"fjmp",x86fjmp},
{"hlt",x86hlt},
{"idiv",x86idiv},
{"imul",x86imul},
{"in",x86in},
{"inc",x86inc},
{"int",x86int},
{"into",x86into},
{"iret",x86iret},
{"iretd",x86iretd},
{"ja",x86ja},
{"jae",x86jae},
{"jb",x86jb},
{"jbe",x86jbe},
{"jc",x86jc},
{"jcxz",x86jcxz},
{"je",x86je},
{"jecxz",x86jecxz},
{"jg",x86jg},
{"jge",x86jge},
{"jl",x86jl},
{"jle",x86jle},
{"jmp",x86jmp},
{"jna",x86jna},
{"jnae",x86jnae},
{"jnb",x86jnb},
{"jnbe",x86jnbe},
{"jnc",x86jnc},
{"jne",x86jne},
{"jng",x86jng},
{"jnge",x86jnge},
{"jnl",x86jnl},
{"jnle",x86jnle},
{"jno",x86jno},
{"jnp",x86jnp},
{"jns",x86jns},
{"jnz",x86jnz},
{"jo",x86jo},
{"jp",x86jp},
{"jpe",x86jpe},
{"jpo",x86jpo},
{"js",x86js},
{"jz",x86jz},
{"lahf",x86lahf},
{"lar",x86lar},
{"lds",x86lds},
{"lea",x86lea},
{"leave",x86leave},
{"les",x86les},
{"lfs",x86lfs},
{"lgdt",x86lgdt},
{"lgs",x86lgs},
{"lidt",x86lidt},
{"lldt",x86lldt},
{"lmsw",x86lmsw},
{"lock",x86lock},
{"lods",x86lods},
{"lodsb",x86lodsb},
{"lodsd",x86lodsd},
{"lodsw",x86lodsw},
{"loop",x86loop},
{"loope",x86loope},
{"loopne",x86loopne},
{"loopnz",x86loopnz},
{"loopz",x86loopnz},
{"lss",x86lss},
{"mov",x86mov},
{"movs",x86movs},
{"movsb",x86movsb},
{"movsd",x86movsd},
{"movsw",x86movsw},
{"movsx",x86movsx},
{"movzx",x86movzx},
{"mul",x86mul},
{"neg",x86neg},
{"nop",x86nop},
{"not",x86not},
{"or",x86or},
{"out",x86out},
{"pop",x86pop},
{"popa",x86popa},
{"popad",x86popad},
{"popf",x86popf},
{"popfd",x86popfd},
{"push",x86push},
{"pusha",x86pusha},
{"pushad",x86pushad},
{"pushf",x86pushf},
{"pushfd",x86pushfd},
{"rcl",x86rcl},
{"rcr",x86rcr},
{"rep",x86rep},
{"repe",x86repe},
{"repne",x86repne},
{"repnz",x86repnz},
{"repz",x86repz},
{"ret",x86ret},
{"retf",x86retf},
{"rol",x86rol},
{"ror",x86ror},
{"sahf",x86sahf},
{"sal",x86sal},
{"sar",x86sar},
{"sbb",x86sbb},
{"scas",x86scas},
{"scasb",x86scasb},
{"scasd",x86scasd},
{"scasw",x86scasw},
{"seta",x86seta},
{"setae",x86setae},
{"setb",x86setb},
{"setbe",x86setbe},
{"setc",x86setc},
{"setcxz",x86setcxz},
{"sete",x86sete},
{"setecxz",x86setecxz},
{"setg",x86setg},
{"setge",x86setge},
{"setl",x86setl},
{"setle",x86setle},
{"setna",x86setna},
{"setnae",x86setnae},
{"setnb",x86setnb},
{"setnbe",x86setnbe},
{"setnc",x86setnc},
{"setne",x86setne},
{"setng",x86setng},
{"setnge",x86setnge},
{"setnl",x86setnl},
{"setnle",x86setnle},
{"setno",x86setno},
{"setnp",x86setnp},
{"setns",x86setns},
{"setnz",x86setnz},
{"seto",x86seto},
{"setp",x86setp},
{"setpe",x86setpe},
{"setpo",x86setpo},
{"sets",x86sets},
{"setz",x86setz},
{"sgdt",x86sgdt},
{"shl",x86shl},
{"shld",x86shld},
{"shr",x86shr},
{"shrd",x86shrd},
{"sidt",x86sidt},
{"sldt",x86sldt},
{"smsw",x86smsw},
{"stc",x86stc},
{"std",x86std},
{"sti",x86sti},
{"stos",x86stos},
{"stosb",x86stosb},
{"stosd",x86stosd},
{"stosw",x86stosw},
{"sub",x86sub},
{"test",x86test},
{"use16",x86use16},
{"use32",x86use32},
{"wait",x86wait},
{"xchg",x86xchg},
{"xlat",x86xlat},
{"xor",x86xor},
{0}};
