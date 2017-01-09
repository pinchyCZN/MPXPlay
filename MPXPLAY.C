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
//function: Mpxplay main

#include <string.h>
#include "newfunc\newfunc.h"
#include "mpxinbuf.h"
#include "au_mixer\au_mixer.h"
#include "au_cards\au_cards.h"
#include "control\control.h"
#include "control\cntfuncs.h"
#include "decoders\decoders.h"
#include "display\display.h"
#include "playlist\playlist.h"

static void mpxplay_init_all(void);
static void mpxplay_init_1(void);
static void mpxplay_init_2(void);
static void mpxplay_init_run(void);

static void mpxplay_main_cycle(void);
static void mpxplay_tsr_cycle_1(void);
static void mpxplay_tsr_cycle_2(void);
static void main_part1(struct mainvars *);
static void open_new_infile(struct mainvars *);
static unsigned int mpxplay_set_playstartpos(struct frame *, struct mainvars *);
static void start_or_stop_playing(struct mainvars *);
static void main_part2(struct mainvars *);
static void check_seeking(struct mainvars *);
static void crossfade_auto_control(struct frame *, struct frame *, struct mainvars *);
static void crossfade_manual_set(struct mainvars *, unsigned int newpart);
static void crossfade_initvar(struct mainvars *, struct crossfade_info *);

extern mainvars mvps;
extern unsigned int refdisp, crossfadepart, outmode, displaymode, desktopmode;
extern unsigned int intsoundconfig, intsoundcontrol, prebuffertype, shutdownatx_enabled;
extern unsigned int playcontrol, playreplay, playrand, playcountsong, playlistsave;
extern int playstartpercent, playstartframe;
extern char *playstarttime, *playcounttime;
extern unsigned int playcountframe, playcountpercent;
extern unsigned long int08counter, mpxplay_signal_events;
extern int MIXER_var_mutelen;
static volatile unsigned int mtc1_running;

void main(int argc, char *argv[])
{
	//pds_mswin_previousinstance_close();
	newfunc_init(argv[0]);		// is windows?; is LFN?; init textdisplay, init error handlers
	mpxplay_control_initvar(argc, argv, &mvps);	// set variables to default
	mpxplay_control_configfile_parts_loadini();	// open and load configfile;read keyboard codes,global variables
	mpxplay_control_getcommandlineopts(argc, argv);

	mpxplay_init_1();
	if(outmode & OUTMODE_TYPE_MASK) {
		mpxplay_init_2();
		mpxplay_init_run();
	}

	mpxplay_close_program(0);
}

static void mpxplay_init_1(void)
{
	struct mainvars *mvp = &mvps;
	mpxplay_save_startdir(mvp);
	mpxplay_control_checkvar(mvp);	// check (and correct the bad) variables
	AU_init(mvp->aui);			// audio init (and test)
	mpxplay_control_configfile_parts_init();	// init (hw) parts of mpxplay.ini (mouse,joy,serial-port,LCD,startup)
	mpxplay_mpxinbuf_init(mvp);
	mpxplay_infile_init(mvp);	// static struct build and infile-preinit (mpx_init())
	mpxplay_playlist_init(mvp);	// alloc playlist & id3 memory
	mpxplay_display_init(mvp);
	refdisp = RDT_INIT_FULL | RDT_OPTIONS | RDT_HEADER | RDT_ID3INFO | RDT_EDITOR;
	refresh_desktop(mvp);
	mpxplay_control_startup_loadlastlist();	// !!! moved here from configfile_parts_init
	playlist_get_allfilenames(mvp);	// playlist, directory or drive scan
	playlist_id3list_load(mvp, NULL);	// -ig
	playlist_init_playside(mvp);	// playside,editorside
	refdisp = RDT_INIT_EDIT | RDT_EDITOR;
	refresh_desktop(mvp);
	playlist_chkentry_get_allfileinfos(mvp);	// header (filetype),id3,filesize
	playlist_editlist_id3filter(mvp);	// -if
	playlist_id3list_save(mvp);	// -is
	playlist_write_id3tags(mvp);	// -iw
}

static void mpxplay_init_2(void)
{
	mpxplay_control_startup_getstartpos(&mvps);	// get startup position
	playlist_init_playsong(&mvps);	// startup, -pss
	AU_setmixer_all(mvps.aui);	// -scv,-sctr,-scbs
	MIXER_allfuncinit_init();
	mpxplay_videoout_init(mvps.voi);
	check_dosshellstart();		// -xs
	mpxplay_mpxinbuf_prealloc(&mvps);	// -bp
	AU_ini_interrupts(mvps.aui);	// dma monitor, interrupt decoder
	mpxplay_timer_reset_counters();	// to redirect int08 callings if no int08
#ifndef MPXPLAY_WIN32
	if(funcbit_test(intsoundconfig, INTSOUND_TSR)) {	// -xr
		mpxplay_timer_addfunc(&mpxplay_tsr_cycle_1, NULL, MPXPLAY_TIMERTYPE_INT08 | MPXPLAY_TIMERTYPE_REPEAT | MPXPLAY_TIMERFLAG_OWNSTACK | MPXPLAY_TIMERFLAG_INDOS, 0);
		mpxplay_timer_addfunc(&mpxplay_tsr_cycle_2, NULL, MPXPLAY_TIMERTYPE_INT08 | MPXPLAY_TIMERTYPE_REPEAT | MPXPLAY_TIMERFLAG_OWNSTACK | MPXPLAY_TIMERFLAG_INDOS, 0);
	}
#endif
	intsoundcontrol = intsoundconfig;	// -xr needs
	mpxplay_timer_addfunc(&check_seeking, &mvps, MPXPLAY_TIMERTYPE_REPEAT, 1);
}

static void mpxplay_empty(void)
{
#ifdef MPXPLAY_WIN32
	Sleep(10);
#endif
}

#ifdef MPXPLAY_WIN32

static void mpxplay_init_run(void)
{
	funcbit_smp_disable(intsoundcontrol, INTSOUND_DECODER);
	funcbit_smp_value_put(mvps.partselect, 1);
	if((funcbit_test(intsoundconfig, INTSOUND_TSR) && newfunc_newhandler08_maincycles_init(&mvps, &mpxplay_tsr_cycle_1, &mpxplay_tsr_cycle_2))
	   || (funcbit_test(intsoundconfig, INTSOUND_DECODER) && newfunc_newhandler08_maincycles_init(&mvps, &mpxplay_main_cycle, NULL))	// ???
		) {
		funcbit_smp_copy(intsoundcontrol, intsoundconfig, (INTSOUND_DECODER | INTSOUND_TSR));
		do {
			mpxplay_empty();
		} while(mvps.partselect != 0);
	} else {
		funcbit_smp_copy(intsoundcontrol, intsoundconfig, INTSOUND_DECODER);
		do {
			mpxplay_main_cycle();
		} while(mvps.partselect != 0);
	}
}

#else

static void mpxplay_init_run(void)
{
	printf("1111\n");
	if(funcbit_test(intsoundconfig, INTSOUND_TSR)) {
		do {
			mpxplay_empty();
		} while(mvps.partselect != 0);
	} else {
		do {
			mpxplay_main_cycle();
		} while(mvps.partselect != 0);
	}
	printf("xxxxxxx\n");
}
#endif

static void mpxplay_main_cycle(void)
{
	struct mainvars *mvp = &mvps;
	switch (mvp->partselect) {
	case 1:
		funcbit_smp_value_put(mtc1_running, 1);
		main_part1(mvp);
		funcbit_smp_value_put(mtc1_running, 0);
		break;
	case 2:
		main_part2(mvp);
		break;
	}
}

static void mpxplay_tsr_cycle_1(void)
{
	if(intsoundcontrol & INTSOUND_DECODER) {
#ifdef __DOS__
		mtc1_running = 1;
#endif
		mpxplay_main_cycle();
#ifdef __DOS__
		mtc1_running = 0;
#endif
	}
}

static void mpxplay_tsr_cycle_2(void)
{
	if((intsoundcontrol & INTSOUND_DECODER) && !mtc1_running)
		mpxplay_timer_execute_maincycle_funcs();
}

//------------------------------------------------------------------------

static void main_part1(struct mainvars *mvp)
{
	if(!playcountsong--) {
		mvp->partselect = 0;
	} else {
		do {
			if(refdisp)
				refresh_desktop(mvp);
			if(mvp->adone) {
				unsigned int retcode;
				if(!(playcontrol & PLAYC_HIGHSCAN)) {
					playstartframe = playstartpercent = 0;
					playstarttime = NULL;
				}
				if((mvp->psip->editsidetype & PLT_DOOMQUEUE) && ((mvp->psip != mvp->psie) || (mvp->direction == 0) || (mvp->adone == ADONE_EOF) || crossfadepart))
					retcode = playlist_jukebox_skip(mvp);
				else
					retcode = playlist_skip(mvp);
				if(!retcode) {
					struct playlist_side_info *psi = mvp->psip;
					if((playcontrol & PLAYC_EXITENDLIST) && (!(psi->editsidetype & PLT_DIRECTORY) || (psi->lastentry >= psi->firstsong))) {
						mvp->partselect = 0;
						return;
					} else
						mpxplay_stop_and_clear(mvp, 1);
				}
			}
			if((mvp->aktfilenum->infobits & PEIF_INDEXED) && ((outmode & OUTMODE_TYPE_FILE) || (mvp->aui->card_handler->infobits & SNDCARD_FLAGS_DISKWRITER) == SNDCARD_FLAGS_DISKWRITER))
				funcbit_enable(mvp->aui->card_controlbits, AUINFOS_CARDCNTRLBIT_AUTOTAGLFN);
			else
				funcbit_disable(mvp->aui->card_controlbits, AUINFOS_CARDCNTRLBIT_AUTOTAGLFN);

			open_new_infile(mvp);
			if(mvp->adone) {
				char sout[64];
				snprintf(sout, sizeof(sout), "Searching next file (%2d/%d)", mvp->aktfilenum - mvp->psip->firstsong + 1, mvp->psip->lastentry - mvp->psip->firstsong + 1);
				display_message(0, 0, sout);
				display_message(1, 0, "");
			}
		} while(mvp->adone && (mvp->direction != 0) && !funcbit_test(playcontrol, PLAYC_ABORTNEXT));

		start_or_stop_playing(mvp);
		mpxplay_control_keyboard_songnum_reset();
		mvp->direction = mvp->seek_relative = mvp->sndempty = mvp->adone = mvp->fdone = 0;
		mvp->newfilenum = NULL;
		funcbit_disable(playcontrol, PLAYC_CONTINUOUS_SEEK);
		mpxplay_timer_reset_counters();
		funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_NEWFILE);
		clear_message();
		refdisp |= RDT_BROWSER | RDT_EDITOR | RDT_OPTIONS | RDT_HEADER | RDT_ID3INFO;
		mvp->partselect = 2;
	}
}

static void open_new_infile(struct mainvars *mvp)
{
	struct frame *frp0 = mvp->frp0, *frp = frp0;
	struct mpxplay_audio_decoder_info_s *adi = frp0->infile_infos->audio_decoder_infos;
	unsigned int last_freq = adi->freq, infobits_save;

	if(crossfadepart || funcbit_test(intsoundconfig, INTSOUND_DECODER))
		frp++;

	mvp->adone = ADONE_RESTART;
	if((frp->buffertype & PREBUFTYPE_LOADNEXT_OK) || playlist_open_infile(frp, mvp, mvp->aktfilenum)) {

		playlist_pei0_set(mvp, mvp->aktfilenum, 0);

		adi = frp->infile_infos->audio_decoder_infos;

		infobits_save = adi->infobits;

		AU_setrate(mvp->aui, adi);

		if((infobits_save & ADI_FLAG_BITSTREAMOUT) != (adi->infobits & ADI_FLAG_BITSTREAMOUT)) {
			frp->infile_infos->allframes = frp->allframes = 0;
			miis_to_frp(frp->infile_infos, frp);	// re-call after AU_setrate
		}

		if(!(playcontrol & PLAYC_RUNNING) || (adi->freq != last_freq))
			crossfadepart = CROSS_CLEAR;	// don't modify it to crossfade_manual_set(CROSS_CLEAR)

		if(mpxplay_decoders_alloc(frp, 1) != MPXPLAY_ERROR_INFILE_OK)
			goto err_out_oni;

		if(!(frp->buffertype & PREBUFTYPE_LOADNEXT_OK))
			if(!mpxplay_mpxinbuf_alloc(mvp, frp))
				goto err_out_oni;

		if(!mpxplay_set_playstartpos(frp, mvp))
			goto err_out_oni;

		funcbit_smp_disable(intsoundcontrol, (INTSOUND_DECODER | INTSOUND_TSR));

		newfunc_newhandler08_waitfor_threadend();

		if(!MIXER_configure(mvp->aui, frp))
			goto err_out_oni;

		funcbit_smp_disable(playcontrol, PLAYC_BEGINOFSONG);

		mpxplay_mpxinbuf_set_intsound(frp, intsoundconfig);

		if(frp != frp0)
			pds_smp_memxch((char *)frp0, (char *)frp, MPXPLAY_INFILE_STRUCTFRAME_REALSIZE - sizeof(frp0->fro));

		MIXER_allfuncinit_reinit();

		if(crossfadepart == CROSS_CLEAR) {
			if(mvp->seek_relative >= 0 && !funcbit_test(intsoundconfig, INTSOUND_DECODER))
				funcbit_smp_enable(playcontrol, PLAYC_BEGINOFSONG);
			//if((playcontrol&PLAYC_RUNNING) && (frp->infile_infos->seektype!=MPX_SEEKTYPE_BOF))
			// MIXER_allfuncinit_restart();
		}

		if(crossfadepart == CROSS_LOAD)
			crossfade_manual_set(mvp, CROSS_FADE);
		else
			crossfade_manual_set(mvp, CROSS_CLEAR);

		clear_volnum();

		mvp->adone = 0;
		funcbit_smp_value_put(mvp->idone, MPXPLAY_ERROR_INFILE_OK);
		mvp->foundfile = 1;

		funcbit_smp_copy(intsoundcontrol, intsoundconfig, (INTSOUND_DECODER | INTSOUND_TSR));

#ifdef MPXPLAY_LINK_VIDEO
		mpxplay_infile_video_config_open(mvp->voi, frp0->infile_infos->video_decoder_infos);
		if((playcontrol & PLAYC_AUTOGFX) && displaymode && frp0->infile_infos->video_decoder_infos->video_res_x)
			mpxplay_display_switch_to_graphmode(mvp);
#endif

		if(playrand) {
			if(mvp->direction < 0)	//
				playlist_randlist_popq();	// pop prev/curr-file
			playlist_randlist_pushq(mvp->psip, mvp->aktfilenum);	// push curr
		}
	}

	return;

  err_out_oni:
	if(!frp->pcmdec_buffer)
		crossfadepart = CROSS_CLEAR;
	funcbit_smp_copy(intsoundcontrol, intsoundconfig, (INTSOUND_DECODER | INTSOUND_TSR));
	return;
}

long mpxplay_calculate_timesec_to_framenum(struct frame *frp, char *pst)
{
	long timesech = pds_strtime_to_hexhtime(pst);
	timesech = PDS_HEXHTIME_TO_HSECONDS(timesech);
	return (long)((float)frp->allframes * (float)timesech * 10.0 / (float)frp->infile_infos->timemsec);
}

long mpxplay_calculate_index_start_end(struct frame *frp, struct mainvars *mvp, struct playlist_entry_info *pei)
{
	long index_end;
	frp->index_start = frp->index_end = 0;

	if(pei->pstime) {
		frp->index_start = (long)((float)frp->allframes * (float)pei->pstime / (float)frp->infile_infos->timemsec);
		if(frp->index_start >= frp->allframes)
			if(frp->allframes > mvp->seek_frames)
				frp->index_start = frp->allframes - mvp->seek_frames;
			else
				frp->index_start = 0;
	}

	if(pei->petime) {
		frp->index_end = (long)((float)frp->allframes * (float)pei->petime / (float)frp->infile_infos->timemsec);
		if(frp->index_end > frp->allframes)
			frp->index_end = frp->allframes;
	}

	frp->timesec = (pei->petime && (pei->petime < frp->infile_infos->timemsec)) ? pei->petime : frp->infile_infos->timemsec;
	if(frp->timesec > pei->pstime)
		frp->timesec -= pei->pstime;
	frp->timesec = (frp->timesec + 500) / 1000;

	index_end = (frp->index_end) ? frp->index_end : frp->allframes;
	frp->index_len = (index_end > frp->index_start) ? (index_end - frp->index_start) : 1;

	return index_end;
}

static long mpxplay_calculate_playstartpos(struct frame *frp, struct mainvars *mvp)
{
	long newframenum = mvp->seek_relative, index_end;

	index_end = mpxplay_calculate_index_start_end(frp, mvp, mvp->pei0);

	if(playstartpercent)
		playstartframe = frp->index_len * playstartpercent / 100;
	if(playstarttime)
		playstartframe = mpxplay_calculate_timesec_to_framenum(frp, playstarttime);
	if(playstartframe)
		newframenum = frp->index_start + playstartframe;

	if(playcountpercent)
		playcountframe = frp->index_len * playcountpercent / 100;
	if(playcounttime)
		playcountframe = mpxplay_calculate_timesec_to_framenum(frp, playcounttime);

	if(newframenum < 0) {
		newframenum = -newframenum;
		if(index_end > (frp->index_start + newframenum))
			newframenum = index_end - newframenum;
		else
			newframenum = frp->index_start;
	} else {
		index_end = (frp->index_end) ? frp->index_end : (frp->index_start + frp->index_len);
		if((playcontrol & PLAYC_HIGHSCAN) && ((newframenum + playcountframe) > index_end)) {	// in demo mode if song is less than 1:10
			if((index_end - frp->index_start) > playcountframe)
				newframenum = index_end - playcountframe;	// seek to len-10sec
			else
				newframenum = frp->index_start;
		}
		if(!newframenum || (frp->index_end && (newframenum > frp->index_end)))
			newframenum = frp->index_start;
	}

	return newframenum;
}

static unsigned int mpxplay_set_playstartpos(struct frame *frp, struct mainvars *mvp)
{
	long newframenum = mpxplay_calculate_playstartpos(frp, mvp);
	long prereadbytes = mpxplay_diskdrive_drive_config(frp->mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_PREREADBUFBYTES, NULL, NULL);

	if(prereadbytes > 0) {
		display_message(0, 0, "Preloading next file:");
		display_message(1, 0, mvp->pei0->filename);
	}

	newframenum = mpxplay_infile_fseek(frp, newframenum);
	if(newframenum < 0)			// possible in demo mode if song is less than 1 min
		newframenum = mpxplay_infile_fseek(frp, frp->index_start);
	if(newframenum < 0) {		// major seeking problem
		if(prereadbytes > 0)
			clear_message();
		return 0;
	}

	if(prereadbytes > 0) {
		while((frp->prebufferbytes_forward < prereadbytes) && !mpxplay_mpxinbuf_buffer_check(frp)) {
		}
		clear_message();
	}

	if(newframenum)
		frp->infile_infos->seektype = MPX_SEEKTYPE_NORM;
	else
		frp->infile_infos->seektype = MPX_SEEKTYPE_BOF;
	if(!(playcontrol & PLAYC_RUNNING))
		funcbit_enable(frp->infile_infos->seektype, MPX_SEEKTYPE_PAUSE);
#ifdef MPXPLAY_LINK_VIDEO
	if(frp->infile_infos->video_decoder_infos->video_res_x)
		if((displaymode & DISP_GRAPHICAL) || ((playcontrol & PLAYC_AUTOGFX) && displaymode))
			funcbit_enable(frp->infile_infos->seektype, MPX_SEEKTYPE_VIDEO);
#endif
	frp->frameNum = newframenum;

	if(!(desktopmode & DTM_EDIT_MAGNETFOLLOWSKIP))
		playlist_editorhighline_set(mvp->psip, mvp->aktfilenum);

	return 1;
}

static void start_or_stop_playing(struct mainvars *mvp)
{
	if(playcontrol & PLAYC_ABORTNEXT) {
		funcbit_enable(playcontrol, PLAYC_PAUSENEXT);
		funcbit_disable(playcontrol, PLAYC_ABORTNEXT);
	}
	if(playcontrol & PLAYC_PAUSEALL)
		funcbit_enable(playcontrol, PLAYC_PAUSENEXT);
	if(playcontrol & PLAYC_PAUSENEXT) {
		funcbit_disable(playcontrol, (PLAYC_PAUSENEXT | PLAYC_STARTNEXT));
		AU_stop(mvp->aui);
	}
	if(!(playcontrol & PLAYC_RUNNING)) {
		funcbit_enable(mvp->aui->card_controlbits, AUINFOS_CARDCNTRLBIT_DMACLEAR);
		MIXER_allfuncinit_restart();
		crossfade_reset(mvp);
	}
	if(playcontrol & PLAYC_STARTNEXT) {
		funcbit_disable(playcontrol, PLAYC_STARTNEXT);
		AU_prestart(mvp->aui);
	}
}

void mpxplay_stop_and_clear(struct mainvars *mvp, unsigned int endwaitflag)
{
	struct playlist_side_info *psip = mvp->psip;
	if(endwaitflag)
		AU_wait_and_stop(mvp->aui);
	else
		AU_stop(mvp->aui);
	mvp->aktfilenum = psip->firstsong - 1;
	playlist_randlist_clearall(psip);
	//if(psip==mvp->psie && psip->firstsong<=psip->lastentry)
	// playlist_editorhighline_set(psip,psip->firstsong);
	if(!mvp->foundfile) {
		playlist_editorhighline_set(psip, psip->firstsong);
		playlist_chkentry_enable_entries(psip);
	}
	crossfade_reset(mvp);
	playlist_close_infile(mvp->frp0, mvp);
	mpxplay_infile_close(mvp->frp0 + 1);
	funcbit_disable(playcontrol, (PLAYC_STARTNEXT | PLAYC_PAUSENEXT | PLAYC_HIGHSCAN | PLAYC_CONTINUOUS_SEEK | PLAYC_ABORTNEXT));
	playstartframe = playstartpercent = playcountframe = playcountpercent = 0;
	playstarttime = playcounttime = NULL;
	mvp->step = mvp->direction = mvp->foundfile = 0;
	mvp->newfilenum = mvp->newsong = NULL;
	mvp->idone = MPXPLAY_ERROR_INFILE_EOF;
	refdisp |= RDT_BROWSER | RDT_EDITOR | RDT_OPTIONS | RDT_HEADER | RDT_ID3INFO;
}

//-------------------------------------------------------------------------
//extern unsigned long *int24errorcount_r;
static void main_part2(struct mainvars *mvp)
{
	struct frame *frp0 = mvp->frp0, *frp1 = frp0 + 1;
	unsigned long i;
	if(playcontrol & PLAYC_RUNNING) {
		if(!(frp0->infile_infos->seektype & MPX_SEEKTYPES_CLEARBUF)) {
			i = 0;
			mpxplay_diskdrive_file_config(frp0->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_READWAIT, &i, NULL);
			newfunc_newhandler08_waitfor_threadend();
			mvp->fdone = mpxplay_mpxinbuf_buffer_check(frp0);	// read file 0.
			i = 1;
			if(mvp->fdone && (mpxplay_diskdrive_file_config(frp0->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_READWAIT, &i, NULL) > 0)) {
				if(frp0->filepos < frp0->filesize) {
					if(!frp0->readwait_endtime) {
						frp0->readwait_endtime = pds_gettimem() + 10000;	// !!! in msec
						mvp->fdone = 0;
					} else if(pds_gettimem() < frp0->readwait_endtime)
						mvp->fdone = 0;
					//else
					// display_timed_message("File read error!");
				}
			} else
				frp0->readwait_endtime = 0;
			if((prebuffertype & PREBUFTYPE_PRELOADNEXT) && !(playreplay & REPLAY_SONG) && mvp->fdone && !(frp1->buffertype & PREBUFTYPE_LOADNEXT_MASK) && (crossfadepart != CROSS_FADE)) {	// -bpn
				if(frp1->prebufferbegin) {
					struct playlist_entry_info *ehl_save = mvp->psip->editorhighline, *newfi = mvp->newfilenum, *pei = newfi;
					if((pei || (pei = playlist_get_newfilenum(mvp)))	// is nextfile?
					   && (!(mvp->aktfilenum->infobits & PEIF_INDEXED) || !(pei->infobits & PEIF_INDEXED) || (pds_strcmp(mvp->pei0->filename, pei->filename) != 0))	// isn't the same file with index(es)?
					   && playlist_open_infile(frp1, mvp, mvp->newfilenum)	// can open?
					   && mpxplay_mpxinbuf_alloc(mvp, frp1)	// can init buffer?
					   && (mpxplay_infile_fseek(frp1, 0) == 0)) {	// can seek to bof?
						funcbit_enable(frp1->buffertype, PREBUFTYPE_LOADNEXT_OK);
						refdisp |= RDT_EDITOR;
					} else {
						funcbit_enable(frp1->buffertype, PREBUFTYPE_LOADNEXT_FAILED);
						mvp->newfilenum = newfi;
					}
					playlist_editorhighline_set(mvp->psip, ehl_save);
				} else {
					display_timed_message("No buffer (free memory) to preload next file!");
					funcbit_enable(frp1->buffertype, PREBUFTYPE_LOADNEXT_FAILED);
				}
			}
			if(frp1->buffertype & PREBUFTYPE_LOADNEXT_OK) {
				i = 0;
				mpxplay_diskdrive_file_config(frp1->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_READWAIT, &i, NULL);
				mpxplay_mpxinbuf_buffer_check(frp1);	// read next file
				i = 1;
				mpxplay_diskdrive_file_config(frp1->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_READWAIT, &i, NULL);
			}
		}
		if(crossfadepart == CROSS_FADE) {
			newfunc_newhandler08_waitfor_threadend();
			mpxplay_mpxinbuf_buffer_check(frp1);	// read file 1.
		}

		if(!funcbit_test(intsoundcontrol, INTSOUND_DECODER))	// if not interrupt decoding
			mvp->idone = mpxplay_infile_decode(mvp->aui);	// then mpxplay_infile_decode called from here

		if(mvp->idone != MPXPLAY_ERROR_INFILE_OK) {
			if((mvp->idone == MPXPLAY_ERROR_INFILE_EOF) || ((mvp->idone == MPXPLAY_ERROR_INFILE_NODATA) && mvp->fdone)) {	// if end of file 0.
				crossfade_part_step(mvp);
				mvp->adone = ADONE_EOF;	// next song
			} else
				mvp->idone = MPXPLAY_ERROR_INFILE_OK;
		}
		if(mvp->cfi->usecrossfade && (mvp->idone == MPXPLAY_ERROR_INFILE_OK))
			crossfade_auto_control(frp0, frp1, mvp);
		if(playcountframe && (frp0->framecounter >= playcountframe))
			mvp->adone = ADONE_EOF;
	}
	if(frp0->index_end && (frp0->frameNum >= frp0->index_end))
		mvp->adone = ADONE_EOF;

//------ display and control funcs -------------------------------------
	if(!funcbit_test(intsoundcontrol, INTSOUND_TSR))	// if not TSR mode
		mpxplay_timer_execute_maincycle_funcs();
//----------------------------------------------------------------------

	if(mvp->newsong) {
		struct playlist_side_info *psi = mvp->psie;
		if(psi->psio->editsidetype & PLT_DOOMQUEUE)	// jukebox left side handling
			playlist_jukebox_add_entry(mvp, psi);
	}
	if(mvp->newsong || (mvp->step != 0)) {
		struct crossfade_info *cfi = mvp->cfi;
		if((crossfadepart == CROSS_OUT) || (crossfadepart == CROSS_LOAD)) {
			playlist_get_newfilenum(mvp);
			playlist_randlist_resetsignflag(mvp->newfilenum);
			refdisp |= RDT_EDITOR;
			cfi->crosswait = 10;
		} else {
			if(cfi->usecrossfade && (playcontrol & PLAYC_RUNNING) && !cfi->crosswait) {
				crossfade_reset(mvp);
				cfi->crossfadebegin = frp0->frameNum;
				crossfade_manual_set(mvp, CROSS_OUT);
			} else {
				if((mvp->step < 0) || mvp->newsong || !mvp->newfilenum) {
					//if(playrand ||
					if(mvp->aktfilenum >= mvp->psip->firstsong) {
						playlist_randlist_resetsignflag(mvp->newfilenum);
						mvp->newfilenum = NULL;
					}
					if(playlist_get_newfilenum(mvp))
						mvp->adone = ADONE_RESTART;
				} else {
					mvp->step = 0;
					mvp->newsong = NULL;
					mvp->adone = ADONE_RESTART;
				}
			}
		}
	}
	if(mvp->adone) {
		if(playcountsong) {
			struct playlist_entry_info *pei;
			if((mvp->adone == ADONE_EOF) && (playreplay & REPLAY_SONG) && !mvp->newfilenum) {	// repeat song
				mvp->adone = mvp->fdone = 0;
				mvp->seek_absolute = frp0->index_start + 1;
				check_seeking(mvp);
				mvp->idone = MPXPLAY_ERROR_INFILE_OK;
				playcountsong--;
				goto mainpart2_end;
			}
			if((crossfadepart != CROSS_OUT && crossfadepart != CROSS_LOAD) || !(playcontrol & PLAYC_RUNNING)) {
				pei = mvp->newfilenum;
				if(!pei) {
					mvp->direction = 0;
					pei = playlist_get_newfilenum(mvp);
				}
				if((pei >= mvp->psip->firstsong) && (pei <= mvp->psip->lastentry)
				   && (mvp->pei0->infobits & PEIF_INDEXED) && (pei->infobits & PEIF_INDEXED)
				   && (pds_strcmp(mvp->pei0->filename, pei->filename) == 0)
					) {			// gapless indexed playing (don't skip to next song, continue the current one)
					unsigned long curr_petime = mvp->pei0->petime;
					if((!(desktopmode & DTM_EDIT_MAGNETFOLLOWSKIP) || (mvp->aktfilenum == mvp->psie->editorhighline)) && !(mvp->psip->editsidetype & PLT_DOOMQUEUE))
						playlist_editorhighline_set(mvp->psip, pei);
					if((mvp->psip->editsidetype & PLT_DOOMQUEUE) && ((mvp->psip != mvp->psie) || (mvp->direction == 0) || (mvp->adone == ADONE_EOF))) {
						playlist_editlist_delete_entry_manual(mvp->psip, mvp->aktfilenum);
						pei = mvp->newfilenum;
					}
					mvp->aktfilenum = pei;
					mvp->newfilenum = NULL;
					playlist_pei0_set(mvp, pei, 0);
					if((mvp->adone == ADONE_EOF) && !(playcontrol & PLAYC_HIGHSCAN) && curr_petime && (curr_petime == pei->pstime)) {
						mpxplay_calculate_index_start_end(mvp->frp0, mvp, pei);
						mvp->seek_absolute = 0;
						mvp->seek_relative = 0;
					} else {
						if(!(playcontrol & PLAYC_HIGHSCAN)) {
							playstartframe = playstartpercent = 0;
							playstarttime = NULL;
						}
						mvp->adone = mvp->fdone = 0;
						mvp->seek_absolute = mpxplay_calculate_playstartpos(mvp->frp0, mvp) + 1;
						mvp->seek_relative = 0;
						check_seeking(mvp);
						if(mvp->adone)
							goto jump_to_next;	// failed seeking
						mvp->idone = MPXPLAY_ERROR_INFILE_OK;
					}
					if(playrand) {
						if(mvp->direction < 0)	//
							playlist_randlist_popq();	// pop prev/curr-file
						playlist_randlist_pushq(mvp->psip, mvp->aktfilenum);	// push curr
					}
					mvp->adone = 0;
					mvp->direction = 0;
					mvp->foundfile = 1;
					mvp->frp0->framecounter = 0;
					crossfade_initvar(mvp, mvp->cfi);
					playcountsong--;
					if((outmode & OUTMODE_TYPE_FILE) || (mvp->aui->card_handler->infobits & SNDCARD_SETRATE))
						AU_setrate(mvp->aui, mvp->frp0->infile_infos->audio_decoder_infos);
					start_or_stop_playing(mvp);
					refdisp |= RDT_BROWSER | RDT_EDITOR | RDT_OPTIONS | RDT_HEADER | RDT_ID3INFO;
					funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_NEWFILE);
					goto mainpart2_end;
				}
			}
		}						// else finish it and jump to next
	  jump_to_next:
		if(!(playcontrol & PLAYC_HIGHSCAN)) {
			playstartpercent = playstartframe = playcountframe = playcountpercent = 0;
			playstarttime = playcounttime = NULL;
		}
		mvp->partselect = 1;
	} else {
#if defined(MPXPLAY_WIN32)
		if((intsoundcontrol & INTSOUND_DECODER) && mpxplay_check_buffers_full(mvp)
		   && ((intsoundcontrol & INTSOUND_TSR) || !funcbit_test(mpxplay_signal_events, MPXPLAY_SIGNALMASK_OTHER))
			) {
			Sleep(1000 / INT08_CYCLES_NEW);
		}
#elif defined(__DOS__)
		if((intsoundcontrol & INTSOUND_DECODER) && mpxplay_check_buffers_full(mvp)
		   && !(intsoundcontrol & INTSOUND_TSR) && !funcbit_test(mpxplay_signal_events, MPXPLAY_SIGNALMASK_OTHER)
			) {
			pds_cpu_hlt();		// int08 wakes up
		}
#endif
		if(!(playcontrol & PLAYC_RUNNING))
			AU_pause_process(mvp->aui);
	}
  mainpart2_end:
	funcbit_disable(mpxplay_signal_events, MPXPLAY_SIGNALMASK_OTHER);

/*{
 char sout[32];
 static unsigned int count;
 sprintf(sout,"err:%8.8X %d",*int24errorcount_r,count);
 display_message(1,0,sout);
}*/
}

unsigned int mpxplay_check_buffers_full(struct mainvars *mvp)
{
	if(!(playcontrol & PLAYC_RUNNING) || ((mvp->aui->card_infobits & AUINFOS_CARDINFOBIT_DMAFULL) && (mvp->frp0->buffertype & PREBUFTYPE_FILLED)))
		return 1;
	return 0;
}

//-------------------------------------------------------------------------
static void check_seeking(struct mainvars *mvp)
{
	struct frame *frp = mvp->frp0;
	unsigned int intsoundcntrl_save;

	if(((mvp->seek_relative != 0) || mvp->seek_absolute) && frp->infile_funcs && ((MIXER_var_mutelen < 3) || !(playcontrol & PLAYC_RUNNING) || mvp->seek_absolute)) {
		if(mvp->seek_absolute)
			mvp->seek_absolute--;
		else
			mvp->seek_absolute = frp->frameNum + mvp->seek_relative;
		if(mvp->seek_absolute < frp->index_start) {
			mvp->step = -1;
			crossfade_reset(mvp);
			mvp->cfi->crosswait = 15;
		} else {
			long framenum_get;
			if(frp->index_end && (mvp->seek_absolute >= frp->index_end) && (mvp->seek_relative >= 0))
				framenum_get = MPXPLAY_ERROR_INFILE_EOF;
			else {
				if(mvp->seek_absolute < (frp->index_start + 100) && mvp->seek_relative <= 0)
					mvp->seek_absolute = frp->index_start;
				if(MIXER_var_mutelen)
					MIXER_var_mutelen = 255;
				//if(MIXER_getfunction("MIX_MUTE"))
				// MIXER_setfunction("MIX_MUTE",MIXER_SETMODE_ABSOLUTE,255);
				framenum_get = mpxplay_infile_fseek(mpxplay_mpxinbuf_seekhelper_init(frp), mvp->seek_absolute);
			}

			if(framenum_get == MPXPLAY_ERROR_INFILE_EOF) {
				mpxplay_mpxinbuf_seekhelper_close(frp);
				MIXER_setfunction("MIX_MUTE", MIXER_SETMODE_RESET, 0);
				if(crossfadepart == CROSS_OUT)
					crossfade_manual_set(mvp, CROSS_LOAD);
				mvp->seek_relative = 0;
				mvp->adone = ADONE_RESTART;
			} else {
				if(!(playcontrol & PLAYC_RUNNING)) {
					funcbit_enable(mvp->aui->card_controlbits, AUINFOS_CARDCNTRLBIT_DMACLEAR);
					MIXER_allfuncinit_restart();
				}
				if(mvp->seek_absolute)
					MIXER_setfunction("MIX_MUTE", MIXER_SETMODE_ABSOLUTE, 4);
				else
					MIXER_setfunction("MIX_MUTE", MIXER_SETMODE_RESET, 0);

				MPXPLAY_INTSOUNDDECODER_DISALLOW;
				mpxplay_mpxinbuf_seekhelper_close(frp);
				frp->frameNum = framenum_get;
				if(mvp->seek_absolute) {
					frp->infile_infos->seektype = MPX_SEEKTYPE_NORM;
					funcbit_enable(mvp->aui->card_controlbits, AUINFOS_CARDCNTRLBIT_DMADONTWAIT);
				} else {
					frp->infile_infos->seektype = MPX_SEEKTYPE_BOF;
					clear_volnum();
					if(playcontrol & PLAYC_RUNNING)
						MIXER_allfuncinit_restart();
				}
				if(!(playcontrol & PLAYC_RUNNING))
					funcbit_enable(frp->infile_infos->seektype, MPX_SEEKTYPE_PAUSE);
#ifdef MPXPLAY_LINK_VIDEO
				if((displaymode & DISP_GRAPHICAL) && frp->infile_infos->video_decoder_infos->video_res_x)
					funcbit_enable(frp->infile_infos->seektype, MPX_SEEKTYPE_VIDEO);
#endif

				if((crossfadepart == CROSS_OUT) && (mvp->seek_relative < 0)) {
					crossfade_reset(mvp);
					mvp->cfi->crosswait = 5;
				}

				MPXPLAY_INTSOUNDDECODER_ALLOW;

				if(!(playcontrol & PLAYC_CONTINUOUS_SEEK))
					mvp->seek_relative = 0;
			}
		}
		mvp->seek_absolute = mvp->sndempty = 0;
	}
}

//**************************************************************************

static void crossfade_auto_control(struct frame *frp0, struct frame *frp1, struct mainvars *mvp)
{
	struct playlist_side_info *psip = mvp->psip;
	struct crossfade_info *cfi = mvp->cfi;
	if(!cfi->crosswait) {
		switch (crossfadepart) {
		case CROSS_CLEAR:
			if((frp0->frameNum >= cfi->crossfadebegin)
			   && (!(psip->editsidetype & PLT_DOOMQUEUE) || (psip->lastentry > psip->firstentry))
			   && !(playcontrol & (PLAYC_PAUSEALL | PLAYC_PAUSENEXT))) {
				if((mvp->pei0->infobits & PEIF_INDEXED) && !(playreplay & REPLAY_SONG)) {	// !!! don't do crossfade between continuous indexes
					struct playlist_entry_info *pei = mvp->newfilenum;
					if(!pei) {
						mvp->direction = 0;
						pei = playlist_get_newfilenum(mvp);
					}
					if((pei >= mvp->psip->firstsong) && (pei <= mvp->psip->lastentry)
					   && (pei->infobits & PEIF_INDEXED) && (mvp->pei0->petime == pei->pstime)
					   && (pds_strcmp(mvp->pei0->filename, pei->filename) == 0)
						) {
						//cfi->crosswait=15;
						break;
					}
				}
				crossfade_manual_set(mvp, CROSS_OUT);
			}
			break;
		case CROSS_OUT:
			if(frp0->frameNum >= (cfi->crossfadebegin + cfi->crossfadepoint)) {
				if((!(psip->editsidetype & PLT_DOOMQUEUE) || (psip->lastentry > psip->firstentry))
				   && !(playcontrol & (PLAYC_PAUSEALL | PLAYC_PAUSENEXT))) {
					crossfade_manual_set(mvp, CROSS_LOAD);
				} else {
					if(frp0->frameNum > cfi->crossfadeend)
						mvp->adone = ADONE_EOF;
				}
			}
			break;
		case CROSS_FADE:
			if((frp1->frameNum >= cfi->crossfadeend) || (frp0->frameNum >= (frp0->index_start + cfi->crossfade_in_len)))
				crossfade_manual_set(mvp, CROSS_IN);
			break;
		case CROSS_IN:
			if(!(cfi->crossfadetype & CFT_FADEIN) || (frp0->frameNum >= (frp0->index_start + cfi->crossfade_in_len)))
				crossfade_manual_set(mvp, CROSS_CLEAR);
			break;
		}
	} else {
		if(int08counter >= (cfi->crosscounter + REFRESH_DELAY_JOYMOUSE)) {
			cfi->crosswait--;
			cfi->crosscounter = int08counter;
		}
	}
}

static void crossfade_manual_set(struct mainvars *mvp, unsigned int newpart)
{
	struct playlist_side_info *psip = mvp->psip;
	struct crossfade_info cfi_new;
	unsigned int intsoundcntrl_save;

	pds_memcpy(&cfi_new, mvp->cfi, sizeof(struct crossfade_info));

	switch (newpart) {
	case CROSS_OUT:
		mvp->cfi->crosswait = 0;
		if(mvp->newfilenum)
			mvp->step = 0;
		else {
			if((playreplay & REPLAY_SONG) && mvp->aktfilenum >= psip->firstsong && mvp->aktfilenum <= psip->lastentry)
				mvp->newfilenum = mvp->aktfilenum;
			else {
				playlist_get_newfilenum(mvp);
				playlist_randlist_resetsignflag(mvp->newfilenum);
			}
		}
		if(mvp->newfilenum) {
			cfi_new.crossfadeend = cfi_new.crossfadebegin + cfi_new.crossfade_out_len;
		} else {
			mvp->cfi->crosswait = 15;
			crossfade_initvar(mvp, mvp->cfi);
			return;
		}
		refdisp |= RDT_EDITOR;
		break;
	case CROSS_LOAD:
		mvp->adone = ADONE_EOF;
		break;
	case CROSS_FADE:
		crossfade_initvar(mvp, &cfi_new);
		break;					// at open_new_infile
	case CROSS_CLEAR:
		crossfade_initvar(mvp, &cfi_new);
		refdisp |= RDT_EDITOR;
	case CROSS_IN:
		mpxplay_infile_reset(mvp->frp0 + 1);
		mpxplay_timer_addfunc(mpxplay_infile_close, mvp->frp0 + 1, MPXPLAY_TIMERFLAG_INDOS, 0);
	}

	MPXPLAY_INTSOUNDDECODER_DISALLOW;

	crossfadepart = newpart;
	pds_smp_memcpy((char *)mvp->cfi, (char *)&cfi_new, sizeof(struct crossfade_info));

	MIXER_setfunction("MIX_CROSSFADER", MIXER_SETMODE_ABSOLUTE, newpart);

	MPXPLAY_INTSOUNDDECODER_ALLOW;
	refdisp |= RDT_OPTIONS;
}

void crossfade_part_step(struct mainvars *mvp)
{
	switch (crossfadepart) {
	case CROSS_OUT:
		crossfade_manual_set(mvp, CROSS_LOAD);
		break;
	case CROSS_FADE:
		if(mvp->cfi->crossfadetype & CFT_FADEIN) {
			crossfade_manual_set(mvp, CROSS_IN);
			break;
		}
	case CROSS_IN:
		crossfade_manual_set(mvp, CROSS_CLEAR);
	}
}

static void crossfade_initvar(struct mainvars *mvp, struct crossfade_info *cfi)
{
	struct frame *frp0 = mvp->frp0;
	if(cfi->crossfadelimit)
		cfi->crossfadepoint = cfi->crossfade_out_len;
	cfi->crossfadebegin = ((frp0->index_end) ? frp0->index_end : frp0->allframes) - cfi->crossfade_out_len;
}

void crossfade_reset(struct mainvars *mvp)
{
	if(crossfadepart) {
		if(crossfadepart != CROSS_OUT)	// ???
			mvp->newfilenum = NULL;	//
		mvp->direction = 0;
		crossfade_manual_set(mvp, CROSS_CLEAR);
	}
}

//-------------------------------------------------------------------------
//extra exit infos
#define EEI_INFILENAME 1
#define EEI_CRASH      2

static struct exitinfo_s {
	char *exiterrmsg;
	unsigned int extrainfo;
} exitinfo[] = {
	{
	"", 0},						//0
	{
	"Abnormal termination", EEI_INFILENAME},	//1
	{
	"Floating point error", EEI_INFILENAME},	//2
	{
	"Illegal operation", EEI_INFILENAME},	//3
	{
	"Interrupted by CTRL-C", EEI_INFILENAME},	//4
	{
	"Invalid access to storage", EEI_INFILENAME},	//5
	{
	"Request for program termination", EEI_INFILENAME},	//6
	{
	"Interrupted by CTRL-BREAK", EEI_INFILENAME},	//7
	{
	"Soundcard init failed!", 0},	//8
	{
	"Not enough extended memory (or extender's limit reached)!", 0},	//9
	{
	"Not enough conventional memory!", 0},	//10
	{
	"No file(s) found (empty playlist)!", 0},	//11
	{
	"Can't open/write output file! (exists or disk full or sharing violation)", EEI_INFILENAME},	//12
	{
	"Program crashed (DIVISION BY ZERO) (bad environment, mpxplay.ini or audio file)", EEI_INFILENAME | EEI_CRASH},	//13
	{
	"Program crashed (EXCEPTION ERROR) (bad environment, mpxplay.ini or audio file)", EEI_INFILENAME | EEI_CRASH}	//14
};

void mpxplay_close_program(unsigned int exitcode)
{
	struct mainvars *mvp = &mvps;

	if(mvp->aui)
		AU_stop(mvp->aui);
	mpxplay_restore_startdir();
	funcbit_disable(intsoundcontrol, INTSOUND_DECODER);
	if(!exitcode && mvp->frp0)
		exitcode = ((100 * mvp->frp0->frameNum) / mvp->frp0->allframes) + 16;
	mpxplay_display_close(mvp);
	if(mvp->aui)
		AU_close(mvp->aui);
	MIXER_allfuncinit_close();
	playlist_savelist_save_playlist(mvp, NULL, NULL, playlistsave);
	mpxplay_control_configfile_parts_saveini();	// save variables; save startup;
	mpxplay_control_configfile_parts_close();	// hw close
	mpxplay_control_configfile_close();	// close mpxplay.ini
	mpxplay_playlist_close();
	mpxplay_infile_deinit();
	mpxplay_mpxinbuf_close(mvp);

	if(exitcode < 15) {
		if((exitinfo[exitcode].extrainfo & EEI_INFILENAME) && (mvp->pei0) && (mvp->pei0->filename))
			display_warning_message(mvp->pei0->filename);
		display_warning_message(exitinfo[exitcode].exiterrmsg);
		if(!(exitinfo[exitcode].extrainfo & EEI_CRASH))
			newfunc_exception_handlers_close();
	}
	newfunc_close();
	newfunc_error_handlers_close();

	if(shutdownatx_enabled)
		pds_shutdown_atx();

	exit(exitcode);
}
