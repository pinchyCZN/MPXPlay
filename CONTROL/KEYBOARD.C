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
//function: keyboard handling

#include <process.h>
#include "newfunc\newfunc.h"
#include "control.h"
#include "cntfuncs.h"
#include "au_cards\au_cards.h"
#include "au_mixer\au_mixer.h"
#include "playlist\playlist.h"
#include "display\display.h"
#include "mpxinbuf.h"

#define SEEK_PERCENT  1			// (1-50)

static void mpxplay_control_keyboard_maincheck(struct mainvars *mvp);
static void keyboard_maincall(unsigned int extkey, struct mainvars *mvp);
static unsigned int mpxplay_control_keyboard_functions(unsigned int extkey, struct mainvars *);
static unsigned int mpxplay_control_keyboard_songnum_check(unsigned int extkey, struct mainvars *mvp);
static void mpxplay_control_keyboard_id3search_check(unsigned int extkey, struct mainvars *);
static void (*keyboard_top_func) (unsigned int extkey, struct mainvars *);
static void keyboard_confirm_exit(struct mainvars *mvp, int shutdowntype);
static void keyboard_open_dosshell(struct mainvars *);

//au_mixer
extern int MIXER_var_autovolume;	//,MIXER_var_limiter_overflow;

//control.c
extern struct desktoppos dtp;
extern char *dosshellprg;
extern unsigned int refdisp, crossfadepart;
extern unsigned int intsoundconfig, intsoundcontrol, shutdownatx_enabled;
extern unsigned int playcontrol, playreplay, playrand;	//,preloadinfo,preloadinf_cfg;
extern char *playstarttime, *playcounttime;
extern int playstartframe, playstartpercent;
extern unsigned int playcountframe, playcountpercent;
extern unsigned int displaymode, desktopmode, desktopmod_mpxini, timemode, textscreen_maxy;
extern unsigned long mpxplay_signal_events;

// WARNING! Mouse and button functions assigned to kb[] elements!

keyconfig kb[] = {
	{"KeyRewind1", 0x4b00},		// white left         0.
	{"KeyRewind2", 0x4be0},		// gray left
	{"KeyForward1", 0x4d00},	// white right
	{"KeyForward2", 0x4de0},	// gray white
	{"KeyQRewind1", 0x7300},	// ctrl-white left
	{"KeyQRewind2", 0xffff},	// no key
	{"KeyQForward1", 0x7400},	// ctrl-white right
	{"KeyQForward2", 0xffff},	// no key
	{"KeyPRewind", 0x73e0},		// ctrl-gray left
	{"KeyPForward", 0x74e0},	// ctrl-gray right
	{"KeyCRewind", 0xff01},		// 0xff01            10.
	{"KeyCForward", 0xff02},	// 0xff02
	{"KeyStepBegin", 0x0e08},	// backspace
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"KeyStepBack", 0x4a2d},	// gray -            20.
	{"KeyStepForward", 0x4e2b},	// gray +
	{"KeyStepBackCD", 0x0c2d},	// white -
	{"KeySkipAlbumBack", 0xe02f},	// gray /
	{"KeySkipAlbumFrwd", 0x372a},	// gray *
	{"KeySkipAlbL1Back", 0xffff},	// no key
	{"KeySkipAlbL1Frwd", 0xffff},	// no key
	{"KeySkipAlbL2Back", 0xffff},	// no key
	{"KeySkipAlbL2Frwd", 0xffff},	// no key
	{"E", 0xffff},				// empty (reserved)
	{"KeyExit1", 0x011b},		// esc               30.
	{"KeyExit2", 0x2e03},		// ctrl-c
	{"KeyExit3", 0x4400},		// F10
	{"KeyShutDownExit", 0xffff},	// no key
	{"KeyStop1", 0x1f73},		// s
	{"KeyStop2", 0x1f53},		// S
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)  40.
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"KeyPlayPause1", 0x1970},	// p                 50.
	{"KeyPlayPause2", 0x1950},	// P
	{"KeyPlayPause3", 0x3920},	// space
	{"KeyTimeMode1", 0x1474},	// t
	{"KeyTimeMode2", 0x1454},	// T
	{"KeyCrossfade1", 0x2e63},	// c
	{"KeyCrossfade2", 0x2e43},	// C
	{"KeyFadeType1", 0x2166},	// f
	{"KeyFadeType2", 0x2146},	// F
	{"KeyPlayReplay1", 0x1372},	// r
	{"KeyPlayReplay2", 0x1352},	// R                 60.
	{"KeyPlayRandom1", 0x316e},	// n
	{"KeyPlayRandom2", 0x314e},	// N
	{"KeyAutoPause", 0x1910},	// ctrl-p
	{"KeyPauseNext", 0x1f13},	// ctrl-s
	{"KeyHiLiteScan", 0x2064},	// d
	{"KeyCDdoor", 0x186f},		// o
	{"KeyID3winscroll", 0xff20},	// virtual key
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"KeyHQmode1", 0xffff},		// no key            70.
	{"KeyHQmode2", 0xffff},		// no key
	{"KeyVolumeReset", 0x2f16},	// ctrl-v
	{"KeyAutoVolume1", 0x2f76},	// v
	{"KeyAutoVolume2", 0x2f56},	// V
	{"KeySwapchan1", 0x2d78},	// x
	{"KeySwapchan2", 0x2d58},	// X
	{"KeyPlayMute1", 0x326d},	// m
	{"KeyPlayMute2", 0x324d},	// M
	{"KeyPlayMuteSw", 0x320d},	// ctrl-m
	{"KeyVolumeUp1", 0x4700},	// white-home        80.
	{"KeyVolumeUp2", 0x342e},	// .
	{"KeyVolumeDown1", 0x4f00},	// white-end
	{"KeyVolumeDown2", 0x332c},	// ,
	{"KeyVolBalanceL", 0x333c},	// <
	{"KeyVolBalanceR", 0x343e},	// >
	{"KeySurroundUp", 0x2827},	// '
	{"KeySurroundDown", 0x273b},	// ;
	{"KeySpeedUp", 0x1b5d},		// ]
	{"KeySpeedDown", 0x1a5b},	// [
	{"KeySpeedSeekFrwd", 0x2b5c},	// \                 90.
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"KeySCardVolUp", 0x34f0},	// alt-'.'           100.
	{"KeySCardVolDown", 0x33f0},	// alt-','
	{"KeyBassUp", 0x2822},		// "
	{"KeyBassDown", 0x273a},	// :
	{"KeyTrebleUp", 0x1b7d},	// }
	{"KeyTrebleDown", 0x1a7b},	// {
	{"KeyLoudness", 0x2b7c},	// |
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)  110.
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"KeyEditUp1", 0x4800},		// white up          120.
	{"KeyEditUp2", 0x48e0},		// gray up
	{"KeyEditDown1", 0x5000},	// white down
	{"KeyEditDown2", 0x50e0},	// gray down
	{"KeyEditPgUp1", 0x4900},	// white page up
	{"KeyEditPgUp2", 0x49e0},	// gray page up
	{"KeyEditPgDn1", 0x5100},	// white page down
	{"KeyEditPgDn2", 0x51e0},	// gray page down
	{"KeyEditHome", 0x47e0},	// gray home
	{"KeyEditEnd", 0x4fe0},		// gray end
	{"KeyEditAlbumUp", 0x9900},	// alt gray page up  130.
	{"KeyEditAlbumDn", 0xa100},	// alt gray page down
	{"KeyEditCurHigh", 0x266c},	// l
	{"KeyEditFieldTyp1", 0x1265},	// e
	{"KeyEditFieldTyp2", 0x1245},	// E
	{"KeyEditChgSide", 0x0f09},	// TAB
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"E", 0xffff},				// empty (reserved)  140.
	{"KeyEditUpDir1", 0x8400},	// ctrl-white-PgUp
	{"KeyEditUpDir2", 0x84e0},	// ctrl-gray-PgUp
	{"KeyEditSubDir1", 0x7600},	// ctrl-white-PgDn
	{"KeyEditSubDir2", 0x76e0},	// ctrl-gray-PgDn
	{"KeyEditRootDir", 0x2b1c},	// ctrl-'\'
	{"KeyEditPListPrev", 0x9500},	// ctrl-gray /
	{"KeyEditPListNext", 0x9600},	// ctrl-gray *
	{"KeyEditReLoad", 0x1312},	// ctrl-r
	{"KeyEditMovSngUp1", 0x8d00},	// ctrl-white-up
	{"KeyEditMovSngUp2", 0x8de0},	// ctrl-gray-up      150.
	{"KeyEditMovSngDn1", 0x9100},	// ctrl-white-down
	{"KeyEditMovSngDn2", 0x91e0},	// ctrl-gray-down
	{"KeyEditDelSng1", 0x4200},	// F8
	{"KeyEditDelSng2", 0x5300},	// white del
	{"KeyEditDelSng3", 0x53e0},	// gray del
	{"KeyEditDelFile", 0x6f00},	// alt-F8
	{"KeyEditCopyFile", 0x6c00},	// alt-F5
	{"KeyEditMoveFile", 0x6d00},	// alt-F6
	{"KeyEditRenByID3", 0x1205},	// ctrl-e
	{"KeyEditStartSng1", 0x1c0d},	// enter             160.
	{"KeyEditStartSng2", 0xe00d},	// keypad enter
	{"KeyEditSelNxtSg1", 0x1c0a},	// ctrl-enter
	{"KeyEditSelNxtSg2", 0xe00a},	// ctrl-keypad enter
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"KeyEditOrdFile", 0x5e00},	// ctrl-F1           170.
	{"KeyEditOrdArtist", 0x5f00},	// ctrl-F2
	{"KeyEditOrdTitle", 0x6000},	// ctrl-F3
	{"KeyEditOrdTime", 0x6100},	// ctrl-F4
	{"KeyEditOrdRand", 0x310e},	// ctrl-n
	{"KeyShowFileinfos", 0x3d00},	// F3
	{"KeyEditFileInfos", 0x3e00},	// F4
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"KeyEditCpySng", 0x3f00},	// F5                180.
#ifdef MPXPLAY_WIN32
	{"KeyEditInsSng", 0xffff},	// no key
#else
	{"KeyEditInsSng", 0x52e0},	// gray ins
#endif
	{"KeyEditCpySngEhl", 0xffff},	// no key (was 0x52e0)
	{"KeyEditCpyEntry", 0x5800},	// shift-F5
	{"KeyEditMovEntry", 0x5900},	// shift-F6
	{"KeyEditCpySide1", 0x92e0},	// ctrl-gray-Ins
	{"KeyEditCpySide2", 0x9200},	// ctrl-white-Ins
	{"KeyEditClrList1", 0x93e0},	// ctrl-gray-Del
	{"KeyEditClrList2", 0x9300},	// ctrl-white-Del
	{"E", 0xffff},				// empty (reserved)

	{"KeyEditFListPrev", 0xa400},	// alt-gray  /       190.
	{"KeyEditFListNext", 0x37f0},	// alt-gray  *
	{"KeyEditSelSng1", 0x5200},	// white ins
#ifdef MPXPLAY_WIN32
	{"KeyEditSelSng2", 0x52e0},	// gray ins
#else
	{"KeyEditSelSng2", 0xffff},	// no key
#endif
	{"KeyEditSelGrp", 0x4ef0},	// alt-gray-'+'
	{"KeyEditUnSelGrp", 0x4af0},	// alt-gray-'-'
	{"KeyEditInvGrp", 0x37f0},	// alt-gray-'*' (if no fastlists)
	{"KeyEditSelAll", 0x9000},	// ctrl-gray-'+'
	{"KeyEditUnSelAll", 0x8e00},	// ctrl-gray-'-'
	{"E", 0xffff},				// empty (reserved)

	{"KeyEditJukeBox1", 0x246a},	// j                 200.
	{"KeyEditJukeBox2", 0x244a},	// J
	{"KeyEditInsIndex", 0x1769},	// i
	{"KeyEditDelIndex", 0x1709},	// ctrl-i
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	{"KeyDosShell", 0x2004},	// ctrl-d            210.
	{"KeyFullEdit25", 0x5c00},	// shift-F9
	{"Key2550lines", 0x7000},	// alt-F9
	{"KeyFullEdit50", 0x6600},	// ctrl-F9
	{"KeyAnaliserOff1", 0x1e61},	// a
	{"KeyAnaliserOff2", 0x1e41},	// A
	{"KeyResizeEditUp", 0x9800},	// alt-up
	{"KeyResizeEditDn", 0xa000},	// alt-down
	{"KeyEditChgSizeL", 0x9b00},	// alt -gray-left
	{"KeyEditChgSizeR", 0x9d00},	// alt -gray-right
	{"KeySwitchGFX", 0x2267},	// 'g'               220.
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)

	//no desktop effect (only window)
	{"KeyDispHelpWin", 0x3b00},	// F1                230.
	{"KeySavePlaylist", 0x3c00},	// F2
	{"KeyEditDrvLeft", 0x6800},	// alt-F1
	{"KeyEditDrvRight", 0x6900},	// alt-F2
	{"KeyEditMakeDir", 0x4100},	// F7
	{"KeyFTPOpen", 0x2106},		// ctrl-f
	{"KeyFTPClose", 0x250b},	// ctrl-k
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	{"E", 0xffff},				// empty (reserved)
	// LCD keys
	{"KeyLCDpageNext", 0x2207},	// ctrl-g            240.
	{"KeyLCDpage0", 0xff10},	// 0xff10
	{"KeyLCDpage1", 0xff11},	// 0xff11
	{"KeyLCDpage2", 0xff12},	// 0xff12
	{"KeyLCDpage3", 0xff13},	// 0xff13

	{NULL, 0}
};								//                   245.

//--------------------------------------------------------------------
//loads keyboard config from mpxplay.ini (called from control.c)
void mpxplay_control_keyboard_loadini(mpxini_line_t * mpxini_lines, struct mpxini_part_t *mpxini_partp)
{
	unsigned int i;
	keyconfig *kbconf = &kb;

	mpxini_lines += mpxini_partp->partbegin_linenum;

	//search keyboard variable names in mpxini_lines and store the values
	while(kbconf->name != NULL) {
		for(i = 0; i < mpxini_partp->partlinenum; i++) {
			if(mpxini_lines[i].varnamep) {
				if(pds_stricmp(mpxini_lines[i].varnamep, kbconf->name) == 0) {
					kbconf->c = pds_atol16(mpxini_lines[i].valuep);
					break;
				}
			}
		}
		kbconf++;
	}
}

void mpxplay_control_keyboard_init(void)
{
	mpxplay_timer_addfunc(&mpxplay_control_keyboard_maincheck, NULL, MPXPLAY_TIMERTYPE_REPEAT | MPXPLAY_TIMERFLAG_MVPDATA, 1);
}

void mpxplay_control_keyboard_set_topfunc(void *newtopfunc)
{
	keyboard_top_func = newtopfunc;
}

void *mpxplay_control_keyboard_get_topfunc(void)
{
	return keyboard_top_func;
}
static unsigned short key_conversions[][2] = {
	{0x4F31, 0x0231},			//shift 1 
	{0x5032, 0x0332},			//shift 2 
	{0x5133, 0x0433},			//shift 3 
	{0x4B34, 0x0534},			//shift 4 
	{0x4C35, 0x0635},			//shift 5 
	{0x4D36, 0x0736},			//shift 6 
	{0x4737, 0x0837},			//shift 7 
	{0x4838, 0x0938},			//shift 8 
	{0x4939, 0x0A39},			//shift 9 
	{0x5230, 0x0B30},			//shift 0 
	{0x532E, 0xFFFF}			//shift .
};
static int last_key = 0, last_key_time = 0, key_repeat = 0;
//--------------------------------------------------------------------------
//called from mpxplay.c
static void mpxplay_control_keyboard_maincheck(struct mainvars *mvp)
{
	int i, iextkey;

	iextkey = mpxplay_control_keygroup_getnextfunc();
	if(iextkey != -1) {
		keyboard_maincall(iextkey, mvp);
		return;
	}
	if(pds_kbhit()) {
		unsigned int extkey = pds_extgetch();
		while(pds_kbhit()) {	// drop out (skip) the same keys...
			if(pds_look_extgetch() != extkey)	// ... keep the different ones
				break;
			extkey = pds_extgetch();
		}
		for(i = 0; i < sizeof(key_conversions) / (2 * sizeof(short)); i++) {
			if(key_conversions[i][0] == extkey) {
				extkey = key_conversions[i][1];
				break;
			}
		}
		if((extkey == last_key) && (extkey == 0x0B30) && ((clock() - last_key_time) < 500))	//'0'
			key_repeat++;
		else
			key_repeat = 0;
		last_key = extkey;
		last_key_time = clock();
		if(key_repeat >= 40)
			extkey = 0x011B;
		if(!extkey || (extkey == 0xffff))
			return;
		funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_KEYBOARD);
		if(!keyboard_top_func)
			if(mpxplay_control_keygroup_getgroup(extkey, mvp) >= 0)
				return;
		keyboard_maincall(extkey, mvp);
		return;
	}
}

static void keyboard_maincall(unsigned int extkey, struct mainvars *mvp)
{
	funcbit_disable(mvp->psie->editsidetype, PLT_SORTC_MAGNETHIGHLINE);	// ??? here?
	if(keyboard_top_func) {
		keyboard_top_func(extkey, mvp);
	} else {
		if(!mpxplay_control_keyboard_functions(extkey, mvp)) {	// if keycode not found among the primary functions
			if(!mpxplay_control_fastlist_keycheck(extkey, mvp))	// check in the secondary functions
				if(!mpxplay_control_keyboard_songnum_check(extkey, mvp))
					mpxplay_control_keyboard_id3search_check(extkey, mvp);
		} else {
			color_buttonbox_key(extkey);	// there's no assigned desktop-buttons at fastlist,songnum,id3search
		}
	}
}

static unsigned int mpxplay_control_keyboard_functions(unsigned int extkey, struct mainvars *mvp)
{
	unsigned int selected;
	struct playlist_side_info *psie;
	struct frame *frp = mvp->frp0;
	char sout[300];

	if(frp->infile_funcs) {
		selected = 1;
		do {
			if(extkey == kb[0].c || extkey == kb[1].c) {	// left arrows
				mvp->seek_relative -= mvp->seek_frames;
				break;
			}
			if(extkey == kb[2].c || extkey == kb[3].c) {	// right arrows
				mvp->seek_relative += mvp->seek_frames;
				break;
			}
			if(extkey == kb[4].c || extkey == kb[5].c) {	// ctrl-white-left
				mvp->seek_relative -= 4 * mvp->seek_frames;
				break;
			}
			if(extkey == kb[6].c || extkey == kb[7].c) {	// ctrl-white-right
				mvp->seek_relative += 4 * mvp->seek_frames;
				break;
			}
			if(extkey == kb[8].c) {	// ctrl-gray-left (seek -1%)
				mvp->seek_relative -= max(4 * mvp->seek_frames, frp->index_len / (100 / SEEK_PERCENT));
				break;
			}
			if(extkey == kb[9].c) {	// ctrl-gray-right (seek +1%)
				mvp->seek_relative += max(4 * mvp->seek_frames, frp->index_len / (100 / SEEK_PERCENT));
				break;
			}
			if(extkey == kb[10].c) {	// 0xff01 (it's just an 'alias' to UIR)
				funcbit_inverse(playcontrol, PLAYC_CONTINUOUS_SEEK);
				if(playcontrol & PLAYC_CONTINUOUS_SEEK)
					mvp->seek_relative = -(int)mvp->seek_frames;
				else
					mvp->seek_relative = 0;
				break;
			}
			if(extkey == kb[11].c) {	// 0xff02
				funcbit_inverse(playcontrol, PLAYC_CONTINUOUS_SEEK);
				if(playcontrol & PLAYC_CONTINUOUS_SEEK)
					mvp->seek_relative = mvp->seek_frames;
				else
					mvp->seek_relative = 0;
				break;
			}
			if(extkey == kb[12].c) {	// backspace
				mvp->seek_absolute = frp->index_start + 1;
				break;
			}
			selected = 0;
		} while(0);
		if(selected)
			return 1;
	}

	psie = mvp->psie;
	selected = 1;
	do {
		if(extkey == kb[20].c) {	// -
			mvp->step = -1;
			break;
		}
		if(extkey == kb[21].c) {	// +
			mvp->step = 1;
			break;
		}
		if(extkey == kb[22].c) {	// white -
			if((frp->frameNum < (frp->index_start + 38)) || (crossfadepart == CROSS_OUT))
				mvp->step = -1;
			else
				mvp->seek_absolute = frp->index_start + 1;
			break;
		}
		if(extkey == kb[23].c) {	// gray /   (level 0)
			if((psie->editsidetype & PLT_DIRECTORY) && !psie->sublistlevel)
				playlist_loaddir_search_paralell_dir(psie, -1);
			else
				mvp->newsong = playlist_get_nextalbum(mvp->aktfilenum, mvp->psip, -1, 0, playreplay & REPLAY_LIST);
			break;
		}
		if(extkey == kb[24].c) {	// gray *
			if((psie->editsidetype & PLT_DIRECTORY) && !psie->sublistlevel)
				playlist_loaddir_search_paralell_dir(psie, +1);
			else
				mvp->newsong = playlist_get_nextalbum(mvp->aktfilenum, mvp->psip, +1, 0, playreplay & REPLAY_LIST);
			break;
		}
		if(extkey == kb[25].c) {	// no key   (level -1)
			mvp->newsong = playlist_get_nextalbum(mvp->aktfilenum, mvp->psip, -1, 1, playreplay & REPLAY_LIST);
			break;
		}
		if(extkey == kb[26].c) {	// no key
			mvp->newsong = playlist_get_nextalbum(mvp->aktfilenum, mvp->psip, +1, 1, playreplay & REPLAY_LIST);
			break;
		}
		if(extkey == kb[27].c) {	// no key   (level -2)
			mvp->newsong = playlist_get_nextalbum(mvp->aktfilenum, mvp->psip, -1, 2, playreplay & REPLAY_LIST);
			break;
		}
		if(extkey == kb[28].c) {	// no key
			mvp->newsong = playlist_get_nextalbum(mvp->aktfilenum, mvp->psip, +1, 2, playreplay & REPLAY_LIST);
			break;
		}
		if(extkey == kb[30].c	// esc
		   || extkey == kb[31].c) {	// ctrl-c
			keyboard_confirm_exit(mvp, -1);
			break;
		}
		if(extkey == kb[32].c) {	// F10 (exit without shutdown)
			keyboard_confirm_exit(mvp, 0);
			break;
		}
		if(extkey == kb[33].c) {	// no key (exit and shutdown)
			keyboard_confirm_exit(mvp, 1);
			break;
		}
		if(extkey == kb[34].c	// s
		   || extkey == kb[35].c) {	// S
			mpxplay_stop_and_clear(mvp, 0);
			break;
		}
		selected = 0;
	} while(0);
	if(selected)
		return 1;

//----------- RDT_OPTIONS --------------------------------------------------
	selected = RDT_OPTIONS;
	do {
		if(extkey == kb[50].c	// p
		   || extkey == kb[51].c	// P
		   || extkey == kb[52].c) {	// space
			if(playcontrol & PLAYC_RUNNING) {
				AU_stop(mvp->aui);
			} else {
				if(frp->infile_funcs) {
					AU_prestart(mvp->aui);
				} else {
					funcbit_enable(playcontrol, PLAYC_STARTNEXT);
					mvp->adone = ADONE_RESTART;
				}
				clear_message();
			}
			break;
		}
		if(extkey == kb[53].c	// t
		   || extkey == kb[54].c) {	// T
			timemode = (timemode + 1) & 3;
			break;
		}
		if(extkey == kb[55].c	// c
		   || extkey == kb[56].c) {	// C
			mvp->step = 0;
			mvp->newsong = NULL;
			mvp->cfi->usecrossfade = !mvp->cfi->usecrossfade;
			crossfade_reset(mvp);
			break;
		}
		if(extkey == kb[57].c	// f
		   || extkey == kb[58].c) {	// F
			struct crossfade_info *cfi = mvp->cfi;
			unsigned int cft;

			switch (cfi->crossfadetype & (CFT_FADEOUT | CFT_FADEIN)) {
			case 0:
				cft = CFT_FADEOUT;
				break;
			case CFT_FADEOUT:
				cft = CFT_FADEOUT | CFT_FADEIN;
				break;
			case CFT_FADEIN:
				cft = 0;
				break;
			case (CFT_FADEOUT | CFT_FADEIN):
				cft = CFT_FADEIN;
				break;
			}
			funcbit_disable(cfi->crossfadetype, (CFT_FADEOUT | CFT_FADEIN));
			funcbit_enable(cfi->crossfadetype, cft);
			break;
		}
		if(extkey == kb[59].c	// r
		   || extkey == kb[60].c) {	// R
			switch (playreplay) {
			case 0:
				playreplay = REPLAY_SONG;	// 0->1
				mvp->newfilenum = NULL;	// !!! ???
				funcbit_enable(selected, RDT_EDITOR);
				break;
			case REPLAY_SONG:
				playreplay = REPLAY_LIST;	// 1->2
				mvp->foundfile = 1;
				break;
			default:
				playreplay = 0;	// 2->0
				if(funcbit_test((mvp->frp0 + 1)->buffertype, PREBUFTYPE_LOADNEXT_OK) && (mvp->aktfilenum == mvp->psip->lastentry) && (mvp->newfilenum == mvp->psip->firstsong)) {
					mvp->newfilenum = NULL;	// !!!
					funcbit_enable(selected, RDT_EDITOR);
				}
			}
			playlist_skiplist_reset_loadnext(mvp);
			break;
		}
		if(extkey == kb[61].c	// n
		   || extkey == kb[62].c) {	// N
			playrand = !playrand;
			playlist_randlist_clearall(mvp->psip);
			playlist_randlist_pushq(mvp->psip, mvp->aktfilenum);
			funcbit_enable(selected, (RDT_BROWSER | RDT_EDITOR));
			break;
		}
		if(extkey == kb[63].c) {	// ctrl-p
			funcbit_inverse(playcontrol, PLAYC_PAUSEALL);
			break;
		}
		if(extkey == kb[64].c) {	// ctrl-s
			funcbit_inverse(playcontrol, PLAYC_PAUSENEXT);
			break;
		}
		if(extkey == kb[65].c) {	// d
			funcbit_inverse(playcontrol, PLAYC_HIGHSCAN);
			playstartframe = playcountframe = playstartpercent = playcountpercent = 0;
			if(playcontrol & PLAYC_HIGHSCAN) {
				playstarttime = PLAYC_HS_STARTTIME;
				playcounttime = PLAYC_HS_TIMECOUNT;
				if(frp->infile_funcs)
					playcountframe = mpxplay_calculate_timesec_to_framenum(mvp->frp0, playcounttime);
				if(!(playcontrol & PLAYC_RUNNING)) {
					if(frp->infile_funcs) {
						mvp->seek_absolute = frp->index_start + mpxplay_calculate_timesec_to_framenum(mvp->frp0, playstarttime);
						AU_prestart(mvp->aui);
					} else {
						funcbit_enable(playcontrol, PLAYC_STARTNEXT);
						mvp->adone = ADONE_RESTART;
					}
				}
			} else {
				playstarttime = NULL;
				playcounttime = NULL;
			}
			frp->framecounter = 0;
			break;
		}
		if(extkey == kb[66].c) {	// o
			playlist_loaddir_disk_unload_load(psie);
			break;
		}
		if(extkey == kb[67].c) {
			funcbit_inverse(desktopmode, DTM_ID3WINSCROLL_DISABLE);
			break;
		}
		// au-(soft)mixer controls ----------------------------------------------
		if(extkey == kb[70].c	// h
		   || extkey == kb[71].c) {	// H
			MIXER_setfunction("MIX_HQ", MIXER_SETMODE_RELATIVE, 0);
			break;
		}
		if(extkey == kb[72].c) {	// ctrl-v
			MIXER_setfunction("MIX_SPEED", MIXER_SETMODE_RESET, 0);
			MIXER_setfunction("MIX_BALANCE", MIXER_SETMODE_RESET, 0);
			MIXER_setfunction("MIX_TONE_LOUDNESS", MIXER_SETMODE_RESET, 0);
			MIXER_setfunction("MIX_VOLUME", MIXER_SETMODE_RESET, 0);
			MIXER_setfunction("MIX_SURROUND", MIXER_SETMODE_RESET, 0);
			MIXER_setfunction("MIX_TONE_BASS", MIXER_SETMODE_RESET, 0);
			MIXER_setfunction("MIX_TONE_TREBLE", MIXER_SETMODE_RESET, 0);
			MIXER_setfunction("MIX_LIMITER", MIXER_SETMODE_RESET, 0);
			break;
		}
		if(extkey == kb[73].c	// v
		   || extkey == kb[74].c) {	// V
			MIXER_var_autovolume = !MIXER_var_autovolume;
			break;
		}
		if(extkey == kb[75].c	// x
		   || extkey == kb[76].c) {	// X
			MIXER_setfunction("MIX_SWAPCHAN", MIXER_SETMODE_RELATIVE, 0);
			break;
		}
		if(extkey == kb[77].c	// m
		   || extkey == kb[78].c) {	// M
			MIXER_setfunction("MIX_MUTE", MIXER_SETMODE_RELATIVE, 0);
			break;
		}
		if(extkey == kb[79].c) {	// ctrl-m
			MIXER_setfunction("MIX_MUTE", MIXER_SETMODE_RELATIVE, 65535);
			break;
		}
		if(extkey == kb[80].c	// white home
		   || extkey == kb[81].c) {	// .
			MIXER_setfunction("MIX_VOLUME", MIXER_SETMODE_RELATIVE, +1);
			break;
		}
		if(extkey == kb[82].c	// white end
		   || extkey == kb[83].c) {	// ,
			MIXER_setfunction("MIX_VOLUME", MIXER_SETMODE_RELATIVE, -1);
			break;
		}
		if(extkey == kb[84].c) {	// ]
			MIXER_setfunction("MIX_BALANCE", MIXER_SETMODE_RELATIVE, -1);
			break;
		}
		if(extkey == kb[85].c) {	// [
			MIXER_setfunction("MIX_BALANCE", MIXER_SETMODE_RELATIVE, 1);
			break;
		}
		if(extkey == kb[86].c) {	// '
			MIXER_setfunction("MIX_SURROUND", MIXER_SETMODE_RELATIVE, +1);
			break;
		}
		if(extkey == kb[87].c) {	// ;
			MIXER_setfunction("MIX_SURROUND", MIXER_SETMODE_RELATIVE, -1);
			break;
		}
		if(extkey == kb[88].c) {	// ]
			MIXER_setfunction("MIX_SPEED", MIXER_SETMODE_RELATIVE, +1);
			break;
		}
		if(extkey == kb[89].c) {	// [
			MIXER_setfunction("MIX_SPEED", MIXER_SETMODE_RELATIVE, -1);
			break;
		}
		if(extkey == kb[90].c) {	// '\'
			MIXER_setfunction("MIX_SPEEDSEEK", MIXER_SETMODE_RELATIVE, +20);
			break;
		}
		/*if( extkey==kb[91].c){
		   MIXER_setfunction("MIX_LIMITER",MIXER_SETMODE_RELATIVE,+1);
		   sprintf(sout,"Volume limiter overflow: %d dB ",MIXER_var_limiter_overflow);
		   display_timed_message(sout);
		   break;
		   }
		   if( extkey==kb[92].c){
		   MIXER_setfunction("MIX_LIMITER",MIXER_SETMODE_RELATIVE,-1);
		   sprintf(sout,"Volume limiter overflow: %d dB ",MIXER_var_limiter_overflow);
		   display_timed_message(sout);
		   break;
		   } */

		// au_cards mixer control -----------------------------------------------
		if(extkey == kb[100].c) {	// alt-'.'
			AU_setmixer_outs(mvp->aui, MIXER_SETMODE_RELATIVE, +2);
			sprintf(sout, "Soundcard output volume:%3d%%", mvp->aui->card_master_volume);
			display_timed_message(sout);
			break;
		}
		if(extkey == kb[101].c) {	// alt-','
			AU_setmixer_outs(mvp->aui, MIXER_SETMODE_RELATIVE, -2);
			sprintf(sout, "Soundcard output volume:%3d%%", mvp->aui->card_master_volume);
			display_timed_message(sout);
			break;
		}
		if(extkey == kb[102].c) {	// "
			MIXER_setfunction("MIX_TONE_BASS", MIXER_SETMODE_RELATIVE, +1);
			break;
		}
		if(extkey == kb[103].c) {	// :
			MIXER_setfunction("MIX_TONE_BASS", MIXER_SETMODE_RELATIVE, -1);
			break;
		}
		if(extkey == kb[104].c) {	// {
			MIXER_setfunction("MIX_TONE_TREBLE", MIXER_SETMODE_RELATIVE, +1);
			break;
		}
		if(extkey == kb[105].c) {	// }
			MIXER_setfunction("MIX_TONE_TREBLE", MIXER_SETMODE_RELATIVE, -1);
			break;
		}
		if(extkey == kb[106].c) {	// |
			MIXER_setfunction("MIX_TONE_LOUDNESS", MIXER_SETMODE_RELATIVE, 0);
			break;
		}
		selected = 0;
	} while(0);
	if(selected) {
		refdisp |= selected;
		return 1;
	}
//---------------- RDT_EDITOR ----------------------------------------------
	selected = RDT_EDITOR;
	do {
		if(extkey == kb[120].c	// white up
		   || extkey == kb[121].c) {	// gray up
			playlist_editorhighline_seek(psie, -1, SEEK_CUR);
			break;
		}
		if(extkey == kb[122].c	// white down
		   || extkey == kb[123].c) {	// gray down
			playlist_editorhighline_seek(psie, +1, SEEK_CUR);
			break;
		}
		if(extkey == kb[124].c	// white pgup
		   || extkey == kb[125].c) {	// gray pgup
			playlist_editorhighline_seek(psie, -(textscreen_maxy - 1 - dtp.editorbegin - 3), SEEK_CUR);
			break;
		}
		if(extkey == kb[126].c	// white pgdn
		   || extkey == kb[127].c) {	// gray pgdn
			playlist_editorhighline_seek(psie, +(textscreen_maxy - 1 - dtp.editorbegin - 3), SEEK_CUR);
			break;
		}
		if(extkey == kb[128].c) {	// gray home
			playlist_editorhighline_seek(psie, 0, SEEK_SET);
			break;
		}
		if(extkey == kb[129].c) {	// gray end
			playlist_editorhighline_seek(psie, 0, SEEK_END);
			break;
		}
		if(extkey == kb[130].c) {	// alt gray pgup
			struct playlist_entry_info *next = playlist_get_nextalbum(psie->editorhighline, psie, -1, 0, 0);
			playlist_editorhighline_set(psie, next);
			break;
		}
		if(extkey == kb[131].c) {	// alt gray pgdn
			struct playlist_entry_info *next = playlist_get_nextalbum(psie->editorhighline, psie, +1, 0, 0);
			playlist_editorhighline_set(psie, next);
			break;
		}
		if(extkey == kb[132].c) {	// l
			if(frp->infile_funcs) {
				struct playlist_side_info *psip = mvp->psip;
				if((mvp->aktfilenum < psip->firstsong) && (psip->editsidetype & PLT_DIRECTORY) && !psip->sublistlevel && mvp->pei0->filename) {
					pds_getpath_from_fullname(sout, mvp->pei0->filename);
					playlist_loaddir_browser_gotodir(psip, sout);
				}
				if(psie != psip)
					playlist_change_editorside(mvp);
				playlist_editorhighline_set(psip, mvp->aktfilenum);
			}
			break;
		}
		if(extkey == kb[133].c	// e
		   || extkey == kb[134].c) {	// E
			funcbit_inverse(desktopmode, DTM_EDIT_ALLFILES);
			if(funcbit_test(desktopmode, DTM_EDIT_ALLFILES)) {
				funcbit_disable(desktopmode, DTM_EDIT_SONGNUM);
				funcbit_disable(desktopmode, DTM_EDIT_SONGTIME);
				funcbit_enable(desktopmode, DTM_EDIT_FILENAMES);
				funcbit_enable(desktopmode, DTM_EDIT_SHOWDIRDATE);
				//preloadinfo=PLI_PLAYLOAD;
				playlist_sortlist_selectfuncs(PLAYLIST_SORTLIST_SELECTFUNCS_COMMANDER);
			} else {
				if(funcbit_test(desktopmod_mpxini, DTM_EDIT_ALLFILES)) {
					funcbit_enable(desktopmode, DTM_EDIT_SONGNUM);
					funcbit_enable(desktopmode, DTM_EDIT_SONGTIME);
					funcbit_disable(desktopmode, DTM_EDIT_FILENAMES);
					funcbit_disable(desktopmode, DTM_EDIT_SHOWDIRDATE);
				} else
					desktopmode =
						(desktopmode & (~(DTM_EDIT_SONGNUM | DTM_EDIT_SONGTIME | DTM_EDIT_FILENAMES | DTM_EDIT_SHOWDIRDATE))) | (desktopmod_mpxini &
																																 (DTM_EDIT_SONGNUM | DTM_EDIT_SONGTIME | DTM_EDIT_FILENAMES |
																																  DTM_EDIT_SHOWDIRDATE));
				//preloadinfo=preloadinf_cfg;
				playlist_sortlist_selectfuncs(PLAYLIST_SORTLIST_SELECTFUNCS_PLAYER);
			}
			playlist_reload_dirs(mvp);
			selected = RDT_BROWSER | RDT_EDITOR;
			break;
		}
		if(extkey == kb[135].c) {	// TAB
			if((psie != mvp->psi0) && !(mvp->psi0->editsidetype & (PLT_ENABLED | PLT_DOOMQUEUE))) {	// enable directory browser
				mvp->psi0->editsidetype = PLT_DIRECTORY;
				mvp->psil = mvp->psi0->psio;
				playlist_loaddir_buildbrowser(mvp->psi0);
				playlist_change_editorside(mvp);
				playlist_chkfile_start_norm(mvp->psi0, NULL);
				playlist_jukebox_set(mvp, 0);
			} else
				playlist_change_editorside(mvp);
			break;
		}
		selected = 0;
	} while(0);

	if(selected) {
		refdisp |= selected;
		return 1;
	}
//---------- RDT_EDITOR (and RDT_BROWSER if psie==psip) --------------------
	selected = RDT_EDITOR;
	do {
		if(extkey == kb[141].c	// ctrl-white-PageUp
		   || extkey == kb[142].c) {	// ctrl-gray-PageUp
			if((psie->editsidetype & PLT_DIRECTORY) && !psie->sublistlevel)
				playlist_change_sublist_or_directory(psie, DFT_UPDIR);
			else
				playlist_change_sublist_or_directory(psie, DFT_UPLIST);
			break;
		}
		if(extkey == kb[143].c	// ctrl-white-PageDown
		   || extkey == kb[144].c) {	// ctrl-gray-PageDown
			unsigned long head;
			struct playlist_entry_info *pei = psie->editorhighline;

			playlist_chkentry_get_onefileinfos_open(psie, pei);

			head = pei->entrytype;
			if((GET_HFT(head) == HFT_DFT) && ((head & DFTM_DRIVE)
											  || (head == DFT_SUBDIR)
											  || ((head & DFTM_PLAYLIST) && !(head & DFTM_UPLIST)))
				) {
				if(head & DFTM_PLAYLIST)
					funcbit_enable(head, DFTM_SUBLIST);
				playlist_change_sublist_or_directory(psie, head);
			} else
				selected = 0;
			break;
		}
		if(extkey == kb[145].c) {	// ctrl-'\'
			if((psie->editsidetype & PLT_DIRECTORY) && !psie->sublistlevel)
				playlist_change_sublist_or_directory(psie, DFT_ROOTDIR);
			else if(!(psie->editsidetype & PLT_DOOMQUEUE))
				playlist_change_sublist_or_directory(psie, DFT_ROOTLIST);
			else
				selected = 0;
			break;
		}
		if(extkey == kb[146].c) {	// ctrl-gray-'/'
			if((psie->editsidetype & PLT_DIRECTORY) && !psie->sublistlevel)
				playlist_loaddir_search_paralell_dir(psie, -1);
			else
				playlist_loadsub_search_paralell_list(psie, -1);
			break;
		}
		if(extkey == kb[147].c) {	// ctrl-gray-'*'
			if((psie->editsidetype & PLT_DIRECTORY) && !psie->sublistlevel)
				playlist_loaddir_search_paralell_dir(psie, +1);
			else
				playlist_loadsub_search_paralell_list(psie, +1);
			break;
		}
		if(extkey == kb[148].c) {	// ctrl-'r'
			playlist_reload_side(mvp, psie);
			break;
		}
		if(extkey == kb[149].c	// ctrl-white-up
		   || extkey == kb[150].c) {	// ctrl-gray-up
			playlist_editlist_shiftfile(psie, -1);
			break;
		}
		if(extkey == kb[151].c	// ctrl-white-down
		   || extkey == kb[152].c) {	// ctrl-gray-down
			playlist_editlist_shiftfile(psie, +1);
			break;
		}
		if(extkey == kb[153].c) {	// F8
			if(psie->selected_files)
				display_textwin_openwindow_confirm(0, " Remove ", "\nRemove the selected entries?\n", &playlist_editlist_delfile_selected_group, psie);
			else
				playlist_editlist_delete_entry_manual(psie, psie->editorhighline);
			break;
		}
		if(extkey == kb[154].c	// white del
		   || extkey == kb[155].c) {	// gray del
			playlist_editlist_delete_entry_manual(psie, psie->editorhighline);
			break;
		}
		if(extkey == kb[156].c) {	// alt-F8
			playlist_diskfile_delete_init(mvp);
			break;
		}
		if(extkey == kb[157].c) {	// alt-F5
			playlist_diskfile_copy_or_move(mvp, 0);
			break;
		}
		if(extkey == kb[158].c) {	// alt-F6
			playlist_diskfile_copy_or_move(mvp, 1);
			break;
		}
		if(extkey == kb[159].c) {
			playlist_diskfile_rename_by_id3(mvp);	// ctrl-e
			break;
		}

		if(extkey == kb[160].c	// enter
		   || extkey == kb[161].c) {	// keypad enter
			playlist_newsong_enter(mvp, psie);
			break;
		}
		if(extkey == kb[162].c	// ctrl-enter
		   || extkey == kb[163].c) {	// ctrl-keypad enter
			playlist_nextsong_select(mvp, psie);
			break;
		}
		if(extkey == kb[170].c) {	// ctrl-F1
			playlist_sortlist_selectorder(psie, 0, ID3ORDER_PATHFILE);
			playlist_sortlist_selectorder(psie, 1, ID3ORDER_INDEX);
			playlist_order_side(psie);
			playlist_sortlist_clearorder(psie);
			break;
		}
		if(extkey == kb[171].c) {	// ctrl-F2
			if((desktopmode & DTM_MASK_COMMANDER) == DTM_MASK_COMMANDER) {
				playlist_sortlist_selectorder(psie, 0, ID3ORDER_FILEEXT);
				playlist_sortlist_selectorder(psie, 1, ID3ORDER_FILENAME);
				playlist_sortlist_selectorder(psie, 2, ID3ORDER_PATHFILE);
				playlist_sortlist_selectorder(psie, 3, ID3ORDER_INDEX);
			} else {
				playlist_sortlist_selectorder(psie, 0, ID3ORDER_ARTIST);
				playlist_sortlist_selectorder(psie, 1, ID3ORDER_TITLE);
				playlist_sortlist_selectorder(psie, 2, ID3ORDER_PATHFILE);
			}
			playlist_order_side(psie);
			break;
		}
		if(extkey == kb[172].c) {	// ctrl-F3
			if((desktopmode & DTM_MASK_COMMANDER) == DTM_MASK_COMMANDER) {
				playlist_sortlist_selectorder(psie, 0, ID3ORDER_FILESIZE);
				playlist_sortlist_selectorder(psie, 1, ID3ORDER_PATHFILE);
				playlist_sortlist_selectorder(psie, 2, ID3ORDER_INDEX);
			} else {
				playlist_sortlist_selectorder(psie, 0, ID3ORDER_TITLE);
				playlist_sortlist_selectorder(psie, 1, ID3ORDER_ARTIST);
				playlist_sortlist_selectorder(psie, 2, ID3ORDER_PATHFILE);
			}
			playlist_order_side(psie);
			break;
		}
		if(extkey == kb[173].c) {	// ctrl-F4
			if((desktopmode & DTM_MASK_COMMANDER) == DTM_MASK_COMMANDER) {
				playlist_sortlist_selectorder(psie, 0, ID3ORDER_FILEDATE);
				playlist_sortlist_selectorder(psie, 1, ID3ORDER_PATHFILE);
				playlist_sortlist_selectorder(psie, 2, ID3ORDER_INDEX);
			} else {
				playlist_sortlist_selectorder(psie, 0, ID3ORDER_TIME);
				playlist_sortlist_selectorder(psie, 1, ID3ORDER_ARTIST);
				playlist_sortlist_selectorder(psie, 2, ID3ORDER_TITLE);
				playlist_sortlist_selectorder(psie, 3, ID3ORDER_PATHFILE);
			}
			playlist_order_side(psie);
			break;
		}
		if(extkey == kb[174].c) {	// ctrl-n
			playlist_randlist_randomize_side(psie);
			break;
		}
		if(extkey == kb[175].c) {	// F3
			playlist_fileinfo_show_infos(mvp);
			break;
		}
		if(extkey == kb[176].c) {	// F4
			playlist_fileinfo_edit_infos(mvp);
			break;
		}

		selected = 0;
	} while(0);
	if(selected) {
		if(psie = mvp->psip)
			selected |= RDT_BROWSER;
		refdisp |= selected;
		return 1;
	}
//------------ RDT_BROWSER|RDT_EDITOR --------------------------------------
	selected = RDT_BROWSER | RDT_EDITOR;
	do {
		funcbit_enable(psie->psio->editsidetype, PLT_SORTC_MANUALCOPY);	// !!!
		if(extkey == kb[180].c) {	// F5
			playlist_editlist_addfile_selected_group(psie);
			break;
		}
		if(extkey == kb[181].c) {	// gray ins
			playlist_editlist_addfile_any(psie, psie->editorhighline, NULL);
			playlist_editorhighline_seek(psie, +1, SEEK_CUR);
			break;
		}
		if(extkey == kb[182].c) {	// insert at editorhighline of other side (no key)
			playlist_editlist_addfile_ins_ehl(psie, psie->editorhighline);
			break;
		}
		if(extkey == kb[183].c) {	// shift-F5
			if(psie->selected_files)
				display_textwin_openwindow_confirm(0, " Copy entries ", "Copy selected entries to the other side?", &playlist_editlist_copy_selected_group, psie);
			else
				playlist_editlist_copy_entry(psie, psie->editorhighline);
			break;
		}
		if(extkey == kb[184].c) {	// shift-F6
			if(psie->selected_files)
				display_textwin_openwindow_confirm(0, " Move entries ", "Move selected entries to the other side?", &playlist_editlist_move_selected_group, psie);
			else {
				playlist_editlist_copy_entry(psie, psie->editorhighline);
				playlist_editlist_delete_entry_manual(psie, psie->editorhighline);
			}
			break;
		}
		if(extkey == kb[185].c	// ctrl-gray-Ins
		   || extkey == kb[186].c	// ctrl-white-Ins
			) {
			playlist_editlist_copyside(psie);
			break;
		}
		funcbit_disable(psie->psio->editsidetype, PLT_SORTC_MANUALCOPY);	// !!!

		if(extkey == kb[187].c	// ctrl-gray-Del
		   || extkey == kb[188].c	// ctrl-white-Del
			) {
			struct playlist_side_info *psi = mvp->psi0 + 1;
			if(psi->editsidetype & PLT_DIRECTORY) {
				if(psi->psio->editsidetype & PLT_DIRECTORY)
					break;
				psi = psi->psio;
			} else {
				if(psi->psio->editsidetype & PLT_DOOMQUEUE)
					psi = psi->psio;
			}
			playlist_clear_side(psi);
			playlist_disable_side_full(psi);
			break;
		}

		if(extkey == kb[190].c) {	// alt-gray '/'
			if(mpxplay_control_fastlist_step(mvp, -1))
				break;
		}
		if(extkey == kb[191].c) {	// alt-gray '*'
			if(mpxplay_control_fastlist_step(mvp, +1))
				break;
		}

		if(extkey == kb[192].c	// white ins
		   || extkey == kb[193].c) {	// gray ins
			struct playlist_entry_info *pei = psie->editorhighline;
			if((pei->entrytype != DFT_UPDIR) && (pei->entrytype != DFT_UPLIST)) {
				funcbit_inverse(pei->infobits, PEIF_SELECTED);
				if(pei->infobits & PEIF_SELECTED)
					psie->selected_files++;
				else if(psie->selected_files)
					psie->selected_files--;
			}
			playlist_editorhighline_seek(psie, +1, SEEK_CUR);
			break;
		}
		if(extkey == kb[194].c) {	// alt-gray-'+'
			playlist_editlist_groupselect_open(psie, 0);
			break;
		}
		if(extkey == kb[195].c) {	// alt-gray-'-'
			playlist_editlist_groupselect_open(psie, 1);
			break;
		}
		if(extkey == kb[196].c) {	// alt-gray-'*' (if no fastlists)
			playlist_editlist_group_invert_selection(psie);
			break;
		}
		if(extkey == kb[197].c) {	// ctrl-gray-'+'
			playlist_editlist_group_select_all(psie);
			break;
		}
		if(extkey == kb[198].c) {	// ctrl-gray-'-'
			playlist_editlist_group_unselect_all(psie);
			break;
		}

		if(extkey == kb[200].c	// j
		   || extkey == kb[201].c) {	// J
			playlist_jukebox_switch(mvp);
			break;
		}
		if(extkey == kb[202].c) {	// i
			playlist_editlist_insert_index(mvp);
			break;
		}
		if(extkey == kb[203].c) {	// ctrl-i
			playlist_editlist_delete_index(mvp);
			break;
		}
		selected = 0;
	} while(0);
	funcbit_disable(psie->psio->editsidetype, PLT_SORTC_MANUALCOPY);	// !!!
	if(selected) {
		refdisp |= selected;
		return 1;
	}
//--------------------------------------------------------------------------
	selected = RDT_INIT_FULL | RDT_BROWSER | RDT_EDITOR | RDT_OPTIONS | RDT_HEADER | RDT_ID3INFO;
	do {
		if(extkey == kb[210].c) {	// ctrl-d
			if(!(crossfadepart & CROSS_OUT)) {
				keyboard_open_dosshell(mvp);
				break;
			}
		}
		if(displaymode) {
			if(extkey == kb[220].c) {	// g
				if(displaymode & DISP_GRAPHICAL)
					mpxplay_display_switch_to_textmode(mvp);
				else
					mpxplay_display_switch_to_graphmode(mvp);
				break;
			}
		}
		if((displaymode & DISP_FULLSCREEN) && !(displaymode & DISP_GRAPHICAL)) {
			if(extkey == kb[211].c) {	// shift-F9
				funcbit_inverse(displaymode, DISP_NOFULLEDIT);
				extkey = kb[212].c;
			}
			if(extkey == kb[212].c) {	// alt-F9
				funcbit_inverse(displaymode, DISP_50LINES);
				if(displaymode & DISP_50LINES)
					pds_textdisplay_setresolution(50);
				else
					pds_textdisplay_setresolution(25);
				break;
			}
			if(extkey == kb[213].c) {	// ctrl-F9
				funcbit_inverse(displaymode, DISP_NOFULLEDIT);
				break;
			}
			selected = RDT_INIT_ANABRO | RDT_BROWSER;
			if(extkey == kb[214].c	// a
			   || extkey == kb[215].c) {	// A
				funcbit_inverse(displaymode, DISP_ANALISER);
				break;
			}
			selected = RDT_INIT_BROWS | RDT_INIT_EDIT | RDT_RESET_EDIT | RDT_BROWSER | RDT_EDITOR;
			if(extkey == kb[216].c) {	// alt-up
				display_editor_resize_y(-3);
				break;
			}
			if(extkey == kb[217].c) {	// alt-down
				display_editor_resize_y(3);
				break;
			}
			selected = RDT_INIT_EDIT | RDT_EDITOR;
			if(extkey == kb[218].c) {	// alt-gray-left
				if(display_editor_resize_x(mvp, -1))
					break;
			}
			if(extkey == kb[219].c) {	// alt-gray-right
				if(display_editor_resize_x(mvp, +1))
					break;
			}
		}
		selected = 0;
	} while(0);
	if(selected) {
		refdisp |= selected;
		return 1;
	}

	do {
		if(extkey == kb[230].c) {	// F1
			display_help_window();
			break;
		}
		if(extkey == kb[231].c) {	// F2
			playlist_savelist_manual_save(mvp);
			break;
		}
		if(extkey == kb[232].c) {	// alt-F1
			playlist_loaddir_select_drive(mvp, 0);
			break;
		}
		if(extkey == kb[233].c) {	// alt-F2
			playlist_loaddir_select_drive(mvp, 1);
			break;
		}
		if(extkey == kb[234].c) {	// F7
			playlist_loaddir_makedir_open(psie);
			break;
		}
		if(extkey == kb[235].c) {	// ctrl-f
			playlist_loaddir_ftpurl_open(psie);
			break;
		}
		if(extkey == kb[236].c) {	// ctrl-k
			playlist_loaddir_ftpurl_close(psie);
			break;
		}

		if(extkey == kb[240].c) {	// ctrl-g
			LCD_page_select(mvp, -1);
			break;
		}
		if(extkey == kb[241].c) {	// 0xff10
			LCD_page_select(mvp, 0);
			break;
		}
		if(extkey == kb[242].c) {	// 0xff11
			LCD_page_select(mvp, 1);
			break;
		}
		if(extkey == kb[243].c) {	// 0xff12
			LCD_page_select(mvp, 2);
			break;
		}
		if(extkey == kb[244].c) {	// 0xff13
			LCD_page_select(mvp, 3);
			break;
		}
		return 0;
	} while(0);

	return 1;
}

static int keyboard_exit_shuttype;

static void keyboard_execute_exit(struct mainvars *mvp)
{
	mvp->partselect = 0;
	if(keyboard_exit_shuttype >= 0)
		shutdownatx_enabled = keyboard_exit_shuttype;
}

static void keyboard_confirm_exit(struct mainvars *mvp, int shutdowntype)
{
	keyboard_exit_shuttype = shutdowntype;
	if(desktopmode & DTM_CONFIRM_EXIT)
		if(display_textwin_openwindow_confirm(0, " Exit ", "Do you want to exit from Mpxplay?", &keyboard_execute_exit, mvp))
			return;
	keyboard_execute_exit(mvp);
}

static void keyboard_open_dosshell(struct mainvars *mvp)
{
	unsigned int mpxmaxy, mpxgraphmode;

	funcbit_enable(intsoundcontrol, INTSOUND_DOSSHELL);

	mpxgraphmode = displaymode & DISP_GRAPHICAL;
	if(mpxgraphmode)
		mpxplay_display_switch_to_textmode(mvp);

	pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_NORMAL);
	if(displaymode & DISP_FULLSCREEN) {
		mpxmaxy = textscreen_maxy;
		pds_textdisplay_vidmem_restore();
	}

	if(dosshellprg != NULL) {
		system(dosshellprg);
	} else {
		const char *comspec = getenv("COMSPEC");
		if(comspec != NULL)
			system(comspec);
		else
			system("command.com");
	}

	newfunc_errorhnd_int24_init();	// it seems dos4g doesn't restore my settings (dos32a works properly)

	if(displaymode & DISP_FULLSCREEN) {
		pds_textdisplay_vidmem_save();
		pds_textdisplay_setresolution(mpxmaxy);
	}
	pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_HIDDEN);

	if(mpxgraphmode)
		mpxplay_display_switch_to_graphmode(mvp);

	funcbit_disable(intsoundcontrol, INTSOUND_DOSSHELL);
}

//-------------------------------------------------------------------------
void check_dosshellstart(void)
{
	if(intsoundconfig & INTSOUND_DOSSHELL) {
		funcbit_disable(intsoundconfig, INTSOUND_DOSSHELL);
		pds_pushkey(kb[210].c);
	}
}

//--------------------------------------------------------------------
// numbers 1-9

#define SONGNUM_EXECUTE_DELAY 30	// 20 = 1 sec (use 20 for keyboard, 30-40 for IR)

static unsigned int mcks_songnum, mcks_digitpow10 = 1, mcks_zeroes;

static void mcks_execute(struct mainvars *mvp)
{
	struct playlist_side_info *psi;

	if(mvp->psie->psio->editsidetype & PLT_DOOMQUEUE)
		psi = mvp->psie;
	else
		psi = mvp->psip;

	if((psi->lastentry >= psi->firstsong) && mcks_songnum) {
		unsigned int allsongs = psi->lastentry - psi->firstsong;

		if((mcks_songnum - 1) <= allsongs) {
			mvp->newsong = psi->firstsong + (mcks_songnum - 1);
			if(!(playcontrol & PLAYC_RUNNING) && !mvp->frp0->infile_funcs && (mvp->aktfilenum < mvp->psip->firstsong)) {
				crossfade_reset(mvp);
				funcbit_enable(playcontrol, PLAYC_STARTNEXT);
			}
		}
	}
	mpxplay_control_keyboard_songnum_reset();
	mvp->newfilenum = NULL;
}

static unsigned int mpxplay_control_keyboard_songnum_check(unsigned int extkey, struct mainvars *mvp)
{
	unsigned int allsongs;
	struct playlist_side_info *psi;
	char sout[30], stmp[16];

	if(mvp->psie->psio->editsidetype & PLT_DOOMQUEUE)
		psi = mvp->psie;
	else
		psi = mvp->psip;

	if(psi->lastentry < psi->firstsong)
		goto err_out_mcks;

	allsongs = psi->lastentry - psi->firstsong + 1;
	if(extkey < 0xffff) {		// a real keycode
		extkey &= 0xff;
		if((extkey >= '0') && (extkey <= '9')) {
			extkey -= '0';
			mcks_songnum = mcks_songnum * 10 + extkey;
			if(!mcks_songnum && (mcks_zeroes < 10))	// int32 limit
				mcks_zeroes++;
			mcks_digitpow10 *= 10;

			pds_memset(stmp, '0', mcks_zeroes);
			stmp[mcks_zeroes] = 0;
			if(mcks_songnum)
				sprintf(sout, "Songnum: %s%d", stmp, mcks_songnum);
			else
				sprintf(sout, "Songnum: %s", stmp);
			display_static_message(0, 0, sout);
			display_static_message(1, 0, "");

			if((mcks_songnum * 10 > allsongs) || (mcks_digitpow10 > allsongs)) {
				mcks_execute(mvp);	// execute immediately
				return 0;
			} else {
				mpxplay_timer_addfunc(&mcks_execute, mvp, MPXPLAY_TIMERTYPE_WAKEUP, mpxplay_timer_secs_to_counternum(SONGNUM_EXECUTE_DELAY) / 20);
				//playlist_editorhighline_set(psi,psi->firstsong+(mcks_songnum-1));
			}
			return 1;
		}
	}

  err_out_mcks:
	if(mcks_songnum || mcks_zeroes) {
		mpxplay_control_keyboard_songnum_reset();
		clear_static_message();
	}
	return 0;
}

void mpxplay_control_keyboard_songnum_reset(void)
{
	if(mcks_songnum || mcks_zeroes)
		clear_static_message();
	mpxplay_timer_deletefunc(&mcks_execute, NULL);
	mcks_songnum = mcks_zeroes = 0;
	mcks_digitpow10 = 1;
}

//--------------------------------------------------------------------------
// ALT-letter ('a' - 'z')

static void mpxplay_control_keyboard_id3search_check(unsigned int extkey, struct mainvars *mvp)
{
	static char alt_table[53] = "qwertyuiop[]  asdfghjkl;'\\  zxcvbnm,./  1234567890-=";
	static char searchstring[35];
	static unsigned int s_init;
	struct playlist_side_info *psi = mvp->psie;
	struct playlist_entry_info *pei;
	struct playlist_entry_info *search_begin, *search_end;
	int search_step;			// -1 or +1 (up/down arrows)
	unsigned int scancode, keycode, found, retry;
	unsigned int ss_len, as_len, ts_len;	// searchstring_len,artiststring_len,titlestring_len
	char newchar, *s_titlep, temps[35], sout[50];

	search_begin = psi->editorhighline;
	search_end = psi->lastentry + 1;	// +1 : i!=search_end
	search_step = 1;
	ss_len = pds_strlen(searchstring);

	if(s_init) {
		switch (extkey) {
		case 0x1c0a:			// ctrl-ENTER
		case 0x3f00:			// F5
		case KEY_TAB:
		case KEY_ENTER1:
		case KEY_ENTER2:
			pds_pushkey(extkey);	// push back the keycode for the keyboard_primary (start song at enter)
			goto mckic_close;
		case KEY_ESC:
			funcbit_disable(psi->editsidetype, PLT_SORTC_MAGNETHIGHLINE);
			goto mckic_close;
		case 0x4b00:			// white left
		case 0x4be0:			// gray left
		case KEY_BACKSPACE:
			if(ss_len)
				searchstring[ss_len - 1] = 0;
			extkey = search_step = 0;
			break;
		case KEY_UP_GRAY:
		case KEY_UP_WHITE:
			if(psi->editorhighline > psi->firstentry) {
				search_begin = psi->editorhighline - 1;
				search_end = psi->firstentry - 1;	// -1 : i!=search_end
				search_step = -1;
			} else
				search_step = 0;
			extkey = 0;
			break;
		case KEY_DOWN_GRAY:
		case KEY_DOWN_WHITE:
			if(psi->editorhighline < psi->lastentry)
				search_begin = psi->editorhighline + 1;
			else
				search_step = 0;
			extkey = 0;
			break;
		}
	}

	if(extkey && (ss_len < 30)) {
		scancode = extkey >> 8;
		keycode = extkey & 0xff;

		if(!keycode && ((scancode >= 0x10 && scancode <= 0x37) || (scancode >= 0x78 && scancode <= 0x83))) {	// ALT-key
			if((scancode >= 0x78) && (scancode <= 0x83))
				scancode -= 0x40;
			scancode -= 0x10;
			newchar = alt_table[scancode];
		} else {
			if(!s_init)			// 1st character requires an ALT
				return;
			if(keycode < 0x20)	// control codes
				return;
			newchar = keycode;
		}
		searchstring[ss_len] = newchar;
		searchstring[++ss_len] = 0;
		s_init = 1;
		retry = 2;
		mpxplay_control_keyboard_set_topfunc(&mpxplay_control_keyboard_id3search_check);
	} else
		retry = 1;

	funcbit_enable(psi->editsidetype, PLT_SORTC_MAGNETHIGHLINE);

	if(search_step != 0) {
		pds_strcpy(temps, searchstring);
		s_titlep = pds_strchr(temps, ':');
		if(s_titlep) {
			*s_titlep++ = 0;
			ts_len = pds_strlen(s_titlep);
		} else
			ts_len = 0;
		as_len = pds_strlen(temps);

		found = 0;
		do {
			if(!s_titlep) {		// search in all metadata (+filename)
				for(pei = search_begin; pei != search_end; pei += search_step) {
					unsigned int i;
					for(i = 0; i <= I3I_MAX; i++) {
						if(pei->id3info[i] && pds_strstri(pei->id3info[i], temps)) {
							found = 1;
							break;
						}
					}
					if(!found) {
						if(pds_strstri(pei->filename, temps))
							found = 1;
					}
					if(found) {
						playlist_editorhighline_set(psi, pei);
						break;
					}
				}
			} else {			// search in artist:title only
				for(pei = search_begin; pei != search_end; pei += search_step) {
					char *ipa, *ipt;
					if(GET_HFT(pei->entrytype) == HFT_DFT || (!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE])) {
						ipa = ipt = pds_getfilename_from_fullname(pei->filename);
					} else {
						ipa = pei->id3info[I3I_ARTIST];
						ipt = pei->id3info[I3I_TITLE];
						if(!ipa)
							ipa = ipt;
						if(!ipt)
							ipt = ipa;
					}
					if(!as_len || pds_strstri(ipa, temps)) {	// title search only || artist found
						if(!ts_len || pds_strstri(ipt, s_titlep)) {	// artist search only || title found
							playlist_editorhighline_set(psi, pei);
							found = 1;
							break;
						}
					}
				}
			}
			if(found)
				break;
			if(!(--retry))
				break;
			search_end = psi->firstentry - 1;
			search_step = -1;
		} while(1);
		refdisp |= RDT_EDITOR;
	}

	sprintf(sout, "Search: %s_ ", searchstring);
	display_static_message(0, 0, sout);
	display_static_message(1, 0, "");
	//display_static_message(1,0,"Press ESC to finish (exit from) the search");
	return;

  mckic_close:
	searchstring[0] = 0;
	clear_static_message();
	s_init = 0;
	mpxplay_control_keyboard_set_topfunc(NULL);
}
