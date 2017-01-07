//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2010 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: timer/scheduler/signalling (timed function calls)

#include <mpxplay.h>
#include <mpxinbuf.h>
#include "newfunc.h"

#define MPXPLAY_TIMEDS_INITSIZE  32
#define MPXPLAY_TIMERS_STACKSIZE 32768
#define MTF_FLAG_LOCK 1

typedef struct mpxplay_timed_s{
 void *func;
 void *data;
 unsigned long timer_flags;
 unsigned long refresh_delay;  // or signal event (SIGNALTYPE)
 unsigned long refresh_counter;

#ifdef __DOS__
 char *ownstackmem;     // for int08 functions
 void __far *oldstack;
 char __far *newstack;
#endif
}mpxplay_timed_s;

typedef void (*call_timedfunc_nodata)(void);
typedef void (*call_timedfunc_withdata)(void *);

static void newfunc_timer_delete_entry(mpxplay_timed_s *mtf);
static void mpxplay_timer_delete_int08_funcs(void);

extern volatile unsigned int intsoundcontrol;
extern unsigned long intdec_timer_counter;
extern struct mainvars mvps;

static volatile mpxplay_timed_s *mpxplay_timed_functions;
static volatile unsigned long mtf_storage,mtf_flags;
#ifdef __DOS__
static volatile unsigned int oldint08_running;
#endif
volatile unsigned long int08counter,mpxplay_signal_events;

static unsigned int newfunc_timer_alloc(void)
{
 unsigned int newsize;
 mpxplay_timed_s *mtf;

 if(mtf_storage)
  newsize=mtf_storage*2;
 else
  newsize=MPXPLAY_TIMEDS_INITSIZE;

 mtf=pds_calloc(newsize,sizeof(mpxplay_timed_s));
 if(!mtf)
  return 0;
 funcbit_smp_enable(mtf_flags,MTF_FLAG_LOCK);
 if(mpxplay_timed_functions){
  pds_smp_memcpy((char *)mtf,(char *)mpxplay_timed_functions,mtf_storage*sizeof(mpxplay_timed_s));
  pds_free((void *)mpxplay_timed_functions);
 }
 funcbit_smp_pointer_put(mpxplay_timed_functions,mtf);
 funcbit_smp_value_put(mtf_storage,newsize);
 funcbit_smp_disable(mtf_flags,MTF_FLAG_LOCK);
 return newsize;
}

static void mpxplay_timer_close(void)
{
 mpxplay_timed_s *mtf=(mpxplay_timed_s *)mpxplay_timed_functions;
 if(mtf){
#ifdef __DOS__
  unsigned int i;
#endif

  funcbit_smp_enable(mtf_flags,MTF_FLAG_LOCK);
  mpxplay_timer_delete_int08_funcs();
  funcbit_smp_pointer_put(mpxplay_timed_functions,NULL);

#ifdef __DOS__
  for(i=0;i<mtf_storage;i++){
   if(mtf->ownstackmem)
    pds_free(mtf->ownstackmem);
   mtf++;
  }
#endif
  pds_free(mtf);
  funcbit_smp_disable(mtf_flags,MTF_FLAG_LOCK);
 }
 funcbit_smp_value_put(mtf_storage,0);
}

static mpxplay_timed_s *newfunc_timer_search_entry(mpxplay_timed_s *mtf,void *func,void *data)
{
 mpxplay_timed_s *mte=((mpxplay_timed_s *)mpxplay_timed_functions)+mtf_storage;

 if(!mtf)
  mtf=(mpxplay_timed_s *)mpxplay_timed_functions;
 if(!mtf)
  return NULL;
 if(mtf>=mte)
  return NULL;

 do{
  if(mtf->func==func)
   if(!data || mtf->data==data)
    return mtf;
  mtf++;
 }while(mtf<mte);

 return NULL;
}

static mpxplay_timed_s *newfunc_timer_getfree_entry(void)
{
 mpxplay_timed_s *mtf;

 mtf=newfunc_timer_search_entry(NULL,NULL,NULL);

 if(!mtf)
  if(newfunc_timer_alloc())
   mtf=newfunc_timer_search_entry(NULL,NULL,NULL);

 return mtf;
}

static int newfunc_timer_add_entry(mpxplay_timed_s *mtf,void *func,void *data,unsigned int timer_flags,unsigned int refresh_delay)
{
#ifdef __DOS__
 if(funcbit_test(timer_flags,MPXPLAY_TIMERFLAG_OWNSTACK)){
  if(!mtf->ownstackmem){
   mtf->ownstackmem=(char *)pds_malloc(MPXPLAY_TIMERS_STACKSIZE+32);
   if(!mtf->ownstackmem)
    return -1;
   mtf->newstack=(char far *)(mtf->ownstackmem+MPXPLAY_TIMERS_STACKSIZE);
  }
 }
#endif
 funcbit_smp_pointer_put(mtf->func,func);
 funcbit_smp_pointer_put(mtf->data,data);
 funcbit_smp_value_put(mtf->timer_flags,timer_flags);
 funcbit_smp_value_put(mtf->refresh_delay,refresh_delay);
 funcbit_smp_value_put(mtf->refresh_counter,funcbit_smp_value_get(int08counter));
 return (int)(mtf-mpxplay_timed_functions); // index in mpxplay_timed_functions
}

static void newfunc_timer_delete_entry(mpxplay_timed_s *mtf)
{
 if(mtf){
  funcbit_smp_pointer_put(mtf->func,NULL);
  funcbit_smp_value_put(mtf->timer_flags,0);
 }
}

//------------------------------------------------------------------------
unsigned long mpxplay_timer_secs_to_counternum(unsigned long secs)
{
 mpxp_int64_t cn;     // 1000.0ms/55.0ms = 18.181818 ticks per sec
 pds_fto64i((float)secs*(1000.0/55.0)*(float)INT08_DIVISOR_DEFAULT/(float)INT08_DIVISOR_NEW,&cn);
 return cn;
}

int mpxplay_timer_addfunc(void *func,void *data,unsigned int timer_flags,unsigned int refresh_delay)
{
 mpxplay_timed_s *mtf=NULL;

 if(!func)
  return -1;

 if(!(funcbit_test(timer_flags,MPXPLAY_TIMERFLAG_MULTIPLY)))
  mtf=newfunc_timer_search_entry(NULL,func,NULL); // update previous instance if exists

 if(!mtf)
  mtf=newfunc_timer_getfree_entry();

 if(!mtf)
  return -1;

 if(!data && funcbit_test(timer_flags,MPXPLAY_TIMERFLAG_MVPDATA))
  data=&mvps;

 return newfunc_timer_add_entry(mtf,func,data,timer_flags,refresh_delay);
}

int mpxplay_timer_modifyfunc(void *func,int timer_flags,int refresh_delay)
{
 mpxplay_timed_s *mtf;

 if(!func)
  return -1;
 mtf=newfunc_timer_search_entry(NULL,func,NULL);
 if(mtf){
  if(timer_flags>=0)
   funcbit_smp_value_put(mtf->timer_flags,timer_flags);
  if(refresh_delay>=0)
   funcbit_smp_value_put(mtf->refresh_delay,refresh_delay);
  return (int)(mtf-mpxplay_timed_functions);
 }
 return -1;
}

int mpxplay_timer_modifyhandler(void *func,int handlernum_index,int timer_flags,int refresh_delay)
{
 if((handlernum_index>=0) && (handlernum_index<=mtf_storage) && mpxplay_timed_functions){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[handlernum_index];
  if(mtf->func==func){
   if(timer_flags>=0)
    funcbit_smp_value_put(mtf->timer_flags,timer_flags);
   if(refresh_delay>=0)
    funcbit_smp_value_put(mtf->refresh_delay,refresh_delay);
   return handlernum_index;
  }
 }
 return -1;
}

void mpxplay_timer_deletefunc(void *func,void *data)
{
 mpxplay_timed_s *mtf;

 if(func){
  mtf=newfunc_timer_search_entry(NULL,func,data);
  if(mtf){
   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY))
    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT);
   else
    newfunc_timer_delete_entry(mtf);
  }
 }
}

void mpxplay_timer_deletehandler(void *func,int handlernum_index)
{
 if((handlernum_index>=0) && (handlernum_index<=mtf_storage) && mpxplay_timed_functions){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[handlernum_index];
  if(mtf->func==func){
   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY))
    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT);
   else
    newfunc_timer_delete_entry(mtf);
  }
 }
}

void mpxplay_timer_executefunc(void *func)
{
 mpxplay_timed_s *mtf;

 if(func){
  mtf=newfunc_timer_search_entry(NULL,func,NULL);
  if(mtf && mtf->func){
   if(mtf->data)
    ((call_timedfunc_withdata)(mtf->func))(mtf->data);
   else
    ((call_timedfunc_nodata)(mtf->func))();
  }
 }
}

// currently returns 1 if there is delay, returns 0 if no
unsigned int mpxplay_timer_lowpriorstart_wait(void)
{
 if(!mpxplay_check_buffers_full(&mvps))
  return 1;
 return 0;
}

//------------------------------------------------------------------------

void mpxplay_timer_reset_counters(void)
{
 mpxplay_timed_s *mtf=(mpxplay_timed_s *)funcbit_smp_pointer_get(mpxplay_timed_functions);
 unsigned int i,clearint08=!funcbit_smp_test(mvps.aui->card_handler->infobits,SNDCARD_INT08_ALLOWED); // ???
 if(!mtf)
  return;
 if(funcbit_smp_test(mtf_flags,MTF_FLAG_LOCK))
  return;
 for(i=0;i<funcbit_smp_value_get(mtf_storage);i++){
  if(funcbit_smp_pointer_get(mtf->func)){

   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT) && !funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_SIGNAL)){
    if(funcbit_smp_value_get(int08counter)>funcbit_smp_value_get(mtf->refresh_delay))
     funcbit_smp_value_put(mtf->refresh_counter,int08counter-mtf->refresh_delay);
    else
     funcbit_smp_value_put(mtf->refresh_counter,0);
   }

   if(clearint08)
    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_INT08);
  }
  mtf++;
 }
 funcbit_smp_disable(mpxplay_signal_events,MPXPLAY_SIGNALMASK_TIMER); // !!!
}

#define MPXPLAY_TIMER_MAINCYCLE_EXCLUSION (MPXPLAY_TIMERTYPE_INT08|MPXPLAY_TIMERFLAG_BUSY)

void mpxplay_timer_execute_maincycle_funcs(void) // not reentrant!
{
 unsigned int i;
 volatile unsigned long signal_events;
 if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
  return;
 if(funcbit_smp_test(mtf_flags,MTF_FLAG_LOCK))
  return;
 signal_events=funcbit_smp_value_get(mpxplay_signal_events);
 funcbit_smp_disable(mpxplay_signal_events,MPXPLAY_SIGNALMASK_TIMER);
 for(i=0;i<funcbit_smp_value_get(mtf_storage);i++){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];
  void *mtf_func=funcbit_smp_pointer_get(mtf->func);
  if(mtf_func && !funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMER_MAINCYCLE_EXCLUSION)){

   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_SIGNAL)){
    if(funcbit_smp_test(signal_events,mtf->refresh_delay)){

#if defined(__DOS__)
     if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_INDOS)){ // ???
      if(pds_filehand_check_infilehand())    //
       continue;                             //
      if(pds_indos_flag())                   //
       continue;                             //
     }                                       //
#endif

     funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);

     if(funcbit_smp_pointer_get(mtf->data))
      ((call_timedfunc_withdata)(mtf_func))(mtf->data);
     else
      ((call_timedfunc_nodata)(mtf_func))();

     mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i]; // function may modify mpxplay_timed_functions (alloc -> new begin pointer)

     if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT))
      newfunc_timer_delete_entry(mtf);

     funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);
     funcbit_smp_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_REALTIME); // ???
    }
   }else{
    if(funcbit_smp_value_get(int08counter)>=(funcbit_smp_value_get(mtf->refresh_counter)+funcbit_smp_value_get(mtf->refresh_delay))){

     if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_LOWPRIOR)){
      if(!mpxplay_check_buffers_full(&mvps))
       continue;
     }

#if defined(__DOS__)
     if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_INDOS)){ // ???
      if(pds_filehand_check_infilehand())    //
       continue;                             //
      if(pds_indos_flag())                   //
       continue;                             //
     }                                       //
#endif

     funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);

     funcbit_smp_value_put(mtf->refresh_counter,funcbit_smp_value_get(int08counter));

     if(funcbit_smp_pointer_get(mtf->data))
      ((call_timedfunc_withdata)(mtf_func))(mtf->data);
     else
      ((call_timedfunc_nodata)(mtf_func))();

     mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];

     if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT))
      newfunc_timer_delete_entry(mtf);

     funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);

     if(!funcbit_smp_value_get(mtf->refresh_delay))
      funcbit_smp_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_REALTIME);
    }
   }

  }
 }
 if(funcbit_smp_test(mpxplay_signal_events,MPXPLAY_SIGNALMASK_TIMER))
  funcbit_smp_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_REALTIME);
}

#if defined(__DOS__) && defined(__WATCOMC__)

void call_func_ownstack_withdata(void *data,void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_withdata parm [eax][edx][ebx][ecx] = \
  "mov word ptr [ebx+4],ss" \
  "mov dword ptr [ebx],esp" \
  "lss esp,[ecx]" \
  "call edx" \
  "lss esp,[ebx]"

void call_func_ownstack_withdata_sti(void *data,void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_withdata_sti parm [eax][edx][ebx][ecx] = \
  "mov word ptr [ebx+4],ss" \
  "mov dword ptr [ebx],esp" \
  "lss esp,[ecx]" \
  "sti"\
  "call edx" \
  "cli"\
  "lss esp,[ebx]"

void call_func_ownstack_nodata(void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_nodata parm [eax][edx][ebx] = \
  "mov word ptr [edx+4],ss" \
  "mov dword ptr [edx],esp" \
  "lss esp,[ebx]" \
  "call eax" \
  "lss esp,[edx]"

void call_func_ownstack_nodata_sti(void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_nodata_sti parm [eax][edx][ebx] = \
  "mov word ptr [edx+4],ss" \
  "mov dword ptr [edx],esp" \
  "sti"\
  "lss esp,[ebx]" \
  "call eax" \
  "lss esp,[edx]"\
  "cli"

#endif // __DOS__ && __WATCOMC__

#define MPXPLAY_TIMER_MAX_PARALELL_INT08_THREADS 8

#define MPXPLAY_TIMER_INT08_EXCLUSION (MPXPLAY_TIMERTYPE_SIGNAL|MPXPLAY_TIMERFLAG_BUSY)

void mpxplay_timer_execute_int08_funcs(void)
{
 static volatile unsigned int recall_counter;
 unsigned int i;
 if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
  return;
 if(funcbit_smp_test(mtf_flags,MTF_FLAG_LOCK))
  return;
 if(funcbit_smp_value_get(recall_counter)>=MPXPLAY_TIMER_MAX_PARALELL_INT08_THREADS)
  return;
 funcbit_smp_value_increment(recall_counter);
 for(i=0;(i<funcbit_smp_value_get(mtf_storage)) && funcbit_smp_pointer_get(mpxplay_timed_functions);i++){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];
  void *mtf_func=funcbit_smp_pointer_get(mtf->func);
  if(mtf_func && funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_INT08) && !funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMER_INT08_EXCLUSION)){
   if(funcbit_smp_value_get(int08counter)>=(funcbit_smp_value_get(mtf->refresh_counter)+funcbit_smp_value_get(mtf->refresh_delay))){

#if defined(__DOS__)
    if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_INDOS)){
     if(oldint08_running)
      continue;
     if(pds_filehand_check_infilehand())
      continue;
     if(pds_indos_flag())
      continue;
    }
#endif

    funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);

    funcbit_smp_value_put(mtf->refresh_counter,funcbit_smp_value_get(int08counter));

#if defined(__DOS__) && defined(__WATCOMC__)
    if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_OWNSTACK)){
     mtf->newstack=(char far *)(mtf->ownstackmem+MPXPLAY_TIMERS_STACKSIZE);
     if(mtf->data){
      if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_STI))
       call_func_ownstack_withdata_sti(mtf->data,mtf_func,&mtf->oldstack,&mtf->newstack);
      else
       call_func_ownstack_withdata(mtf->data,mtf_func,&mtf->oldstack,&mtf->newstack);
     }else{
      if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_STI))
       call_func_ownstack_nodata_sti(mtf_func,&mtf->oldstack,&mtf->newstack);
      else
       call_func_ownstack_nodata(mtf_func,&mtf->oldstack,&mtf->newstack);
     }
    }else
#endif
    { // no stack, no sti
     if(funcbit_smp_pointer_get(mtf->data))
      ((call_timedfunc_withdata)(mtf_func))(mtf->data);
     else
      ((call_timedfunc_nodata)(mtf_func))();
    }

    if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
     break;

    mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];

    if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT))
     newfunc_timer_delete_entry(mtf);

    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);
   }
  }
 }

 if(funcbit_smp_value_get(recall_counter)>0)
  funcbit_smp_value_decrement(recall_counter);
}

static void mpxplay_timer_delete_int08_funcs(void)
{
 unsigned int i,retry,countend=funcbit_smp_value_get(int08counter)+64;
 if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
  return;
#ifdef MPXPLAY_WIN32
 int08counter=GetTickCount()+64;
#endif
 do{
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)mpxplay_timed_functions;
  retry=0;
  for(i=0;i<mtf_storage;i++,mtf++){
   if(mtf->func && funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_INT08)){
#ifdef MPXPLAY_WIN32
    int08counter=GetTickCount();
#endif
    if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY) || (int08counter>countend)){
     funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);
     newfunc_timer_delete_entry(mtf);
    }else{
     funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT);
     retry=1;
    }
   }
  }
 }while(retry);
}

//----------------------------------------------------------------------
// INT08 and win32 thread handling (interrupt decoder/dma_monitor/etc.)

#ifdef MPXPLAY_WIN32
#include <winbase.h>
#include <process.h>
#include <aclapi.h>

#define MPXPLAY_MAINTHREAD_RIGHTS (THREAD_TERMINATE|THREAD_SUSPEND_RESUME|THREAD_SET_INFORMATION)
#define MPXPLAY_INT08THREAD_RIGHTS (THREAD_TERMINATE|THREAD_SUSPEND_RESUME|THREAD_SET_INFORMATION)
#define MPXPLAY_THREAD_AFFINITY_MASK 0x00000001

#if defined(__WATCOMC__)
BOOL WINAPI RequestWakeupLatency(LATENCY_TIME latency);
#endif

static HANDLE int08_timer_thread_handle;
static HANDLE int08_thread_handle;
static HANDLE handle_maincycle1,handle_maincycle2;
static void *main_cycle1,*main_cycle2;
static DWORD int08_thread_id;
static HANDLE int08_timer_handle;

static void newhandler_08_timer_thread(void *empty)
{
 LARGE_INTEGER DueTime;
 if(!int08_timer_handle){
  int08_timer_handle=CreateWaitableTimer(NULL,0,NULL);
  if(int08_timer_handle){
   DueTime.QuadPart=-100;
   SetWaitableTimer(int08_timer_handle,&DueTime,(long)(1000/INT08_CYCLES_NEW/2),NULL,NULL,0);
  }else{

  }
 }
 RequestWakeupLatency(LT_LOWEST_LATENCY);

 do{
  funcbit_smp_value_increment(int08counter);
  WaitForSingleObjectEx(int08_timer_handle,(long)(1000/INT08_CYCLES_NEW)+2,1);
 }while(mpxplay_timed_functions);

 if(int08_timer_handle){
  CancelWaitableTimer(int08_timer_handle);
  CloseHandle(int08_timer_handle);
  int08_timer_handle=NULL;
 }
}

static void newhandler_08_thread(void *empty)
{
 int08_thread_id=GetCurrentThreadId();

 do{
  funcbit_smp_enable(intsoundcontrol,INTSOUND_INT08RUN);
  intdec_timer_counter=GetTickCount();
  if(handle_maincycle1)
   SuspendThread(handle_maincycle1);
  if(handle_maincycle2)
   SuspendThread(handle_maincycle2);
  mpxplay_timer_execute_int08_funcs();
  funcbit_smp_disable(intsoundcontrol,INTSOUND_INT08RUN);
  if(handle_maincycle1)
   ResumeThread(handle_maincycle1);
  if(handle_maincycle2)
   ResumeThread(handle_maincycle2);
  WaitForSingleObjectEx(int08_timer_handle,(long)(1000/INT08_CYCLES_NEW)+2,1);
 }while(mpxplay_timed_functions);
}

void newfunc_newhandler08_init(void)
{
 //TIMECAPS tc;
 //char sout[100];

 //timeGetDevCaps(&tc,sizeof(tc));

 //sprintf(sout,"min:%d max:%d",tc.wPeriodMin,tc.wPeriodMax);
 //pds_textdisplay_printf(sout);
 //getch();

#ifndef MPXPLAY_USE_SMP
 {
  HANDLE curr_process=GetCurrentProcess();
  if(curr_process)
   SetProcessAffinityMask(curr_process,MPXPLAY_THREAD_AFFINITY_MASK);
 }
#endif

 if(!int08_timer_thread_handle){
  int08_timer_thread_handle=(HANDLE)_beginthread((void *)newhandler_08_timer_thread,0,(void *)&intsoundcontrol);
  if(int08_timer_thread_handle){
   SetThreadPriority(int08_timer_thread_handle,THREAD_PRIORITY_HIGHEST);
   SetSecurityInfo(int08_timer_thread_handle,SE_KERNEL_OBJECT,MPXPLAY_INT08THREAD_RIGHTS,NULL,NULL,NULL,NULL);
#ifndef MPXPLAY_USE_SMP
   SetThreadAffinityMask(int08_timer_thread_handle,MPXPLAY_THREAD_AFFINITY_MASK);
#endif
  }else{

  }
 }

 if(!int08_thread_handle){
  int08_thread_handle=(HANDLE)_beginthread((void *)newhandler_08_thread,0,(void *)&intsoundcontrol);
  if(int08_thread_handle){
   SetThreadPriority(int08_thread_handle,THREAD_PRIORITY_HIGHEST);
   SetSecurityInfo(int08_thread_handle,SE_KERNEL_OBJECT,MPXPLAY_INT08THREAD_RIGHTS,NULL,NULL,NULL,NULL);
#ifndef MPXPLAY_USE_SMP
   SetThreadAffinityMask(int08_thread_handle,MPXPLAY_THREAD_AFFINITY_MASK);
#endif
  }else{

  }
 }
}

unsigned int newfunc_newhandler08_is_current_thread(void)
{
 if(GetCurrentThreadId()==int08_thread_id)
  return 1;
 return 0;
}

void newfunc_newhandler08_waitfor_threadend(void)
{
 if(!newfunc_newhandler08_is_current_thread()){
  unsigned int retry=10;
  while(funcbit_smp_test(intsoundcontrol,INTSOUND_INT08RUN) && (--retry))
   Sleep(0);
 }
}

void newfunc_newhandler08_close(void)
{
 funcbit_smp_disable(intsoundcontrol,INTSOUND_DECODER);
 if(int08_thread_handle){
  newfunc_newhandler08_waitfor_threadend();
  SuspendThread(int08_thread_handle);
 }
 if(handle_maincycle1)   SuspendThread(handle_maincycle1);
 if(handle_maincycle2)   SuspendThread(handle_maincycle2);
 mpxplay_timer_close();
 if(int08_thread_handle) TerminateThread(int08_thread_handle,0);
 if(handle_maincycle1)   TerminateThread(handle_maincycle1,0);
 if(handle_maincycle2)   TerminateThread(handle_maincycle2,0);
 if(int08_timer_handle){
  CancelWaitableTimer(int08_timer_handle);
  CloseHandle(int08_timer_handle);
  int08_timer_handle=NULL;
 }
 if(int08_timer_thread_handle) TerminateThread(int08_timer_thread_handle,0);
 if(int08_thread_handle) CloseHandle(int08_thread_handle);
#ifndef WIN32
 if(handle_maincycle1)   CloseHandle(handle_maincycle1);
#endif
 if(handle_maincycle2)   CloseHandle(handle_maincycle2);
#ifndef WIN32
 if(int08_timer_thread_handle) CloseHandle(int08_timer_thread_handle);
#endif
}

static unsigned long thread_maincycle_1(struct mainvars *mvp)
{
 do{
  ((call_timedfunc_nodata)(main_cycle1))();
  if(main_cycle2 || funcbit_test(intsoundcontrol,INTSOUND_INT08RUN))
   Sleep(0);
 }while(mvps.partselect);
 return 0;
}

static unsigned long thread_maincycle_2(struct mainvars *mvp)
{
 do{
  ((call_timedfunc_nodata)(main_cycle2))();
  if(!funcbit_smp_test(mpxplay_signal_events,MPXPLAY_SIGNALMASK_OTHER))
   Sleep(1000/INT08_CYCLES_NEW);
  else
   Sleep(0);
 }while(mvps.partselect);
 return 0;
}

unsigned int newfunc_newhandler08_maincycles_init(struct mainvars *mvp,void *cycle1,void *cycle2)
{
 if(cycle1){
  main_cycle1=cycle1;
  handle_maincycle1=(HANDLE)_beginthread((void *)thread_maincycle_1,0,(void *)mvp);
  if(!handle_maincycle1)
   return 0;
  SetSecurityInfo(handle_maincycle1,SE_KERNEL_OBJECT,MPXPLAY_MAINTHREAD_RIGHTS,NULL,NULL,NULL,NULL);
#ifndef MPXPLAY_USE_SMP
  SetThreadAffinityMask(handle_maincycle1,MPXPLAY_THREAD_AFFINITY_MASK);
#endif
 }
 if(cycle2){
  main_cycle2=cycle2;
  handle_maincycle2=(HANDLE)_beginthread((void *)thread_maincycle_2,0,(void *)mvp);
  if(!handle_maincycle2)
   return 0;
  SetSecurityInfo(handle_maincycle2,SE_KERNEL_OBJECT,MPXPLAY_MAINTHREAD_RIGHTS,NULL,NULL,NULL,NULL);
#ifndef MPXPLAY_USE_SMP
  SetThreadAffinityMask(handle_maincycle2,MPXPLAY_THREAD_AFFINITY_MASK);
#endif
 }
 return 1;
}

#elif defined(__DOS__)

static unsigned int oldint08_timercount;
void (__far __interrupt *oldint08_handler)();

void loades(void);
#pragma aux loades = "push ds" "pop es"

void savefpu(void);
#pragma aux savefpu = "sub esp,200" "fsave [esp]"

void clearfpu(void);
#pragma aux clearfpu = "finit"

void restorefpu(void);
#pragma aux restorefpu = "frstor [esp]" "add esp,200"

void cld(void);
#pragma aux cld="cld"

static void __interrupt __loadds newhandler_08(void)
{
 savefpu();
 clearfpu();
#ifdef __WATCOMC__
 loades();
#endif

 int08counter++; // for the general timing

 intdec_timer_counter+=INT08_DIVISOR_NEW; // for CPU usage (at interrupt-decoder)

 oldint08_timercount+=INT08_DIVISOR_NEW; // for the old-int08 handler

 if((oldint08_timercount&0xFFFF0000) && !oldint08_running){
  oldint08_running=1;
  oldint08_timercount-=0x00010000;
  oldint08_handler();
  cld();
  oldint08_running=0;
 }else{
  outp(0x20,0x20);
 }

 mpxplay_timer_execute_int08_funcs();

 restorefpu();
}

void newfunc_newhandler08_init(void)
{
 if(!oldint08_handler){
  oldint08_handler=(void (__far __interrupt *)())pds_dos_getvect(MPXPLAY_TIMER_INT);
  pds_dos_setvect(MPXPLAY_TIMER_INT,newhandler_08);
  outp(0x43, 0x34);
  outp(0x40, (INT08_DIVISOR_NEW&0xff));
  outp(0x40, (INT08_DIVISOR_NEW>>8));
 }
}

void newfunc_newhandler08_close(void)
{
 funcbit_smp_disable(intsoundcontrol,INTSOUND_DECODER);
 mpxplay_timer_close();
 if(oldint08_handler){
  pds_dos_setvect(MPXPLAY_TIMER_INT,oldint08_handler);
  outp(0x43, 0x34);
  outp(0x40, 0x00);
  outp(0x40, 0x00);
 }
}

#else // other OSes :)

#endif
