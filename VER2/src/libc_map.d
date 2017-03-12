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
int _fseek(FILE* stream,long offset,int whence);
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
int _stricmp(const char*,const char*);

double _sin(double);
double _cos(double);
double _tan(double);
double _sqrt(double);
double _frexp(double,int *);
double _pow(double,double);

uint _clock();
enum CLOCKS_PER_SEC=1000;

int __getch();
int __kbhit();

int dos_get_key(int *extended);

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

int get_usb_key(ref int extended)
{
	int result;
	int ext=0;
	result=dos_get_key(&ext);
	return result;
}

