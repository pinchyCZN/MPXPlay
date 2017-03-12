module libc_map;

import core.stdc.stdio: FILE;
import core.stdc.stdarg: va_list;

extern (C):
@nogc:
nothrow:

FILE* _fopen(const char *name, const char *param);
int _fclose(FILE *f);
int _fread(void *buf, int size, int rec, FILE *f);
int _fwrite(const void *buf, int size, int rec, FILE *f);
int _fseek(FILE* stream,uint offset,int whence);
uint _ftell(FILE* stream);
void* _memset(void *buf, int val, int size);
void* _memmove(void *s1, const void *s2, int size);
void* _memcpy(void *s1, const void *s2, int size);


void* _malloc(int size);
void* _realloc(void *,int);
void _free(void *buf);

void* _printf(const char*,...);
void* _vprintf(const char*,va_list);

int _strlen(const char *);
int _strncpy(char *,const char *,int);
int __stricmp(const char*,const char*);

double _sin(double);
double _cos(double);
double _tan(double);
double _sqrt(double);
double _frexp(double,int *);
double _pow(double,double);

int _outp(int port,int val);
int _inp(int port);

uint _clock();
enum CLOCKS_PER_SEC=1000;

int __getch();
int __kbhit();

enum{
	VK_TAB=0x09,
	VK_FWDSLASH=0x2F,
	VK_ASTERISK=0x2A,
	VK_BACKSPACE=0x08,
	VK_MINUS=0x2D,
	VK_PLUS=0x2B,
	VK_ENTER=0x0D,
	VK_PERIOD=0x2E,
	VK_0=0x30,
	VK_1=0x31,
	VK_2=0x32,
	VK_3=0x33,
	VK_4=0x34,
	VK_5=0x35,
	VK_6=0x36,
	VK_7=0x37,
	VK_8=0x38,
	VK_9=0x39
};

int dos_get_key(int *extended);
int dos_put_key(int key);

int get_key(ref int extended)
{
	int result=0;
	extended=false;
	if(__kbhit()){
		result=__getch();
		if(result==0){
			result=__getch();
			extended=true;
		}
	}
	return result;
}

void log_msg(const char *fmt,...)
{
	import core.stdc.stdarg;
	import core.stdc.stdio;
	va_list arg;
	va_start(arg,fmt);
	vprintf(fmt,arg);
}
