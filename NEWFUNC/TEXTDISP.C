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
//function: text-display handing


#include "newfunc.h"
#include "display\display.h"
#ifdef MPXPLAY_WIN32
#include <malloc.h>
#include <wincon.h>
static HANDLE hConsoleOutput;
static WORD *consolesave_attrib;
static char *consolesave_text;
static unsigned int textscreen_size, consolesave_size;
#endif

#define TEXTSCREEN_BASE_ADDR 0xb8000

unsigned long textscreen_linear_address = TEXTSCREEN_BASE_ADDR;
unsigned int textscreen_mode, textscreen_maxx, textscreen_maxy, oldposrow;
unsigned int textscreen_console_codepage = 437;

#define TEXTSCREEN_LIN_ADDR textscreen_linear_address

static unsigned int textscreen_oldmaxy, textscreen_oldcursor_y;
static unsigned int textscreen_memory_size;
#ifdef __DOS__
static unsigned int *vidmemsave_field, vidmemsave_size;
#endif

#ifdef MPXPLAY_WIN32

void newfunc_textdisplay_init(void)
{
	COORD size;
	hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	textscreen_mode = pds_textdisplay_getmode();
	pds_textdisplay_getresolution();

	size.X = textscreen_maxx;
	size.Y = textscreen_maxy;
	SetConsoleScreenBufferSize(hConsoleOutput, size);

	if(textscreen_console_codepage)
		SetConsoleOutputCP(textscreen_console_codepage);

	textscreen_size = textscreen_maxx * textscreen_maxy;
	textscreen_memory_size = textscreen_size * TEXTSCREEN_BYTES_PER_CHAR;
	if(consolesave_size < textscreen_size) {
		consolesave_size = textscreen_size * 2;
		if(consolesave_attrib)
			pds_free(consolesave_attrib);
		consolesave_attrib = (WORD *) pds_malloc(consolesave_size * sizeof(WORD));
		if(consolesave_text)
			pds_free(consolesave_text);
		consolesave_text = (char *)pds_malloc(consolesave_size);
		if(!consolesave_attrib || !consolesave_text)
			consolesave_size = 0;
	}
}

#else

void newfunc_textdisplay_init(void)
{
	textscreen_mode = pds_textdisplay_getmode();
	pds_textdisplay_getresolution();
	textscreen_memory_size = textscreen_maxx * textscreen_maxy * TEXTSCREEN_BYTES_PER_CHAR;
	if(vidmemsave_size < textscreen_memory_size) {
		vidmemsave_size = textscreen_memory_size * 2;
		if(vidmemsave_field)
			pds_free(vidmemsave_field);
		vidmemsave_field = pds_malloc(vidmemsave_size);
		if(!vidmemsave_field)
			vidmemsave_size = 0;
	}
	//pds_cpu_mtrr_enable_wc(0xA0000,128);
}
#endif

void newfunc_textdisplay_close(void)
{
#ifdef MPXPLAY_WIN32
	if(consolesave_attrib) {
		pds_free(consolesave_attrib);
		consolesave_attrib = NULL;
	}
	if(consolesave_text) {
		pds_free(consolesave_text);
		consolesave_text = NULL;
	}
	consolesave_size = 0;
#else
	//pds_cpu_mtrr_disable_wc(0xA0000);
	if(vidmemsave_field) {
		pds_free(vidmemsave_field);
		vidmemsave_field = NULL;
	}
	vidmemsave_size = 0;
#endif
}

void pds_textdisplay_charxy(unsigned int color, unsigned int outx, unsigned int outy, char outchar)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		DWORD dlo;
		char chdata = outchar;
		curpos.X = outx;
		curpos.Y = outy;
		FillConsoleOutputAttribute(hConsoleOutput, color, 1, curpos, &dlo);
		WriteConsoleOutputCharacterA(hConsoleOutput, &chdata, 1, curpos, &dlo);
	} else
#endif
	{
		char *addr;
		outy *= textscreen_maxx;
		outy += outx;
		outy *= TEXTSCREEN_BYTES_PER_CHAR;
		outy += TEXTSCREEN_LIN_ADDR;
		addr = (char *)outy;
		addr[0] = outchar;
		addr[1] = color;
	}
}

unsigned int pds_textdisplay_textxy(unsigned int color, unsigned int outx, unsigned int outy, char *string_s)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		DWORD dlen, dlo;
		curpos.X = outx;
		curpos.Y = outy;
		dlen = pds_strlen(string_s);
		FillConsoleOutputAttribute(hConsoleOutput, color, dlen, curpos, &dlo);
		WriteConsoleOutputCharacterA(hConsoleOutput, string_s, dlen, curpos, &dlo);
		return dlen;
	} else
#endif
	{
		char *addr, *string = string_s;
		outy *= textscreen_maxx;
		outy += outx;
		outy *= TEXTSCREEN_BYTES_PER_CHAR;
		outy += TEXTSCREEN_LIN_ADDR;
		addr = (char *)outy;
		while(string[0] != 0) {
			addr[0] = string[0];
			addr[1] = color;
			addr += 2;
			string++;
		}
		return (string - string_s);	// lenght of string
	}
}

unsigned int pds_textdisplay_textxyn(unsigned int color, unsigned int outx, unsigned int outy, unsigned int maxlen, char *string_s)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		DWORD dlen, dlo;
		curpos.X = outx;
		curpos.Y = outy;
		dlen = min(pds_strlen(string_s), maxlen);
		FillConsoleOutputAttribute(hConsoleOutput, color, dlen, curpos, &dlo);
		WriteConsoleOutputCharacterA(hConsoleOutput, string_s, dlen, curpos, &dlo);
		return dlen;
	} else
#endif
	{
		char *addr, *string = string_s;
		outy *= textscreen_maxx;
		outy += outx;
		outy *= TEXTSCREEN_BYTES_PER_CHAR;
		outy += TEXTSCREEN_LIN_ADDR;
		addr = (char *)outy;
		while(string[0] != 0 && maxlen) {
			addr[0] = string[0];
			addr[1] = color;
			addr += 2;
			string++;
			maxlen--;
		}
		return (string - string_s);	// lenght of string
	}
}


void pds_textdisplay_textxyan(unsigned int x, unsigned int y, char *text, unsigned short *attribs, unsigned int len)
{
	if(!len)
		return;
#ifdef MPXPLAY_WIN32
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		DWORD dlo;
		curpos.X = x;
		curpos.Y = y;
		WriteConsoleOutputAttribute(hConsoleOutput, attribs, (DWORD) len, curpos, &dlo);
		WriteConsoleOutputCharacterA(hConsoleOutput, text, (DWORD) len, curpos, &dlo);
	} else
#endif
	{
		char *addr;
		y *= textscreen_maxx;
		y += x;
		y *= TEXTSCREEN_BYTES_PER_CHAR;
		y += TEXTSCREEN_LIN_ADDR;
		addr = (char *)y;
		do {
			addr[0] = text[0];
			addr[1] = (unsigned char)attribs[0];
			addr += 2;
			text++;
			attribs++;
		} while(--len);
	}
}

void pds_textdisplay_clrscr(void)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos = { 0, 0 };
		DWORD dlo;
		FillConsoleOutputAttribute(hConsoleOutput, (CLB_BASE << 4) | (CL_BASE << 0), textscreen_size, curpos, &dlo);
		FillConsoleOutputCharacterA(hConsoleOutput, (char)' ', textscreen_size, curpos, &dlo);
	} else
#endif
	{
		const unsigned int cval = (CL_BASE << 24) | (CLB_BASE << 28) | (CL_BASE << 8) | (CLB_BASE << 12);	//0x07000700;
		register unsigned int *addr = (unsigned int *)TEXTSCREEN_LIN_ADDR;
		register unsigned int len = textscreen_memory_size / sizeof(unsigned int);
		do {
			*addr++ = cval;
		} while(--len);
	}
}

void pds_textdisplay_scrollup(unsigned int num)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		SMALL_RECT lpScrollRectangle;
		COORD dwDestinationOrigin;
		CHAR_INFO lpFill = { ' ', (CLB_BASE << 4) | (CL_BASE << 0) };
		dwDestinationOrigin.X = 0;
		dwDestinationOrigin.Y = 0;
		lpScrollRectangle.Left = 0;
		lpScrollRectangle.Top = num;
		lpScrollRectangle.Right = textscreen_maxx;
		lpScrollRectangle.Bottom = textscreen_maxy;
		ScrollConsoleScreenBufferA(hConsoleOutput, &lpScrollRectangle, NULL, dwDestinationOrigin, &lpFill);
	} else
#endif
	{
		register unsigned int *addr = (unsigned int *)TEXTSCREEN_LIN_ADDR, *saveaddr = addr;
		unsigned int i, linesize = textscreen_maxx * TEXTSCREEN_BYTES_PER_CHAR / sizeof(unsigned int);

		for(; num; num--) {
			addr = saveaddr;
			i = linesize * (textscreen_maxy - 1);
			for(; i; i--) {
				addr[0] = addr[linesize];
				addr++;
			}
			i = linesize;
			for(; i; i--)
				*addr++ = 0x07000700;
		}
	}
}

void pds_textdisplay_printf(char *outtext)
{
	unsigned int len = pds_strlen(outtext);
	do {
		pds_textdisplay_spacecxyn(0, 0, oldposrow, textscreen_maxx);
		pds_textdisplay_textxyn(7, 0, oldposrow, textscreen_maxx, outtext);
		if(oldposrow >= (textscreen_maxy - 1))
			pds_textdisplay_scrollup(1);
		else {
			oldposrow++;
			pds_textdisplay_setcursor_position(0, oldposrow);
		}
		if(len <= textscreen_maxx)
			break;
		len -= textscreen_maxx;
		outtext += textscreen_maxx;
	} while(1);
}

unsigned int pds_textdisplay_getbkcolorxy(unsigned int outx, unsigned int outy)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		WORD color;
		DWORD dlo;
		curpos.X = outx;
		curpos.Y = outy;
		ReadConsoleOutputAttribute(hConsoleOutput, &color, 1, curpos, &dlo);
		return (unsigned int)(color >> 4);
	} else
#endif
	{
		char *addr;
		outy *= textscreen_maxx;
		outy += outx;
		outy *= TEXTSCREEN_BYTES_PER_CHAR;
		outy += TEXTSCREEN_LIN_ADDR + 1;
		addr = (char *)outy;
		return (addr[0] >> 4);
	}
}

unsigned int pds_textdisplay_lowlevel_getbkcolorxy(unsigned int outx, unsigned int outy)
{
#ifdef __DOS__
	char *addr;
	outy *= textscreen_maxx;
	outy += outx;
	outy *= TEXTSCREEN_BYTES_PER_CHAR;
	outy += TEXTSCREEN_BASE_ADDR + 1;
	addr = (char *)outy;
	return (addr[0] >> 4);
#else
	return 0;
#endif
}

void pds_textdisplay_setcolorxy(unsigned int color, unsigned int outx, unsigned int outy)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		DWORD dlo;
		curpos.X = outx;
		curpos.Y = outy;
		FillConsoleOutputAttribute(hConsoleOutput, color, 1, curpos, &dlo);
	} else
#endif
	{
		unsigned char *addr;
		outy *= textscreen_maxx;
		outy += outx;
		outy *= TEXTSCREEN_BYTES_PER_CHAR;
		outy += TEXTSCREEN_LIN_ADDR + 1;
		addr = (char *)outy;
		addr[0] &= 0xf0;
		addr[0] |= color;
	}
}

void pds_textdisplay_setbkcolorxy(unsigned int bkcolor, unsigned int outx, unsigned int outy)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		WORD color;
		DWORD dlo;
		curpos.X = outx;
		curpos.Y = outy;
		ReadConsoleOutputAttribute(hConsoleOutput, &color, 1, curpos, &dlo);
		color &= 0x0f;
		color |= (bkcolor << 4);
		FillConsoleOutputAttribute(hConsoleOutput, color, 1, curpos, &dlo);
	} else
#endif
	{
		unsigned char *addr;
		outy *= textscreen_maxx;
		outy += outx;
		outy *= TEXTSCREEN_BYTES_PER_CHAR;
		outy += TEXTSCREEN_LIN_ADDR + 1;
		addr = (char *)outy;
		addr[0] &= 0x0f;
		addr[0] |= (bkcolor << 4);
	}
}

void pds_textdisplay_lowlevel_setbkcolorxy(unsigned int bkcolor, unsigned int outx, unsigned int outy)
{
#ifdef __DOS__
	unsigned char *addr;
	outy *= textscreen_maxx;
	outy += outx;
	outy *= TEXTSCREEN_BYTES_PER_CHAR;
	outy += TEXTSCREEN_BASE_ADDR + 1;
	addr = (char *)outy;
	addr[0] &= 0x0f;
	addr[0] |= (bkcolor << 4);
#endif
}

void pds_textdisplay_spacecxyn(unsigned int bkcolor, unsigned int x, unsigned int y, unsigned int len)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		COORD curpos;
		DWORD dlo;
		curpos.X = x;
		curpos.Y = y;
		FillConsoleOutputAttribute(hConsoleOutput, (bkcolor << 4) | (CL_BASE << 0), len, curpos, &dlo);
		FillConsoleOutputCharacterA(hConsoleOutput, (char)' ', len, curpos, &dlo);
	} else
#endif
	{
		bkcolor <<= 4;
		bkcolor |= 7;
		for(; len; len--)
			pds_textdisplay_charxy(bkcolor, x++, y, 32);
	}
}

unsigned int pds_textdisplay_text2buf(unsigned int color, unsigned short *buf, unsigned int buflen, char *string_s)
{
	char *string = string_s;
	if(!buf)
		return 0;
	while(string[0] != 0 && buflen) {
		buf[0] = ((unsigned short)string[0]) | ((unsigned short)color << 8);
		string++;
		buf++;
		buflen--;
	}
	buf[0] = 0x0000;
	return (string - string_s);
}

unsigned int pds_textdisplay_text2field(unsigned int color, unsigned short *buf, char *string_s)
{
	char *string = string_s;
	if(!buf || !string || !string[0])
		return 0;
	do {
		buf[0] = ((unsigned short)string[0]) | ((unsigned short)color << 8);
		buf++;
		string++;
	} while(string[0]);
	return (string - string_s);
}

void pds_textdisplay_textbufxyn(unsigned int outx, unsigned int outy, unsigned short *buf, unsigned int maxlen)
{
#if defined(MPXPLAY_WIN32)
	if(textscreen_linear_address == TEXTSCREEN_BASE_ADDR) {
		unsigned int i;
		unsigned short *abuf;
		char *cbuf;
		abuf = (unsigned short *)alloca(maxlen * sizeof(*abuf));
		if(!abuf)
			return;
		cbuf = (char *)alloca(maxlen * sizeof(*cbuf));
		if(!cbuf)
			return;
		for(i = 0; i < maxlen; i++) {
			abuf[i] = buf[0] >> 8;
			cbuf[i] = buf[0] & 0xff;
			buf++;
		}
		pds_textdisplay_textxyan(outx, outy, cbuf, abuf, maxlen);
	} else
#endif
	{
		unsigned short *addr;
		outy *= textscreen_maxx;
		outy += outx;
		outy *= TEXTSCREEN_BYTES_PER_CHAR;
		outy += TEXTSCREEN_LIN_ADDR;
		addr = (unsigned short *)outy;
		while(buf[0] != 0 && maxlen) {
			addr[0] = buf[0];
			addr++;
			buf++;
			maxlen--;
		}
	}
}

//-------------------------------------------------------------------------
#ifdef MPXPLAY_WIN32
void pds_textdisplay_consolevidmem_merge(unsigned short *attribs, char *text, char *destbuf, unsigned int bufsize)
{
	if(!attribs || !text || !destbuf || !bufsize)
		return;
	bufsize &= ~1;
	do {
		destbuf[0] = *text++;
		destbuf[1] = *attribs++;
		destbuf += 2;
		bufsize -= 2;
	} while(bufsize);
}

void pds_textdisplay_consolevidmem_read(unsigned short *attribs, char *text, unsigned int maxsize)
{
	COORD curpos;
	DWORD dlen, dlo;
	curpos.X = 0;
	curpos.Y = 0;
	dlen = min(maxsize, textscreen_size);
	ReadConsoleOutputAttribute(hConsoleOutput, attribs, textscreen_size, curpos, &dlo);
	ReadConsoleOutputCharacterA(hConsoleOutput, text, textscreen_size, curpos, &dlo);
}

void pds_textdisplay_vidmem_save(void)
{
	unsigned int len;

	newfunc_textdisplay_init();

	len = min(textscreen_size, consolesave_size);
	if(!len)
		return;

	pds_textdisplay_consolevidmem_read(consolesave_attrib, consolesave_text, consolesave_size);

	textscreen_oldmaxy = textscreen_maxy;
	textscreen_oldcursor_y = pds_textdisplay_getcursor_y();
}

#else

void pds_textdisplay_vidmem_save(void)
{
	unsigned int len;
	unsigned int *addr_scr = (unsigned int *)TEXTSCREEN_LIN_ADDR;
	unsigned int *addr_sav = vidmemsave_field;

	newfunc_textdisplay_init();

	len = min(textscreen_memory_size, vidmemsave_size);
	if(!len)
		return;

	pds_qmemcpy(addr_sav, addr_scr, len / 4);

	textscreen_oldmaxy = textscreen_maxy;
	textscreen_oldcursor_y = pds_textdisplay_getcursor_y();
}

#endif

#ifdef MPXPLAY_WIN32
void pds_textdisplay_consolevidmem_separate(unsigned short *attribs, char *text, char *srcbuf, unsigned int bufsize)
{
	if(!attribs || !text || !srcbuf || !bufsize)
		return;
	bufsize &= ~1;
	do {
		*text++ = srcbuf[0];
		*attribs++ = *((unsigned char *)&srcbuf[1]);
		srcbuf += 2;
		bufsize -= 2;
	} while(bufsize);
}

void pds_textdisplay_consolevidmem_write(unsigned short *attribs, char *text, unsigned int maxsize)
{
	COORD curpos;
	DWORD dlen, dlo;
	curpos.X = 0;
	curpos.Y = 0;
	dlen = min(maxsize, textscreen_size);
	WriteConsoleOutputAttribute(hConsoleOutput, attribs, textscreen_size, curpos, &dlo);
	WriteConsoleOutputCharacterA(hConsoleOutput, text, textscreen_size, curpos, &dlo);
}

void pds_textdisplay_consolevidmem_xywrite(unsigned int x, unsigned int y, unsigned short *attribs, char *text, unsigned int len)
{
	COORD curpos;
	DWORD dlo;
	curpos.X = x;
	curpos.Y = y;
	WriteConsoleOutputAttribute(hConsoleOutput, attribs, (DWORD) len, curpos, &dlo);
	WriteConsoleOutputCharacterA(hConsoleOutput, text, (DWORD) len, curpos, &dlo);
}

void pds_textdisplay_vidmem_restore(void)
{
	unsigned int len;

	pds_textdisplay_setresolution(textscreen_oldmaxy + 1);

	len = min(textscreen_size, consolesave_size);
	if(!len)
		return;

	pds_textdisplay_consolevidmem_write(consolesave_attrib, consolesave_text, consolesave_size);

	pds_textdisplay_setcursor_position(0, textscreen_oldcursor_y);
}

#else

void pds_textdisplay_vidmem_restore(void)
{
	unsigned int len;
	unsigned int *addr_scr = (unsigned int *)TEXTSCREEN_LIN_ADDR;
	unsigned int *addr_sav = vidmemsave_field;

	pds_textdisplay_setresolution(textscreen_oldmaxy + 1);

	len = min(textscreen_memory_size, vidmemsave_size);
	if(!len)
		return;

	pds_qmemcpy(addr_scr, addr_sav, len / 4);

	pds_textdisplay_setcursor_position(0, textscreen_oldcursor_y);
}
#endif

//---------------------------------------------------------------------
//bios callings
unsigned int pds_textdisplay_setmode(unsigned int mode)
{
#if defined(MPXPLAY_WIN32)
	//SetConsoleMode(hConsoleOutput,mode);
#elif defined(__DOS__)
	union REGS regs;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = mode;
	int386(0x10, &regs, &regs);
#endif
	return 1;
}

unsigned int pds_textdisplay_getmode(void)
{
#if defined(MPXPLAY_WIN32)
	//DWORD mode;
	//GetConsoleMode(hConsoleOutput,&mode);
	//return mode;
	return 0;
#elif defined(__DOS__)
	union REGS regs;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = 0x0f00;
	int386(0x10, &regs, &regs);
	return (regs.w.ax & 0xff);
#else
	return 0;
#endif
}

unsigned int pds_textdisplay_setlastmode(void)
{
	return pds_textdisplay_setmode(textscreen_mode);
}

void asmset50lines(void);

static void pds_textdisplay_set50lines(void)
{
#ifdef MPXPLAY_WIN32
	COORD size;
	size.X = 80;
	size.Y = 50;
	//#if (_WIN32_WINNT >= 0x0501)
	//SetConsoleDisplayMode(hConsoleOutput,0x03,&size);
	//#else
	SetConsoleScreenBufferSize(hConsoleOutput, size);
	//#endif
#elif defined(__DOS__)
#if defined(NEWFUNC_ASM) && defined(__WATCOMC__)
#pragma aux asmset50lines=\
  "mov ax,1112h"\
  "xor ebx,ebx"\
  "int 10h"\
  modify[eax ebx ecx edx edi esi];
	asmset50lines();
#else
	union REGS regs;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = 0x1112;
	regs.w.bx = 0;
	int386(0x10, &regs, &regs);
#endif
	//redefine_fonts_8x8();
#endif
}

void asmset25lines(void);

static void pds_textdisplay_set25lines(void)
{
#ifdef MPXPLAY_WIN32
	COORD size;
	size.X = 80;
	size.Y = 25;
	//#if (_WIN32_WINNT >= 0x0501)
	//SetConsoleDisplayMode(hConsoleOutput,0x03,&size);
	//#else
	SetConsoleScreenBufferSize(hConsoleOutput, size);
	//#endif
#elif defined(__DOS__)
#if defined(NEWFUNC_ASM) && defined(__WATCOMC__)
#pragma aux asmset25lines=\
  "mov ax,1114h"\
  "xor ebx,ebx"\
  "int 10h"\
  modify[eax ebx ecx edx edi esi];
	asmset25lines();
#else
	union REGS regs;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = 0x1114;
	regs.w.bx = 0;
	int386(0x10, &regs, &regs);
#endif
	//redefine_fonts_8x16();
#endif
}

void pds_textdisplay_setresolution(unsigned int lines)
{
	if(lines >= 40)
		pds_textdisplay_set50lines();
	else
		pds_textdisplay_set25lines();
	newfunc_textdisplay_init();
}

void pds_textdisplay_getresolution(void)
{
#if defined(MPXPLAY_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO bufinf;
	GetConsoleScreenBufferInfo(hConsoleOutput, &bufinf);
	textscreen_maxx = bufinf.dwSize.X;
	textscreen_maxy = bufinf.dwSize.Y;
#elif defined(__DOS__)
	unsigned char *biosmem = (char *)0;
	textscreen_maxx = biosmem[0x44a];
	textscreen_maxy = biosmem[0x484] + 1;
#else
	textscreen_maxx = 80;
	textscreen_maxy = 50;
#endif
}

unsigned int pds_textdisplay_getcursor_y(void)
{
#if defined(MPXPLAY_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO bufinf;
	GetConsoleScreenBufferInfo(hConsoleOutput, &bufinf);
	return bufinf.dwCursorPosition.Y;
#elif defined(__DOS__)
	unsigned char *biosmem = (char *)0;
	return biosmem[0x451];
#else
	return 0;
#endif
}

void asm_textdisplaysetcursorposition(unsigned int);

void pds_textdisplay_setcursor_position(unsigned int x, unsigned int y)
{
#if defined(MPXPLAY_WIN32)
	COORD curpos;
	curpos.X = x;
	curpos.Y = y;
	SetConsoleCursorPosition(hConsoleOutput, curpos);
#elif defined(__DOS__)
#if defined(NEWFUNC_ASM) && defined (__WATCOMC__)
#pragma aux asm_textdisplaysetcursorposition=\
  "mov ax,0200h"\
  "xor ebx,ebx"\
  "int 10h"\
  parm[edx] modify[eax ebx ecx edx edi esi];
	asm_textdisplaysetcursorposition((y << 8) | x);
#else
	union REGS regs;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = 0x0200;
	regs.w.dx = ((y << 8) | x);
	int386(0x10, &regs, &regs);
#endif
#endif
}

void asm_textdisplaysetcursorshape(long);

void pds_textdisplay_setcursorshape(long value)
{
#if defined(MPXPLAY_WIN32)
	CONSOLE_CURSOR_INFO curinf;
	if(value & TEXTCURSORSHAPE_HIDDEN) {
		curinf.dwSize = 8;
		curinf.bVisible = 0;
	} else {
		curinf.dwSize = (value == TEXTCURSORSHAPE_FULLBOX) ? 96 : 8;
		curinf.bVisible = 1;
	}
	SetConsoleCursorInfo(hConsoleOutput, &curinf);
#elif defined(__DOS__)
#if defined(NEWFUNC_ASM) && defined (__WATCOMC__)
#pragma aux asm_textdisplaysetcursorshape=\
  "mov ecx,eax"\
  "mov ax,0100h"\
  "int 10h"\
  parm[eax] modify[eax ebx ecx edx edi esi];
	asm_textdisplaysetcursorshape(value);
#else
	union REGS regs;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = 0x0100;
	regs.w.cx = value;
	int386(0x10, &regs, &regs);
#endif
#endif
}

void pds_textdisplay_resetcolorpalette(void)
{
#ifdef __DOS__
	outp(0x03c8, 5);			// magenta
	outp(0x03c9, 0x2a);
	outp(0x03c9, 0);
	outp(0x03c9, 0x2a);
#endif
}

//*************************************************************************
// character generator

/*struct{
 unsigned char code;
 unsigned char data[8];
}fontdata8[2]={{219,0x00,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe},
	      {220,0x00,0x00,0x00,0x00,0xfe,0xfe,0xfe,0xfe}};*/
// 00000000
// 00000000
// 00000000
// 00000000
// 11100000
// 00010000
// 00011000
// 00011000

/*struct{
 unsigned char code;
 unsigned char ccount;
 unsigned char data[8];
}fontdata8[10]={{219 ,0,0x00,0x6c,0x6c,0x6c,0x6c,0x6c,0x6c,0x6c},
		//{219,10,0x00,0x7e,0x7e,0x7e,0x00,0x7e,0x7e,0x7e},
		{220 ,0,0x00,0x00,0x00,0x00,0xfe,0xfe,0xfe,0xfe},

		{240 ,0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
		{241 ,0,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00},

		{242 ,0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f},
		{243 ,0,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		{244 ,0,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		{245 ,0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0},

		{246 ,0,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08},
		{247 ,0,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10}};

void redefine_fonts_8x8(void)
{
 int c,d;
 struct rminfo RMI;

 for(c=0;c<2;c++){
  for(d=0;d<8;d++)
   dosmemput[d]=fontdata8[c].data[d];

  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX = 0x00001100;
  RMI.EBX = 0x00000800;
  RMI.ECX = 0x00000001;
  RMI.EDX = fontdata8[c].code;
  RMI.ES  = segment1;
  RMI.EBP = 0;
  pds_dpmi_realmodeint_call(0x10,&RMI);
 }
}

struct{
 unsigned char code;
 unsigned char data[16];
}fontdata16[2]={
 {219,0x00,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0x00,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe},  // 219
 {220,0x00,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0x00,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe}}; // 220

void redefine_fonts_8x16(void)
{
 int c,d;
 struct rminfo RMI;

 for(c=0;c<2;c++)
  for(d=0;d<16;d++)
   dosmemput[(c*16)+d]=fontdata16[c].data[d];

 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX = 0x00001100;
 RMI.EBX = 0x00001000;
 RMI.ECX = 0x00000002;
 RMI.EDX = fontdata16[0].code;
 RMI.ES  = segment1;
 RMI.EBP = 0;
 pds_dpmi_realmodeint_call(0x10,&RMI);
}*/
