#ifndef cd_drive_h
#define cd_drive_h

#define CDDRIVE_TRACKTYPE_NONE  0
#define CDDRIVE_TRACKTYPE_DATA  1
#define CDDRIVE_TRACKTYPE_AUDIO 2

#define CD_FRAME_SIZE    2352
#define CD_READ_SECTORS  24
#define CD_READBUF_SIZE  (CD_READ_SECTORS*CD_FRAME_SIZE)
#define CD_SYNC_SECTORS  8
#define CD_SYNC_SIZE     CD_FRAME_SIZE

#define CDDRIVE_RETCODE_FAILED     0
#define CDDRIVE_RETCODE_OK         1
#define CDDRIVE_RETCODE_RESET_READ 2

typedef struct cddrive_disk_info_s{
 unsigned int  firsttrack;
 unsigned int  lasttrack;
 unsigned int  nb_tracks;
 unsigned long nb_frames;
 char *artist_name;
 char *album_name;
}cddrive_disk_info_s;

typedef struct cddrive_track_info_s{
 unsigned int  tracknum;
 unsigned int  tracktype;
 unsigned long framepos_begin;
 unsigned long framepos_end;
 char *artist;
 char *title;
}cddrive_track_info_s;

typedef struct cddrive_lowlevel_func_s{
 unsigned int (*drive_check)(unsigned int drivenum);
 void        *(*drive_mount)(unsigned int drivenum);
 void         (*drive_unmount)(void *mdip);
 unsigned int (*get_diskinfo)(void *mdip,unsigned int drivenum,struct cddrive_disk_info_s *diskinfos);
 unsigned int (*get_trackinfos)(void *mdip,unsigned int drivenum,struct cddrive_track_info_s *trackinfos);
 unsigned int (*start_read)(void *mdip,unsigned int drivenum,struct cddrive_track_info_s *trackinfo);
 unsigned int (*read_sectors_cda)(void *mdip,char *buffer,unsigned long sectorpos,unsigned long sectornum);
 unsigned int (*read_sectors_raw)(void *mdip,char *buffer,unsigned long sectorpos,unsigned long sectornum);
 unsigned int (*load_unload_media)(void *mdip,unsigned int drivenum);
 unsigned int (*set_speed)(void *mdip,unsigned int speed);
}cddrive_lowlevel_func_s;

#endif // cd_drive_h
