#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <unistd.h>

typedef unsigned char uchar;

#define USEHIGH    ; whether to use low level ASM code
#define M68000 0
#define Z80 1
#define X86 2
#define MAXIN 3
#define EXPRMAX 256
#define MACDEPTH 10
#define LF 10
#define ATTRWHITE 1
#define ATTRSYM 2
#define ATTRFSYM 4
#define ATTROP 8
#define ATTRNUM 16
#define ATTRHEX 32
#define CHUNK 0xffff
#define LOCAL '.'
/* leave 1.5K for macro expansion */
#define MACSIZE 0x600
#define MAXDB 65536L
#define SYMMAX 64

#define iswhite(ch) (attr[ch&255] & ATTRWHITE)
#define isokfirst(ch) (attr[ch&255] & ATTRFSYM)
#define isoksym(ch) (attr[ch&255] & ATTRSYM)
#define isokop(ch) (attr[ch&255] & ATTROP)
#define isoknum(ch) (attr[ch&255] & ATTRNUM)
#define ishex(ch) (attr[ch&255] & ATTRHEX)
#define tohex(ch) (ch<'A' ? ch-'0' : (ch&15)+9)

#define ERR 99
#define ABSTYPE 0
#define RELTYPE 1
#define REGTYPE 2
#define XREFTYPE 3
#define MACTYPE 4
#define NOTHING 98

struct anopcode {
	char *opcodename;
	void (*opcodehandler)();
};

struct reference {
	struct reference *refnext;
	int32_t refoff;
	short reftype;
};

struct sym {
	char *symtext;
	struct sym *symnext;
	long symvalue;
	uchar symtype;
	uchar symflags;
	unsigned int symnum;
	unsigned int symunique;
	struct reference *symref;
	struct sym *sympublic;
	int32_t symdelta;
};
/* sym flags */
#define APUBLIC 1
#define ADEF 2
#define ABSS 4

struct oplist
{
	struct anopcode *listopcodes;
	int numops,powerops;
};

extern struct oplist z80list,scanlist,directlist,x86list,*currentlist;
extern struct anopcode scantab[];
extern struct anopcode z80codes[];
extern struct anopcode directs[];
extern struct anopcode x86codes[];

extern long exprval; /* expr fills in */

extern uchar exprstack[EXPRMAX];
extern uchar *exprsp;

extern int eaword1; /* effectaddr fills in */
extern int eaword2;
extern int ealen;
extern int eabits;
extern int eaop;
extern int earel;

extern long operval;
extern long soffset;
extern struct sym *nextsym;
extern unsigned nexttext;
extern unsigned maxlines;
extern int numopen;
extern uchar options[];
extern FILE * outfile;
extern FILE *listfile;
extern int32_t pcount,pcmax,z80zero;
extern long pline;
extern int pass;
extern int cline;
extern char *macstack;
extern char *macpars[10];
extern int maclens[10];
extern uchar attr[128];
extern int symcount;
extern FILE *errorfile;
extern int errorcount;
extern uchar **xdefhead;
extern uchar **xrefhead;
extern uchar **blockhead;
extern long blockcount;
extern struct sym *lastxref;

/*-----------------All chars follow------*/
extern uchar alabel; /* flag, 1 if this line had a label */
extern struct sym linelabel,symbol,symbol2,opsym;
extern char ltext[SYMMAX+2];
extern char stext[SYMMAX+2];
extern char stext2[SYMMAX+2];
extern char opcode[80];
extern uchar variant;
extern uchar optl,opto;
extern uchar exprtype;
extern uchar exprflag;
extern uchar opertype;
extern uchar phase;
extern uchar storing;
extern uchar oppri;
extern uchar opop;
extern uchar depth,ydepth;
extern uchar expanding;
extern uchar inmac;
extern uchar cpu;
extern uchar xrdflag;
extern char inputname[];
extern char origname[];
extern char outputname[];
extern char umac[3];
extern char *TREAD;
extern char *TWRITE;
extern struct sym *symbols;

extern struct sym *findsym();
extern struct sym *addsym();
extern char *inpoint;
#define at() (*inpoint)
#define get() (*inpoint++)
#define back() (*--inpoint)
#define back1() (--inpoint)
#define forward1() (++inpoint)
char *storeline();
extern char *textpoint;
void removecr();
extern void *tmalloc(int);
extern void tfree(void *);
extern uchar anumsign;
extern struct sym *lastrel;

// z80.c
void z80init(void);

// x86.c
void x86init(void);

// asm.c
int comma(void);
void expr(void);
void outw(int32_t);
void wout(int32_t);
void outl(int32_t);
void lout(int32_t);
void bout(uchar);
void outofrange(void);
void badreg(void);
void dodcb(void);
void dodsb(void);
void addref(struct sym *asym, int type);
void addreloc(int size);
void badsize(void);
int expect(char ch);
int expects(char *str);
int numsign(void);
void absreq(void);
void badmode(void);
void syntaxerr(void);
void (*scan(char *code, struct oplist *list))(void);
int gather(char *str);
void rexpr(void);
int skipwhite(void);
void error2(char *emsg);
