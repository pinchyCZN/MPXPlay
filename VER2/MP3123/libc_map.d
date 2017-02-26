module libc_map;

import core.stdc.stdio: FILE;

extern (C) @nogc nothrow FILE* _fopen(const char *name, const char *param);
extern (C) @nogc nothrow int _fclose(FILE *f);
extern (C) @nogc nothrow int _fread(void *buf, int size, int rec, FILE *f);
extern (C) @nogc nothrow int _fwrite(const void *buf, int size, int rec, FILE *f);
extern (C) @nogc nothrow int _fseek(FILE* stream,long offset,int whence);
extern (C) @nogc nothrow uint _ftell(FILE* stream);
extern (C) @nogc nothrow void* _memset(void *buf, int val, int size);
extern (C) @nogc nothrow void* _memmove(void *s1, const void *s2, int size);
extern (C) @nogc nothrow void* _memcpy(void *s1, const void *s2, int size);


extern (C) @nogc nothrow void* _malloc(int size);
extern (C) @nogc nothrow void* _realloc(void *,int);
extern (C) @nogc nothrow void _free(void *buf);

extern (C) @nogc nothrow void* _printf(const char*,...);

extern (C) @nogc nothrow double _sin(double);
extern (C) @nogc nothrow double _cos(double);
extern (C) @nogc nothrow double _tan(double);
extern (C) @nogc nothrow double _sqrt(double);
extern (C) @nogc nothrow double _frexp(double,int *);
extern (C) @nogc nothrow double _pow(double,double);

extern (C) @nogc nothrow uint _clock();
enum CLOCKS_PER_SEC=1000;

extern (C) @nogc nothrow int _getch();
extern (C) @nogc nothrow int _kbhit();

extern (C) @nogc nothrow int dos_get_key(int *extended);

extern (C)
nothrow @nogc
int get_key(ref int extended)
{
	int result=0;
	extended=false;
	if(_kbhit()){
		result=_getch();
		if(result==0){
			result=_getch();
			extended=true;
		}
	}
	return result;
}

extern (C)
nothrow @nogc
int get_usb_key(ref int extended)
{
	int result;
	int ext=0;
	result=dos_get_key(&ext);
	return result;
}
