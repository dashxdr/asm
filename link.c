#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define INCNAME "LINKDIR"
#define DATABLOCKSIZE 0x40000L
#define IOBLOCKSIZE 30000
#define OUTBLOCKSIZE 0x80000L
#define MAXIN 256
#define MAXHUNKS 1024
#define CODE 0
#define BSS 1
#define UNDEF 255
#define MAXFILES 1024
#define MAXDIRS 64

struct define {
	struct define *defnext;
	char *deftext;
	int deftype;
	int defhunk;
	int32_t defoffset;
	struct use *defuse;
};
struct use {
	struct use *usenext;
	int usetype;
	int usehunk;
	int32_t useoffset;
};

struct sym {
	struct sym *symnext;
	char *symtext;
	int32_t symval;
	int symhunk;
	int sympad;
};
struct input
{
	char inputname[80];
	char *inputloc;
};
struct hunk
{
	int hunktype;
	int32_t codelen;
	int32_t bsslen;
	int32_t codedelta;
	int32_t bssdelta;
};


void patch(int thishunk,int num);
void fix(int which);
void handlefile(char *str);
void symout(char *str);
void dosymbols(void);
void scan(char *str);

int unresolved=0;

int hunkcount,hunkmax;

char *inputnames[MAXFILES];
char *dirnames[MAXDIRS];
int inputnum;
int dirnum;

char *TOPCODE="_topcode";
char *TOPBSS="_topbss";
struct define *predef(char *, int32_t);

int infile;
int outfile;
char *filepoint;
char *fileend;
struct input *ins=0;
struct hunk *hunks=0;
char *datablock=0;
char *datapoint;
char *ioblock=0;
int incount;
char scr1[80];
struct define **headers=0;
char outputname[256];
int32_t *longp;
short *wordp;
char multflag=1;

char *outbuff=0;
char *outpoint,*outend;

char *codeloc;

int32_t maxcode;
int32_t maxbss;
int32_t dummy;
#define OPTH 1
#define OPTS 2
#define OPTU 4
#define OPTF 8
#define OPTM 16
struct sym *symhead;
struct define *topcode,*topbss;
char *inctext;
char options[256];


void skip(int32_t len) {
	filepoint+=len<<2;
}

struct define *newdef()
{
	struct define *t;
	t=(void *)datapoint;
	datapoint+=sizeof(struct define);
	return t;
}
struct sym *newsym()
{
	struct sym *t;
	t=(void *)datapoint;
	datapoint+=sizeof(struct sym);
	return t;
}
struct use *newuse()
{
	struct use *t;
	t=(void *)datapoint;
	datapoint+=sizeof(struct use);
	return t;
}
char *addtext(char *str) {
	char *t;
	int len;
	t=datapoint;
	len=strlen(str)+1;
	len+=len&1;
	datapoint+=len;
	strcpy(t,str);
	return t;
}

int revword(unsigned int val) {
	return ((val&0xff00)>>8) | ((val&0x00ff)<<8);
}
uint32_t revlong(uint32_t val) {
	return ((val&0xff000000)>>24) | ((val&0xff0000)>>8) | ((val&0xff00)<<8) | ((val&0xff)<<24);
}

void lout(uint32_t val) {
	*(uint32_t *)outpoint=revlong(val);
	outpoint+=4;
}

void outw(short val) {
	*(short *)outpoint=val;
	outpoint+=2;
}

int32_t read4(void) {
	int32_t val;

	val=((filepoint[0]&255)<<24) | ((filepoint[1]&255)<<16) | 
			((filepoint[2]&255)<<8) | (filepoint[3]&255);
	filepoint+=4;
	return val;
}

void symbol(int v) {
	int t;

	t=v<<2;

	bcopy(filepoint,scr1,t);
	filepoint+=t;
	scr1[t]=0;
}

int hash8(char *str)
{
	int hash=0;
	char ch;

	while((ch=*str++))
	{
		hash+=hash;
		if(hash&256) hash-=255;
		hash^=ch&0xff;
	}
	return hash;
}

struct define *finddef(char *str) {
	struct define *h;

	h=headers[hash8(str)];
	while(h)
	{
		if(!strcmp(str,h->deftext)) break;
		h=h->defnext;
	}
	return h;
}


void freeind(void **addr) {
	if(*addr)
	{
		free(*addr);
		*addr=0;
	}
}

void freestuff(void) {
	int i;
	if(ins)
	{
		for(i=0;i<incount;i++)
			freeind((void **)&ins[i].inputloc);
	}
	for(i=0;i<inputnum;i++)
		freeind((void **)inputnames+i);
	for(i=0;i<dirnum;i++)
		freeind((void **)dirnames+i);

	freeind((void **)&ins);
	freeind((void **)&ioblock);
	freeind((void **)&datablock);
	freeind((void **)&outbuff);
	freeind((void **)&headers);
	freeind((void **)&hunks);
}

void nomem(void) {
	freestuff();
	printf("Out of memory\n");
	exit(1);
}

void missing(char *str) {
	if(!unresolved)
		printf("Unresolved references:\n");
	printf("%s\n",str);
	unresolved++;
}

void addname(char *name) {
	char *p;
		if(inputnum<MAXFILES && (p=malloc(strlen(name)+1)))
		{
			inputnames[inputnum++]=p;
			strcpy(p,name);
		}
}
void adddirname(char *name) {
	char *p;
	int i;
		if(dirnum<MAXFILES && (p=malloc(strlen(name)+1)))
		{
			dirnames[dirnum++]=p;
			strcpy(p,name);
			while((i=strlen(p)) && p[i-1]=='/') p[i-1]=0;
		}
}

void setup(int argc, char **argv)
{
	int i,j,mode;
	char ch;

	inputnum=0;
	dirnum=0;
	strcpy(outputname,"a.out");
	for(i=0;i<256;i++) options[i]=0;
	options['s']=1;
	for(i=1,mode=0;i<argc;i++)
	{
reswitch:
		switch(mode)
		{
		case 0:
			if(argv[i][0]!='-')
			{
				addname(argv[i]);
				break;
			}
			mode=argv[i][1];
			if(argv[i][2]) {j=2;goto reswitch;}
			else {j=0;continue;}
		case 'o':
			strcpy(outputname,argv[i]+j);
			break;
		case '-':
			while((ch=argv[i][j++])) options[ch*0x7f]^=1;
			break;
		case 'f':
			handlefile(argv[i]+j);
			break;
		case 'L':
			adddirname(argv[i]+j);
			break;
		default:
			mode=0;
			break;
		}
		mode=0;
	}
}


int main(int argc, char **argv) {
	int i;
	struct define *d;

	if(argc<2)
	{
		printf("LINK Copyright (c) 1997 by David Ashley\n");
		printf("Use: link [--OPTIONS] [-f <filelist>] [-o output] inputfile...\n");
		printf("m   Warn of multiple definitions\n");
		printf("s   Pass symbol table information through\n");
		printf("u   Display usage information\n");
		return 0;
	}

	incount=0;
	inputnum=0;
	dirnum=0;
	ins=malloc(MAXIN*sizeof(struct input));
	if(!ins) nomem();
	hunks=malloc(MAXHUNKS*sizeof(struct hunk));
	if(!hunks) nomem();
	bzero(ins,MAXIN*sizeof(struct input));
	datablock=malloc(DATABLOCKSIZE);
	if(!datablock) nomem();
	datapoint=datablock;
	outbuff=malloc(OUTBLOCKSIZE);
	if(!outbuff) nomem();
	outpoint=outbuff;
	outend=outbuff+OUTBLOCKSIZE;
	ioblock=malloc(IOBLOCKSIZE);
	if(!ioblock) nomem();
	headers=malloc(256*sizeof(struct define *));
	if(!headers) nomem();

	symhead=0;
	for(i=0;i<256;i++) headers[i]=0;

	hunkcount=0;
	topcode=predef(TOPCODE,0);
	topbss=predef(TOPBSS,0);

	setup(argc,argv);

/*
	if(~options['h'])
		scan("_start");
*/

	multflag=1;
	for(i=0;i<inputnum;i++)
		scan(inputnames[i]);
/*
	for(i=1;i<argc;i++)
	{
		if(argv[i][0]!='-')
			scan(argv[i]);
		else if(argv[i][1]=='f')
			handlefile(argv[i]+2);
	}
*/
	multflag=0;
/*	if(~options['h'])
		scan("_lib");*/
#ifdef DEBUG
	for(i=0;i<incount;i++)
	{
		printf("file:%s\n",ins[i].inputname);
		printf(" CODE=%08lx,BSS=%08lx\n",ins[i].codelen,ins[i].bsslen);
	}
#endif
	unresolved=0;
	for(i=0;i<256;i++)
	{
		if((d=headers[i]))
			while(d)
			{
				if(d->deftype==UNDEF)
					missing(d->deftext);
				d=d->defnext;
			}
	}
	if(unresolved) goto abort;
	if(!incount || !hunkcount) {printf("Nothing to do\n");goto abort;}

	maxbss=0;
	maxcode=0;
	hunkmax=hunkcount;

	for(i=0;i<hunkmax;i++)
	{
		hunks[i].codedelta=maxcode;
		maxbss-=hunks[i].bsslen;
		hunks[i].bssdelta=maxbss;
		maxcode+=hunks[i].codelen;
	}

	topcode->defoffset=maxcode;
	topbss->defoffset=maxbss;

	outfile=creat(outputname,0700);
	if(outfile<0) {printf("Cannot open output file %s\n",outputname);goto abort;}

	lout(0x3f3);
	lout(0);
	lout(1);
	lout(0);
	lout(0);
	lout(maxcode>>2);
	lout(0x3e9);
	lout(maxcode>>2);

#if 0
	exelen=maxcode+0x20;
	i=exelen&0x1ff;
	j=exelen >> 9;
	if(!i) i=0x200;
	else j++;
	outw(0x5a4d); /* EXE sig */
	outw(i); /* # of bytes in last page */
	outw(j); /* # of 512 byte pages */
	outw(0); /* # of reloc items */
	outw(2); /* # of paragraphs of header */
	outw(0x400); /* min alloc */
	outw(0xffff); /* max alloc */
	outw((short)(maxcode+15>>4)); /* ss */
	outw(0); /* sp */
	outw(0); /* checksum */
	outw(0); /* PC */
	outw(0); /* cs */
	outw(0); /* offset to reloc tab */
	outw(0); /* overlay # */
	lout(0x44415645L);
#endif

	hunkcount=0;
	for(i=0;i<incount;i++) fix(i);
	if(options['s']) dosymbols();

	lout(0x3f2);
	int res=write(outfile,outbuff,outpoint-outbuff);res=res;
	close(outfile);

#ifdef DEBUG
	for(i=0;i<256;i++)
		if(d=headers[i])
			while(d)
			{
				printf("%08lx ",ins[d->defhunk].codedelta+d->defoffset);
				printf(d->deftext);
				putchar('\n');
/*				t1=d->defuse;
				while(t1)
				{
					printf(" %d:%08lx",t1->usehunk,t1->useoffset);
					t1=t1->usenext;
				}
				putchar('\n');*/
				d=d->defnext;
			}
#endif

abort:
	if(options['u'])
	{
		printf("%08lx Memory used for data\n",datapoint-datablock);
		printf("Code:%06x BSS:%06x\n",maxcode,maxbss);
	}
	freestuff();
	exit(0);
}

void dosymbols(void) {
	int i;
	int32_t base;
	int type,num;
	int count;
	struct define *d,*od;

	count=0;
	for(i=0;i<256;i++)
	{
		d=headers[i];
		while(d)
		{
			od=d;
			d=d->defnext;
			type=od->deftype;
			num=od->defhunk;
			if(type==CODE) base=hunks[num].codedelta;
			else if(type==BSS) base=hunks[num].bssdelta;
			else continue;
			base+=od->defoffset;
			if(!count) lout(0x3f0L);
			count++;
			symout(od->deftext);
			lout(base);
		}
	}
	while(symhead)
	{
		if(!finddef(symhead->symtext))
		{
			if(!count) lout(0x3f0L);
			count++;
			symout(symhead->symtext);
			lout(symhead->symval+hunks[symhead->symhunk].codedelta);
		}
		symhead=symhead->symnext;
	}
	if(count) lout(0L);
}

void symout(char *str) {
	int len;
	len=strlen(str);
	*outpoint++=0;
	*outpoint++=0;
	*outpoint++=0;
	*outpoint++=(len+3)>>2;
	bcopy(str,outpoint,len);
	outpoint+=len;
	while(len++ &3) *outpoint++=0;
}

#define FILEMAX 4096
void handlefile(char *str) {
	int file;
	char filearea[FILEMAX+1];
	int len;
	char name[256],ch,*p,*p2;

	file=open(str,O_RDONLY);
	if(file<0)
	{
		printf("Cannot open parameter file %s\n",str);
		return;
	}
	len=read(file,filearea,FILEMAX);
	close(file);
	filearea[len]=0;
	if(len==FILEMAX)
		printf("Parameter file %s truncated to %d bytes\n",str,FILEMAX);
	p=filearea;
	for(;;)
	{
		while((ch=*p) && (ch==' ' || ch==13 || ch==10 || ch==9)) p++;
		if(!*p) break;
		p2=name;
		while((ch=*p) && ch!=' ' && ch!=13 && ch!=10 && ch!=9) *p2++=*p++;
		*p2=0;
		addname(name);
	}
}


void scan(char *str) {
	int32_t v1;
	int t,type;
	struct use *t2;
	int htype=0;
	int hashval;
	struct define *def1;
	char iname[256];
	char namecopy[64];
	int32_t filelen;
	struct define *adef;
	struct sym *asym;
	struct use *ause;
	int thishunk=0;
	int thisfile;
	int i;

	i=strlen(str);
	if(i>2 && !strcmp(str+i-2,".o"))
		strcpy(namecopy,str);
	else
		sprintf(namecopy,"%s.o",str);
	strcpy(ins[incount].inputname,namecopy);
	thisfile=incount;

	infile=open(namecopy,O_RDONLY);
	if(infile<0)
	{
		for(i=0;i<dirnum;i++)
		{
			sprintf(iname,"%s/%s",dirnames[i],namecopy);
			infile=open(iname,O_RDONLY);
			if(infile>=0)
				break;
		}
		if(infile<0)
		{
			printf("Unable to open file %s\n",namecopy);
			return;
		}
		strcpy(ins[incount].inputname,iname);
	}

	filelen=lseek(infile,0L,2);
	lseek(infile,0L,0);
	filepoint=malloc(filelen);
	if(!filepoint) nomem();
	fileend = filepoint + filelen;
	ins[incount].inputloc=filepoint;
	int res=read(infile,filepoint,filelen);res=res;
	close(infile);
	incount++;

	for(;;)
	{
		if(filepoint+4>fileend) break;
		v1=read4();
		switch((int)v1)
		{
		case 0x3f3:
			printf("Cannot link executable %s\n",str);
			return;
		case	0x3eb: /* hunk_bss */
			thishunk=hunkcount++;
			htype=BSS;
			hunks[thishunk].codelen=0;
			hunks[thishunk].bsslen=read4()*4L;
			break;
		case 0x3f2: /* hunk_end */
			break;
		case 0x3e8: /* hunk_name */
			skip(read4());
			break;
		case 0x3ea: /* hunk_data */
		case 0x3e9: /* hunk_code */
			thishunk=hunkcount++;
			htype=CODE;
			v1=read4();
			hunks[thishunk].codelen=v1<<2;
			hunks[thishunk].bsslen=0;
			skip(v1);
			break;
		case 0x3cd: /* hunk_16reloc */
		case 0x3cc: /* hunk_32reloc */
		case 0x3ed: /* hunk_reloc16 */
		case 0x3ec: /* hunk_reloc32 */
			while((v1=read4())) skip(v1+1);
			break;
		case 0x3f0: /* hunk_symbol */
			while((v1=read4()))
			{
				symbol((int)v1);
				asym=newsym();
				asym->symnext=symhead;
				asym->symtext=addtext(scr1);
				asym->symval=read4();
				asym->symhunk=thishunk;
				symhead=asym;
			}
			break;
		case	0x3f1: /* hunk_debug */
			v1=read4(); skip(v1);
			break;
		case 0x3e7: /* hunk_unit */
			if((v1=read4())) skip(v1);
			break;
		case 0x3ef: /* hunk_ext */
			while((v1=read4()))
			{
				type=(uint32_t)v1>>24;
				v1&=0xffffffL;
				symbol((int)v1);
				switch(type)
				{
				case 1: /* def */
					def1=finddef(scr1);
					if(def1)
					{
						v1=read4();
						if(def1->deftype==UNDEF)
						{
							def1->defoffset=v1;
							def1->defhunk=thishunk;
							def1->deftype=htype;
						}
						else
							if(options['m'] || multflag) printf("Multiply defined symbol %s\n",scr1);
					}
					else
					{
						hashval=hash8(scr1);
						adef=newdef();
						adef->defnext=headers[hashval];
						headers[hashval]=adef;
						adef->deftext=addtext(scr1);
						adef->deftype=htype;
						adef->defhunk=thishunk;
						adef->defoffset=read4();
						adef->defuse=0;
					}
					break;
				case 0x91: /* 32ref */
				case 0x93: /* 16ref */
				case 0x99: /* 32rel */
				case 0x9b: /* 16rel */
				case 0x81: /* ref32 129 */
				case 0x83: /* ref16 131 */
				case 0x8b: /* rel16 139 was 140?? */
					def1=finddef(scr1);
					if(!def1)
					{
						def1=newdef();
						hashval=hash8(scr1);
						def1->defnext=headers[hashval];
						headers[hashval]=def1;
						def1->deftext=addtext(scr1);
						def1->deftype=UNDEF;
						def1->defuse=0;
					}
					t=read4();
					t2=def1->defuse;
					while(t--)
					{
						ause=newuse();
						ause->usenext=t2;
						t2=ause;
						ause->usetype=type;
						ause->usehunk=thishunk;
						ause->useoffset=read4();
					}
					def1->defuse=t2;
					break;
				}
			}
			break;
		default:
			if(v1==0xffffffffL) return;
			printf("Unknown code %08x in file %s\n",
				v1,ins[thisfile].inputname);
			return;
		}
	}
}
struct define *predef(char *name, int32_t value)
{
int hashval;
struct define *adef;

	hashval=hash8(name);
	adef=newdef();
	adef->defnext=headers[hashval];
	headers[hashval]=adef;
	adef->deftext=addtext(name);
	adef->deftype=1;
	adef->defhunk=0;
	adef->defoffset=value;
	adef->defuse=0;
	return adef;
}


void fix(int which) {
	int32_t v1,v2,v3;
	int htype=0,t;
	int thishunk=0;
	int starthunk;

	starthunk=hunkcount;
	filepoint=ins[which].inputloc;
	for(;;)
	{
		v1=read4();
		switch((int)v1)
		{
		case	0x3eb: /* hunk_bss */
			thishunk=hunkcount++;
			htype=BSS;
			read4();
			break;
		case 0x3f2: /* hunk_end */
			if(htype==CODE)
			{
				v1=hunks[thishunk].codelen;
				bcopy(codeloc,outpoint,v1);
				outpoint+=v1;
			}
			break;
		case 0x3e8: /* hunk_name */
			skip(read4());
			break;
		case 0x3ea: /* hunk_data */
		case 0x3e9: /* hunk_code */
			thishunk=hunkcount++;
			htype=CODE;
			v1=read4();
			codeloc=filepoint;
			filepoint+=v1<<2;
			patch(thishunk,which);
			break;
		case 0x3cd: /* hunk_16reloc */
			while((v1=read4()))
			{
				v2=read4();
				v2=hunks[starthunk+v2].codedelta;
				while(v1--)
				{
					v3=read4();
					wordp=(void *)(codeloc+v3);
					*wordp+=v2;
				}
			}
			break;
		case 0x3ed: /* hunk_reloc16 */
			while((v1=read4()))
			{
				v2=read4();
				v2=hunks[starthunk+v2].codedelta;
				while(v1--)
				{
					v3=read4();
					wordp=(void *)(codeloc+v3);
					*wordp=revword(revword(*wordp)+v2);
				}
			}
			break;
		case 0x3cc: /* hunk_32reloc */
			while((v1=read4()))
			{
				v2=read4();
				v2=hunks[starthunk+v2].codedelta;
				while(v1--)
				{
					v3=read4();
					longp=(void *)(codeloc+v3);
					*longp+=v2;
				}
			}
			break;
		case 0x3ec: /* hunk_reloc32 */
			while((v1=read4()))
			{
				v2=read4();
				v2=hunks[starthunk+v2].codedelta;
				while(v1--)
				{
					v3=read4();
					longp=(void *)(codeloc+v3);
					*longp=revlong(revlong(*longp)+v2);
				}
			}
			break;
		case 0x3f0: /* hunk_symbol */
			while((v1=read4())) skip(v1+1);
			break;
		case	0x3f1: /* hunk_debug */
			v1=read4(); skip(v1);
			break;
		case 0x3e7: /* hunk_unit */
			if((v1=read4())) skip(v1);
			break;
		case 0x3ef: /* hunk_ext */
			while((v1=read4()))
			{
				t=(uint32_t)v1>>24;
				v1&=0xffffffL;
				skip(v1);
				switch(t)
				{
				case 1: /* def */
				case 2:
				case 3:
					read4();
					break;
				case 0x81: /* ref32 129 */
				case 0x83: /* ref16 131 */
				case 0x84: /* ref8 132 */
				case 0x8b: /* rel16 140 */
				case 0x91: /* 32ref */
				case 0x93: /* 16ref */
				case 0x99: /* 32rel */
				case 0x9b: /* 16rel */
				default:
					v1=read4();
					skip(v1);
					break;
				}
			}
			break;
		default:
			return;
		}
	}
}

void rangeerr(int num, int loc, struct define *def) {
	printf("Range error in %s,offset %x, symbol=%s\n",
		ins[num].inputname,loc,def->deftext);
}

void patch(int thishunk,int num)
{
	int i,t,n;
	struct define *d;
	int32_t loc;
	int32_t dest,t2;
	struct use *t1;
	int32_t thishunkbase;

	for(i=0;i<256;i++) {
		d=headers[i];
		while(d)
		{
			n=d->defhunk;
			t=d->deftype;
			if(t==CODE) dest=hunks[n].codedelta;
			else if(t==BSS) dest=hunks[n].bssdelta;
			else continue;
			thishunkbase=hunks[thishunk].codedelta;
			dest+=d->defoffset;

			t1=d->defuse;
			while(t1)
			{
				if(t1->usehunk==thishunk)
				{
					loc=t1->useoffset;
					t=t1->usetype;
					switch(t)
					{
					case 0x91: /* 32ref */
						longp=(void *)(codeloc+loc);
						*longp+=dest;
						break;
					case 0x93: /* 16ref */
						wordp=(void *)(codeloc+loc);
						t2=(unsigned)*wordp+dest;
						if(t2<0 || t2>0xffffL)
							rangeerr(num,loc,d);
						*wordp=t2;
						break;
					case 0x99: /* 32rel */
						longp=(void *)(codeloc+loc);
						*longp+=dest-thishunkbase;
						break;
					case 0x9b: /* 16rel */
						wordp=(void *)(codeloc+loc);
						t2=*wordp+dest-thishunkbase;
						if(t2<-0x8000L || t2>0x7fffL)
							rangeerr(num,loc,d);
						*wordp=t2;
						break;
					case 0x81: /* ref32 */
						longp=(void *)(codeloc+loc);
						*longp=revlong(revlong(*longp)+dest);
						break;
					case 0x83: /* ref16 */
						wordp=(void *)(codeloc+loc);
						t2=revword(*wordp)+dest;
						if(t2<-0x8000L || t2>0x7fffL)
							rangeerr(num,loc,d);
						*wordp=revword((short)t2);
						break;
					case 0x8b: /* rel16 was 8c??? */
						wordp=(void *)(codeloc+loc);
						t2=revword(*wordp)+dest-loc-thishunkbase;
						if(t2<-0x8000L || t2>0x7fffL)
							rangeerr(num,loc,d);
						*wordp=revword((short)t2);
						break;
					}
				}
				t1=t1->usenext;
			}
			d=d->defnext;
		}
	}
}
