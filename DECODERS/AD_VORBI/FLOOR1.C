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

 function: floor backend 1 implementation
 last mod: $Id: floor1.c,v 1.25 2003/01/15 00:00:00 PDSoft Exp $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "codec.h"
#include "codecint.h"
#include "registry.h"
#include "codebook.h"

typedef struct {
	int n;
	int quant_q;
	int ilqq;					// ilog(look->quant_q-1)
	int posts;
	int **fit_value;

	vorbis_info_floor1 *vif1;

	int *hineighbor;
	int *loneighbor;

	int *forward_index;

} vorbis_look_floor1;

static void floor1_free_info(vorbis_info_floor * i)
{
	vorbis_info_floor1 *info = (vorbis_info_floor1 *) i;
	if(info) {
		if(info->partitionclass)
			_ogg_free(info->partitionclass);
		if(info->classes)
			_ogg_free(info->classes);
		if(info->postlist)
			_ogg_free(info->postlist);
		_ogg_free(info);
	}
}

static void floor1_free_look(vorbis_look_floor * vlf)
{
	vorbis_look_floor1 *look = (vorbis_look_floor1 *) vlf;
	unsigned int i;

	if(look && look->vif1 && look->vif1->vi) {
		vorbis_info_floor1 *info = look->vif1;
		vorbis_info *vi = info->vi;
		if(look->fit_value) {
			for(i = 0; i < vi->channels; i++)
				_ogg_free(look->fit_value[i]);
			_ogg_free(look->fit_value);
		}
		if(look->hineighbor)
			_ogg_free(look->hineighbor);
		if(look->loneighbor)
			_ogg_free(look->loneighbor);
		if(look->forward_index)
			_ogg_free(look->forward_index);
		_ogg_free(look);
	}
}

static vorbis_info_floor *floor1_unpack(vorbis_info * vi, oggpack_buffer * opb)
{
	codec_setup_info *ci = vi->codec_setup;
	int j, k, count = 0, maxclass = -1, rangebits;

	vorbis_info_floor1 *info = _ogg_calloc(1, sizeof(*info));
	info->partitions = oggpack_read24(opb, 5);
	info->partitionclass = _ogg_malloc(info->partitions * sizeof(*info->partitionclass));
	for(j = 0; j < info->partitions; j++) {
		info->partitionclass[j] = oggpack_read24(opb, 4);
		if(maxclass < info->partitionclass[j])
			maxclass = info->partitionclass[j];
	}

	maxclass++;

	info->classes = _ogg_calloc((maxclass) ? maxclass : 1, sizeof(vif_class_s));
	if(!info->classes)
		goto err_out;

	for(j = 0; j < maxclass; j++) {
		info->classes[j].dim = oggpack_read24(opb, 3) + 1;
		info->classes[j].subs = oggpack_read24(opb, 2);
		if(info->classes[j].subs < 0)
			goto err_out;
		if(info->classes[j].subs)
			info->classes[j].book = oggpack_read24(opb, 8);
		if(info->classes[j].book < 0 || info->classes[j].book >= ci->books)
			goto err_out;
		for(k = 0; k < (1 << info->classes[j].subs); k++) {
			info->classes[j].subbook[k] = oggpack_read24(opb, 8) - 1;
			if(info->classes[j].subbook[k] < -1 || info->classes[j].subbook[k] >= ci->books)
				goto err_out;
		}
	}

	info->postlist = _ogg_malloc((VIF_POSIT + 2) * sizeof(*info->postlist));
	if(!info->postlist)
		goto err_out;

	info->mult = oggpack_read24(opb, 2) + 1;
	rangebits = oggpack_read24(opb, 4);

	info->postlist[0] = 0;
	info->postlist[1] = 1 << rangebits;
	for(j = 0, k = 0; j < info->partitions; j++) {
		count += info->classes[info->partitionclass[j]].dim;
		for(; k < count; k++) {
			int t = info->postlist[k + 2] = oggpack_read24(opb, rangebits);
			if(t < 0 || t >= (1 << rangebits))
				goto err_out;
		}
	}

	return (info);

  err_out:
	floor1_free_info(info);
	return (NULL);
}

static int icomp(const void *a, const void *b)
{
	int a0 = **(int **)a;
	int b0 = **(int **)b;

	if(a0 > b0)
		return 1;
	if(a0 == b0)
		return 0;

	return -1;
}

static vorbis_look_floor *floor1_look(vorbis_dsp_state * vd, vorbis_info_floor * in)
{
	vorbis_info_floor1 *info = (vorbis_info_floor1 *) in;
	vorbis_look_floor1 *look = _ogg_calloc(1, sizeof(*look));
	vorbis_info *vi = vd->vi;
	int i, j, n, *sortpointer[VIF_POSIT + 2];;

	info->vi = vi;

	look->vif1 = info;
	look->n = info->postlist[1];

	n = 2;
	for(i = 0; i < info->partitions; i++)
		n += info->classes[info->partitionclass[i]].dim;

	look->posts = n;

	look->forward_index = _ogg_malloc(n * sizeof(*look->forward_index));
	look->hineighbor = _ogg_malloc(n * sizeof(look->hineighbor));
	look->loneighbor = _ogg_malloc(n * sizeof(look->loneighbor));

	for(i = 0; i < n; i++)
		sortpointer[i] = info->postlist + i;

	qsort(sortpointer, n, sizeof(*sortpointer), icomp);

	for(i = 0; i < n; i++)
		look->forward_index[i] = sortpointer[i] - info->postlist;

	for(i = 2; i < n; i++) {
		int lo = 0;
		int hi = 1;
		int lx = 0;
		int hx = look->n;
		int currentx = info->postlist[i];
		for(j = 0; j < i; j++) {
			int x = info->postlist[j];
			if(x > lx && x < currentx) {
				lo = j;
				lx = x;
			}
			if(x < hx && x > currentx) {
				hi = j;
				hx = x;
			}
		}
		look->loneighbor[i] = lo;
		look->hineighbor[i] = hi;
	}

	switch (info->mult) {
	case 1:
		look->quant_q = 256;
		look->ilqq = 8;
		break;
	case 2:
		look->quant_q = 128;
		look->ilqq = 7;
		break;
	case 3:
		look->quant_q = 86;
		look->ilqq = 7;
		break;
	case 4:
		look->quant_q = 64;
		look->ilqq = 6;
		break;
	}

	look->fit_value = _ogg_malloc(sizeof(*(look->fit_value)) * vi->channels);
	for(j = 0; j < vi->channels; j++)
		look->fit_value[j] = _ogg_malloc(sizeof(*(look->fit_value[j])) * (look->posts));

	return (look);
}

//new !!!
/*static int render_point(int x0,int x1,int y0,int y1,int x)
{
 y0&=0x7fff;
 y1&=0x7fff;

 y1-=y0;
 x1-=x0;
 x -=x0;

 return (x*y1/x1+y0);
}*/

static ogg_float_t FLOOR1_fromdB_LOOKUP[256] = {
	1.0649863e-07F, 1.1341951e-07F, 1.2079015e-07F, 1.2863978e-07F,
	1.3699951e-07F, 1.4590251e-07F, 1.5538408e-07F, 1.6548181e-07F,
	1.7623575e-07F, 1.8768855e-07F, 1.9988561e-07F, 2.128753e-07F,
	2.2670913e-07F, 2.4144197e-07F, 2.5713223e-07F, 2.7384213e-07F,
	2.9163793e-07F, 3.1059021e-07F, 3.3077411e-07F, 3.5226968e-07F,
	3.7516214e-07F, 3.9954229e-07F, 4.2550680e-07F, 4.5315863e-07F,
	4.8260743e-07F, 5.1396998e-07F, 5.4737065e-07F, 5.8294187e-07F,
	6.2082472e-07F, 6.6116941e-07F, 7.0413592e-07F, 7.4989464e-07F,
	7.9862701e-07F, 8.5052630e-07F, 9.0579828e-07F, 9.6466216e-07F,
	1.0273513e-06F, 1.0941144e-06F, 1.1652161e-06F, 1.2409384e-06F,
	1.3215816e-06F, 1.4074654e-06F, 1.4989305e-06F, 1.5963394e-06F,
	1.7000785e-06F, 1.8105592e-06F, 1.9282195e-06F, 2.0535261e-06F,
	2.1869758e-06F, 2.3290978e-06F, 2.4804557e-06F, 2.6416497e-06F,
	2.8133190e-06F, 2.9961443e-06F, 3.1908506e-06F, 3.3982101e-06F,
	3.6190449e-06F, 3.8542308e-06F, 4.1047004e-06F, 4.3714470e-06F,
	4.6555282e-06F, 4.9580707e-06F, 5.2802740e-06F, 5.6234160e-06F,
	5.9888572e-06F, 6.3780469e-06F, 6.7925283e-06F, 7.2339451e-06F,
	7.7040476e-06F, 8.2047000e-06F, 8.7378876e-06F, 9.3057248e-06F,
	9.9104632e-06F, 1.0554501e-05F, 1.1240392e-05F, 1.1970856e-05F,
	1.2748789e-05F, 1.3577278e-05F, 1.4459606e-05F, 1.5399272e-05F,
	1.6400004e-05F, 1.7465768e-05F, 1.8600792e-05F, 1.9809576e-05F,
	2.1096914e-05F, 2.2467911e-05F, 2.3928002e-05F, 2.5482978e-05F,
	2.7139006e-05F, 2.8902651e-05F, 3.0780908e-05F, 3.2781225e-05F,
	3.4911534e-05F, 3.7180282e-05F, 3.9596466e-05F, 4.2169667e-05F,
	4.4910090e-05F, 4.7828601e-05F, 5.0936773e-05F, 5.4246931e-05F,
	5.7772202e-05F, 6.1526565e-05F, 6.5524908e-05F, 6.9783085e-05F,
	7.4317983e-05F, 7.9147585e-05F, 8.4291040e-05F, 8.9768747e-05F,
	9.5602426e-05F, 0.00010181521F, 0.00010843174F, 0.00011547824F,
	0.00012298267F, 0.00013097477F, 0.00013948625F, 0.00014855085F,
	0.00015820453F, 0.00016848555F, 0.00017943469F, 0.00019109536F,
	0.00020351382F, 0.00021673929F, 0.00023082423F, 0.00024582449F,
	0.00026179955F, 0.00027881276F, 0.00029693158F, 0.00031622787F,
	0.00033677814F, 0.00035866388F, 0.00038197188F, 0.00040679456F,
	0.00043323036F, 0.00046138411F, 0.00049136745F, 0.00052329927F,
	0.00055730621F, 0.00059352311F, 0.00063209358F, 0.00067317058F,
	0.00071691700F, 0.00076350630F, 0.00081312324F, 0.00086596457F,
	0.00092223983F, 0.00098217216F, 0.0010459992F, 0.0011139742F,
	0.0011863665F, 0.0012634633F, 0.0013455702F, 0.0014330129F,
	0.0015261382F, 0.0016253153F, 0.0017309374F, 0.0018434235F,
	0.0019632195F, 0.0020908006F, 0.0022266726F, 0.0023713743F,
	0.0025254795F, 0.0026895994F, 0.0028643847F, 0.0030505286F,
	0.0032487691F, 0.0034598925F, 0.0036847358F, 0.0039241906F,
	0.0041792066F, 0.0044507950F, 0.0047400328F, 0.0050480668F,
	0.0053761186F, 0.0057254891F, 0.0060975636F, 0.0064938176F,
	0.0069158225F, 0.0073652516F, 0.0078438871F, 0.0083536271F,
	0.0088964928F, 0.009474637F, 0.010090352F, 0.010746080F,
	0.011444421F, 0.012188144F, 0.012980198F, 0.013823725F,
	0.014722068F, 0.015678791F, 0.016697687F, 0.017782797F,
	0.018938423F, 0.020169149F, 0.021479854F, 0.022875735F,
	0.024362330F, 0.025945531F, 0.027631618F, 0.029427276F,
	0.031339626F, 0.033376252F, 0.035545228F, 0.037855157F,
	0.040315199F, 0.042935108F, 0.045725273F, 0.048696758F,
	0.051861348F, 0.055231591F, 0.058820850F, 0.062643361F,
	0.066714279F, 0.071049749F, 0.075666962F, 0.080584227F,
	0.085821044F, 0.091398179F, 0.097337747F, 0.10366330F,
	0.11039993F, 0.11757434F, 0.12521498F, 0.13335215F,
	0.14201813F, 0.15124727F, 0.16107617F, 0.17154380F,
	0.18269168F, 0.19456402F, 0.20720788F, 0.22067342F,
	0.23501402F, 0.25028656F, 0.26655159F, 0.28387361F,
	0.30232132F, 0.32196786F, 0.34289114F, 0.36517414F,
	0.38890521F, 0.41417847F, 0.44109412F, 0.46975890F,
	0.50028648F, 0.53279791F, 0.56742212F, 0.60429640F,
	0.64356699F, 0.68538959F, 0.72993007F, 0.77736504F,
	0.82788260F, 0.88168307F, 0.9389798F, 1.F,
};

#ifdef OGG_USE_ASM

void asm_render_line(int, int, float *, float *);

static void render_line(int adx, int ady, ogg_double_t * d, ogg_float_t * fflp)
{
	int base, sy;
	//eax=err
	//ebx=d
	//ecx=fflp
	//edx=ady
	//edi=x
	//esi=adx
#pragma aux asm_render_line=\
 "mov edi,eax"\
 "mov eax,edx"\
 "sar edx,0x1f"\
 "mov esi,edi"\
 "idiv esi"\
 "shl eax,2"\
 "mov dword ptr base,eax"\
 "test edx,edx"\
 "jl adyn"\
  "add eax,4"\
  "jmp adye"\
 "adyn:"\
  "sub eax,4"\
  "neg edx"\
 "adye:"\
 "mov dword ptr sy,eax"\
 "xor eax,eax"\
 "backx:"\
  "fld dword ptr [ebx]"\
  "add eax,edx"\
  "fmul dword ptr [ecx]"\
  "cmp eax,esi"\
  "jl noerr"\
   "sub eax,esi"\
   "add ecx,dword ptr sy"\
   "jmp noerrend"\
  "noerr:"\
   "add ecx,dword ptr base"\
  "noerrend:"\
  "fstp dword ptr [ebx]"\
  "add ebx,4"\
  "dec edi"\
 "jnz backx"\
 parm[eax][edx][ebx][ecx] modify[eax ebx ecx edx edi esi];
	asm_render_line(adx, ady, d, fflp);
}

#else

//new !!!
static void render_line(int adx, int ady, ogg_double_t * d, ogg_float_t * fflp)
{
	int x, err, base, sy;

	base = ady / adx;
	ady -= base * adx;			// ady=ady%adx;
	if(ady < 0) {
		ady = -ady;
		sy = base - 1;
	} else
		sy = base + 1;
	x = adx;
	err = 0;

	do {
		*d++ *= *fflp;
		err += ady;
		if(err >= adx) {
			err -= adx;
			fflp += sy;
		} else {
			fflp += base;
		}
	} while(--x);
}

#endif

static void *floor1_inverse1(vorbis_block * vb, vorbis_look_floor * in, unsigned int channel)
{
	vorbis_look_floor1 *look = (vorbis_look_floor1 *) in;
	vorbis_info_floor1 *info = look->vif1;
	codec_setup_info *ci = info->vi->codec_setup;
	codebook *books = ci->fullbooks;
	oggpack_buffer *vbopb = &vb->opb;
	int i, k;

	if(oggpack_read1(vbopb) == 1) {
		int *fit_value = look->fit_value[channel], *fitp = fit_value;
		unsigned int ilqq = look->ilqq;

		fitp[0] = oggpack_read24(vbopb, ilqq);
		fitp[1] = oggpack_read24(vbopb, ilqq);

		fitp += 2;

		for(i = 0; i < info->partitions; i++) {
			vif_class_s *vcs = &info->classes[info->partitionclass[i]];
			unsigned int cdim = vcs->dim;
			unsigned int csubbits = vcs->subs;
			unsigned int cmask = (1 << csubbits) - 1;
			int cval = 0;

			if(csubbits) {
				cval = vorbis_book_decode((books + vcs->book)->decode_tree, vbopb);
				if(cval < 0)
					goto eop;
			}

			for(k = 0; k < cdim; k++) {
				int book = vcs->subbook[cval & cmask];
				if(book >= 0) {
					if((fitp[0] = vorbis_book_decode((books + book)->decode_tree, vbopb)) < 0)
						goto eop;
				} else {
					fitp[0] = 0;
				}
				fitp++;
				cval >>= csubbits;
			}
		}

		for(i = 2; i < look->posts; i++) {
			int loroom, val;
			int y0, y1;
			unsigned int lli = look->loneighbor[i];
			unsigned int lhi = look->hineighbor[i];

			{					// render_point
				int x0 = info->postlist[lli];
				int dx01 = info->postlist[lhi] - x0;
				int x = info->postlist[i] - x0;
				y0 = fit_value[lli] & 0x7fff;
				y1 = fit_value[lhi] & 0x7fff;

				loroom = y0 + x * (y1 - y0) / dx01;
			}

			val = fit_value[i];
			if(val) {
				int hiroom = look->quant_q - loroom;
				int room = ((hiroom < loroom) ? hiroom : loroom) << 1;

				if(val >= room) {
					if(hiroom > loroom)
						val = val - loroom;
					else
						val = -1 - (val - hiroom);
				} else {
					if(val & 1)
						val = -((val + 1) >> 1);
					else
						val >>= 1;
				}

				fit_value[i] = val + loroom;
				fit_value[lli] = y0;
				fit_value[lhi] = y1;
			} else {
				fit_value[i] = loroom | 0x8000;
			}
		}

		return (fit_value);
	}

  eop:
	return (NULL);
}

static int floor1_inverse2(vorbis_block * vb, vorbis_look_floor * in, void *memo, ogg_double_t * out)
{
	vorbis_look_floor1 *look = (vorbis_look_floor1 *) in;
	vorbis_info_floor1 *info = look->vif1;
	codec_setup_info *ci = info->vi->codec_setup;
	int n = ci->blocksizes[vb->W] / 2;
	int j;

	if(memo) {
		int *fit_value = (int *)memo;
		int ly = fit_value[0] * info->mult;
		int lx = 0;

		for(j = 1; j < look->posts; j++) {
			int current = look->forward_index[j];
			int hy = fit_value[current];
			if(!(hy & (~0x7fff))) {
				int hx = info->postlist[current];
				hy *= info->mult;

				if(hx > lx)
					render_line(hx - lx, hy - ly, out + lx, &FLOOR1_fromdB_LOOKUP[ly]);
				else
					out[lx] *= FLOOR1_fromdB_LOOKUP[ly];

				lx = hx;
				ly = hy;
			}
		}

		for(j = lx; j < n; j++)
			out[j] *= FLOOR1_fromdB_LOOKUP[ly];

		return (1);
	}
	memset(out, 0, sizeof(*out) * n);
	return (0);
}

vorbis_func_floor floor1_exportbundle = {
	&floor1_unpack,
	&floor1_look,
	&floor1_free_info,
	&floor1_free_look,
	&floor1_inverse1,
	&floor1_inverse2
};
