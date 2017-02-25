module libc_map;

import core.stdc.stdio;

extern (C) @nogc nothrow FILE* _fopen(const char *name, const char *param);
extern (C) @nogc nothrow int _fclose(FILE *f);
extern (C) @nogc nothrow int _fread(void *buf, int size, int rec, FILE *f);
extern (C) @nogc nothrow int _fwrite(const void *buf, int size, int rec, FILE *f);
extern (C) @nogc nothrow int _fseek(FILE* stream,long offset,int whence);
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
