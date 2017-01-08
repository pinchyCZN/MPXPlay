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
//function: string handling

#include <string.h>
#include "newfunc.h"

unsigned int pds_strcpy(char *dest, char *src)
{
	char *begin;
	if(!dest || !src)
		return 0;
	begin = src;
	do {
		*dest = *src;
		if(!src[0])
			break;
		dest++;
		src++;
	} while(1);
	return (src - begin);		// returns the lenght of string, not the target pointer!
}

unsigned int pds_strmove(char *dest, char *src)
{
	unsigned int len, count;
	if(!dest || !src)
		return 0;
	if(dest < src)
		return pds_strcpy(dest, src);
	count = len = pds_strlen(src) + 1;
	src += len;
	dest += len;
	do {
		src--;
		dest--;
		*dest = *src;
	} while(--count);
	return len;					// returns the lenght of string
}

unsigned int pds_strncpy(char *dest, char *src, unsigned int maxlen)
{
	char *begin;
	if(!dest || !src || !maxlen)
		return 0;
	begin = src;
	do {
		*dest = *src;
		if(!src[0])
			break;
		dest++;
		src++;
	} while(--maxlen);
	return (src - begin);		// returns the lenght of string, not the target pointer!
}

unsigned int pds_strcat(char *strp1, char *strp2)
{
	if(!strp1 || !strp2)
		return 0;
	return pds_strcpy(&strp1[pds_strlen(strp1)], strp2);
}

static int pds_strchknull(char *str1, char *str2)
{
	if(!str1 || !str1[0])
		if(str2 && str2[0])
			return -1;
		else
			return 0;
	if(!str2 || !str2[0])
		if(str1 && str1[0])
			return 1;
		else
			return 0;

	return 2;
}

int pds_strcmp(char *strp1, char *strp2)
{
	char c1, c2;
	int retcode = pds_strchknull(strp1, strp2);
	if(retcode != 2)
		return retcode;

	do {
		c1 = *strp1;
		c2 = *strp2;
		if(c1 != c2)
			if(c1 < c2)
				return -1;
			else
				return 1;
		strp1++;
		strp2++;
	} while(c1 && c2);
	return 0;
}

int pds_stricmp(char *strp1, char *strp2)
{
	char c1, c2;
	int retcode = pds_strchknull(strp1, strp2);
	if(retcode != 2)
		return retcode;

	do {
		c1 = *strp1++;
		c2 = *strp2++;
		if(c1 != c2) {
			if(c1 >= 'a' && c1 <= 'z')	// convert to uppercase
				c1 -= 32;		// c1-='a'-'A'
			if(c2 >= 'a' && c2 <= 'z')
				c2 -= 32;
			if(c1 != c2)
				if(c1 < c2)
					return -1;
				else
					return 1;
		}
	} while(c1 && c2);
	return 0;
}

int pds_strricmp(char *str1, char *str2)
{
	char *pstr1 = str1, *pstr2 = str2;
	int retcode = pds_strchknull(str1, str2);
	if(retcode != 2)
		return retcode;

	while(pstr1[0] != 0)
		pstr1++;
	while(pstr1[0] == 0 || pstr1[0] == 32)
		pstr1--;
	if(pstr1 <= str1)
		return 1;
	while(pstr2[0] != 0)
		pstr2++;
	while(pstr2[0] == 0 || pstr2[0] == 32)
		pstr2--;
	if(pstr2 <= str2)
		return -1;
	while(pstr1 >= str1 && pstr2 >= str2) {
		char c1 = pstr1[0];
		char c2 = pstr2[0];
		if(c1 >= 'a' && c1 <= 'z')	// convert to uppercase
			c1 -= 32;
		if(c2 >= 'a' && c2 <= 'z')
			c2 -= 32;
		if(c1 != c2) {
			if(c1 < c2)
				return -1;
			else
				return 1;
		}
		pstr1--;
		pstr2--;
	}
	return 0;
}

int pds_strlicmp(char *str1, char *str2)
{
	char c1, c2;
	int retcode = pds_strchknull(str1, str2);
	if(retcode != 2)
		return retcode;

	do {
		c1 = *str1;
		c2 = *str2;
		if(!c1 || !c2)
			break;
		if(c1 != c2) {
			if(c1 >= 'a' && c1 <= 'z')	// convert to uppercase
				c1 -= 32;
			if(c2 >= 'a' && c2 <= 'z')
				c2 -= 32;
			if(c1 != c2) {
				if(c1 < c2)
					return -1;
				else
					return 1;
			}
		}
		str1++;
		str2++;
	} while(1);
	return 0;
}

int pds_strncmp(char *strp1, char *strp2, unsigned int counter)
{
	char c1, c2;
	int retcode = pds_strchknull(strp1, strp2);
	if(retcode != 2)
		return retcode;
	if(!counter)
		return 0;
	do {
		c1 = *strp1;
		c2 = *strp2;
		if(c1 != c2)
			if(c1 < c2)
				return -1;
			else
				return 1;
		strp1++;
		strp2++;
	} while(c1 && c2 && --counter);
	return 0;
}

int pds_strnicmp(char *strp1, char *strp2, unsigned int counter)
{
	char c1, c2;
	int retcode = pds_strchknull(strp1, strp2);
	if(retcode != 2)
		return retcode;
	if(!counter)
		return 0;
	do {
		c1 = *strp1;
		c2 = *strp2;
		if(c1 != c2) {
			if(c1 >= 'a' && c1 <= 'z')
				c1 -= 32;
			if(c2 >= 'a' && c2 <= 'z')
				c2 -= 32;
			if(c1 != c2) {
				if(c1 < c2)
					return -1;
				else
					return 1;
			}
		}
		strp1++;
		strp2++;
	} while(c1 && c2 && --counter);
	return 0;
}

unsigned int pds_strlen(char *strp)
{
	char *beginp;
	if(!strp || !strp[0])
		return 0;
	beginp = strp;
	do {
		strp++;
	} while(*strp);
	return (unsigned int)(strp - beginp);
}

unsigned int pds_strlenc(char *strp, char seek)
{
	char *lastnotmatchp, *beginp;

	if(!strp || !strp[0])
		return 0;

	lastnotmatchp = NULL;
	beginp = strp;
	do {
		if(*strp != seek)
			lastnotmatchp = strp;
		strp++;
	} while(*strp);

	if(!lastnotmatchp)
		return 0;
	return (unsigned int)(lastnotmatchp - beginp + 1);
}

/*unsigned int pds_strlencn(char *strp,char seek,unsigned int len)
{
 char *lastnotmatchp,*beginp;

 if(!strp || !strp[0] || !len)
  return 0;

 lastnotmatchp=NULL;
 beginp=strp;
 do{
  if(*strp!=seek)
   lastnotmatchp=strp;
  strp++;
 }while(*strp && --len);

 if(!lastnotmatchp)
  return 0;
 return (unsigned int)(lastnotmatchp-beginp+1);
}*/

char *pds_strchr(char *strp, char seek)
{
	if(!strp || !strp[0])
		return NULL;
	do {
		if(*strp == seek)
			return strp;
		strp++;
	} while(*strp);
	return NULL;
}

char *pds_strrchr(char *strp, char seek)
{
	char *foundp = NULL, curr;

	if(!strp)
		return foundp;

	curr = *strp;
	if(!curr)
		return foundp;
	do {
		if(curr == seek)
			foundp = strp;
		strp++;
		curr = *strp;
	} while(curr);
	return foundp;
}

char *pds_strnchr(char *strp, char seek, unsigned int len)
{
	if(!strp || !strp[0] || !len)
		return NULL;
	do {
		if(*strp == seek)
			return strp;
		strp++;
	} while(*strp && --len);
	return NULL;
}

char *pds_strstr(char *s1, char *s2)
{
	if(s1 && s2 && s2[0]) {
		char c20 = *s2;
		do {
			char c1 = *s1;
			if(!c1)
				break;
			if(c1 == c20) {		// search the first occurence
				char *s1p = s1, *s2p = s2;
				do {			// compare the strings (part of s1 with s2)
					char c2 = *(++s2p);
					if(!c2)
						return s1;
					c1 = *(++s1p);
					if(!c1)
						return NULL;
					if(c1 != c2)
						break;
				} while(1);
			}
			s1++;
		} while(1);
	}
	return NULL;
}

char *pds_strstri(char *s1, char *s2)
{
	if(s1 && s2 && s2[0]) {
		char c20 = *s2;
		if(c20 >= 'a' && c20 <= 'z')	// convert to uppercase (first character of s2)
			c20 -= 32;
		do {
			char c1 = *s1;
			if(!c1)
				break;
			if(c1 >= 'a' && c1 <= 'z')	// convert to uppercase (current char of s1)
				c1 -= 32;
			if(c1 == c20) {		// search the first occurence
				char *s1p = s1, *s2p = s2;
				do {			// compare the strings (part of s1 with s2)
					char c2;
					s2p++;
					c2 = *s2p;
					if(!c2)
						return s1;
					s1p++;
					c1 = *s1p;
					if(!c1)
						return NULL;
					if(c1 >= 'a' && c1 <= 'z')	// convert to uppercase
						c1 -= 32;
					if(c2 >= 'a' && c2 <= 'z')	// convert to uppercase
						c2 -= 32;
					if(c1 != c2)
						break;
				} while(1);
			}
			s1++;
		} while(1);
	}
	return NULL;
}

unsigned int pds_strcutspc(char *src)
{
	char *dest, *dp;

	if(!src)
		return 0;

	dest = src;

	while(src[0] && (src[0] == 32))
		src++;

	if(!src[0]) {
		dest[0] = 0;
		return 0;
	}
	if(src > dest) {
		char c;
		dp = dest;
		do {
			c = *src++;			// move
			*dp++ = c;			//
		} while(c);
		dp -= 2;
	} else {
		while(src[1])
			src++;
		dp = src;
	}
	while((*dp == 32) && (dp >= dest))
		*dp-- = 0;

	if(dp < dest)
		return 0;

	return (dp - dest + 1);
}

unsigned int pds_str_fixlenc(char *str, unsigned int newlen, char c)
{
	unsigned int currlen = pds_strlen(str);
	if(currlen < newlen) {
		str += currlen;
		do {
			*str++ = c;
		} while((++currlen) < newlen);
	}
	str[newlen] = 0;
	return newlen;
}

void pds_listline_slice(char **listparts, char *cutchars, char *listline)
{
	char *lastpart = listline, *nextpart = NULL;
	int i, ccn = (int)pds_strlen(cutchars) + 1;	// +1 search for the trailing zero
	for(i = 0; i < ccn;) {
		listparts[i] = lastpart;
		do {
			nextpart = pds_strchr(lastpart, cutchars[i]);
			if(nextpart) {
				*nextpart++ = 0;
				lastpart = nextpart;
			}
			i++;
		} while(!nextpart && i < ccn);
	}
}

// does str contains uppercase chars only?
unsigned int pds_chkstr_uppercase(char *str)
{
	if(!str || !str[0])
		return 0;
	do {
		char c = *str;
		if((c >= 'a') && (c <= 'z'))	// found lowercase char
			return 0;
		if(c >= 128)			// found non-us char
			return 0;
		str++;
	} while(*str);
	return 1;
}

void pds_ltoa(int value, char *ltoastr)
{
	static unsigned int dekadlim[10] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };
	unsigned int dekad;

	dekad = 1;
	while((value >= dekadlim[dekad]) && (dekad < 10))
		dekad++;
	do {
		*ltoastr++ = (value / dekadlim[dekad - 1]) % 10 + 0x30;
	} while(--dekad);
	*ltoastr = 0;
}

/*void pds_ltoa16(int value,char *ltoastr)
{
 static int dekadlim[9]={1,0x10,0x100,0x1000,0x10000,0x100000,0x1000000,0x10000000,0x100000000};
 int dekad;

 dekad=1;
 while(value>dekadlim[dekad] && dekad<9)
  dekad++;
 do{
  int number=(value/dekadlim[dekad-1])%16;
  if(number>9)
   ltoastr[0]=number-10+'A';
  else
   ltoastr[0]=number+'0';
  dekad--;
  ltoastr++;
 }while(dekad>0);
 ltoastr[0]=0;
}*/

long pds_atol(char *strp)
{
	long number = 0;
	unsigned int negative = 0;

	if(!strp || !strp[0])
		return number;

	while(*strp == ' ')
		strp++;

	if(*strp == '-') {
		negative = 1;
		strp++;
	} else {
		if(*strp == '+')
			strp++;
	}

	do {
		if((strp[0] < '0') || (strp[0] > '9'))
			break;
		number = (number << 3) + (number << 1);	// number*=10;
		number += (unsigned long)strp[0] - '0';
		strp++;
	} while(1);
	if(negative)
		number = -number;
	return number;
}

mpxp_int64_t pds_atoi64(char *strp)
{
	mpxp_int64_t number = 0;
	unsigned int negative = 0;

	if(!strp || !strp[0])
		return number;

	while(*strp == ' ')
		strp++;

	if(*strp == '-') {
		negative = 1;
		strp++;
	} else {
		if(*strp == '+')
			strp++;
	}

	do {
		if((strp[0] < '0') || (strp[0] > '9'))
			break;
		number = (number << 3) + (number << 1);	// number*=10;
		number += (unsigned long)strp[0] - '0';
		strp++;
	} while(1);
	if(negative)
		number = -number;
	return number;
}

long pds_atol16(char *strp)
{
	unsigned long number = 0;

	if(!strp || !strp[0])
		return number;

	while(*strp == ' ')
		strp++;

	if(*((unsigned short *)strp) == (((unsigned short)'x' << 8) | (unsigned short)'0'))
		strp += 2;

	do {
		char c = *strp++;
		if(c >= '0' && c <= '9')
			c -= '0';
		else if(c >= 'a' && c <= 'f')
			c -= ('a' - 10);
		else if(c >= 'A' && c <= 'F')
			c -= ('A' - 10);
		else
			break;

		number <<= 4;			// number*=16;
		number += (unsigned long)c;
	} while(1);

	return number;
}

//-----------------------------------------------------------------------
// some filename/path routines (string handling only, no DOS calls)
char *pds_getfilename_from_fullname(char *fullname)
{
	char *filenamep;

	if(!fullname)
		return NULL;

	filenamep = pds_strrchr(fullname, PDS_DIRECTORY_SEPARATOR_CHAR);
	if(filenamep)
		filenamep++;
	else {
		filenamep = fullname;
		if(filenamep[1] == ':' && filenamep[2])	// if no filename, it gives back drive-name (ie: C:) ! (if exists) (required for playlist-editor/directory-browser and for some LCD items)
			filenamep += 2;
	}
	return filenamep;
}

void pds_getfilename_noext_from_fullname(char *strout, char *fullname)
{
	char *filename, *extension;

	if(!strout)
		return;
	if(!fullname || !fullname[0]) {
		*strout = 0;
		return;
	}

	filename = pds_getfilename_from_fullname(fullname);
	extension = pds_strrchr(filename, '.');

	if(extension > filename) {
		unsigned int len = extension - filename;
		pds_strncpy(strout, filename, len);
		strout[len] = 0;
	} else
		pds_strcpy(strout, filename);
}

char *pds_getpath_from_fullname(char *path, char *fullname)
{
	char *filenamep;

	if(!path)
		return NULL;
	if(!fullname) {
		*path = 0;
		return NULL;
	}

	if(path != fullname)
		pds_strcpy(path, fullname);
	filenamep = pds_strrchr(path, PDS_DIRECTORY_SEPARATOR_CHAR);
	if(filenamep) {
		if((filenamep == path) || (path[1] == ':' && filenamep == (path + 2)))	// "\\filename.xxx" or "c:\\filename.xxx"
			filenamep[1] = 0;
		else
			filenamep[0] = 0;	// c:\\subdir\\filename.xxx
		filenamep++;
	} else {
		filenamep = pds_strchr(path, ':');
		if(filenamep)
			*(++filenamep) = 0;
		else {
			filenamep = path;
			*path = 0;
		}
	}
	filenamep = fullname + (filenamep - path);
	return filenamep;
}

char *pds_filename_get_extension(char *fullname)
{
	char *ext = NULL, *fn = pds_getfilename_from_fullname(fullname);
	if(fn) {
		ext = pds_strrchr(fn, '.');
		if(ext)
			ext++;
	}
	return ext;
}

unsigned int pds_filename_conv_slashes_to_local(char *filename)
{
	unsigned int found = 0;
	char c;
	if(!filename)
		return found;

	do {
		c = *filename;
		if(c == PDS_DIRECTORY_SEPARATOR_CHRO) {
			c = PDS_DIRECTORY_SEPARATOR_CHAR;
			*filename = c;
			found = 1;
		}
		filename++;
	} while(c);
	return found;
}

unsigned int pds_filename_conv_slashes_to_unxftp(char *filename)
{
	unsigned int found = 0;
#if (PDS_DIRECTORY_SEPARATOR_CHAR!=PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)
	char c;
	if(!filename)
		return found;

	do {
		c = *filename;
		if(c == PDS_DIRECTORY_SEPARATOR_CHAR) {
			c = PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP;
			*filename = c;
			found = 1;
		}
		filename++;
	} while(c);
#endif
	return found;
}

// !!! without path
void pds_filename_conv_forbidden_chars(char *filename)
{
	char c, *s, *d;
	s = d = filename;
	while((c = *s)) {
		if((c == '?') || (c == '*') || (c == ':') || (c == '|') || (c < 0x20) || (c > 254)) {
			s++;
			continue;
		}
		if((c == '/') || (c == '\\'))
			c = ',';
		else if(c == '\"')
			c = '\'';
		else if(c == '<')
			c = '(';
		else if(c == '>')
			c = ')';
		*d = c;
		s++;
		d++;
	}
	*d = 0;
}

int pds_getdrivenum_from_path(char *path)
{
	// a=0 b=1 c=2 ...
	if(path && path[0] && path[1] == ':') {
		char d = path[0];
		if(d >= 'a' && d <= 'z')
			return (d - 'a');
		if(d >= 'A' && d <= 'Z')
			return (d - 'A');
		if(d >= '0' && d <= '7')	// !!!
			return ((d - '0') + ('Z' - 'A' + 1));	// for remote drives
	}
	return -1;
}

unsigned int pds_path_is_dir(char *path)	// does path seem to be a directory?
{
	char *p;

	if(!path || !path[0])
		return 0;
	if(path[1] == ':' && pds_getdrivenum_from_path(path) >= 0)	// d:
		if(path[2] == 0 || (path[2] == PDS_DIRECTORY_SEPARATOR_CHAR && path[3] == 0))
			return 1;
	if(path[0] == '.' && path[1] == '.' && path[2] == 0)	// ..
		return 1;
	if(path[0] == '.' && path[1] == 0)	// .
		return 1;
	if(path[0] == PDS_DIRECTORY_SEPARATOR_CHAR && path[1] == 0)	// backslash only
		return 1;
	p = pds_strrchr(path, PDS_DIRECTORY_SEPARATOR_CHAR);	// backslash at end
	if(p && p[1] == 0)
		return 1;
	if(pds_strchr(path, '*') || pds_strchr(path, '?'))	// directory name/path may not contain wildchars
		return 0;

	return 1;
}

// !!! "d:" is also accepted as full-path
unsigned int pds_filename_check_absolutepath(char *path)
{
	char *dd = pds_strnchr(path, ':', PDS_DIRECTORY_DRIVESTRLENMAX);	// for non local drives too (like ftp:)
	if(dd && (!dd[1] || (dd[1] == PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN) || (dd[1] == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)))
		return 1;
	return 0;
}

//remove ".." from filename (ie: d:\temp\..\track01.mp3 -> d:\track01.mp3)
unsigned int pds_filename_remove_relatives(char *filename)
{
	char *fn, *dd, *next, *prev, *curr;
	char currdirstr[4] = { PDS_DIRECTORY_SEPARATOR_CHAR, '.', PDS_DIRECTORY_SEPARATOR_CHAR, 0 };
	char updirstr[4] = { PDS_DIRECTORY_SEPARATOR_CHAR, '.', '.', 0 };

	if(!filename)
		return 0;

	fn = filename;
	do {
		dd = pds_strstr(fn, currdirstr);	// "\.\"
		if(!dd)
			break;
		pds_strcpy(dd, dd + 2);	// "\.\" -> "\"
		fn = dd;
	} while(1);

	fn = filename;
	do {
		dd = pds_strstr(fn, updirstr);	// "\.."
		if(!dd)
			break;
		prev = NULL;
		curr = dd - 1;
		while(curr >= filename) {	// search prev "\"
			if(*curr == PDS_DIRECTORY_SEPARATOR_CHAR) {
				prev = curr;
				break;
			}
			curr--;
		}
		next = dd + 3;			// next "\" or eol
		if(!prev)
			prev = dd;
		if(!*next)				//
			prev++;				// "\.." -> "\"
		pds_strcpy(prev, next);
		fn = prev;
	} while(1);
	return pds_strlen(filename);
}

unsigned int pds_filename_build_fullpath(char *destbuf, char *currdir, char *filename)
{
	unsigned int len;
	char *p;

	if(!destbuf)
		return 0;
	if(!currdir || !filename) {
		*destbuf = 0;
		return 0;
	}

	if(pds_filename_check_absolutepath(filename)) {
		pds_strcpy(destbuf, filename);
		return pds_filename_remove_relatives(destbuf);
	}

	if(!currdir[0]) {
		*destbuf = 0;
		return 0;
	}

	len = pds_strcpy(destbuf, currdir);

	if((filename[0] == PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN) || (filename[0] == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)) {
		p = pds_strchr(destbuf, filename[0]);
		if(p) {
			if(p[1] == filename[0])	// "//"
				p++;
			*p = 0;
			len = p - destbuf;
		}
	} else {
		if((destbuf[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN) && (destbuf[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP) && filename[0])
			len += pds_strcpy(&destbuf[len], PDS_DIRECTORY_SEPARATOR_STR);
	}
	pds_strcpy(&destbuf[len], filename);
	return pds_filename_remove_relatives(destbuf);
}

unsigned int pds_filename_assemble_fullname(char *destbuf, char *path, char *name)
{
	unsigned int len;
	if(!destbuf)
		return 0;
	if(path && path[0]) {
		if(destbuf == path)
			len = pds_strlen(path);
		else
			len = pds_strcpy(destbuf, path);
		if(destbuf[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR)
			len += pds_strcpy(&destbuf[len], PDS_DIRECTORY_SEPARATOR_STR);
		len += pds_strcpy(&destbuf[len], pds_getfilename_from_fullname(name));
	} else
		len = pds_strcpy(destbuf, name);
	return len;
}

unsigned int pds_filename_wildchar_chk(char *filename)
{
	if(!filename)
		return 0;
	if(pds_strchr(filename, '?') || pds_strchr(filename, '*'))
		return 1;
	return 0;
}

unsigned int pds_filename_wildchar_cmp(char *fullname, char *mask)
{
	unsigned int match;
	char *fn, *fe, *mn, *me;

	if(!fullname || !fullname[0] || !mask || !mask[0])
		return 0;
	if(pds_strcmp(mask, PDS_DIRECTORY_ALLFILE_STR) == 0 || pds_strcmp(mask, "*.?*") == 0)
		return 1;

	fn = pds_getfilename_from_fullname(fullname);
	if(!fn)
		return 0;
	fe = pds_strrchr(fn, '.');
	if(fe) {
		fe++;
		if(!fe[0])
			fe = NULL;
	}

	mn = mask;
	me = pds_strrchr(mn, '.');
	if(me) {
		me++;
		if(!me[0])
			me = NULL;
	}

	if((!fe && me) || (fe && !me))
		return 0;

	//fprintf(stdout,"fn:%s fe:%s me:%s\n",fn,fe,me);

	match = 1;
	while(fn[0] && (!fe || (fn < fe))) {	// check filename (without extension)
		//fprintf(stdout,"fn:%c mn:%c\n",fn[0],mn[0]);
		if(!mn[0] || (me && mn >= me)) {
			match = 0;
			break;
		}
		if(mn[0] == '*')		// ie: track*
			break;
		if(mn[0] != '?') {
			char cf, cm;
			cf = fn[0];
			if(cf >= 'a' && cf <= 'z')
				cf -= 'a' - 'A';
			cm = mn[0];
			if(cm >= 'a' && cm <= 'z')
				cm -= 'a' - 'A';
			if(cf != cm) {
				match = 0;
				break;
			}
		}
		fn++;
		mn++;
	}

	if(!fn[0] && mn[0] && (mn[0] != '*'))	// mask is longer than filename
		match = 0;
	if(me && (mn < me) && (mn[0] != '*'))	// mask is longer than filename
		match = 0;

	if(match && fe && me) {		// check extension
		//fprintf(stdout,"fe:%c me:%c\n",fe[0],me[0]);
		do {
			if(!me[0]) {
				if(fe[0])
					match = 0;
				break;
			}
			if(me[0] == '*')
				break;
			if(!fe[0]) {
				match = 0;
				break;
			}
			if(me[0] != '?') {
				char cf, cm;
				cf = fe[0];
				if(cf >= 'a' && cf <= 'z')
					cf -= 'a' - 'A';
				cm = me[0];
				if(cm >= 'a' && cm <= 'z')
					cm -= 'a' - 'A';
				if(cf != cm) {
					match = 0;
					break;
				}
			}
			fe++;
			me++;
		} while(1);
	}

	return match;
}

unsigned int pds_stri_wildchar_cmp(char *str, char *mask)
{
	unsigned int match;
	char ms;

	if(!mask || !mask[0])
		return 0;
	if(mask[0] == '*' && !mask[1])
		return 1;
	if(!str || !str[0])
		return 0;

	match = 1;
	ms = *mask;
	if(ms >= 'a' && ms <= 'z')	// convert to uppercase (first character of mask)
		ms -= 32;
	do {
		char c1 = *str;
		if(!c1) {
			match = 0;
			break;
		}
		if(c1 >= 'a' && c1 <= 'z')	// convert to uppercase (current char of s1)
			c1 -= 32;
		if((c1 == ms) || (ms == '?')) {	// search the first occurence
			char *s1p = str, *s2p = mask;
			do {				// compare the strings (part of str with mask)
				char c2 = *(++s2p);
				if(!c2) {
					match = 2;
					break;
				}
				if(c2 == '*') {
					if(s2p[1])
						match = 0;
					else
						match = 2;
					break;
				}
				c1 = *(++s1p);
				if(!c1) {
					match = 0;
					break;
				}
				if(c2 != '?') {
					if(c1 >= 'a' && c1 <= 'z')	// convert to uppercase
						c1 -= 32;
					if(c2 >= 'a' && c2 <= 'z')	// convert to uppercase
						c2 -= 32;
					if(c1 != c2)
						break;
				}
			} while(1);
		}
		str++;
	} while(match == 1);
	return match;
}

// create a new filename from filename+mask (modification)
unsigned int pds_filename_wildchar_rename(char *destname, char *srcname, char *mask)
{
	char *fnd, *fns, *fne, *fed, *fes, *fee, *mn, *me;

	if(!destname)
		return 0;
	if(!srcname || !srcname[0] || !mask || !mask[0]) {
		*destname = 0;
		return 0;
	}

	if(!pds_strchr(mask, '*') && !pds_strchr(mask, '?')) {	// mask does not contain wildchars
		pds_strcpy(destname, mask);	// then use mask as a new filename
		return 1;
	}

	pds_strcpy(destname, srcname);

	if(pds_strcmp(mask, "*.*") == 0 || pds_strcmp(mask, "*.?*") == 0)
		return 1;

	fns = pds_getfilename_from_fullname(srcname);
	if(!fns)
		return 0;
	fes = pds_strrchr(fns, '.');
	if(fes) {
		fne = fes;				// end of 1st part (before extension)
		fes++;
	} else
		fne = fes = fns + pds_strlen(fns);	// end of srcfilename (no extension)

	fnd = pds_getfilename_from_fullname(destname);
	if(!fnd)
		return 0;
	fed = pds_strrchr(fnd, '.');
	if(fed)
		fed++;

	mn = mask;
	me = pds_strrchr(mn, '.');
	if(me)
		me++;

	do {						// check filename (without extension)
		//fprintf(stdout,"1. fns:%c fnd:%c mn:%c %8.8X %8.8X\n",fns[0],fnd[0],mn[0],mn,me);
		if(!mn[0] || (me && mn >= (me - 1))) {	// end of first part of the mask
			break;
		}
		if(mn[0] == '*') {		// ie: track*
			if(fns < fne)
				fnd += (fne - fns);	// skip non-modified chars
			break;
		}
		if(mn[0] == '?') {
			if(fns >= fne)		// mask run out from filename (without extension)
				break;
			*fnd = *fns;		// copy char from src to dest
		} else
			*fnd = *mn;			// modify/insert filename with char from mask
		//fprintf(stdout,"3. fns:%c fnd:%c mn:%c\n",fns[0],fnd[0],mn[0]);
		fns++;
		fnd++;
		mn++;
	} while(1);

	fnd[0] = fnd[1] = 0;		// close filename
	fed = &fnd[1];				// new extension pos (leave space for dot)

	//if(fes || me){ // check extension
	if(me && me[0]) {
		fee = fes + pds_strlen(fes);	// end of extension (src)
		do {
			//fprintf(stdout,"1. fes:%c fed:%c me:%c\n",fes[0],fed[0],me[0]);
			if(me[0] == '*') {	// ie: .m*
				while(*fes)		// no more modification
					*fed++ = *fes++;	// copy left chars from src to dest
				break;
			}

			if(me[0] == '?') {
				if(fes >= fee)	// mask run out from srcfilename
					break;		// finish
				*fed = *fes;	// copy char from src to dest
			} else
				*fed = *me;		// modify extension with char from mask
			//fprintf(stdout,"3. fes:%c fed:%c me:%c\n",fes[0],fed[0],me[0]);
			fed++;
			fes++;
			me++;
		} while(me[0]);

		*fed = 0;
	}

	if(fed > &fnd[1])			// filename has extension
		fnd[0] = '.';			// insert dot in filename before extension

	return 1;
}
