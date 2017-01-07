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
//function: win32 aspi and ntscsi based audio CD handling
//based on CDex (http://cdexos.sourceforge.net)

#include <newfunc/newfunc.h>

#ifdef MPXPLAY_WIN32

#include "cd_drive.h"
#include "cd_aspi.h"

#define WINASPI_SCSI_TIMEOUT 10000

#define WDIS_DRIVEFLAG_ATAPI        1
#define WDIS_DRIVEFLAG_MMC          2
#define WDIS_DRIVEFLAG_BIGENDIAN    4
#define WDIS_DRIVEFLAG_SWAPCHANS    8
#define WDIS_DRIVEFLAG_DOORCLOSED  16

//#define WDIS_READMETHOD_MMC          0

//typedef mpxp_int32_t (*call_func_nodata)(void);
//typedef mpxp_int32_t (*call_func_withdata)(void *);

typedef struct winaspi_drive_info_s{
 unsigned int drivenum;
 void *lowlevel_drive_data; // winaspi or ntscsi
 struct cddrive_disk_info_s *diskinfos;
 struct cddrive_track_info_s trackinfo;
 unsigned int adapter_id,target_id,lun_id,dev_type;
 unsigned int drive_flags;
 unsigned int drive_type;
 unsigned int read_mode;
 unsigned int setspeed_mode;
 unsigned int endian_mode;
 unsigned int enable_mode;
 unsigned int density;
 CDSTATUSINFO cd_statusinfo;
 mpxp_uint8_t toc_data[CB_CDROMSECTOR];
}winaspi_drive_info_s;

typedef void (*aspi_deinit_func_t)(struct winaspi_drive_info_s *wdis);
typedef mpxp_uint32_t (*aspi_get_support_info_t)(struct winaspi_drive_info_s *wdis);
typedef mpxp_uint32_t (*aspi_send_command_t)(struct winaspi_drive_info_s *wdis,void *);

static HMODULE winaspi_dll_handle;
static aspi_get_support_info_t winaspi_get_support_info_dllfunc;
static aspi_send_command_t winaspi_send_command_dllfunc;
static unsigned int winaspi_num_initialized_drivers,winaspi_num_ha;
static unsigned int winaspi_adapter_id,winaspi_target_id,winaspi_lun_id;

//aspi32
static unsigned int WINASPI_init(struct winaspi_drive_info_s *wdis);
static void WINASPI_deinit(struct winaspi_drive_info_s *wdis);
static mpxp_uint32_t WINASPI_send_command(struct winaspi_drive_info_s *wdis,void *cmd);
//ntscsi
static unsigned int NTSCSI_init(struct winaspi_drive_info_s *wdis);
static void NTSCSI_deinit(struct winaspi_drive_info_s *wdis);
static mpxp_uint32_t NTSCSI_send_command(struct winaspi_drive_info_s *wdis,void *cmd);
//select
static aspi_deinit_func_t      CDASPI32_deinit_func;
static aspi_send_command_t     CDASPI32_send_command_func;

static void WINASPICD_drive_unmount(void *wdip);

static unsigned int WINASPICD_drive_check(unsigned int drivenum)
{
 if(pds_chkdrive(drivenum)!=DRIVE_CDROM)
  return CDDRIVE_RETCODE_FAILED;

 return CDDRIVE_RETCODE_OK;
}

static void *WINASPICD_drive_mount(unsigned int drivenum)
{
 struct winaspi_drive_info_s *wdis;

 wdis=(struct winaspi_drive_info_s *)calloc(1,sizeof(*wdis));
 if(!wdis)
  return wdis;
 wdis->drivenum=drivenum;
 funcbit_enable(wdis->drive_flags,WDIS_DRIVEFLAG_ATAPI);
 //funcbit_enable(wdis->drive_flags,WDIS_DRIVEFLAG_BIGENDIAN);
 //funcbit_enable(wids->drive_flags,WDIS_DRIVEFLAG_SWAPCHANS);
 funcbit_enable(wdis->drive_flags,WDIS_DRIVEFLAG_DOORCLOSED);
 wdis->drive_type=CDASPI_DRIVETYPE_GENERIC;
 wdis->read_mode=CDASPI_READMODE_MMC;
 wdis->setspeed_mode=CDASPI_SPEEDMODE_MMC;

 if(!NTSCSI_init(wdis))
  if(!WINASPI_init(wdis))
   goto err_out_mount;

 return wdis;

err_out_mount:

 WINASPICD_drive_unmount(wdis);
 return NULL;
}

static void WINASPICD_drive_unmount(void *wdip)
{
 struct winaspi_drive_info_s *wdis=wdip;
 if(CDASPI32_deinit_func)
  CDASPI32_deinit_func(wdis); // no wdis checking here due to the static-local variables checking in low level functions
 if(wdis)
  free(wdis);
}

//-------------------------------------------------------------------------

static unsigned int cdaspi_scsi_abort(struct winaspi_drive_info_s *wdis,srb_exec_scsicmd_s *sp)
{
 SRB_Abort s;

 pds_memset(&s,0,sizeof(s));

 s.SRB_Cmd     = SC_ABORT_SRB;	    // ASPI command code = SC_ABORT_SRB
 s.SRB_HaID    = wdis->adapter_id;  // ASPI host adapter number
 s.SRB_ToAbort = &sp;	            // sp

 CDASPI32_send_command_func(wdis,&s);

 if(s.SRB_Status != SS_COMP)
  return CDDRIVE_RETCODE_FAILED;

 return CDDRIVE_RETCODE_OK;
}

static unsigned int cdaspi_scsi_issuecmd(struct winaspi_drive_info_s *wdis,unsigned int bFlags,mpxp_uint8_t *lpcbData,int ncbLen,mpxp_uint8_t *lpBuffer,int nBufLen)
{
 HANDLE	hEvent=NULL;
 CDSTATUSINFO *cds=&wdis->cd_statusinfo;
 srb_exec_scsicmd_s mySrb;

 if(ncbLen>sizeof(mySrb.CDBByte))
  return SS_ABORTED;

 pds_memset(&mySrb,0x00,sizeof(mySrb));
 cds->sk=0x0B;
 cds->asc=cds->ascq=cds->ha_stat=cds->target_stat=0;

 if((hEvent = CreateEvent(NULL,TRUE,FALSE,NULL)) == NULL)
  return SS_ABORTED;

 mySrb.SRB_Cmd   = SC_EXEC_SCSI_CMD;
 mySrb.SRB_HaId  = wdis->adapter_id;
 mySrb.SRB_Flags = bFlags;
 mySrb.SRB_Flags|= SRB_EVENT_NOTIFY;
 mySrb.SRB_Target= wdis->target_id;
 mySrb.SRB_Lun   = wdis->lun_id;
 mySrb.SRB_SenseLen=SENSE_LEN;
 mySrb.SRB_CDBLen=ncbLen;
 mySrb.SRB_BufLen=nBufLen;
 mySrb.SRB_BufPointer=lpBuffer;
 mySrb.SRB_PostProc= (POSTPROCFUNC)hEvent;

 if(lpcbData!=NULL)
  memcpy(&mySrb.CDBByte,lpcbData,ncbLen);

 ResetEvent(hEvent);

 if(CDASPI32_send_command_func(wdis,&mySrb) == SS_PENDING){
  if(WaitForSingleObject(hEvent, WINASPI_SCSI_TIMEOUT)==WAIT_TIMEOUT)
   ResetEvent(hEvent);

  if(mySrb.SRB_Status == SS_PENDING){
   cdaspi_scsi_abort(wdis,&mySrb);
   CloseHandle(hEvent);
   return SS_ABORTED;
  }
 }

 cds->sk = mySrb.SenseArea[  2 ] & 0x0F;
 cds->asc = mySrb.SenseArea[ 12 ];
 cds->ascq  = mySrb.SenseArea[ 13 ];
 cds->ha_stat = mySrb.SRB_HaStat;
 cds->target_stat = mySrb.SRB_TargStat;

 CloseHandle(hEvent);

 return mySrb.SRB_Status;
}

static unsigned int cdaspi_get_media_status(struct winaspi_drive_info_s *wdis)
{
 unsigned int status=CDASPI_CDMEDIA_NOT_PRESENT;
 mpxp_uint8_t cmd[6];

 pds_memset(cmd,0,sizeof(cmd));

 cdaspi_scsi_issuecmd(wdis,0x00,cmd,sizeof(cmd),NULL,0);

 switch(wdis->cd_statusinfo.sk){
  case 0: status = CDASPI_CDMEDIA_PRESENT;break;
  case 2: if(wdis->cd_statusinfo.asc==0x3A){
           switch(wdis->cd_statusinfo.ascq){
            case 0x00:status = CDASPI_CDMEDIA_NOT_PRESENT;break;
            case 0x01:status = CDASPI_CDMEDIA_NOT_PRESENT_TRAY_CLOSED;break;
            case 0x02:status = CDASPI_CDMEDIA_NOT_PRESENT_TRAY_OPEN;break;
           }
          }
 }
 return status;
}

static unsigned int WINASPICD_get_diskinfo(void *wdip,unsigned int drivenum,struct cddrive_disk_info_s *diskinfos)
{
 struct winaspi_drive_info_s *wdis=wdip;
 mpxp_uint8_t cmd[10];

 if(!wdis)
  return CDDRIVE_RETCODE_FAILED;

 wdis->diskinfos=diskinfos;

 pds_memset(wdis->toc_data,0,sizeof(wdis->toc_data));
 pds_memset(cmd,0,sizeof(cmd));

 cmd[0]=0x43;
 cmd[1]=wdis->lun_id<<5;
 cmd[6]=1;
 cmd[7]=sizeof(wdis->toc_data) >> 8;
 cmd[8]=sizeof(wdis->toc_data) & 0xFF;

 if(cdaspi_scsi_issuecmd(wdis,SRB_DIR_IN,cmd,sizeof(cmd),wdis->toc_data,sizeof(wdis->toc_data))!=SS_COMP)
  return CDDRIVE_RETCODE_FAILED;
 if(!wdis->toc_data[0] && (wdis->toc_data[1]<2))
  return CDDRIVE_RETCODE_FAILED;
 diskinfos->nb_tracks=(((mpxp_uint32_t)wdis->toc_data[0] << 8) + wdis->toc_data[1] - 2) / 8;

 return CDDRIVE_RETCODE_OK;
}

static unsigned int WINASPICD_get_trackinfos(void *wdip,unsigned int drivenum,struct cddrive_track_info_s *trackinfo_p)
{
 struct winaspi_drive_info_s *wdis=wdip;
 struct cddrive_disk_info_s *diskinfos;
 struct cddrive_track_info_s *trackinfos=trackinfo_p;
 unsigned int i,text_size;
 mpxp_uint8_t *tocdata,cmd[10];
 char line[512];

 if(!wdis)
  return CDDRIVE_RETCODE_FAILED;
 diskinfos=wdis->diskinfos;
 if(!diskinfos || !diskinfos->nb_tracks)
  return CDDRIVE_RETCODE_FAILED;
 tocdata=&wdis->toc_data[0];
 tocdata+=4;                 // begin of track infos
 for(i=diskinfos->firsttrack;i<=diskinfos->lasttrack;i++){
  trackinfos->tracknum=i;
  if((tocdata[2]>=diskinfos->firsttrack) && (tocdata[2]<=diskinfos->lasttrack))
   trackinfos->tracktype=((tocdata[1]&AUDIOTRKFLAG) && !(tocdata[1]&CDROMDATAFLAG))? CDDRIVE_TRACKTYPE_AUDIO:CDDRIVE_TRACKTYPE_DATA;
  trackinfos->framepos_begin=PDS_GETB_BE32(&tocdata[4]);
  trackinfos++;
  tocdata+=8;
 }

 // read cd-text
 pds_memset(wdis->toc_data,0,sizeof(wdis->toc_data));
 pds_memset(cmd,0,sizeof(cmd));
 cmd[0]=0x43;
 cmd[1]=wdis->lun_id<<5;
 cmd[2]=5;
 cmd[6]=1;
 cmd[7]=sizeof(wdis->toc_data) >> 8;
 cmd[8]=sizeof(wdis->toc_data) & 0xFF;
 cdaspi_scsi_issuecmd(wdis,SRB_DIR_IN,cmd,sizeof(cmd),wdis->toc_data,sizeof(wdis->toc_data));

 text_size=(((mpxp_uint32_t)wdis->toc_data[0] << 8 ) | wdis->toc_data[1]) + 2;
 if((text_size>=4) && (text_size<=sizeof(wdis->toc_data))){
  unsigned int nNumPacks=(text_size-4)/sizeof(CDTEXTPACK),outchars=0;
  trackinfos=trackinfo_p;
  for(i=0;i<nNumPacks;i++){
   CDTEXTPACK *textpack=(CDTEXTPACK *)&wdis->toc_data[i*sizeof(CDTEXTPACK)+4];
   if((textpack->block==0) && ((textpack->packType==CDT_TRACK_TITLE) || (textpack->packType==CDT_PERFORMER))){
    unsigned int inchars=0,tracknumber_t=textpack->trackNumber,tracknumber_p=textpack->trackNumber;
    while(inchars<sizeof(textpack->data)){
     char c=textpack->data[inchars++];
     line[outchars]=c;
     if(outchars<(sizeof(line)-1))
      outchars++;
     if(!c){
      unsigned int len=pds_strlen(line);
      if(len){
       switch(textpack->packType){
        case CDT_TRACK_TITLE:
         if(tracknumber_t==0){ // album name
          if(!diskinfos->album_name){
           diskinfos->album_name=(char *)malloc(len+1);
           if(diskinfos->album_name)
            pds_strcpy(diskinfos->album_name,line);
          }
         }else{
          trackinfos=&trackinfo_p[tracknumber_t-1];
          if(!trackinfos->title){
           trackinfos->title=(char *)malloc(len+1);
           if(trackinfos->title)
            pds_strcpy(trackinfos->title,line);
          }
         }
         tracknumber_t++;  // more titles in one pack
         break;
        case CDT_PERFORMER:
         if(tracknumber_p==0){ // performer for all tracks
          if(!diskinfos->artist_name){
           diskinfos->artist_name=(char *)malloc(len+1);
           if(diskinfos->artist_name)
            pds_strcpy(diskinfos->artist_name,line);
          }
         }else{
          trackinfos=&trackinfo_p[tracknumber_p-1];
          if(!trackinfos->artist){
           trackinfos->artist=(char *)malloc(len+1);
           if(trackinfos->artist)
            pds_strcpy(trackinfos->artist,line);
          }
         }
         tracknumber_p++;  // more performers in one pack
         break;
       }
      }
      outchars=0;
     }
    }
   }
  }
  if(diskinfos->artist_name){
   trackinfos=trackinfo_p;
   for(i=0;i<diskinfos->nb_tracks;i++){
    if(!trackinfos->artist)
     trackinfos->artist=diskinfos->artist_name;
    trackinfos++;
   }
  }
 }

 return CDDRIVE_RETCODE_OK;
}

static unsigned int WINASPICD_start_read(void *wdip,unsigned int drivenum,struct cddrive_track_info_s *trackinfo)
{
 struct winaspi_drive_info_s *wdis=wdip;
 if(!wdis)
  return CDDRIVE_RETCODE_FAILED;

 //pds_memcpy(&wdis->trackinfo,trackinfo,sizeof(*trackinfo));

 return CDDRIVE_RETCODE_OK;
}

static unsigned int WINASPICD_read_sectors_cda(void *wdip,char *buffer,unsigned long sectorpos,unsigned long sectornum)
{
 struct winaspi_drive_info_s *wdis=wdip;
 int nCmdSize=10;
 mpxp_uint8_t cmd[12];

 if(!wdis)
  return CDDRIVE_RETCODE_FAILED;

 pds_memset(cmd,0,sizeof(cmd));

 cmd[1]=wdis->lun_id<<5;
 PDS_PUTB_BE32(&cmd[2],sectorpos);

 switch(wdis->read_mode){
  case CDASPI_READMODE_MMC:
   cmd[0]=0xBE;
   PDS_PUTB_BE16(&cmd[7],sectornum);
   cmd[9]=0xF8; // MMC  Set vendor specific byte
   nCmdSize=12;
   break;
  default: // standard read
   cmd[0]=0x28;
   PDS_PUTB_BE16(&cmd[7],sectornum);
   nCmdSize=10;
   break;
 }

 if(cdaspi_scsi_issuecmd(wdis,SRB_DIR_IN,cmd,nCmdSize,buffer,CD_READBUF_SIZE+CD_SYNC_SIZE)!=SS_COMP)
  return CDDRIVE_RETCODE_FAILED;

 //endian corr
 //swap left/right

 return CDDRIVE_RETCODE_OK;
}

static unsigned int WINASPICD_load_unload_media(void *wdip,unsigned int drivenum)
{
 struct winaspi_drive_info_s *wdis=wdip;
 unsigned int stat;
 mpxp_uint8_t cmd[6];

 if(!wdis)
  return CDDRIVE_RETCODE_FAILED;

 pds_memset(cmd,0,sizeof(cmd));
 cmd[0]=0x1b;
 cmd[1]=wdis->lun_id<<5;

 stat=cdaspi_get_media_status(wdis);
 if(stat==CDASPI_CDMEDIA_PRESENT) // door is closed (sure)
  funcbit_enable(wdis->drive_flags,WDIS_DRIVEFLAG_DOORCLOSED);

 if(funcbit_test(wdis->drive_flags,WDIS_DRIVEFLAG_DOORCLOSED)) // not sure
  cmd[4]=0x02;   // open door
 else
  cmd[4]=0x03;   // close

 funcbit_inverse(wdis->drive_flags,WDIS_DRIVEFLAG_DOORCLOSED);

 if(cdaspi_scsi_issuecmd(wdis,SRB_DIR_OUT,cmd,sizeof(cmd),NULL,0)==SS_COMP)
  return CDDRIVE_RETCODE_OK;

 return CDDRIVE_RETCODE_FAILED;
}

static unsigned int WINASPICD_set_speed(void *wdip,unsigned int speednum)
{
 struct winaspi_drive_info_s *wdis=wdip;
 unsigned int cmdsize,retcode=SS_PENDING,stat;
 mpxp_uint8_t cmd[12],buf[16];

 if(!wdis)
  return CDDRIVE_RETCODE_FAILED;
 if(!speednum)
  return CDDRIVE_RETCODE_OK;

 stat=cdaspi_get_media_status(wdis);
 if(stat!=CDASPI_CDMEDIA_PRESENT)
  return CDDRIVE_RETCODE_FAILED;

 pds_memset(cmd,0,sizeof(cmd));
 pds_memset(buf,0,sizeof(buf));

 switch(wdis->setspeed_mode){
  case CDASPI_SPEEDMODE_MMC:
   cmdsize= 12;
   cmd[0] = 0xBB;
   cmd[1] = wdis->lun_id << 5 ;
   PDS_PUTB_BE16(&cmd[2],((speednum>=48)? 0xffff:(speednum*0xb0)));
   cmd[4] = 0xFF;
   cmd[5] = 0xFF;

   retcode=cdaspi_scsi_issuecmd(wdis,SRB_DIR_OUT,cmd,cmdsize,NULL,0);
   break;

  // case READ10:   cmd[0]=0x28; cmdsize=10;break;	// Read Std
  // case SPEEDNEC: cmd[0]=0xD4; cmdsize=10;break;	// Read 10
  case CDASPI_SPEEDMODE_SONY:
   cmdsize=6;
   cmd[0]=0x15;  // MODE select
   cmd[1]=0x10;  // no save page
   cmd[4]=4 + 4; // sizeof(mode)

   if(speednum > 4) // speed values > 1 are drive dependent
    speednum = 8;

   buf[4]=0x31;
   buf[5]=2;
   buf[6]=(speednum / 2);

   retcode=cdaspi_scsi_issuecmd(wdis,SRB_DIR_OUT,cmd,cmdsize,buf,8);
 }
 if(retcode!=SS_COMP)
  return CDDRIVE_RETCODE_FAILED;
 return CDDRIVE_RETCODE_OK;
}

struct cddrive_lowlevel_func_s WINASPICD_lowlevel_funcs={
 &WINASPICD_drive_check,
 &WINASPICD_drive_mount,
 &WINASPICD_drive_unmount,
 &WINASPICD_get_diskinfo,
 &WINASPICD_get_trackinfos,
 &WINASPICD_start_read,
 &WINASPICD_read_sectors_cda,
 NULL,
 &WINASPICD_load_unload_media,
 &WINASPICD_set_speed
};

//------------------------------------------------------------------------
//winaspi functions (with wnaspi32.dll) (under win9x only (?))
static unsigned int winaspi_get_cdrom_devices(struct winaspi_drive_info_s *wdis);

static unsigned int WINASPI_init(struct winaspi_drive_info_s *wdis)
{
 if(!winaspi_dll_handle)
  winaspi_dll_handle=newfunc_dllload_winlib_load("wnaspi32.dll");
 if(!winaspi_dll_handle)
  goto err_out_init;
 if(!winaspi_get_support_info_dllfunc)
  winaspi_get_support_info_dllfunc=(aspi_get_support_info_t)GetProcAddress(winaspi_dll_handle,"GetASPI32SupportInfo");
 if(!winaspi_send_command_dllfunc)
  winaspi_send_command_dllfunc=(aspi_send_command_t)GetProcAddress(winaspi_dll_handle,"SendASPI32Command");
 if(!winaspi_get_support_info_dllfunc || !winaspi_send_command_dllfunc)
  goto err_out_init;

 CDASPI32_deinit_func=WINASPI_deinit;
 CDASPI32_send_command_func=WINASPI_send_command;

 if(winaspi_get_cdrom_devices(wdis)!=CDDRIVE_RETCODE_OK)
  goto err_out_init;

 winaspi_num_initialized_drivers++;
 return 1;

err_out_init:
 WINASPI_deinit(wdis);
 return 0;
}

static void WINASPI_deinit(struct winaspi_drive_info_s *wdis)
{
 if(winaspi_num_initialized_drivers)
  winaspi_num_initialized_drivers--;
 if(!winaspi_num_initialized_drivers && winaspi_dll_handle){
  winaspi_get_support_info_dllfunc=NULL;
  winaspi_send_command_dllfunc=NULL;
  newfunc_dllload_winlib_close(winaspi_dll_handle); // static!
  winaspi_dll_handle=NULL;
 }
}

//windows and win-DLLs use stack calling conventions, Mpxplay uses register-based...
mpxp_int32_t asm_stackcall_proc(void *proc,void *data);

static mpxp_int32_t winaspi_call_proc_stackbased(void *proc,void *data)
{
//#if defined(__WATCOMC__)
#pragma aux asm_stackcall_proc=\
 "cmp edx,0"\
 "je nodata"\
 "push edx"\
 "call eax"\
 "pop edx"\
 "jmp end"\
 "nodata:call eax"\
 "end:"\
 parm[eax][edx] value[eax] modify[ebx ecx edx edi esi];
 return asm_stackcall_proc(proc,data);
/*#else // !!! ???
 if(data)
  return ((call_func_withdata)proc)(data);
 else
  return (call_func_nodata)proc)();
#endif*/
}

static mpxp_uint32_t winaspi_get_support_info(void)
{
 return winaspi_call_proc_stackbased(winaspi_get_support_info_dllfunc,NULL);
 //return winaspi_get_support_info_dllfunc();
}

static mpxp_uint32_t WINASPI_send_command(struct winaspi_drive_info_s *wdis,void *cmd)
{
 return winaspi_call_proc_stackbased(winaspi_send_command_dllfunc,cmd);
}

static unsigned int winaspi_get_aspi_status(struct winaspi_drive_info_s *wdis)
{
 mpxp_int32_t status=0,error;

 status=winaspi_get_support_info();
 error=(status>>8)&0xff;

 if(error==SS_COMP){
  winaspi_num_ha=status&0xff;
  return CDDRIVE_RETCODE_OK;
 }

 return CDDRIVE_RETCODE_FAILED;
}

/*static unsigned int winaspi_modesense(struct winaspi_drive_info_s *wdis,int nPage,mpxp_uint8_t *buf,unsigned int bufsize)
{
 unsigned int retcode=0,len;
 mpxp_uint8_t cmd[12];

 pds_memset(buf,0,bufsize);
 pds_memset(cmd,0,sizeof(cmd));

 cmd[1]=wdis->lun_id<<5;
 cmd[2]=nPage&0x3f;

 if(wdis->drive_flags&WDIS_DRIVEFLAG_ATAPI){
  cmd[0]= 0x5A;					// Operation Code
  cmd[7]= bufsize>>8;
  cmd[8]= bufsize&0xFF;

  retcode=winaspi_scsi_issuecmd(wdis,SRB_DIR_IN,cmd,10,buf,bufsize);

  len=buf[1];
  len+=((int)buf[0])<<8;

  // Convert header to standard SCSI header
  buf[0]=(BYTE)(len-4);
  buf[1]=buf[2];
  buf[2]=buf[3];
  buf[3]=buf[7];
  memmove(buf+4,buf+8,bufsize-4);
 }else{
  if(bufsize>=256)
   return retcode;
  cmd[0]=0x1A;
  cmd[8]=bufsize;
  retcode=winaspi_scsi_issuecmd(wdis,SRB_DIR_IN,cmd,6,buf,bufsize);
 }
 return retcode;
}*/

static unsigned int winaspi_get_aspi_error(srb_header_s *sh)
{
 mpxp_int32_t tickstart=GetTickCount();
 while((sh->SRB_Status==SS_PENDING) && ((GetTickCount()-tickstart)<WINASPI_SCSI_TIMEOUT))
  Sleep(0);
 return sh->SRB_Status;
}

static unsigned int winaspi_get_device_type(struct winaspi_drive_info_s *wdis,unsigned int adapter_id,unsigned int target_id,unsigned int lun_id)
{
 struct srb_gdevblock_s srg;

 pds_memset(&srg,0,sizeof(srg));

 srg.SRB_Cmd   = SC_GET_DEV_TYPE;
 srg.SRB_HaId  = adapter_id;
 srg.SRB_Target= target_id;
 srg.SRB_Lun   = lun_id;

 CDASPI32_send_command_func(wdis,&srg);
 if(winaspi_get_aspi_error((srb_header_s *)&srg)!=SS_COMP)
  return 0;

 return srg.SRB_DeviceType;
}

/*static unsigned int winaspi_device_is_mmc(struct winaspi_drive_info_s *wdis)
{
 unsigned int retcode=0;
 SCSICDMODEPAGE2A *mp;
 mpxp_uint8_t buf[255];

 winaspi_modesense(wdis,0x2A,buf,sizeof(buf));

 mp=(SCSICDMODEPAGE2A*)(&buf[4]);

 if(mp->p_code == 0x2A){
  if(mp->p_len>=4){
   //funcbit_enable(wdis->drive_flags,WDIS_DRIVEFLAGS_MMC);
   //if(mp->cd_da_supported)
    retcode=1;
  }//else // MMC drive, but reports CDDA incapable
   //retcode=1;
 }
 return 0;
}*/

static unsigned int winaspi_get_cdrom_devices(struct winaspi_drive_info_s *wdis)
{
 unsigned int retcode,dev_type;

 retcode=winaspi_get_aspi_status(wdis);
 if(retcode!=CDDRIVE_RETCODE_OK)
  return retcode;

 while(winaspi_adapter_id<winaspi_num_ha){
  while(winaspi_target_id<12){
   while(winaspi_lun_id<8){
    dev_type = winaspi_get_device_type(wdis,winaspi_adapter_id,winaspi_target_id,winaspi_lun_id);
    if((dev_type==DTC_CDROM) || (dev_type==DTC_WORM)){
     wdis->adapter_id=winaspi_adapter_id;
     wdis->target_id=winaspi_target_id;
     wdis->lun_id=winaspi_lun_id;
     wdis->dev_type=dev_type;
     //if(!winaspi_device_is_mmc(wdis))
     // wdis->read_mode=CDASPI_READMODE_10;
     return CDDRIVE_RETCODE_OK;
    }
    winaspi_lun_id++;
   }
   winaspi_lun_id=0;
   winaspi_target_id++;
  }
  winaspi_target_id=0;
  winaspi_adapter_id++;
 }
 winaspi_adapter_id=0;
 return CDDRIVE_RETCODE_FAILED;
}


//-----------------------------------------------------------------------
// NTSCSI functions under Win2k/XP
#include <stddef.h> // for offsetof()

typedef struct {
  BYTE ha;
  BYTE tgt;
  BYTE lun;
  BYTE driveLetter;
  BOOL bIsCDDrive;
  HANDLE hDevice;
}NTSCSIDRIVE;

static void ntscsi_GetDriveInformation(BYTE drivenum,NTSCSIDRIVE *pDrive);
static HANDLE ntscsi_GetFileHandle(BYTE drivenum);
static DWORD ntscsi_ExecSCSICommand(NTSCSIDRIVE	*di,srb_exec_scsicmd_s *lpsrb, BOOL bBeenHereBefore);

static unsigned int NTSCSI_init(struct winaspi_drive_info_s *wdis)
{
 OSVERSIONINFO	osVersion;
 NTSCSIDRIVE	*di;

 pds_memset(&osVersion,0,sizeof(osVersion));
 osVersion.dwOSVersionInfoSize = sizeof( osVersion );
 GetVersionEx(&osVersion);
 if(osVersion.dwPlatformId!=VER_PLATFORM_WIN32_NT)
  return 0;

 di=calloc(1,sizeof(*di));
 if(!di)
  return 0;
 wdis->lowlevel_drive_data=di;
 di->driveLetter=wdis->drivenum;

 CDASPI32_deinit_func=NTSCSI_deinit;
 CDASPI32_send_command_func=NTSCSI_send_command;

 pds_memset(di,0,sizeof(*di));
 ntscsi_GetDriveInformation(wdis->drivenum,di);
 if(di->bIsCDDrive!=TRUE)
  goto err_out_init;

 wdis->adapter_id=di->ha;
 wdis->target_id=di->tgt;
 wdis->lun_id=di->lun;

 return 1;

err_out_init:
 NTSCSI_deinit(wdis);
 return 0;
}

static void NTSCSI_deinit(struct winaspi_drive_info_s *wdis)
{
 if(wdis){
  NTSCSIDRIVE *di=wdis->lowlevel_drive_data;
  if(di){
   if(di->hDevice)
    CloseHandle(di->hDevice);
   free(di);
   wdis->lowlevel_drive_data=NULL;
  }
 }
}

static mpxp_uint32_t NTSCSI_send_command(struct winaspi_drive_info_s *wdis,void *srb_p)
{
 struct srb_header_s *lpsrb=srb_p;
 DWORD dwReturn = SS_ERR;

 if(!lpsrb)
  return 0;

 switch(lpsrb->SRB_Cmd){
  case SC_ABORT_SRB:      // ???
  case SC_EXEC_SCSI_CMD:
   dwReturn = ntscsi_ExecSCSICommand(wdis->lowlevel_drive_data,(srb_exec_scsicmd_s *)lpsrb,FALSE);
   break;
  default:
   lpsrb->SRB_Status = SS_ERR;
   dwReturn = SS_ERR;
   break;
 }
 return dwReturn;
}

static HANDLE ntscsi_GetFileHandle(BYTE drivenum)
{
 HANDLE	fh = NULL;
 DWORD dwFlags = GENERIC_READ;
 DWORD dwAccessMode = FILE_SHARE_READ;
 OSVERSIONINFO osver;
 char devicename[16];

 pds_memset(&osver,0,sizeof(osver));
 osver.dwOSVersionInfoSize = sizeof(osver);
 GetVersionEx(&osver);

 if((osver.dwPlatformId==VER_PLATFORM_WIN32_NT) && (osver.dwMajorVersion>4)){
  dwFlags |= GENERIC_WRITE;
  dwAccessMode |= FILE_SHARE_WRITE;
 }

 sprintf(devicename, "\\\\.\\%c:", (char)('A'+drivenum) );

 fh=CreateFileA(devicename,dwFlags,dwAccessMode,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

 if(fh==INVALID_HANDLE_VALUE){
  dwFlags ^= GENERIC_WRITE;
  dwAccessMode ^= FILE_SHARE_WRITE;
  fh=CreateFileA(devicename,dwFlags,dwAccessMode,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 }

 return fh;
}

static void ntscsi_GetDriveInformation(BYTE drivenum, NTSCSIDRIVE *pDrive)
{
 HANDLE	       fh;
 SCSI_ADDRESS  scsiAddr;
 ULONG	       returned;

 fh=ntscsi_GetFileHandle(drivenum);
 if(fh==INVALID_HANDLE_VALUE)
  return;

 pds_memset(&scsiAddr,0,sizeof(scsiAddr));
 scsiAddr.Length = sizeof( SCSI_ADDRESS );

 if(DeviceIoControl(fh,IOCTL_SCSI_GET_ADDRESS,NULL,0,&scsiAddr,sizeof(SCSI_ADDRESS),&returned,NULL)){
  pDrive->bIsCDDrive = TRUE;
  pDrive->ha  = scsiAddr.PortNumber;
  pDrive->tgt = scsiAddr.TargetId;
  pDrive->lun = scsiAddr.Lun;
  pDrive->driveLetter = drivenum;
  pDrive->hDevice = INVALID_HANDLE_VALUE;
 }else{
  if(GetLastError()==50){ // support USB/FIREWIRE devices where this call is not supported, assign drive letter as device ID
   pDrive->bIsCDDrive = TRUE;
   pDrive->ha  = drivenum;
   pDrive->tgt = 0;
   pDrive->lun = 0;
   pDrive->driveLetter = drivenum;
   pDrive->hDevice = INVALID_HANDLE_VALUE;
  }else{
   pDrive->bIsCDDrive = FALSE;
  }
 }
 CloseHandle(fh);
}

static DWORD ntscsi_ExecSCSICommand(NTSCSIDRIVE	*di,srb_exec_scsicmd_s *lpsrb, BOOL bBeenHereBefore)
{
 BOOL	status;
 ULONG	returned = 0;
 SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;

 if(di->hDevice==INVALID_HANDLE_VALUE)
  di->hDevice = ntscsi_GetFileHandle(di->driveLetter);
 if(di->hDevice==INVALID_HANDLE_VALUE)
  return SS_ERR;

 pds_memset(&swb,0,sizeof(swb));
 swb.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);

 if(lpsrb->SRB_Flags&SRB_DIR_IN)
  swb.spt.DataIn = SCSI_IOCTL_DATA_IN;
 else if(lpsrb->SRB_Flags&SRB_DIR_OUT)
  swb.spt.DataIn = SCSI_IOCTL_DATA_OUT;
 else
  swb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;

 swb.spt.DataTransferLength = lpsrb->SRB_BufLen;
 swb.spt.TimeOutValue	    = 15;
 swb.spt.DataBuffer	    = lpsrb->SRB_BufPointer;
 swb.spt.SenseInfoLength    = lpsrb->SRB_SenseLen;
 swb.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER,ucSenseBuf);
 swb.spt.CdbLength	    = lpsrb->SRB_CDBLen;

 memcpy(swb.spt.Cdb,lpsrb->CDBByte,lpsrb->SRB_CDBLen);

 status=DeviceIoControl(di->hDevice,IOCTL_SCSI_PASS_THROUGH_DIRECT,&swb,sizeof(swb),&swb,sizeof(swb),&returned,NULL);

 memcpy(lpsrb->SenseArea,swb.ucSenseBuf,lpsrb->SRB_SenseLen);

 if(status){
  lpsrb->SRB_Status = SS_COMP;
 }else{
  DWORD dwErrCode;

  lpsrb->SRB_Status   = SS_ERR;
  lpsrb->SRB_TargStat = 0x0004;
  lpsrb->SRB_Hdr_Rsvd = dwErrCode = GetLastError();

  if(!bBeenHereBefore && ((dwErrCode == ERROR_MEDIA_CHANGED) || (dwErrCode == ERROR_INVALID_HANDLE)) ){
   if(dwErrCode != ERROR_INVALID_HANDLE){
    CloseHandle(di->hDevice );
    di->hDevice = INVALID_HANDLE_VALUE;
   }
   ntscsi_GetDriveInformation(di->driveLetter,di);
   lpsrb->SRB_Status = ntscsi_ExecSCSICommand(di,lpsrb,TRUE);
  }
 }

 return lpsrb->SRB_Status;
}

#endif // MPXPLAY_WIN32
