/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: channel mapping 0 implementation
 last mod: $Id: mapping0.c,v 1.55 2003/10/01 00:00:00 PDSoft Exp $

 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "codec.h"
#include "codecint.h"
#include "codebook.h"
#include "window.h"
#include "registry.h"
#include "os.h"
#include "mdct.h"

static void mapping0_free_info(vorbis_info_mapping * i)
{
	vorbis_info_mapping0 *info = (vorbis_info_mapping0 *) i;
	if(info) {
		if(info->coupling_mag)
			_ogg_free(info->coupling_mag);
		if(info->coupling_ang)
			_ogg_free(info->coupling_ang);
		if(info->chmuxlist)
			_ogg_free(info->chmuxlist);
		if(info->floorsubmap)
			_ogg_free(info->floorsubmap);
		if(info->residuesubmap)
			_ogg_free(info->residuesubmap);
		if(info->pcmbundle)
			_ogg_free(info->pcmbundle);
		if(info->nonzero)
			_ogg_free(info->nonzero);
		if(info->floormemo)
			_ogg_free(info->floormemo);
		_ogg_free(info);
	}
}

static int ilog(unsigned int v)
{
	int ret = 0;
	if(v)
		--v;
	while(v) {
		ret++;
		v >>= 1;
	}
	return (ret);
}

static vorbis_info_mapping *mapping0_unpack(vorbis_info * vi, oggpack_buffer * opb)
{
	int i;
	vorbis_info_mapping0 *info = _ogg_calloc(1, sizeof(*info));
	codec_setup_info *ci = vi->codec_setup;

	if(!info)
		goto err_out;

	_ogg_memset(info, 0, sizeof(*info));

	if(oggpack_read1(opb))
		info->submaps = oggpack_read24(opb, 4) + 1;
	else
		info->submaps = 1;

	if(oggpack_read1(opb)) {
		info->coupling_steps = oggpack_read24(opb, 8) + 1;

		info->coupling_mag = _ogg_malloc(sizeof(*info->coupling_mag) * info->coupling_steps);
		info->coupling_ang = _ogg_malloc(sizeof(*info->coupling_ang) * info->coupling_steps);
		if(!info->coupling_mag || !info->coupling_ang)
			goto err_out;

		for(i = 0; i < info->coupling_steps; i++) {
			int testM = info->coupling_mag[i] = oggpack_read24(opb, ilog(vi->channels));
			int testA = info->coupling_ang[i] = oggpack_read24(opb, ilog(vi->channels));

			if(testM < 0 || testA < 0 || testM == testA || testM >= vi->channels || testA >= vi->channels)
				goto err_out;
		}
	}

	if(oggpack_read24(opb, 2) != 0)
		goto err_out;

	info->chmuxlist = _ogg_calloc(vi->channels, sizeof(*info->chmuxlist));
	if(!info->chmuxlist)
		goto err_out;

	if(info->submaps > 1) {
		for(i = 0; i < vi->channels; i++) {
			info->chmuxlist[i] = oggpack_read24(opb, 4);
			if(info->chmuxlist[i] >= info->submaps)
				goto err_out;
		}
	}

	info->floorsubmap = _ogg_malloc(sizeof(*info->floorsubmap) * info->submaps);
	info->residuesubmap = _ogg_malloc(sizeof(*info->residuesubmap) * info->submaps);
	if(!info->floorsubmap || !info->residuesubmap)
		goto err_out;

	for(i = 0; i < info->submaps; i++) {
		oggpack_adv(opb, 8);
		info->floorsubmap[i] = oggpack_read24(opb, 8);
		if(info->floorsubmap[i] >= ci->floors)
			goto err_out;
		info->residuesubmap[i] = oggpack_read24(opb, 8);
		if(info->residuesubmap[i] >= ci->residues)
			goto err_out;
	}

	info->pcmbundle = _ogg_malloc(sizeof(*info->pcmbundle) * vi->channels);
	info->nonzero = _ogg_malloc(sizeof(*info->nonzero) * vi->channels);
	info->floormemo = _ogg_malloc(sizeof(*info->floormemo) * vi->channels);
	if(!info->pcmbundle || !info->nonzero || !info->floormemo)
		goto err_out;

	return info;

  err_out:
	mapping0_free_info(info);
	return (NULL);
}

#ifdef OGG_SPECTRUM_ANALISER	// defined in os_types.h

static unsigned int analiser_bandnum;
static unsigned long *analiser_banddata;

static unsigned long lasts[32], currs[32];
static float scale = 48000.0f;

void ogg_vorbis_analiser_config(unsigned int bn, unsigned long *bd)
{
	analiser_bandnum = bn;
	analiser_banddata = bd;
}

void ogg_vorbis_analiser_clear(void)
{
	memset(&lasts[0], 0, 32 * sizeof(unsigned long));
	memset(&currs[0], 0, 32 * sizeof(unsigned long));
}

ogg_double_t *asm_band_add(unsigned int, ogg_double_t *);

static void ogg_calculate_analiser_bands(ogg_double_t * pcm, unsigned int currbs, unsigned int lastch)
{
	unsigned int nbs, bands = analiser_bandnum;
	unsigned long anl, *ap, *lp, *cp = &currs[0];

	nbs = currbs >> 6;			// currbs/64 (/2 /32)
	if(!nbs)
		return;

	if(lastch) {
		ap = analiser_banddata;
		lp = &lasts[0];
	}

	do {
#ifdef OGG_USE_ASM
#ifdef __WATCOMC__
#pragma aux asm_band_add=\
  "fldz"\
  "back:"\
   "fld dword ptr [esi]"\
   "add esi,4"\
   "fabs"\
   "dec edx"\
   "fadd"\
  "jnz back"\
  "fmul dword ptr scale"\
  "fistp dword ptr anl"\
  parm[edx][esi] value[esi];
		pcm = asm_band_add(nbs, pcm);
#endif
#else
		float anf = 0.0f;
		unsigned int k = nbs;
		do {
			anf += fabs(*pcm++);
		} while(--k);
		anl = (unsigned long)(anf * scale);
#endif

		*cp += anl;

		if(lastch) {
			unsigned long lnl = *lp;
			anl = *cp;
			*lp = anl;

			anl = (anl + lnl) >> 1;

			if(bands < analiser_bandnum) {
				unsigned long ap1 = *(ap - 1);	// create fake high bands (for a nicer displaying)
				if(anl < 100 && anl < (ap1 >> 1))
					anl = ap1 >> 1;
			}
			if(*ap)
				*ap = (*ap + anl) >> 1;
			else
				*ap = anl;
			ap++;
			*cp = 0;
			lp++;
		}

		cp++;
	} while(--bands);
}

#endif

static int mapping0_inverse(vorbis_block * vb, vorbis_info_mapping * l)
{
	vorbis_dsp_state *vd = vb->vd;
	vorbis_info *vi = vd->vi;
	codec_setup_info *ci = vi->codec_setup;
	backend_lookup_state *b = vd->backend_state;
	vorbis_info_mapping0 *info = (vorbis_info_mapping0 *) l;

	unsigned int i, j;
	unsigned long n = vb->pcmend;

	for(i = 0; i < vi->channels; i++) {
		int flrsubmap = info->floorsubmap[info->chmuxlist[i]];

		info->floormemo[i] = _floor_P[ci->floor_type[flrsubmap]]->inverse1(vb, b->flr[flrsubmap], i);
		if(info->floormemo[i])
			info->nonzero[i] = 1;
		else
			info->nonzero[i] = 0;
		vorbis_fclear_block(vb->pcm[i], n >> 1);
	}

	for(i = 0; i < info->coupling_steps; i++) {
		if(info->nonzero[info->coupling_mag[i]] || info->nonzero[info->coupling_ang[i]]) {
			info->nonzero[info->coupling_mag[i]] = 1;
			info->nonzero[info->coupling_ang[i]] = 1;
		}
	}

	for(i = 0; i < info->submaps; i++) {
		unsigned int ch_in_bundle = 0;
		for(j = 0; j < vi->channels; j++) {
			if((info->chmuxlist[j] == i) && info->nonzero[j])
				info->pcmbundle[ch_in_bundle++] = vb->pcm[j];
		}

		if(ch_in_bundle) {
			unsigned int ressubmap = info->residuesubmap[i];
			_residue_P[ci->residue_type[ressubmap]]->inverse(vb, b->residue[ressubmap], info->pcmbundle, ch_in_bundle);
		}
	}

	i = info->coupling_steps;
	if(i) {
		do {
			ogg_double_t *pcmM, *pcmA;

			i--;
			pcmM = vb->pcm[info->coupling_mag[i]];
			pcmA = vb->pcm[info->coupling_ang[i]];

			j = n >> 1;
			do {
				long maglong = *((long *)pcmM);	// x86 specific code
				if(maglong > 0) {
					if(*((long *)pcmA) > 0) {
						*pcmA = *pcmM - *pcmA;
					} else {
						*pcmM += *pcmA;
						*((long *)pcmA) = maglong;
					}
				} else {
					if(*((long *)pcmA) > 0) {
						*pcmA += *pcmM;
					} else {
						*pcmM -= *pcmA;
						*((long *)pcmA) = maglong;
					}
				}
				pcmM++;
				pcmA++;
			} while(--j);
		} while(i);
	}

	for(i = 0; i < vi->outchannels; i++) {
		ogg_double_t *pcm = vb->pcm[i];
		//if(info->nonzero[i]){
		int flrsubmap = info->floorsubmap[info->chmuxlist[i]];

		_floor_P[ci->floor_type[flrsubmap]]->inverse2(vb, b->flr[flrsubmap], info->floormemo[i], pcm);

#ifdef OGG_SPECTRUM_ANALISER
		if(analiser_bandnum && analiser_banddata)
			ogg_calculate_analiser_bands(pcm, n, (i == (vi->outchannels - 1)));
#endif

		oggdec_mdct_backward(b->transform[vb->W], pcm, pcm);

		//}else
		// vorbis_fclear_block(pcm,n);
	}

	return (0);
}

vorbis_func_mapping mapping0_exportbundle = {
	&mapping0_unpack,
	&mapping0_free_info,
	&mapping0_inverse
};
