#ifndef cntfuncs_h
#define cntfuncs_h

#include <stdio.h>
#include "control.h"
#include "mpxplay.h"

extern void mpxplay_control_initvar(int,char *argv[],struct mainvars *);
extern void mpxplay_control_getcommandlineopts(int,char *argv[]);
extern void mpxplay_control_checkvar(struct mainvars *);
extern void mpxplay_control_configfile_parts_loadini(void);
extern void mpxplay_control_configfile_parts_saveini(void);
extern void mpxplay_control_configfile_parts_init(void);
extern void mpxplay_control_configfile_parts_close(void);
extern void mpxplay_control_configfile_close(void);

extern void mpxplay_control_keyboard_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern void mpxplay_control_keyboard_init(void);
extern void mpxplay_control_keyboard_set_topfunc(void *newtopfunc);
extern void *mpxplay_control_keyboard_get_topfunc(void);
extern void check_dosshellstart(void);
extern void mpxplay_control_keyboard_songnum_reset(void);

extern void mpxplay_control_keygroup_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern int mpxplay_control_keygroup_getgroup(unsigned int extkey,struct mainvars *);
extern int mpxplay_control_keygroup_getnextfunc(void);

extern void mpxplay_control_fastlist_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern unsigned int mpxplay_control_fastlist_enabled(void);
extern void mpxplay_control_fastlist_searchfilename(struct playlist_side_info *psi,char *filename);
extern unsigned int mpxplay_control_fastlist_keycheck(unsigned int extkey,mainvars *);
extern unsigned int mpxplay_control_fastlist_step(mainvars *mvp,int direction);

extern void mpxplay_control_serial_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern void mpxplay_control_serial_init(void);
extern void mpxplay_control_serial_close(void);

extern void mpxplay_control_startup_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern void mpxplay_control_startup_loadlastlist(void);
extern void mpxplay_control_startup_getstartpos(mainvars *mvp);
extern void mpxplay_control_startup_saveini(mpxini_line_t *,struct mpxini_part_t *,FILE *);

extern void mpxplay_control_mouse_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern void mpxplay_control_mouse_winconsole_getevent(void);
extern void mpxplay_control_mouse_init(void);
extern void mpxplay_control_mouse_close(void);
extern void mpxplay_control_mouse_getpos(unsigned int *hmousex,unsigned int *hmousey);
extern void mpxplay_control_mouse_setrange(unsigned int hx,unsigned int hy);

extern void mpxplay_control_joy_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern void mpxplay_control_joy_init(void);

#endif
