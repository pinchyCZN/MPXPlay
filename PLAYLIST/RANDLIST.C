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
//function: random playing

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_OUTPUT stdout

#include "newfunc\newfunc.h"
#include "playlist.h"
#include "mpxinbuf.h"

#define RANDQUEUE_SIZE 256

#define playlist_randlist_getsignflag(pei) (pei->infobits&PEIF_RNDSIGNED)
#define playlist_randlist_getplayflag(pei) (pei->infobits&PEIF_RNDPLAYED)
#define playlist_randlist_getflags(pei) (pei->infobits&(PEIF_RNDSIGNED|PEIF_RNDPLAYED))

static void playlist_randlist_deletesignflag(struct playlist_entry_info *pei);
static void playlist_randlist_setplayflag(struct playlist_entry_info *pei);
static void playlist_randlist_deleteplayflag(struct playlist_entry_info *pei);

extern unsigned int playrand, playreplay;
extern mainvars mvps;

static int randplaycounter, randsigncounter;
static struct playlist_entry_info *rq[RANDQUEUE_SIZE];

static void playlist_randlist_clearpeiflags(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei = psi->firstsong, *lastentry = psi->lastentry;
	if(lastentry >= pei) {
		do {
			funcbit_disable(pei->infobits, (PEIF_RNDSIGNED | PEIF_RNDPLAYED));
		} while((++pei) <= lastentry);
	}
	randsigncounter = randplaycounter = 0;
}

static void playlist_randlist_clearqueue(void)
{
	pds_memset((&rq[0]), 0, RANDQUEUE_SIZE * sizeof(playlist_entry_info *));
}

void playlist_randlist_clearall(struct playlist_side_info *psi)
{
	if(playrand) {
		playlist_randlist_clearpeiflags(psi);
		playlist_randlist_clearqueue();
	}
	playlist_skiplist_reset_loadnext(psi->mvp);
}

//remove entry from the queue and reset its flag
void playlist_randlist_delete(struct playlist_entry_info *pei)
{
	if(playrand && pei) {
		struct playlist_entry_info **rqp = &rq[0];
		unsigned int i = 0;
		do {
			if(!*rqp)
				break;
			if(*rqp == pei) {
				pds_memcpy((rqp), (rqp + 1), (RANDQUEUE_SIZE - i - 1) * sizeof(playlist_entry_info *));
				rq[RANDQUEUE_SIZE - 1] = NULL;
				break;
			}
			rqp++;
		} while(++i < RANDQUEUE_SIZE);
		playlist_randlist_deletesignflag(pei);
		playlist_randlist_deleteplayflag(pei);
	}
}

// correct random-queue based on pei->myself
void playlist_randlist_correctq(struct playlist_side_info *psi, struct playlist_entry_info *firstentry, struct playlist_entry_info *lastentry)
{
	if(playrand && (psi == psi->mvp->psip)) {
		if(!firstentry)
			firstentry = psi->firstsong;
		if(!lastentry)
			lastentry = psi->lastentry;
		if(lastentry >= firstentry) {
			struct playlist_entry_info **rqp = &rq[0];
			unsigned int i = RANDQUEUE_SIZE;
			do {
				struct playlist_entry_info *pei, *rqv;
				rqv = *rqp;
				if(!rqv)
					break;
				pei = firstentry;
				do {
					if(rqv == pei->myself) {
						*rqp = pei;
						break;
					}
				} while((++pei) <= lastentry);
				rqp++;
			} while(--i);
		}
	}
}

//exchange queue elements (at playlist_swap_entries())
void playlist_randlist_xchq(struct playlist_entry_info *pei1, struct playlist_entry_info *pei2)
{
	if(playrand) {
		struct playlist_entry_info **rqp = &rq[0];
		unsigned int i = RANDQUEUE_SIZE;
		do {
			struct playlist_entry_info *rqv = *rqp;
			if(!rqv)
				break;
			if(rqv == pei1)
				*rqp = pei2;
			else if(rqv == pei2)
				*rqp = pei1;
			rqp++;
		} while(--i);
	}
}

unsigned int playlist_getsongcounter(struct mainvars *mvp)
{
	if(playrand)
		return randplaycounter;
	else {
		struct playlist_side_info *psip = mvp->psip;
		if(mvp->aktfilenum >= psip->firstsong)
			return (mvp->aktfilenum - psip->firstsong + 1);
	}
	return 0;
}

void playlist_randlist_pushq(struct playlist_side_info *psi, struct playlist_entry_info *pei)
{
	if(playrand && (pei >= psi->firstsong) && (pei <= psi->lastentry)) {
		playlist_randlist_setsignflag(pei);
		playlist_randlist_setplayflag(pei);
		pds_qmemcpyr((&rq[1]), (&rq[0]), RANDQUEUE_SIZE - 1);
		rq[0] = pei;
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rnd push: %4d", pei - psi->firstsong + 1);
		//mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"rnd push: %4d  queue: %d %d %d",pei-psi->firstsong+1,rq[0]-psi->firstsong+1,rq[1]-psi->firstsong+1,rq[2]-psi->firstsong+1);
	}
}

playlist_entry_info *playlist_randlist_popq(void)
{
	playlist_randlist_deletesignflag(rq[0]);
	playlist_randlist_deleteplayflag(rq[0]);
	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rnd popq1: 0:%4d 1:%4d", rq[0] - mvps.psip->firstsong + 1, rq[1] - mvps.psip->firstsong + 1);
	pds_memcpy((&rq[0]), (&rq[1]), (RANDQUEUE_SIZE - 1) * sizeof(playlist_entry_info *));
	rq[RANDQUEUE_SIZE - 1] = NULL;
	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rnd popq2: 0:%4d 1:%4d", rq[0] - mvps.psip->firstsong + 1, rq[1] - mvps.psip->firstsong + 1);
	return (rq[0]);
}

void playlist_randlist_setsignflag(struct playlist_entry_info *pei)
{
	if(pei)
		if(!playlist_randlist_getsignflag(pei) && !playlist_randlist_getplayflag(pei)) {
			funcbit_enable(pei->infobits, PEIF_RNDSIGNED);
			randsigncounter++;
		}
}

void playlist_randlist_resetsignflag(struct playlist_entry_info *pei)
{
	if(pei)
		if(playlist_randlist_getsignflag(pei) && !playlist_randlist_getplayflag(pei)) {
			funcbit_disable(pei->infobits, PEIF_RNDSIGNED);
			if(randsigncounter > 0)
				randsigncounter--;
		}
}

static void playlist_randlist_deletesignflag(struct playlist_entry_info *pei)
{
	if(pei)
		if(playlist_randlist_getsignflag(pei)) {
			funcbit_disable(pei->infobits, PEIF_RNDSIGNED);
			if(randsigncounter > 0)
				randsigncounter--;
		}
}

static void playlist_randlist_setplayflag(struct playlist_entry_info *pei)
{
	if(pei)
		if(!playlist_randlist_getplayflag(pei)) {
			funcbit_enable(pei->infobits, PEIF_RNDPLAYED);
			randplaycounter++;
		}
}

static void playlist_randlist_deleteplayflag(struct playlist_entry_info *pei)
{
	if(pei)
		if(playlist_randlist_getplayflag(pei)) {
			funcbit_disable(pei->infobits, PEIF_RNDPLAYED);
			if(randplaycounter > 0)
				randplaycounter--;
		}
}

struct playlist_entry_info *playlist_randlist_getnext(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei, *newrandpei = NULL;
	long allsongs = (psi->lastentry - psi->firstsong) + 1;
	int rn;

	if(allsongs < 2)
		return NULL;
	do {
		if(randsigncounter >= allsongs) {
			if(playreplay & REPLAY_LIST) {
				playlist_randlist_clearpeiflags(psi);
				randsigncounter = randplaycounter = 0;
			} else
				break;
		}
		rn = pds_rand(allsongs - randsigncounter);
		for(pei = psi->firstsong; pei <= psi->lastentry; pei++) {
			if(!playlist_randlist_getflags(pei)) {
				if(!rn) {		// we search the rn. not-played song
					newrandpei = pei;
					break;
				}
				rn--;
			}
		}
		if(newrandpei) {
			playlist_randlist_setsignflag(newrandpei);
			break;
		}
		randsigncounter++;		// bugfix to avoid endless cycle (if the randsigncounter and the number of (signed) rand-flags don't match)
	} while(1);

	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rnd next: %4d  count: %d/%d", (long)(newrandpei - psi->firstsong) + 1, randsigncounter, allsongs);

	return newrandpei;
}

/*struct playlist_entry_info *playlist_randlist_getprev(struct playlist_entry_info *loc_newfilenum)
{
 struct playlist_entry_info *prevpei;
 prevpei=playlist_randlist_popq();
 if(!prevpei){
  playlist_randlist_deletesignflag(loc_newfilenum);
  playlist_randlist_deleteplayflag(loc_newfilenum);
  prevpei=loc_newfilenum;
 }
 return prevpei;
}*/

struct playlist_entry_info *playlist_randlist_getprev(struct playlist_entry_info *loc_newfilenum)
{
	struct playlist_entry_info *prevpei;
	if(rq[0] && rq[1])			// min 2 elements (current_pei,previous_pei)
		prevpei = playlist_randlist_popq();
	else {						// queue is empty
		playlist_randlist_popq();	// ??? pop last entry
		playlist_randlist_deletesignflag(loc_newfilenum);
		playlist_randlist_deleteplayflag(loc_newfilenum);
		prevpei = loc_newfilenum;
	}
	return prevpei;
}

//-prn2
void playlist_randlist_randomize_side(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	int allsongs = psi->lastentry - psi->firstsong;
	for(pei = psi->firstsong; pei <= psi->lastentry; pei++) {
		unsigned int rn = pds_rand(allsongs);
		playlist_swap_entries(psi, pei, psi->firstsong + rn);
	}
}
