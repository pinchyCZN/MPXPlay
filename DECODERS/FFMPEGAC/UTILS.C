/*
 * utils for libavcodec
 * Copyright (c) 2001 Fabrice Bellard.
 * Copyright (c) 2003 Michel Bardiaux for the av_log API
 * Copyright (c) 2002-2004 Michael Niedermayer <michaelni@gmx.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file utils.c
 * utils.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "avcodec.h"
//#include "dsputil.h"
//#include "mpegvideo.h"
//#include "integer.h"
//#include <stdarg.h>
//#include <limits.h>

const uint8_t ff_log2_tab[256] = {
	0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

void *av_mallocz(unsigned int size)
{
	void *ptr;

	ptr = av_malloc(size);
	if(!ptr)
		return NULL;
	memset(ptr, 0, size);
	return ptr;
}

/**
 * realloc which does nothing if the block is large enough
 */
void *av_fast_realloc(void *ptr, unsigned int *size, unsigned int min_size)
{
	if(min_size < *size)
		return ptr;

	*size = FFMAX(17 * min_size / 16 + 32, min_size);

	return av_realloc(ptr, *size);
}


static unsigned int last_static = 0;
static unsigned int allocated_static = 0;
static void **array_static = NULL;

/**
 * allocation of static arrays - do not use for normal allocation.
 */
void *av_mallocz_static(unsigned int size)
{
	void *ptr = av_mallocz(size);

	if(ptr) {
		array_static = av_fast_realloc(array_static, &allocated_static, sizeof(void *) * (last_static + 1));
		if(!array_static)
			return NULL;
		array_static[last_static++] = ptr;
	}

	return ptr;
}

/**
 * same as above, but does realloc
 */

void *av_realloc_static(void *ptr, unsigned int size)
{
	int i;
	if(!ptr)
		return av_mallocz_static(size);
	/* Look for the old ptr */
	for(i = 0; i < last_static; i++) {
		if(array_static[i] == ptr) {
			array_static[i] = av_realloc(array_static[i], size);
			return array_static[i];
		}
	}
	return NULL;

}

/**
 * free all static arrays and reset pointers to 0.
 */
void av_free_static(void)
{
	while(last_static) {
		av_freep(&array_static[--last_static]);
	}
	av_freep(&array_static);
}

/**
 * Frees memory and sets the pointer to NULL.
 * @param arg pointer to the pointer which should be freed
 */
void av_freep(void *arg)
{
	void **ptr = (void **)arg;
	av_free(*ptr);
	*ptr = NULL;
}

/* av_log API */

static int av_log_level = AV_LOG_QUIET;

static void av_log_default_callback(void *ptr, int level, const char *fmt, va_list vl)
{
	static int print_prefix = 1;
	AVClass *avc = ptr ? *(AVClass **) ptr : NULL;
	if(level > av_log_level)
		return;
#undef fprintf
	if(print_prefix && avc) {
		fprintf(stderr, "[%s @ %p]", avc->item_name(ptr), avc);
	}
#define fprintf please_use_av_log

	print_prefix = strstr(fmt, "\n") != NULL;

	vfprintf(stderr, fmt, vl);
}

static void (*av_log_callback) (void *, int, const char *, va_list) = av_log_default_callback;

void av_log(void *avcl, int level, const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	av_vlog(avcl, level, fmt, vl);
	va_end(vl);
}

void av_vlog(void *avcl, int level, const char *fmt, va_list vl)
{
	av_log_callback(avcl, level, fmt, vl);
}

void av_log_set_level(int level)
{
	av_log_level = level;
}
