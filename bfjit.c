/*********************************************************************************************/
/*
	BFJIT
	a fast, optimizing brainfuck heap compiler

	Copyright (c) 2011 Christian Fiedler

	This software is provided 'as-is', without any express or implied warranty.
	In no event will the authors be held liable for any damages arising from
	the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1.	The origin of this software must not be misrepresented; you must
			not claim that you wrote the original software. If you use this
			software in a product, an acknowledgment in the product
			documentation would be appreciated but is not required.
		
		2.	Altered source versions must be plainly marked as such, and must
			not be misrepresented as being the original software.
    
		3.	This notice may not be removed or altered from any source
			distribution.

*/
/*********************************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/*********************************************************************************************/
/* Predefined Modifications */


// if your system gives 13 and 10 instead of 10 on return, activate this
//#define USE_NEWLINE_HACK_13


// tries to optimize [>>] and [<<] by using x86 string functions
#define USE_STRINGFUNCTIONS

// Remove the ifdef if needed
#ifdef __gnu_linux__
#define USE_POSIX
#endif

//#define DEBUGMODE

/* End of Modifications */
/*********************************************************************************************/

#ifdef USE_POSIX
#include <sys/mman.h>
#include <malloc.h>
#include <unistd.h>
#endif

#ifdef DEBUGMODE
#define DEBUG(X) X
#else
#define DEBUG(X)
#endif

// Needn't to be changed, as it can be changed by the user via commandline
#define STANDARD_MEMLEN		0x8000
#define STANDARD_MEMSTART	STANDARD_MEMLEN/2

/*********************************************************************************************/
/*	Macros to keep 
 *	Modifications for plattforms different from 80386 can be made here
 *	Note, that I used inline assembler in the stub functions below! 
 */

#define INC_PTR_ESI				if(cellsize==1){INC_PTR8_ESI}else if(cellsize==2){INC_PTR16_ESI}else{INC_PTR32_ESI}
#define DEC_PTR_ESI				if(cellsize==1){DEC_PTR8_ESI}else if(cellsize==2){DEC_PTR16_ESI}else{DEC_PTR32_ESI}
#define ADD_PTR_ESI(X)			if(cellsize==1){ADD_PTR8_ESI(X)}else if(cellsize==2){ADD_PTR16_ESI(X)}else{ADD_PTR32_ESI(X)}
#define PTR_ESI_TO_ZERO			if(cellsize==1){PTR8_ESI_TO_ZERO}else if(cellsize==2){PTR16_ESI_TO_ZERO}else{PTR32_ESI_TO_ZERO}
#define TEST_PTR_ESI			if(cellsize==1){TEST_PTR8_ESI}else if(cellsize==2){TEST_PTR16_ESI}else{TEST_PTR32_ESI}
#define FIND_NONZERO_R			if(cellsize==1){FIND_NONZERO8_R}else if(cellsize==2){FIND_NONZERO16_R}else{FIND_NONZERO32_R}
#define FIND_NONZERO_L			if(cellsize==1){FIND_NONZERO8_L}else if(cellsize==2){FIND_NONZERO16_L}else{FIND_NONZERO32_L}

#define INC_PTR8_ESI			*codei++=0xFE;*codei++=0x06;
#define INC_PTR16_ESI			*codei++=0x66;*codei++=0xFF;*codei++=0x06;
#define INC_PTR32_ESI			*codei++=0xFF;*codei++=0x06;

#define FIND_NONZERO8_R			*codei++=0x30;*codei++=0xC0;*codei++=0xFD;*codei++=0xF3;*codei++=0xAE;*codei++=0x46;
#define FIND_NONZERO16_R		*codei++=0x66;*codei++=0x31;*codei++=0xC0;*codei++=0xFD;*codei++=0xF3;*codei++=0x66;\
								*codei++=0xAF;*codei++=0x46;
#define FIND_NONZERO32_R		*codei++=0x31;*codei++=0xC0;*codei++=0xFD;*codei++=0xF3;*codei++=0xAF;*codei++=0x46;

#define FIND_NONZERO8_L			*codei++=0x30;*codei++=0xC0;*codei++=0xFC;*codei++=0xF3;*codei++=0xAE;*codei++=0x4E;
#define FIND_NONZERO16_L		*codei++=0x66;*codei++=0x31;*codei++=0xC0;*codei++=0xFC;*codei++=0xF3;*codei++=0x66;\
								*codei++=0xAF;*codei++=0x4E;
#define FIND_NONZERO32_L		*codei++=0x31;*codei++=0xC0;*codei++=0xFC;*codei++=0xF3;*codei++=0xAF;*codei++=0x4E;

#define DEC_PTR8_ESI			*codei++=0xFE;*codei++=0x0E;
#define DEC_PTR16_ESI			*codei++=0x66;*codei++=0xFF;*codei++=0x0E;
#define DEC_PTR32_ESI			*codei++=0xFF;*codei++=0x0E;

#define ADD_PTR8_ESI(X)			*codei++=0x80;*codei++=0x06;*codei++=X;
#define ADD_PTR16_ESI(X)		*codei++=0x66;*codei++=0x81;*codei++=0x06;*codei++=X&0xFF;*codei++=(X>>8)&0xFF;
#define ADD_PTR32_ESI(X)		*codei++=0x81;*codei++=0x06;*codei++=X&0xFF;*codei++=(X>>8)&0xFF; \
								*codei++=(X>>16)&0xFF;*codei++=X>>24;

#define INC_ESI					*codei++=0x46;
#define DEC_ESI					*codei++=0x4E;
#define ADD_BYTE_ESI(X)			*codei++=0x83;*codei++=0xC6;*codei++=X;
#define SUB_BYTE_ESI(X)			*codei++=0x83;*codei++=0xEE;*codei++=X;
#define ADD_ESI(X)				*codei++=0x81;*codei++=0xC6;*codei++=X&0xFF;\
								*codei++=(X>>8)&0xFF;*codei++=(X>>16)&0xFF;*codei++=X>>24;
								
#define FARCALL(X)				*codei++=0xBB;*codei++=X&0xFF;*codei++=(X>>8)&0xFF;*codei++=(X>>16)&0xFF;\
								*codei++=X>>24;*codei++=0xFF;*codei++=0xD3;
#define SET_LOOPSTART_JMP(X)	loopstart[1]=X&0xFF;loopstart[2]=(tmp>>8)&0xFF;\
								loopstart[3]=(tmp>>16)&0xFF;loopstart[4]=tmp>>24;

								
#define PTR8_ESI_TO_ZERO		*codei++=0xC6;*codei++=0x06;*codei++=0x00;
#define PTR16_ESI_TO_ZERO		*codei++=0x66;*codei++=0xC7;*codei++=0x06;*codei++=0x00;*codei++=0x00;
#define PTR32_ESI_TO_ZERO		*codei++=0xC7;*codei++=0x06;*codei++=0x00;*codei++=0x00;*codei++=0x00;*codei++=0x00;

#define	LOOP_BEGIN				*codei++=0xE9;codei+=4;

#define	TEST_PTR8_ESI			*codei++=0x80;*codei++=0x3E;*codei++=0x00;
#define	TEST_PTR16_ESI			*codei++=0x66;*codei++=0x83;*codei++=0x3E;*codei++=0x00;
#define	TEST_PTR32_ESI			*codei++=0x83;*codei++=0x3E;*codei++=0x00;

#define JNE_SHORT(X)			*codei++=0x75;*codei++=(X+4)&0xFF;
#define JNE_NEAR(X)				*codei++=0x0F;*codei++=0x85;*codei++=X&0xFF;*codei++=(X>>8)&0xFF;\
								*codei++=(X>>16)&0xFF;*codei++=X>>24;
								
#define INCDEC_PTR_ESI_LEN		((cellsize==1)?INCDEC_PTR8_ESI_LEN:((cellsize==2)?INCDEC_PTR16_ESI_LEN:INCDEC_PTR32_ESI_LEN))
#define ADD_PTR_ESI_LEN			((cellsize==1)?ADD_PTR8_ESI_LEN:((cellsize==2)?ADD_PTR16_ESI_LEN:ADD_PTR32_ESI_LEN))
#define PTR_ESI_TO_ZERO_LEN		((cellsize==1)?PTR8_ESI_TO_ZERO_LEN:((cellsize==2)?PTR16_ESI_TO_ZERO_LEN:PTR32_ESI_TO_ZERO_LEN))
#define LOOP_END_LEN			((cellsize==1)?LOOP_END8_LEN:((cellsize==2)?LOOP_END16_LEN:LOOP_END32_LEN))
#define FIND_NONZERO_LEN		((cellsize==1)?FIND_NONZERO8_LEN:((cellsize==2)?FIND_NONZERO16_LEN:FIND_NONZERO32_LEN))

#define INCDEC_PTR8_ESI_LEN		2
#define INCDEC_PTR16_ESI_LEN	3
#define INCDEC_PTR32_ESI_LEN	2

#define ADD_PTR8_ESI_LEN		3
#define ADD_PTR16_ESI_LEN		5
#define ADD_PTR32_ESI_LEN		7

#define FIND_NONZERO8_LEN		7
#define FIND_NONZERO16_LEN		7
#define FIND_NONZERO32_LEN		7

#define PTR8_ESI_TO_ZERO_LEN	3
#define PTR16_ESI_TO_ZERO_LEN	5
#define PTR32_ESI_TO_ZERO_LEN	6

#define INCDEC_ESI				1
#define ADDSUB_BYTE_ESI			3
#define ADDSUB_ESI				6
#define FARCALL_LEN				7
#define LOOP_BEGIN_LEN			5

#define LOOP_END8_LEN			9
#define LOOP_END16_LEN			10
#define LOOP_END32_LEN			9

/*********************************************************************************************/


FILE *datei;

char *compilername;

unsigned char *bfcode,*bfcodei;
unsigned char *code,look,*codei;
unsigned char *mem, *filename;
int add_pn=0,codelen,memlen,memstart,filesize;
int error=0;
unsigned int add_pm=0;
int extended=0;
int newlinehack;
int cellsize=1;
int oneof=10;
/*
 *	oneof:	0 ... writes zero to cell, if eof is read
 *			255 ... writes 255 to cell, if eof is read
 *			anything else ... leaves the cell unchanged, if eof is read
 */

void check_pm(void){
	if(add_pm!=0){
		switch(add_pm){
		case 1:
			INC_PTR_ESI
			break;
		case -1:
			DEC_PTR_ESI
			break;
		default:
			ADD_PTR_ESI(add_pm)
			break;
		}
		add_pm=0;
	}
}

void check_pn(void){
	if(add_pn!=0){
		if(add_pn==1){
			INC_ESI
		}else if(add_pn==-1){
			DEC_ESI
		}else if(add_pn>=2 && add_pn<=0x7F){
			ADD_BYTE_ESI(add_pn)
		}else if(add_pn>=-0x7F && add_pn<=-2){
			SUB_BYTE_ESI((-add_pn))
		}else{ 
			ADD_ESI(add_pn)
		}
		add_pn=0;
	}
}

void usage(void){
	printf(	"\nusage:  %s [file] [-16|-32] [-0|-255] [-s start] [-m size]\n" \
			"\nfile\t\tname of input file\n" \
			"-m\t\tsize of programs memory (standard %d)\n" \
			"-s\t\tstart point of the memory pointer (>=0) (standard %d)\n" \
			"-16\t\tset cell size to 16 bit\n" \
			"-32\t\tset cell size to 32 bit\n" \
			"-0\t\twrites 0 into cell, if eof is read\n" \
			"-255\t\twrites 255 into cell, if eof is read\n" \
			"-h\t\tshows this message\n" \
			"\n", compilername,STANDARD_MEMLEN,STANDARD_MEMSTART);
	exit(0);
}


void len_pm(int *len){
	if(add_pm!=0){
		if(add_pm==1 || add_pm==255){
			*len+=INCDEC_PTR_ESI_LEN;
		}else{
			*len+=ADD_PTR_ESI_LEN;
		}
		add_pm=0;
	}
}

void len_pn(int *len){
	if(add_pn!=0){
		if(add_pn==1 || add_pn==-1){
			*len+=INCDEC_ESI;
		}else if(add_pn>=-0x7F && add_pn<=0x7F){
			*len+=ADDSUB_BYTE_ESI;
		}else{
			*len+=ADDSUB_ESI;
		}
		add_pn=0;
	}
}


int proglen(void){
	int len;
	for(len=1,look=*bfcodei++;*bfcodei!=0;look=*bfcodei++){
		switch(look){
		case '+':
			len_pn(&len);
			add_pm++;
			break;
		case '-':
			len_pn(&len);
			add_pm--;
			break;
		case '>':
			len_pm(&len);
			add_pn+=cellsize;
			break;
		case '<':
			len_pm(&len);
			add_pn-=cellsize;
			break;
		case '.':
			len_pm(&len);
			len_pn(&len);
			len+=FARCALL_LEN;
			break;
		case ',':
			len_pm(&len);
			len_pn(&len);
			len+=FARCALL_LEN;
			break;
		case '0':
			len_pn(&len);
			len+=PTR_ESI_TO_ZERO_LEN;
			break;
#ifdef USE_STRINGFUNCTIONS
		case 'r':
		case 'l':
			check_pm();
			check_pn();
			len+=FIND_NONZERO_LEN;
			
			break;
#endif
		case '#':
			len_pm(&len);
			len_pn(&len);
			len+=FARCALL_LEN;
			break;
		case '[':
			len_pm(&len);
			len_pn(&len);
			len+=LOOP_BEGIN_LEN;
			break;
		case ']':
			len_pm(&len);
			len_pn(&len);
			len+=LOOP_END_LEN;
			break;
		}
	}
	len_pm(&len);
	len_pn(&len);
	return len;
}

unsigned char *esi;


int getnum(int *zahl){
	int ret=0,min=0;
	look=getchar();
	if(look=='-'){
		look=getchar();
		min=1;
	}
	if(!isdigit(look)) return 0;
	while(isdigit(look)){
		ret=10*ret+look-'0';
		look=getchar();
	}
	if(min) ret=-ret;
	*zahl=ret;
	return 1;
}

void extension_debug_output(int i){
	unsigned int z;
	switch(cellsize){
	case 1:
		z=(mem+memstart)[i];
		if(z>=32 && z<=126)
			printf("[%d]\t= %02X %3d\t\'%c\'\n",i,z,z,z);
		else
			printf("[%d]\t= %02X %3d\n",i,z,z);
		break;
	case 2:
		z=((short*)(mem+memstart))[i];
		if(z>=32 && z<=126)
			printf("[%d]\t= %04X %5d\t\'%c\'\n",i,z,z,z);
		else
			printf("[%d]\t= %04X %5d\n",i,z,z);
		break;
	case 4:
		z=((int*)(mem+memstart))[i];
		if(z>=32 && z<=126)
			printf("[%d]\t= %08X %10d\t\'%c\'\n",i,z,z,z);
		else
			printf("[%d]\t= %08X %10d\n",i,z,z);
		break;
	}
}

/*********************************************************************************************/
/*	runtime stubs - these functions are called AT RUNTIME! Except the extension_debug_stub, they
 *	should be extremely fast!
 *	So, here is the rest of the plattform-specific stuff (inline assembler)
 */
void extension_debug_stub(void){
	asm volatile("movl %%esi,%0\n" :: "m" (esi));
	int zahl1,zahl2;
	printf("\nDEBUG QUERY\t\tcell pointer at: %p  ",esi-(int)mem-memstart);
	if(esi>=mem && esi<mem+memlen)
		printf("(ok)");
	else
		printf("!! out of bounds !!");
	printf("\n\n");
	fflush(stdin);
	for(;;){
		printf("> ");
		if(getnum(&zahl1)){
			if(zahl1>=-memstart && zahl1<memlen-memstart){
				if(look==','){
					if(getnum(&zahl2)){
						if(zahl2>=-memstart && zahl2<memlen-memstart && zahl2>zahl1){
							for(;zahl1<=zahl2;zahl1++) extension_debug_output(zahl1);
						}
					}
				}else{
					extension_debug_output(zahl1);
				}
			}
		}else{
			break;
		}
	}
	fflush(stdin);
	asm volatile("movl %0,%%esi\n" :: "m" (esi));
}

void print_stub(void){
	asm volatile(	"movl %%esi,%0\n"
					"xorl %%eax,%%eax\n"
					"movb (%%esi),%%al\n"
					"push %%eax\n"
					"call *%1\n"
					"pop %%eax\n" // shorter than "add $4, %%esp"
					"movl %0, %%esi\n"
					:: "m" (esi), "b" ((int)putchar));
}

/*	The next three functions are very similar to each other. They are
 *	split up to speed up the programs runtime.
 */
void input_stub_eof_none(void){
	asm volatile("mov %%esi,%0" :: "m" (esi));
	unsigned char i;
	if(feof(stdin)) return;
	i=getchar();
#ifdef USE_NEWLINE_HACK_13
	if(i==13) i=10;
#endif
	*esi=i;
	asm volatile("mov %0,%%esi\n" :: "m" (esi));
}


void input_stub_eof_0(void){
	asm volatile("mov %%esi,%0" :: "m" (esi));
	unsigned char i;
	if(feof(stdin)) i=0; else i=getchar();
#ifdef USE_NEWLINE_HACK_13
	if(i==13) i=10;
#endif
	*esi=i;
	asm volatile("mov %0,%%esi\n" :: "m" (esi));
}

void input_stub_eof_255(void){
	asm volatile("mov %%esi,%0" :: "m" (esi));
	unsigned char i;
	if(feof(stdin)) i=255; else i=getchar();
#ifdef USE_NEWLINE_HACK_13
	if(i==13) i=10;
#endif
	*esi=i;
	asm volatile("mov %0,%%esi\n" :: "m" (esi));
}

void program(void){
	int tmp;
	unsigned char *loopstart;
	for(look=*bfcodei++;*(bfcodei-1)!=0;look=*bfcodei++){
		switch(look){
		case '+':
			check_pn();
			add_pm++;
			break;
		case '-':
			check_pn();
			add_pm--;
			break;
		case '>':
			check_pm();
			add_pn+=cellsize;
			break;
		case '<':
			check_pm();
			add_pn-=cellsize;
			break;
#ifdef USE_STRINGFUNCTIONS
		case 'r':
			check_pm();
			check_pn();
			FIND_NONZERO_R
			
			break;
		case 'l':
			check_pm();
			check_pn();
			FIND_NONZERO_L
			
			break;
#endif
		case '#':
			check_pm();
			check_pn();
			
			FARCALL((int)extension_debug_stub)
			
			break;
		case '.':
			check_pm();
			check_pn();
						
			FARCALL((int)print_stub)
			
			break;
		case ',':
			check_pm();
			check_pn();

			switch(oneof){
			case 0:
				FARCALL((int)input_stub_eof_0);
				break;
			case 255:
				FARCALL((int)input_stub_eof_255);
				break;
			default:
				FARCALL((int)input_stub_eof_none);
				break;
			}
			break;
		case '0':
			check_pn();
			PTR_ESI_TO_ZERO
			
			break;
		case '[':
			check_pm();
			check_pn();


			loopstart=codei;
			LOOP_BEGIN
			program();
			
			tmp=(int)codei-(int)loopstart-5;
			SET_LOOPSTART_JMP(tmp)
			
			TEST_PTR_ESI
			
			tmp=((int)loopstart-(int)codei-1);
			if(tmp>=-0x7F){
				JNE_SHORT(tmp)
			}else{ 
				JNE_NEAR(tmp)
			}
			break;
		case ']':
			check_pm();
			check_pn();
			return;
		}
	}
	check_pm();
	check_pn();
	error++;
}


void parsecmdln(int argc, char *argv[]){
	int i;
	if(argc>1){
		for(i=1;i<argc;i++){
			if(argv[i][0]=='-'){
				if(strcmp(argv[i]+1,"16")==0){
					cellsize=2;
				}else if(strcmp(argv[i]+1,"32")==0){
					cellsize=4;
				}else if(strcmp(argv[i]+1,"0")==0){
					oneof=0;
				}else if(strcmp(argv[i]+1,"255")==0){
					oneof=255;
				}else{
					switch(argv[i][1]){
					case 'm':
						if(argc==i+1) usage();
						memlen=atoi(argv[++i]);
						break;
					case 's':
						if(argc==i+1) usage();
						memstart=atoi(argv[++i]);
						break;
					case 'h':
						usage();
						break;
					}
				}
			}
		}
	}
}

void parsebf1(void){
	int tmp;
	for(look=fgetc(datei);bfcodei-bfcode<=filesize && !feof(datei);look=fgetc(datei)){
		if(datei==stdin && look=='!') break;
		if(		look=='+' || look=='-' || look=='>' || look=='<' || look=='.' || \
				look==',' || look==']' || look=='[' || look=='#'){
			*bfcodei++=look;
		}else if(look=='['){
			// make "[-]" to single instruction
			// make "[>>]" to single instruction
			// make "[<<]" to single instruction
			tmp=fgetc(datei);
			if(tmp=='-'){
				tmp=fgetc(datei);
				if(tmp==']'){
					*bfcodei++='0';
				}else{
					*bfcodei++='[';
					*bfcodei++='-';
					*bfcodei++=(char) tmp;
				}
#ifdef USE_STRINGFUNCTIONS
			}else if(tmp=='>'){
				tmp=fgetc(datei);
				if(tmp=='>'){
					tmp=fgetc(datei);
					if(tmp==']'){
						*bfcodei++='r';
					}else{
						*bfcodei++='[';
						*bfcodei++='>';
						*bfcodei++='>';
						*bfcodei++=(char) tmp;
					}
				}else{
					*bfcodei++='[';
					*bfcodei++='>';
					*bfcodei++=(char) tmp;
				}
			}else if(tmp=='<'){
				tmp=fgetc(datei);
				if(tmp=='<'){
					tmp=fgetc(datei);
					if(tmp==']'){
						*bfcodei++='l';
					}else{
						*bfcodei++='[';
						*bfcodei++='<';
						*bfcodei++='<';
						*bfcodei++=(char) tmp;
					}
				}else{
					*bfcodei++='[';
					*bfcodei++='<';
					*bfcodei++=(char) tmp;
				}
#endif
			}else{
				*bfcodei++='[';
				*bfcodei++=(char) tmp;
			}
		}
	}
	filesize=bfcodei-bfcode;
	*bfcodei=0;
}

unsigned char* alloc_execmem(int len){
	unsigned char* execmem;
#ifdef USE_POSIX
	int pagesz = sysconf(_SC_PAGE_SIZE);
	execmem = memalign(pagesz, len);
#else
	execmem = malloc(len+4096);	
#endif
	if(execmem == NULL) exit(17);

	return execmem;
}

void bypass_nx(void *mem){
#ifdef USE_POSIX
	mprotect(mem, codelen, PROT_READ | PROT_EXEC);
#endif
}

unsigned char* compile(void){
	unsigned char *execmem;
	parsebf1();
	if(datei!=stdin) fclose(datei);
	bfcodei=bfcode;

	codelen=proglen();	
	execmem = alloc_execmem(codelen);

	codei = code = execmem;
	mem=malloc(memlen*cellsize);
	memset(mem,0,memlen);

	bfcodei=bfcode;

	program();

	*codei++=0xC3; // Return

	return execmem;
}

int openfile(int argc, char *argv[]){
	int i;
	for(i=1,datei=NULL;i<argc && datei==NULL;i++){
		datei=fopen(argv[i],"r");
	}
	return (datei!=NULL);
}

int main(int argc, char *argv[]){
	unsigned char *execmem;	
	compilername=argv[0];
	memlen=STANDARD_MEMLEN;
	memstart=STANDARD_MEMSTART;
	parsecmdln(argc,argv);
	if(openfile(argc,argv)){
		for(filesize=0;!feof(datei);filesize++) fgetc(datei);
		fseek(datei,0,SEEK_SET);
	}else{
		datei=stdin;
		printf("Program length: ");
		while(scanf("%d",&filesize) == 0);
	}
	bfcodei=bfcode=malloc(filesize+1);
	execmem = compile();
	free(bfcode);
	bfcodei=bfcode=NULL;
	if(error!=1) return 1;		// if sourcecode is wrong, for instance: "[[++]"
	
#ifdef DEBUGMODE
	printf("compilation completed with no errors\n");
	printf("execmem: %p\tcodelen: %d\tmem: %p\tmemlen: %d cells\n", execmem, codelen, mem, memlen);
#endif

	DEBUG(printf("marking execmem as executable...\n"));
	bypass_nx(execmem);

	DEBUG(printf("executing...\n"));
	asm volatile("call *%0" :: "m" (code), "S" (mem+memstart));		// again plattform specific
	printf("\n");

	return 0;
}
