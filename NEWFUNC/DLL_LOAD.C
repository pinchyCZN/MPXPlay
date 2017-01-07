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
//function: DLL manager

#include "newfunc.h"
#include "dll_load.h"

#ifdef MPXPLAY_LINK_DLLLOAD // defined in dll_load.h

//#define DLLLOAD_DEBUG 1

#include <stdlib.h>
#include <direct.h>
#include <i86.h>

#include "control\control.h"
#include "playlist\playlist.h"

#if defined(__DOS__) && defined(__WATCOMC__)
#include "dos32lib.h"
#endif

#define DLLFOUNDS_INITSIZE 32 // initial number of max loaded DLLs

extern struct mpxplay_resource_s mpxplay_resources;

#if defined(__DOS__) && defined(__WATCOMC__)
extern unsigned long _dynend; // !!! begin of crwdata (65 bytes)
#endif

static dll_found_s *dll_founds;
static unsigned int dllfounds_size,dllfounds_entries,dllfounds_loaded;
unsigned int newfunc_dllload_displaydlls;

static void dllload_unload_and_clear_dllinfo(struct dll_found_s *d);
static mpxplay_module_entry_s **dllload_copy_mei(mpxplay_module_entry_s **mei_dest,mpxplay_module_entry_s **mei_src);
static void dllload_clear_mei(mpxplay_module_entry_s **mei);
static void dllload_clear_modulecallpoints(mpxplay_module_entry_s **mei);
static unsigned int dllload_check_modulecallpoints(mpxplay_module_entry_s **mei);

static unsigned int newfunc_dllload_alloc(void)
{
 struct dll_found_s *newp;
 unsigned int newsize;

 if(dllfounds_size)
  newsize=dllfounds_size<<2;
 else
  newsize=DLLFOUNDS_INITSIZE;
 newp=pds_calloc(newsize,sizeof(struct dll_found_s));
 if(!newp)
  return 0;
 if(dll_founds){
  pds_memcpy(newp,dll_founds,dllfounds_size*sizeof(struct dll_found_s));
  pds_free(dll_founds);
 }
 dll_founds=newp;
 dllfounds_size=newsize;
 return newsize;
}

static void dllload_getfilenames(void)
{
 unsigned int fferror,len;
 struct pds_find_t ffblk;
 char dllpath[300],dllsearchmask[300];

 pds_getpath_from_fullname(dllpath,freeopts[OPT_PROGNAME]);
 len=pds_strlen(dllpath);
 if(len && dllpath[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR)
  pds_strcpy(&dllpath[len],PDS_DIRECTORY_SEPARATOR_STR);

 len=pds_strcpy(dllsearchmask,dllpath);
 pds_strcpy(&dllsearchmask[len],"*.dll");

 fferror=pds_findfirst(dllsearchmask,_A_NORMAL,&ffblk);
 while(!fferror){
  if(!(ffblk.attrib&(_A_SUBDIR|_A_VOLID))){
   struct dll_found_s *d;

   if(dllfounds_entries>=dllfounds_size)
    if(!newfunc_dllload_alloc())
     break;
   d=&dll_founds[dllfounds_entries];
   d->dllfilename=(char *)malloc(pds_strlen(dllpath)+pds_strlen(ffblk.name)+1);
   if(!d->dllfilename)
    break;
   len=pds_strcpy(d->dllfilename,dllpath);
   pds_strcpy(&d->dllfilename[len],ffblk.name);
   dllfounds_entries++;
  }
  fferror=pds_findnext(&ffblk);
 }
 pds_findclose(&ffblk);
}

static unsigned int dllload_load_dll(struct dll_found_s *d)
{
 unsigned int m,error;
 mpxplay_dll_entry_s *mdep;
#if defined(MPXPLAY_WIN32)
 FARPROC fp;
#elif defined(__DOS__) && defined(__WATCOMC__)
 DWORD (*fp)();
 TSF32 tsfregs;
#endif

 if(!d)
  return 0;
 if(d->prochandle)
  return 1;

 error=1;
#if defined(MPXPLAY_WIN32)
 d->prochandle = LoadLibrary(d->dllfilename);
#elif defined(__DOS__) && defined(__WATCOMC__)
 pds_memset((void *)&tsfregs,0,sizeof(TSF32));
 d->prochandle = D32LoadModule(d->dllfilename,"\x00\r" ,&tsfregs);
#else
 return 0;
#endif
#ifdef DLLLOAD_DEBUG
 fprintf(stdout,"%s %2d ",d->dllfilename,d->prochandle);
#endif

 if(d->prochandle){
#if defined(MPXPLAY_WIN32)
  fp=GetProcAddress(d->prochandle,"mpxplay_dll_entrypoint_");
#elif defined(__DOS__) && defined(__WATCOMC__)
  fp=D32QueryProcAddr(d->prochandle,0,"mpxplay_dll_entrypoint_",(DWORD *)&fp);
#endif
#ifdef DLLLOAD_DEBUG
  fprintf(stdout,"fp:%8.8X ",(DWORD)fp);
#endif
  if(fp){
#if defined(__DOS__) && defined(__WATCOMC__)
   mdep=(mpxplay_dll_entry_s *)(*fp)(&mpxplay_resources,&_dynend); // this sends the mpxplay-resources and gets the dll-entry-structure
#else
   mdep=(mpxplay_dll_entry_s *)(*fp)(); // this gets the dll-entry-structure
#endif
   if(mdep && (mdep->entry_structure_version==MPXPLAY_DLLENTRY_STRUCTURE_VERSION) && mdep->module_entries[0]){
    for(m=0;m<MPXPLAY_DLLENTRY_MAX_MODULES;m++){
     if(!mdep->module_entries[m]){  // are the modules closed with a NULL ?
      d->module_entries=dllload_copy_mei(&d->module_entries[0],&mdep->module_entries[0]);
      if(d->module_entries)
       error=0;
      break;
     }
#ifdef DLLLOAD_DEBUG
     fprintf(stdout,"\n module: mj:%4.4X mi:%4.4X mn:%4s mv:%4.4X mp:%8.8X",
            mdep->module_entries[m]->moduletype_major,mdep->module_entries[m]->moduletype_minor,mdep->module_entries[m]->modulename_minor,mdep->module_entries[m]->module_structure_version,mdep->module_entries[m]->module_callpoint);
#endif
    }
   }
  }
 }
 if(error)
  dllload_unload_and_clear_dllinfo(d);
#ifdef DLLLOAD_DEBUG
 fprintf(stdout,"\n");
#endif
 return (!error);
}

// unload DLL, but keep infos
static void dllload_unload_dll(struct dll_found_s *d)
{
 if(d){
  if(d->prochandle){
#if defined(MPXPLAY_WIN32)
   FreeLibrary(d->prochandle);
#elif defined(__DOS__) && defined(__WATCOMC__)
   D32UnloadModule(d->prochandle);
#endif
   d->prochandle=0;
  }
  dllload_clear_modulecallpoints(d->module_entries);
 }
}

// unload dll, if all modules are 'disabled' (modulecallpoint==NULL)
static unsigned int dllload_check_and_unload_dll(struct dll_found_s *d)
{
 if(!dllload_check_modulecallpoints(d->module_entries)){
  dllload_unload_dll(d);
  return 1;
 }
 return 0;
}

// unload DLL, clear all infos (unusefull DLL)
static void dllload_unload_and_clear_dllinfo(struct dll_found_s *d)
{
 if(d){
  if(d->dllfilename)
   free(d->dllfilename);
  if(d->prochandle){
#if defined(MPXPLAY_WIN32)
   FreeLibrary(d->prochandle);
#elif defined(__DOS__) && defined(__WATCOMC__)
   D32UnloadModule(d->prochandle);
#endif
  }
  dllload_clear_mei(d->module_entries);
  pds_memset(d,0,sizeof(dll_found_s));
 }
}

//------------------------------------------------------------------

// copy module infos (structures) from the DLL to Mpxplay's memory
static mpxplay_module_entry_s **dllload_copy_mei(mpxplay_module_entry_s **mei_dest,mpxplay_module_entry_s **mei_src)
{
 unsigned int m,i;

 if(mei_src){
  m=0;
  while(mei_src[m]) // count modules
   m++;
  if(!m)
   return mei_dest;
  // allocate module-info space
  if(!mei_dest){
   mei_dest=(mpxplay_module_entry_s **)calloc(m+1,sizeof(mpxplay_module_entry_s *));
   if(!mei_dest)
    return mei_dest;
   for(i=0;i<m;i++){
    mei_dest[i]=calloc(1,sizeof(mpxplay_module_entry_s));
    if(!mei_dest[i])
     goto err_out;
   }
  }
  for(i=0;i<m;i++){
   mpxplay_module_entry_s *md=mei_dest[i],*ms=mei_src[i];
   md->moduletype_major=ms->moduletype_major;
   md->moduletype_minor=ms->moduletype_minor;
   if(ms->modulename_minor){
    if(!md->modulename_minor){
     md->modulename_minor=malloc(pds_strlen(ms->modulename_minor)+1);
     if(md->modulename_minor)
      pds_strcpy(md->modulename_minor,ms->modulename_minor); // copy modulename_minor
    }
   }
   md->module_structure_version=ms->module_structure_version;
   md->module_callpoint=ms->module_callpoint;
  }
 }
 return mei_dest;

err_out:
 dllload_clear_mei(mei_dest);
 return NULL;
}

// clear all module infos (the dll is unusefull for us)
static void dllload_clear_mei(mpxplay_module_entry_s **mei)
{
 if(mei){
  mpxplay_module_entry_s **m=mei;
  while(*m){
   if((*m)->modulename_minor)
    free((*m)->modulename_minor);
   free(*m);
   m++;
  }
  free(mei);
 }
}

// disable module (we don't want to use it further)
static void dllload_disable_modulecallpoint(mpxplay_module_entry_s **mei)
{
 if(mei && *mei)
  (*mei)->module_callpoint=NULL;
}

// after the close of a dll, the module callpoints are invalid (at the next loading they will change)
static void dllload_clear_modulecallpoints(mpxplay_module_entry_s **mei)
{
 if(mei){
  while(*mei){
   (*mei)->module_callpoint=NULL;
   mei++;
  }
 }
}

// retrun 0, if all module_callpoints are NULL (there is no usefull module in the DLL)
static unsigned int dllload_check_modulecallpoints(mpxplay_module_entry_s **mei)
{
 if(mei){
  while(*mei){
   if((*mei)->module_callpoint)
    return 1;
   mei++;
  }
 }
 return 0;
}

//-----------------------------------------------------------------------

static void dllload_checkall(void)
{
 unsigned int i;
 struct dll_found_s *d;

 if(dllfounds_loaded)
  return;

 dllload_getfilenames();

 d=dll_founds;
 if(d){
  for(i=0;i<dllfounds_entries;i++,d++){
   dllload_load_dll(d);       // open, load infos/structures
   dllload_unload_dll(d);     // close (keep infos)
  }
 }
 dllfounds_loaded=1;
#ifdef DLLLOAD_DEBUG
 pds_extgetch();
 fflush(stdout);
#endif
}

#endif // MPXPLAY_LINK_DLLLOAD

mpxplay_module_entry_s *newfunc_dllload_getmodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor_p,mpxplay_module_entry_s *prev_module)
{
 mpxplay_module_entry_s *module=NULL;
#ifdef MPXPLAY_LINK_DLLLOAD
 unsigned int i;
 struct dll_found_s *d,*dll=NULL;
 mpxplay_module_entry_s **mme_pos=NULL;
 char *modulename_minor,modulename_minor_s[128];

 if(!dllfounds_entries)
  dllload_checkall();

 d=dll_founds;
 if(d){
  if(modulename_minor_p){
   pds_strcpy(modulename_minor_s,modulename_minor_p); // !!! this is not the right place
   if(pds_strcutspc(modulename_minor_s))              // !!! to do this
    modulename_minor=&modulename_minor_s[0];
  }else
   modulename_minor=NULL;

  for(i=0;i<dllfounds_entries;i++,d++){
   mpxplay_module_entry_s **mmep=d->module_entries;
   if(!mmep || !*mmep)
    continue;
   do{
    if( ((*mmep)->moduletype_major==moduletype_major)
     && ( (!moduletype_minor && !modulename_minor && ((*mmep)>prev_module))
          || (moduletype_minor && (moduletype_minor==(*mmep)->moduletype_minor))
          || (modulename_minor && (pds_stricmp(modulename_minor,(*mmep)->modulename_minor)==0))
        )
     && (!module || ((*mmep)<module)) // to get the first/next module (sequential addresses)
    ){
     module=*mmep;
     dll=d;
     mme_pos=mmep;
    }

    mmep++;
   }while(*mmep);
  }
  if(module){
   if(dllload_load_dll(dll)){
    if(!module->module_callpoint){ // refresh/reload module_callpoint (if disablemodule called before)
     i=mme_pos - d->module_entries;
     if(i<MPXPLAY_DLLENTRY_MAX_MODULES)
      module->module_callpoint=d->me_dll[i]->module_callpoint;
    }
   }else
    module=NULL;
  }
 }
#ifdef DLLLOAD_DEBUG
 if(module)
  fprintf(stdout,"GETMODULE success: mj:%4.4X mi:%4.4X mn:%4s mv:%4.4X mp:%8.8X\n",
            module->moduletype_major,module->moduletype_minor,module->modulename_minor,module->module_structure_version,module->module_callpoint);
 else
  fprintf(stdout,"GETMODULE failed : mj:%4.4X mi:%4.4X mn:%4s         pv:%8.8X\n",
            moduletype_major,moduletype_minor,modulename_minor,prev_module);
 fflush(stdout);
#endif
#endif
 return module;
}

unsigned int newfunc_dllload_reloadmodule(mpxplay_module_entry_s *module)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 unsigned int i;
 struct dll_found_s *d;

 if(!dllfounds_entries)
  return 0;
 if(!module)
  return 0;

 d=dll_founds;
 if(d){
  for(i=0;i<dllfounds_entries;i++,d++){
   mpxplay_module_entry_s **mmep=d->module_entries;
   if(!mmep || !*mmep)
    continue;
   do{
    if((*mmep) == module){
     if(dllload_load_dll(d)){
      if(!module->module_callpoint){ // refresh/reload module_callpoint (if disablemodule called before)
       unsigned int j=mmep - d->module_entries;
       if(j<MPXPLAY_DLLENTRY_MAX_MODULES)
        module->module_callpoint=d->me_dll[j]->module_callpoint;
      }
      return 1;
     }
     return 0;
    }
    mmep++;
   }while(*mmep);
  }
 }
#endif
 return 0;
}

// return 1, if dll has unloaded
unsigned int newfunc_dllload_disablemodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor,mpxplay_module_entry_s *module)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 unsigned int i;
 struct dll_found_s *d;

 if(!dllfounds_entries)
  return 0;

 d=dll_founds;
 if(d){
  for(i=0;i<dllfounds_entries;i++,d++){
   mpxplay_module_entry_s **mmep=d->module_entries;
   if(!mmep)
    continue;
   do{
    if( ((*mmep)==module)
     || ( (*mmep)->moduletype_major==moduletype_major)
        && (  (moduletype_minor && (moduletype_minor==(*mmep)->moduletype_minor))
           || (modulename_minor && (pds_stricmp(modulename_minor,(*mmep)->modulename_minor)==0)) // !!! bad
        )
    ){
     dllload_disable_modulecallpoint(mmep);
     return (dllload_check_and_unload_dll(d));
    }
    mmep++;
   }while(*mmep);
  }
 }
#endif
 return 0;
}

/*void newfunc_dllload_unloadmodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor,mpxplay_module_entry_s *module)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 unsigned int i;
 struct dll_found_s *d;

 if(!dllfounds_entries)
  return;

 d=dll_founds;
 if(d){
  for(i=0;i<dllfounds_entries;i++,d++){
   mpxplay_module_entry_s **mmep=d->module_entries;
   if(!mmep)
    continue;
   do{
    if( ((*mmep)==module)
     || ( (*mmep)->moduletype_major==moduletype_major)
        && (  (moduletype_minor && (moduletype_minor==(*mmep)->moduletype_minor))
           || (modulename_minor && (pds_stricmp(modulename_minor,(*mmep)->modulename_minor)==0)) // !!! bad
        )
    ){
     dllload_unload_dll(d);
     break;
    }
    mmep++;
   }while(*mmep);
  }
 }
#endif
}*/

// keep module, unload others in a specified type
/*void newfunc_dllload_keepmodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor,mpxplay_module_entry_s *module)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 unsigned int i;
 struct dll_found_s *d;

 if(!dllfounds_entries)
  return;

 d=dll_founds;
 if(d){
  for(i=0;i<dllfounds_entries;i++,d++){
   mpxplay_module_entry_s **mmep=d->module_entries;
   if(!mmep)
    continue;
   do{
    if((*mmep)->moduletype_major==moduletype_major){
     if(   ((*mmep)!=module)
        && (!moduletype_minor || (moduletype_minor!=(*mmep)->moduletype_minor))
        && (!modulename_minor || (pds_stricmp(modulename_minor,(*mmep)->modulename_minor)!=0)) // !!! bad
     ){
      dllload_disable_modulecallpoint(mmep);
     }
    }
    mmep++;
   }while(*mmep);
   dllload_check_and_unload_dll(d);
  }
 }
#endif
}*/

#ifdef MPXPLAY_LINK_DLLLOAD

static struct mt_to_mn_s{
 unsigned int type;
 unsigned int ver;
 char *name;
}mt2mn[]={
{MPXPLAY_DLLMODULETYPE_CONTROL_CMDLINE   ,MPXPLAY_DLLMODULEVER_CONTROL_CMDLINE,"commandline"},
//{MPXPLAY_DLLMODULETYPE_CONTROL_KEYBOARD,MPXPLAY_DLLMODULEVER_CONTROL_KEYBOARD,"keyboard"},
{MPXPLAY_DLLMODULETYPE_CONTROL_SERIAL    ,MPXPLAY_DLLMODULEVER_CONTROL_SERIAL,"serialhandler"},
{MPXPLAY_DLLMODULETYPE_DISPLAY_VISUALPI  ,MPXPLAY_DLLMODULEVER_DISPLAY_VISUALPI,"visual-plugin"},
//{MPXPLAY_DLLMODULETYPE_DISPLAY_LCDLOWFUNC,MPXPLAY_DLLMODULEVER_DISPLAY_LCDLOWFUNC,"lcd-hardware"},
//{MPXPLAY_DLLMODULETYPE_DRIVEHAND         ,MPXPLAY_DLLMODULEVER_DRIVEHAND,"drive-handler"},
//{MPXPLAY_DLLMODULETYPE_FILEHAND_LOW      ,MPXPLAY_DLLMODULEVER_FILEHAND_LOW,"file-source"},
//{MPXPLAY_DLLMODULETYPE_FILEIN_PLAYLIST   ,MPXPLAY_DLLMODULEVER_FILEIN_PLAYLIST,"playlist"},
{MPXPLAY_DLLMODULETYPE_FILEIN_PARSER     ,MPXPLAY_DLLMODULEVER_FILEIN_PARSER,"infile-parser"},
{MPXPLAY_DLLMODULETYPE_FILEIN_CONTAINER  ,MPXPLAY_DLLMODULEVER_FILEIN_CONTAINER,"file-container"},
{MPXPLAY_DLLMODULETYPE_DECODER_AUDIO     ,MPXPLAY_DLLMODULEVER_DECODER_AUDIO,"audio decoder"},
//{MPXPLAY_DLLMODULETYPE_DECODER_VIDEO     ,MPXPLAY_DLLMODULEVER_DECODER_VIDEO,"video decoder"},
{MPXPLAY_DLLMODULETYPE_AUCARD            ,MPXPLAY_DLLMODULEVER_AUCARD,"audio output"},
{MPXPLAY_DLLMODULETYPE_AUMIXER           ,MPXPLAY_DLLMODULEVER_AUMIXER,"audio mixer"},
{MPXPLAY_DLLMODULETYPE_VIDEOOUT          ,MPXPLAY_DLLMODULEVER_VIDEOOUT,"video output"},
//{MPXPLAY_DLLMODULETYPE_VIDEOMIX          ,MPXPLAY_DLLMODULEVER_VIDEOMIX,"video mixer"},
{0,0,NULL}
};

static struct mt_to_mn_s *dllload_majortype_check(unsigned int type)
{
 struct mt_to_mn_s *m=&mt2mn[0];

 do{
  if(m->type==type)
   return m;
  m++;
 }while(m->type);

 return NULL;
}

#endif

void newfunc_dllload_list_dlls(void)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 unsigned int i,found=0;
 struct dll_found_s *d;
 char sout[128];

 d=dll_founds;
 if(d){
  for(i=0;i<dllfounds_entries;i++,d++){
   mpxplay_module_entry_s **mmep;
   if(!d->dllfilename)
    continue;
   snprintf(sout,sizeof(sout),"%s",pds_getfilename_from_fullname(d->dllfilename));
   pds_textdisplay_printf(sout);
   mmep=d->module_entries;
   if(!mmep)
    continue;
   found=1;
   do{
    struct mt_to_mn_s *mts=dllload_majortype_check((*mmep)->moduletype_major);
    snprintf(sout,sizeof(sout),"  module: type:%4.4X (%-14s)  sname:%-8s  sver:%4.4X %s",
           (*mmep)->moduletype_major,mts->name,(*mmep)->modulename_minor,
           (*mmep)->module_structure_version,(((*mmep)->module_structure_version!=mts->ver)? "not supp":""));
    pds_textdisplay_printf(sout);
    mmep++;
   }while(*mmep);
  }
 }
 if(!found)
  pds_textdisplay_printf("No DLLs found (in the directory of mpxplay.exe)!");
#else
 pds_textdisplay_printf("DLL loader routines are not linked in this Mpxplay version!");
#endif
}

void newfunc_dllload_closeall(void)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 unsigned int i;
 struct dll_found_s *d=dll_founds;

 if(d){
  for(i=0;i<dllfounds_entries;i++,d++)
   dllload_unload_and_clear_dllinfo(d);
  pds_free(dll_founds);
  dll_founds=NULL;
 }
#endif
}

#ifdef MPXPLAY_WIN32
HMODULE newfunc_dllload_winlib_load(char *libname)
{
 HMODULE dll_handle=NULL;
 unsigned int namelen=pds_strlen(libname);
 char path[MAX_PATHNAMELEN];

 if(namelen && (namelen<sizeof(path))){
  dll_handle=LoadLibrary(libname);
  if(!dll_handle){
   GetSystemDirectory(path,sizeof(path)-namelen);
   pds_strcat(path,"\\");
   pds_strcat(path,libname);
   dll_handle=LoadLibrary(path);
   if(!dll_handle){
    GetWindowsDirectory(path,sizeof(path)-namelen);
    pds_strcat(path,"\\");
    pds_strcat(path,libname);
    dll_handle=LoadLibrary(path);
   }
  }
 }
 return dll_handle;
}

void newfunc_dllload_winlib_close(HMODULE dllhandle)
{
 if(dllhandle)
  FreeLibrary(dllhandle);
}

unsigned int newfunc_dllload_winlib_getfuncs(HMODULE dllhandle,struct pds_win32dllcallfunc_t *funcs)
{
 while(funcs->funcname){
  funcs->funcptr=GetProcAddress(dllhandle,funcs->funcname);
  if(!funcs->funcptr)
   return 0;
  funcs++;
 }
 return 1;
}

//windows and win-DLLs use stack calling conventions, Mpxplay uses register-based...
mpxp_int32_t asm_stackcall_proc_arg0(void *proc);
mpxp_int32_t asm_stackcall_proc_arg1(void *proc,void *data);
mpxp_int32_t asm_stackcall_proc_arg2(void *proc,void *data1,void *data2);
mpxp_int32_t asm_stackcall_proc_arg3(void *proc,void *data1,void *data2,void *data3);

static mpxp_int32_t dllload_call_proc_stackbased_arg0(void *proc)
{
#pragma aux asm_stackcall_proc_arg0=\
 "call eax"\
 parm[eax] value[eax] modify[ebx ecx edx edi esi];
 return asm_stackcall_proc_arg0(proc);
}

static mpxp_int32_t dllload_call_proc_stackbased_arg1(void *proc,void *data)
{
#pragma aux asm_stackcall_proc_arg1=\
 "push edx"\
 "call eax"\
 "pop edx"\
 parm[eax][edx] value[eax] modify[ebx ecx edx edi esi];
 return asm_stackcall_proc_arg1(proc,data);
}

static mpxp_int32_t dllload_call_proc_stackbased_arg2(void *proc,void *data1,void *data2)
{
#pragma aux asm_stackcall_proc_arg2=\
 "push ebx"\
 "push edx"\
 "call eax"\
 "pop edx"\
 "pop ebx"\
 parm[eax][edx][ebx] value[eax] modify[ebx ecx edx edi esi];
 return asm_stackcall_proc_arg2(proc,data1,data2);
}

static mpxp_int32_t dllload_call_proc_stackbased_arg3(void *proc,void *data1,void *data2,void *data3)
{
#pragma aux asm_stackcall_proc_arg3=\
 "push ecx"\
 "push ebx"\
 "push edx"\
 "call eax"\
 "pop edx"\
 "pop ebx"\
 "pop ecx"\
 parm[eax][edx][ebx][ecx] value[eax] modify[ebx ecx edx edi esi];
 return asm_stackcall_proc_arg3(proc,data1,data2,data3);
}

long newfunc_dllload_winlib_callfunc(struct pds_win32dllcallfunc_t *func,void *data1,void *data2,void *data3)
{
 if(!func || !func->funcptr)
  return 0;
 switch(func->argnum){
  case 0:return dllload_call_proc_stackbased_arg0(func->funcptr);
  case 1:return dllload_call_proc_stackbased_arg1(func->funcptr,data1);
  case 2:return dllload_call_proc_stackbased_arg2(func->funcptr,data1,data2);
  default:return dllload_call_proc_stackbased_arg3(func->funcptr,data1,data2,data3);
 }
 return 0;
}

#endif
