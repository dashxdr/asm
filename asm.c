#include "asm.h"
#include <sys/timeb.h>

#define BLOCKSIZE 0x40000L
#define IOBLOCKSIZE 30000
#define MACROMAX 16384
#define MAXSYMS 16384
#define FORCEOBJ 1
#define LISTBOTH 0


char *outblock=0;
char *outoff;
char *textblock=0;
char *listbuff=0;
char *textpoint;
char *macroblock=0;
char **filelist=0;
struct sym *publichead;
uchar anumsign;
struct sym *lastrel;
uchar somebss;
int lineCount;

struct reference *reloc16head=0,*reloc32head=0;
int reloc32num=0,reloc16num=0;


char *listpoint,*listend;

struct sym *heresym;

long lseek();


struct oplist z80list,scanlist,directlist,x86list,*currentlist;

void domacro(void);
void doendm(void);
void doinclude(void);
void setyes(void);
void setno(void);
void doset(void);
void doequ(void);
int dopass(void);
void zeros(long size);
int dnd(void);
int cnc(void);
void doset(void);
void doequ(void);
void outpublics(void);
void outbss(void);
void outrelocs(void);
void symsout(int mask);
void tailout(void);
void headout(void);
void endstore(void);
void addtext(char *str);
void operand(void);
void operator(void);
int trytop(void);
void expr2(void);
void listline(char *toff1, char *ooff1, int pcsave);
void flushlist(void);
void handlelabel(void (*func)(void));
int token(struct sym *asym);
void aline(void);
void flush(void);
void assemble(char *str);
int dofile(void);
void closeall(void);

struct sym **headers=0;
long exprval;

char *inpoint;
uchar en68000=0;

uchar exprstack[EXPRMAX];
uchar *exprsp;
void failerr(void);

#define INCNAME "A86INC"

void dom68000(void);
void dox86(void);
void doz80(void);

struct sym *lastref;
long operval;
long soffset;
struct sym *nextsym;
unsigned maxlines;
int numopen;
uchar options[256];
FILE *outfile=0;
FILE *listfile=0;
int32_t pcount,pcmax;
int pass,passlist;
int cline;
char *macstack;
char *macpars[10];
int maclens[10];
uchar attr[128];
int symcount;
FILE *errorfile=0;
int errorcount,warncount;
uchar **blockhead;
long blockcount;
struct sym *symbols;

/*-----------------All uchars follow------*/
uchar alabel; /* flag, 1 if this line had a label */
struct sym linelabel,symbol,symbol2,opsym;
char ltext[SYMMAX+2];
char stext[SYMMAX+2];
char stext2[SYMMAX+2];
char opcode[80];
uchar variant;
uchar exprtype;
uchar exprflag;
uchar opertype;
uchar phase;
uchar storing;
uchar oppri;
uchar opop;
uchar depth,ydepth;
uchar expanding;
uchar inmac;
uchar xrdflag;
char filename[256];
char inputname[256];
char outputname[256];
char umac[3];
char *TREAD="r";
char *TWRITE="w";

/* abs=0, rel=1, reg=2, xref=3, mac=4 */
uchar cnone[5][5]={
	{ABSTYPE,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR}};
uchar cplus[5][5]={
	{ABSTYPE,RELTYPE,ERR,XREFTYPE,ERR},
	{RELTYPE,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR},
	{XREFTYPE,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR}};
uchar cminus[5][5]={
	{ABSTYPE,ERR,ERR,ERR,ERR},
	{RELTYPE,ABSTYPE,ERR,ERR,ERR},
	{ERR,ERR,REGTYPE,ERR,ERR},
	{XREFTYPE,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR}};
uchar cdivide[5][5]={
	{ABSTYPE,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR},
	{ERR,ERR,REGTYPE,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR},
	{ERR,ERR,ERR,ERR,ERR}};



long gettime();

long gtime()
{
	struct timeb tb;

	ftime(&tb);
	return tb.time*1000 + tb.millitm;
}




void makename(char *to, char *from, char *ch) {
	strcpy(to,from);
	strcat(to,ch);
}

int hash8(char *str)
{
	int hash=0;
	char ch;

	while((ch=*str++))
	{
		hash+=hash;
		if(hash&256) hash-=255;
		hash^=ch;
	}
	return hash;
}


struct sym *addsym(struct sym *asym) {
	int h;

	h=hash8(asym->symtext);
	nextsym->symnext=headers[h];
	headers[h]=nextsym;
	nextsym->symvalue=asym->symvalue;
	nextsym->symtype=asym->symtype;
	nextsym->symnum=asym->symnum;
	nextsym->symunique=asym->symunique;
	nextsym->symflags=asym->symflags;
	nextsym->symref=0;
	nextsym->symtext=textpoint;
	nextsym++; /* bounds check */
	addtext(asym->symtext);
	return nextsym-1;
}



void fixattr(int orval, char *str) {
	char ch;

	while((ch=*str++))
		attr[ch&255]|=orval;
}
void error(char *emsg) {
	if(errorfile) fprintf(errorfile,"%d:%s\n",cline,emsg);
	else printf("%s(%d) %s\n",filename,cline,emsg);
}
void error2(char *emsg) {
	if(pass) {errorcount++;error(emsg);}
}
void error1(char *emsg) {
	if(!pass) {errorcount++;error(emsg);}
}

void phaserr(void) {error2("Phase error");}
void failerr(void) {error2("FAIL directive");}
void unbalancedq(void) {error2("Unbalanced '");}
void unbalanced(void) {error2("Unbalanced ()");}
void badreg(void) {error2("Illegal register");}
void baduchar(void) {error2("Illegal ucharacter");}
void illegalop(void) {error2("Illegal op code");}
void syntaxerr(void) {error2("Syntax error");}
void absreq(void) {error2("Absolute data required");}
void badmode(void) {error2("Illegal effective address");}
void outofrange(void) {error2("Operand out of range");}
void div0(void) {error2("Divide by zero");}
void unknownerr(char *str) {
	char temp[80];
	strcpy(temp,"Undefined symbol ");
	strcat(temp,str);
	error2(temp);
}
void duplicate(void) {error1("Duplicate label");}
void badoperation(void) {error2("Illegal operation");}
void badsize(void) {error2("Illegal opcode size");}
void warn2(char *emsg) {if(pass) {warncount++;error(emsg);}}

void nomem(int num) {
	printf("Not enough memory %d\n",num);
	closeall();
	exit(num);
}
void freeind(void **addr) {
	if(*addr) {
		tfree(*addr);
		*addr=0;
	}
}
void ccexit(void) {
	closeall();
	printf("*** Break\n");
	exit(1);
}
void freestuff(void) {
	char **p;

	freeind((void**)&headers);
	freeind((void**)&listbuff);
	freeind((void**)&outblock);
	freeind((void**)&textblock);
	freeind((void**)&macroblock);
	freeind((void**)&symbols);

	while(filelist)
	{
		p=filelist;
		filelist=(void *)*filelist;
		tfree(p);
	}
}
void *tmalloc(int len)
{
void *p;
	p=malloc(len);
	return p;
}
void tfree(void *pntr)
{
	free(pntr);
}



void countops(struct oplist *list, struct anopcode *tab) {
	int i,j;
	i=0;
	list->listopcodes=tab;
	while((tab++) ->opcodename) i++;
	j=1;
	while(j<=i) j+=j;
	j>>=1;
	list->numops=i;
	list->powerops=j;
}
void dox86(void) {
	currentlist=&x86list;
	z80zero=0;
}
void doz80(void) {
	currentlist=&z80list;
	z80zero=pcount;
}
void dom68000(void) {
	currentlist=&scanlist;
	z80zero=0;
}

char *nametail(char *str) {
	char *p;

	p=str+strlen(str);
	while(p>str && p[-1]!='/' && p[-1]!=':') p--;
	return p;
}

void setup(int argc, char **argv) {
	int i,j,mode;
	char ch;

	outputname[0]=0;
	filename[0]=0;
	inputname[0]=0;
	for(i=0;i<256;i++) options[i]=0;
	for(i=1,mode=0;i<argc;i++)
	{
reswitch:
		switch(mode)
		{
		case 0:
			if(argv[i][0]!='-')
			{
				strcpy(inputname,argv[i]);
				break;
			}
			mode=argv[i][1];
			if(argv[i][2]) {j=2;goto reswitch;}
			else {j=0;continue;}
		case 'o':
			strcpy(outputname,argv[i]+j);
			mode=0;
			break;
		case '-':
			while((ch=argv[i][j++])) options[ch&255]=1;
			break;
		default:
			mode=0;
			break;
		}
		mode=0;
	}
}
void makefilename(void) {
	int i;
	i=strlen(inputname);
	if(i>2 && (!strcmp(inputname+i-2,".a") || !strcmp(inputname+i-2,".s")))
		strcpy(filename,inputname);
	else sprintf(filename,"%s.a",inputname);
}


int main(int argc,char **argv)
{
	char temp[80];
	char *pntr,ch;
	int i;
	long	time1,time2,lpm;
	long pcmaxt;
	struct sym *asym;
	int sfile;

	time1=gtime();
	en68000=strcmp(nametail(*argv),"a86");
	errorcount=0;warncount=0;
	errorfile=0;
	listfile=0;
	blockhead=0;
	blockcount=0;
	outfile=0;
	publichead=0;
	xrdflag=FORCEOBJ && !en68000;
	filelist=0;

	if(argc<2)
	{
		printf("%s Copyright (C) 1997 by David Ashley\n",nametail(*argv));
		printf("%s <inputfile> [-o <outputfile>] [-- options]\n",nametail(*argv));
		printf("Options:\n");
		printf("b    Produce binary file\n");
		printf("e    Send errors to <inputfile>.err\n");
		printf("l    Produce listing file <inputfile>.l\n");
		printf("o    Inhibit creation of object file\n");
		printf("s    Include symbol table information in output\n");
		printf("u    Display statistics on usage and speed\n");
		return 1;
	}

	headers=tmalloc(256*sizeof(struct sym *));
	if(!headers) nomem(11);
	bzero(headers,256*sizeof(struct sym *));
	outblock=tmalloc(BLOCKSIZE);
	if(!outblock) nomem(1);
	textblock=tmalloc(BLOCKSIZE);
	if(!textblock) nomem(2);
	textpoint=textblock;
	listbuff=tmalloc(IOBLOCKSIZE);
	if(!listbuff) nomem(10);
	listpoint=listbuff;
	listend=listbuff+IOBLOCKSIZE-256;
	macroblock=tmalloc(MACROMAX);
	if(!macroblock) nomem(4);
	macstack=macroblock;
	symbols=tmalloc(sizeof(struct sym) * MAXSYMS);
	if(!symbols) nomem(6);
	nextsym=symbols;
	heresym=nextsym++;
	bzero(heresym,sizeof(struct sym));
	heresym->symtype=RELTYPE;

	countops(&scanlist,scantab);
	countops(&z80list,z80codes);
	countops(&directlist,directs);
	countops(&x86list,x86codes);

	somebss=0;
	for(i=0;i<128;i++)
		attr[i]=0;
	fixattr(ATTRWHITE,"\t \015");
	fixattr(ATTRSYM+ATTRFSYM+ATTROP,
		"abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	fixattr(ATTRFSYM+ATTROP,".");
	fixattr(ATTRFSYM,"@");
	fixattr(ATTRSYM+ATTRNUM+ATTRHEX+ATTROP,"0123456789");
	fixattr(ATTRHEX,"abcdefABCDEF");
	linelabel.symtext=ltext;
	symbol.symtext=stext;
	symbol2.symtext=stext2;
	opsym.symtext=opcode;

	setup(argc,argv);
	makefilename();
	sfile=open(filename,O_RDONLY);
	if(sfile<0) {printf("Cannot open %s\n",filename);return 1;}
	close(sfile);

	if(options['e'])
	{
		makename(temp,inputname,".err");
		errorfile=fopen(temp,"w");
	}
	if(options['l'])
	{
		makename(temp,inputname,".l");
		listfile=fopen(temp,"w");
		if(!listfile)
		{
			printf("Cannot open listing file %s\n",temp);
			closeall();
			return 1;
		}
	}

	outoff=outblock;
	phase=0;
	pass=0;
	passlist=LISTBOTH;
	if(dopass()) goto skip2;
	outoff=outblock;
	pass=1;
	passlist=1;

	xrdflag=xrdflag || publichead;

	makefilename();

	if(!options['o'])
	{
/*
		strcpy(temp,inputname);
		if(!en68000 && options['b']) strcat(temp,".com");
		else if(!en68000 || xrdflag) strcat(temp,".o");
*/
		if(!outputname[0])
		{
			strcpy(outputname,filename);
			outputname[strlen(outputname)-2]=0;
			if(xrdflag) strcat(outputname,".o");
		}
		outfile=fopen(outputname,"w");
		if(!outfile)
		{
			printf("Cannot open output file %s\n",outputname);
			closeall();
			return 1;
		}
	}

	if(!options['b']) headout();
	dopass();
	pcmaxt=pcmax;
	if(options['s'] && !options['b'])
		symsout(ADEF);
	outrelocs();
	if(xrdflag)
		outpublics();
	if(!options['b']) tailout();
	if(somebss)
		outbss();
	flush();
	if(listfile && 0)
	{
		for(i=0;i<256;i++)
		{
			asym=headers[i];
			while(asym)
			{
				putc(asym->symtype+'0',listfile);
				putc(':',listfile);
				pntr=asym->symtext;
				while((ch=*pntr++)) putc(ch,listfile);
				fprintf(listfile,"=%lx\n",(long)asym->symvalue);
				asym=asym->symnext;
			}
		}
	}
	time2=gtime();
	time2-=time1;if(time2==0) time2++;

	lpm=(maxlines*6000L)/time2;

	if(options['u'])
	{
		printf("%06lx/%06lx Macro/symbol text/xdef/xref used\n",textpoint-textblock,BLOCKSIZE);
		printf("%04x/%04x     Symbols\n",(int)(nextsym-symbols),MAXSYMS);
		printf("%06lx        Object code bytes\n",pcmaxt);
		printf("%-11d   Lines\n",maxlines);
		printf("%-11ld   Lines/minute\n",lpm);
	}

skip2:
	closeall();
	if(errorcount) printf("%d error%s\n",errorcount,
		errorcount==1 ? "" : "s");
	if(warncount) printf("%d warning%s\n",warncount,
		warncount==1 ? "" : "s");
	exit(errorcount!=0);
}

void closeall(void) {
	if(listfile) {flushlist();fclose(listfile);}
	if(outfile) fclose(outfile);
	if(errorfile) fclose(errorfile);
	freestuff();
}


void flush(void) {
	if(outfile)
		fwrite(outblock,1,outoff-outblock,outfile);
	outoff=outblock;
}



int dopass(void)
{
	lineCount = 0;
	umac[0]='a'-1;umac[1]=umac[2]='a';
	symcount=1;
	x86init();
	z80init();
	if(en68000) dom68000();
	else dox86();
	storing=0;
	pcmax=0;
	depth=0;ydepth=0;
	pcount=0;
	maxlines=0;
	expanding=0;inmac=0;
	numopen=0;
	dofile();
	if(!en68000)
		while(pcmax&15)
			bout(0);
	else
		while(pcmax & 3)
			bout(0);
	return 0;
}
long filelen(file)
int file;
{
long len,loc;
	loc=lseek(file,0L,1);
	len=lseek(file,0L,2);
	lseek(file,loc,0);
	return len;
}
int dofile(void)
{
	int file;
	long len;
	char *addr;
	char **fl;

	cline=0;
	if(!pass)
	{
		file=open(filename,O_RDONLY);
		if(file<0)
		{
			printf("Cannot open input file %s\n",filename);
			return 1;
		}
		len=filelen(file);
		addr=tmalloc(len+65);
		if(!addr) nomem(5);
		fl=filelist;
		filelist=(void *)addr;
		*filelist=(void *)fl;
		strcpy((void *)(filelist+1),filename);

		addr+=64;
		len=read(file,addr,len);
		addr[len]=0;
		removecr(addr);

		close(file);
	} else
	{
		fl=filelist;
		while(fl)
		{
			if(!strcmp((void *)(fl+1),filename))
			{
				addr=(void *)fl;
				addr+=64;
				break;
			}
			fl=(void *)*fl;
		}
		if(!fl)
		{
			printf("Pass 2: File %s not seen in pass 1\n",filename);
			return 1;
		}
	}
	numopen++;
	assemble(addr);
	maxlines+=cline;
	numopen--;
	return 0;
}


int expect(char ch) {
	if(get()==ch) return 0;
	syntaxerr();back1();return 1;
}
int expects(char *str) {
	while(*str)
		if(get()!=*str++)
			{syntaxerr();back1();return 1;}
	return 0;
}

void assemble(char *str) {
	inpoint=str;

	while(at())
	{
		++cline;
		aline();
		while(get()!=LF);
	}
}

int lineC=1;
void aline(void) {
	++lineCount;
//	printf("Line %d\n", lineCount);
	void (*func)(void);
	char *toff1;
	char *ooff1;
	char ch;
	int pcsave;
	int i,j;
	struct sym *pntr;
	char *mactop,*cursave;
	char *p,*p2;

	heresym->symvalue=pcount;
	if(currentlist==&z80list) heresym->symvalue-=z80zero;
	toff1=inpoint;
	ooff1=outoff;
	pcsave=pcount;

	alabel=0;
	ch=at();
	if(!storing && !iswhite(ch))
	{
		if(ch==';') goto printline;

		if(isokfirst(ch))
		{
			if(ch!=LOCAL && ch!='@') ++symcount;
			ch=token(&linelabel);
			alabel=1;
			if(ch==':') forward1();

			linelabel.symtype=RELTYPE;
			linelabel.symvalue=pcount-z80zero;
			linelabel.symnum= symcount;
		}
	}
	if(storing && !iswhite(at())) {if(!pass) inpoint=storeline(toff1);goto printline;}
	ch=skipwhite();
	if(isokfirst(ch))
	{
		ch=gather(opcode);
		variant=0;
		skipwhite();
		
		anumsign=(at()=='#');

		func=scan(opcode,currentlist);

		if(!func) func=scan(opcode,&directlist);

		if(storing)
		{
			if(func==doendm) {storing=0;if(!pass) endstore();}
			else if(!pass) inpoint=storeline(toff1);
			goto printline;
		}
		if(func)
		{
			if(func==doinclude && depth==ydepth)
			{
				if(listfile && passlist)
					listline(toff1,ooff1,pcsave); /*{save=inpoint;listline(toff1,ooff1,pcsave);inpoint=save;}*/
				doinclude();
				return;
			}
			if(depth==ydepth)
			{
				if(alabel) handlelabel(func);
				func();
			}
			else if(func>setyes && func<setno) func();
		} else
		{
			if((pntr=findsym(&opsym)))
			{
				if(pntr->symtype==MACTYPE)
				{
					umac[0]++;
					if(umac[0]=='z'+1)
					{
						umac[0]='a';
						umac[1]++;
						if(umac[1]=='z'+1)
						{
							umac[1]='a';
							umac[2]++;
						}
					}
					mactop=macstack;
					maclens[0]=0;
					for(i=1;i<10;i++)
					{
						macpars[i]=inpoint;
						maclens[i]=0;
						for(;;)
						{
							ch=at();
							if(ch==LF) break;
							if(iswhite(ch)) break;
							forward1();
							if(ch==',') break;
							maclens[i]++;
						}
					}
					if(listfile && passlist && expanding)
						listline(toff1,ooff1,pcsave);
					if(inmac==MACDEPTH)
						{error("Macro overflow");return;}
					inmac++;
					p=(char *)pntr->symvalue;
					while((ch=*p++))
					{
						if(ch=='\\')
						{
							if(isoknum((ch=*p)))
							{
								p++;
								ch-='0';
								j=maclens[ch&255];
								p2=macpars[ch&255];
								while(j--) *macstack++=*p2++;
							} else if(ch=='@')
							{
								i++;
								*macstack++=umac[2];
								*macstack++=umac[1];
								*macstack++=umac[0];
							}
						} else *macstack++=ch;
					}
					*macstack++=0;

					cursave=inpoint;
					inpoint=mactop;
					while(at())
					{
						aline();
						while(get()!=LF);
					}
					inpoint=cursave;
					macstack=mactop;inmac--;
					if(expanding) return;
				}
			} else
				illegalop();
		}
	} else if(alabel && depth==ydepth) handlelabel(0);

printline:
	if(listfile && passlist) listline(toff1,ooff1,pcsave);
}

void handlelabel(void (*func)(void)) {
	struct sym *pntr;

	if(!pass)
	{
		if((pntr=findsym(&linelabel)))
		{
			if(func!=doset)
			{
				if(pntr->symflags&ADEF)
					duplicate();
				else
				{
					pntr->symvalue=linelabel.symvalue;
					pntr->symtype=linelabel.symtype;
					pntr->symnum=linelabel.symnum;
					pntr->symflags|=ADEF;
					pntr->symunique=linelabel.symunique;
				}
			}
		} else
		{
			linelabel.symflags=ADEF;
			addsym(&linelabel);
		}
	} else
	{
		pntr=findsym(&linelabel);
		if(pntr->symvalue!=linelabel.symvalue)
		{
			if(func!=doset && func!=doequ && func!=domacro && !phase)
			{
				phase=1;
				phaserr();
			}
		}
	}
}

void flushlist(void) {
	fwrite(listbuff,1,listpoint-listbuff,listfile);
	listpoint=listbuff;
}
void listadvance(void) {while(*listpoint) ++listpoint;}

void listline(char *toff1, char *ooff1, int pcsave) {
	char *ooff2;
	char ch,lflag;
	int xpos,cnt;
	char *save;

	if(!expanding && inmac) return;

	if(listpoint>listend)
		flushlist();

	save=inpoint;
	ooff2=outoff;
	outoff=ooff1;
	lflag=0;
	for(;;)
	{
		sprintf(listpoint,"%04x",pcsave);
		listadvance();
		xpos=5;
		cnt=0;
		*listpoint++=inmac ? '+' : ' ';
		while(outoff<ooff2)
		{
			sprintf(listpoint,"%02x",*outoff++);
			listadvance();
			pcsave++;
			xpos+=2;
			cnt++;
			if(cnt==8) break;
		}
		if(!lflag)
		{
			lflag++;
			while(xpos<24) {xpos++; *listpoint++=' ';}
			inpoint=toff1;
			while((ch=get())!=LF) *listpoint++=ch;
			back1();
		}
		*listpoint++='\n';
		if(outoff==ooff2) break;
	}
	inpoint=save;
}




void pushl(long val) {
	*(long *)exprsp=val;
	exprsp+=sizeof(long);
}
void pushb(uchar val) {
	*exprsp++=val;
}
long popl()
{
	exprsp-=sizeof(long);
	return *(long *)exprsp;
}
int popb()
{
	return *--exprsp;
}

void rexpr(void) {
	exprsp=exprstack;
	exprflag=0;
	lastref=0;
	expr2();
	if(exprflag & 1) unbalanced();
	if(exprflag & 2) badoperation();
/*
	if(pass && exprtype==RELTYPE)
	{
		flags=((struct sym *)exprval)->symflags;
		if(flags & APUBLIC && ~flags&ADEF)
			printf("Reference to extrn %s\n",((struct sym *)exprval)->symtext);
	}
*/
}
void expr(void) {
	rexpr();
	if(exprtype==RELTYPE)
	{
		lastrel=(struct sym *)exprval;
		exprval=lastrel->symvalue+lastrel->symdelta;
	}
}
/*uchar opuchars[]={'+','-','/','*','|','&','<<','>>','!'};*/
void expr2(void) {
	pushb(0);
/*
	if(at()=='-')
	{
		forward1();
		pushl(0L);
		pushb(ABSTYPE);
		pushb(1);
		pushb(0x10);
	}
*/
	for(;;)
	{
		if(at()=='(')
		{
			forward1();
			expr2();
			if(get()!=')') {exprflag|=1;back1();}
			operval=exprval;
			opertype=exprtype;
		} else operand();
		operator();
		if(trytop()) break;
		pushl(operval);
		pushb(opertype);
		pushb(opop);
		pushb(oppri);
	}
	exprval=operval;
	exprtype=opertype;
	popb();
}
int trytop(void) {
	uchar toppri,topop,toptype;
	long topval;
	struct sym *sym1,*sym2;

	for(;;)
	{
		toppri=popb();
		if(oppri>toppri) {pushb(toppri);return oppri==8;}
		topop=popb();
		toptype=popb();
		topval=popl();
		switch(topop)
		{
		case 0: /* + */
			if(toptype==RELTYPE)
			{
				((struct sym *)topval)->symdelta+=operval;
				operval=topval;
			} else if(opertype==RELTYPE)
			{
				((struct sym *)operval)->symdelta+=topval;
			} else
				operval+=topval;
			opertype=cplus[toptype][opertype];
			break;
		case 1: /* - */
			if(toptype==RELTYPE)
			{
				if(opertype==RELTYPE)
				{
					sym1=(void *)topval;sym2=(void *)operval;
					if(pass && ~sym2->symflags & ADEF && sym2->symflags & APUBLIC)
					{
						opertype=ERR;
						break;
					}
					if(sym1->symflags & APUBLIC && ~sym1->symflags & ADEF)
					{
						sym1->symdelta-=sym2->symvalue+sym2->symdelta;
						operval=(long)sym1;
						opertype=RELTYPE;
					} else
					{
						operval=sym1->symvalue+sym1->symdelta - sym2->symvalue-sym2->symdelta;
						opertype=ABSTYPE;
					}
					break;
				}
				else
				{
					sym1=(void *)topval;
					sym1->symdelta-=operval;
					operval=(long)sym1;
				}
			} else
				operval=topval-operval;
			opertype=cminus[toptype][opertype];
			break;
		case 2: /* / */
			opertype=cdivide[toptype][opertype];
			if(!operval) {div0();operval=1;}
			operval=topval/operval;
			break;
		case 3: /* * */
			operval*=topval;
			opertype=cnone[toptype][opertype];
			break;
		case 4: /* | */
			operval|=topval;
			opertype=cnone[toptype][opertype];
			break;
		case 5: /* & */
			operval&=topval;
			opertype=cnone[toptype][opertype];
			break;
		case 6: /* << */
			operval=topval<<operval;
			opertype=cnone[toptype][opertype];
			break;
		case 7: /* >> */
			operval=topval>>operval;
			opertype=cnone[toptype][opertype];
			break;
		case 8: return 1;
		}
		if(opertype==ERR) {opertype=ABSTYPE;exprflag|=2;}
	}
}

void operator(void) {
	uchar ch;

	ch=get();
	switch(ch)
	{
		case '+': oppri=16;opop=0;break;
		case '-': oppri=16;opop=1;break;
		case '/': oppri=24;opop=2;break;
		case '*': oppri=24;opop=3;break;
		case '|': oppri=32;opop=4;break;
		case '&': oppri=40;opop=5;break;
		case '<':
			if(get()!='<') back1();
			oppri=48;opop=6;break;
		case '>':
			if(get()!='>') back1();
			oppri=32;opop=7;break;
		default:
			back1();oppri=8;opop=8;
	}
}

/*
+ 010
- 110
/ 218,20f
* 318
| 420
& 528
<< 630
>> 730
. , ( ) white ; 008
*/



/* fills in operval and opertype, leaves pointer on ucharacter stopped on */
void operand(void) {
	char ch;
	struct sym *pntr;
	char *p;

	ch=at();
	if(ch=='-')
	{
		forward1();
		operand();
		if(opertype==ABSTYPE) operval=-operval;
		else error2("Illegal use of unary -");
		return;
	} else if(ch=='~')
	{
		forward1();
		operand();
		if(opertype==ABSTYPE) operval=~operval;
		else error2("Illegal use of unary ~");
		return;
	} else if(isokfirst(ch))
	{
		token(&symbol);
		p=stext;
		ch=*p++;
		if((pntr=findsym(&symbol)))
		{
			opertype=pntr->symtype;
			if(opertype==RELTYPE)
			{
				operval=(long)pntr;
				if(pntr->symflags & ABSS)
					pntr->symdelta=-pntr->symvalue;
				else
					pntr->symdelta=0;
			} else
				operval=pntr->symvalue;
			if(pass && !(pntr->symflags&(APUBLIC|ADEF))) unknownerr(pntr->symtext);
		} else
		{
			operval=0;opertype=RELTYPE;
			symbol.symvalue=0;
			symbol.symtype=RELTYPE;
			symbol.symnum=0xffff;
			symbol.symflags=0;
			operval=(long)(pntr=addsym(&symbol));
			pntr->symdelta=0;
			if(pass)
				unknownerr(symbol.symtext);
		}
	} else if(isoknum(ch))
	{
		operval=0;
		while(isoknum((ch=get()))) {operval*=10;operval+=ch-'0';}
		opertype=ABSTYPE;
		back1();
		ch=tolower(ch);
		if(ch=='h' || (ch>='a' && ch<='f')) error2("Old fashioned 'h' stuff");
	} else if(ch=='$')
	{
		forward1();
		operval=0;
		while(ishex((ch=get()))) {operval<<=4;operval+=tohex(ch);}
		opertype=ABSTYPE;
		back1();
	} else if(ch=='\'')
	{
		forward1();
		opertype=ABSTYPE;
		operval=0;
		while((ch=get()))
		{
			if(ch==LF) {back1();unbalancedq();break;}
			if(ch=='\'')
				if(get()!='\'') {back1();break;}
			operval<<=8;operval+=ch;
		}
	} else if(ch=='*')
	{
		forward1();
		opertype=RELTYPE;
		operval=(long)heresym;
		heresym->symdelta=0;
	} else
	{
		operval=0;
		opertype=NOTHING;
	}
}


char *storeline(char *str) {
	char ch;
	do
	{
		ch=*str++;
		*textpoint++=ch;
	} while(ch!=10);
	return str-1;
}
void endstore(void) {
	*textpoint++=0;
	if((long)textpoint&1) textpoint++;
}
void addword(short val) {
	*(short *)textpoint=val;
	textpoint+=2;
}
void addpntr(void *val) {
	*(uchar **)textpoint=val;
	textpoint+=sizeof(void *);
}
void addlong(int32_t val) {
	*(int32_t *)textpoint=val;
	textpoint+=4;
}
void addtext(char *str) {
	int len;
	len=(strlen(str)+2)&0xfffe;
	bcopy(str,textpoint,len);
	textpoint+=len;
}

void predef(char *name, long value) {
	strcpy(symbol2.symtext,name);
	symbol2.symtype=ABSTYPE;
	symbol2.symvalue=value;
	symbol2.symunique=0;
	symbol2.symnum=0xffff;
	symbol2.symflags=ADEF;
	if(!findsym(&symbol2)) addsym(&symbol2);
}


int comma(void) {if(get()==',') return 0; else back1();syntaxerr();return 1;}
int numsign(void) {if(get()=='#') return 0; else back1();syntaxerr();return 1;}
int squote(void) {if(get()=='\'') return 0; else back1(); return 1;}

void dofiledirective(void) {}

void dodcb(void)
{
	uchar ch;
	for(;;)
	{
		ch=at();
		if(ch==LF) break;
		if(ch==',') {syntaxerr();return;}
		if(ch=='\'')
		{
			forward1();
			while((ch=get())!=LF)
			{
				if(ch=='\'')
				{
					if((ch=get())!='\'') break;
					bout(ch);continue;
				}
				bout(ch);
			}
			back1();
		} else
		{
			expr();
			bout((uchar)exprval);
		}
		ch=skipwhite();
		if(ch!=',') break;
		forward1();
		skipwhite();
	}
}
void checkref(int size) {
	struct sym *asym;

	if(exprtype==RELTYPE)
	{
		asym=(void *)exprval;
		exprval=asym->symvalue+asym->symdelta;
		if(pass) {
			if(asym->symflags & APUBLIC && ~asym->symflags & ADEF)
				addref(asym,(size==2 ? 0x83 : 0x81) | (en68000 ? 0 : 0x10));
			else
				addreloc(size);
		}
	}
}

void doworddata(void (*wo)(int32_t)) {
	uchar ch;
	for(;;)
	{
		rexpr();
		checkref(2);
		wo((int)exprval);
		ch=skipwhite();
		if(ch!=',') break;
		forward1();
		skipwhite();
	}
}

void dolongdata(void (*lo)(int32_t)) {
	uchar ch;
	for(;;)
	{
		rexpr();
		checkref(4);
		lo(exprval);
		ch=skipwhite();
		if(ch!=',') break;
		forward1();
		skipwhite();
	}
}


void dodw(void) {doworddata(outw);}
void dodcw(void) {doworddata(wout);}
void dodd(void) {dolongdata(outl);}
void dodcl(void) {dolongdata(lout);}



void dodsb(void)
{
	expr();
	zeros(exprval);
}
void dodsw(void)
{
	expr();
	zeros(exprval<<1);
}
void dodsl(void)
{
	expr();
	zeros(exprval<<2);
}

void zeros(long size) {
	if(size>MAXDB || size<0) outofrange();
	else while(size--) bout(0);
}

void doalign(void)
{
long v1,v2;
	expr();
	v1=exprval;
	v2=pcount%v1;
	if(v2) zeros(v1-v2);
}

/* all conditionals are between setyes and setno */
void setyes(void){if(depth==ydepth) ++ydepth;++depth;}
void doifeq(void){expr();if(!exprval) setyes(); else setno();}
void doifne(void){expr();if(exprval) setyes(); else setno();}
void doifge(void){expr();if(exprval>=0) setyes(); else setno();}
void doifgt(void){expr();if(exprval>0) setyes(); else setno();}
void doifle(void){expr();if(exprval<=0) setyes(); else setno();}
void doiflt(void){expr();if(exprval<0) setyes(); else setno();}
void doifc(void){if(!cnc()) setyes(); else setno();}
void doifnc(void){if(cnc()) setyes(); else setno();}
void doifd(void){if(dnd()) setyes(); else setno();}
void doifnd(void){if(!dnd()) setyes(); else setno();}
void doelse(void)
{
	if(!depth) illegalop();
	else
	{
		if(depth==ydepth) --ydepth;
		else
		if(depth==ydepth+1) ++ydepth;
	}
}
void doendc(void){if(!depth) illegalop();else {if(depth==ydepth) --ydepth;--depth;}}
void setno(void){++depth;}
/* all conditionals are between setyes and setno */
int dnd(void)
{
	struct sym *dndsym;
	token(&symbol2);
	dndsym=findsym(&symbol2);
	if(!dndsym) return 0;

	if(!pass) return 1;

	if(dndsym->symnum < symcount) return 0;
	return 1;
}
int fetchstr(char *str) {
	char ch;
	if(squote()) return 1;
	for(;;)
	{
		ch=get();
		if(ch==LF || ch==0) {back1();return 1;}
		*str++=ch;
		if(ch!='\'') continue;
		*--str=0;break;
	}
	return 0;
}
int cnc(void)
{
	char str1[80],str2[80];

	if(fetchstr(str1)) {syntaxerr();return 0;}
	if(comma()) return 0;
	if(fetchstr(str2)) {syntaxerr();return 0;}
	return strcmp(str1,str2);
}
void doorg(void){expr();pcount=exprval;}

void dobss(void)
{
	struct sym *s;
	uchar *t;

	xrdflag=1;
	if(pass) return;
	token(&symbol2);
	s=findsym(&symbol2);
	if(comma()) return;
	expr();
	if(s)
	{
		if(s->symflags&(ADEF|ABSS))
		{
			duplicate();
			return;
		}
		s->symvalue=blockcount;
		s->symtype=RELTYPE;
		s->symflags|=ABSS|APUBLIC;
		t=(void *)blockhead;
		blockhead=(void *)textpoint;
		addpntr(t);
		addpntr(s);
		s->sympublic=publichead;
		publichead=s;
	} else
	{
		symbol2.symvalue=blockcount;
		symbol2.symtype=RELTYPE;
		symbol2.symnum=symcount;
		symbol2.symflags=ABSS|APUBLIC;
		s=addsym(&symbol2);
		t=(void *)blockhead;
		blockhead=(void *)textpoint;
		addpntr(t);
		addpntr(s);
		s->sympublic=publichead;
		publichead=s;
	}
	somebss=1;
	if(exprval>1 && (blockcount&1)) blockcount++;
	blockcount+=exprval;
}

void dopublic(void)
{
struct sym *s;
	xrdflag=1;
	if(pass) return;
	for(;;)
	{
		skipwhite();
		token(&symbol2);
		s=findsym(&symbol2);
		if(s)
		{
			if(~s->symflags & APUBLIC)
			{
				s->symflags|=APUBLIC;
				s->sympublic=publichead;
				publichead=s;
			}
		} else
		{
			symbol2.symvalue=0;
			symbol2.symtype=RELTYPE;
			symbol2.symnum=symcount;
			symbol2.symflags=APUBLIC;
			s=addsym(&symbol2);
			s->sympublic=publichead;
			publichead=s;
		}
/*
		if(skipwhite()==':')
		{
			forward1();
			skipwhite();
			while(isoksym(at())) get();
		}
*/
		if(skipwhite()!=',') break;
		forward1();
	}
}

void doendm(void){;}
void domacro(void)
{
	struct sym *pntr;

	storing++;
	if(pass) return;
	pntr=findsym(&linelabel);
	pntr->symtype=MACTYPE;
	pntr->symvalue=(long)textpoint;
}
void doexpon(void){expanding=1;}
void doexpoff(void){expanding=0;}
void doinclude(void)
{
	char iname[80];
	char namesave[80];
	int linesave;
	char *macsave;
	char *pntr,ch,*p2;
	int	sfile;
	char *save;
	char *env;

	if(numopen==MAXIN) {error("Too many nested includes");return;}

	ch=at();
	if(ch!='\'' && ch!='"') {syntaxerr();return;}
	forward1();
	pntr=iname;
	for(;;)
	{
		ch=get();if(ch==LF) {back1();break;}
		if(ch=='\'' || ch=='"' || iswhite(ch)) break;
		*pntr++=ch;
	}
	*pntr=0;
	sfile=open(iname,O_RDONLY);
	if(sfile<0)
	{
		if((env=getenv(INCNAME)))
		{
			strcpy(namesave,iname);
			pntr=env;
			while(*pntr)
			{
				p2=iname;
				while(*pntr && *pntr!=';') *p2++=*pntr++;
				if(*pntr) pntr++;
				*p2=0;
				strcat(iname,namesave);
				if((sfile=open(iname,O_RDONLY))>=0) break;
			}
		}
		if(!sfile)
			{error("Cannot open file");return;}
	}
	close(sfile);
	strcpy(namesave,filename);
	strcpy(filename,iname);
	linesave=cline;

	save=inpoint;
	macsave=macstack;
	dofile();
	macstack=macsave;
	inpoint=save;
	strcpy(filename,namesave);
	cline=linesave;
}

void donds(void)
{
	token(&symbol2);
	if(comma()) return;
	expr();
	soffset-=exprval;
	symbol2.symvalue=soffset;
	symbol2.symtype=ABSTYPE;
	symbol2.symflags=ADEF;
	if(findsym(&symbol2))
		{if(!pass) duplicate();}
	else
		addsym(&symbol2);
}
void domds(void)
{
	token(&symbol2);
	if(comma()) return;
	expr();
	symbol2.symvalue=soffset;
	soffset+=exprval;
	symbol2.symtype=ABSTYPE;
	symbol2.symflags=ADEF;
	if(findsym(&symbol2))
		{if(!pass) duplicate();}
	else
		addsym(&symbol2);
}

void dostructure(void)
{
	token(&symbol2);
	if(comma()) return;
	expr();
	soffset=exprval;
}


void doinit(void)
{
	expr();
	soffset=exprval;
}

void varsize(size)
int size;
{
	token(&symbol2);
	symbol2.symvalue=soffset;
	soffset+=size;
	symbol2.symtype=ABSTYPE;
	symbol2.symflags=ADEF;
	symbol2.symunique=0;
	symbol2.symnum=0xffff;
	symbol2.symref=0;
	symbol2.sympublic=0;
	if(findsym(&symbol2))
		{if(!pass) duplicate();}
	else
		addsym(&symbol2);
}
void dolabel(void) {varsize(0);}
void dobyte(void) {varsize(1);}
void doword(void) {varsize(2);}
void dolong(void) {varsize(4);}

void doset(void)
{
	struct sym *pntr;

	expr();
	pntr=findsym(&linelabel);
	pntr->symtype=exprtype;
	pntr->symvalue=exprval;
}

void doequ(void)
{
	struct sym *pntr;

	expr();
	pntr=findsym(&linelabel);
	pntr->symtype=exprtype;
	if(pass && pntr->symvalue!=exprval) phaserr();
	pntr->symvalue=exprval;
}

void doeven(void) {if(pcount&1) bout(0);}
void dosteven(void) {if(soffset&1) soffset++;}

void headout(void)
{
	long temp;

	temp=pcmax;
	if(!xrdflag)
	{
		lout(0x3f3L);
		lout(0L);
		lout(1L);
		lout(0L);
		lout(0L);
		lout(temp>>2);
	}
/*
	else
	{
		lout(0x3e7L);
		strcpy(txt,inputname);
		strcat(txt,".o");
		p2=txt;
		while(*p2) p2++;
		while(p2>txt)
		{
			ch=*--p2;
			if(ch!='/' && ch!=':' && ch!='\\') continue;
			++p2;break;
		}
		p1=p2;
		while(*p1) p1++;
		while((p1-p2)&3) *p1++=0;
		t=p1-p2;
		lout((long)t>>2);
		p1=p2;
		while(t--) bout(*p1++);
	}
*/
	lout(0x3e9L);
	lout(temp>>2);
}
void tailout(void){lout(0x3f2L);} /* hunk_end */
void symout(char *s,int d) {
	int j;
	char text[80],*p,ch;

	p=text;
	while((ch=*s++)) *p++=ch;
	while((p-text)&3) *p++=0;
	j=p-text;
	p=text;
	bout(d);
	wout(0);
	bout(j>>2);
	while(j--) bout(*p++);
}
void symsout(int mask)
{
	int i;
	struct sym *pntr;

	lout(0x3f0L);
	for(i=0;i<256;i++)
	{
		pntr=headers[i];
		while(pntr)
		{
			if(pntr->symtype==RELTYPE && !pntr->symunique && pntr->symflags & mask)
			{
				symout(pntr->symtext,0);
				lout(pntr->symvalue);
			}
			pntr=pntr->symnext;
		}
	}
	lout(0L);
}

void addblock(void *p, int len) {
	memcpy(textpoint, p, len);
	textpoint += len;
}
void addref(struct sym *asym, int type) {
	struct reference r;
	r.refnext = asym->symref;
	r.refoff = pcount;
	r.reftype = type;
	asym->symref=(void *)textpoint;
	addblock(&r, sizeof(r));
}
void addreloc(int size) {
	struct reference r;
	r.refoff = pcount;
	if(size==2)
	{
		r.refnext = reloc16head;
		r.reftype = 2;
		reloc16head=(void *)textpoint;
		++reloc16num;
	} else
	{
		r.refnext = reloc32head;
		r.reftype = 4;
		reloc32head=(void *)textpoint;
		++reloc32num;
	}
	addblock(&r, sizeof(r));
	
}
void outreloclist(int hunktype, struct reference *head, int num) {
	if(!num) return;
	lout(hunktype);
	lout(num);
	lout(0);
	while(head)
	{
		lout(head->refoff);
		head=head->refnext;
	}
	lout(0);
}
void outrelocs(void) {
	if(en68000) {
		outreloclist(0x3ed,reloc16head,reloc16num);
		outreloclist(0x3ec,reloc32head,reloc32num);
	} else {
		outreloclist(0x3cd,reloc16head,reloc16num);
		outreloclist(0x3cc,reloc32head,reloc32num);
	}
}

void outbss(void) {
	struct sym *asym;

	lout(0x3eb);
	lout((blockcount+3)>>2);
	if(options['s'])
		symsout(ABSS);
	lout(0x3efL); /* hunk_ext */
	asym=publichead;
	while(asym)
	{
		if(asym->symflags & ABSS)
		{
			symout(asym->symtext,1);
			lout(asym->symvalue);
		}
		asym=asym->sympublic;
	}
	lout(0);
	lout(0x3f2);
}


void outpublics(void)
{
struct sym *asym;
struct reference *aref,*ref2,*t;
int count,type;

	if(!publichead) return;
	lout(0x3efL); /* hunk_ext */
	asym=publichead;
	while(asym)
	{

		if(asym->symflags & ADEF)
		{
			symout(asym->symtext,1);
			lout(asym->symvalue);
		}
		asym=asym->sympublic;
	}
	asym=publichead;
	while(asym)
	{
		if(~asym->symflags & ADEF)
		{
			while((aref=asym->symref))
			{
				type=aref->reftype;
				count=0;
				while(aref)
				{
					if(aref->reftype==type) count++;
					aref=aref->refnext;
				}

				symout(asym->symtext,type);
				lout((long)count);
				ref2=0;
				aref=asym->symref;
				while(aref)
				{
					t=aref;
					aref=aref->refnext;
					if(t->reftype==type) lout(t->refoff);
					else
					{
						t->refnext=ref2;
						ref2=t;
					}
				}
				asym->symref=ref2;
			}
		}
		asym=asym->sympublic;
	}
	lout(0L);
}
void docnop(void)
{
int firstval,secondval;

	expr();
	firstval=exprval;
	if(comma()) return;
	expr();
	secondval=exprval;
	if(firstval>=secondval || secondval==0)
	{
		outofrange();
		return;
	}
	while(pcount%secondval!=firstval)
		bout(0);
}



void donothing(void) {}

struct anopcode directs[]={
{"align",doalign},
{"aptr",dolong},
{"bool",doword},
{"bptr",dolong},
{"bss",dobss},
{"byte",dobyte},
{"cnop",docnop},
{"cptr",doword},
{"dc",dodcw},
{"dc.b",dodcb},
{"dc.l",dodcl},
{"dc.w",dodcw},
{"dd",dodd},
{"ds",dodsw},
{"ds.b",dodsb},
{"ds.l",dodsl},
{"ds.w",dodsw},
{"dw",dodw},
{"else",doelse},
{"end",donothing},
{"endc",doendc},
{"endm",doendm},
{"equ",doequ},
{"even",doeven},
{"expoff",doexpoff},
{"expon",doexpon},
{"fail",failerr},
{"file",dofiledirective},
{"float",dolong},
{"if",doifne},
{"ifc",doifc},
{"ifd",doifd},
{"ifeq",doifeq},
{"ifge",doifge},
{"ifgt",doifgt},
{"ifle",doifle},
{"iflt",doiflt},
{"ifnc",doifnc},
{"ifnd",doifnd},
{"ifne",doifne},
{"include",doinclude},
{"init",doinit},
{"label",dolabel},
{"lbss",dobss},
{"long",dolong},
{"m68000",dom68000},
{"macro",domacro},
{"mds",domds},
{"nds",donds},
{"org",doorg},
{"public",dopublic},
{"rptr",dolong},
{"set",doset},
{"short",doword},
{"space",dodcb},
{"steven",dosteven},
{"struct",domds},
{"structure",dostructure},
{"ubyte",dobyte},
{"ulong",dolong},
{"ushort",doword},
{"uword",doword},
{"word",doword},
{"x86",dox86},
{"z80",doz80},
{0}};



void (*scan(char *code, struct oplist *list))(void) {
	struct anopcode *op;
	struct anopcode *table;
	int num,power;
	int way,n;
	table=list->listopcodes;
	num=list->numops;
	power=list->powerops;
	n=power-1;
	op=table+n;
	for(;;)
	{
		power>>=1;
		if(n>=num) way=-1;
		else
			if(!(way=strcmp(code,op->opcodename))) return op->opcodehandler;
		if(!power) return 0;
		if(way<0) {op-=power;n-=power;}
		else {op+=power;n+=power;}
	}
}
void outl(int32_t val)
{
	outw((short)val);
	outw((short)(val>>16));
}
void lout(int32_t val)
{
	wout((short)(val>>16));
	wout((short)val);
}
void outw(int32_t val)
{
	bout(val);
	bout(val>>8);
}
void wout(int32_t val)
{
	bout(val>>8);
	bout(val);
}
void bout(uchar val)
{
	*outoff++=val;
	pcount++;
	pcmax++;
}


struct sym *findsym(struct sym *asym)
{
	struct sym *pntr;
	char *p;

	p=asym->symtext;
	pntr=headers[hash8(p)];
	while(pntr && strcmp(pntr->symtext,p))
		pntr=pntr->symnext;
	return pntr;
}
int token(struct sym *asym) {
	char *pntr;
	int endch;
	pntr=asym->symtext;
	while(isoksym(endch=get()))
		*pntr++=endch;
	*pntr=0;
	return back();
}
int gather(char *str) {
	uchar ch;
	while(isokop(ch=get())) *str++=ch;
	*str=0;
	return back();
}
int skipwhite(void) {
	while(iswhite(get()));
	return back();
}
void removecr(char *where)
{
	char *p,ch;
	p=where;
	while((*p++=ch=*where++))
		if(ch==13) p--;
}
