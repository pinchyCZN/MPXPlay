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
//function:sort/order list

#include <string.h>
#include "newfunc\newfunc.h"
#include "playlist.h"
#include "display\display.h"

typedef int (*qsc_t)(const void *,const void *);

static void correct_pfilenums(struct playlist_side_info *psi,struct playlist_entry_info *pei_begin,struct playlist_entry_info *pei_end);

extern unsigned int sortcontrol;
extern char sortorder_string[256];
static char order_table[256];

static int check_order_str(char *strp1,char *strp2)
{
 char c1,c2;

 if(!strp1 || !strp1[0])
  if(strp2 && strp2[0])
   return -1;
  else
   return 0;
 if(!strp2 || !strp2[0])
  if(strp1 && strp1[0])
   return 1;
  else
   return 0;

 do{
  c1=order_table[*strp1];
  c2=order_table[*strp2];
  if(c1!=c2){
   if(c1<c2)
    return -1;
   else
    return 1;
  }
  strp1++;
  strp2++;
 }while(c1);
 return 0;
}


static int check_order_id3inf_title(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 char *s0,*s1;

 s0=pei0->id3info[I3I_TITLE];
 if(!s0)
  s0=pei0->id3info[I3I_ARTIST];
 if(!s0)
  s0=pds_getfilename_from_fullname(pei0->filename);

 s1=pei1->id3info[I3I_TITLE];
 if(!s1)
  s1=pei1->id3info[I3I_ARTIST];
 if(!s1)
  s1=pds_getfilename_from_fullname(pei1->filename);

 return check_order_str(s0,s1);
}

static int check_order_id3inf_artist(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 char *s0,*s1;

 s0=pei0->id3info[I3I_ARTIST];
 if(!s0)
  s0=pei0->id3info[I3I_TITLE];
 if(!s0)
  s0=pds_getfilename_from_fullname(pei0->filename);

 s1=pei1->id3info[I3I_ARTIST];
 if(!s1)
  s1=pei1->id3info[I3I_TITLE];
 if(!s1)
  s1=pds_getfilename_from_fullname(pei1->filename);

 return check_order_str(s0,s1);
}

static int check_order_id3inf_album(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 return check_order_str(pei0->id3info[I3I_ALBUM],pei1->id3info[I3I_ALBUM]);
}

static int check_order_id3inf_year(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 return check_order_str(pei0->id3info[I3I_YEAR],pei1->id3info[I3I_YEAR]);
}

static int check_order_id3inf_comment(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 return check_order_str(pei0->id3info[I3I_COMMENT],pei1->id3info[I3I_COMMENT]);
}

static int check_order_id3inf_genre(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 return check_order_str(pei0->id3info[I3I_GENRE],pei1->id3info[I3I_GENRE]);
}

static int check_order_id3inf_tracknum(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 long t1=pds_atol(pei0->id3info[I3I_TRACKNUM]);
 long t2=pds_atol(pei1->id3info[I3I_TRACKNUM]);
 if(t1<t2)
  return -1;
 if(t1==t2)
  return 0;
 return 1;
}

static int check_order_time(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 unsigned long t1=playlist_entry_get_timemsec(pei0);
 unsigned long t2=playlist_entry_get_timemsec(pei1);
 if(t1<t2)
  return -1;
 if(t1==t2)
  return 0;
 return 1;
}

static int check_order_filesize(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 unsigned int f1=pei0->filesize;
 unsigned int f2=pei1->filesize;
 if(f1<f2)
  return -1;
 if(f1==f2)
  return 0;
 return 1;
}

static int check_order_path(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 char path1[MAX_PATHNAMELEN],path2[MAX_PATHNAMELEN];
 pds_getpath_from_fullname(path1,pei0->filename);
 pds_getpath_from_fullname(path2,pei1->filename);
 return check_order_str(path1,path2);
}

static int check_order_filename(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 char *filename1,*filename2;
 filename1=pds_getfilename_from_fullname(pei0->filename);
 filename2=pds_getfilename_from_fullname(pei1->filename);
 return check_order_str(filename1,filename2);
}

static int check_order_fileext(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 char *ext1,*ext2;
 ext1=pds_strrchr(pei0->filename,'.');
 if(ext1)
  ext1++;
 ext2=pds_strrchr(pei1->filename,'.');
 if(ext2)
  ext2++;
 return check_order_str(ext1,ext2);
}

static int check_order_pathfile(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 return check_order_str(pei0->filename,pei1->filename);
}

static int check_order_filedate(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 mpxp_uint32_t date0=PDS_GETB_LE32(&pei0->filedate); // !!! sizeof(pds_fdate_t)==sizeof(mpxp_uint32_t)
 mpxp_uint32_t date1=PDS_GETB_LE32(&pei1->filedate);

 if(date0<date1)
  return -1;
 if(date0==date1)
  return 0;
 return 1;
}

static int check_order_dft(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 unsigned long head1=pei0->entrytype;
 unsigned long head2=pei1->entrytype;

 if(head1<DFTM_DFT)
  head1=DFT_AUDIOFILE;
 if(head2<DFTM_DFT)
  head2=DFT_AUDIOFILE;

 if(head1<head2)
  return -1;
 if(head1==head2)
  return 0;
 return 1;
}

static int check_order_index_pst(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 if((pei0->infobits&PEIF_INDEXED) && (pei1->infobits&PEIF_INDEXED) && (pds_stricmp(pei0->filename,pei1->filename)==0)){
  if(pei0->pstime<pei1->pstime)
   return -1;
  if(pei0->pstime>pei1->pstime)
   return 1;
 }
 return 0;
}

static check_order_func_t *check_order_funcs[]={
 &check_order_id3inf_title,
 &check_order_id3inf_artist,
 &check_order_id3inf_album,
 &check_order_id3inf_year,
 &check_order_id3inf_comment,
 &check_order_id3inf_genre,
 &check_order_id3inf_tracknum,
 &check_order_time,
 &check_order_filesize,
 &check_order_path,
 &check_order_filename,
 &check_order_pathfile,
 &check_order_filedate,
 &check_order_dft,
 &check_order_index_pst,
 NULL
};

static check_order_func_t *check_order_func_player[]={
 &check_order_id3inf_title,
 &check_order_id3inf_artist,
 &check_order_id3inf_album,
 &check_order_id3inf_year,
 &check_order_id3inf_comment,
 &check_order_id3inf_genre,
 &check_order_id3inf_tracknum,
 &check_order_time,
 &check_order_filesize,
 &check_order_path,
 &check_order_filename,
 &check_order_pathfile,
 &check_order_filedate,
 &check_order_dft,
 &check_order_index_pst,
 NULL
};

static check_order_func_t *check_order_func_commander[]={
 &check_order_id3inf_title,
 &check_order_id3inf_artist,
 &check_order_id3inf_album,
 &check_order_id3inf_year,
 &check_order_fileext,
 &check_order_id3inf_genre,
 &check_order_id3inf_tracknum,
 &check_order_time,
 &check_order_filesize,
 &check_order_path,
 &check_order_filename,
 &check_order_pathfile,
 &check_order_filedate,
 &check_order_dft,
 &check_order_index_pst,
 NULL
};

static check_order_func_t **side_orderfuncp;

#if defined(__WATCOMC__) && (PLAYLIST_MAX_ORDERKEYS==5) && (SORTC_DESCENDING==1)

int asm_coqs(const struct playlist_entry_info *pei0,const struct playlist_entry_info *pei1);

#pragma aux asm_coqs=\
 "mov ebx,eax"\
 "mov ecx,edx"\
 "mov edi,5"\
 "mov esi,dword ptr side_orderfuncp"\
 "back1:"\
  "cmp dword ptr [esi],0"\
  "je endnull"\
  "mov eax,ebx"\
  "mov edx,ecx"\
  "call dword ptr [esi]"\
  "test eax,eax"\
  "jz cont1"\
   "test byte ptr sortcontrol,1"\
   "jz ascending"\
    "neg eax"\
   "ascending:"\
   "jmp end"\
  "cont1:"\
  "add esi,4"\
  "dec edi"\
 "jnz back1"\
 "endnull:xor eax,eax"\
 "end:"\
 parm [eax][edx] value[eax] modify[eax ebx ecx edx edi esi];

static int check_order_func_qs(const struct playlist_entry_info *pei0,const struct playlist_entry_info *pei1)
{
 return asm_coqs(pei0,pei1);
}

#else

static int check_order_func_qs(const struct playlist_entry_info *pei0,const struct playlist_entry_info *pei1)
{
 unsigned int keylevel=0;
 do{
  int result;
  if(!(*(side_orderfuncp+keylevel)))
   break;
  result=(*(side_orderfuncp+keylevel))((struct playlist_entry_info *)pei0,(struct playlist_entry_info *)pei1);
  if(result!=0){
   if(sortcontrol&SORTC_DESCENDING)
    result=-result;
   return result;
  }
 }while(++keylevel<PLAYLIST_MAX_ORDERKEYS);
 return 0;
}

#endif

static int check_order_func_one(struct playlist_side_info *psi,struct playlist_entry_info *pei0,struct playlist_entry_info *pei1)
{
 unsigned int keylevel=0;
 do{
  int result;
  if(!psi->id3ordertype[keylevel])
   break;
  result=((psi->id3ordertype[keylevel])(pei0,pei1));
  if(result!=0){
   if(sortcontrol&SORTC_DESCENDING)
    result=-result;
   return result;
  }
 }while(++keylevel<PLAYLIST_MAX_ORDERKEYS);
 return 0;
}

// order one (new or modified) entry in a sorted (!) playlist
/*static struct playlist_entry_info *sortlist_order_one_block_new(struct playlist_side_info *psi,
                                                            struct playlist_entry_info *pei_src,
                                                            struct playlist_entry_info *firstentry,
                                                            struct playlist_entry_info *lastentry)
{
 struct playlist_entry_info *pei_bottom,*pei_top,*pei_pos,*pei_lastgoodpos=NULL,*pei_start;
 unsigned int center;
 int result;
 struct playlist_entry_info pei_tmp;

 //fprintf(stdout,"order one: top:%d  ir0:%d ir1:%d ir2:%d\n",lastentry-firstentry,
 // ((char *)psi->id3ordertype[0]-(char *)check_order_funcs[0])/4,((char *)psi->id3ordertype[1]-(char *)check_order_funcs[0])/4,((char *)psi->id3ordertype[2]-(char *)check_order_funcs[0])/4);
 if(!psi->id3ordertype[0])
  return pei_src;

 pei_bottom=firstentry;
 pei_top=lastentry;
 if(pei_top<=pei_bottom)
  return pei_src;

 result=1;

 center=(pei_top-pei_bottom);
 center>>=1;

 //fprintf(stdout,"center:%d \n",center);

 if(center>2){
  do{
   pei_pos=pei_bottom+center;
   if(pei_pos==pei_src)
    pei_pos++;
   pei_start=pei_pos;
   while((pei_pos<=pei_top) && !funcbit_test(pei_pos->infobits,PEIF_SORTED)) // search in already sorted field only
   //while((pei_pos<=pei_top) && (pei_pos->entrytype==DFT_NOTCHECKED)) // search in already sorted field only
    pei_pos++;
   if(pei_pos>pei_top)
    result=-1;
   else{
    result=check_order_func_one(psi,pei_src,pei_pos);
    if(result==0)
     break;
    pei_lastgoodpos=pei_pos;
   }
   //fprintf(stdout,"s1 pos:%d res:%d center:%d bottom:%d top:%d\n",pei_pos-firstentry,result,center,pei_bottom-firstentry,pei_top-firstentry);
   if(result<0)
    pei_top=pei_start;
   else
    pei_bottom=pei_pos;
   center=(pei_top-pei_bottom);
   center>>=1;
  }while(center>2);
 }

 if(center<=2){
  pei_pos=pei_bottom;
  do{
   while((pei_pos<=pei_top) && !funcbit_test(pei_pos->infobits,PEIF_SORTED))
   //while((pei_pos<=pei_top) && (pei_pos->entrytype==DFT_NOTCHECKED)) // search in already sorted field only
    pei_pos++;
   if(pei_pos>pei_top)
    break;
   result=check_order_func_one(psi,pei_src,pei_pos);
   //fprintf(stdout,"s2 pos:%d res:%d \n",pei_pos-firstentry,result);
   if(result<0){
    if(pei_pos<pei_src)
     pei_lastgoodpos=pei_pos;
    break;
   }
   pei_lastgoodpos=pei_pos;
   pei_pos++;
  }while(pei_pos<=pei_top);
 }

 if(!pei_lastgoodpos)
  return pei_src;
 pei_pos=pei_lastgoodpos;

 if(pei_pos>lastentry)
  pei_pos=lastentry;

 //fprintf(stdout,"copy src:%d pos:%d\n",pei_src-firstentry,pei_pos-firstentry);
 if(pei_pos!=pei_src){
  pds_memcpy((char *)&pei_tmp,pei_src,sizeof(struct playlist_entry_info));
  if(pei_src>pei_pos)
   pds_qmemcpyr((pei_pos+1),pei_pos,(pei_src-pei_pos)*sizeof(struct playlist_entry_info)/4);
  else
   pds_qmemcpy(pei_src,(pei_src+1),(pei_pos-pei_src)*sizeof(struct playlist_entry_info)/4);
  pds_memcpy(pei_pos,(char *)&pei_tmp,sizeof(struct playlist_entry_info));
  correct_pfilenums(psi,pei_src,pei_pos);
 }
 return pei_pos;
}*/

static struct playlist_entry_info *sortlist_order_one_block(struct playlist_side_info *psi,
                                                            struct playlist_entry_info *pei_src,
                                                            struct playlist_entry_info *firstentry,
                                                            struct playlist_entry_info *lastentry)
{
 struct playlist_entry_info *pei_bottom,*pei_top,*pei_pos,*pei_lastgoodpos;
 unsigned int center;
 int result;
 struct playlist_entry_info pei_tmp;

 if(!psi->id3ordertype[0])
  return pei_src;

 pei_bottom=firstentry;
 pei_top=lastentry;
 if(pei_top<=pei_bottom)
  return pei_src;

 result=1;

 center=(pei_top-pei_bottom);
 center>>=1;

 if(center>2){
  do{
   pei_pos=pei_bottom+center;
   if(pei_pos==pei_src)
    pei_pos++;
   result=check_order_func_one(psi,pei_src,pei_pos);
   if(result==0)
    break;
   if(result<0)
    pei_top=pei_pos;
   else
    pei_bottom=pei_pos;
   center=(pei_top-pei_bottom);
   center>>=1;
  }while(center>2);
 }

 if(center<=2){
  pei_pos=pei_lastgoodpos=pei_bottom;
  do{
   result=check_order_func_one(psi,pei_src,pei_pos);
   if(result<0){
    if(pei_pos<pei_src)
     pei_lastgoodpos=pei_pos;
    break;
   }
   pei_lastgoodpos=pei_pos;
   pei_pos++;
  }while(pei_pos<=pei_top);
  pei_pos=pei_lastgoodpos;
 }

 if(pei_pos>lastentry)
  pei_pos=lastentry;

 if(pei_pos!=pei_src){
  pds_memcpy((char *)&pei_tmp,pei_src,sizeof(struct playlist_entry_info));
  if(pei_src>pei_pos)
   pds_qmemcpyr((pei_pos+1),pei_pos,(pei_src-pei_pos)*sizeof(struct playlist_entry_info)/4);
  else
   pds_qmemcpy(pei_src,(pei_src+1),(pei_pos-pei_src)*sizeof(struct playlist_entry_info)/4);
  pds_memcpy(pei_pos,(char *)&pei_tmp,sizeof(struct playlist_entry_info));
  correct_pfilenums(psi,pei_src,pei_pos);
 }
 return pei_pos;
}

static void correct_pfilenums(struct playlist_side_info *psi,struct playlist_entry_info *pei_begin,struct playlist_entry_info *pei_end)
{
 struct mainvars *mvp;

 if(pei_begin==pei_end)
  return;

 mvp=psi->mvp;
 if(psi==mvp->psip){
  if(mvp->aktfilenum){
   if(mvp->aktfilenum==pei_begin){
    mvp->aktfilenum=pei_end;
   }else{
    if(pei_begin<pei_end){
     if(mvp->aktfilenum>pei_begin && mvp->aktfilenum<=pei_end)
      mvp->aktfilenum--;
    }else{
     if(mvp->aktfilenum>=pei_end && mvp->aktfilenum<pei_begin)
      mvp->aktfilenum++;
    }
   }
  }
  if(mvp->newfilenum){
   if(mvp->newfilenum==pei_begin){
    mvp->newfilenum=pei_end;
   }else{
    if(pei_begin<pei_end){
     if(mvp->newfilenum>pei_begin && mvp->newfilenum<=pei_end)
      mvp->newfilenum--;
    }else{
     if(mvp->newfilenum>=pei_end && mvp->newfilenum<pei_begin)
      mvp->newfilenum++;
    }
   }
  }
  if(mvp->newsong){
   if(mvp->newsong==pei_begin){
    mvp->newsong=pei_end;
   }else{
    if(pei_begin<pei_end){
     if(mvp->newsong>pei_begin && mvp->newsong<=pei_end)
      mvp->newsong--;
    }else{
     if(mvp->newsong>=pei_end && mvp->newsong<pei_begin)
      mvp->newsong++;
    }
   }
  }
 }
 if(psi->editsidetype&PLT_SORTC_MAGNETHIGHLINE){
  if(psi->editorhighline==pei_begin){
   playlist_editorhighline_set_nocenter(psi,pei_end);
  }else{
   if(pei_begin<pei_end){
    if(psi->editorhighline>pei_begin && psi->editorhighline<=pei_end)
     playlist_editorhighline_seek(psi,-1,SEEK_CUR);
   }else{
    if(psi->editorhighline>=pei_end && psi->editorhighline<pei_begin)
     playlist_editorhighline_seek(psi,+1,SEEK_CUR);
   }
  }
 }
 if((pei_end->entrytype&DFTM_DFT) && (pei_begin>=psi->firstsong) && (pei_end<=psi->firstsong))
  psi->firstsong++;
 if(pei_end<pei_begin){
  struct playlist_entry_info *tmp;
  tmp=pei_begin;
  pei_begin=pei_end;
  pei_end=tmp;
 }
 playlist_randlist_correctq(psi,pei_begin,pei_end);
 playlist_peimyself_reset(psi,pei_begin,pei_end);
}

//--------------------------------------------------------------------------
static void ordertypes_save(struct playlist_side_info *psi,check_order_func_t *ot_save[PLAYLIST_MAX_ORDERKEYS])
{
 pds_memcpy((void *)&ot_save[0],(void *)&psi->id3ordertype[0],PLAYLIST_MAX_ORDERKEYS*sizeof(check_order_func_t *));
}

static void ordertypes_restore(struct playlist_side_info *psi,check_order_func_t *ot_save[PLAYLIST_MAX_ORDERKEYS])
{
 pds_memcpy((void *)&psi->id3ordertype[0],(void *)&ot_save[0],PLAYLIST_MAX_ORDERKEYS*sizeof(check_order_func_t *));
}

static void ordertype_insert(struct playlist_side_info *psi,unsigned int index,check_order_func_t *func)
{
 if(index<(PLAYLIST_MAX_ORDERKEYS-1))
  pds_qmemcpyr((void *)&psi->id3ordertype[index+1],(void *)&psi->id3ordertype[index],(PLAYLIST_MAX_ORDERKEYS-index-1)*sizeof(check_order_func_t *)/4);
 psi->id3ordertype[index]=func;
}

static void ordertype_append(struct playlist_side_info *psi,check_order_func_t *func)
{
 unsigned int i;
 for(i=0;i<PLAYLIST_MAX_ORDERKEYS;i++){
  if(psi->id3ordertype[i]==func) // !!! we don't put the same key twice
   break;
  if(!psi->id3ordertype[i]){
   psi->id3ordertype[i]=func;
   break;
  }
 }
}

//------------------------------------------------------------------------
static void build_ordertable(void)
{
 char *ord=&sortorder_string[0];
 unsigned int i,pos;
 char tmp[256];

 for(i=0;i<256;i++)
  tmp[i]=i;

 pos=0;
 do{
  unsigned int c=*ord;
  if(!c)
   break;
  if(c>32 && c!='\"'){
   if(!pos){
    pos=c;
    pos++;
   }else{
    unsigned int from=33; // bellow 32 are control chars
    do{
     if(tmp[from]==c){
      if(from<pos){
       memmove(&tmp[from],&tmp[from+1],pos-from);
       tmp[pos-1]=c;
      }else{
       if(from>pos)
        memmove(&tmp[pos+1],&tmp[pos],from-pos);
       tmp[pos]=c;
       pos++;
      }
      break;
     }
     from++;
    }while(from<256);
   }
  }
  ord++;
 }while(pos<256);

 for(i=0;i<256;i++)
  order_table[tmp[i]]=i;

 for(i='a';i<='z';i++)
  order_table[i]=order_table[i-('a'-'A')]; // lower to uppercase
}

static unsigned int sortlist_initialized;

void playlist_sortlist_init(struct mainvars *mvp)
{
 struct playlist_side_info *psi0=mvp->psi0,*psi;
 unsigned int s,k,type;

 if(sortlist_initialized)
  return;

 for(k=0;k<PLAYLIST_MAX_ORDERKEYS;k++){
  psi=psi0;
  type=*((unsigned int *)&psi->id3ordertype[k]);
  if(type>ID3ORDER_DISABLED)
   type=ID3ORDER_DISABLED;
  for(s=0;s<PLAYLIST_MAX_SIDES;s++,psi++)
   psi->id3ordertype[k]=check_order_funcs[type]; // convert index-value to func-pointer
 }

 psi=psi0;   // !!! auto order keys
 for(s=0;s<PLAYLIST_MAX_SIDES;s++,psi++){
  if(psi->id3ordertype[0]){
   ordertype_append(psi,check_order_funcs[ID3ORDER_PATHFILE]);
   ordertype_append(psi,check_order_funcs[ID3ORDER_INDEX]);
  }
 }

 build_ordertable();
 sortlist_initialized=1;
}

void playlist_sortlist_selectfuncs(unsigned int select)
{
 switch(select){
  case PLAYLIST_SORTLIST_SELECTFUNCS_PLAYER:pds_memcpy(check_order_funcs,check_order_func_player,sizeof(check_order_funcs));break;
  case PLAYLIST_SORTLIST_SELECTFUNCS_COMMANDER:pds_memcpy(check_order_funcs,check_order_func_commander,sizeof(check_order_funcs));break;
 }
}

void playlist_sortlist_selectorder(struct playlist_side_info *psi,unsigned int key,unsigned int type)
{
 if(type<ID3ORDER_DISABLED){
  // !!! further development
  //if((key==0) && (psi->id3ordertype[key]==check_order_funcs[type]))
  // funcbit_inverse(sortcontrol,SORTC_DESCENDING);
  //else
   psi->id3ordertype[key]=check_order_funcs[type];
 }else{
  psi->id3ordertype[key]=NULL;
 }
}

void playlist_sortlist_clearorder(struct playlist_side_info *psi)
{
 pds_memset(&psi->id3ordertype[0],0,sizeof(psi->id3ordertype));
}

mpxp_uint32_t playlist_sortlist_get_orderkeys_in_hexa(struct playlist_side_info *psi)
{
 mpxp_uint32_t i,value=0;
 for(i=0;i<PLAYLIST_MAX_ORDERKEYS;i++){
  mpxp_uint32_t type=ID3ORDER_SAVEMASK;
  if(psi->id3ordertype[i])
   for(type=0;type<ID3ORDER_DISABLED;type++)
    if(psi->id3ordertype[i]==check_order_funcs[type])
     break;
  if(type>=ID3ORDER_DISABLED)
   type=ID3ORDER_SAVEMASK;
  value|=type<<(i*4);
 }
 return value;
}

void playlist_sortlist_set_orderkeys_from_hexa(struct playlist_side_info *psi,mpxp_uint32_t value)
{
 mpxp_uint32_t i;
 playlist_sortlist_init(psi->mvp);
 if(psi->id3ordertype[0]) // ??? set by -io
  return;
 for(i=0;i<PLAYLIST_MAX_ORDERKEYS;i++){
  mpxp_uint32_t type=(value>>(i*4))&ID3ORDER_SAVEMASK;
  if(type<ID3ORDER_DISABLED)
   psi->id3ordertype[i]=check_order_funcs[type];
 }
}

//------------------------------------------------------------------------
static void ordertypes_set_dft(struct playlist_side_info *psi)
{
 if(!psi->id3ordertype[0]){
  psi->id3ordertype[0]=check_order_funcs[ID3ORDER_DFT];
  psi->id3ordertype[1]=check_order_funcs[ID3ORDER_PATHFILE];
  psi->id3ordertype[2]=check_order_funcs[ID3ORDER_INDEX];
  psi->id3ordertype[3]=NULL;
 }else{
  ordertype_insert(psi,0,check_order_funcs[ID3ORDER_DFT]);
  ordertype_append(psi,check_order_funcs[ID3ORDER_PATHFILE]);
  ordertype_append(psi,check_order_funcs[ID3ORDER_INDEX]);
 }
}

// sort at chkfile
struct playlist_entry_info *playlist_order_entry_block(struct playlist_side_info *psi,
                                                       struct playlist_entry_info *pei_src,
                                                       struct playlist_entry_info *firstentry,
                                                       struct playlist_entry_info *lastentry)
{
 struct playlist_entry_info *pei_pos=NULL;
 if((psi->lastentry>psi->firstentry) && !funcbit_test(pei_src->infobits,PEIF_SORTED)
  && (psi->id3ordertype[0] || ((psi->editsidetype&PLT_DIRECTORY) && !psi->sublistlevel && !(psi->editsidetype&PLT_SORTC_MANUALCOPY)))
 ){
  struct mainvars *mvp=psi->mvp;
  unsigned int highakt=(psi->editorhighline==mvp->aktfilenum);
  check_order_func_t *ot_save[PLAYLIST_MAX_ORDERKEYS];

  if(firstentry<psi->firstentry)
   firstentry=psi->firstentry;
  if(lastentry>psi->lastentry)
   lastentry=psi->lastentry;
  if(lastentry<=firstentry)
   return pei_pos;

  ordertypes_save(psi,ot_save);
  ordertypes_set_dft(psi);
  pei_pos=sortlist_order_one_block(psi,pei_src,firstentry,lastentry);
  ordertypes_restore(psi,ot_save);

  if(highakt)
   playlist_editorhighline_set(psi,mvp->aktfilenum);
  if(pei_pos==pei_src)                            //
   funcbit_enable(pei_pos->infobits,PEIF_SORTED); // do not sort again sorted entry
 }
 return pei_pos;
}

// sort at rename (diskfile)
struct playlist_entry_info *playlist_order_entry(struct playlist_side_info *psi,struct playlist_entry_info *pei_src)
{
 return playlist_order_entry_block(psi,pei_src,psi->firstentry,psi->lastentry);
}

void playlist_order_block(struct playlist_side_info *psi,
                          struct playlist_entry_info *firstentry,
                          struct playlist_entry_info *lastentry)
{
 struct mainvars *mvp;
 unsigned int highakt;

 if(!psi->id3ordertype[0])
  return;

 if(firstentry<psi->firstentry)
  firstentry=psi->firstentry;
 if(lastentry>psi->lastentry)
  lastentry=psi->lastentry;
 if(lastentry<=firstentry)
  return;

 mvp=psi->mvp;
 highakt=(psi->editorhighline==mvp->aktfilenum);

 side_orderfuncp=&psi->id3ordertype[0];

 qsort((void *)(firstentry),(lastentry-firstentry+1),sizeof(playlist_entry_info),(qsc_t)check_order_func_qs);

 if((mvp->aktfilenum>=firstentry) && (mvp->aktfilenum<=lastentry))
  mvp->aktfilenum=playlist_peimyself_search(psi,mvp->aktfilenum);
 if((mvp->newfilenum>=firstentry) && (mvp->newfilenum<=lastentry))
  mvp->newfilenum=playlist_peimyself_search(psi,mvp->newfilenum);
 if((psi->editorhighline>=firstentry) && (psi->editorhighline<=lastentry)){
  if(psi->editsidetype&PLT_SORTC_MAGNETHIGHLINE)
   playlist_editorhighline_set_nocenter(psi,playlist_peimyself_search(psi,psi->editorhighline));
  else if(highakt)
   playlist_editorhighline_set_nocenter(psi,mvp->aktfilenum);
 }

 playlist_randlist_correctq(psi,firstentry,lastentry);
 playlist_peimyself_reset(psi,firstentry,lastentry);
}

static void playlist_order_side_block(struct playlist_side_info *psi,
                                      struct playlist_entry_info *firstentry,
                                      struct playlist_entry_info *lastentry)
{
 if(lastentry>firstentry){
  playlist_order_block(psi,firstentry,lastentry);

  if(psi->chkfilenum_curr)
   playlist_chkfile_start_norm(psi,0);
  else // ??? PLI_DISPLOAD
   display_editorside_reset(psi);
 }
}

// manual sort or sort at extended playlists
void playlist_order_side(struct playlist_side_info *psi)
{
 if(psi->id3ordertype[0]){
  check_order_func_t *ot_save[PLAYLIST_MAX_ORDERKEYS];
  funcbit_enable(psi->editsidetype,PLT_SORTC_MAGNETHIGHLINE); // !!!
  ordertypes_save(psi,ot_save);
  ordertypes_set_dft(psi);
  playlist_order_side_block(psi,psi->firstsong,psi->lastentry);
  ordertypes_restore(psi,ot_save);
  funcbit_disable(psi->editsidetype,PLT_SORTC_MAGNETHIGHLINE); // !!!
 }
}

static void playlist_order_dft_block(struct playlist_side_info *psi,
                                     struct playlist_entry_info *firstentry,
                                     struct playlist_entry_info *lastentry)
{
 check_order_func_t *ot_save[PLAYLIST_MAX_ORDERKEYS];

 ordertypes_save(psi,ot_save);
 psi->id3ordertype[0]=NULL; // to set dft always
 ordertypes_set_dft(psi);
 playlist_order_side_block(psi,firstentry,lastentry);
 ordertypes_restore(psi,ot_save);
}

// sort at directory browser
void playlist_order_dft(struct playlist_side_info *psi)
{
 playlist_order_dft_block(psi,psi->firstentry,psi->lastentry);
}

// sort (block of files) at dir-scan
void playlist_order_filenames_block(struct playlist_side_info *psi,
                                    struct playlist_entry_info *firstentry,
                                    struct playlist_entry_info *lastentry)
{
 if(!psi->id3ordertype[0])
  playlist_order_dft_block(psi,firstentry,lastentry);
}

// sort at (manually) disabled sort
void playlist_order_filenames(struct playlist_side_info *psi)
{
 playlist_order_filenames_block(psi,psi->firstsong,psi->lastentry);
}

void playlist_swap_entries(struct playlist_side_info *psi,struct playlist_entry_info *e1,struct playlist_entry_info *e2)
{
 struct mainvars *mvp;

 pds_memxch((char *)e1,(char *)e2,sizeof(playlist_entry_info));

 mvp=psi->mvp;
 if(psi==mvp->psip){
  if(mvp->aktfilenum==e1)
   mvp->aktfilenum=e2;
  else
   if(mvp->aktfilenum==e2)
    mvp->aktfilenum=e1;
  if(mvp->newfilenum==e1)
   mvp->newfilenum=e2;
  else
   if(mvp->newfilenum==e2)
    mvp->newfilenum=e1;
  playlist_randlist_xchq(e1,e2);
 }
 if(psi->chkfilenum_curr){
  struct playlist_entry_info *lower=min(e1,e2);
  struct playlist_entry_info *higher=max(e1,e2);
  if(psi->chkfilenum_curr>=lower && psi->chkfilenum_curr<=higher)
   psi->chkfilenum_curr=lower;
  if(psi->chkfilenum_curr<psi->chkfilenum_begin)
   psi->chkfilenum_begin=psi->chkfilenum_curr;
 }
 e1->myself=e1;
 e2->myself=e2;
}

unsigned int playlist_sortlist_is_preordered_type(check_order_func_t *ot)
{
 if( (ot==&check_order_filesize)
  || (ot==&check_order_path)
  || (ot==&check_order_filename)
  || (ot==&check_order_pathfile)
  || (ot==&check_order_filedate)
 ){
  return 1;
 }
 return 0;
}
