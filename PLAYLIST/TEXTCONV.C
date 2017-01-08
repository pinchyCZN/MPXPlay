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
//function:text (tag) conversion : codepage to codepage and UTF-8/16 decoding

#include <malloc.h>
#include "newfunc\newfunc.h"
#include "playlist.h"
#include "charmaps.h"

extern unsigned int id3textconv, textscreen_console_codepage;
extern char cp_winchars[256], cp_doschars[256];

// A lot of elements in the table(s) are same with the US-ASCII
// We begin the table at the first different element (cp_maps[].begin)

static struct cp_map_s {
	char *name;
	unsigned int id_num;		// probably win32 only
	unsigned short *map;
	unsigned short begin;
} cp_maps[] = {
	{
	"ISO-8859-2", 28592, mapping_iso_8859_2, 161},	// the default source cp
	{
	"CP437", 437, mapping_unicode_cp437, 128},	// the default target cp
	{
	"ISO-8859-1", 28591, NULL, 256}, {
	"ISO-8859-3", 28593, mapping_iso_8859_3, 161}, {
	"ISO-8859-4", 28594, mapping_iso_8859_4, 161}, {
	"ISO-8859-5", 28595, mapping_iso_8859_5, 161}, {
	"ISO-8859-6", 28596, mapping_iso_8859_6, 172}, {
	"ISO-8859-7", 28597, mapping_iso_8859_7, 161}, {
	"ISO-8859-8", 28598, mapping_iso_8859_8, 170}, {
	"ISO-8859-9", 28599, mapping_iso_8859_9, 208}, {
	"ISO-8859-10", 28600, mapping_iso_8859_10, 161},	// ???
	{
	"ISO-8859-13", 28603, mapping_iso_8859_13, 161}, {
	"ISO-8859-14", 28604, mapping_iso_8859_14, 161},	// ???
	{
	"ISO-8859-15", 28605, mapping_iso_8859_15, 164}, {
	"ISO-8859-16", 28606, mapping_iso_8859_16, 161},	// ???
	{
	"CP424", 20424, mapping_unicode_cp424, 4}, {
	"CP737", 737, mapping_unicode_cp737, 128}, {
	"CP775", 775, mapping_unicode_cp775, 128}, {
	"CP850", 850, mapping_unicode_cp850, 128}, {
	"CP852", 852, mapping_unicode_cp852, 128}, {
	"CP855", 855, mapping_unicode_cp855, 128}, {
	"CP856", 856, mapping_unicode_cp856, 128},	// ???
	{
	"CP857", 857, mapping_unicode_cp857, 128}, {
	"CP858", 858, mapping_unicode_cp858, 128}, {
	"CP860", 860, mapping_unicode_cp860, 128}, {
	"CP861", 861, mapping_unicode_cp861, 128}, {
	"CP862", 862, mapping_unicode_cp862, 128}, {
	"CP863", 863, mapping_unicode_cp863, 128}, {
	"CP864", 864, mapping_unicode_cp864, 37}, {
	"CP865", 865, mapping_unicode_cp865, 128}, {
	"CP866", 866, mapping_unicode_cp866, 128}, {
	"CP869", 869, mapping_unicode_cp869, 128}, {
	"CP874", 874, mapping_unicode_cp874, 128}, {
	"CP932", 932, mapping_unicode_cp932, 161}, {
	"CP936", 936, NULL, 256}, {
	"CP949", 949, NULL, 256}, {
	"CP950", 950, NULL, 256}, {
	"CP1006", 1006, mapping_unicode_cp1006, 161},	// ???
	{
	"CP1250", 1250, mapping_unicode_cp1250, 128}, {
	"CP1251", 1251, mapping_unicode_cp1251, 128}, {
	"CP1252", 1252, mapping_unicode_cp1252, 128}, {
	"CP1253", 1253, mapping_unicode_cp1253, 128}, {
	"CP1254", 1254, mapping_unicode_cp1254, 128}, {
	"CP1255", 1255, mapping_unicode_cp1255, 128}, {
	"CP1256", 1256, mapping_unicode_cp1256, 128}, {
	"CP1257", 1257, mapping_unicode_cp1257, 128}, {
	"CP1258", 1258, mapping_unicode_cp1258, 128}, {
	NULL, 0, NULL, 0}
};

char *textconv_codepage_sourcename, *textconv_codepage_targetname;
static struct cp_map_s *textconv_cpsource_map, *textconv_cptarget_map;
static unsigned char *textconv_table_unicode_to_char;
static unsigned char *textconv_table_cp_to_cp_IN;
static unsigned char *textconv_table_cp_to_cp_OUT;

static struct cp_map_s *textconv_select_mapping_by_name(char *codepage_name)
{
	struct cp_map_s *targetmap = NULL;
	if(codepage_name && codepage_name[0]) {
		struct cp_map_s *mapp = &cp_maps[0];
		while(mapp->name) {
			if(pds_stricmp(mapp->name, codepage_name) == 0) {
				targetmap = mapp;
				break;
			}
			mapp++;
		}
	}
	return (targetmap);
}

#ifdef MPXPLAY_WIN32

static struct cp_map_s *textconv_select_mapping_by_id(unsigned int id)
{
	struct cp_map_s *targetmap = NULL;
	if(id) {
		struct cp_map_s *mapp = &cp_maps[0];
		while(mapp->id_num) {
			if(mapp->id_num == id) {
				targetmap = mapp;
				break;
			}
			mapp++;
		}
	}
	return (targetmap);
}

static void textconv_cpmaps_init(void)
{
	if(!textconv_cpsource_map) {
		textconv_cpsource_map = textconv_select_mapping_by_name(textconv_codepage_sourcename);
		if(!textconv_cpsource_map) {
			if(funcbit_test(id3textconv, ID3TEXTCONV_GET_WINCP))
				textconv_cpsource_map = textconv_select_mapping_by_id(GetACP());
			if(!textconv_cpsource_map)
				textconv_cpsource_map = &cp_maps[0];
		}
		textconv_cptarget_map = textconv_select_mapping_by_name(textconv_codepage_targetname);
		if(!textconv_cptarget_map) {
			if(funcbit_test(id3textconv, ID3TEXTCONV_GET_WINCP))
				textconv_cptarget_map = textconv_select_mapping_by_id(GetOEMCP());
			if(!textconv_cptarget_map)
				textconv_cptarget_map = &cp_maps[1];
		}
		textscreen_console_codepage = textconv_cptarget_map->id_num;
		if(funcbit_test(id3textconv, ID3TEXTCONV_CODEPAGE))	// at -8
			textconv_cptarget_map = NULL;	// WinChars/DosChars are used
	}
}

#else

static void textconv_cpmaps_init(void)
{
	if(!textconv_cpsource_map) {
		textconv_cpsource_map = textconv_select_mapping_by_name(textconv_codepage_sourcename);
		if(!textconv_cpsource_map)
			textconv_cpsource_map = &cp_maps[0];
		if(!funcbit_test(id3textconv, ID3TEXTCONV_CODEPAGE)) {
			textconv_cptarget_map = textconv_select_mapping_by_name(textconv_codepage_targetname);
			if(!textconv_cptarget_map)
				textconv_cptarget_map = &cp_maps[1];
		}
	}
}

#endif

//---------------------------------------------------------------------
// text decoding side

// init codepage conversion (WinChars to DosChars or -8ucp to -8ccp)
static unsigned int playlist_textconv_init_codepage_IN(void)
{
	unsigned int i, j;
	struct cp_map_s *sourcemap, *targetmap;
	unsigned short *srmap;

	if(textconv_table_cp_to_cp_IN)
		return 1;

	textconv_table_cp_to_cp_IN = malloc(256);
	if(!textconv_table_cp_to_cp_IN)
		return 0;
	for(i = 0; i < 32; i++)
		textconv_table_cp_to_cp_IN[i] = 32;
	for(i = 32; i < 256; i++)
		textconv_table_cp_to_cp_IN[i] = i;

	if(funcbit_test(id3textconv, ID3TEXTCONV_CODEPAGE)) {	// WinChars/DosChars based codepage conversion
		for(i = 0; i < pds_strlen(cp_winchars); i++)
			textconv_table_cp_to_cp_IN[cp_winchars[i]] = cp_doschars[i];
	} else {					// -8cup to -8ccp codepage conversion
		targetmap = textconv_cptarget_map;
		if(!targetmap)
			return 0;
		sourcemap = textconv_cpsource_map;
		if(targetmap == sourcemap)
			return 0;
		srmap = sourcemap->map;
		for(i = 0; i < (256 - sourcemap->begin); i++, srmap++) {
			unsigned short *tm = targetmap->map, srcunicode = *srmap;
			for(j = 0; j < (256 - targetmap->begin); j++, tm++) {
				if(*tm == srcunicode) {
					textconv_table_cp_to_cp_IN[sourcemap->begin + i] = targetmap->begin + j;
					break;
				}
			}
		}
		funcbit_enable(id3textconv, ID3TEXTCONV_CODEPAGE);
	}

	return 1;
}

// init unicode (UTF16,UTF8) to CPNNN conversion
static unsigned int playlist_textconv_init_unicode_IN(void)
{
	unsigned int i, srcmap_begin;
	unsigned short *srcmap_map;

	if(textconv_table_unicode_to_char)
		return 1;
	textconv_table_unicode_to_char = malloc(65536 * sizeof(*textconv_table_unicode_to_char));

	if(!textconv_table_unicode_to_char)
		return 0;

	pds_memset(textconv_table_unicode_to_char, '_', 65536 * sizeof(*textconv_table_unicode_to_char));

	textconv_cpmaps_init();

	srcmap_map = textconv_cpsource_map->map;
	srcmap_begin = textconv_cpsource_map->begin;

	for(i = 0; i < srcmap_begin; i++)
		textconv_table_unicode_to_char[i] = i;

	for(; i < 256; i++)
		if(!funcbit_test(id3textconv, ID3TEXTCONV_VALIDATE) || (pds_strchr(cp_winchars, (int)i)))	// to avoid invalid UTF-8 decodings
			textconv_table_unicode_to_char[srcmap_map[i - srcmap_begin]] = i;

	return 1;
}

void mpxplay_playlist_textconv_init(void)
{
	textconv_cpmaps_init();
	if(!playlist_textconv_init_codepage_IN())
		funcbit_disable(id3textconv, ID3TEXTCONV_CODEPAGE);
	if(funcbit_test(id3textconv, ID3TEXTCONV_UTF_ALL))
		if(!playlist_textconv_init_unicode_IN())
			funcbit_disable(id3textconv, ID3TEXTCONV_UTF_ALL);
}

void mpxplay_playlist_textconv_close(void)
{
	if(textconv_table_cp_to_cp_IN) {
		free(textconv_table_cp_to_cp_IN);
		textconv_table_cp_to_cp_IN = NULL;
	}
	if(textconv_table_unicode_to_char) {
		free(textconv_table_unicode_to_char);
		textconv_table_unicode_to_char = NULL;
	}
	if(textconv_table_cp_to_cp_OUT) {	// close of encoding side
		free(textconv_table_cp_to_cp_OUT);
		textconv_table_cp_to_cp_OUT = NULL;
	}
}

//-----------------------------------------------------------------------

//little endian utf16 to char
static unsigned int playlist_textconv_utf16_LE_to_char(unsigned char *str, unsigned int datalen)
{
	unsigned int index_in = 0, index_out = 0;
	unsigned short unicode;

	if(datalen < 2)
		return datalen;
	if(!playlist_textconv_init_unicode_IN())
		return datalen;

	do {
		unicode = PDS_GETB_LEU16(&str[index_in]);
		if(!unicode)
			break;
		index_in += 2;
		if(unicode != 0xfffe && unicode != 0xfeff) {
			str[index_out] = textconv_table_unicode_to_char[unicode];
			index_out++;
		}
	} while(index_in < datalen);
	str[index_out] = 0;
	return index_out;
}

//big endian utf16 to char
static unsigned int playlist_textconv_utf16_BE_to_char(unsigned char *str, unsigned int datalen)
{
	unsigned int index_in = 0, index_out = 0;
	unsigned short unicode;

	if(datalen < 2)
		return datalen;
	if(!playlist_textconv_init_unicode_IN())
		return datalen;

	do {
		unicode = PDS_GETB_BE16(&str[index_in]);
		if(!unicode)
			break;
		index_in += 2;
		if(unicode != 0xfffe && unicode != 0xfeff && unicode != 0xffff) {
			str[index_out] = textconv_table_unicode_to_char[unicode];
			index_out++;
		}
	} while(index_in < datalen);
	str[index_out] = 0;
	return index_out;
}

//utf8 to char
static unsigned int playlist_textconv_utf8_to_char(unsigned char *str, unsigned int datalen)
{
	unsigned int index_in = 0, index_out = 0;

	if(!datalen)
		return datalen;
	if(!playlist_textconv_init_unicode_IN())
		return datalen;
	if((str[0] == 0xef) && (str[1] == 0xbb) && (str[2] == 0xbf))
		index_in += 3;

	do {
		unsigned short unicode;
		unsigned int codesize;
		unsigned char c;

		c = str[index_in];
		if(!c)
			break;

		codesize = 0;

		if(c & 0x80) {
			if((c & 0xe0) == 0xe0) {
				unicode = (c & 0x0F) << 12;
				c = str[index_in + 1];
				if(c) {
					unicode |= (c & 0x3F) << 6;
					c = str[index_in + 2];
					if(c) {
						unicode |= (c & 0x3F);
						codesize = 3;
					}
				}
			} else {
				unicode = (c & 0x3F) << 6;
				c = str[index_in + 1];
				if(c) {
					unicode |= (c & 0x3F);
					codesize = 2;
				}
			}
		}

		if(codesize && (textconv_table_unicode_to_char[unicode] >= textconv_cpsource_map->begin)) {	// we try to find out is this an UTF-8 or not
			str[index_out] = textconv_table_unicode_to_char[unicode];
			index_in += codesize;
		} else {
			c = str[index_in];
			str[index_out] = c;
			index_in++;
		}
		index_out++;

	} while(index_in < datalen);

	str[index_out] = 0;
	return index_out;
}

static unsigned int playlist_textconv_codepage_to_codepage_in(unsigned char *str, unsigned int datalen)
{
	unsigned int len;

	if(!playlist_textconv_init_codepage_IN())
		return datalen;

	len = 0;
	do {
		str[0] = textconv_table_cp_to_cp_IN[str[0]];
		str++;
		len++;
	} while(*str && (len < datalen));
	return len;
}

unsigned int mpxplay_playlist_textconv_do(char *str, unsigned int datalen, unsigned int doneconv)
{
	if(funcbit_test(id3textconv, (ID3TEXTCONV_CODEPAGE | ID3TEXTCONV_UTF8 | ID3TEXTCONV_UTF16))) {
		if((str == NULL) || (str[0] == 0) || !datalen)
			return 0;
		if(funcbit_test(id3textconv, ID3TEXTCONV_UTF16) && !funcbit_test(doneconv, ID3TEXTCONV_UTF16))
			datalen = playlist_textconv_utf16_LE_to_char(str, datalen);
		else if(funcbit_test(id3textconv, ID3TEXTCONV_UTF8) && !funcbit_test(doneconv, ID3TEXTCONV_UTF8))
			datalen = playlist_textconv_utf8_to_char(str, datalen);
		if(funcbit_test(id3textconv, ID3TEXTCONV_CODEPAGE) && !funcbit_test(doneconv, ID3TEXTCONV_CODEPAGE))
			datalen = playlist_textconv_codepage_to_codepage_in(str, datalen);
	} else {
		if(!datalen)
			datalen = pds_strlen(str);
	}
	return datalen;
}

// makes the conversion with a new config
unsigned int mpxplay_playlist_textconv_selected_do(char *str, unsigned int datalen, unsigned int select, unsigned int doneconv)
{
	unsigned int id3tc_save = id3textconv, retcode;
	if(select) {
		funcbit_disable(id3textconv, ID3TEXTCONV_CVTYPE_ALL);
		funcbit_enable(id3textconv, select);
	}
	retcode = mpxplay_playlist_textconv_do(str, datalen, doneconv);
	id3textconv = id3tc_save;
	return retcode;
}

//----------------------------------------------------------------------
// text encoding side

static unsigned int playlist_textconv_char_to_utf16_LE(unsigned char *p_dest, unsigned char *src, unsigned int dest_buflen)
{
	unsigned int len_out = 0;
	unsigned short *dest = (unsigned short *)p_dest;
	struct cp_map_s *sourcemap;

	if(!dest || !src || (dest_buflen < 6))
		return len_out;

	textconv_cpmaps_init();
	sourcemap = textconv_cpsource_map;

	dest_buflen -= 2;

	do {
		unsigned int c = (unsigned int)src[0], wc;
		if(!c)
			break;
		if(c >= sourcemap->begin)
			wc = sourcemap->map[c - sourcemap->begin];
		else
			wc = c;
		PDS_PUTB_LE16(dest, wc);
		src++;
		dest++;
		len_out += 2;
	} while(len_out < dest_buflen);

	dest[0] = 0;

	return len_out;
}

static unsigned int playlist_textconv_char_to_utf16_BE(unsigned char *p_dest, unsigned char *src, unsigned int dest_buflen)
{
	unsigned int len_out = 0;
	unsigned short *dest = (unsigned short *)p_dest;
	struct cp_map_s *sourcemap;

	if(!dest || !src || (dest_buflen < 6))
		return len_out;

	textconv_cpmaps_init();
	sourcemap = textconv_cpsource_map;

	dest_buflen -= 2;

	do {
		unsigned int c = (unsigned int)src[0], wc;
		if(!c)
			break;
		if(c >= sourcemap->begin)
			wc = sourcemap->map[c - sourcemap->begin];
		else
			wc = c;
		PDS_PUTB_BE16(dest, wc);
		src++;
		dest++;
		len_out += 2;
	} while(len_out < dest_buflen);

	dest[0] = 0;

	return len_out;
}

static unsigned int playlist_textconv_char_to_utf8(unsigned char *dest, unsigned char *src, unsigned int dest_buflen)
{
	unsigned int len_out = 0;
	struct cp_map_s *sourcemap;

	if(!dest || !src || (dest_buflen < 5))
		return len_out;

	dest_buflen -= 4;

	textconv_cpmaps_init();
	sourcemap = textconv_cpsource_map;

	do {
		unsigned int c = *src++, wc;

		if(!c)
			break;

		wc = (c < sourcemap->begin) ? c : sourcemap->map[c - sourcemap->begin];

		if(wc < (1 << 7)) {
			dest[0] = wc;
			dest += 1;
			len_out += 1;
		} else if(wc < (1 << 11)) {
			dest[0] = 0xc0 | (wc >> 6);
			dest[1] = 0x80 | (wc & 0x3f);
			dest += 2;
			len_out += 2;
		} else if(wc < (1 << 16)) {
			dest[0] = 0xe0 | (wc >> 12);
			dest[1] = 0x80 | ((wc >> 6) & 0x3f);
			dest[2] = 0x80 | (wc & 0x3f);
			dest += 3;
			len_out += 3;
		}
	} while(len_out < dest_buflen);

	dest[0] = 0;

	return len_out;
}

static unsigned int playlist_textconv_init_codepage_OUT(void)
{
	unsigned int i, j;
	struct cp_map_s *sourcemap, *targetmap;
	unsigned short *tgmap;

	if(textconv_table_cp_to_cp_OUT)
		return 1;
	textconv_table_cp_to_cp_OUT = malloc(256);
	if(!textconv_table_cp_to_cp_OUT)
		return 0;
	for(i = 0; i < 32; i++)
		textconv_table_cp_to_cp_OUT[i] = 32;
	for(i = 32; i < 256; i++)
		textconv_table_cp_to_cp_OUT[i] = i;

	if(textconv_cptarget_map) {	// !!! ??? not tested
		sourcemap = textconv_cpsource_map;
		targetmap = textconv_cptarget_map;
		tgmap = targetmap->map;
		for(i = 0; i < (256 - targetmap->begin); i++, tgmap++) {
			unsigned short *sm = sourcemap->map, tgunicode = *tgmap;
			for(j = 0; j < (256 - sourcemap->begin); j++, sm++) {
				if(*sm == tgunicode) {
					textconv_table_cp_to_cp_OUT[targetmap->begin + i] = sourcemap->begin + j;
					break;
				}
			}
		}
	} else {
		for(i = 0; i < pds_strlen(cp_doschars); i++) {
			unsigned char d = cp_doschars[i];
			if(!(d >= 'a' && d <= 'z') && !(d >= 'A' && d <= 'Z') && !(d >= '0' && d <= '9'))	// ??? (else the converting back may be wrong)
				textconv_table_cp_to_cp_OUT[cp_doschars[i]] = cp_winchars[i];
		}
	}
	return 1;
}

static unsigned int playlist_textconv_codepage_to_codepage_out(unsigned char *dest, unsigned char *src)
{
	unsigned int i;

	if(!src || !dest)
		return 0;
	if(!playlist_textconv_init_codepage_OUT())
		return 0;

	i = MAX_ID3LEN - 1;
	while(src[0]) {
		dest[0] = textconv_table_cp_to_cp_OUT[src[0]];
		src++;
		dest++;
		if(!(--i))
			break;
	}
	dest[0] = 0;

	return (MAX_ID3LEN - i);
}

char *mpxplay_playlist_textconv_back(unsigned char *dest, unsigned char *src)
{
	if(!dest)
		return src;
	if(!src)
		dest[0] = 0;
	else {
		if(funcbit_test(id3textconv, (ID3TEXTCONV_CODEPAGE | ID3TEXTCONV_CP_BACK)))
			playlist_textconv_codepage_to_codepage_out(dest, src);
		else
			return src;
	}
	return dest;
}

unsigned int mpxplay_playlist_textconv_selected_back(unsigned char *dest, unsigned int dest_buflen, char *src, unsigned int select)
{
	char *currd, *strtmp;
	unsigned int len;

	if(!dest || !select)
		return 0;
	if(!src) {
		dest[0] = 0;
		return 0;
	}
	strtmp = alloca(dest_buflen + 8);
	if(!strtmp)
		return 0;

	if(funcbit_test(select, ID3TEXTCONV_CODEPAGE) && funcbit_test(select, ID3TEXTCONV_UTF_ALL))
		currd = strtmp;
	else
		currd = dest;

	if(funcbit_test(select, ID3TEXTCONV_CODEPAGE)) {
		playlist_textconv_codepage_to_codepage_out(currd, src);
		src = currd;
		currd = dest;
	}
	if(funcbit_test(select, ID3TEXTCONV_UTF8)) {
		len = pds_strncpy(currd, src, dest_buflen);
		currd[dest_buflen - 1] = 0;
		if(len <= playlist_textconv_utf8_to_char(currd, len))	// ??? is text already UTF8 encoded?
			len = playlist_textconv_char_to_utf8(currd, src, dest_buflen);	// if no, then convert to UTF8
		else {
			len = pds_strncpy(currd, src, dest_buflen);	// utf8_to_char overwrites it
			currd[dest_buflen - 1] = 0;
		}
	} else if(funcbit_test(select, ID3TEXTCONV_UTF16))
		len = playlist_textconv_char_to_utf16_LE(currd, src, dest_buflen);

	return len;
}

//------------------------------------------------------------------------
// API

mpxplay_textconv_func_s mpxplay_playlist_textconv_funcs = {
	&id3textconv,
	0,

	&playlist_textconv_utf16_LE_to_char,
	&playlist_textconv_utf16_BE_to_char,
	&playlist_textconv_utf8_to_char,
	&playlist_textconv_codepage_to_codepage_in,
	&mpxplay_playlist_textconv_do,

	&playlist_textconv_char_to_utf16_LE,
	&playlist_textconv_char_to_utf16_BE,
	&playlist_textconv_char_to_utf8,
	&playlist_textconv_codepage_to_codepage_out,
	&mpxplay_playlist_textconv_back
};
