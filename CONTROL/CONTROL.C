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
//function: control functions main
//          and global variable definition/initialization

#include <newfunc\newfunc.h>
#include "control.h"
#include "cntfuncs.h"
#include "mpxinbuf.h"
#include <au_cards\au_cards.h>
#include <au_mixer\au_mixer.h>
#include <display\display.h>
#include <display\visualpi.h>
#include <videoout\videoout.h>

#define MPXPLAY_CMDLG_INITSIZE 16

static unsigned int cmdlg_alloc(unsigned int newsize);
static void mpxplay_control_progexectime(struct mainvars *mvp);
static void mpxplay_printhelp(void);

frame fr[3];
mainvars mvps;
mpxplay_audioout_info_s au_infos;
static mpxplay_videoout_info_s videoout_infos;
static playlist_side_info playlist_sideinfos[PLAYLIST_MAX_SIDES];
static crossfade_info cf_infos;
static display_visual_data_s visual_datas;
struct desktoppos dtp;

//au_mixer
extern unsigned int MIXER_controlbits;
extern int MIXER_var_usehq, MIXER_var_volume, MIXER_var_balance, MIXER_var_mute_voldiv;
extern int MIXER_var_swapchan, MIXER_var_autovolume, MIXER_var_limiter_overflow;
static int MIXER_varcfg_surround, MIXER_varcfg_speed;

//display\visualpi.c
extern char *display_visual_plugin_selectname;
extern unsigned int display_visual_plugin_starttime;
//newfunc\drivehnd.c
extern unsigned int uselfn;
//playlist\textconv.c
extern char *textconv_codepage_sourcename, *textconv_codepage_targetname;
//diskdriv\drv_ftp.c
extern unsigned long mpxplay_diskdrive_drvftp_config;

//global
char *id3loadname = "!FILES", *id3savename = "!FILESS";
char *m3usavename = "MPXPLAY.M3U", *mxusavename = "MPXPLAY.MXU", *cuesavename = "MPXPLAY.CUE";
char *dosshellprg, *drivescanletters, *id3filterkeyword;
char *id3tagset[I3I_MAX + 1];
char *freeopts[MAXFREEOPTS];
char cp_winchars[256], cp_doschars[256];
char sortorder_string[256];
unsigned int playlist_max_filenum_list;

unsigned int playlistload, playlistsave, loadid3tag, loadid3list, id3savefields;
unsigned int preloadinfo, saveid3list, writeid3tag, preloadinf_cfg;
unsigned int id3textconv, sortcontrol;
unsigned int crossfadepart, intsoundconfig, intsoundcontrol;
unsigned int prebuffertype, prebufferblocks, prebuffermegabytes;
unsigned int outmode, cdw_control_flags, cdw_control_speed;
unsigned int playcontrol, playreplay, playrand;
unsigned int playstartsong, playcountsong;
int playstartlist, playstartframe, playstartpercent, control_startup_type_override;
char *playstarttime, *playcounttime;
unsigned int playcountframe, playcountpercent;
unsigned int fullelapstime;
unsigned int channelmode;
unsigned int SOUNDLIMITvol, SOUNDLIMITbegin, SOUNDLIMITlen;
unsigned int displaymode, desktopmode, desktopmod_mpxini;
unsigned int refdisp, editorsideborder, analtabnum, timemode, mouse_on;
unsigned int useglvariables, shutdownatx_enabled;
unsigned long allcpuusage, allcputime;
unsigned int videocontrol, stream_select_audio;
unsigned int aucards_select_devicenum;	//
int aucards_select_config;		// !!! later move it to au_infos

static unsigned int mpxplay_progtimebegin, mpxplay_playtimestart, mpxplay_progtimelen;
static char *p_playtimestart, *p_progtimelen, *p_progtimeexit;
static unsigned int playendframe, backbuffermegabytes;
static unsigned int do_printhelp, do_listdlls, do_listvideooutmodes;

static topt **cmdl_groups;
static unsigned int cmdlg_size, cmdlg_entries;

//note: ARG_CHAR is allways pointer in opts[]

static topt main_opts[] = {
	{"@", ARG2 | ARG_CHAR | ARG_OR, &playlistload, PLL_LOADLIST, &freeopts[OPT_INPUTFILE]},
	{"@d", ARG2 | ARG_CHAR, &playlistload, PLL_LOADLIST | PLL_DOOMBOX, &freeopts[OPT_INPUTFILE]},
	{"@i", ARG_OR, &playlistload, PLL_LOADLIST | PLL_STDIN, 0},
	{"@s", ARG2 | ARG_CHAR, &playlistsave, PLST_MANUAL, &m3usavename},
	{"@sx", ARG2 | ARG_CHAR, &playlistsave, PLST_MANUAL | PLST_MXU, &mxusavename},
	{"@sw", ARG2 | ARG_CHAR, &playlistsave, PLST_MANUAL | PLST_EXTM3U, &m3usavename},
	{"@sc", ARG2 | ARG_CHAR, &playlistsave, PLST_MANUAL | PLST_CUE, &cuesavename},
	{"t", ARG_OR, &outmode, OUTMODE_TYPE_TEST, 0},
	{"o", ARG_OR, &outmode, OUTMODE_TYPE_FILE, 0},
	{"of", ARG1 | ARG_NUM, &au_infos.freq_set, 0, 0},
	{"oc", ARG1 | ARG_NUM, &au_infos.chan_set, 0, 0},
	{"ob", ARG1 | ARG_NUM, &au_infos.bits_set, 0, 0},
	{"obs", ARG_OR, &outmode, OUTMODE_CONTROL_FILE_BITSTREAMOUT, 0},
	{"obf", ARG_OR, &outmode, OUTMODE_CONTROL_FILE_FLOATOUT, 0},
	{"oni", ARG_OR, &outmode, OUTMODE_CONTROL_FILE_TAGLFN, 0},
	{"oat", ARG_OR, &au_infos.card_controlbits, AUINFOS_CARDCNTRLBIT_AUTOTAGGING, 0},
	{"db", 0, &playlist_sideinfos[0].editsidetype, 0, 0},
	{"db2", 0, &playlist_sideinfos[1].editsidetype, PLT_DIRECTORY, 0},
	{"dd", ARG_OR, &playlistload, PLL_DOOMBOX, 0},
	{"ds", ARG2 | ARG_CHAR | ARG_OR, &playlistload, PLL_DRIVESCAN, &drivescanletters},
	{"drfc", ARG1 | ARG_NUM, &mpxplay_diskdrive_drvftp_config, 0, 0},
	{"bn", 0, &prebuffertype, PREBUFTYPE_NONE, 0},
	{"bs", 0, &prebuffertype, PREBUFTYPE_SHORTRING, 0},
	{"bp", 0, &prebuffertype, PREBUFTYPE_LONGRING | PREBUFTYPE_INT, 0},
	{"bb", ARG_OR, &prebuffertype, PREBUFTYPE_LONGRING | PREBUFTYPE_BACK | PREBUFTYPE_INT, 0},
	{"bpn", ARG_OR, &prebuffertype, PREBUFTYPE_LONGRING | PREBUFTYPE_INT | PREBUFTYPE_PRELOADNEXT, 0},
	{"bl", 0, &prebuffertype, PREBUFTYPE_FULL | PREBUFTYPE_INT, 0},
	{"bni", ARG_OR, &intsoundconfig, INTSOUND_NOINTDEC, 0},
	{"bn8", ARG_OR, &intsoundconfig, INTSOUND_NOINT08, 0},
	{"bpb", ARG1 | ARG_NUM, &prebufferblocks, 0, 0},
	{"bpm", ARG1 | ARG_NUM, &prebuffermegabytes, 0, 0},
	{"bbm", ARG2 | ARG_NUM | ARG_OR, &prebuffertype, PREBUFTYPE_LONGRING | PREBUFTYPE_BACK | PREBUFTYPE_INT, &backbuffermegabytes},
	{"f0", 0, &displaymode, 0, 0},
	{"ff", 0, &displaymode, DISP_FRAMES, 0},
	{"fl", 0, &displaymode, DISP_TIMEPOS, 0},
	{"fe", ARG_AND, &displaymode, ~DISP_NOFULLEDIT, 0},
	{"fs", ARG_AND, &displaymode, ~DISP_50LINES, 0},
	{"v", ARG_OR, &displaymode, DISP_VERBOSE, 0},
	{"inl", 0, &preloadinfo, PLI_NOTLOAD, 0},
	{"irl", 0, &preloadinfo, PLI_PRELOAD, 0},
	{"ipl", 0, &preloadinfo, PLI_PLAYLOAD, 0},
	{"idl", 0, &preloadinfo, PLI_DISPLOAD, 0},
	{"ihl", 0, &preloadinfo, PLI_EHLINELOAD, 0},
	{"in", 0, &loadid3tag, ID3LOADMODE_NONE, 0},
	{"inf", 0, &loadid3tag, ID3LOADMODE_NOFILE, 0},
	{"if", ARG1 | ARG_CHAR, &id3filterkeyword, 0, 0},
	{"io", ARG1 | ARG_NUM, &(playlist_sideinfos[0].id3ordertype[0]), 0, 0},
	{"io2", ARG1 | ARG_NUM, &(playlist_sideinfos[0].id3ordertype[1]), 0, 0},
	{"io3", ARG1 | ARG_NUM, &(playlist_sideinfos[0].id3ordertype[2]), 0, 0},
	{"io4", ARG1 | ARG_NUM, &(playlist_sideinfos[0].id3ordertype[3]), 0, 0},
	{"iod", ARG_OR, &sortcontrol, SORTC_DESCENDING, 0},
	{"il", ARG2 | ARG_CHAR, &loadid3list, ID3LISTTYPE_LOCAL, &id3loadname},
	{"ig", ARG2 | ARG_CHAR, &loadid3list, ID3LISTTYPE_GLOBAL, &id3loadname},
	{"is", ARG2 | ARG_CHAR, &saveid3list, 1, &id3savename},
	{"ist", ARG2 | ARG_NUM, &id3savefields, 2, &id3savefields},
	{"iw", 0, &writeid3tag, 1, 0},
	{"ita", ARG1 | ARG_CHAR, &id3tagset[I3I_ARTIST], 0, 0},
	{"itt", ARG1 | ARG_CHAR, &id3tagset[I3I_TITLE], 0, 0},
	{"itl", ARG1 | ARG_CHAR, &id3tagset[I3I_ALBUM], 0, 0},
	{"ity", ARG1 | ARG_CHAR, &id3tagset[I3I_YEAR], 0, 0},
	{"itc", ARG1 | ARG_CHAR, &id3tagset[I3I_COMMENT], 0, 0},
	{"itg", ARG1 | ARG_CHAR, &id3tagset[I3I_GENRE], 0, 0},
	{"itn", ARG1 | ARG_CHAR, &id3tagset[I3I_TRACKNUM], 0, 0},
	{"scs", ARG1 | ARG_CHAR, &au_infos.card_selectname, 0, 0},
	{"scd", ARG1 | ARG_NUM, &aucards_select_devicenum, 0, 0},
	{"scc", ARG1 | ARG_NUM, &aucards_select_config, 0, 0},
	{"sct", ARG2 | ARG_CHAR | ARG_OR, &au_infos.card_controlbits, AUINFOS_CARDCNTRLBIT_TESTCARD, &au_infos.card_selectname},
	{"ddma", ARG_OR, &au_infos.card_controlbits, AUINFOS_CARDCNTRLBIT_DOUBLEDMA, 0},
	{"scm", ARG_OR, &au_infos.card_controlbits, AUINFOS_CARDCNTRLBIT_MIDASMANUALCFG, 0},
	{"scv", ARG1 | ARG_NUM, &au_infos.card_master_volume, 0, 0},
	{"scvom", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_MASTER], 0, 0},
	{"scvop", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_PCM], 0, 0},
	{"scvoh", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_HEADPHONE], 0, 0},
	{"scvos", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_SPDIFOUT], 0, 0},
	{"scvim", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_MICIN], 0, 0},
	{"scvil", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_LINEIN], 0, 0},
	{"scvic", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_CDIN], 0, 0},
	{"scvia", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_AUXIN], 0, 0},
	{"sctr", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_TREBLE], 0, 0},
	{"scbs", ARG1 | ARG_NUM, &au_infos.card_mixer_values[AU_MIXCHAN_BASS], 0, 0},
	{"cl", 0, &channelmode, CHM_LEFT, 0},
	{"cm", 0, &channelmode, CHM_DOWNMIX, 0},
	{"csa", ARG1 | ARG_NUM, &stream_select_audio, 0, 0},
	{"sl", ARG1 | ARG_NUM, &SOUNDLIMITvol, 0, 0},
	{"sv", ARG1 | ARG_NUM, &MIXER_var_volume, 0, 0},
	{"sva", 0, &MIXER_var_autovolume, 1, 0},
	{"sr", ARG1 | ARG_NUM, &MIXER_varcfg_surround, 0, 0},
	{"sp", ARG1 | ARG_NUM, &MIXER_varcfg_speed, 0, 0},
	{"mxlo", ARG1 | ARG_NUM, &MIXER_var_limiter_overflow, 0, 0},
	{"mxmd", ARG1 | ARG_NUM, &MIXER_var_mute_voldiv, 0, 0},
	{"psu", ARG1 | ARG_NUM, &control_startup_type_override, 0, 0},
	{"pss", ARG2 | ARG_NUM | ARG_OR, &playcontrol, PLAYC_STARTNEXT, &playstartsong},
	{"psf", ARG1 | ARG_NUM, &playstartframe, 0, 0},
	{"pst", ARG1 | ARG_CHAR, &playstarttime, 0, 0},
	{"psp", ARG1 | ARG_NUM, &playstartpercent, 0, 0},
	{"pcs", ARG1 | ARG_NUM, &playcountsong, 0, 0},
	{"pcf", ARG1 | ARG_NUM, &playcountframe, 0, 0},
	{"pct", ARG1 | ARG_CHAR, &playcounttime, 0, 0},
	{"pcp", ARG1 | ARG_NUM, &playcountpercent, 0, 0},
	{"pef", ARG1 | ARG_NUM, &playendframe, 0, 0},
	{"pslf", ARG2 | ARG_NUM | ARG_OR, &playlistload, PLL_FASTLIST, &playstartlist},
	{"ppa", ARG_OR, &playcontrol, PLAYC_PAUSEALL, 0},
	{"ppn", ARG_OR, &playcontrol, PLAYC_PAUSENEXT, 0},
	{"phs", ARG_OR, &playcontrol, PLAYC_HIGHSCAN, 0},
	{"prn", 0, &playrand, 1, 0},
	{"prn2", 0, &playrand, 2, 0},
	{"pre", 0, &playreplay, REPLAY_LIST, 0},
	{"pre1", 0, &playreplay, REPLAY_SONG, 0},
	{"cf", 0, &cf_infos.usecrossfade, 1, 0},
	{"cft", ARG1 | ARG_NUM, &cf_infos.crossfadetype, 0, 0},
	{"cfo", ARG1 | ARG_NUM, &cf_infos.crossfade_out_len, 0, 0},
	{"cfp", ARG1 | ARG_NUM, &cf_infos.crossfadepoint, 0, 0},
	{"cfi", ARG1 | ARG_NUM, &cf_infos.crossfade_in_len, 0, 0},
	{"cfl", ARG1 | ARG_NUM, &cf_infos.crossfadelimit, 0, 0},
	{"8", ARG_OR, &id3textconv, ID3TEXTCONV_CODEPAGE, 0},
	{"8f", ARG_OR, &id3textconv, ID3TEXTCONV_FILENAME, 0},
	{"8u", ARG_OR, &id3textconv, ID3TEXTCONV_UTF8, 0},
	{"8ua", ARG_OR, &id3textconv, ID3TEXTCONV_UTF_AUTO, 0},
	{"8uv", ARG_OR, &id3textconv, (ID3TEXTCONV_VALIDATE | ID3TEXTCONV_UTF_AUTO), 0},
	{"8w", ARG_OR, &id3textconv, ID3TEXTCONV_GET_WINCP, 0},
	{"8b", ARG_OR, &id3textconv, ID3TEXTCONV_CP_BACK, 0},
	{"8ucp", ARG2 | ARG_CHAR | ARG_OR, &id3textconv, ID3TEXTCONV_UTF_AUTO, &textconv_codepage_sourcename},
	{"8ccp", ARG1 | ARG_CHAR, &textconv_codepage_targetname, 0, 0},
	{"ebs", ARG1 | ARG_NUM, &editorsideborder, 0, 0},
	{"x", ARG1 | ARG_CHAR, &dosshellprg, 0, 0},
	{"xs", ARG_OR, &intsoundconfig, INTSOUND_DOSSHELL, 0},
	{"xr", ARG_OR, &intsoundconfig, INTSOUND_TSR, 0},
	{"xas", 0, &shutdownatx_enabled, 1, 0},
	{"xts", ARG1 | ARG_CHAR, &p_playtimestart, 0, 0},
	{"xtl", ARG1 | ARG_CHAR, &p_progtimelen, 0, 0},
	{"xte", ARG1 | ARG_CHAR, &p_progtimeexit, 0, 0},
	{"xel", ARG_OR, &playcontrol, PLAYC_EXITENDLIST, 0},
	{"xce", ARG_OR, &desktopmode, DTM_CONFIRM_EXIT, 0},
	{"vps", ARG1 | ARG_CHAR, &display_visual_plugin_selectname, 0, 0},
	{"vpt", ARG1 | ARG_NUM, &display_visual_plugin_starttime, 0, 0},
	{"vos", ARG1 | ARG_CHAR, &videoout_infos.config_screenhandler_name, 0, 0},
	{"vom", ARG1 | ARG_NUM | ARG_HEX, &videoout_infos.config_mode, 0, 0},
	{"vox", ARG1 | ARG_NUM, &videoout_infos.config_res_x, 0, 0},
	{"voy", ARG1 | ARG_NUM, &videoout_infos.config_res_y, 0, 0},
	{"vob", ARG1 | ARG_NUM, &videoout_infos.config_bpp, 0, 0},
	{"volm", 0, &do_listvideooutmodes, 1, 0},
	{"dlls", 0, &do_listdlls, 1, 0},
	{"?", 0, &do_printhelp, 1, 0},
	{"h", 0, &do_printhelp, 1, 0},
	{0, 0, 0, 0, 0}
};

static mpxini_var_s gl[] = {
	{"UseVariables", &useglvariables, 0},	//0.
//file/buffer
	{"Prebuffertype", &prebuffertype, 0},
	{"Intsoundcfg", &intsoundconfig, 0},
	{"Bufferblocks", &prebufferblocks, 0},
//playcontrol
	{"PlaySongCount", &playcountsong, 0},
	{"PlayRandom", &playrand, ARG_SAVE},
	{"PlayReplay", &playreplay, ARG_SAVE},
	{"PlayControl", &playcontrol, ARG_SAVE},
	{"SeekFrames", &mvps.seek_frames, 0},
//soundcard
	{"SoundcardName", &au_infos.card_selectname, ARG_CHAR | ARG_POINTER},
	{"SoundcardFreq", &au_infos.freq_set, 0},	//10.
	{"SoundcardChan", &au_infos.chan_set, 0},
	{"SoundcardBits", &au_infos.bits_set, 0},
	{"SoundcardVol", &au_infos.card_master_volume, 0},
//{"SoundcardVol" ,&au_infos.card_mixer_values[AU_MIXCHAN_MASTER][AU_MIXCHANFUNC_VOLUME],0},
	{"SoundcardTrbl", &au_infos.card_mixer_values[AU_MIXCHAN_TREBLE], ARG_SAVE},
	{"SoundcardBass", &au_infos.card_mixer_values[AU_MIXCHAN_BASS], ARG_SAVE},
//mixer
	{"MixerControl", &MIXER_controlbits, 0},
	{"HQmode", &MIXER_var_usehq, ARG_SAVE},
	{"SoundVolume", &MIXER_var_volume, ARG_SAVE},
	{"AutoVolume", &MIXER_var_autovolume, ARG_SAVE},
	{"Surround", &MIXER_varcfg_surround, ARG_SAVE},	//20.
	{"Speed", &MIXER_varcfg_speed, ARG_SAVE},
	{"Balance", &MIXER_var_balance, ARG_SAVE},
	{"Swapchan", &MIXER_var_swapchan, ARG_SAVE},
	{"SoundLimitVol", &SOUNDLIMITvol, 0},
	{"SoundLimitBeg", &SOUNDLIMITbegin, 0},
	{"SoundLimitLen", &SOUNDLIMITlen, 0},
	{"UseCrossfade", &cf_infos.usecrossfade, ARG_SAVE},
	{"CFtype", &cf_infos.crossfadetype, ARG_SAVE},
	{"CFlimit", &cf_infos.crossfadelimit, 0},
	{"CFpoint", &cf_infos.crossfadepoint, 0},	//30.
	{"CFoutlength", &cf_infos.crossfade_out_len, 0},
	{"CFinlength", &cf_infos.crossfade_in_len, 0},
//decoder
	{"ChannelMode", &channelmode, 0},
	{"CDWcontrol", &cdw_control_flags, 0},
	{"CDWspeed", &cdw_control_speed, 0},
//display
	{"Displaymode", &displaymode, ARG_SAVE},
	{"Desktopmode", &desktopmode, ARG_SAVE},
	{"TimeMode", &timemode, ARG_SAVE},
	{"EditSideBordr", &editorsideborder, ARG_SAVE},
	{"EditorBegin", &(dtp.relative_songposline), ARG_SAVE},	//40.
//playlist&id3tag
	{"MaxFilenames", &playlist_max_filenum_list, 0},
	{"Preloadinfo", &preloadinfo, 0},
	{"ID3ordertype", &(playlist_sideinfos[0].id3ordertype[0]), 0},	//ARG_SAVE},
	{"SortOrder", &sortorder_string[0], ARG_CHAR},
	{"PlayListSave", &playlistsave, 0},
	{"LoadID3tag", &loadid3tag, 0},
	{"LoadID3list", &loadid3list, 0},
	{"ID3savetype", &id3savefields, 0},
	{"Conv852437", &id3textconv, 0},
	{"WinChars", &cp_winchars[0], ARG_CHAR},	//50.
	{"DosChars", &cp_doschars[0], ARG_CHAR},
	{"UseLFN", &uselfn, 0},
//system
	{"ShutdownATX", &shutdownatx_enabled, 0},
	{NULL, NULL, 0}				//54.
};

void mpxplay_control_initvar(int argc, char *argv[], struct mainvars *mvp)
{
	unsigned int i, j;
	struct playlist_side_info *psi;
	struct crossfade_info *cfi;
	struct frame *frp0;

	freeopts[OPT_PROGNAME] = argv[0];

	mvp->frp0 = &fr[0];
	mvp->aui = &au_infos;
	mvp->aui->mvp = mvp;
	mvp->voi = &videoout_infos;
	mvp->cfi = &cf_infos;
	mvp->vds = &visual_datas;
	mvp->partselect = mvp->direction = mvp->foundfile = 1;
	mvp->idone = MPXPLAY_ERROR_INFILE_EOF;

	//----------------------------------------------------------------------
	psi = &playlist_sideinfos[0];
	mvp->psi0 = mvp->psie = mvp->psip = mvp->psil = psi;

	psi->editsidetype = PLT_DIRECTORY;
	psi->mvp = mvp;
	psi->psio = psi + 1;

	psi++;
	psi->mvp = mvp;
	psi->psio = mvp->psi0;

	//set default id3ordertype on all sides and on all keys
	psi = mvp->psi0;
	for(i = 0; i < PLAYLIST_MAX_SIDES; i++) {
		for(j = 0; j < PLAYLIST_MAX_ORDERKEYS; j++)
			*((unsigned int *)(&psi->id3ordertype[j])) = ID3ORDER_DISABLED;
		psi++;
	}
	//-----------------------------------------------------------------------
	cfi = mvp->cfi;
	cfi->crossfade_out_len = 250;
	cfi->crossfade_in_len = 200;
	cfi->crossfadepoint = 120;
	cfi->crossfadetype = CFT_FADEOUT;
	//-----------------------------------------------------------------------
	frp0 = &fr[0];
	frp0->allframes = 1;
	frp0->fro = &fr[1];
	fr[1].fro = frp0;
	//----------------------------------------------------------------------

	mvp->seek_frames = 50;

	playcountsong = 0x7fffffff;
	SOUNDLIMITvol = 5;
	SOUNDLIMITbegin = 400;
	SOUNDLIMITlen = 23;

	aucards_select_config = -1;
	AU_setmixer_init(mvp->aui);
	MIXER_init();

	MIXER_varcfg_surround = MIXER_getvalue("MIX_SURROUND");
	MIXER_varcfg_speed = MIXER_getvalue("MIX_SPEED");
	MIXER_var_usehq = 1;

	displaymode = DISP_TIMEPOS | DISP_VERBOSE | DISP_FULLSCREEN | DISP_NOFULLEDIT | DISP_ANALISER | DISP_50LINES;
	desktopmode = DTM_SONGPOS | DTM_LISTPOS | DTM_EDIT_ELEVATOR | DTM_EDIT_VERTICAL | DTM_EDIT_FULLPATH | DTM_EDIT_FULLTIME | DTM_EDIT_SONGTIME | DTM_EDIT_SONGNUM;	// =507
	editorsideborder = EDITOR_SIDE_SIZE_DEFAULT;
	loadid3tag = ID3LOADMODE_ALL;
	id3textconv = ID3TEXTCONV_UTF_AUTO;
	preloadinfo = PLI_DISPLOAD;
	prebuffertype = PREBUFTYPE_LONGRING | PREBUFTYPE_INT;
	channelmode = CHM_STEREO;
	cdw_control_flags = 1;
	outmode = OUTMODE_TYPE_AUDIO;
#ifdef __DOS__
	playlist_max_filenum_list = 9999;
#else
	playlist_max_filenum_list = 19999;
#endif

	id3savefields = IST_DEFAULT;
	useglvariables = allcpuusage = allcputime = 1;
	uselfn = USELFN_ENABLED;
	playstartlist = control_startup_type_override = -1;

	videocontrol = MPXPLAY_VIDEOCONTROL_DECODEVIDEO;

	cmdlg_alloc(MPXPLAY_CMDLG_INITSIZE);
}

static unsigned int cmdlg_alloc(unsigned int newsize)
{
	topt **newcmdlg = (topt **) calloc(newsize, sizeof(*cmdl_groups));
	if(!newcmdlg)
		return 0;
	if(cmdl_groups) {
		pds_memcpy((void *)newcmdlg, (void *)cmdl_groups, cmdlg_size * sizeof(*cmdl_groups));
		free(cmdl_groups);
		cmdl_groups = newcmdlg;
	} else {
		cmdl_groups = newcmdlg;
		cmdl_groups[0] = main_opts;
		cmdlg_entries = 1;
	}
	cmdlg_size = newsize;
	return 1;
}

static void cmdlg_free(void)
{
	if(cmdl_groups) {
		free(cmdl_groups);
		cmdl_groups = NULL;
	}
	cmdlg_size = cmdlg_entries = 0;
}

static void cmdlg_loaddlls(void)
{
#ifdef MPXPLAY_LINK_DLLLOAD
	mpxplay_module_entry_s *dll_found = NULL;
	do {
		dll_found = newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_CONTROL_CMDLINE, 0, NULL, dll_found);	// get next
		//fprintf(stdout,"dll:%8.8X sv:%4.4X\n",dll_found,dll_found->module_structure_version);
		if(dll_found && (dll_found->module_structure_version == MPXPLAY_DLLMODULEVER_CONTROL_CMDLINE)) {	// !!!
			if(cmdlg_entries >= cmdlg_size)
				if(!cmdlg_alloc(cmdlg_size * 2))
					break;
			cmdl_groups[cmdlg_entries] = (topt *) dll_found->module_callpoint;
			if(newfunc_dllload_disablemodule(0, 0, NULL, dll_found))	// we don't use it anymore
				cmdl_groups[cmdlg_entries] = NULL;	// dll has unloaded (rare)
			else
				cmdlg_entries++;	// dll has keeped
		}
	} while(dll_found);
#endif
}

void mpxplay_control_getcommandlineopts(int argc, char *argv[])
{
	unsigned int i, g, freeoptcount, found;
	topt *pointer;
	if(argc > 1) {
		freeoptcount = 1;		// 0. is the progname
		cmdlg_loaddlls();
		for(i = 1; i < argc; i++) {
			if(argv[i][0] == '-' || argv[i][0] == '/') {
				found = 0;
				for(g = 0; g < cmdlg_entries && !found; g++) {
					pointer = cmdl_groups[g];
					while(pointer->oname != NULL) {
						if(pds_stricmp(&argv[i][1], pointer->oname) == 0) {
							if(!(pointer->flags & ARG1) || ((pointer->flags & ARG2) == ARG2)) {
								if(pointer->flags & ARG_OR)
									*((int *)pointer->var) |= pointer->value;
								else if(pointer->flags & ARG_AND)
									*((int *)pointer->var) &= pointer->value;
								else
									*((int *)pointer->var) = pointer->value;
							}
							if((pointer->flags & ARG1) == ARG1) {
								char *source;
								i++;
								source = argv[i];
								if(i < argc && source && source[0] != '-' && source[0] != '/') {
									void *target = ((pointer->flags & ARG2) == ARG2) ? pointer->var2 : pointer->var;
									if(pointer->flags & ARG_CHAR)
										*((char **)target) = source;
									else if(pointer->flags & ARG_FLO)
										*((float *)target) = (float)atof(source);
									else if(pointer->flags & ARG_HEX)
										*((int *)target) = pds_atol16(source);
									else
										*((int *)target) = pds_atol(source);
								} else
									i--;
							}
							found = 1;
							break;
						}
						pointer++;
					}
				}
			} else if(freeoptcount < MAXFREEOPTS)
				freeopts[freeoptcount++] = &argv[i][0];
		}
	}
}

void mpxplay_control_checkvar(struct mainvars *mvp)
{
	struct playlist_side_info *psi;
	struct mpxplay_audioout_info_s *aui;
	struct crossfade_info *cfi;
	long i;

	if(do_listdlls) {
		newfunc_dllload_list_dlls();
		mpxplay_close_program(0);
	}
	if(do_listvideooutmodes) {
		mpxplay_videoout_listmodes(mvp->voi);
		mpxplay_close_program(0);
	}

	psi = mvp->psi0;

	loadid3tag &= (ID3LOADMODE_ALL | ID3LOADMODE_PREFER_LIST);
	if(loadid3list)
		funcbit_enable(loadid3tag, ID3LOADMODE_LIST);
	if(writeid3tag)
		saveid3list = 0;
	if(writeid3tag || saveid3list) {
		psi->editsidetype = PLT_DIRECTORY;
		displaymode = 0;
		preloadinfo = PLI_NOTLOAD;
		outmode = OUTMODE_TYPE_NULL;
		control_startup_type_override = 0;
	}

	if(outmode & OUTMODE_TYPE_TEST) {
		funcbit_disable(outmode, OUTMODE_TYPE_MASK & (~OUTMODE_TYPE_TEST));
		//funcbit_enable(intsoundconfig,INTSOUND_NOINT08); // ???
	}
	if(outmode & OUTMODE_TYPE_FILE) {
		funcbit_disable(outmode, OUTMODE_TYPE_MASK & (~OUTMODE_TYPE_FILE));
		funcbit_enable(intsoundconfig, INTSOUND_NOINT08);
		funcbit_disable(psi->editsidetype, PLT_DIRECTORY);
		control_startup_type_override = 0;
	}

	if(funcbit_test(prebuffertype, (PREBUFTYPE_BACK | PREBUFTYPE_PRELOADNEXT))) {
		funcbit_disable(prebuffertype, (PREBUFTYPE_FULL | PREBUFTYPE_SHORTRING));
		funcbit_enable(prebuffertype, PREBUFTYPE_LONGRING);
	}

	if(funcbit_test(prebuffertype, PREBUFTYPE_INT))
		funcbit_enable(intsoundconfig, INTSOUND_DECODER);

	if(!prebuffertype || funcbit_test(prebuffertype, PREBUFTYPE_SHORTRING))
		funcbit_disable(intsoundconfig, INTSOUND_DECODER);

	if(funcbit_test(intsoundconfig, (INTSOUND_NOINTDEC | INTSOUND_NOINT08)))
		funcbit_disable(intsoundconfig, INTSOUND_FUNCTIONS);

	if(funcbit_test(intsoundconfig, INTSOUND_TSR)) {
		funcbit_enable(intsoundconfig, INTSOUND_DECODER);
		if(!funcbit_test(prebuffertype, PREBUFTYPE_FULL)) {
			funcbit_disable(prebuffertype, PREBUFTYPE_MASK);
			funcbit_enable(prebuffertype, PREBUFTYPE_LONGRING);
		}
	}
	if(funcbit_test(prebuffertype, PREBUFTYPE_BACK)) {
		if(!prebuffermegabytes)
			prebuffermegabytes = PREBUFFERBLOCKS_LONGRING * PREBUFFERBLOCKSIZE_DECODE / 1048576;	// 1mbyte
		if(!backbuffermegabytes)
			backbuffermegabytes = prebuffermegabytes;
		prebuffermegabytes += backbuffermegabytes;
		i = 100 - (long)(100.0 * (float)backbuffermegabytes / (float)prebuffermegabytes);
		if(i > PREBUFTYPE_BACKBUF_PERCENT_MAX)
			i = PREBUFTYPE_BACKBUF_PERCENT_MAX;
		PREBUFTYPE_PUT_BACKBUF_PERCENT(prebuffertype, i);
	}

	if(prebuffermegabytes)
		prebufferblocks = prebuffermegabytes * 1048576 / PREBUFFERBLOCKSIZE_DECODE;

	switch (prebuffertype & PREBUFTYPE_MASK) {
	case PREBUFTYPE_LONGRING:
		if(!prebufferblocks)
			prebufferblocks = PREBUFFERBLOCKS_LONGRING;
	case PREBUFTYPE_SHORTRING:
		funcbit_disable(prebuffertype, PREBUFTYPE_MASK);
		funcbit_enable(prebuffertype, PREBUFTYPE_RING);
		break;
	case PREBUFTYPE_FULL:
		if(!prebufferblocks && funcbit_test(intsoundconfig, INTSOUND_DECODER))
			prebufferblocks = PREBUFFERBLOCKS_LONGRING;	// if cannot alloc fullbuffer
	}
	if(prebufferblocks < PREBUFFERBLOCKS_SHORTRING)
		prebufferblocks = PREBUFFERBLOCKS_SHORTRING;

	if(outmode != OUTMODE_TYPE_NULL) {
		if((playstartpercent >= 16) && (playstartpercent <= 115))
			playstartpercent -= 16;
		else
			playstartpercent = 0;
		if(playendframe > playstartframe && !playcountframe && !playcounttime && !playcountpercent)
			playcountframe = playendframe - playstartframe;
		if(playcontrol & PLAYC_HIGHSCAN) {
			if(!playstartsong && !playrand)
				playstartsong = PLAYC_HS_STARTSONG;
			if(!playstartframe && !playstarttime && !playstartpercent)
				playstarttime = PLAYC_HS_STARTTIME;
			if(!playcountframe && !playcounttime && !playcountpercent)
				playcounttime = PLAYC_HS_TIMECOUNT;
		}
	}

	if(!(displaymode & DISP_FULLSCREEN)) {
		if(preloadinfo == PLI_DISPLOAD)	// -idl is not usefull in non-fullscreen mode
			preloadinfo = PLI_EHLINELOAD;
		funcbit_disable(gl[36].type, ARG_SAVE);	// do not save displaymode in non-fullscreen mode
		funcbit_disable(gl[37].type, ARG_SAVE);	// desktopmode detto
	}
	if(!(outmode & OUTMODE_TYPE_AUDIO))
		funcbit_disable(useglvariables, 2);	// do not save global variables into mpxplay.ini in non-audio mode

	if(do_printhelp || (freeopts[OPT_INPUTFILE] == NULL && !(psi->editsidetype & (PLT_DIRECTORY | PLT_ENABLED))
						&& !((mvp->psi0 + 1)->editsidetype & PLT_ENABLED) && !playlistload)) {
		mpxplay_printhelp();
		mpxplay_close_program(0);
	}

	timemode &= 3;
	channelmode &= CHM_USERCNTRL_MASK;

	aui = mvp->aui;

	if(outmode == OUTMODE_TYPE_NULL) {
		aui->card_selectname = "NOT";
	} else {
		if(aui->card_controlbits & AUINFOS_CARDCNTRLBIT_MIDASMANUALCFG)
			aui->card_selectname = "MID";
		if(outmode & OUTMODE_TYPE_TEST)
			aui->card_selectname = "NUL";
		if(outmode & OUTMODE_TYPE_FILE)
			aui->card_selectname = "WAV";

		if(aui->chan_set > PCM_MAX_CHANNELS)
			aui->chan_set = PCM_MAX_CHANNELS;
		if(aui->bits_set > PCM_MAX_BITS)
			aui->bits_set = PCM_MAX_BITS;

		//if -scv then we set the master and pcm volume with it and we turn off the mute switches
		/*if(aui->card_mixer_values[AU_MIXCHAN_MASTERVOL]){
		   aui->card_mixer_values[AU_MIXCHAN_PCMVOL]=aui->card_mixer_values[AU_MIXCHAN_MASTERVOL];
		   aui->card_mixer_values[AU_MIXCHAN_MASTERSWITCH]=aui->card_mixer_values[AU_MIXCHAN_PCMSWITCH]=100;
		   } */

		if(MIXER_controlbits & MIXER_CONTROLBIT_SPEED1000)
			funcbit_enable(aui->mixer_infobits, AUINFOS_MIXERCTRLBIT_SPEED1000);

		MIXER_setfunction("MIX_SURROUND", MIXER_SETMODE_ABSOLUTE, MIXER_varcfg_surround);
		MIXER_setfunction("MIX_SPEED", MIXER_SETMODE_ABSOLUTE, MIXER_varcfg_speed);

		MIXER_checkallfunc_setflags();

		//crossfade-lens check
		cfi = mvp->cfi;
		if(cfi->crossfadepoint > cfi->crossfade_out_len)
			cfi->crossfadepoint = cfi->crossfade_out_len >> 1;
		if(cfi->crossfade_in_len < (cfi->crossfade_out_len - cfi->crossfadepoint))
			cfi->crossfade_in_len = cfi->crossfade_out_len - cfi->crossfadepoint + 1;

		//start-time, exit-time -> run-time calc
		if(p_progtimelen || p_progtimeexit || p_playtimestart) {
			mpxplay_progtimebegin = pds_gettime();
			mpxplay_progtimebegin = PDS_HEXTIME_TO_SECONDS(mpxplay_progtimebegin);
			if(p_playtimestart) {
				unsigned long timestart = pds_strtime_to_hextime(p_playtimestart, 1);
				timestart = PDS_HEXTIME_TO_SECONDS(timestart);
				if(timestart < mpxplay_progtimebegin)
					timestart += 24 * 3600;
				timestart -= mpxplay_progtimebegin;
				mpxplay_playtimestart = timestart;	// elapsed from the progtimebegin
				funcbit_enable(playcontrol, PLAYC_PAUSENEXT);
			}
			if(p_progtimelen) {
				mpxplay_progtimelen = pds_strtime_to_hextime(p_progtimelen, 1);
				mpxplay_progtimelen = PDS_HEXTIME_TO_SECONDS(mpxplay_progtimelen);
				if(mpxplay_playtimestart)
					mpxplay_progtimelen += mpxplay_playtimestart;
			}
			if(p_progtimeexit) {
				unsigned long exittime = pds_strtime_to_hextime(p_progtimeexit, 1);
				exittime = PDS_HEXTIME_TO_SECONDS(exittime);
				if(exittime < mpxplay_progtimebegin)
					exittime += 24 * 3600;	// exit next day
				exittime -= mpxplay_progtimebegin;
				if(!mpxplay_progtimelen || (exittime < mpxplay_progtimelen))
					mpxplay_progtimelen = exittime;
			}
			if(mpxplay_playtimestart || mpxplay_progtimelen)
				mpxplay_timer_addfunc(&mpxplay_control_progexectime, mvp, MPXPLAY_TIMERTYPE_REPEAT, mpxplay_timer_secs_to_counternum(1));
		}
	}

	//playlist init
	if(playlistload & PLL_DOOMBOX) {
		mvp->psil = psi;
		funcbit_enable(psi->editloadtype, PLL_DOOMBOX);
		funcbit_enable((psi + 1)->editsidetype, PLT_DOOMQUEUE);
	} else
		mvp->psil = psi + 1;
	if(freeopts[OPT_INPUTFILE]) {
		playlist_loadsub_getinputfile(mvp->psil);
		if(!(playlistload & PLL_LOADLIST))
			if(playlist_loadlist_check_extension(freeopts[OPT_INPUTFILE]) && !pds_strchr(freeopts[OPT_INPUTFILE], '*') && !pds_strchr(freeopts[OPT_INPUTFILE], '?'))
				funcbit_enable(playlistload, PLL_LOADLIST);
	}
	if(playlistload & PLL_LOADLIST) {
		funcbit_disable(playlistload, PLL_DRIVESCAN | PLL_DIRSCAN);
	} else {
		if(drivescanletters)
			funcbit_enable(playlistload, PLL_DRIVESCAN);
		else
			funcbit_disable(playlistload, PLL_DRIVESCAN);
		if(freeopts[OPT_INPUTFILE])
			funcbit_enable(playlistload, PLL_DIRSCAN);
	}
	if((playlistload & PLL_DOOMBOX) && (playlistload & PLL_TYPE_CMDL))
		funcbit_disable(psi->editsidetype, PLT_DIRECTORY);

	preloadinf_cfg = preloadinfo;
	if((desktopmode & DTM_MASK_COMMANDER) == DTM_MASK_COMMANDER) {
		//preloadinfo=PLI_PLAYLOAD;
		playlist_sortlist_selectfuncs(PLAYLIST_SORTLIST_SELECTFUNCS_COMMANDER);
	}
}

static void mpxplay_control_progexectime(mainvars * mvp)
{
	unsigned long currtime = pds_gettime();
	currtime = PDS_HEXTIME_TO_SECONDS(currtime);
	if(currtime < mpxplay_progtimebegin)	// after midnight
		currtime += 24 * 3600;	// +1 day
	currtime -= mpxplay_progtimebegin;	// elapsed run-time (in seconds)
	if(mpxplay_playtimestart) {
		if(currtime >= mpxplay_playtimestart) {
			if(mvp->frp0->infile_funcs) {
				AU_prestart(mvp->aui);
			} else {
				funcbit_enable(playcontrol, PLAYC_STARTNEXT);
				mvp->adone = ADONE_RESTART;
			}
			mpxplay_playtimestart = 0;
			refdisp |= RDT_OPTIONS;
		}
	}
	if(mpxplay_progtimelen) {
		if(currtime > mpxplay_progtimelen)
			mvp->partselect = 0;
	}
}

static void mpxplay_control_savevar_set(void)
{
	displaymode &= DISP_TEXTMODES;
	desktopmode = (desktopmode & (~DTM_MASK_LOCKMPXINI)) | (desktopmod_mpxini & DTM_MASK_LOCKMPXINI);
	playcontrol &= PLAYC_SAVEMASK;
	MIXER_varcfg_surround = MIXER_getvalue("MIX_SURROUND");
	MIXER_varcfg_speed = MIXER_getvalue("MIX_SPEED");
}

//-----------------------------------------------------------------------
//load/save, init/close mpxplay.ini parts/infos (keycodes,global vars,startup,other hardwares)
static void mpxplay_control_global_loadini(mpxini_line_t *, struct mpxini_part_t *);
static void mpxplay_control_global_saveini(mpxini_line_t *, struct mpxini_part_t *, FILE *);

static mpxini_part_t mpxini_parts[] = {
//    partlinenum=1 is a hack to initialize the part without mpxplay.ini
	{"[keyboard]", 0, 1, 0, 0, &mpxplay_control_keyboard_loadini, NULL, &mpxplay_control_keyboard_init, NULL},
	{"[keygroups]", 0, 0, 0, 0, &mpxplay_control_keygroup_loadini, NULL, NULL, NULL},
	{"[fastlists]", 0, 0, 0, 0, &mpxplay_control_fastlist_loadini, NULL, NULL, NULL},
	{"[mouse]", 0, 1, 0, 0, &mpxplay_control_mouse_loadini, NULL, &mpxplay_control_mouse_init, &mpxplay_control_mouse_close},
	{"[joystick]", 0, 0, 0, 0, &mpxplay_control_joy_loadini, NULL, &mpxplay_control_joy_init, NULL},
	{"[serialport]", 0, 0, 0, 0, &mpxplay_control_serial_loadini, NULL, &mpxplay_control_serial_init, &mpxplay_control_serial_close},
	{"[LCDdisplay]", 0, 0, 0, 0, &mpxplay_display_lcd_loadini, NULL, &mpxplay_display_lcd_init, &mpxplay_display_lcd_close},
	{"[global]", 0, 0, 0, 0, &mpxplay_control_global_loadini, &mpxplay_control_global_saveini, NULL, NULL},	// recommended last-1
	{"[startup]", 0, 0, 0, MPXINI_FLAG_LOADINI_POST, &mpxplay_control_startup_loadini, &mpxplay_control_startup_saveini, NULL, NULL},	// must be the last!
};

#define MPXINI_PARTNUM (sizeof(mpxini_parts)/sizeof(mpxini_part_t))

#define CONTROL_CFGFILE_FLAG_READONLY    1
#define CONTROL_CFGFILE_FLAG_PARTSINITOK 2

static FILE *configfile;
static char *configmem;
static mpxini_line_t mpxini_lines[MPXINI_MAX_LINES];
static mpxini_part_t *mpxini_lastfoundpart;
static unsigned int control_cfgfile_flags;

static void mpxinifunc_search_partbegin(char *currline, unsigned int currlinenum)
{
	unsigned int i = MPXINI_PARTNUM;
	mpxini_part_t *partp = &mpxini_parts[0];

	do {
		if(pds_strncmp(currline, partp->partname, pds_strlen(partp->partname)) == 0) {
			if(mpxini_lastfoundpart)
				mpxini_lastfoundpart->partlinenum = currlinenum - mpxini_lastfoundpart->partbegin_linenum;
			mpxini_lastfoundpart = partp;
			partp->partbegin_linenum = currlinenum + 1;
			partp->filepos = currline - configmem + currlinenum + pds_strlen(currline) + 1;
			break;
		}
		partp++;
	} while(--i);
}

//open mpxplay.ini and load configurations/variables
void mpxplay_control_configfile_parts_loadini(void)
{
	char *s, *s2, *s3;
	unsigned int i, mpxini_linecount, len;
	struct mpxini_part_t *partp;
	char cfgfilename[300];

	pds_getpath_from_fullname(cfgfilename, freeopts[OPT_PROGNAME]);	//create path to mpxplay.ini
	len = pds_strlen(cfgfilename);
	if(len && (cfgfilename[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR))
		len += pds_strcpy(&cfgfilename[len], PDS_DIRECTORY_SEPARATOR_STR);
	pds_strcpy(&cfgfilename[len], "mpxplay.ini");

	configfile = fopen(cfgfilename, "r+");
	if(!configfile) {
		configfile = fopen(cfgfilename, "r");
		if(!configfile)
			return;
		funcbit_enable(control_cfgfile_flags, CONTROL_CFGFILE_FLAG_READONLY);
		display_warning_message("Warning: mpxplay.ini is read-only!");
	}

	i = pds_filelength(configfile->_handle);
	if(i > MPXINI_MAX_SIZE)
		return;
	configmem = malloc(i + 1024);
	if(!configmem)
		return;

	s = configmem;
	mpxini_linecount = 0;
	while(fgets(s, MPXINI_MAX_CHARSPERLINE, configfile) && (mpxini_linecount < MPXINI_MAX_LINES)) {
		char *s_next = s + pds_strlen(s);

		mpxinifunc_search_partbegin(s, mpxini_linecount);	// search [partnames] (ie:[keyboard],[serialport])

		//cut one line to variablename,value and comment parts (if possible)
		if((s[0] != ';') && (s2 = pds_strchr(s, '='))) {	// if line begins with ';' then it's comment allways
			*s2++ = 0;
			mpxini_lines[mpxini_linecount].varnamep = s;	// variable name
			mpxini_lines[mpxini_linecount].varnamelen = pds_strlen(s);	// save orogonal variable len
			pds_strcutspc(s);
			s3 = pds_strchr(s2, ';');
			if(s3) {
				*s3++ = 0;
				mpxini_lines[mpxini_linecount].commentp = s3;	// comment
			}
			mpxini_lines[mpxini_linecount].valuelen = pds_strlen(s2);	// save original value len
			while(*s2 == ' ' && *s2 != 0)	// skip spaces
				s2++;
			mpxini_lines[mpxini_linecount].valuep = s2;	// value
			if(!s3)
				s3 = s2;
		} else {				// it's a comment line only
			mpxini_lines[mpxini_linecount].commentp = s;
			s3 = s;
		}
		s3 = pds_strchr(s3, '\n');
		if(s3)
			s3[0] = 0;

		mpxini_linecount++;
		s = s_next;
	}

	if(mpxini_lastfoundpart)	// close of the part search
		mpxini_lastfoundpart->partlinenum = mpxini_linecount - mpxini_lastfoundpart->partbegin_linenum;

	// load and work up (handle) the configuration of parts
	partp = &mpxini_parts[0];
	i = MPXINI_PARTNUM;
	do {
		if(partp->loadini && !(partp->flags & MPXINI_FLAG_LOADINI_POST) && partp->partbegin_linenum && partp->partlinenum)
			partp->loadini(mpxini_lines, partp);
		partp++;
	} while(--i);
}

// save variables (parts) into mpxplay.ini
void mpxplay_control_configfile_parts_saveini(void)
{
	unsigned int i;
	struct mpxini_part_t *partp;

	if(!configmem)				// failed open or malloc at load
		return;
	if(funcbit_test(control_cfgfile_flags, CONTROL_CFGFILE_FLAG_READONLY))
		return;

	mpxplay_control_savevar_set();

	partp = &mpxini_parts[0];
	i = MPXINI_PARTNUM;
	do {
		if(partp->saveini && partp->partbegin_linenum && partp->partlinenum)
			partp->saveini(mpxini_lines, partp, configfile);
		partp++;
	} while(--i);
}

//initialize parts (hardwares)
void mpxplay_control_configfile_parts_init(void)
{
	unsigned int i;
	struct mpxini_part_t *partp;

	partp = &mpxini_parts[0];
	i = MPXINI_PARTNUM;
	do {
		if(partp->loadini && (partp->flags & MPXINI_FLAG_LOADINI_POST) && partp->partbegin_linenum && partp->partlinenum)
			partp->loadini(mpxini_lines, partp);
		if(partp->init && partp->partlinenum)
			partp->init();
		partp++;
	} while(--i);
	funcbit_enable(control_cfgfile_flags, CONTROL_CFGFILE_FLAG_PARTSINITOK);
}

//close parts (hardwares)
void mpxplay_control_configfile_parts_close(void)
{
	unsigned int i;
	struct mpxini_part_t *partp;

	if(!funcbit_test(control_cfgfile_flags, CONTROL_CFGFILE_FLAG_PARTSINITOK))
		return;
	funcbit_disable(control_cfgfile_flags, CONTROL_CFGFILE_FLAG_PARTSINITOK);	// run close once

	partp = &mpxini_parts[0];
	i = MPXINI_PARTNUM;
	do {
		if(partp->close && partp->partlinenum)
			partp->close();
		partp++;
	} while(--i);
}

// close mpxplay.ini
void mpxplay_control_configfile_close(void)
{
	if(configmem) {
		free(configmem);
		configmem = NULL;
	}
	if(configfile) {
		fclose(configfile);
		configfile = NULL;
	}
	control_cfgfile_flags = 0;
	cmdlg_free();
}

void mpxplay_control_general_loadini(mpxini_line_t * mpxini__lines,	// structure of all mpxplay.ini lines
									 mpxini_part_t * mpxini_partp,	// part info
									 mpxini_var_s * vars_begin	// variable structure
	)
{
	unsigned int i;
	mpxini_var_s *varp;

	mpxini__lines += mpxini_partp->partbegin_linenum;

	for(i = 0; i < mpxini_partp->partlinenum; i++) {
		if(mpxini__lines->varnamep) {
			varp = vars_begin;
			while(varp->name != NULL) {
				if(pds_stricmp(mpxini__lines->varnamep, varp->name) == 0) {
					if(varp->type & ARG_CHAR) {
						if(mpxini__lines->valuep[0] != 0 && mpxini__lines->valuep[0] != ' ') {
							if(varp->type & ARG_POINTER)
								*((char **)(varp->c)) = mpxini__lines->valuep;
							else
								pds_strcpy((char *)varp->c, mpxini__lines->valuep);
						}
					} else {
						if(varp->type & ARG_HEX)
							*((unsigned int *)varp->c) = pds_atol16(mpxini__lines->valuep);
						else
							*((int *)varp->c) = pds_atol(mpxini__lines->valuep);
					}
					mpxini__lines->storepoint = (void *)varp;
					break;
				}
				varp++;
			}
		}
		mpxini__lines++;
	}
}

/*void mpxplay_control_general_saveini(
          mpxini_line_t *mpxini__lines, // structure of all mpxplay.ini lines
	  mpxini_part_t *mpxini_partp,  // part info
          FILE *conffile
	 )
{
 unsigned int i;
 char sout[MPXINI_MAX_CHARSPERLINE];

 fseek(conffile,mpxini_partp->filepos,SEEK_SET);

 mpxini__lines+=mpxini_partp->partbegin_linenum;

 for(i=0;i<mpxini_partp->partlinenum;i++){
  if(mpxini__lines->varnamep && mpxini__lines->storepoint){
   mpxini_var_s *varp=(mpxini_var_s *)mpxini__lines->storepoint;

   if(varp->type&ARG_SAVE){
    if(varp->type&ARG_CHAR){
     if(varp->type&ARG_POINTER)
      snprintf(sout,sizeof(sout),"%-13s=%s ",varp->name,(*((char **)varp->c) && **((char **)varp->c))? *((char **)varp->c):"");
     else // static field
      snprintf(sout,sizeof(sout),"%-13s=%s ",varp->name,(((char *)varp->c) && *((char *)varp->c))? (char *)varp->c:"");
    }else{
     if(varp->type&ARG_HEX)
      sprintf(sout,"%-13s=%8.8X ",varp->name,*((unsigned int *)varp->c));
     else // decimal
      sprintf(sout,"%-13s=%-5d ",varp->name,*((int *)varp->c));
    }
   }else{
    snprintf(sout,sizeof(sout),"%-13s=%s",varp->name,mpxini__lines->valuep);
   }

   if(mpxini__lines->commentp){
    pds_strcat(sout,";");
    pds_strcat(sout,mpxini__lines->commentp);
   }
   pds_strcat(sout,"\n");

  }else{
   snprintf(sout,sizeof(sout),"%s\n",mpxini__lines->commentp);
  }
  fputs(sout,conffile);
  mpxini__lines++;
 }
}*/

//-------------------------------------------------------------------------
// load/save global variables
static void mpxplay_control_global_loadini(mpxini_line_t * mpxini__lines, struct mpxini_part_t *mpxini_partp)
{
	unsigned int i;

	mpxini__lines += mpxini_partp->partbegin_linenum;

	for(i = 0; i < mpxini_partp->partlinenum && (useglvariables & 1); i++) {
		if(mpxini__lines->varnamep) {
			mpxini_var_s *glpoint = &gl;
			while((glpoint->name != NULL) && (useglvariables & 1)) {
				if(pds_stricmp(mpxini__lines->varnamep, glpoint->name) == 0) {
					if(glpoint->type & ARG_CHAR) {
						if(mpxini__lines->valuep[0] != 0 && mpxini__lines->valuep[0] != ' ') {
							if(glpoint->type & ARG_POINTER)
								*((char **)(glpoint->c)) = mpxini__lines->valuep;
							else
								pds_strcpy((char *)glpoint->c, mpxini__lines->valuep);
						}
					} else {
						*((int *)glpoint->c) = pds_atol(mpxini__lines->valuep);
					}
					mpxini__lines->storepoint = (void *)glpoint;
					break;
				}
				glpoint++;
			}
		}
		mpxini__lines++;
	}

	desktopmod_mpxini = desktopmode;
}

//#define MPXPLAY_CONTROL_GLOBAL_SAVEINI_DEBUG 1

static void mpxplay_control_global_saveini(mpxini_line_t * mpxini__lines, struct mpxini_part_t *mpxini_partp, FILE * conffile)
{
	unsigned int i;
	char sval[MPXINI_MAX_CHARSPERLINE], sout[MPXINI_MAX_CHARSPERLINE];

	if(!(useglvariables & 2))
		return;

	fseek(conffile, mpxini_partp->filepos, SEEK_SET);

	mpxini__lines += mpxini_partp->partbegin_linenum;

	for(i = 0; i < mpxini_partp->partlinenum; i++) {
		char *sp = &sout[0];
		if(mpxini__lines->varnamep && mpxini__lines->storepoint) {
			mpxini_var_s *glpoint = (mpxini_var_s *) mpxini__lines->storepoint;
			pds_strcpy(sp, glpoint->name);
			sp += pds_str_fixlenc(sp, mpxini__lines->varnamelen, ' ');
			sp += pds_strcpy(sp, "=");
			if(glpoint->type & ARG_SAVE) {
				if(glpoint->type & ARG_CHAR) {	// ??? not tested (no such case yet)
					if(glpoint->type & ARG_POINTER)
						pds_strcpy(sval, *((char **)glpoint->c));	// ???
					else
						pds_strcpy(sval, (char *)glpoint->c);
				} else {
					int num_val = *((int *)glpoint->c);
					sprintf(sval, "%-9d", num_val);
				}
			} else {
				pds_strcpy(sval, mpxini__lines->valuep);
			}
			pds_str_fixlenc(sval, mpxini__lines->valuelen, ' ');
			sp += pds_strcpy(sp, sval);
		} else {
#ifdef MPXPLAY_CONTROL_GLOBAL_SAVEINI_DEBUG
			if(mpxini__lines->varnamep || mpxini__lines->storepoint || mpxini__lines->valuep) {
				snprintf(sout, sizeof(sout), "mpxini save error: %8.8X %8.8X %8.8X %s %s", mpxini__lines->varnamep, mpxini__lines->storepoint, mpxini__lines->valuep,
						 ((mpxini__lines->varnamep) ? mpxini__lines->varnamep : ""), ((mpxini__lines->valuep) ? mpxini__lines->valuep : ""));
				pds_textdisplay_printf(sout);
				getch();
			}
#endif
			if(mpxini__lines->varnamep && mpxini__lines->valuep) {	// this can happen at an unknown variable (ie: if the exe and ini version don't match)
				pds_strcpy(sp, mpxini__lines->varnamep);
				sp += pds_str_fixlenc(sp, mpxini__lines->varnamelen, ' ');
				sp += pds_strcpy(sp, "=");
				pds_strcpy(sval, mpxini__lines->valuep);
				pds_str_fixlenc(sval, mpxini__lines->valuelen, ' ');
				sp += pds_strcpy(sp, sval);
			}
		}
		if(mpxini__lines->commentp) {
			if(mpxini__lines->commentp[0] != ';')
				sp += pds_strcpy(sp, ";");
			sp += pds_strcpy(sp, mpxini__lines->commentp);
		}
		if(sp > (&sout[0])) {
			pds_strcpy(sp, "\n");
			fputs(sout, conffile);
		}
		mpxini__lines++;
	}
}

//-------------------------------------------------------------------------
// print help
static void mpxplay_printhelp(void)
{
#ifdef MPXPLAY_LINK_FULL
#ifdef MPXPLAY_WIN32
	pds_textdisplay_printf("Mpxplay v1.57   Audio player for Windows (console)   by PDSoft 1998-2010");
#else
	pds_textdisplay_printf("Mpxplay v1.57          Audio player for DOS          by PDSoft 1998-2010");
#endif
	pds_textdisplay_printf(" ");
	pds_textdisplay_printf("Supported fileformats: AAC,AC3,APE,AVI,FLAC,MP2/MP3,MP4,MPC,OGG,WAV,WMA,WV,CDW");
#else
	pds_textdisplay_printf("Mpxplay v1.57  Audio player for DOS  (light version) by PDSoft 1998-2010");
	pds_textdisplay_printf(" ");
	pds_textdisplay_printf("Supported fileformats: AAC,MP2/MP3,MP4,MPC,OGG,WAV,CDW");
#endif
	pds_textdisplay_printf(" ");
	pds_textdisplay_printf("usage: MPXPLAY.EXE [option(s)] [file/playlist/searchmask] [file2] [file3] [...]");
	pds_textdisplay_printf(" ");
	pds_textdisplay_printf(" -f[0/f/l/s/e] (full)screen mode(s) -ds multiple drive scan (cde -> c: d: e:)");
	pds_textdisplay_printf(" -b[p/l] pre/full input buffer      -sc[s/t/v] sound card select/test/volume");
	pds_textdisplay_printf(" -p[ss/sf/sp] start song/frame/%    -p[rn/re] randomize/repeat playlist ");
	pds_textdisplay_printf(" -c[l/m] left channel only/downmix  -cf[i/p/o/l/t] crossfade settings");
	pds_textdisplay_printf(" -t   testmode (no output)          -o  write sound output to a wav file");
	pds_textdisplay_printf(" -dd  jukebox mode                  -s[v/r/p] sound volume/surround/speed");
	pds_textdisplay_printf(" ");
	pds_textdisplay_printf("Playing controls (see readme.txt for the full list of options and keys):");
	pds_textdisplay_printf("ESC - exit                             P  - start/pause playing");
	pds_textdisplay_printf(" -  - step back in playlist            +  - step to next song in playlist");
	pds_textdisplay_printf(" -> - forward (right arrow)            <- - rewind (left arrow)");
	pds_textdisplay_printf("BACKSPC - restart song                NUM - fast step (step to n. song)");
	pds_textdisplay_printf(" A-spectrum analyser T-time mode V-auto volume ,.-volume ;'-surround [] -speed");
	pds_textdisplay_printf(" X-swapchan C-crossfade F-cf-fadeout/in N-random R-replay M-mute");
}
