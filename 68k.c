#include "asm.h"


long bits[16]={0x10000L,0x20000L,0x40000L,0x80000L,
	0x100000L,0x200000L,0x400000L,0x800000L,
	0x1000000L,0x2000000L,0x4000000L,0x8000000L,
	0x10000000L,0x20000000L,0x40000000L,0x80000000L};

struct sym *lastxref;
int eaword1; /* effectaddr fills in */
int eaword2;
int ealen;
int eabits;
int eaop;
int earel;

struct sym *lastxref2;
int eaword12;
int eaword22;
int ealen2;
int eabits2;
int eaop2;
int earel2;

void xchgea(void) {
	struct sym *tempsym;
	int temp;
	tempsym=lastxref;lastxref=lastxref2;lastxref2=tempsym;
	temp=eaword1;eaword1=eaword12;eaword12=temp;
	temp=eaword2;eaword2=eaword22;eaword22=temp;
	temp=ealen;ealen=ealen2;ealen2=temp;
	temp=eabits;eabits=eabits2;eabits2=temp;
	temp=eaop;eaop=eaop2;eaop2=temp;
	temp=earel;earel=earel2;earel2=temp;
}

long getreg(void) {
	char ch;
	long val;
	char *insave;

	insave=inpoint;
	ch=get();
	if(ch=='d' || ch=='D')
	{
		ch=get();
		if(ch>='0' && ch<'8' && !isoksym(at()))
			{val=ch-'0';return val+bits[val];}
	} else if(ch=='a' || ch=='A')
	{
		ch=get();
		if(ch>='0' && ch<'8' && !isoksym(at()))
			{val=ch-'0'+8;return val+bits[val];}
	} else if(ch=='s' || ch=='S')
	{
		ch=get();;
		if((ch=='p' || ch=='P') && !isoksym(at()))
			return 0x8000000fL;
		else if((ch=='r' || ch=='R') && !isoksym(at()))
			return 0x3cL;
	} else if(ch=='c' || ch=='C')
		{if(tolower(get())=='c' && tolower(get())=='r' && !isoksym(at()))
			return 88L;}
	else if(ch=='u' || ch=='U')
		{if(tolower(get())=='s' && tolower(get())=='p' && !isoksym(at()))
			return 99L;}
	else if(ch=='p' || ch=='P')
		{if(tolower(get())=='c' && !isoksym(at()))
			return 16L;}
	inpoint=insave;
	return 0L;
}

int addressreg(void) {
	int treg;
	treg=getreg()&0xffff;
	if(treg<8 || treg>15) badreg();
	return treg-8;
}
int datareg(void) {
	int treg;
	long val;
	treg=(val=getreg())&0xffff;
	if(!val || treg>7) badreg();
	return treg;
}
int anyreg(void) {
	int treg;
	long val;
	treg=(val=getreg())&0xffff;
	if(!val || treg>15) badreg();
	return treg;
}

long regexpr(long val) {
	char ch1,ch2;
	long reg0,reg1,reg2;

	ch2=0;
	reg1=val;
	for(;;)
	{
		ch1=at();
		if(ch1=='-')
		{
			forward1();
			reg2=getreg();
			if(!reg2) badreg();
			reg1=((reg2&0xffff0000L)<<1)-(reg1&0xffff0000L);
			continue;
		}
		if(ch2)
		{
			reg1=reg1 | (reg0 & 0xffff0000L);
			ch2=0;
		}
		if(ch1=='/')
		{
			ch2=get();
			reg0=reg1;
			reg1=getreg();
			if(!reg1) badreg();
			continue;
		}
		break;
	}
	return reg1;
}

void emitxref16(void) {if(pass) addref(lastxref,0x83);}
void emitxref32(void) {if(pass) addref(lastxref,0x81);}
void emitxrel16(void) {if(pass) addref(lastxref,0x8b);}
void emitreloc32(void) {if(pass) addreloc(4);}

void emitea(void) {
	if(!ealen) return;
	if(eabits==0x200 && earel!=2) /* d16(pc) */
	{
		eaword1-=pcount;
	} else if(eabits==0x400) /* d8(pc,xn) */
	{
		eaword1=(eaword1&0xff00)+(((eaword1&0xff)-pcount)&0xff);
	}
	--ealen;
	if(earel==2)
	{
		if(eabits==0x80 || (eabits==0x800 && !ealen)) /* abs.w, #d16 */
			emitxref16();
		else if(eabits==0x100 || (eabits==0x800 && ealen)) /* abs.l, #d32 */
			emitxref32();
		else if(eabits==0x200) /* addr(pc) */
			emitxrel16();
	} else if(earel==1)
	{
		if(ealen) /* abs.l,#d32 */
			emitreloc32();
		else
			if(eabits==0x800 || eabits==0x80)
				outofrange(); /* temporary */
	}

	wout(eaword1);
	if(!ealen) return;
	wout(eaword2);
}

void index68k(void) {
char ch;
int reg;

	reg=getreg();
	reg&=15;
	reg<<=12;
	ch=get();
	if(ch=='.')
	{
		ch=get();
		if(ch=='l' || ch=='L')
		reg+=0x800;
		forward1();
	}
	eaword1&=0xff;
	eaword1+=reg;
}

/* eabits:
dn		=0001
an		=0002
(an)+	=0004
(a0)		=0008
-(an)	=0010
d16(an)	=0020
d8(an,xn)	=0040
d16.w	=0080
d32.l	=0100
d16(pc)	=0200
d8(pc,xn)	=0400
#d32,d16	=0800
sr		=1000
ccr		=2000
usp		=4000
pc		=8000
*/
void effectaddr(void) {
	char ch;
	long reg;

	eaop=0;
	ch=get();
	if(ch=='#')
	{
		expr();
		eaword1=exprval>>16;
		eaword2=exprval;
		eaop=0x3c;
		earel=0;
		if(exprtype==RELTYPE)
		{
			earel=1;
			if(lastrel->symflags & APUBLIC && ~lastrel->symflags & ADEF)
			{
				lastxref=lastrel;
				earel=2;
			}
		}
		eabits=0x800;
		ealen=1;
		if(variant==3) ealen++; else eaword1=eaword2;
	} else if(ch=='(')
	{
		reg=getreg();
		if(at()==',') {exprval=eaword1=0;exprtype=ABSTYPE;goto forceindex;}
		if(!reg) goto eaback;
		reg&=0xffff;
		if(reg<8 || reg>15) badreg();
		eaop=(reg&7)+0x10;
		if(get()!=')') {syntaxerr();return;}
		ealen=0;
		earel=0;
		if(at()=='+')
		{
			forward1();
			eaop+=8;
			eabits=4;
		}
		else
			eabits=8;
	} else if(ch=='-')
	{
		if(at()=='(')
		{
			forward1();
			reg=getreg();
			if(!reg)
			{
				back1();
				goto eaback;
			}
			reg&=0xffff;
			if(reg<8 || reg>15) badreg();
			eaop=(reg&7)+0x20;
			if(get()!=')') {syntaxerr();return;}
			ealen=0;
			earel=0;
			eabits=0x10;
		}
		else goto eaback;
	} else
	{
eaback:
		back1();
		if(isokfirst(at()))
		{
			reg=getreg();
			if(reg)
			{
				reg=regexpr(reg);
				ealen=0;
				earel=0;
				eabits=1;
				eaop=reg&0xffff;
				eaword1=reg>>16;
				if(eaop>=8)
				{
					if(eaop<16)
						eabits=2;
					else if(eaop==0x3c)
						eabits=0x1000;
					else if(eaop==88)
						{eabits=0x2000;eaop=0x3c;}
					else if(eaop==99)
						eabits=0x4000;
					else eabits=0x8000;
				}
				return;
			}
		}
		expr();
		earel=0;
		if(exprtype==RELTYPE)
		{
			earel=1;
			if(lastrel->symflags & APUBLIC && ~lastrel->symflags & ADEF)
			{
				lastxref=lastrel;
				earel=2;
			}
		}
		ch=at();
		if(ch=='.')
		{
			forward1();
			ch=get();
			if(ch=='w' || ch=='W')
			{
				eaword1=exprval;
				if(exprval<-0x8000L || exprval>0x7fffL) outofrange();
				eaop=0x38;
				eabits=0x80;
				ealen=1;
			} else /* if(ch=='l' || ch=='L') */
			{
longsize:
				eaword1=exprval>>16L;
				eaword2=exprval;
				eaop=0x39;
				eabits=0x100;
				ealen=2;
			}
		} else if(ch=='(')
		{
			forward1();
			eaword1=exprval;
			reg=getreg();
forceindex:
			if(reg==0x10)
			{
				/* earel=1; */
				if(exprtype==ABSTYPE) eaword1+=pcount;
				ch=get();
				if(ch==')')
				{
					eaop=0x3a;
					ealen=1;
					eabits=0x200;
				} else /* if(ch==',') */
				{
					index68k();
					eaop=0x3b;ealen=1;eabits=0x400;
				}
			} else
			{
				ch=get();
				if(ch==',')
				{
					eaop=reg+0x28;
					index68k();
					ealen=1;eabits=0x40;
				} else /* if(ch==')') */
				{
					eaop=reg+0x20;ealen=1;eabits=0x20;
				}
			}
		} else goto longsize;
	}
}
void absexpr(void) {
	expr();
	if(exprtype!=ABSTYPE) absreq();
}
void singleea(int op, int invalids) {
	effectaddr();
	if(eabits & invalids) badmode();
	wout(eaop+op);
	emitea();
}


void allmoves(int op) {
	long seaword1,seaword2,sealen,seabits,seaop,searel,slastxref;
	long deaword1,deaword2,dealen,deabits,deaop,dearel,dlastxref;

	effectaddr();
	seaword1=eaword1;seaword2=eaword2;
	sealen=ealen;seabits=eabits;seaop=eaop;searel=earel;slastxref=(long)lastxref;

	comma();

	effectaddr();
	deaword1=eaword1;deaword2=eaword2;
	dealen=ealen;deabits=eabits;deaop=eaop;dearel=earel;dlastxref=(long)lastxref;

	if((seabits|deabits) & 0x7000)
	{
		if(seabits & 0x3000) /* sr, ccr, */
		{
			if(deabits&0xfe02) badmode();
			if(seabits==0x2000) /* ccr, */
			{
				wout(0x42c0+deaop);
				emitea();
			} else /* sr, */
			{
				wout(0x40c0+deaop);
				emitea();
			}
		} else if(seabits==0x4000) /* usp, */
		{
			if(deabits!=2) badmode();
			wout(deaop|0x4e68);
		} else if(deabits & 0x3000) /* ,sr ,ccr */
		{
			if(seabits&0xf002) badmode();
			eaword1=seaword1;eaword2=seaword2;
			ealen=sealen;eabits=seabits;eaop=seaop;earel=searel;
			lastxref=(void *)slastxref;

			if(deabits==0x2000) /* ,ccr */
				{wout(eaop | 0x44c0);emitea();}
			else
				{wout(eaop | 0x46c0);emitea();}
		} else /* ,usp */
		{
			if(seabits!=2) badmode();
			wout((seaop-8) | 0x4e60);
		}
	} else
	{
		op+=seaop+((deaop&7)<<9)+((deaop&0x38)<<3);
		wout(op);
		eaword1=seaword1;eaword2=seaword2;
		ealen=sealen;eabits=seabits;eaop=seaop;earel=searel;
		lastxref=(void *)slastxref;
		emitea();
		eaword1=deaword1;eaword2=deaword2;
		ealen=dealen;eabits=deabits;eaop=deaop;earel=dearel;
		lastxref=(void *)dlastxref;
		emitea();
		if(deabits&0x200) badmode();
	}
}

void domovew(void){allmoves(0x3000);}
void domovel(void){variant=3;allmoves(0x2000);}
void domoveb(void){allmoves(0x1000);}


void addsubq(int op) {
	int val;
	if(numsign()) return;
	absexpr();
	val=exprval;
	if(val==0 || val>8) outofrange();
	val&=7;
	val<<=9;
	if(comma()) return;
	singleea(val+op,0xfe00);
}
void dosubql(void) {addsubq(0x5180);}
void dosubqw(void) {addsubq(0x5140);}
void dosubqb(void) {addsubq(0x5100);}
void doaddql(void) {addsubq(0x5080);}
void doaddqw(void) {addsubq(0x5040);}
void doaddqb(void) {addsubq(0x5000);}

void dojmp(void){singleea(0x4ec0,0xf817);}
void dojsr(void){singleea(0x4e80,0xf817);}
void dostr(void){singleea(0x50c0,0xfe02);}
void dosfa(void){singleea(0x51c0,0xfe02);}
void doshi(void){singleea(0x52c0,0xfe02);}
void dosls(void){singleea(0x53c0,0xfe02);}
void doscc(void){singleea(0x54c0,0xfe02);}
void doscs(void){singleea(0x55c0,0xfe02);}
void dosne(void){singleea(0x56c0,0xfe02);}
void doseq(void){singleea(0x57c0,0xfe02);}
void dosvc(void){singleea(0x58c0,0xfe02);}
void dosvs(void){singleea(0x59c0,0xfe02);}
void dospl(void){singleea(0x5ac0,0xfe02);}
void dosmi(void){singleea(0x5bc0,0xfe02);}
void dosge(void){singleea(0x5cc0,0xfe02);}
void doslt(void){singleea(0x5dc0,0xfe02);}
void dosgt(void){singleea(0x5ec0,0xfe02);}
void dosle(void){singleea(0x5fc0,0xfe02);}
void dotas(void){singleea(0x4ac0,0xfe02);}
void donbcd(void){singleea(0x4800,0xfe02);}
void dopea(void){singleea(0x4840,0xf817);}

void branches(int op) {
	long off;
	expr();
	wout(op);
	if(exprtype==RELTYPE)
	{
		if(lastrel->symflags & APUBLIC)
		{
			if(lastrel->symflags & ADEF)
				off=exprval-pcount;
			else
			{
				off=exprval;
				lastxref=lastrel;
				emitxrel16();
			}
		} else
			off=exprval-pcount;
	} else
		off=exprval;
	wout((int)off);
	if(off<-0x8000 || off>0x7fff) outofrange();
}
void dobra(void){branches(0x6000);}
void dobsr(void){branches(0x6100);}
void dobhi(void){branches(0x6200);}
void dobls(void){branches(0x6300);}
void dobcc(void){branches(0x6400);}
void dobcs(void){branches(0x6500);}
void dobne(void){branches(0x6600);}
void dobeq(void){branches(0x6700);}
void dobvc(void){branches(0x6800);}
void dobvs(void){branches(0x6900);}
void dobpl(void){branches(0x6a00);}
void dobmi(void){branches(0x6b00);}
void dobge(void){branches(0x6c00);}
void doblt(void){branches(0x6d00);}
void dobgt(void){branches(0x6e00);}
void doble(void){branches(0x6f00);}

void shortbranches(int op) {
	int off;
	expr();
	off=exprval-pcount-2;
	wout(op | (off&0xff));
	if(off<-0x80 || off>0x7f || off==0) outofrange();
}
void dobras(void){shortbranches(0x6000);}
void dobsrs(void){shortbranches(0x6100);}
void dobhis(void){shortbranches(0x6200);}
void doblss(void){shortbranches(0x6300);}
void dobccs(void){shortbranches(0x6400);}
void dobcss(void){shortbranches(0x6500);}
void dobnes(void){shortbranches(0x6600);}
void dobeqs(void){shortbranches(0x6700);}
void dobvcs(void){shortbranches(0x6800);}
void dobvss(void){shortbranches(0x6900);}
void dobpls(void){shortbranches(0x6a00);}
void dobmis(void){shortbranches(0x6b00);}
void dobges(void){shortbranches(0x6c00);}
void doblts(void){shortbranches(0x6d00);}
void dobgts(void){shortbranches(0x6e00);}
void dobles(void){shortbranches(0x6f00);}

void dbranches(int op) {
	long off;
	int reg;

	reg=getreg()&0xffff;
	if(reg>7) badreg();
	if(comma()) return;
	wout(op+reg);
	expr();
	off=exprval-pcount;
	if(off<-0x8000 || off>0x7fff) outofrange();
	wout((int)off);
}
void dodbtr(void){dbranches(0x50c8);}
void dodbra(void){dbranches(0x51c8);}
void dodbhi(void){dbranches(0x52c8);}
void dodbls(void){dbranches(0x53c8);}
void dodbcc(void){dbranches(0x54c8);}
void dodbcs(void){dbranches(0x55c8);}
void dodbne(void){dbranches(0x56c8);}
void dodbeq(void){dbranches(0x57c8);}
void dodbvc(void){dbranches(0x58c8);}
void dodbvs(void){dbranches(0x59c8);}
void dodbpl(void){dbranches(0x5ac8);}
void dodbmi(void){dbranches(0x5bc8);}
void dodbge(void){dbranches(0x5cc8);}
void dodblt(void){dbranches(0x5dc8);}
void dodbgt(void){dbranches(0x5ec8);}
void dodble(void){dbranches(0x5fc8);}

void dolink(void) {
	int op;
	op=addressreg();
	if(comma()) return;
	if(numsign()) return;
	op|=0x4e50;
	expr();
	if(exprtype!=ABSTYPE) absreq();
	if(exprval<-0x8000 || exprval >0x7fff) outofrange();
	wout(op);
	wout((int)exprval);
}
void dounlk(void)
{
	int op;
	op=addressreg();
	wout(op|0x4e58);
}

void bcd(int op) {
	int sr,dr;

	if(at()=='-')
	{
		forward1();
		if(expect('(')) return;
		sr=addressreg();
		if(expects("),-(")) return;
		dr=addressreg();
		op|=8;
	} else
	{
		sr=datareg();
		if(comma()) return;
		dr=datareg();
	}
	wout(op | (dr<<9) | sr);
}

void doaddxl(void){bcd(0xd180);}
void doaddxw(void){bcd(0xd140);}
void doaddxb(void){bcd(0xd100);}
void dosubxl(void){bcd(0x9180);}
void dosubxw(void){bcd(0x9140);}
void dosubxb(void){bcd(0x9100);}
void doabcd(void){bcd(0xc100);}
void dosbcd(void){bcd(0x8100);}

void immeds(int op, int op2) {
/*
	if(numsign()) return;
	expr();
	tval=exprval;
*/
	effectaddr();
	if(eabits!=0x800) {badmode();return;}

	if(comma()) return;
	xchgea();

	effectaddr();
	if(eabits&(op2 ? 0x0e00 :0x0e02)) badmode();
	if(op2 && (eabits&2))
	{
		op=op2;
		eaop&=7;
		eaop<<=9;
	}

	wout(op | eaop);
	xchgea();
	emitea();
	xchgea();
/*	if(variant<3) wout((int)tval); else lout(tval);*/
	emitea();
}

void doorib(void){immeds(0x0000,0);}
void dooriw(void){immeds(0x0040,0);}
void dooril(void){variant=3;immeds(0x0080,0);}

void doandib(void){immeds(0x0200,0);}
void doandiw(void){immeds(0x0240,0);}
void doandil(void){variant=3;immeds(0x0280,0);}

void dosubib(void){immeds(0x0400,0);}
void dosubiw(void){immeds(0x0440,0x90fc);}
void dosubil(void){variant=3;immeds(0x0480,0x91fc);}

void doaddib(void){immeds(0x0600,0);}
void doaddiw(void){immeds(0x0640,0xd0fc);}
void doaddil(void){variant=3;immeds(0x0680,0xd1fc);}

void doeorib(void){immeds(0x0a00,0);}
void doeoriw(void){immeds(0x0a40,0);}
void doeoril(void){variant=3;immeds(0x0a80,0);}

void docmpib(void){immeds(0x0c00,0);}
void docmpiw(void){immeds(0x0c40,0xb0fc);}
void docmpil(void){variant=3;immeds(0x0c80,0xb1fc);}

void doclrb(void){singleea(0x4200,0xfe02);}
void doclrw(void){singleea(0x4240,0xfe02);}
void doclrl(void){variant=3;singleea(0x4280,0xfe02);}

void donotb(void){singleea(0x4600,0xfe02);}
void donotw(void){singleea(0x4640,0xfe02);}
void donotl(void){variant=3;singleea(0x4680,0xfe02);}

void dotstb(void){singleea(0x4a00,0xfe02);}
void dotstw(void){singleea(0x4a40,0xfe02);}
void dotstl(void){variant=3;singleea(0x4a80,0xfe02);}

void donegb(void){singleea(0x4400,0xfe02);}
void donegw(void){singleea(0x4440,0xfe02);}
void donegl(void){variant=3;singleea(0x4480,0xfe02);}

void donegxb(void){singleea(0x4a00,0xfe02);}
void donegxw(void){singleea(0x4a40,0xfe02);}
void donegxl(void){variant=3;singleea(0x4a80,0xfe02);}

void dolea(void) {
	int reg;

	effectaddr();
	if(eabits & 0xf81b) badmode();
	if(comma()) return;
	reg=addressreg();
	wout(0x41c0 | eaop | (reg<<9));
	emitea();
}

void arrith(int op, int op2) {
	int reg;

	effectaddr();
	if(comma()) return;
	if(eabits==1)
	{
		reg=eaop;
		effectaddr();
		if(eabits & (op2 ? 0xfe00 :  0xfe02)) badmode();
		if(op2 && (eabits&0x0002))
		{
			op=op2;
			eaop&=7;
		}
		if((op & 0x100) || eaop>7)
		{
			if((op&0xf100)==0xb000) badmode();
			wout(op|(reg<<9)|0x100|eaop);
			emitea();
		} else
			wout(op|(eaop<<9)|reg);
	} else
	{
		if(op & 0x100) badmode();
		if(op2)
		{
			reg=anyreg();
			if(reg>7)
			{
				op=op2;
				reg-=8;
			}
		}
		else
			reg=datareg();
		wout(op|(reg<<9)|eaop);
		emitea();
	}
}

void doaddb(void){if(anumsign) doaddib(); else arrith(0xd000,0);}
void doaddw(void){if(anumsign) doaddiw(); else arrith(0xd040,0xd0c0);}
void doaddl(void){if(anumsign) doaddil(); else {variant=3;arrith(0xd080,0xd1c0);}}

void doandb(void){if(anumsign) doandib(); else arrith(0xc000,0);}
void doandw(void){if(anumsign) doandiw(); else arrith(0xc040,0);}
void doandl(void){if(anumsign) doandil(); else {variant=3;arrith(0xc080,0);}}

void docmpb(void){if(anumsign) docmpib(); else arrith(0xb000,0);}
void docmpw(void){if(anumsign) docmpiw(); else arrith(0xb040,0xb0c0);}
void docmpl(void){if(anumsign) docmpil(); else {variant=3;arrith(0xb080,0xb1c0);}}

void doeorb(void){if(anumsign) doeorib(); else arrith(0xb100,0);}
void doeorw(void){if(anumsign) doeoriw(); else arrith(0xb140,0);}
void doeorl(void){if(anumsign) doeoril(); else {variant=3;arrith(0xb180,0);}}

void doorb(void){if(anumsign) doorib(); else arrith(0x8000,0);}
void doorw(void){if(anumsign) dooriw(); else arrith(0x8040,0);}
void doorl(void){if(anumsign) dooril(); else {variant=3;arrith(0x8080,0);}}

void dosubb(void){if(anumsign) dosubib(); else arrith(0x9000,0);}
void dosubw(void){if(anumsign) dosubiw(); else arrith(0x9040,0x90c0);}
void dosubl(void){if(anumsign) dosubil(); else {variant=3;arrith(0x9080,0x91c0);}}

void addresses(int op) {
	int reg;

	effectaddr();
	if(comma()) return;
	reg=addressreg();
	wout(op|(reg<<9)|eaop);
	emitea();
}

void docmpaw(void) {addresses(0xb0c0);}
void docmpal(void) {variant=3;addresses(0xb1c0);}
void doaddaw(void) {addresses(0xd0c0);}
void doaddal(void) {variant=3;addresses(0xd1c0);}
void dosubaw(void) {addresses(0x90c0);}
void dosubal(void) {variant=3;addresses(0x91c0);}

void rotates(int op) {
	int reg;

	if(get()=='#')
	{
		absexpr();
		if(comma()) return;
		reg=datareg();
		wout(((op&0x600)>>6) | (op&0xe1c0) | reg | (((int)exprval&7)<<9));
	} else
	{
		back1();
		effectaddr();
		if(eabits==1)
		{
			if(comma()) return;
			reg=datareg();
			wout(((op&0x600)>>6) | (op&0xe1c0) | reg | (eaop<<9) | 0x20);
		} else
		{
			if(eabits & 0x7e03) badmode();
			if((op&0xc0)!=0x40) badsize();
			wout(eaop | op | 0xc0);
			emitea();
		}
	}
}

void doasrb(void){rotates(0xe000);}
void doasrw(void){rotates(0xe040);}
void doasrl(void){rotates(0xe080);}

void doaslb(void){rotates(0xe100);}
void doaslw(void){rotates(0xe140);}
void doasll(void){rotates(0xe180);}

void dolsrb(void){rotates(0xe200);}
void dolsrw(void){rotates(0xe240);}
void dolsrl(void){rotates(0xe280);}

void dolslb(void){rotates(0xe300);}
void dolslw(void){rotates(0xe340);}
void dolsll(void){rotates(0xe380);}

void doroxrb(void){rotates(0xe400);}
void doroxrw(void){rotates(0xe440);}
void doroxrl(void){rotates(0xe480);}

void doroxlb(void){rotates(0xe500);}
void doroxlw(void){rotates(0xe540);}
void doroxll(void){rotates(0xe580);}

void dororb(void){rotates(0xe600);}
void dororw(void){rotates(0xe640);}
void dororl(void){rotates(0xe680);}

void dorolb(void){rotates(0xe700);}
void dorolw(void){rotates(0xe740);}
void doroll(void){rotates(0xe780);}

void doswap(void){wout(0x4840 | datareg());}

void divmul(int op) {
	effectaddr();
	if(comma()) return;
	if(eabits & 0xf002) badmode();
	wout(eaop | (datareg()<<9) | op);
	emitea();
}
void dochk(void){divmul(0x4180);}
void dodivu(void){divmul(0x80c0);}
void dodivs(void){divmul(0x81c0);}
void domulu(void){divmul(0xc0c0);}
void domuls(void){divmul(0xc1c0);}



void docmpms(int op) {
	int reg;

	if(expect('(')) return;
	reg=addressreg();
	if(expects(")+,(")) return;
	wout(op | reg | (addressreg()<<9));
}
void docmpmb(void){docmpms(0xb108);}
void docmpmw(void){docmpms(0xb148);}
void docmpml(void){docmpms(0xb188);}

void dotrap(void) {
	int tnum;

	if(numsign()) return;
	expr();
	tnum=exprval;
	if(tnum<0 || tnum>15) outofrange();
	wout(tnum | 0x4e40);
}

void doext(void){wout(datareg() | 0x4880);}
void doextl(void){wout(datareg() | 0x48c0);}

void domoveq(void) {
	if(numsign()) return;
	expr();
	if(exprval<-128 || exprval>127) outofrange();
	if(comma()) return;
	wout(((int)exprval&0xff) | (datareg()<<9) | 0x7000);
}

void bitops(int op, int invalids) {
	int reg;

	if(get()=='#')
	{
		expr();
		reg=exprval;
		if(comma()) return;
		effectaddr();
		if(eabits & 0xfe02) badmode();
		wout(eaop | op | 0x800);
		wout(reg);
		emitea();
	} else
	{
		back1();
		reg=datareg();
		if(comma()) return;
		singleea(op | 0x100 | (reg<<9),invalids);
	}
}
void dobtst(void) {bitops(0,0xf602);}
void dobchg(void) {bitops(0x40,0xfe02);}
void dobclr(void) {bitops(0x80,0xfe02);}
void dobset(void) {bitops(0xc0,0xfe02);}

void doexg(void) {
	int reg1,reg2;
	reg1=getreg()&0xffff;
	if(reg1>15) badreg();
	if(comma()) return;
	reg2=getreg()&0xffff;
	if(reg2>15) badreg();
	if(reg1<8)
	{
		if(reg2<8) wout(0xc140 | (reg2<<9) | reg1);
		else wout(0xc188 | (reg1<<9) | reg2);
	} else
	{
		if(reg2<8) wout(0xc188 | (reg2<<9) | (reg1-8));
		else wout(0xc148 | ((reg2-8)<<9) | reg1);
	}
}

void domovems(int op) {
	unsigned int mask,rmask,i,j,k,oldeabits;
	long reg;

	effectaddr();
	if(eabits&0x0803)
	{
		oldeabits=eabits;
		mask=eaword1;
		if(comma()) return;
		effectaddr();
		if(eabits & 0xfe07) badmode();
		if(eabits==0x10 && oldeabits!=0x800)
		{
			rmask=0;j=1;k=0x8000;
			for(i=0;i<16;i++)
			{
				if(mask&j) rmask+=k;
				j<<=1;k>>=1;
			}
			mask=rmask;
		}
		wout(op | eaop);
		wout(mask);
		emitea();
	} else
	{
		if(comma()) return;
		if(at()=='#')
		{
			forward1();
			expr();
			mask=exprval;
		} else
		{
			reg=getreg();
			if(!reg) badmode();
			reg=regexpr(reg);
			mask=reg>>16;
		}
		if(eabits & 0xf813) badmode();
		wout(op | eaop | 0x400);
		wout(mask);
		emitea();
	}
}
void domovemw(void){domovems(0x4880);}
void domoveml(void){domovems(0x48c0);}



void doillegal(void){wout(0x4afc);}
void doreset(void){wout(0x4e70);}
void donop(void){wout(0x4e71);}
void dostop(void){wout(0x4e72);}
void dorte(void){wout(0x4e73);}
void dorts(void){wout(0x4e75);}
void dotrapv(void){wout(0x4e76);}
void dortr(void){wout(0x4e77);}

struct anopcode scantab[]={
{"abcd",doabcd},
{"add",doaddw},
{"add.b",doaddb},
{"add.l",doaddl},
{"add.w",doaddw},
{"adda",doaddaw},
{"adda.l",doaddal},
{"adda.w",doaddaw},
{"addi",doaddiw},
{"addi.b",doaddib},
{"addi.l",doaddil},
{"addi.w",doaddiw},
{"addq",doaddqw},
{"addq.b",doaddqb},
{"addq.l",doaddql},
{"addq.w",doaddqw},
{"addx",doaddxw},
{"addx.b",doaddxb},
{"addx.l",doaddxl},
{"addx.w",doaddxw},
{"and",doandw},
{"and.b",doandb},
{"and.l",doandl},
{"and.w",doandw},
{"andi",doandiw},
{"andi.b",doandib},
{"andi.l",doandil},
{"andi.w",doandiw},
{"asl",doaslw},
{"asl.b",doaslb},
{"asl.l",doasll},
{"asl.w",doaslw},
{"asr",doasrw},
{"asr.b",doasrb},
{"asr.l",doasrl},
{"asr.w",doasrw},
{"bcc",dobcc},
{"bcc.s",dobccs},
{"bchg",dobchg},
{"bclr",dobclr},
{"bcs",dobcs},
{"bcs.s",dobcss},
{"beq",dobeq},
{"beq.s",dobeqs},
{"bge",dobge},
{"bge.s",dobges},
{"bgt",dobgt},
{"bgt.s",dobgts},
{"bhi",dobhi},
{"bhi.s",dobhis},
{"ble",doble},
{"ble.s",dobles},
{"bls",dobls},
{"bls.s",doblss},
{"blt",doblt},
{"blt.s",doblts},
{"bmi",dobmi},
{"bmi.s",dobmis},
{"bne",dobne},
{"bne.s",dobnes},
{"bpl",dobpl},
{"bpl.s",dobpls},
{"bra",dobra},
{"bra.s",dobras},
{"bset",dobset},
{"bsr",dobsr},
{"bsr.s",dobsrs},
{"btst",dobtst},
{"bvc",dobvc},
{"bvc.s",dobvcs},
{"bvs",dobvs},
{"bvs.s",dobvss},
{"chk",dochk},
{"clr",doclrw},
{"clr.b",doclrb},
{"clr.l",doclrl},
{"clr.w",doclrw},
{"cmp",docmpw},
{"cmp.b",docmpb},
{"cmp.l",docmpl},
{"cmp.w",docmpw},
{"cmpa",docmpaw},
{"cmpa.l",docmpal},
{"cmpa.w",docmpaw},
{"cmpi",docmpiw},
{"cmpi.b",docmpib},
{"cmpi.l",docmpil},
{"cmpi.w",docmpiw},
{"cmpm",docmpmw},
{"cmpm.b",docmpmb},
{"cmpm.l",docmpml},
{"cmpm.w",docmpmw},
{"dbcc",dodbcc},
{"dbcs",dodbcs},
{"dbeq",dodbeq},
{"dbf",dodbra},
{"dbfa",dodbra},
{"dbge",dodbge},
{"dbgt",dodbgt},
{"dbhi",dodbhi},
{"dble",dodble},
{"dbls",dodbls},
{"dblt",dodblt},
{"dbmi",dodbmi},
{"dbne",dodbne},
{"dbpl",dodbpl},
{"dbra",dodbra},
{"dbt",dodbtr},
{"dbtr",dodbtr},
{"dbvc",dodbvc},
{"dbvs",dodbvs},
{"divs",dodivs},
{"divs.w",dodivs},
{"divu",dodivu},
{"divu.w",dodivu},
{"eor",doeorw},
{"eor.b",doeorb},
{"eor.l",doeorl},
{"eor.w",doeorw},
{"eori",doeoriw},
{"eori.b",doeorib},
{"eori.l",doeoril},
{"eori.w",doeoriw},
{"exg",doexg},
{"ext",doext},
{"ext.l",doextl},
{"ext.w",doext},
{"illegal",doillegal},
{"jmp",dojmp},
{"jsr",dojsr},
{"lea",dolea},
{"link",dolink},
{"lsl",dolslw},
{"lsl.b",dolslb},
{"lsl.l",dolsll},
{"lsl.w",dolslw},
{"lsr",dolsrw},
{"lsr.b",dolsrb},
{"lsr.l",dolsrl},
{"lsr.w",dolsrw},
{"move",domovew},
{"move.b",domoveb},
{"move.l",domovel},
{"move.w",domovew},
{"movem",domovemw},
{"movem.l",domoveml},
{"moveq",domoveq},
{"muls",domuls},
{"muls.w",domuls},
{"mulu",domulu},
{"mulu.w",domulu},
{"nbcd",donbcd},
{"neg",donegw},
{"neg.b",donegb},
{"neg.l",donegl},
{"neg.w",donegw},
{"negx",donegxw},
{"negx.b",donegxb},
{"negx.l",donegxl},
{"negx.w",donegxw},
{"nop",donop},
{"not",donotw},
{"not.b",donotb},
{"not.l",donotl},
{"not.w",donotw},
{"or",doorw},
{"or.b",doorb},
{"or.l",doorl},
{"or.w",doorw},
{"ori",dooriw},
{"ori.b",doorib},
{"ori.l",dooril},
{"ori.w",dooriw},
{"pea",dopea},
{"reset",doreset},
{"rol",dorolw},
{"rol.b",dorolb},
{"rol.l",doroll},
{"rol.w",dorolw},
{"ror",dororw},
{"ror.b",dororb},
{"ror.l",dororl},
{"ror.w",dororw},
{"roxl",doroxlw},
{"roxl.b",doroxlb},
{"roxl.l",doroxll},
{"roxl.w",doroxlw},
{"roxr",doroxrw},
{"roxr.b",doroxrb},
{"roxr.l",doroxrl},
{"roxr.w",doroxrw},
{"rte",dorte},
{"rtr",dortr},
{"rts",dorts},
{"sbcd",dosbcd},
{"scc",doscc},
{"scs",doscs},
{"seq",doseq},
{"sf",dosfa},
{"sfa",dosfa},
{"sge",dosge},
{"sgt",dosgt},
{"shi",doshi},
{"sle",dosle},
{"sls",dosls},
{"slt",doslt},
{"smi",dosmi},
{"sne",dosne},
{"spl",dospl},
{"st",dostr},
{"stop",dostop},
{"str",dostr},
{"sub",dosubw},
{"sub.b",dosubb},
{"sub.l",dosubl},
{"sub.w",dosubw},
{"suba",dosubaw},
{"suba.l",dosubal},
{"suba.w",dosubaw},
{"subi",dosubiw},
{"subi.b",dosubib},
{"subi.l",dosubil},
{"subi.w",dosubiw},
{"subq",dosubqw},
{"subq.b",dosubqb},
{"subq.l",dosubql},
{"subq.w",dosubqw},
{"subx",dosubxw},
{"subx.b",dosubxb},
{"subx.l",dosubxl},
{"subx.w",dosubxw},
{"svc",dosvc},
{"svs",dosvs},
{"swap",doswap},
{"tas",dotas},
{"trap",dotrap},
{"trapv",dotrapv},
{"tst",dotstw},
{"tst.b",dotstb},
{"tst.l",dotstl},
{"tst.w",dotstw},
{"unlk",dounlk},
{0}};
