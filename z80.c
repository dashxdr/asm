#include "asm.h"

int32_t z80zero;

void z80init(void) {
	z80zero=0;
}


void z80rels(char op) {
	long off;

	bout(op);
	expr();
	off=exprval-pcount+z80zero;
	bout((int)off);
	if(off<-0x80 || off>0x7f) outofrange();
}

int z80r16(void) {
	char ch;

	ch=get();

	switch(tolower(ch))
	{
		case 'b':return 0x00;
		case 'd':return 0x10;
		case 'h':return 0x20;
		case 's':
			if(tolower(get())=='p') return 0x30;
			back1();
			break;
		case 'p':
			if(tolower(get())=='s')
			{
				if(tolower(get())=='w') return 0x30;
				back1();
			}
			back1();
			break;
	}
	badreg();
	back1();
	return 0;
}

int z80r8(void) {
	char ch;

	ch=get();
	switch(tolower(ch))
	{
		case 'b': return 0;
		case 'c': return 1;
		case 'd': return 2;
		case 'e': return 3;
		case 'h': return 4;
		case 'l': return 5;
		case 'm': return 6;
		case 'a': return 7;
	}
	badreg();
	back1();
	return 0;
}

void z80ax(char op) {
	char r1;
	r1=z80r16();
	if(r1>=0x20) badreg();
	else bout(op | r1);
}

void z80inrdcr(char op) {
	bout(op | (z80r8()<<3));
}

void z80immed(char op) {
	bout(op);
	expr();
	bout((char)exprval);
}

void z80ops16(char op) {
	bout(op | z80r16());
}

void z80abs(char op) {
	int	res;
	bout(op);
	expr();
	res=exprval;
	bout(res);
	bout(res>>8);
}

void z80arrith(char op) {
	bout(op+z80r8());
}



void z80aci(void){z80immed(0xce);}
void z80adc(void){z80arrith(0x88);}
void z80add(void){z80arrith(0x80);}
void z80adi(void){z80immed(0xc6);}
void z80ana(void){z80arrith(0xa0);}
void z80ani(void){z80immed(0xe6);}
void z80call(void){z80abs(0xcd);}
void z80cc(void){z80abs(0xdc);}
void z80cm(void){z80abs(0xfc);}
void z80cma(void){bout(0x2f);}
void z80cmc(void){bout(0x3f);}
void z80cmp(void){z80arrith(0xb8);}
void z80cnc(void){z80abs(0xd4);}
void z80cnz(void){z80abs(0xc4);}
void z80cout(void)
{
	char r1;
	r1=z80r8();
	bout(0xed);
	bout(0x41 | (r1<<3));
}
void z80cp(void){z80abs(0xf4);}
void z80cpe(void){z80abs(0xec);}
void z80cpi(void){z80immed(0xfe);}
void z80cpo(void){z80abs(0xe4);}
void z80cz(void){z80abs(0xcc);}
void z80daa(void){bout(0x27);}
void z80dad(void){z80ops16(0x09);}
void z80db(void){dodcb();}
void z80dcr(void){z80inrdcr(0x05);}
void z80dcx(void){z80ops16(0x0b);}
void z80di(void){bout(0xf3);}
void z80djnz(void){z80rels(0x10);}
void z80ds(void){dodsb();}
void z80dw(void)
{
	for(;;)
	{
		expr();
		bout((int)exprval);
		bout((int)exprval>>8);
		if(get()!=',') break;
	}
	back1();
}
void z80ei(void){bout(0xfb);}
void z80ex(void){bout(0x08);}
void z80exx(void){bout(0xd9);}
void z80hlt(void){bout(0x76);}
void z80im0(void){wout(0xed46);}
void z80im1(void){wout(0xed56);}
void z80im2(void){wout(0xed5e);}
void z80in(void){z80immed(0xdb);}
void z80inr(void){z80inrdcr(0x04);}
void z80inx(void){z80ops16(0x03);}
void z80jc(void){z80abs(0xda);}
void z80jm(void){z80abs(0xfa);}
void z80jmp(void){z80abs(0xc3);}
void z80jnc(void){z80abs(0xd2);}
void z80jnz(void){z80abs(0xc2);}
void z80jp(void){z80abs(0xf2);}
void z80jpe(void){z80abs(0xea);}
void z80jpo(void){z80abs(0xe2);}
void z80jr(void){z80rels(0x18);}
void z80jrc(void){z80rels(0x38);}
void z80jrnc(void){z80rels(0x30);}
void z80jrnz(void){z80rels(0x20);}
void z80jrz(void){z80rels(0x28);}
void z80jz(void){z80abs(0xca);}
void z80lda(void){z80abs(0x3a);}
void z80ldax(void){z80ax(0x0a);}
void z80ldd(void){wout(0xeda8);}
void z80lddr(void){wout(0xedb8);}
void z80ldi(void){wout(0xeda0);}
void z80ldir(void){wout(0xedb0);}
void z80lhld(void){z80abs(0x2a);}
void z80lxi(void){
	char r1;

	r1=z80r16();
	if(comma()) return;
	z80abs(r1 | 1);
}
void z80mov(void)
{
	char r1,r2;

	r1=z80r8();
	if(comma()) return;
	r2=z80r8();
	bout((r1<<3) | r2 | 0x40);
}
void z80mvi(void)
{
	char r1;

	r1=z80r8();
	if(comma()) return;
	bout(6+(r1<<3));
	expr();
	bout((int)exprval);
}
void z80nop(void){bout(0x00);}
void z80ora(void){z80arrith(0xb0);}
void z80org(void){expr();z80zero=pcount-exprval;}
void z80ori(void){z80immed(0xf6);}
void z80out(void){z80immed(0xd3);}
void z80pchl(void){bout(0xe9);}
void z80pop(void){z80ops16(0xc1);}
void z80push(void){z80ops16(0xc5);}
void z80ral(void){bout(0x17);}
void z80rar(void){bout(0x1f);}
void z80rc(void){bout(0xd8);}
void z80ret(void){bout(0xc9);}
void z80reti(void){wout(0xed4d);}
void z80retn(void){wout(0xed45);}
void z80rlc(void){bout(0x07);}
void z80rm(void){bout(0xf8);}
void z80rnc(void){bout(0xd0);}
void z80rnz(void){bout(0xc0);}
void z80rp(void){bout(0xf0);}
void z80rpe(void){bout(0xe8);}
void z80rpo(void){bout(0xe0);}
void z80rrc(void){bout(0x0f);}
void z80rst(void)
{
	char ch;

	expr();
	ch=exprval;
	ch&=7;
	bout(0xc7 | (ch<<3));
}
void z80rz(void){bout(0xc8);}
void z80sbb(void){z80arrith(0x98);}
void z80sbi(void){z80immed(0xde);}
void z80shld(void){z80abs(0x22);}
void z80sphl(void){bout(0xf9);}
void z80sta(void){z80abs(0x32);}
void z80stax(void){z80ax(0x02);}
void z80stc(void){bout(0x37);}
void z80sub(void){z80arrith(0x90);}
void z80sui(void){z80immed(0xd6);}
void z80xchg(void){bout(0xeb);}
void z80xra(void){z80arrith(0xa8);}
void z80xri(void){z80immed(0xee);}
void z80xthl(void){bout(0xe3);}


struct anopcode z80codes[]={
{"aci",z80aci},
{"adc",z80adc},
{"add",z80add},
{"adi",z80adi},
{"ana",z80ana},
{"ani",z80ani},
{"call",z80call},
{"cc",z80cc},
{"cm",z80cm},
{"cma",z80cma},
{"cmc",z80cmc},
{"cmp",z80cmp},
{"cnc",z80cnc},
{"cnz",z80cnz},
{"cout",z80cout},
{"cp",z80cp},
{"cpe",z80cpe},
{"cpi",z80cpi},
{"cpo",z80cpo},
{"cz",z80cz},
{"daa",z80daa},
{"dad",z80dad},
{"db",z80db},
{"dcr",z80dcr},
{"dcx",z80dcx},
{"di",z80di},
{"djnz",z80djnz},
{"ds",z80ds},
{"dw",z80dw},
{"ei",z80ei},
{"ex",z80ex},
{"exx",z80exx},
{"hlt",z80hlt},
{"im0",z80im0},
{"im1",z80im1},
{"im2",z80im2},
{"in",z80in},
{"inr",z80inr},
{"inx",z80inx},
{"jc",z80jc},
{"jm",z80jm},
{"jmp",z80jmp},
{"jnc",z80jnc},
{"jnz",z80jnz},
{"jp",z80jp},
{"jpe",z80jpe},
{"jpo",z80jpo},
{"jr",z80jr},
{"jrc",z80jrc},
{"jrnc",z80jrnc},
{"jrnz",z80jrnz},
{"jrz",z80jrz},
{"jz",z80jz},
{"lda",z80lda},
{"ldax",z80ldax},
{"ldd",z80ldd},
{"lddr",z80lddr},
{"ldi",z80ldi},
{"ldir",z80ldir},
{"lhld",z80lhld},
{"lxi",z80lxi},
{"mov",z80mov},
{"mvi",z80mvi},
{"nop",z80nop},
{"ora",z80ora},
{"org",z80org},
{"ori",z80ori},
{"out",z80out},
{"pchl",z80pchl},
{"pop",z80pop},
{"push",z80push},
{"ral",z80ral},
{"rar",z80rar},
{"rc",z80rc},
{"ret",z80ret},
{"reti",z80reti},
{"retn",z80retn},
{"rlc",z80rlc},
{"rm",z80rm},
{"rnc",z80rnc},
{"rnz",z80rnz},
{"rp",z80rp},
{"rpe",z80rpe},
{"rpo",z80rpo},
{"rrc",z80rrc},
{"rst",z80rst},
{"rz",z80rz},
{"sbb",z80sbb},
{"sbi",z80sbi},
{"shld",z80shld},
{"sphl",z80sphl},
{"sta",z80sta},
{"stax",z80stax},
{"stc",z80stc},
{"sub",z80sub},
{"sui",z80sui},
{"xchg",z80xchg},
{"xra",z80xra},
{"xri",z80xri},
{"xthl",z80xthl},
{0}};
