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
//function: COM,UIR,LPT,NECIR,VT100,Serial-Mouse control

#include "control.h"
#include "newfunc\newfunc.h"
#include "newfunc\dll_load.h"
#include "display\display.h"

//#define MPX_DEBUG_SERIAL 1

#ifdef MPX_DEBUG_SERIAL
 //#define MPX_DEBUG_SER_IRQ 1
#endif

#if defined(__DOS__) || defined(MPXPLAY_WIN32)
 #define MPXPLAY_LINK_SERIAL_PORTHAND 1
#endif

#ifdef MPXPLAY_LINK_SERIAL_PORTHAND
 #ifdef MPXPLAY_WIN32
 //#define MPXPLAY_LINK_SERIAL_COMCNTRL 1
 #define MPXPLAY_LINK_SERIAL_UIR      1
 //#define MPXPLAY_LINK_SERIAL_LPTCNTRL 1
 //#define MPXPLAY_LINK_SERIAL_NECIR    1
 //#define MPXPLAY_LINK_SERIAL_VT100    1
 //#define MPXPLAY_LINK_SERIAL_SERMOUSE 1
 #else
 #define MPXPLAY_LINK_SERIAL_COMCNTRL 1
 #define MPXPLAY_LINK_SERIAL_UIR      1
 #define MPXPLAY_LINK_SERIAL_LPTCNTRL 1
 #define MPXPLAY_LINK_SERIAL_NECIR    1
 #define MPXPLAY_LINK_SERIAL_VT100    1
 #define MPXPLAY_LINK_SERIAL_SERMOUSE 1
 #endif
#endif

#ifdef MPXPLAY_LINK_WATTCP32 // mpxplay.h
 #define MPXPLAY_LINK_SERIAL_TCPIP 1
#endif

#ifdef MPXPLAY_LINK_SERIAL_TCPIP
 #define MPXPLAY_LINK_SERIAL_FTPSRV  1
 #define MPXPLAY_LINK_SERIAL_TELSRV  1
 //#define MPXPLAY_LINK_SERIAL_IRCSRV  1
 //#define MPXPLAY_LINK_SERIAL_LIRC_DEBUG 1
#endif

#define SERIAL_MAX_LOADED_HANDLERS 16 // at once
#define SERIAL_MAX_FUNCS           99
#define SERIAL_MAX_INDATALEN       16
#define SERIAL_DIRECTKEY_SELECT   128 // if indatalen==128 then directkeycodes used (in handler_comcntrl)
#define SERIAL_HANDDATAFLAG_DIRECTKEY 1
#define SERIAL_READPORT_REFRESH    18 // in 1 sec

static void mpxplay_control_serial_readport(void);

#ifdef MPXPLAY_LINK_SERIAL_COMCNTRL
static unsigned int serial_handler_comcntrl_init(struct serial_handler_data_s *);
static void         serial_handler_comcntrl_close(struct serial_handler_data_s *);
#endif
#ifdef MPXPLAY_LINK_SERIAL_UIR
static unsigned int serial_handler_uir_init(struct serial_handler_data_s *);
static void         serial_handler_uir_close(struct serial_handler_data_s *);
#ifdef MPXPLAY_WIN32
static void         serial_handler_uir_read(struct serial_handler_data_s *);
#endif
#endif
#ifdef MPXPLAY_LINK_SERIAL_LPTCNTRL
static unsigned int serial_handler_lptcntrl_initi(struct serial_handler_data_s *);
static unsigned int serial_handler_lptcntrl_initr(struct serial_handler_data_s *);
static void         serial_handler_lptcntrl_close(struct serial_handler_data_s *);
static void         serial_handler_lptcntrl_read(struct serial_handler_data_s *);
#endif
#ifdef MPXPLAY_LINK_SERIAL_NECIR
static unsigned int serial_handler_necir_init(struct serial_handler_data_s *);
static void         serial_handler_necir_close(struct serial_handler_data_s *);
#endif
#ifdef MPXPLAY_LINK_SERIAL_VT100
static unsigned int serial_handler_vt100_init(struct serial_handler_data_s *);
static void         serial_handler_vt100_close(struct serial_handler_data_s *);
#endif
#ifdef MPXPLAY_LINK_SERIAL_SERMOUSE
static unsigned int serial_handler_sermouse_init(struct serial_handler_data_s *);
static void         serial_handler_sermouse_close(struct serial_handler_data_s *);
#endif
#ifdef MPXPLAY_LINK_SERIAL_FTPSRV
 static unsigned int serial_handler_ftpsrv_init(struct serial_handler_data_s *);
 static void         serial_handler_ftpsrv_close(struct serial_handler_data_s *);
 static void         serial_handler_ftpsrv_read(struct serial_handler_data_s *);
#endif
#ifdef MPXPLAY_LINK_SERIAL_TELSRV
 static unsigned int serial_handler_telsrv_init(struct serial_handler_data_s *);
 static void         serial_handler_telsrv_close(struct serial_handler_data_s *);
 static void         serial_handler_telsrv_read(struct serial_handler_data_s *);
#endif
#ifdef MPXPLAY_LINK_SERIAL_IRCSRV
 static unsigned int serial_handler_ircsrv_init(struct serial_handler_data_s *);
 static void         serial_handler_ircsrv_close(struct serial_handler_data_s *);
 static void         serial_handler_ircsrv_read(struct serial_handler_data_s *);
 #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
  static void lircStr(char *,unsigned long,unsigned int);
 #endif
#endif

typedef struct serial_port_data_s{
 unsigned int baud;           // at COM ports
#ifdef MPXPLAY_WIN32
 HANDLE comm_thread_handle;
 HANDLE comm_port_handle;
 unsigned int error_retry;
 DCB dcb;
#else
 unsigned int baseport;
 unsigned int oldportirqflag;
#endif
}serial_port_data_s;

typedef struct serial_irq_data_s{
 unsigned int irqnum;
 unsigned int oldirqflag;
 void (__far __interrupt *oldhandler)();
}serial_irq_data_s;

typedef struct serial_func_data_s{
 unsigned char databytes[SERIAL_MAX_INDATALEN];
 unsigned short keycode;
}serial_func_data_s;

typedef struct serial_handler_data_s{
 unsigned int portselect;   // 1-4 : COM , 5-8 : LPT
 unsigned int indatalen;
 unsigned int flags;
 struct serial_port_data_s portdatas;
 struct serial_irq_data_s  irqdatas;
 void *private_datas;
 unsigned int number_of_funcs;
 struct serial_func_data_s serial_funcs[SERIAL_MAX_FUNCS];
 unsigned int inbytecount;
 unsigned char inbytesstore[SERIAL_MAX_INDATALEN+1];
}serial_handler_data_s;

typedef struct serial_handler_routine_s{
 char *handname;
 unsigned int (*initport)(struct serial_handler_data_s *);
 void (*closeport)(struct serial_handler_data_s *);
 void (*readport)(struct serial_handler_data_s *);
}serial_handler_routine_s;

typedef struct serial_handler_main_s{
 struct serial_handler_routine_s *handlerroutines;
 struct serial_handler_data_s    *handlerdatas;
}serial_handler_main_s;

#if defined(MPXPLAY_LINK_SERIAL_PORTHAND) || defined(MPXPLAY_LINK_SERIAL_TCPIP)

static serial_handler_routine_s serial_static_handler_routines[]={
#ifdef MPXPLAY_LINK_SERIAL_COMCNTRL
 {"COMC",   &serial_handler_comcntrl_init,&serial_handler_comcntrl_close,NULL},
#endif
#ifdef MPXPLAY_LINK_SERIAL_UIR
 #ifdef MPXPLAY_WIN32
 {"UIR",    &serial_handler_uir_init,     &serial_handler_uir_close     ,&serial_handler_uir_read},
 #else
 {"UIR",    &serial_handler_uir_init,     &serial_handler_uir_close     ,NULL},
 #endif
#endif
#ifdef MPXPLAY_LINK_SERIAL_LPTCNTRL
 {"LPTC",   &serial_handler_lptcntrl_initi,&serial_handler_lptcntrl_close,NULL},
 {"LPTR",   &serial_handler_lptcntrl_initr,&serial_handler_lptcntrl_close,&serial_handler_lptcntrl_read},
#endif
#ifdef MPXPLAY_LINK_SERIAL_NECIR
 {"NECIR",  &serial_handler_necir_init,   &serial_handler_necir_close   ,NULL},
#endif
#ifdef MPXPLAY_LINK_SERIAL_VT100
 {"VT100",  &serial_handler_vt100_init,   &serial_handler_vt100_close   ,NULL},
#endif
#ifdef MPXPLAY_LINK_SERIAL_SERMOUSE
 {"SERMOUS",&serial_handler_sermouse_init,&serial_handler_sermouse_close,NULL}
#endif
#ifdef MPXPLAY_LINK_SERIAL_FTPSRV
 ,{"FTPSRV", &serial_handler_ftpsrv_init,&serial_handler_ftpsrv_close,&serial_handler_ftpsrv_read}
#endif
#ifdef MPXPLAY_LINK_SERIAL_TELSRV
 ,{"TELSRV", &serial_handler_telsrv_init,&serial_handler_telsrv_close,&serial_handler_telsrv_read}
#endif
#ifdef MPXPLAY_LINK_SERIAL_IRCSRV
 ,{"IRCSRV", &serial_handler_ircsrv_init,&serial_handler_ircsrv_close,&serial_handler_ircsrv_read}
#endif
};

#define SERIAL_NUMBER_OF_STATIC_HANDLERS (sizeof(serial_static_handler_routines)/sizeof(serial_handler_routine_s)-1)

static serial_handler_main_s *serial_loaded_handlers;

static unsigned int serial_main_enable,serial_loaded_handlercount;
#if defined(MPXPLAY_LINK_SERIAL_PORTHAND) && defined(__DOS__)
static char *biosmem=(char *)0;
#endif

static mpxini_var_s serial_base_vars[]={
 {"SerialEnable", &serial_main_enable,  ARG_NUM},
 {NULL,NULL,0}
};
#endif

//--------------------------------------------------------------------
// load config from mpxplay.ini (called from control.c)
#if defined(MPXPLAY_LINK_SERIAL_PORTHAND) || defined(MPXPLAY_LINK_SERIAL_TCPIP)
static int pds_hextoint_1char(char *str)
{
 int num;
 if(str[0]>='0' && str[0]<='9')
  num=(int)str[0]-'0';
 else
  if(str[0]>='a' && str[0]<='f')
   num=(int)str[0]-'a'+10;
  else
   if(str[0]>='A' && str[0]<='F')
    num=(int)str[0]-'A'+10;
   else
    num=-1;
 return num;
}
#endif

void mpxplay_control_serial_loadini(mpxini_line_t *mpxini_lines,struct mpxini_part_t *mpxini_partp)
{
#if defined(MPXPLAY_LINK_SERIAL_PORTHAND) || defined(MPXPLAY_LINK_SERIAL_TCPIP)
 unsigned int i,mpxlinecounter,error;
 mpxini_line_t *linep;
 struct serial_handler_data_s *current_handlerdatas;
 unsigned int current_funccount;
 char strtmp[MPXINI_MAX_CHARSPERLINE],sout[MPXINI_MAX_CHARSPERLINE];

 mpxplay_control_general_loadini(mpxini_lines,mpxini_partp,serial_base_vars);

 if(!serial_main_enable)
  return;

 serial_loaded_handlers=(serial_handler_main_s *)calloc(SERIAL_MAX_LOADED_HANDLERS,sizeof(struct serial_handler_main_s));
 if(!serial_loaded_handlers){
  display_warning_message("Couldn't alloc handlers (not enough memory)!");
  return;
 }

 linep=mpxini_lines+mpxini_partp->partbegin_linenum;

 current_handlerdatas=NULL;
 serial_loaded_handlercount=0;

 for(mpxlinecounter=0;mpxlinecounter<mpxini_partp->partlinenum;mpxlinecounter++,linep++){
  if(linep->varnamep){
   if(pds_strlicmp(linep->varnamep,"HandlerCfg")==0){
    char *currdata,*nextdata;
    struct serial_handler_routine_s *handrp;
    struct serial_handler_main_s *handp;
    unsigned int portselect=0,baud=0,indatalen=0;

    if(current_handlerdatas){
     current_handlerdatas->number_of_funcs=current_funccount;
     current_handlerdatas=NULL;
    }
    if(serial_loaded_handlercount>=SERIAL_MAX_LOADED_HANDLERS)
     break;

    pds_strcpy(strtmp,linep->valuep);
    // handler name (COMC,UIR,etc.)
    currdata=&strtmp[0];
    nextdata=pds_strchr(currdata,','); // end of handlername
    if(nextdata)
     *nextdata++=0;
    if(!pds_strcutspc(currdata)){
     display_warning_message("Warning: empty HandlerCFG");
     continue; // skip line (handler)
    }
    handrp=&serial_static_handler_routines[0];
    for(i=0;i<=SERIAL_NUMBER_OF_STATIC_HANDLERS;i++,handrp++)
     if(pds_stricmp(handrp->handname,currdata)==0)
      break;
    if(i>SERIAL_NUMBER_OF_STATIC_HANDLERS){
#ifdef MPXPLAY_LINK_DLLLOAD
     mpxplay_module_entry_s *dll_serhand=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_CONTROL_SERIAL,0,currdata,NULL);
     //fprintf(stdout,"dll:%8.8X sv:%4.4X\n",dll_serhand,dll_serhand->module_structure_version);
     if(dll_serhand && (dll_serhand->module_structure_version==MPXPLAY_DLLMODULEVER_CONTROL_SERIAL)){ // !!!
      handrp=(serial_handler_routine_s *)dll_serhand->module_callpoint;
     }else
#endif
     {
      snprintf(sout,sizeof(sout),"Warning: unknown serial handler: %s ",currdata);
      display_warning_message(sout);
      continue; // skip line (handler)
     }
    }
    // port name (COM1,LPT1,etc.)
    if(nextdata){
     currdata=nextdata;
     nextdata=pds_strchr(currdata,',');
     if(nextdata)
      *nextdata++=0;
     pds_strcutspc(currdata);
     if(pds_strlicmp(currdata,"COM")==0){
      portselect=pds_atol(&currdata[sizeof("COM")-1]);
      if(!portselect || portselect>4){
       snprintf(sout,sizeof(sout),"Warning: invalid port : COM%d at handler %s ",portselect,handrp->handname);
       display_warning_message(sout);
       continue; // skip line (handler)
      }
     }else
      if(pds_strlicmp(currdata,"LPT")==0){
       portselect=pds_atol(&currdata[sizeof("LPT")-1]);
       if(!portselect || portselect>3){
        snprintf(sout,sizeof(sout),"Warning: invalid port : LPT%d at handler %s ",portselect,handrp->handname);
        display_warning_message(sout);
        continue; // skip line (handler)
       }
       portselect+=4;
      }
     // baud
     if(nextdata){
      currdata=nextdata;
      nextdata=pds_strchr(currdata,',');
      if(nextdata)
       *nextdata++=0;
      baud=pds_atol(currdata);
      // indatalen
      if(nextdata){
       currdata=nextdata;
       indatalen=pds_atol(currdata);
      }
     }
    }

    // all values ok, alloc and store datas
    handp=&serial_loaded_handlers[serial_loaded_handlercount];
    handp->handlerdatas=calloc(1,sizeof(struct serial_handler_data_s));
    if(!handp->handlerdatas){
     snprintf(sout,sizeof(sout),"Warning: couldn't alloc datafield for handler %s -> disabled ",handrp->handname);
     display_warning_message(sout);
     continue; // skip line (handler)
    }
    current_handlerdatas=handp->handlerdatas;
    current_handlerdatas->portselect=portselect;
    current_handlerdatas->portdatas.baud=baud;
    if(indatalen==SERIAL_DIRECTKEY_SELECT){
     funcbit_enable(current_handlerdatas->flags,SERIAL_HANDDATAFLAG_DIRECTKEY);
     indatalen=2;
    }
    current_handlerdatas->indatalen=indatalen;
    handp->handlerroutines=handrp;
    serial_loaded_handlercount++;

    current_funccount=0;
    #ifdef MPX_DEBUG_SERIAL
    sprintf(sout,"hand:%s port:%d baud:%d datalen:%d ",
                    handrp->handname,current_handlerdatas->portselect,current_handlerdatas->portdatas.baud,current_handlerdatas->indatalen);
    pds_textdisplay_printf(sout);
    #endif
   }else{
    if(pds_strlicmp(linep->varnamep,"SerialFunc")==0 && current_handlerdatas && (current_funccount<SERIAL_MAX_FUNCS)){
     char *indatap=linep->valuep;
     unsigned char *datab=current_handlerdatas->serial_funcs[current_funccount].databytes;
     int numh,numl;

     //read indata bytes
     error=i=0;
     while(*indatap!=',' && (i++<current_handlerdatas->indatalen)){
      numh=pds_hextoint_1char(indatap);
      if(numh<0){
       error=1;
       break;
      }
      indatap++;
      numl=pds_hextoint_1char(indatap);
      if(numl<0){
       error=1;
       break;
      }
      indatap++;
      *datab++=(numh<<4)|numl;
     }
     if(error) // found bad (non-hexa) character(s) before reach indatalen
      continue;

     while(*indatap!=',' && *indatap!=0) // skip extra chars (spaces or any), search ','
      indatap++;

     if(*indatap==0)  // end of line (',' not found)
      continue;

     while((*indatap==',' || *indatap==' ') && *indatap!=0) // skip ',' and ' ', search any other
      indatap++;

     if(*indatap==0) // end of line (keycode not found)
      continue;

     //read keyboard-code
     numh=0;
     for(i=0;i<4;i++){
      numl=pds_hextoint_1char(indatap);
      if(numl<0){
       error=1;
       break;
      }
      numh=(numh<<4)|numl;
      indatap++;
     }

     if(error)
      continue;

     current_handlerdatas->serial_funcs[current_funccount].keycode=numh;

     current_funccount++;
    }
   }
  }
 }

 if(!serial_loaded_handlercount){
  display_warning_message("Warning: no serial-handler is configured!");
  serial_main_enable=0;
  return;
 }

 if(current_handlerdatas)
  current_handlerdatas->number_of_funcs=current_funccount;

#endif
}

void mpxplay_control_serial_init(void)
{
#if defined(MPXPLAY_LINK_SERIAL_PORTHAND) || defined(MPXPLAY_LINK_SERIAL_TCPIP)
 unsigned int i,found,enable_readport;
 char sout[100];
 if(serial_main_enable && serial_loaded_handlers){
  struct serial_handler_main_s *handp=&serial_loaded_handlers[0];
  found=enable_readport=0;
  for(i=0;i<serial_loaded_handlercount;i++,handp++){
   if(handp->handlerdatas){
    if(!handp->handlerroutines->initport || handp->handlerroutines->initport(handp->handlerdatas)){
     found=1;
     if(handp->handlerroutines->readport)
      enable_readport=1;
    }else{
     snprintf(sout,sizeof(sout),"Warning: Couldn't initialize port control (%s on %s%d)!",
             handp->handlerroutines->handname,
             (handp->handlerdatas->portselect<=4)? "COM":"LPT",
             (handp->handlerdatas->portselect<=4)? handp->handlerdatas->portselect:(handp->handlerdatas->portselect-4));
     display_warning_message(sout);
     if(handp->handlerroutines->closeport)
      handp->handlerroutines->closeport(handp->handlerdatas);
     free(handp->handlerdatas);
     handp->handlerdatas=NULL;
    }
   }
  }
  if(found){
   if(enable_readport)
    mpxplay_timer_addfunc(&mpxplay_control_serial_readport,NULL,MPXPLAY_TIMERTYPE_REPEAT,mpxplay_timer_secs_to_counternum(1)/SERIAL_READPORT_REFRESH);
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("serial init ok");
   #endif
  }else
   serial_main_enable=0;
 }
#endif
}

void mpxplay_control_serial_close(void)
{
#if defined(MPXPLAY_LINK_SERIAL_PORTHAND) || defined(MPXPLAY_LINK_SERIAL_TCPIP)
 int i;
 if(serial_loaded_handlers){
  struct serial_handler_main_s *handp=&serial_loaded_handlers[serial_loaded_handlercount];
  for(i=0;i<serial_loaded_handlercount;i++){
   handp--;                           // !!! reversed close
   if(handp->handlerdatas){
    if(handp->handlerroutines->closeport)
     handp->handlerroutines->closeport(handp->handlerdatas);
    free(handp->handlerdatas);
    handp->handlerdatas=NULL;
   }
  }
  free(serial_loaded_handlers);
  serial_loaded_handlers=NULL;
  serial_loaded_handlercount=0;
 }
#endif
}

#if defined(MPXPLAY_LINK_SERIAL_PORTHAND) || defined(MPXPLAY_LINK_SERIAL_TCPIP)
static void mpxplay_control_serial_readport(void)
{
 unsigned int i;
 struct serial_handler_main_s *handp=&serial_loaded_handlers[0];

 for(i=0;i<serial_loaded_handlercount;i++,handp++)
  if(handp->handlerroutines->readport && handp->handlerdatas)
   handp->handlerroutines->readport(handp->handlerdatas);
}
#endif

//--------------------------------------------------------------------------
#ifdef MPXPLAY_LINK_SERIAL_PORTHAND

// common routines for HandlerCFG=COMC and HandlerCFG=UIR
// some routines are based on the dosir2pc source

#ifdef MPX_DEBUG_SERIAL
 static char sout[200];
 unsigned int inchar;
#endif

//-------------------------------------------------------------------------
// common com and lpt-port init/close (useful for every control handlers)

#ifdef MPXPLAY_WIN32

/*static unsigned int serial_interrupt_init(struct serial_handler_data_s *handdatas,void *newirqhandler)
{
 DWORD thid;
 handdatas->portdatas.comm_thread_handle=CreateThread(NULL,65536,newirqhandler,(void *)handdatas,CREATE_NO_WINDOW,&thid);
 if(handdatas->portdatas.comm_thread_handle == INVALID_HANDLE_VALUE)
  return 0;
 return 1;
}

static void serial_interrupt_close(struct serial_handler_data_s *handdatas)
{
 if(handdatas->portdatas.comm_thread_handle){
  TerminateThread(handdatas->portdatas.comm_thread_handle,0);
  CloseHandle(handdatas->portdatas.comm_thread_handle);
 }
}*/

static void serial_com_port_reset(struct serial_port_data_s *portdatas)
{
 if(portdatas->comm_port_handle)
  PurgeComm(portdatas->comm_port_handle,0x000f);
}

static unsigned int serial_com_port_init(struct serial_port_data_s *portdatas,unsigned int portselect,unsigned int useirq)
{
 char *pcCommPort = "COM0";
 DCB *dcb=&portdatas->dcb;

 if(portselect>4)
  return 0;

 pcCommPort[3]='0'+portselect;

 portdatas->comm_port_handle = CreateFile( pcCommPort,
                    GENERIC_READ | GENERIC_WRITE,
                    0,    // must be opened with exclusive-access
                    NULL, // no security attributes
                    OPEN_EXISTING, // must use OPEN_EXISTING
                    0,//FILE_FLAG_OVERLAPPED,
                    NULL  // hTemplate must be NULL for comm devices
                    );

 if(portdatas->comm_port_handle == INVALID_HANDLE_VALUE)
  return 0;

 if(!GetCommState(portdatas->comm_port_handle, dcb))
  return 0;

 dcb->BaudRate = portdatas->baud;// set the baud rate
 dcb->ByteSize = 8;              // data size, xmit, and rcv
 dcb->Parity   = NOPARITY;       // no parity bit
 dcb->StopBits = ONESTOPBIT;     // one stop bit

 if(!SetCommState(portdatas->comm_port_handle, dcb))
  return 0;

 //EscapeCommFunction(portdatas->comm_port_handle,7); // RESETDEV ???

 if(!SetCommMask(portdatas->comm_port_handle, EV_RXCHAR))
  return 0;

 serial_com_port_reset(portdatas);

 return 1;
}

static void serial_com_port_close(struct serial_port_data_s *portdatas)
{
 if(portdatas->comm_port_handle){
  serial_com_port_reset(portdatas);
  CloseHandle(portdatas->comm_port_handle);
  portdatas->comm_port_handle=0;
 }
}

static unsigned int serial_com_port_read(struct serial_port_data_s *portdatas,unsigned char *readbuf,unsigned int readlen,unsigned int usewait)
{
 //DWORD EvtMask=0,b=0;
 DWORD k=0;
 DWORD nb_bytes_read=0;
 //OVERLAPPED o;
 COMSTAT c;
 //char sout[100];

 *readbuf=0;
 if(portdatas->comm_port_handle){
  if(usewait){
   /*o.hEvent = CreateEvent(
        NULL,   // default security attributes
        TRUE,   // manual-reset event
        FALSE,  // not signaled
        NULL    // no name
		);
   o.Internal = 0;
   o.InternalHigh = 0;
   o.Offset = 0;
   o.OffsetHigh = 0;
#ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("WaitCommEvent");
#endif
   if(!WaitCommEvent(portdatas->comm_port_handle,&EvtMask,&o))
    goto err_out;*/
#ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("ClearCommError");
#endif
   pds_memset(&c,0,sizeof(c));
   ClearCommError(portdatas->comm_port_handle,&k,&c);
#ifdef MPX_DEBUG_SERIAL
   sprintf(sout,"reado: evt:%8.8X ob:%d k:%d",EvtMask,c.cbInQue,k);
   pds_textdisplay_printf(sout);
#endif
   if(c.cbInQue<readlen)
    return 0;
   /*if(!funcbit_test(EvtMask,EV_RXCHAR)){
#ifdef MPX_DEBUG_SERIAL
    sprintf(sout,"read1: evt:%8.8X",EvtMask);
    pds_textdisplay_printf(sout);
#endif
    goto err_out;
   }*/
  }
#ifdef MPX_DEBUG_SERIAL
  pds_textdisplay_printf("readfile begin");
#endif
  ReadFile(portdatas->comm_port_handle,readbuf,readlen,&nb_bytes_read,NULL);
 }
//err_out:
#ifdef MPX_DEBUG_SERIAL
  sprintf(sout,"read: %2X  l:%d h:%8.8X pd:%8.8X",(unsigned int)*readbuf,nb_bytes_read,portdatas->comm_port_handle,portdatas);
  pds_textdisplay_printf(sout);
#endif
 return nb_bytes_read;
}

static unsigned int serial_com_port_write(struct serial_port_data_s *portdatas,unsigned char byte)
{
 unsigned char bb=byte;
 DWORD nb=0;
 if(portdatas->comm_port_handle){
  WriteFile(portdatas->comm_port_handle,&bb,1,&nb,NULL);
  //if(TransmitCommChar(portdatas->comm_port_handle,(char)byte))
  // return 1;
 }
#ifdef MPX_DEBUG_SERIAL
  sprintf(sout,"write: %2X  %c l:%d h:%8.8X",(unsigned int)bb,bb,nb,portdatas->comm_port_handle);
  pds_textdisplay_printf(sout);
#endif
 return nb;
}

#ifdef MPXPLAY_LINK_SERIAL_LPTCNTRL

static unsigned int serial_lpt_port_init(struct serial_port_data_s *portdatas,unsigned int portselect,unsigned int use_irq)
{
 return 0;
}

static void serial_lpt_port_close(struct serial_port_data_s *portdatas)
{

}

#endif // MPXPLAY_LINK_SERIAL_LPTCNTRL

#else // __DOS__ ---------------------------------------------------------

#define IMR 0x21 // Interrupt Mask Register

#define UART_THR  (com_baseport+0x00) // Transmit Hold Register (Write, DLAB=0)
#define UART_RBR  (com_baseport+0x00) // Receiver Buffer Register (Read, DLAB=0)
#define UART_DLLB (com_baseport+0x00) // Divisor Latch Low Byte (DLAB=1)
#define UART_DLHB (com_baseport+0x01) // Divisor Latch High Byte (DLAB=1)
#define UART_IER  (com_baseport+0x01) // Interrupt Enable Register
#define UART_IIR  (com_baseport+0x02) // Interrupt Identification Register (Read)
#define UART_FCR  (com_baseport+0x02) // FIFO Control Register (Write)
#define UART_LCR  (com_baseport+0x03) // Line Control Register
#define UART_MCR  (com_baseport+0x04) // Modem Control Register
#define UART_LSR  (com_baseport+0x05) // Line Status Register
#define UART_MSR  (com_baseport+0x06) // Modem Status Register (not used here)

#define COM_READ_TIMEOUT 256 // readbyte timeout (retry)

#define BAUD_TO_DIVISOR(baud) (115200/baud)

void loades();
#pragma aux loades = "push ds" "pop es"

static void serial_interrupt_init(struct serial_irq_data_s *irqdatas,unsigned int portselect,void far *newirqhandler)
{
 unsigned int temp;

 switch(portselect){
  case 1:
  case 3:irqdatas->irqnum=4;break;
  case 2:
  case 4:irqdatas->irqnum=3;break;

  case 5:irqdatas->irqnum=7;break;
  case 6:irqdatas->irqnum=5;break;
  case 7:irqdatas->irqnum=7;break;
  default:return;
 }

 _disable();
 irqdatas->oldhandler=(void (__far __interrupt *)())pds_dos_getvect(0x08+irqdatas->irqnum);
 pds_dos_setvect(0x08+irqdatas->irqnum, newirqhandler);

 irqdatas->oldirqflag=inp(IMR)&(1<<irqdatas->irqnum);
 temp = inp(IMR);              // Read the value of the Interrupt Mask Reg
 temp&= ~(1 << irqdatas->irqnum); // set bit to zero (enable irq)
 outp(IMR,temp);               // Send the value back
 _enable();
}

static void serial_interrupt_close(struct serial_irq_data_s *irqdatas)
{
 unsigned int temp;

 if(irqdatas->irqnum){
  _disable();
  temp = inp(IMR);                  // Read the value of the Interrupt Mask Reg
  temp&= ~(1 << irqdatas->irqnum);  // mask out the bit of irq
  temp|=irqdatas->oldirqflag;       // set the old status
  outp(IMR,temp);                   // Send the value back

  pds_dos_setvect(0x08+irqdatas->irqnum,(void *)irqdatas->oldhandler); // restore the original handler
  _enable();
 }
}

//-------------------------------------------------------------------------

static void serial_com_port_reset(struct serial_port_data_s *portdatas)
{
 unsigned int com_baseport=portdatas->baseport;
 outp(UART_FCR,0x07); // clear FIFO
}

static unsigned int serial_com_port_init(struct serial_port_data_s *portdatas,unsigned int portselect,unsigned int useirq)
{
 unsigned int com_baseport;

 if(portselect>4)
  return 0;

 com_baseport=*((unsigned short *)&biosmem[0x400+(portselect-1)*2]);
 if(!com_baseport) // is port enabled?
  return 0;
 portdatas->baseport=com_baseport;

 _disable();

 portdatas->oldportirqflag=inp(UART_IER)&1;
 // setup UART registers
 outp (UART_LCR,0x00);       // DLAB off
 outp (UART_IER,0x00);       // Diasable interrupts
 outp (UART_LCR,0x80);       // DLAB on
 outpw(UART_DLLB,BAUD_TO_DIVISOR(portdatas->baud));// set baud divisor
 outp (UART_LCR,0x03);       // DLAB off, 8 bits, 1 Stop, Noparity
 outp (UART_MCR,0x0b);       // Set RTS & DTR and AUX2 (required for interrupt!)
 outp (UART_FCR,0x07);       // enable & clear FIFO buffers, trigger @ 1byte
 //inp  (UART_MSR);            // clear modem status register

#ifdef MPX_DEBUG_SERIAL
 sprintf(sout,"LSR status:%2X ",inp(UART_LSR));
 pds_textdisplay_printf(sout);
#endif

#ifdef MPX_DEBUG_SERIAL
 sprintf(sout,"IIR status:%2X ",inp(UART_IIR));
 pds_textdisplay_printf(sout);
#endif

 // removed, because the serial port handling can work without it in some cases (at some hardware)
 //if(!(inp(UART_IIR)&0x40)) // is FIFO successfully enabled?
 // return 0;                 // no (maybe it's an old 8250)

 //if(inp(UART_LSR)&0x9e){ // check the line status register for errors
 // _enable();
 // return 0;
 //}

 serial_com_port_reset(portdatas);

 outp(UART_IER,0x01);  // Enable interrupts for data ready on the serial line (8250)

 _enable();

 return com_baseport;
}

static void serial_com_port_close(struct serial_port_data_s *portdatas)
{
 unsigned int com_baseport=portdatas->baseport;
 if(com_baseport){
  if(!portdatas->oldportirqflag){ // it was disabled previously
   outp(UART_IER,0x00);  // Disable interrupts for data ready on the serial line (8250)
   outp(UART_FCR,0x00);  // disable FIFO
  }
 }
}

static unsigned int serial_com_port_read(struct serial_port_data_s *portdatas,unsigned int *byte)
{
 unsigned int com_baseport=portdatas->baseport;
 int timeout=COM_READ_TIMEOUT;

 do{
  if(!timeout--){
   *byte=0;
   return 0;
  }
 }while(!(inp(UART_LSR)&1));
 *byte=(unsigned int)inp(UART_RBR);

 #ifdef MPX_DEBUG_SERIAL
 sprintf(sout,"read: %2X  %c ",*byte,*byte);
 pds_textdisplay_printf(sout);
 #endif
 return 1;
}

static unsigned int serial_lpt_port_init(struct serial_port_data_s *portdatas,unsigned int portselect,unsigned int use_irq)
{
 unsigned int lpt_baseport;

 if(portselect<=4) // is LPT selected?
  return 0;

 lpt_baseport=*((unsigned short *)&biosmem[0x400+(portselect-1)*2]);
 if(!lpt_baseport) // is port enabled?
  return 0;

 portdatas->baseport=lpt_baseport;
 portdatas->oldportirqflag=inp(lpt_baseport+2);

 if(use_irq)
  outp(lpt_baseport+2,portdatas->oldportirqflag | 0x10); //enable irq

 return lpt_baseport;
}

static void serial_lpt_port_close(struct serial_port_data_s *portdatas)
{
 if(portdatas->baseport)
  outp(portdatas->baseport+2,portdatas->oldportirqflag);
}

#endif // __DOS__

#endif // MPXPLAY_LINK_SERIAL_PORTHAND

//--------------------------------------------------------------------------
#ifdef MPXPLAY_LINK_SERIAL_COMCNTRL
// HandlerCFG = COMC,COMn,baud,datalen  (datalen can be 1-16, or 128: direct keycodes)
// normal serial port handling/reading (com port and irq init only)

static struct serial_handler_data_s *comcntrl_handlerdatas;

static void __interrupt __loadds serial_handler_comcntrl_irqroutine(void)
{
 struct serial_handler_data_s *handdatas;
 unsigned int com_baseport;

 outp(0x20,0x20);     // Tell the 8259A End Of Interrupt
#ifdef __WATCOMC__
 loades();
#endif

 handdatas=comcntrl_handlerdatas;
 com_baseport=handdatas->portdatas.baseport;

 outp(UART_IER,0x00); // disable interrupts for data ready on the serial line (8250)
                      // to avoid serial-communication freezing (calling next/new irq before the current one finishing)
 //if((inp(UART_IIR)&0x06)==0x02){ // received data available (doesn't work)
 //if(inp(UART_LSR)&0x01){ // maybe this works
#ifdef MPX_DEBUG_SER_IRQ
  inchar=inp(UART_RBR);
  handdatas->inbytesstore[handdatas->indatalen]=inchar;
  pds_ltoa16(inchar,&sout[0]);
  pds_textdisplay_printf(sout);
#else
  handdatas->inbytesstore[handdatas->indatalen]=(unsigned char)inp(UART_RBR);
#endif

  pds_memcpy(&handdatas->inbytesstore[0],&handdatas->inbytesstore[1],handdatas->indatalen);

  if((++handdatas->inbytecount)>=handdatas->indatalen){
   if(handdatas->flags&SERIAL_HANDDATAFLAG_DIRECTKEY){
    // big endian, little endian?
    unsigned int keycode=(((unsigned int)handdatas->inbytesstore[0])<<8)|((unsigned int)handdatas->inbytesstore[1]);
    pds_pushkey(keycode);
    //pds_pushkey(*((unsigned short *)&handdatas->inbytesstore[0]));
    handdatas->inbytecount=0;
   }else{
    unsigned int i;
    serial_func_data_s *funcp=handdatas->serial_funcs;
    for(i=0;i<handdatas->number_of_funcs;i++){ // compare the collected bytes with the serial_func[].databytes
     unsigned int b,found=1;
     for(b=0;b<handdatas->indatalen;b++){
      if(funcp->databytes[b]!=handdatas->inbytesstore[b]){
       found=0;
       break;
      }
     }
     if(found){                // the two string is equal
      pds_pushkey(funcp->keycode);  // execute the keycode
      handdatas->inbytecount=0;
     #ifdef MPX_DEBUG_SER_IRQ
      pds_textdisplay_printf("found keycode");
     #endif
      break;
     }
     funcp++;
    }
    #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
    lircStr("UIR\n",(handdatas->inbytesstore[0] << 24) | (((handdatas->indatalen > 1) ? handdatas->inbytesstore[1] : 0) << 16) | (((handdatas->indatalen > 2) ? handdatas->inbytesstore[2] : 0) << 8) | ((handdatas->indatalen > 3) ? handdatas->inbytesstore[3] : 0),0);
    #endif
   }
  }
  outp(UART_IER,0x01); // enable interrupts for data ready on the serial line (8250)
 //}
}

static unsigned int serial_handler_comcntrl_init(struct serial_handler_data_s *handdatas)
{
 if(!serial_com_port_init(&handdatas->portdatas,handdatas->portselect,1))
  return 0;
 serial_interrupt_init(&handdatas->irqdatas,handdatas->portselect,(void far *)serial_handler_comcntrl_irqroutine);
 comcntrl_handlerdatas=handdatas;
 return 1;
}

static void serial_handler_comcntrl_close(struct serial_handler_data_s *handdatas)
{
 serial_com_port_close(&handdatas->portdatas);
 serial_interrupt_close(&handdatas->irqdatas);
}
#endif // MPXPLAY_LINK_SERIAL_COMCNTRL

//--------------------------------------------------------------------------
#ifdef MPXPLAY_LINK_SERIAL_UIR
// HandlerCFG = UIR,COMn,9600,datalen
// infra receiver handling

#define SERIAL_UIR_BASE_DELAY  20

#ifdef MPXPLAY_WIN32

#define SERIAL_UIR_RETRY       3 // 2*3

static void serial_handler_uir_read(struct serial_handler_data_s *handdatas)
{
 unsigned int i=serial_com_port_read(&handdatas->portdatas,&handdatas->inbytesstore[0],handdatas->indatalen,1);
 if(i==handdatas->indatalen){
  serial_func_data_s *funcp=handdatas->serial_funcs;
  for(i=0;i<handdatas->number_of_funcs;i++){ // compare the collected bytes with the serial_func[].databytes
   unsigned int b,found=1;
   for(b=0;b<handdatas->indatalen;b++){
    if(funcp->databytes[b]!=handdatas->inbytesstore[b]){
     found=0;
     break;
    }
   }
   if(found){                // the two string is equal
    pds_pushkey(funcp->keycode);  // execute the keycode
    handdatas->inbytecount=0;
    #ifdef MPX_DEBUG_SER_IRQ
    pds_textdisplay_printf("found keycode");
    #endif
    break;
   }
   funcp++;
  }
 }else{
  if(i){ // something went wrong, reset com port
   if(handdatas->portdatas.error_retry<2)
    handdatas->portdatas.error_retry++;
   else{
    serial_com_port_reset(&handdatas->portdatas);
    handdatas->portdatas.error_retry=0;
   }
  }else
   handdatas->portdatas.error_retry=0;
 }
}

static unsigned int serial_handler_uir_init(struct serial_handler_data_s *handdatas)
{
 unsigned int retry,delaj,reloop=2;
 unsigned char rdata[2];

 do{
  if(!serial_com_port_init(&handdatas->portdatas,handdatas->portselect,1))
   return 0;

  delaj=SERIAL_UIR_BASE_DELAY;
  retry=SERIAL_UIR_RETRY;
  do{
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("testing UIR phase 1");
   #endif

   //outp(UART_MCR, 0x08);
   EscapeCommFunction(handdatas->portdatas.comm_port_handle,CLRRTS);
   EscapeCommFunction(handdatas->portdatas.comm_port_handle,CLRDTR);
   delay(delaj);
   //outp(UART_MCR, 0x0B);
   EscapeCommFunction(handdatas->portdatas.comm_port_handle,SETRTS);
   EscapeCommFunction(handdatas->portdatas.comm_port_handle,SETDTR);
   delay(delaj);

   if(serial_com_port_read(&handdatas->portdatas,&rdata[0],1,1))
    if(rdata[0]=='X')
     break;

   //outp(UART_MCR,0x08); // Assert DTR, RTS, and OUT2 on the COM port (a kind of reset)
   //EscapeCommFunction(handdatas->portdatas.comm_port_handle,7); // RESETDEV ???
   EscapeCommFunction(handdatas->portdatas.comm_port_handle,CLRRTS);
   EscapeCommFunction(handdatas->portdatas.comm_port_handle,CLRDTR);
   delay(delaj);
   serial_com_port_reset(&handdatas->portdatas);
   delaj<<=1;
  }while(--retry);

  if(retry){
   delaj=SERIAL_UIR_BASE_DELAY;
   retry=SERIAL_UIR_RETRY;
   do{
    #ifdef MPX_DEBUG_SERIAL
    pds_textdisplay_printf("testing UIR phase 2");
    #endif

    serial_com_port_write(&handdatas->portdatas,'I');
    delay(delaj);
    serial_com_port_write(&handdatas->portdatas,'R');
    delay(delaj);

    if(serial_com_port_read(&handdatas->portdatas,&rdata[0],2,1)==2)
     if(rdata[0]=='O' && rdata[1]=='K')
      break;

    serial_com_port_reset(&handdatas->portdatas);
    //EscapeCommFunction(handdatas->portdatas.comm_port_handle,CLRRTS);
    //EscapeCommFunction(handdatas->portdatas.comm_port_handle,CLRDTR);
    delaj<<=1;
   }while(--retry);
  }

  if(retry){
   serial_com_port_reset(&handdatas->portdatas);
   break;
  }else
   serial_com_port_close(&handdatas->portdatas);
 }while(--reloop);

 if(!reloop)
  return 0;

 //if(!serial_interrupt_init(handdatas,serial_handler_uir_irqroutine))
 // return 0;

 return 1;
}

static void serial_handler_uir_close(struct serial_handler_data_s *handdatas)
{
 serial_com_port_close(&handdatas->portdatas);
 //serial_interrupt_close(handdatas);
}

#else // __DOS__

#define SERIAL_UIR_RETRY       6

static struct serial_handler_data_s *uir_handlerdatas;

static void __interrupt __loadds serial_handler_uir_irqroutine(void)
{
 struct serial_handler_data_s *handdatas;
 unsigned int com_baseport;

 outp(0x20,0x20);     // Tell the 8259A End Of Interrupt
#ifdef __WATCOMC__
 loades();
#endif

 handdatas=uir_handlerdatas;
 com_baseport=handdatas->portdatas.baseport;//uir_base_port;

 outp(UART_IER,0x00); // disable interrupts for data ready on the serial line (8250)

#ifdef MPX_DEBUG_SER_IRQ
 inchar=inp(UART_RBR);
 handdatas->inbytesstore[handdatas->indatalen]=inchar;
 pds_ltoa16(inchar,&sout[0]);
 pds_textdisplay_printf(sout);
#else
 handdatas->inbytesstore[handdatas->indatalen]=(unsigned char)inp(UART_RBR);
#endif

 pds_memcpy(&handdatas->inbytesstore[0],&handdatas->inbytesstore[1],handdatas->indatalen);

 if((++handdatas->inbytecount)>=handdatas->indatalen){
  serial_func_data_s *funcp=handdatas->serial_funcs;
  unsigned int i;

  for(i=0;i<handdatas->number_of_funcs;i++){ // compare the collected bytes with the serial_func[].databytes
   unsigned int b,found=1;
   for(b=0;b<handdatas->indatalen;b++){
    if(funcp->databytes[b]!=handdatas->inbytesstore[b]){
     found=0;
     break;
    }
   }
   if(found){                // the two string is equal
    pds_pushkey(funcp->keycode);  // execute the keycode
    handdatas->inbytecount=0;
    #ifdef MPX_DEBUG_SER_IRQ
    pds_textdisplay_printf("found keycode");
    #endif
    break;
   }
   funcp++;
  }
 }

 outp(UART_IER,0x01); // enable interrupts for data ready on the serial line (8250)
}

static unsigned int serial_handler_uir_init(struct serial_handler_data_s *handdatas)
{
 unsigned int retry,delaj,temp,com_baseport;

 com_baseport=serial_com_port_init(&handdatas->portdatas,handdatas->portselect,1);
 if(!com_baseport)
  return 0;

 delaj=SERIAL_UIR_BASE_DELAY;
 retry=SERIAL_UIR_RETRY;
 do{
  #ifdef MPX_DEBUG_SERIAL
  pds_textdisplay_printf("testing UIR phase 1");
  #endif

  outp(UART_MCR, 0x08);
  delay(delaj);
  outp(UART_MCR, 0x0B);
  delay(delaj);

  serial_com_port_read(&handdatas->portdatas,&temp);

  if(temp==(unsigned int)'X')
   break;

  if(!(--retry))
   return 0;

  outp(UART_MCR,0x08); // Assert DTR, RTS, and OUT2 on the COM port (a kind of reset)
  delay(delaj);        // ???
  serial_com_port_reset(&handdatas->portdatas);
  delaj<<=1;
 }while(1);

 delaj=SERIAL_UIR_BASE_DELAY;
 retry=SERIAL_UIR_RETRY;
 do{
  #ifdef MPX_DEBUG_SERIAL
  pds_textdisplay_printf("testing UIR phase 2");
  #endif

  outp(UART_THR, (int)'I');
  delay(delaj);
  outp(UART_THR, (int)'R');
  delay(delaj);

  serial_com_port_read(&handdatas->portdatas,&temp);
  if(temp==(unsigned int)('O')){
   if(delaj>100)
    delay(delaj-100);
   serial_com_port_read(&handdatas->portdatas,&temp);
   if(temp==(unsigned int)('K'))
    break;
  }

  if(!(--retry))
   return 0;

  serial_com_port_reset(&handdatas->portdatas);
  delaj<<=1;
 }while(1);

 serial_com_port_reset(&handdatas->portdatas);
 serial_interrupt_init(&handdatas->irqdatas,handdatas->portselect,(void far *)serial_handler_uir_irqroutine);

 uir_handlerdatas=handdatas;

 return 1;
}

static void serial_handler_uir_close(struct serial_handler_data_s *handdatas)
{
 serial_com_port_close(&handdatas->portdatas);
 serial_interrupt_close(&handdatas->irqdatas);
}
#endif // __DOS__
#endif // MPXPLAY_LINK_SERIAL_UIR

//-------------------------------------------------------------------------
#ifdef MPXPLAY_LINK_SERIAL_LPTCNTRL
// LPT control (LPTC,LPTR)
/*
*
* Up to 15 LPT buttons (4 bits/pins) can be used on the LPT port.
*
*
* Use the following wiring scheme:
*
*                    button 1   gnd    button 2
*                       /        _         /
*   ...________________/ ________|________/ ________________...
*       |   |      |                           |      |   |
*       -   -      -                           -      -   -  diodes (1N4148)
*       A   A      A                           A      A   A
*       |   |      |         node 1            |      |   |
*       .   .   ..._____________________________...   .   .
*       .   .               |       |                 .   .
* to input pins            ---      -                 to input pins
* (binary coded)           ---     | |                (binary coded)
*                   c=100nF |       -  r=47kOhms
*                           |       |
*                           _________
*                               |
*                               .
*                               .
*                           to pin 10 of the LPT port
*
*
* Notes:
* - input pins are: pins 11,12,13,15 of the LPT port
* - the outer diodes are used for encoding and can be omitted,
*   if not more than 4 buttons are connected
* - in addition to the connection to the input pins, each button has to be
*   connected to node 1 via a diode
*
* Config in mpxplay.ini :
*
*  HandlerCFG = LPTC,LPTn,0,1  ('n' can be 1,2 or 3) (with irq handler)
*  or
*  HandlerCFG = LPTR,LPTn,0,1  (with direct port reading, without irq handler)
*
*  SerialFunc = lpt_status_in, keycode
*
*  where lpt_status_in is 0x00   ... 0x0f   (4 bits -> 0-15 possible values)
*        keycode       is 0x0001 ... 0xfffe (configured in [keyboard] section)
*
*  ie: SerialFunc=0x07,1c0d ; 'enter'
*
*/

static struct serial_handler_data_s *lptcntrl_handlerdatas;

static void serial_handler_lptcntrl_read(struct serial_handler_data_s *handdatas)
{
 unsigned int lpt_baseport,lpt_butt;

 lpt_baseport=handdatas->portdatas.baseport;

 lpt_butt=(inp(lpt_baseport+1) >> 3)^0x0f;
 lpt_butt=((lpt_butt & 0x10) >> 1) | (lpt_butt & 0x07);

 if(lpt_butt){
  serial_func_data_s *funcp=handdatas->serial_funcs;
  unsigned int i;
  #ifdef MPX_DEBUG_SERIAL
  sprintf(sout,"LPT butt: %2.2X ",lpt_butt);
  pds_textdisplay_printf(sout);
  #endif
  for(i=0;i<handdatas->number_of_funcs;i++){
   if(funcp->databytes[0]==lpt_butt){
    pds_pushkey(funcp->keycode);
    #ifdef MPX_DEBUG_SERIAL
    sprintf(sout,"found LPT keycode: %4.4X",(int)funcp->keycode);
    pds_textdisplay_printf(sout);
    #endif
    break;
   }
   funcp++;
  }
  #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
  lircStr("LPT\n",lpt_butt,0);
  #endif
 }

}

static void __interrupt __loadds serial_handler_lptcntrl_irqroutine(void)
{
 outp(0x20,0x20);     // Tell the 8259A End Of Interrupt
#ifdef __WATCOMC__
 loades();
#endif
 serial_handler_lptcntrl_read(lptcntrl_handlerdatas);
}

static unsigned int serial_handler_lptcntrl_initi(struct serial_handler_data_s *handdatas)
{
 if(!serial_lpt_port_init(&handdatas->portdatas,handdatas->portselect,1))
  return 0;
 serial_interrupt_init(&handdatas->irqdatas,handdatas->portselect,(void far *)serial_handler_lptcntrl_irqroutine);
 lptcntrl_handlerdatas=handdatas;
 return 1;
}

static unsigned int serial_handler_lptcntrl_initr(struct serial_handler_data_s *handdatas)
{
 if(!serial_lpt_port_init(&handdatas->portdatas,handdatas->portselect,0))
  return 0;
 lptcntrl_handlerdatas=handdatas;
 return 1;
}

static void serial_handler_lptcntrl_close(struct serial_handler_data_s *handdatas)
{
 serial_lpt_port_close(&handdatas->portdatas);
 serial_interrupt_close(&handdatas->irqdatas);
}
#endif // MPXPLAY_LINK_SERIAL_LPTCNTRL

#ifdef MPXPLAY_LINK_SERIAL_NECIR
/*****************************************************************************
* Realtime infrared decoder for the NECIR protocol (by S. Zeller)            *
* and LPT button handling                                                    *
* for more info about NECIR see 'ISRemote for DOS' at www.bnro.de/~zeller    *
*                                                                            *
* The NECIR part:                                                            *
*                                                                            *
* Advantages over ISRemote for DOS:                                          *
* - does work with all CPUs                                                  *
* - does not depend on EMM386.EXE                                            *
* - should work in DOS Box                                                   *
*                                                                            *
* Notation in mpxplay.ini:                                                   *
*                                                                            *
*   HandlerCFG =NECIR,port,0,6                                               *
*                                                                            *
*    port can be: COM1..COM4, LPT1..LPT3                                     *
*                                                                            *
*   SerialFunc =NECIRCODErepetitions,KEYCODE                                 *
*   example: SerialFunc =1234abcd0000,1c0d                                   *
*                                                                            *
* - use ISRemote for Dos to get NECIRCODE (without blanks)                   *
* - 'repetitions' can be: 0000 for endless repetition,                       *
*                       anything else for no repetition                      *
*                                                                            *
* - make sure that none of the LPT input pins (11,12,13,15) is connected     *
*   permanently to ground, because then the decoder will assume that a LPT   *
*   button is pressed (see below) instead of decoding a remote signal        *
* - under Windows, it may be necessary to disable the COM port, at which the *
*   NECIR receiver is connected, in the device manager                       *
*                                                                            *
*                                                                            *
* The LPT button handling part (see HandlerCFG=LPTC for wiring scheme):      *
*                                                                            *
* Up to 15 LPT buttons can be used concurently to the NECIR receiver on the  *
* same LPT port.                                                             *
*                                                                            *
* Notation in mpxplay.ini:                                                   *
*                                                                            *
*   SerialFunc =00000A0B0C0D,KEYCODE                                         *
*   example: SerialFunc =000001010001,1c0d                                   *
*                                                                            *
* - 'A' corresponds to the state of pin 11 (1 or 0)                          *
* - 'B' corresponds to the state of pin 12 (1 or 0)                          *
* - 'C' corresponds to the state of pin 13 (1 or 0)                          *
* - 'D' corresponds to the state of pin 15 (1 or 0)                          *
*                                                                            *
*****************************************************************************/

//Note: INT08_DIVISOR_NEW should not be smaller than appox. 10000

//Note: Doesn't work in test mode (mpxplay.exe -t)


#define NECIR_SCALE_CLOCK             12  //=1193182MHz/100000 // Timer tick rate /scaling factor

#define irtol 40          //allow 40% tolerance
#define irupscale NECIR_SCALE_CLOCK*(100+irtol)          //allow tolerance      and scale
#define irdownscale NECIR_SCALE_CLOCK*(100-irtol)          //allow tolerance    and scale
#define irdecscale NECIR_SCALE_CLOCK*150
#define bitperiod 1125          // bit period in us
#define firstperiod 5062 // first pulse period in us, use (12*bitperiod) at rising edge trigger and (4.5*bitperiod) at falling edge trigger
#define cmdperiod 107188UL          // command period in us, use (110000-10*bitperiod) at rising edge trigger and (110000-2.5*bitperiod) at falling edge trigger
#define nirrep 5 // number of repetitions to ignore

typedef struct necir_data_s{
 unsigned int  use_lpt,lpt_butt,pulse,repcount;
 unsigned long tscdif,tsctemp,oldtsc;
 unsigned long ircode,oldircode;
 unsigned short keycode;
}necir_data_s;

extern unsigned long int08counter;
static struct serial_handler_data_s *necir_handlerdatas;

#ifdef MPX_DEBUG_SERIAL
 static char necirsout[50];
#endif

static void __interrupt __loadds serial_handler_necir_irqroutine(void)
{
 struct serial_handler_data_s *handdatas;
 struct necir_data_s *necdatas;
 unsigned int necir_baseport,i_temp;

 outp(0x20,0x20);     // Tell the 8259A End Of Interrupt
#ifdef __WATCOMC__
 loades();
#endif

 handdatas=necir_handlerdatas;
 necir_baseport=handdatas->portdatas.baseport;
 necdatas=handdatas->private_datas;

 outp(0x43,0x04);
 i_temp=3;
 while (i_temp--);
 necdatas->tsctemp=inp(0x40);
 i_temp=3;
 while (i_temp--);
 necdatas->tsctemp+=inp(0x40)<<8;
 necdatas->tsctemp=INT08_DIVISOR_NEW-necdatas->tsctemp+int08counter*INT08_DIVISOR_NEW;

 if(necdatas->use_lpt){

  necdatas->lpt_butt=(inp(necir_baseport+1) >> 3)^0x0f;
  necdatas->lpt_butt=((necdatas->lpt_butt & 0x10) >> 1) | (necdatas->lpt_butt & 0x07);

  if(necdatas->lpt_butt){
   serial_func_data_s *funcp=handdatas->serial_funcs;
   unsigned int i;
   for(i=0;i<handdatas->number_of_funcs;i++){
    unsigned int found=1;
    if(funcp->databytes[0]) found=0;
    if(funcp->databytes[1]) found=0;
    if(funcp->databytes[2]!=(necdatas->lpt_butt >> 3)) found=0;
    if(funcp->databytes[3]!=((necdatas->lpt_butt & 0x07) >> 2)) found=0;
    if(funcp->databytes[4]!=((necdatas->lpt_butt & 0x03) >> 1)) found=0;
    if(funcp->databytes[5]!=(necdatas->lpt_butt & 0x01)) found=0;
    if(found){
     pds_pushkey(funcp->keycode);
     #ifdef MPX_DEBUG_SERIAL
     pds_textdisplay_printf("found LPT keycode");
     #endif
     break;
    }
    funcp++;
   }
   #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
   lircStr("LPT\n",necdatas->lpt_butt,0);
   #endif
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("LPT bits:");
   if (necdatas->lpt_butt & 0x01) pds_textdisplay_printf("0");
   if (necdatas->lpt_butt & 0x02) pds_textdisplay_printf("1");
   if (necdatas->lpt_butt & 0x04) pds_textdisplay_printf("2");
   if (necdatas->lpt_butt & 0x08) pds_textdisplay_printf("3");
   #endif
  }
 }else
  necdatas->lpt_butt=0;

 if((necdatas->use_lpt || (inp(necir_baseport+6) & 0x80)) && (!necdatas->lpt_butt)){
  if(necdatas->tsctemp<necdatas->oldtsc)
   necdatas->tsctemp+=INT08_DIVISOR_NEW;  // handle overflow
  necdatas->tscdif=necdatas->tsctemp-necdatas->oldtsc;
  necdatas->oldtsc=necdatas->tsctemp;

  if(necdatas->tscdif<=(cmdperiod*irupscale)/1000){
   if(necdatas->pulse == 0xff){
    if((necdatas->tscdif<=(irdownscale*firstperiod)/1000) || (necdatas->tscdif>(irupscale*firstperiod)/1000)){
     necdatas->pulse=0xff;
    }else
     necdatas->pulse=0;
   }else
    if(necdatas->pulse < 0x20){
     if(necdatas->tscdif<=(bitperiod*irdownscale)/1000){
      necdatas->pulse=0xff;
     }else
      if(necdatas->tscdif>(2*bitperiod*irupscale)/1000){
       if((necdatas->tscdif>(cmdperiod*irdownscale)/1000) && (necdatas->pulse==0) && (necdatas->repcount)){
        if(necdatas->repcount>nirrep){
	 if(necdatas->keycode)
          pds_pushkey(necdatas->keycode);
#ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
         lircStr("NECIR\n",necdatas->ircode,necdatas->repcount);
#endif
        }
	necdatas->repcount++;
       }
       necdatas->pulse=0xff;
      }else{
       necdatas->ircode >>= 1;
       if(necdatas->tscdif>(bitperiod*irdecscale)/1000)
        necdatas->ircode=necdatas->ircode | 0x80000000;
       else
        necdatas->ircode=necdatas->ircode & 0x7fffffff;
       necdatas->pulse++;
       if (necdatas->pulse==0x20){
        serial_func_data_s *funcp=handdatas->serial_funcs;
    	unsigned int i;
#ifdef MPX_DEBUG_SERIAL
       sprintf(necirsout,"received NECIR keycode: %lX",necdatas->ircode);
       pds_textdisplay_printf(necirsout);
#endif
       necdatas->keycode=0;
       if (necdatas->oldircode!=necdatas->ircode)
        necdatas->repcount=0;
       necdatas->oldircode=necdatas->ircode;
       for(i=0;i<handdatas->number_of_funcs;i++){
       	unsigned int found=1;
       	if(funcp->databytes[0]!=((necdatas->ircode & 0x0000ff00) >>  8)) found=0;
       	if(funcp->databytes[1]!=((necdatas->ircode & 0x000000ff)      )) found=0;
       	if(funcp->databytes[2]!=((necdatas->ircode & 0xff000000) >> 24)) found=0;
       	if(funcp->databytes[3]!=((necdatas->ircode & 0x00ff0000) >> 16)) found=0;
       	if(found){
     	 if(!(((funcp->databytes[4]) || (funcp->databytes[5])) && (necdatas->repcount)))
          necdatas->keycode=funcp->keycode;
#ifdef MPX_DEBUG_SERIAL
         pds_textdisplay_printf("found NECIR keycode");
#endif
         break;
        }
     	funcp++;
       }
       if((!necdatas->repcount) || (necdatas->repcount>nirrep)){
        if(necdatas->keycode)
         pds_pushkey(necdatas->keycode);
#ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
	lircStr("NECIR\n",necdatas->ircode,necdatas->repcount);
#endif
      }
      necdatas->repcount++;
      necdatas->pulse=0xff;
     }
    }
   }else
    necdatas->pulse=0xff;
  }else{
   necdatas->pulse=0xff;
   necdatas->repcount=0;
   necdatas->oldircode=0;
  }
 }
}

static unsigned int serial_handler_necir_init(struct serial_handler_data_s *handdatas)
{
 struct necir_data_s *necdatas;
 unsigned int necir_baseport;

 if(handdatas->indatalen!=6)
  return 0;

 necir_baseport=*((unsigned short *)&biosmem[0x400+(handdatas->portselect-1)*2]);
 if(!necir_baseport) // is port enabled?
  return 0;
 handdatas->portdatas.baseport=necir_baseport;

 necdatas=handdatas->private_datas=calloc(1,sizeof(struct necir_data_s));
 if(!necdatas)
  return 0;

 if(handdatas->portselect<=4){ // com port
  inp(necir_baseport+6); //required!
  outp(necir_baseport+3,inp(necir_baseport+3) & 0x7F);
  outp(necir_baseport+1,0x08);
  outp(necir_baseport+4,0x0A);
 }else{                    // lpt port
  outp(necir_baseport+2,inp(necir_baseport+2) | 0x10);
  necdatas->use_lpt=1;
 }

 necdatas->pulse=0xff;
 necdatas->repcount=0;

 serial_interrupt_init(&handdatas->irqdatas,handdatas->portselect,(void far *)serial_handler_necir_irqroutine);
 necir_handlerdatas=handdatas;
 return 1;
}

static void serial_handler_necir_close(struct serial_handler_data_s *handdatas)
{
 serial_interrupt_close(&handdatas->irqdatas);
 if(handdatas->private_datas){
  free(handdatas->private_datas);
  handdatas->private_datas=NULL;
 }
}
#endif // MPXPLAY_LINK_SERIAL_NECIR

#ifdef MPXPLAY_LINK_SERIAL_VT100
/*****************************************************************************
*  VT100 terminal input driver (by S.Zeller)
*
*  Mpxplay.ini config:
*
* HandlerCFG=VT100,COMn,baud,5
*
*  'n' is 1,2,3 or 4
*  baud is configurable (1200-115200)
*
* 'SerialFunc' in MPXPLAY.INI must assign a keycode to the received ASCII
*  character or ESC control sequence.
*
* example:
* SerialFunc =30,0b30 ; '0'
* SerialFunc =31,0231 ; '1'
* SerialFunc =32,0332 ; '2'
* SerialFunc =33,0433 ; '3'
* SerialFunc =34,0534 ; '4'
* SerialFunc =35,0635 ; '5'
* SerialFunc =36,0736 ; '6'
* SerialFunc =37,0837 ; '7'
* SerialFunc =38,0938 ; '8'
* SerialFunc =39,0a39 ; '9'
* SerialFunc =0d,1c0d ; 'enter'
* SerialFunc =1b5b41,4800 ; 'editor up'
* SerialFunc =1b5b42,5000 ; 'editor down'
* SerialFunc =1b5b43,4d00 ; 'fforward'
* SerialFunc =1b5b44,4b00 ; 'rewind'
*****************************************************************************/

#define VTSEQLEN 16

static struct serial_handler_data_s *vt100_handlerdatas;

static void VTInit(void);
static void VTRead(struct serial_handler_data_s *,unsigned char);
static void VTatnrm(unsigned char);
static void VTatescf(unsigned char);
static void VTatthird(unsigned char);
static void VTansiparse(unsigned char);
static void VTextparse(unsigned char);
static void (*VTstate)(unsigned char) = VTatnrm;
static unsigned char VTlastc = '\0';

static void __interrupt __loadds serial_handler_vt100_irqroutine(void)
{
 outp(0x20,0x20);     // Tell the 8259A End Of Interrupt
#ifdef __WATCOMC__
 loades();
#endif

 VTRead(vt100_handlerdatas,inp(vt100_handlerdatas->portdatas.baseport));
}

static unsigned int serial_handler_vt100_init(struct serial_handler_data_s *handdatas)
{
 if(!serial_com_port_init(&handdatas->portdatas,handdatas->portselect,1))
  return 0;

 VTInit();

 serial_interrupt_init(&handdatas->irqdatas,handdatas->portselect,(void far *)serial_handler_vt100_irqroutine);
 vt100_handlerdatas=handdatas;
 return 1;
}

static void serial_handler_vt100_close(struct serial_handler_data_s *handdatas)
{
 serial_com_port_close(&handdatas->portdatas);
 serial_interrupt_close(&handdatas->irqdatas);
}

static void VTInit(void)
{
 VTstate = VTatnrm;
 VTlastc = '\0';
}

static void VTRead(struct serial_handler_data_s *handdatas,unsigned char c)
{

 (*VTstate)(c);

 VTlastc = c;

 if (handdatas->inbytecount<SERIAL_MAX_INDATALEN)
  handdatas->inbytesstore[handdatas->inbytecount++]=c;

 #ifdef MPX_DEBUG_SERIAL
  pds_textdisplay_printf("received char");
 #endif

 if(VTstate==VTatnrm){
  unsigned int i;
  serial_func_data_s *funcp=handdatas->serial_funcs;
  if(handdatas->indatalen<=handdatas->inbytecount){
   handdatas->inbytecount=handdatas->indatalen;
  }else{
   for(i=handdatas->inbytecount;i<((handdatas->indatalen>SERIAL_MAX_INDATALEN)? SERIAL_MAX_INDATALEN:handdatas->indatalen);i++){
    handdatas->inbytesstore[i]=0;
   }
  }
  #ifdef MPX_DEBUG_SERIAL
  handdatas->inbytesstore[SERIAL_MAX_INDATALEN]=0;
  pds_textdisplay_printf(handdatas->inbytesstore);
  #endif

  for(i=0;i<handdatas->number_of_funcs;i++){ // compare the collected bytes with the serial_func[].databytes
   unsigned int b,found;
   found=1;
   for(b=0;b<((handdatas->indatalen>SERIAL_MAX_INDATALEN)? SERIAL_MAX_INDATALEN:handdatas->indatalen);b++){
    if(funcp->databytes[b]!=handdatas->inbytesstore[b]){
     found=0;
     break;
    }
   }
   if(found){                // the two strings are equal
    pds_pushkey(funcp->keycode);  // execute the keycode
    #ifdef MPX_DEBUG_SER_IRQ
    pds_textdisplay_printf("found VT keycode");
    #endif
    break;
   }
   funcp++;
  }
  #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
  lircStr("VT100\n",(handdatas->inbytesstore[0] << 24) | (handdatas->inbytesstore[1] << 16) | (handdatas->inbytesstore[2] << 8) | handdatas->inbytesstore[3],0);
  #endif
  handdatas->inbytecount=0;
 }
}

static void VTatnrm(unsigned char c)
{
 if(c==27)
  VTstate = VTatescf;        /* Next state parser is esc follower */
}

static void VTatescf(unsigned char c)
{
 switch (c) {
  case  '[':                /* Parse ansi args */
            VTstate = VTansiparse;
            break;

  case  '(':
  case  ')':
  case  '#':VTstate = VTatthird;break;

  default:VTstate = VTatnrm;break;
 }
}

static void VTatthird(unsigned char c)
{
 VTstate = VTatnrm;
}

static void VTansiparse(unsigned char c)
{
 if(((c>='0') && (c<='9')) || (c==';'))
  return;
 if(c=='?'){           /* Extended parse mode */
  if(VTlastc == '[')
   VTstate = VTextparse;
  else
   VTstate = VTatnrm;
  return;
 }
 VTstate = VTatnrm;
}

static void VTextparse(unsigned char c)
{
 if((c<'0') || (c>'9'))
  VTstate = VTatnrm;
}

#endif // MPXPLAY_LINK_SERIAL_VT100

/*
The  following  are  the VT100 commands as  described  by  the
Digital VT101 Video Terminal User Guide  (EK-VT101-UG-003).   An
asterik (*)  beside the function indicate that it is  currently
supported.  A plus (+) means the function is trapped and ignored.

Scrolling Functions:

 *  ESC [ pt ; pb r   set scroll region
 *  ESC [ ? 6 h       turn on region - origin mode
 *  ESC [ ? 6 l       turn off region - full screen mode

Cursor Functions:

 *  ESC [ pn A        cursor up pn times - stop at top
 *  ESC [ pn B        cursor down pn times - stop at bottom
 *  ESC [ pn C        cursor right pn times - stop at far right
 *  ESC [ pn D        cursor left pn times - stop at far left
 *  ESC [ pl ; pc H   set cursor position - pl Line, pc Column
 *  ESC [ H           set cursor home
 *  ESC [ pl ; pc f   set cursor position - pl Line, pc Column
 *  ESC [ f           set cursor home
 *  ESC D             cursor down - at bottom of region, scroll up
 *  ESC M             cursor up - at top of region, scroll down
 *  ESC E             next line (same as CR LF)
 *  ESC 7             save cursor position(char attr,char set,org)
 *  ESC 8             restore position (char attr,char set,origin)

Applications / Normal Mode:

 *  ESC [ ? 1 h       cursor keys in applications mode
 *  ESC [ ? 1 l       cursor keys in cursor positioning mode
 *  ESC =             keypad keys in applications mode
 *  ESC >             keypad keys in numeric mode

Character Sets:

 *  ESC ( A           UK char set as G0
 *  ESC ( B           US char set as G0
 *  ESC ( 0           line char set as G0
 *  ESC ) A           UK char set as G1
 *  ESC ) B           US char set as G1
 *  ESC ) 0           line char set as G1
 *  ESC N             select G2 set for next character only
 *  ESC O             select G3 set for next character only

Character Attributes:

 *  ESC [ m           turn off attributes - normal video
 *  ESC [ 0 m         turn off attributes - normal video
!*  ESC [ 4 m         turn on underline mode
 *  ESC [ 7 m         turn on inverse video mode
 *  ESC [ 1 m         highlight
 *  ESC [ 5 m         blink

!  On color systems underlined characters are displayed in blue

Line Attributes:

 +  ESC # 3           double high (top half) - double wide
 +  ESC # 4           double high (bottom half) - double wide
 +  ESC # 5           single wide - single height
 +  ESC # 6           double wide - single height

Erasing:

 *  ESC [ K           erase to end of line (inclusive)
 *  ESC [ 0 K         erase to end of line (inclusive)
 *  ESC [ 1 K         erase to beginning of line (inclusive)
 *  ESC [ 2 K         erase entire line (cursor doesn't move)
 *  ESC [ J           erase to end of screen (inclusive)
 *  ESC [ 0 J         erase to end of screen (inclusive)
 *  ESC [ 1 J         erase to beginning of screen (inclusive)
 *  ESC [ 2 J         erase entire screen (cursor doesn't move)

Tabulation:

 *  ESC H             set tab in current position
 *  ESC [ g           clear tab stop in current position
 *  ESC [ 0 g         clear tab stop in current position
 *  ESC [ 3 g         clear all tab stops

Printing:

 *  ESC [ i           print page
 *  ESC [ 0 i         print page
 *  ESC [ 1 i         print line
 *  ESC [ ? 4 i       auto print off
 *  ESC [ ? 5 i       auto print on
 +  ESC [ 4 i         print controller off
 +  ESC [ 5 i         print controller on

Requests / Reports:

 *  ESC [ 5 n         request for terminal status
    ESC [ 0 n         report - no malfunction
 *  ESC [ 6 n         request for cursor position report
    ESC [ pl;pc R     report - cursor at line pl, & column pc
 *  ESC [ ? 1 5 n     request printer status
    ESC [ ? 1 0 n     report - printer ready
 *  ESC [ c           request to identify terminal type
 *  ESC [ 0 c         request to identify terminal type
 *  ESC Z             request to identify terminal type
    ESC [ ? 1;0 c     report - type VT100

Initialization / Tests:

 +  ESC c             reset to initial state
 +  ESC [ 2 ; 1 y     power up test
 +  ESC [ 2 ; 2 y     loop back test
 +  ESC [ 2 ; 9 y     power up test till failure or power down
 +  ESC [ 2 ; 10 y    loop back test till failure or power down
 +  ESC # 8           video alignment test-fill screen with E's


Setup Functions:

 +  ESC [ ? 2 l       enter VT52 mode
 +  ESC <             exit VT52 mode
 +  ESC [ ? 3 h       132 column mode
 +  ESC [ ? 3 l       80 column mode
 +  ESC [ ? 4 h       smooth scroll
 +  ESC [ ? 4 l       jump scroll
 *  ESC [ ? 5 h       black characters on white screen mode
 *  ESC [ ? 5 l       white characters on black screen mode
 *  ESC [ ? 7 h       auto wrap to new line
 *  ESC [ ? 7 l       auto wrap off
 +  ESC [ ? 8 h       keyboard auto repeat mode on
 +  ESC [ ? 8 l       keyboard auto repeat mode off
 +  ESC [ ? 9 h       480 scan line mode
 +  ESC [ ? 9 l       240 scan line mode
 *  ESC [ ? 1 8 h     print form feed on
 *  ESC [ ? 1 8 l     print form feed off
 *  ESC [ ? 1 9 h     print whole screen
 *  ESC [ ? 1 9 l     print only scroll region
 +  ESC [ 2 0 h       newline mode LF, FF, VT, CR = CR/LF)
 +  ESC [ 2 0 l       line feed mode (LF, FF, VT = LF ; CR = CR)

LED Functions:

!*  ESC [ 0 q         turn off LED 1-4
!*  ESC [ 1 q         turn on LED #1
!*  ESC [ 2 q         turn on LED #2
!*  ESC [ 3 q         turn on LED #3
!*  ESC [ 4 q         turn on LED #4

!   The bottom line of the screen is used as a status line by the
    VT100 emulation.   The information on the bottom line is:

1)  the status of the four VT100 LED's
2)  the  status  of the numeric  keypad  (application mode /normal mode)
3)  the  status  of  the cursor  keypad  (application mode/normal  mode)


Interpreted Control Characters:

 *  ^O                shift in  - selects G0 character set
 *  ^N                shift out - selects G1 character set


VT100 KEYBOARD MAP

   The following table describes the special function keys of the
VT100 and shows the transmitted sequences.  It also shows the key
or  key sequence required to produce this function on the  IBM-PC
keyboard.  The  VT100 has four function keys PF1 - PF4,  four arrow
keys, and a numeric keypad with 0-9,  ".",  "-",  RETURN and  ",".
The numeric  keypad  and  the arrow keys may be in standard  mode or
applications mode as set by the host computer.  Sequences will be
sent as follows:

 To Get                                  Press Key on
VT100 Key    Standard    Applications     IBM Keypad
=====================================================

                                          NUMLOK - On
Keypad:

   0            0           ESC O p           0
   1            1           ESC O q           1
   2            2           ESC O r           2
   3            3           ESC O s           3
   4            4           ESC O t           4
   5            5           ESC O u           5
   6            6           ESC O v           6
   7            7           ESC O w           7
   8            8           ESC O x           8
   9            9           ESC O y           9
   -            -           ESC O m           -
   ,            ,           ESC O l      * (on PrtSc key)
   .            .           ESC O n           .
Return       Return         ESC O M           +


                                         NUMLOK - Off
Arrows:

   Up        ESC [ A        ESC O A           Up
  Down       ESC [ B        ESC O B          Down
  Right      ESC [ C        ESC O C          Right
  Left       ESC [ D        ESC O D          Left

   Up        ESC [ A        ESC O A          Alt 9
  Down       ESC [ B        ESC O B          Alt 0
  Right      ESC [ C        ESC O C          Alt -
  Left       ESC [ D        ESC O D          Alt =

  Note that either set of keys may be used to send VT100 arrow keys.
  The Alt 9,0,-, and = do not require NumLok to be off.

Functions:

PF1 - Gold   ESC O P        ESC O P           F1
PF2 - Help   ESC O Q        ESC O Q           F2
PF3 - Next   ESC O R        ESC O R           F3
PF4 - DelBrk ESC O S        ESC O S           F4


   Please note that the backspace key transmits an ascii DEL
(character 127) while in VT100 emulation.  To get a true ascii
backspace (character 8) you must press control-backspace.
*/

#ifdef MPXPLAY_LINK_SERIAL_SERMOUSE
/*****************************************************************************
*  Low level serial port mouse driver (by S.Zeller)                          *
*                                                                            *
*  Note:                                                                     *
*  If your mouse has a configuration switch,                                 *
*  make sure it is set to MS mode                                            *
*                                                                            *
*  Notation in mpxplay.ini:                                                  *
*                                                                            *
*   HandlerCFG=SERMOUS,COMn,1200,3   ('n' can be 1-4)                        *
*                                                                            *
*   SerialFunc =000EVENTsens,KEYCODE                                         *
*   example: SerialFunc =000100,1c0d;left button press means enter           *
*                                                                            *
* - 'EVENT' can be:  1 for left button                                       *
*                    2 for right button                                      *
*                    3 for move up                                           *
*                    4 for move down                                         *
*                    5 for move left                                         *
*                    6 for move right                                        *                                 *
* - 'sens' can be 00...3f and defines how fast you have to move the mouse to *
*    initiate an event. More than one sensitivities can be specified for one *
*    event, e.g. you can define a SerialFunc with low 'sens' value for normal*
*    seek and a second SerialFunc with high 'sens' value for 4x seek, but be *
*    sure to specify the SerialFunc with the lower 'sens' value first.       *
*                                                                            *
*  Why is such a driver useful?                                              *
*  With this driver you can build your own Jog Dial, if you make the x and y *
*  sensor rolls of a serial mouse accessible by a rotary knob.               *
*****************************************************************************/

typedef struct sermouse_data_s{
 unsigned int incount, signal, in1;
}sermouse_data_s;

static struct serial_handler_data_s *sermouse_handlerdatas;

static void __interrupt __loadds serial_handler_sermouse_irqroutine(void)
{
 struct serial_handler_data_s *handdatas;
 struct sermouse_data_s *smd;
 unsigned int inbyte;

 outp(0x20,0x20);     // Tell the 8259A End Of Interrupt
#ifdef __WATCOMC__
 loades();
#endif

 handdatas=sermouse_handlerdatas;
 smd=handdatas->private_datas;

 inbyte=inp(handdatas->portdatas.baseport);
 if(inbyte>63){
  smd->signal=inbyte;
  smd->incount=0;
 }
 if(inbyte==0)
  smd->signal=smd->signal & (~(smd->incount << 6));
 else
  smd->signal=smd->signal | (smd->incount << 6);
 if(smd->incount==1)
  smd->in1=inbyte;

 if(smd->incount==2){
  serial_func_data_s *funcp=handdatas->serial_funcs;
  unsigned int i,in2;
  int ctr1,ctr2;
  unsigned short rskeycode;

  in2=inbyte;
  smd->incount=0;
  if((smd->signal | 0xDF) == 0xFF) {
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("key 1");
   #endif
   for(i=0;i<handdatas->number_of_funcs;i++){
    if((!funcp->databytes[0]) && (funcp->databytes[1]==1) && (~funcp->databytes[2])){
     pds_pushkey(funcp->keycode);
     #ifdef MPX_DEBUG_SERIAL
     pds_textdisplay_printf("found keycode");
     #endif
     break;
    }
    funcp++;
   }
   #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
   lircStr("MOUSE\n",0x0100,0);
   #endif
  }

  if((smd->signal | 0xEF) == 0xFF) {
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("key 2");
   #endif
   for(i=0;i<handdatas->number_of_funcs;i++){
    if((!funcp->databytes[0]) && (funcp->databytes[1]==2) && (~funcp->databytes[2])) {
     pds_pushkey(funcp->keycode);
     #ifdef MPX_DEBUG_SERIAL
     pds_textdisplay_printf("found keycode");
     #endif
     break;
    }
    funcp++;
   }
   #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
   lircStr("MOUSE\n",0x0200,0);
   #endif
  }
  smd->signal=smd->signal & 0xCF;

  ctr1=0;
  ctr2=0;

  switch(smd->signal){
   case 140 :ctr2=in2-64;break;
   case 128 :ctr2=in2;break;
   case  67 :ctr1=smd->in1-64;break;
   case  64 :ctr1=smd->in1;break;
   case 207 :ctr2=in2-64;ctr1=smd->in1-64;break;
   case 204 :ctr2=in2-64;ctr1=smd->in1;break;
   case 195 :ctr2=in2;ctr1=smd->in1-64;break;
   case 192 :ctr2=in2;ctr1=smd->in1;
  }

  if(ctr2<0){
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("ctr 2, up");
   #endif
   rskeycode=0;
   for(i=0;i<handdatas->number_of_funcs;i++){
    if((!funcp->databytes[0]) && (funcp->databytes[1]==3) && (funcp->databytes[2]<=-ctr2)) {
     rskeycode=funcp->keycode;
     #ifdef MPX_DEBUG_SERIAL
     pds_textdisplay_printf("found keycode");
     #endif
    }
    funcp++;
   }
   #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
   lircStr("MOUSE\n",0x0300,0);
   #endif
   pds_pushkey(rskeycode);
  }

  if(ctr2>0){
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("ctr 2, down");
   #endif
   rskeycode=0;
   for(i=0;i<handdatas->number_of_funcs;i++){
    if((!funcp->databytes[0]) && (funcp->databytes[1]==4) && (funcp->databytes[2]<=ctr2)) {
     rskeycode=funcp->keycode;
     #ifdef MPX_DEBUG_SERIAL
     pds_textdisplay_printf("found keycode");
     #endif
    }
    funcp++;
   }
   #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
   lircStr("MOUSE\n",0x0400,0);
   #endif
   pds_pushkey(rskeycode);
  }

  if(ctr1<0){
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("ctr 1, left");
   #endif
   rskeycode=0;
   for(i=0;i<handdatas->number_of_funcs;i++){
    if((!funcp->databytes[0]) && (funcp->databytes[1]==5) && (funcp->databytes[2]<=-ctr1)) {
     rskeycode=funcp->keycode;
     #ifdef MPX_DEBUG_SERIAL
     pds_textdisplay_printf("found keycode");
     #endif
    }
    funcp++;
   }
   #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
   lircStr("MOUSE\n",0x0500,0);
   #endif
   pds_pushkey(rskeycode);
  }

  if(ctr1>0){
   #ifdef MPX_DEBUG_SERIAL
   pds_textdisplay_printf("ctr 1, right");
   #endif
   rskeycode=0;
   for(i=0;i<handdatas->number_of_funcs;i++){
    if((!funcp->databytes[0]) && (funcp->databytes[1]==6) && (funcp->databytes[2]<=ctr1)) {
     rskeycode=funcp->keycode;
     #ifdef MPX_DEBUG_SERIAL
     pds_textdisplay_printf("found keycode");
     #endif
    }
    funcp++;
   }
   #ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
   lircStr("MOUSE\n",0x0600,0);
   #endif
   pds_pushkey(rskeycode);
  }

 }else
  smd->incount++;
}

static unsigned int serial_handler_sermouse_init(struct serial_handler_data_s *handdatas)
{
 unsigned int com_baseport;

 if(handdatas->indatalen!=3)
  return 0;

 com_baseport=*((unsigned short *)&biosmem[0x400+(handdatas->portselect-1)*2]);
 if(!com_baseport) // is port enabled?
  return 0;
 handdatas->portdatas.baseport=com_baseport;

 handdatas->private_datas=calloc(1,sizeof(struct sermouse_data_s));
 if(!handdatas->private_datas)
  return 0;

 // setup UART registers
 outp (UART_LCR,0x00);  // DLAB off
 outp (UART_IER,0x00);  // Diasable interrupts
 outp (UART_LCR,0x80);  // DLAB on
 outpw(UART_DLLB,BAUD_TO_DIVISOR(1200));// set baud divisor
 outp (UART_LCR,0x02);  // DLAB off, 7 bits, 1 Stop, Noparity
 outp (UART_MCR,0x0b);  // Set RTS & DTR and AUX2 (required for interrupt!)
 outp (UART_FCR,0x07);  // enable & clear FIFO buffers, trigger @ 1byte
 outp (UART_IER,0x01);  // enable interrupts

 serial_interrupt_init(&handdatas->irqdatas,handdatas->portselect,(void far *)serial_handler_sermouse_irqroutine);

 sermouse_handlerdatas=handdatas;

 return 1;
}

static void serial_handler_sermouse_close(struct serial_handler_data_s *handdatas)
{
 serial_interrupt_close(&handdatas->irqdatas);
 if(handdatas->private_datas){
  free(handdatas->private_datas);
  handdatas->private_datas=NULL;
 }
}
#endif // MPXPLAY_LINK_SERIAL_SERMOUSE

//-------------------------------------------------------------------------
// TCP/IP stuff
/*
        FTP Server for WatTCP-32
	Written by L. De Cock, L. Van Deuren
	email: Luc.DeCock@planetinternet.be
	(C) 1998, 2000 L. De Cock
	This is free software. Use at your own risk

	This FTP server implements the base FTP command set.
	Passive mode is not supported yet.
*/
// ported to Watcom/MPXPlay by S.Zeller

// to compile install WATTCP in c:\wc\wattcp,
// build WATCOM 32-bit flat library (see 'install' in c:\wc\wattcp)
// append '-ic:\wc\wattcp\inc' in c:\wc\mpxplay\control\makefile  (done)
// and add library c:\wc\wattcp\lib\wattcpwf.lib to c:\wc\mpxplay\mpxplay.lnk (done)

// To use WATTCP with Windows download NDIS3PKT from http://www.danlan.com/

// mpxplay.ini config:
//
// HandlerCFG=FTPSRV   (port,baud,datalen not required)
// (SerialFunc lines are ignored, not required)

#ifdef MPXPLAY_LINK_SERIAL_TCPIP

#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <tcp.h> //wattcp header

extern int _watt_do_exit;

static unsigned int serial_wattcp_initialized;

#endif // MPXPLAY_LINK_SERIAL_TCPIP

#ifdef MPXPLAY_LINK_SERIAL_FTPSRV

#define VERSION "1.0.2-04 MPXPlay"

#define msgPassRequired   "331 Password required for %s."
#define msgBanner         "220 ALTEST FTP Server ready."
#define msgLoginFailed    "530 Login incorrect."
#define msgLogged         "230 User %s logged in."
#define msgNotLogged      "530 Please login with USER and PASS."
#define msgCmdUnknown     "500 '%s': command not understood."
#define msgCWDSuccess     "250 CWD command successful \"%s\"."
#define msgTypeFailed     "500 'TYPE %s': command not understood."
#define msgTypeOk         "200 Type set to %s."
#define msgPortSuccess    "200 Port command successful."
#define msgPortFailed     "501 Invalid PORT command."
#define msgDirOpen        "150 Opening data connection for directory list."
#define msgDirError       "426 Connection closed; transfer aborted."
#define msgDirFailed      "451 Failed: %s."
#define msgDirOk          "226 Closing data connection."
#define msgPWDSuccess     "257 \"%s\" is current directory."
#define msgRetrDisabled   "500 Cannot RETR."
#define msgRetrSuccess    "150 Opening data connection for %s (%lu bytes)"
#define msgRetrFailed     "501 Cannot RETR. %s"
#define msgRetrAborted    "426 Connection closed; %s."
#define msgRetrOk         "226 File sent successfully."
#define msgStorDisabled   "500 Cannot STOR."
#define msgStorSuccess    "150 Opening data connection for %s."
#define msgStorFailed     "501 Cannot STOR. %s"
#define msgStorAborted    "426 Connection closed; %s."
#define msgStorOk         "226 File received successfully."
#define msgStorError      "426 Connection closed; transfer aborted."
#define msgQuit           "221 Goodbye."
#define msgDeleOk         "250 File '%s' deleted."
#define msgDeleFailed     "450 File '%s' can't be deleted."
#define msgDeleSyntax     "501 Syntax error in parameter."
#define msgDeleNotExists  "550 Error '%s': no such file or directory."
#define msgRmdOk          "250 '%s': directory removed."
#define msgRmdNotExists   "550 '%s': no such directory."
#define msgRmdFailed      "550 '%s': can't remove directory."
#define msgRmdSyntax      "501 Syntax error in parameter."
#define msgMkdOk          "257 '%s': directory created."
#define msgMkdAlready     "550 '%s': file or directory already exists."
#define msgMkdFailed      "550 '%s': can't create directory."
#define msgMkdSyntax      "501 Syntax error in parameter."
#define msgRnfrNotExists  "550 '%s': no such file or directory."
#define msgRnfrSyntax     "501 Syntax error is parameter."
#define msgRnfrOk         "350 File exists, ready for destination name."
#define msgRntoAlready    "553 '%s': file already exists."
#define msgRntoOk         "250 File '%s' renamed to '%s'."
#define msgRntoFailed     "450 File '%s' can't be renamed."
#define msgRntoSyntax     "501 Syntax error in parameter."

#define SOCK_FLUSH(s) ((void)0) /* sock_flush() makes things slower */
//#define SOCK_FLUSH(s) sock_flush(s)

#define CBUF_LEN (16*1024) // dataconnection/communication buffer size
#define SBUF_LEN 2048      // server communication buffer size
#define RBUF_LEN 288       // renamefrom,currpath bufsize

#define FTPSRV_MAX_USERS     8
#define FTPSRV_MAX_TRANSFERS 4 // upload/download

typedef struct ftpserv_communication_s{
 tcp_Socket *srv;
 //char *sbuf;
 char *cbuf;
 struct ftpserv_communication_s *prev_login;
 struct ftpserv_communication_s *next_login;
 int usrCmd;
 int loggedIn;
 int execCmd,listening,inservice,ftpenable;
 unsigned int dataPort;
 union { unsigned char ip[4]; unsigned long ina;} dataIP;
 char *renamefrom;
 char *currpath;
 char password[9];
 char user[31];
}ftpserv_communication_s;

typedef struct ftpserv_dataconnection_s{
 tcp_Socket *cli;
 void *cli_buf;
 dosmem_t *dm;
}ftpserv_dataconnection_s;

static ftpserv_communication_s *users_fc[FTPSRV_MAX_USERS];

static char *strMon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static char sbuf[2048];

static const char *MsgSystem(int ready)
{
 static char buf[100];
 const char *serv = "MPXPlay ALTEST FTP";

 snprintf(buf,sizeof(buf)-sizeof(" ready"),"200 %s server", serv);
 if(ready)
  strcat (buf," ready");
 return (buf);
}

static void ftpsrv_send_message(tcp_Socket *srv,const char *msg, ...)
{
 va_list arg;
 char smsg[256];

 if(!srv)
  return;

 va_start(arg,msg);
 vsnprintf(smsg, sizeof(smsg), msg, arg);
 va_end(arg);
 strcat(smsg,"\r\n");
 tcp_tick(NULL);
 sock_fastwrite(srv,smsg,strlen(smsg));
 tcp_tick(NULL);
}

static ftpserv_dataconnection_s *ftpsrv_make_data_connection(ftpserv_communication_s *fcs)
{
 int res = 0;
 int retry = 3;
 ftpserv_dataconnection_s *tds;

 tds=calloc(1,sizeof(ftpserv_dataconnection_s));
 if(!tds)
  return tds;
 tds->cli=calloc(1,sizeof(tcp_Socket));
 if(!tds->cli)
  goto err_out_tds;

 tds->dm=calloc(1,sizeof(dosmem_t));
 if(!tds->dm)
  goto err_out_tds;
 //if(!pds_dpmi_dos_allocmem(tds->dm,CBUF_LEN))
 // goto err_out_tds;
 //tds->cli_buf=tds->dm->linearptr;

 tds->cli_buf=calloc(CBUF_LEN,1);
 if(!tds->cli_buf)
  goto err_out_tds;

 do{
  tcp_tick(NULL);
  res = tcp_open(tds->cli,20,fcs->dataIP.ina,fcs->dataPort,NULL);
  if(res)
   break;
 }while(--retry);
 if(!res)
  goto err_out_tds;

 sock_setbuf(tds->cli, tds->cli_buf, CBUF_LEN);

 while(!sock_established(tds->cli)) {
  tcp_tick(NULL);
  if(!tcp_tick(tds->cli))
   goto err_out_tds;
 }

 sock_mode(tds->cli, TCP_MODE_NONAGLE | SOCK_MODE_BINARY);

 return tds;

err_out_tds:
 if(tds){
  if(tds->cli)
   free(tds->cli);
  if(tds->cli_buf)
   free(tds->cli_buf);
  //if(tds->dm){
  // pds_dpmi_dos_freemem(tds->dm);
  // free(tds->dm);
  //}
  free(tds);
 }
 return NULL;
}

static void ftpsrv_close_data_connection(ftpserv_dataconnection_s *tds)
{
 if(tds){
  if(tds->cli){
   SOCK_FLUSH(tds->cli);
   sock_close(tds->cli);
   while(tcp_tick(tds->cli)){}
   tcp_tick(NULL); // ???
   free(tds->cli);
  }
  //if(tds->dm){
  // pds_dpmi_dos_freemem(tds->dm);
  // free(tds->dm);
  //}
  if(tds->cli_buf)
   free(tds->cli_buf);
  free(tds);
 }
}

static void StripCmd(char *cmd)
{
 int len = strlen(cmd)-1;

 while ((len >= 0) && ((cmd[len] == '\n') || (cmd[len] == '\r'))){
  cmd[len--] = 0;
 }
}

static void SwitchSlashesToBackSlashes(char *newdir)
{
#if (PDS_DIRECTORY_SEPARATOR_CHAR!='/')
 int idx, len = strlen(newdir);

 for(idx = 0; idx < len; idx++){
  if(newdir[idx] == '/')
   newdir[idx] = PDS_DIRECTORY_SEPARATOR_CHAR;
 }
#endif
}

static void CutTraillingSlash(char *newdir)
{
 int len = strlen(newdir);

 if(len < 2)
  return;

 if((newdir[len-1] == '/') || (newdir[len-1] == '\\'))
  newdir[len-1] = 0;
}

static void SwitchBackSlashesToSlashes(char *newdir)
{
#if (PDS_DIRECTORY_SEPARATOR_CHAR!='/')
 int idx, len = strlen(newdir);

 for(idx = 0; idx < len; idx++){
  if (newdir[idx] == PDS_DIRECTORY_SEPARATOR_CHAR)
   newdir[idx] = '/';
 }
#endif
}

static int CheckCommandAndReturnArg(char *which, char *cmd, char **arg)
{
 if(!*cmd){
  *arg = NULL;
  return 0;
 }

 while(*cmd && isalpha(*cmd)){
  if(*which != toupper(*cmd)){ // it's not this command
   return 0;
  }
  cmd++;
  which++;
 }

 while(*cmd && ((*cmd == ' ') || (*cmd == '\t'))){
  cmd++;
 }

 *arg = cmd;
 return 1;
}

static int CheckForAbort(tcp_Socket *srv)
{
 int len;

 if(!tcp_tick(srv)){ // connection closed by other side
  return 1;
 }

 if(sock_dataready(srv)) {
  len = sock_fastread (srv, sbuf, 512);
  sbuf[len] = 0;
  if(len > 0){
   if(!sbuf[0] || sbuf[0] == 0)
    return 1;
  }
  if(!strncmp(sbuf,"QUIT",4) || !strncmp(sbuf,"ABOR",4))
   return 1;
 }
 return 0;
}

static int CheckUser(ftpserv_communication_s *fcs,char *u)
{
 *fcs->user = 0;
 *fcs->password = 0;
 strncpy(fcs->user,u,30);
 return 1;
}

static int CheckPassword(ftpserv_communication_s *fcs,char *pass)
{
 *fcs->password = 0;
 /*
  All users and any password is accepted.
  Up to you to implements your needs.
  return a value 1 if password accepted or 0 if not
 */
 return 1;
}

static void ChangeDir(ftpserv_communication_s *fcs,char *newdir)
{
 int len = strlen(newdir);
 char *dd = newdir;

 /* Does it include a drive letter? */
 if((len > 2) && isalpha(*newdir) && (newdir[1] == ':')) {
  pds_setdrive(toupper(*newdir)-'A'+1);
  newdir += 2;
  len -= 2;
 }

 SwitchSlashesToBackSlashes(newdir);
 CutTraillingSlash(newdir);

 if(pds_chdir(newdir)!=0){
  ftpsrv_send_message(fcs->srv,msgDeleNotExists, dd);
 }else{
  SwitchBackSlashesToSlashes(dd);
  ftpsrv_send_message(fcs->srv,msgCWDSuccess, dd);
 }
}

static void ConfigDataSocket(ftpserv_communication_s *fcs,char *data)
{
 int nr;
 unsigned pHi, pLo;
 unsigned ip[4];

 nr = sscanf(data,"%u,%u,%u,%u,%u,%u", &(ip[0]), &(ip[1]),
			&(ip[2]), &(ip[3]), &pHi, &pLo);
 if (nr != 6) {
  ftpsrv_send_message(fcs->srv,msgPortFailed);
  return;
 }
 fcs->dataIP.ip[0] = (unsigned char) ip[3];
 fcs->dataIP.ip[1] = (unsigned char) ip[2];
 fcs->dataIP.ip[2] = (unsigned char) ip[1];
 fcs->dataIP.ip[3] = (unsigned char) ip[0];
 fcs->dataPort = ((unsigned) pHi << 8) | pLo;
 ftpsrv_send_message(fcs->srv,msgPortSuccess);
}

static void SendDirectory(ftpserv_communication_s *fcs,char *path, int longFormat)
{
 unsigned long year_curr;
 ftpserv_dataconnection_s *tds;
 struct pds_find_t entry;
 char line[512];

 if(!path || !*path)
  path = "*.*";

 if(strlen(path)==1){
  if(path[0]=='/')
   path = "/*.*";
  if(path[0]=='.')
   path = "*.*";
 }

 SwitchSlashesToBackSlashes(path);

 if(longFormat)
  year_curr=pds_getdate()>>16;

 ftpsrv_send_message(fcs->srv,msgDirOpen);
 tds=ftpsrv_make_data_connection(fcs);
 if(!tds){
  ftpsrv_send_message(fcs->srv,msgDirError);
  return;
 }

 if(pds_findfirst(path,_A_NORMAL|_A_ARCH|_A_SUBDIR|_A_RDONLY,&entry)!=0){
  ftpsrv_close_data_connection(tds);
  ftpsrv_send_message(fcs->srv,msgDirFailed, path);
  return;
 }

 do{
  tcp_tick (NULL);
  if(entry.attrib & (_A_VOLID | _A_HIDDEN | _A_SYSTEM)){// skip volume label and hidden files
   continue;
  }
  if(longFormat) {
   unsigned int year_file = ((unsigned int)entry.fdate.year)+1980;
   if(year_file == year_curr){
    snprintf(line,sizeof(line),"-rw-rw-rw-   1 user     ftp  %11ld %s %2.2d %2.2d:%2.2d %s\r\n",
		 entry.size, strMon[entry.fdate.month-1],
		 (unsigned int)entry.fdate.day, (unsigned int)entry.fdate.hours,
		 (unsigned int)entry.fdate.minutes, entry.name);
   }else{
    snprintf(line,sizeof(line),"-rw-rw-rw-   1 user     ftp  %11ld %s %2.2d %5d %s\r\n",
		 entry.size, strMon[entry.fdate.month-1],
		 (unsigned int)entry.fdate.day, year_file, entry.name);
   }
   if(entry.attrib & _A_SUBDIR){
    line[0] = 'd';
   }
   if(entry.attrib & _A_RDONLY){
    line[2] = '-';
    line[5] = '-';
    line[8] = '-';
   }
  }else{
   snprintf(line,sizeof(line),"%s\r\n", entry.name);
  }
  sock_fastwrite(tds->cli,line,strlen(line));
  if(!tcp_tick(tds->cli)){
   ftpsrv_close_data_connection(tds);
   ftpsrv_send_message(fcs->srv,msgDirError);
   pds_findclose(&entry);
   return;
  }
  if(CheckForAbort(fcs->srv)){
   ftpsrv_close_data_connection(tds);
   ftpsrv_send_message(fcs->srv,msgDirError);
   pds_findclose(&entry);
   return;
  }
 }while(pds_findnext(&entry)==0);

 pds_findclose(&entry);

 ftpsrv_close_data_connection(tds);
 ftpsrv_send_message(fcs->srv,msgDirOk);
}

typedef struct transferfile_s{
 ftpserv_dataconnection_s *tds;
 ftpserv_communication_s **fcu;
 int  filehandnum;
 long filelen;
 long bytes_processed;
 char *cbuf;
 unsigned int buflen;
 int timer_id;
}transferfile_s;

static void SendFile_callback(struct transferfile_s *sfi)
{
 ftpserv_communication_s *fcs;
 int len,sent,toSend,res;
 char *msg;

 if(sfi->bytes_processed>=sfi->filelen){
  msg=msgRetrOk;
  goto err_out_sendfile;
 }
 tcp_tick(NULL);

 len=pds_dos_read(sfi->filehandnum,sfi->cbuf,sfi->buflen);
 if(len<=0){
  msg=msgRetrFailed;
  goto err_out_sendfile;
 }

 toSend = len;
 sent = 0;
 while(sent < len){
  res = sock_write(sfi->tds->cli,&sfi->cbuf[sent],toSend);
  sent += res;
  toSend -= res;
  if(!tcp_tick(sfi->tds->cli)){
   msg=msgRetrFailed;
   goto err_out_sendfile;
  }
 }
 sfi->bytes_processed += len;
 fcs=*(sfi->fcu);
 if(!fcs)
  goto err_out_sendfile;
 /*
 if(CheckForAbort(fcs->srv)){
  msg=msgRetrAborted;
  goto err_out_sendfile;
 }*/

 return; // continue

err_out_sendfile: // finish
 mpxplay_timer_deletehandler(&SendFile_callback,sfi->timer_id);
 if(sfi->filehandnum)
  pds_close(sfi->filehandnum);
 ftpsrv_close_data_connection(sfi->tds);
 fcs=*(sfi->fcu);
 if(fcs)
  ftpsrv_send_message(fcs->srv,msg);
 free(sfi->cbuf);
 free(sfi);
}

static void SendFile_open(ftpserv_communication_s **fcu,char *filename)
{
 ftpserv_communication_s *fcs=*fcu;
 struct transferfile_s *sfi;

 sfi=calloc(1,sizeof(struct transferfile_s));
 if(!sfi){
  ftpsrv_send_message(fcs->srv,msgRetrFailed, filename);
  return;
 }
 sfi->buflen=8192;
 sfi->cbuf=calloc(sfi->buflen,1);
 if(!sfi->cbuf){
  ftpsrv_send_message(fcs->srv,msgRetrFailed, filename);
  goto err_out_sfopen;
 }

 SwitchSlashesToBackSlashes(filename);
 sfi->filehandnum=pds_open_read(filename,O_RDONLY|O_BINARY);
 if(!sfi->filehandnum){
  ftpsrv_send_message(fcs->srv,msgRetrFailed, filename);
  goto err_out_sfopen;
 }
 sfi->filelen = pds_filelength(sfi->filehandnum);

 ftpsrv_send_message(fcs->srv,msgRetrSuccess, filename, sfi->filelen);
 sfi->tds=ftpsrv_make_data_connection(fcs);
 if(!sfi->tds){
  ftpsrv_send_message(fcs->srv,msgRetrAborted, filename);
  goto err_out_sfopen;
 }

 sfi->timer_id=mpxplay_timer_addfunc(&SendFile_callback,sfi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
 if(sfi->timer_id<0)
  goto err_out_sfopen;

 sfi->fcu=fcu;

 return;

err_out_sfopen:
 if(sfi){
  if(sfi->filehandnum)
   pds_close(sfi->filehandnum);
  if(sfi->cbuf)
   free(sfi->cbuf);
  free(sfi);
 }
}

static void ReceiveFile_callback(struct transferfile_s *rfi)
{
 ftpserv_communication_s *fcs;
 char *msg;
 int len=-1;

 tcp_tick(NULL);

 //if(sock_dataready(rfi->tds->cli)){
  len = sock_read(rfi->tds->cli,rfi->cbuf,rfi->buflen);
  if(len>0)
   if(pds_dos_write(rfi->filehandnum,rfi->cbuf,len)!=len){
    msg=msgStorFailed;
    goto err_out_receive;
   }
 //}
 if(len<=0){
  fcs=*(rfi->fcu);
  if(!fcs)
   goto err_out_receive;
  if(CheckForAbort(fcs->srv)){
   msg=msgStorAborted;
   goto err_out_receive;
  }
 }
 if(!tcp_tick(rfi->tds->cli)){
  msg=msgStorOk;
  goto err_out_receive;
 }
 //tcp_tick(NULL);

 return; // continue

err_out_receive: // finish
 mpxplay_timer_deletehandler(&ReceiveFile_callback,rfi->timer_id);
 if(rfi->filehandnum)
  pds_close(rfi->filehandnum);
 ftpsrv_close_data_connection(rfi->tds);
 fcs=*(rfi->fcu);
 if(fcs)
  ftpsrv_send_message(fcs->srv,msg);
 free(rfi);
}

static void ReceiveFile_open(ftpserv_communication_s **fcu,char *filename)
{
 ftpserv_communication_s *fcs=*fcu;
 struct transferfile_s *rfi;

 rfi=calloc(1,sizeof(struct transferfile_s));
 if(!rfi){
  ftpsrv_send_message(fcs->srv,msgStorFailed, filename);
  return;
 }
 rfi->buflen=8192;
 rfi->cbuf=calloc(rfi->buflen,1);
 if(!rfi->cbuf){
  ftpsrv_send_message(fcs->srv,msgStorFailed, filename);
  goto err_out_rfopen;
 }

 SwitchSlashesToBackSlashes(filename);
 rfi->filehandnum=pds_open_create(filename,O_RDWR|O_BINARY);
 if(!rfi->filehandnum){
  ftpsrv_send_message(fcs->srv,msgStorFailed, filename);
  goto err_out_rfopen;
 }

 ftpsrv_send_message(fcs->srv,msgStorSuccess, filename);

 rfi->tds=ftpsrv_make_data_connection(fcs);
 if(!rfi->tds){
  ftpsrv_send_message(fcs->srv,msgStorAborted, filename);
  goto err_out_rfopen;
 }

 rfi->timer_id=mpxplay_timer_addfunc(&ReceiveFile_callback,rfi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
 if(rfi->timer_id<0)
  goto err_out_rfopen;

 rfi->fcu=fcu;

 return;

err_out_rfopen:
 if(rfi){
  if(rfi->filehandnum){
   pds_close(rfi->filehandnum);
   pds_unlink(filename);
  }
  if(rfi->cbuf)
   free(rfi->cbuf);
  free(rfi);
 }
}

static void DeleteFile(ftpserv_communication_s *fcs,char *filename)
{
 if(!filename || !*filename){
  ftpsrv_send_message(fcs->srv,msgDeleSyntax);
  return;
 }

 SwitchSlashesToBackSlashes(filename);
 if(pds_unlink(filename)!=0)
  ftpsrv_send_message(fcs->srv,msgDeleFailed, filename);
 else
  ftpsrv_send_message(fcs->srv,msgDeleOk, filename);
}

static void RemoveDirectory(ftpserv_communication_s *fcs,char *thedir)
{
 if(!thedir || !*thedir){
  ftpsrv_send_message(fcs->srv,msgRmdSyntax);
  return;
 }

 SwitchSlashesToBackSlashes(thedir);
 CutTraillingSlash(thedir);

 if(pds_rmdir(thedir)!=0)
  ftpsrv_send_message(fcs->srv,msgRmdFailed, thedir);
 else
  ftpsrv_send_message(fcs->srv,msgRmdOk, thedir);
}

static void MakeDirectory(ftpserv_communication_s *fcs,char *thedir)
{
 if(!thedir || !*thedir){
  ftpsrv_send_message(fcs->srv,msgMkdSyntax);
  return;
 }

 SwitchSlashesToBackSlashes(thedir);
 CutTraillingSlash(thedir);

 if(pds_mkdir(thedir)!=0)
  ftpsrv_send_message(fcs->srv,msgMkdFailed, thedir);
 else
  ftpsrv_send_message(fcs->srv,msgMkdOk, thedir);
}

// set source filename
static void RenameFrom(ftpserv_communication_s *fcs,char *thename)
{
 if(!thename || !*thename){
  ftpsrv_send_message(fcs->srv,msgRnfrSyntax);
  return;
 }

 SwitchSlashesToBackSlashes(thename);
 //if(access(thename,0)){  // ???
 // ftpsrv_send_message(fcs->srv,msgRnfrNotExists,thename);
 //}else{
  pds_strcpy(fcs->renamefrom,thename);
  ftpsrv_send_message(fcs->srv,msgRnfrOk);
 //}
}

// set target filename and rename
static void RenameTo(ftpserv_communication_s *fcs,char *thename)
{
 if(!thename || !*thename){
  ftpsrv_send_message(fcs->srv,msgRntoSyntax);
  return;
 }

 SwitchSlashesToBackSlashes(thename);

 if(pds_rename(fcs->renamefrom,thename)!=0)
  ftpsrv_send_message(fcs->srv,msgRntoFailed,fcs->renamefrom);
 else
  ftpsrv_send_message(fcs->srv,msgRntoOk,fcs->renamefrom,thename);
}

//--------------------------------------------------------------------------

static int ftpsrv_serve_cycle(ftpserv_communication_s **fcu)
{
 ftpserv_communication_s *fcs=*fcu;
 char *arg;
 int cmdlen=0;

 if(!tcp_tick(fcs->srv)) //
  return 0;              // close
 if(!sock_dataready(fcs->srv))
  return 1;
 cmdlen = sock_gets(fcs->srv, sbuf, 512); // length of received command
 if(cmdlen<3 || cmdlen>511)
  return 1;
 sbuf[cmdlen]=0;
 if(!sbuf[0])  // ???
  return 0;    // close forced
 StripCmd(sbuf);

 // check out which command the client sent
 if(CheckCommandAndReturnArg("USER",sbuf, &arg)){
  if(CheckUser(fcs,arg)){ // user is known
   ftpsrv_send_message(fcs->srv,msgPassRequired,fcs->user);
   fcs->usrCmd = 1;
  }else{
   fcs->usrCmd = 0;
  }
  return 1;
 }
 if(CheckCommandAndReturnArg("PASS",sbuf,&arg) && fcs->usrCmd){
  if(CheckPassword(fcs,arg)){
   fcs->loggedIn = 1;
   ftpsrv_send_message(fcs->srv,msgLogged,fcs->user);
  }else{
   ftpsrv_send_message(fcs->srv,msgLoginFailed);
   fcs->loggedIn = 0;
   fcs->usrCmd = 0;
  }
  return 1;
 }
 if(CheckCommandAndReturnArg("QUIT",sbuf,&arg)){
  ftpsrv_send_message(fcs->srv,msgQuit);
  return 0;
 }
 if(CheckCommandAndReturnArg("SYST",sbuf,&arg)){
  ftpsrv_send_message(fcs->srv,MsgSystem(1));
  return 1;
 }
 if(CheckCommandAndReturnArg("PORT",sbuf, &arg)){
  ConfigDataSocket(fcs,arg);
  return 1;
 }
 if(!fcs->loggedIn){
  ftpsrv_send_message(fcs->srv,msgNotLogged);
  return 1;
 }
 if(CheckCommandAndReturnArg("CWD",sbuf, &arg)){
  ChangeDir(fcs,arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("TYPE",sbuf, &arg)){
  ftpsrv_send_message(fcs->srv,msgTypeOk, arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("NLST",sbuf, &arg)){
  SendDirectory(fcs,arg, 0);
  return 1;
 }
 if(CheckCommandAndReturnArg("LIST",sbuf, &arg)){
  SendDirectory(fcs,arg, 1);
  return 1;
 }
 if(CheckCommandAndReturnArg("PWD",sbuf, &arg) || CheckCommandAndReturnArg("XPWD",sbuf, &arg)){
  int ibuf=0;
  pds_getcwd(sbuf);

  while(sbuf[ibuf]){
   sbuf[ibuf]=sbuf[ibuf+2];
   ibuf++;
  }
  sbuf[ibuf]=0;
  SwitchBackSlashesToSlashes(sbuf);
  ftpsrv_send_message(fcs->srv,msgPWDSuccess, sbuf);
  return 1;
 }
 if(CheckCommandAndReturnArg("RETR",sbuf, &arg)){
  SendFile_open(fcu,arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("STOR",sbuf, &arg)){
  ReceiveFile_open(fcu,arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("DELE",sbuf, &arg)){
  DeleteFile(fcs,arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("RMD",sbuf, &arg) || CheckCommandAndReturnArg("XRMD",sbuf, &arg)){
  RemoveDirectory(fcs,arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("MKD",sbuf, &arg) || CheckCommandAndReturnArg("XMKD",sbuf, &arg)){
  MakeDirectory(fcs,arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("RNFR",sbuf, &arg)){
  RenameFrom(fcs,arg);
  return 1;
 }
 if(CheckCommandAndReturnArg("RNTO",sbuf, &arg)){
  RenameTo(fcs,arg);
  return 1;
 }
 ftpsrv_send_message(fcs->srv,msgCmdUnknown, sbuf);
 return 1;
}

//--------------------------------------------------------------------

static void ftpsrv_serve_dealloc(struct ftpserv_communication_s *fcs)
{
 if(fcs){
  if(fcs->cbuf)
   free(fcs->cbuf);
  //if(fcs->sbuf)
  // free(fcs->sbuf);
  if(fcs->renamefrom)
   free(fcs->renamefrom);
  if(fcs->currpath)
   free(fcs->currpath);
  if(fcs->srv)
   free(fcs->srv);
  free(fcs);
 }
}

static unsigned int ftpsrv_serve_listen_init(void)
{
 struct ftpserv_communication_s **fcu,*fcs;
 unsigned int i;

 fcu=&users_fc[0];
 for(i=0;i<FTPSRV_MAX_USERS;i++,fcu++)
  if(*fcu && (*fcu)->listening)
   return 1;

 fcs=NULL;
 fcu=&users_fc[0];
 for(i=0;i<FTPSRV_MAX_USERS;i++,fcu++){
  fcs=*fcu;
  if(!fcs)
   break;
 }

 if(fcs) // free entry not found
  return 0;

 fcs=calloc(1,sizeof(struct ftpserv_communication_s));
 if(!fcs)
  return 0;
 fcs->srv=calloc(1,sizeof(*fcs->srv));
 if(!fcs->srv)
  goto err_out_sl;
 tcp_listen(fcs->srv, 21, 0, 0, NULL, 0);

 fcs->listening=1;
 *fcu=fcs;

 return 1;

err_out_sl:
 ftpsrv_serve_dealloc(fcs);
 return NULL;
}

static void ftpsrv_serve_close(struct ftpserv_communication_s **fcu)
{
 struct ftpserv_communication_s *fcs=*fcu;
 if(fcs){
  fcs->inservice=0;
  if(fcs->srv)
   while(sock_close(fcs->srv)!=0){}
  ftpsrv_serve_dealloc(fcs);
 }
 *fcu=NULL;
 ftpsrv_serve_listen_init();
}

static unsigned int ftpsrv_serve_login(struct ftpserv_communication_s **fcu)
{
 struct ftpserv_communication_s *fcs=*fcu;

 fcs->dataPort = 20;	// default data connection on port 20
 fcs->cbuf=calloc(CBUF_LEN,1);
 if(!fcs->cbuf)
  goto err_out_si;
 //fcs->sbuf=calloc(SBUF_LEN,1);
 //if(!fcs->sbuf)
 // goto err_out_si;
 fcs->renamefrom=calloc(RBUF_LEN,1);
 if(!fcs->renamefrom)
  goto err_out_si;
 fcs->currpath=calloc(RBUF_LEN,1);
 if(!fcs->currpath)
  goto err_out_si;
 pds_getcwd(fcs->currpath);

 sock_mode(fcs->srv, TCP_MODE_NONAGLE | TCP_MODE_ASCII);

 sock_setbuf(fcs->srv, fcs->cbuf, CBUF_LEN);

 ftpsrv_send_message(fcs->srv,MsgSystem(0));

 ftpsrv_serve_listen_init();

 fcs->inservice=1;
 fcs->listening=0;

 return 1;

err_out_si:
 ftpsrv_serve_close(fcu);
 return 0;
}

static void ftpsrv_maincycle(void)
{
 ftpserv_communication_s **fcu=&users_fc;
 unsigned int users=0;
 do{
  struct ftpserv_communication_s *fcs=*fcu;
  if(fcs){
   if(fcs->listening)
    if(sock_established(fcs->srv))
     ftpsrv_serve_login(fcu);
    else
     tcp_tick(fcs->srv);
   if(fcs->inservice)
    if(!ftpsrv_serve_cycle(fcu))
     ftpsrv_serve_close(fcu);
  }
  fcu++;
 }while(++users<FTPSRV_MAX_USERS);
 tcp_tick(NULL);
}

//-------------------------------------------------------------------------

static unsigned int serial_handler_ftpsrv_init(struct serial_handler_data_s *handdatas)
{
 _watt_do_exit = 0;   // don't exit from the program in sock_init()
 if(!serial_wattcp_initialized)
  if(sock_init()!=0)
   return 0;
 if(!ftpsrv_serve_listen_init())
  return 0;
 return 1;
}

static void serial_handler_ftpsrv_close(struct serial_handler_data_s *handdatas)
{
 if(serial_wattcp_initialized){
  sock_exit(); // ???
  serial_wattcp_initialized=0;
 }
}

static void serial_handler_ftpsrv_read(struct serial_handler_data_s *handdatas)
{
 ftpsrv_maincycle();
}

#endif // MPXPLAY_LINK_SERIAL_FTPSRV

/*******************************************************************************/

#ifdef MPXPLAY_LINK_SERIAL_TELSRV

#define SCREEN_LIN_ADDR 0xB8000

#define DISP_LEN    1999  // ( 25 lines @ 80 chars ) - 1 to avoid screen flicker

#define TELBUFLEN DISP_LEN + 200 // a little more to allow esc sequences

static unsigned char telvttable[256] =  // this conversion table could be filled better
{
  ' ', ' ', 'O', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', '.', ' ', ' ', ' ', ' ', ' '
};

static tcp_Socket telcommunicationSock, *telsrv = &telcommunicationSock;
static char telsmsg[TELBUFLEN];
static int tellistening;
static int telinService;
static int telenable;
static int catinit;

static void telServeInit(void)
{
 sock_mode(telsrv, TCP_MODE_NONAGLE);
 sock_mode(telsrv, TCP_MODE_BINARY);
 catinit=0;
 telsmsg[catinit++]=0x1b;
 telsmsg[catinit++]='[';
 telsmsg[catinit++]='H';
 telsmsg[catinit++]=0x1b;
 telsmsg[catinit++]='[';
 telsmsg[catinit++]='m';
}

static int telServeCycle(struct serial_handler_data_s *handdatas)
{
 if(tcp_tick(telsrv)){
  unsigned char inchar,*addr,oldclr;
  int i,catpos;

  if(sock_fastread (telsrv, &inchar, 1) > 0){
   _disable(); //VTRead is not reentrant and is also called by the VT100 handler
   VTRead(handdatas,inchar);
   _enable();
  }

  oldclr=0;
  i=0;
  addr=(char *)SCREEN_LIN_ADDR;
  catpos=catinit;

  while ((i<DISP_LEN) && (catpos < TELBUFLEN-6)){
   unsigned char clrattr=addr[i*2+1] & 0xF0;

   if(clrattr && (!oldclr)){
    telsmsg[catpos++]=0x1b;
    telsmsg[catpos++]='[';
    telsmsg[catpos++]='7';
    telsmsg[catpos++]='m';
   }
   if((!clrattr) && oldclr){
    telsmsg[catpos++]=0x1b;
    telsmsg[catpos++]='[';
    telsmsg[catpos++]='m';
   }

   telsmsg[catpos++]=telvttable[addr[(i++)*2]];
   oldclr=clrattr;
  }
  telsmsg[catpos]=0;
  sock_fastwrite(telsrv,telsmsg,catpos);
  sock_flush(telsrv);
  return 1;
 }
 return 0;
}

static void telcycle(struct serial_handler_data_s *handdatas)
{
 if(!tellistening){
  tcp_listen(telsrv, 23, 0, 0, NULL, 0);
  tellistening = 1;
 }
 if(sock_established(telsrv)){
  telenable = 1;
  if(!telinService){
   telServeInit();
   telinService = 1;
  }
  if(!telServeCycle(handdatas)){
   tellistening = 0;
   telinService = 0;
   telenable = 0;
  }
 }else{
  if(telenable){
   telenable = 0;
   telinService = 0;
   tellistening = 0;
  }
 }
}

static unsigned int serial_handler_telsrv_init(struct serial_handler_data_s *handdatas)
{
 _watt_do_exit = 0;   // don't exit from the program in sock_init()
 if(!serial_wattcp_initialized){
  if(sock_init()!=0)
   return 0;
  serial_wattcp_initialized=1;
 }
 VTInit();
 return 1;
}

static void serial_handler_telsrv_close(struct serial_handler_data_s *handdatas)
{
 if(serial_wattcp_initialized){
  sock_exit(); // ???
  serial_wattcp_initialized=0;
 }
}

static void serial_handler_telsrv_read(struct serial_handler_data_s *handdatas)
{
 telcycle(handdatas);
 tcp_tick(NULL);
}

#endif // MPXPLAY_LINK_SERIAL_TELSRV

/*********************************************************************************
LIRC compatible server
This server can be queried over TCPIP from lirc compatible applications such as
GIRDER. It allows me to control media players on my Windows PC with IR signals that
are sent to my DIY MPXPlayer, which is located in another room.
*********************************************************************************/
#ifdef MPXPLAY_LINK_SERIAL_IRCSRV

#define LIRCBUFLEN 128

static void lirccycle(void);

static tcp_Socket lirccommunicationSock, *lircsrv = &lirccommunicationSock;
static char lircsmsg[LIRCBUFLEN];
static int lirclistening;
static int lircinService;
static int lircenable;

#ifdef MPXPLAY_LINK_SERIAL_LIRC_DEBUG
static void lircStr(char *lircmsg,unsigned long lircode,unsigned int lircrep)
{
 static char msg[40];
 if(lircenable){
  _disable();
  sprintf(msg,"00000000%8x %2u %8x ",lircode,lircrep,lircode);
  strcat(msg,lircmsg);
  if ((strlen(msg)+strlen(lircsmsg)) < LIRCBUFLEN)
   strcat(lircsmsg,msg);
  _enable();
 }
}
#endif // MPXPLAY_LINK_SERIAL_LIRC_DEBUG

static void lircServeInit(void)
{
 sock_mode(lircsrv, TCP_MODE_NONAGLE);
 sock_mode(lircsrv, TCP_MODE_ASCII);
}

static int lircServeCycle(void)
{
 if(tcp_tick(lircsrv)){
  sock_fastwrite(lircsrv,lircsmsg,strlen(lircsmsg));
  lircsmsg[0]=0;
  sock_flush(lircsrv);
  return 1;
 }
 return 0;
}

static void lirccycle(void)
{
 if(!lirclistening){
  tcp_listen(lircsrv, 8765, 0, 0, NULL, 0);
  lirclistening = 1;
 }

 if(sock_established(lircsrv)){
  lircenable = 1;
  if(!lircinService){
   lircServeInit();
   lircinService = 1;
  }
  if(!lircServeCycle()){
   lirclistening = 0;
   lircinService = 0;
   lircenable = 0;
  }
 }else{
  if(lircenable){
   lircenable = 0;
   lircinService = 0;
   lirclistening = 0;
  }
 }
}

static unsigned int serial_handler_ircsrv_init(struct serial_handler_data_s *handdatas)
{
 _watt_do_exit = 0;   // don't exit from the program in sock_init()
 if(!serial_wattcp_initialized){
  if(sock_init()!=0)
   return 0;
  serial_wattcp_initialized=1;
 }
 return 1;
}

static void serial_handler_ircsrv_close(struct serial_handler_data_s *handdatas)
{
 if(serial_wattcp_initialized){
  sock_exit(); // ???
  serial_wattcp_initialized=0;
 }
}

static void serial_handler_ircsrv_read(struct serial_handler_data_s *handdatas)
{
 lirccycle();
 tcp_tick(NULL);
}

#endif // MPXPLAY_LINK_SERIAL_IRCSRV
