//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: error handling (exception,pagefault,div0,disk,matherr)


#include "mpxplay.h"
#include <signal.h>

typedef void (*mcp_cvt)(int);

#ifdef MPXPLAY_WIN32
 #include <wincon.h>

static BOOL WINAPI pds_ctrlbreak_handler(DWORD p1)
{
 mpxplay_close_program(p1);
 return 0;
}

#else // __DOS__

#include <i86.h>
#include "newfunc.h"

static char div0handstack[IRQ_STACK_SIZE];
static char __far *new_stack_div0;
static void __far *old_stack_div0;
static void __far *old_stack_except;
static void far *oldint_except0x0;
static void far *oldint_except0xd;
static void far *oldint_except0xe;
static void far *oldint1b;
static void far *oldint23;
static void far *oldint24;
static dosmem_t rm_24_dm;
static char realmode_24_code[4]={0xb8,0x00,0x00,0xcf}; // mov ax,0 ; iret
//static char realmode_24_code[10]={0x66,0xff,0x06,0x0A,0x00,  0xb8,0x00,0x00, 0xcf}; // inc dword ptr cs:0x0A ; mov ax,0 ; iret
//unsigned long *int24errorcount_r;

void asm_div0hand(void);

void pds_div0_handler(void)   // (integer) division by zero handler
{
#pragma aux asm_div0hand=\
 "push eax"\
 "mov ax,cs"\
 "cmp ax,word ptr [esp+20]"\
 "pop eax"\
 "jne chain"\
 "push eax"\
 "push edx"\
 "push ebx"\
 "cld"\
 "mov edx,[esp+24]"\
 "push ds"\
 "push es"\
 "mov ax,ds"\
 "mov es,ax"\
 "mov word ptr old_stack_div0+4,ss" \
 "mov dword ptr old_stack_div0+0,esp" \
 "mov ss,ax"\
 "lss esp,new_stack_div0"\
 "mov eax,13"\
 "call mpxplay_close_program"\
 "lss esp,old_stack_div0"\
 "pop es"\
 "pop ds"\
 "pop ebx"\
 "pop edx"\
 "pop eax"\
 "retf"\
 "chain:jmp cs:oldint_except0x0"
 asm_div0hand();
}

void asm_pagefaulthand(void);

void pds_pagefault_handler(void)  // exception error handler (page fault)
{
#pragma aux asm_pagefaulthand=\
 "push eax"\
 "mov ax,cs"\
 "cmp ax,word ptr [esp+20]"\
 "pop eax"\
 "jne chain"\
 "push eax"\
 "push edx"\
 "push ebx"\
 "cld"\
 "mov edx,[esp+24]"\
 "push ds"\
 "push es"\
 "mov ax,ds"\
 "mov es,ax"\
 "mov word ptr old_stack_except+4,ss" \
 "mov dword ptr old_stack_except+0,esp" \
 "mov ss,ax"\
 "lss esp,new_stack_div0"\
 "mov eax,14"\
 "call mpxplay_close_program"\
 "lss esp,old_stack_except"\
 "pop es"\
 "pop ds"\
 "pop ebx"\
 "pop edx"\
 "pop eax"\
 "retf"\
 "chain:jmp cs:oldint_except0xe"
 asm_pagefaulthand();
}

void asmcbh(void);

void pds_ctrlbreak_handler(void)   // disables ctrl-break
{
 #pragma aux asmcbh= "iretd"
 asmcbh();
}

void asmceh(void);

/*void far pds_newhandler_24p()  // skips disk errors
{
#pragma aux asmceh=\
 "mov eax,dword ptr int24errorcount_r"\
 "mov dword ptr [eax],1"\
 "mov eax,0"\
 "iretd"\
 modify[];
 asmceh();
}*/

void far pds_newhandler_24p()  // skips disk errors
{
#pragma aux asmceh=\
 "mov eax,0"\
 "iretd"\
 modify[];
 asmceh();
}
#endif // __DOS__

void newfunc_errorhnd_int24_init(void)
{
#ifdef __DOS__
 pds_dos_setvect(0x24,(void far *)pds_newhandler_24p); // protected mode int24 vector
 if(!rm_24_dm.linearptr)
  pds_dpmi_dos_allocmem(&rm_24_dm,16);
 if(rm_24_dm.linearptr){
  pds_memcpy(rm_24_dm.linearptr,realmode_24_code,4);
  //pds_memcpy(rm_24_dm.linearptr,realmode_24_code,10);
  //int24errorcount_r=(unsigned long *)(rm_24_dm.linearptr+10);
  //*int24errorcount_r=0;
  pds_dpmi_setrmvect(0x24,rm_24_dm.segment,0);         // real mode int24 vector
 }
#endif
}

void newfunc_error_handlers_init(void)
{
#ifdef __DOS__
 int i;

 oldint1b=pds_dos_getvect(0x1b);
 oldint23=pds_dos_getvect(0x23);

 pds_dos_setvect(0x1b, (void far *)pds_ctrlbreak_handler);
 pds_dos_setvect(0x23, (void far *)pds_ctrlbreak_handler);

 oldint24=pds_dos_getvect(0x24);
#endif // __DOS__
 newfunc_errorhnd_int24_init();

 signal(SIGABRT,(mcp_cvt)mpxplay_close_program);
 signal(SIGILL ,(mcp_cvt)mpxplay_close_program);
 signal(SIGSEGV,(mcp_cvt)mpxplay_close_program);

#ifdef MPXPLAY_WIN32
 SetConsoleCtrlHandler(pds_ctrlbreak_handler,1);
#elif defined(__DOS__)
 new_stack_div0=(char far *)(&div0handstack[0]+IRQ_STACK_SIZE);

 oldint_except0x0=pds_dpmi_getexcvect(0x00);
 oldint_except0xe=pds_dpmi_getexcvect(0x0e);
 oldint_except0xd=pds_dpmi_getexcvect(0x0d);

 pds_dpmi_setexcvect(0x00, (void __interrupt (*)())pds_div0_handler);
 for(i=1;i<15;i++)
  pds_dpmi_setexcvect(i, (void __interrupt (*)())pds_pagefault_handler);
#endif // __DOS__
}

void newfunc_error_handlers_close(void)
{
#ifdef __DOS__
 pds_dos_setvect(0x1b, oldint1b);
 pds_dos_setvect(0x23, oldint23);
 pds_dos_setvect(0x24, oldint24);
 pds_dpmi_dos_freemem(&rm_24_dm);
#endif // __DOS__
}

void newfunc_exception_handlers_close(void)
{
#ifdef __DOS__
 pds_dpmi_setexcvect(0x00,oldint_except0x0);
 pds_dpmi_setexcvect(0x0e,oldint_except0xe);
 pds_dpmi_setexcvect(0x0d,oldint_except0xd);
#endif
}

int matherr(struct _exception *a)
{
 a->retval=1.0;
 return 1;
}

/*
#ifdef MPXPLAY_WIN32

static HINSTANCE mpxplay_curr_instance;

static LRESULT CALLBACK EventProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
 switch(message){
  case WM_CLOSE:
   UnregisterClass("Mpxplay for Windows",mpxplay_curr_instance);
   mpxplay_close_program(0);
   break;
 }
 return 0;
}

static void pds_mswin_newinstance_init(void)
{
 WNDCLASS wc;
 mpxplay_curr_instance=GetModuleHandle(NULL);
 wc.style = 0;
 wc.lpfnWndProc = EventProc;
 wc.cbClsExtra = 0;
 wc.cbWndExtra = 0;
 wc.hInstance = mpxplay_curr_instance;
 wc.hCursor = NULL;
 wc.hIcon = NULL;
 wc.hbrBackground = NULL;
 wc.lpszClassName = "Mpxplay for Windows";
 wc.lpszMenuName = NULL;
 RegisterClass(&wc);
}
#endif

void pds_mswin_previousinstance_close(void)
{
#ifdef MPXPLAY_WIN32
 HWND running_mpxplay = FindWindow("Mpxplay for Windows", NULL);
 //printf("aaaa %8.8X %d \n",(long)running_mpxplay,GetLastError());
 //getch();
 if(running_mpxplay!=NULL)
  SendMessage(running_mpxplay, WM_CLOSE, NULL, NULL);
 pds_mswin_newinstance_init();
#endif
}
*/
