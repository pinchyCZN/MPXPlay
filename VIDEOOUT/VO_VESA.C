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
// VESA-bios query

#include "newfunc\newfunc.h"
#include "videoout.h"
#ifndef WIN32
#include <mem.h>
#endif
#include <string.h>

#ifdef __DOS__

//#define VESA_DEBUG 1

typedef struct VESAINFO_s {
	unsigned char VESASignature[4];
	short VESAVersion;
	char *OEMStringPtr;
	unsigned long Capabilities;
	unsigned short VideoModePtr_seg;
	unsigned short VideoModePtr_off;
	short TotalMemory;
	short reserved;
	unsigned long OEMSoftwareRev;
	char *OEMVendorNamePtr;
	char *OEMProductRevPtr;
	unsigned char VIReserved[222];
	unsigned char VIReserved2[256];
} VESAINFO_s;

typedef struct SVGAINFO_s {
	short ModeAttributes;
	unsigned char WinAAttributes;
	unsigned char WinBAttributes;
	short WinGranularity;
	short WinSize;
	short WinASegment;
	short WinBSegment;
	unsigned long WinFuncPtr;
	short BytesPerScanLine;
//---------------------- rest is optional info (since Version 1.2)
	short XResolution;
	short YResolution;
	unsigned char XCharSize;
	unsigned char YCharSize;
	unsigned char NumberOfPlanes;
	unsigned char BitsPerPixel;
	unsigned char NumberOfBanks;
	unsigned char MemoryModel;
	unsigned char BankSize;
	unsigned char NumberOfImagePages;
	unsigned char Reserved;
	unsigned char RedMaskSize;
	unsigned char RedFieldPosition;
	unsigned char GreenMaskSize;
	unsigned char GreenFieldPosition;
	unsigned char BlueMaskSize;
	unsigned char BlueFieldPosition;
	unsigned char RsvdMaskSize;
	unsigned char RsvdFieldPosition;
	unsigned char DirectColorModeInfo;
//--------------------- since Version 2.0
	unsigned long PhysBasePtr;
	unsigned long OffScreenMemOffset;
	short OffScreenMemSize;
	unsigned char Reserved2[206];
} SVGAINFO_s;

typedef struct VESAPALETTEENTRY_s {
	unsigned char bRed;
	unsigned char bGreen;
	unsigned char bBlue;
	unsigned char bAlpha;
} VESAPALETTEENTRY_s;

#define VESAATTR_SUPPORTED	0x001	// supported by hardware
#define VESAATTR_OPT_INFO_AVAIL	0x002
#define VESAATTR_BIOS_OUTPUT	0x004
#define VESAATTR_IS_COLOR_MODE 	0x008
#define VESAATTR_IS_GFX_MODE 	0x010	// is a graphics mode
#define VESAATTR_NON_VGA_COMPAT	0x020
#define VESAATTR_NO_BANK_SWITCH	0x040
#define VESAATTR_LFB_SUPPORTED	0x080	// LFB supported
#define VESAATTR_DBLSCAN_SUPP	0x100

#define VESADOSMEM_SIZE sizeof(struct VESAINFO_s)
#define VESA_MAX_MODES 64		// ???
#define VESA_FRAME_BUFFERS 2	// store 2 frames (pictures) -> double buffer

typedef struct vesa_data_s {
	unsigned long vesa_mem_phys_base_ptr;
	unsigned long vesa_memory_size;

	dosmem_t dm_vesa;
	dosmem_t dm_statebuffer;
	unsigned int vesa_state_bufsize;
	unsigned int vesa_state_saved;

	unsigned short vesa_modes;
	unsigned short vesa_mode_list[VESA_MAX_MODES];
} vesa_data_s;

static unsigned int videoout_vesa_searchmode(struct mpxplay_videoout_info_s *voi, unsigned int xres, unsigned int yres, unsigned int bpp);
static unsigned int loc_vesa_init(struct vesa_data_s *vds);
static void loc_vesa_close(struct vesa_data_s *vds);
static unsigned int loc_vesa_check(struct vesa_data_s *vds, struct VESAINFO_s *vesainfos);
static unsigned int loc_vesa_querymode(struct vesa_data_s *vds, unsigned int mode, struct SVGAINFO_s *svgainfos);
static unsigned int loc_vesa_state_save(struct vesa_data_s *vds);
static unsigned int loc_vesa_state_restore(struct vesa_data_s *vds);

static unsigned int videoout_vesa_init(struct mpxplay_videoout_info_s *voi)
{
	unsigned int i;
	unsigned short *vesamodesptr;
	struct vesa_data_s *vds;
	struct VESAINFO_s vesainfos;
	struct SVGAINFO_s svgainfos;

	if(voi->screen_private_data)
		return 1;

	vds = (struct vesa_data_s *)calloc(1, sizeof(struct vesa_data_s));
	if(!vds)
		return 0;

	voi->screen_private_data = (void *)vds;

	memset((void *)&vesainfos, 0, sizeof(struct VESAINFO_s));

	vesainfos.VESASignature[0] = 'V';
	vesainfos.VESASignature[1] = 'B';
	vesainfos.VESASignature[2] = 'E';
	vesainfos.VESASignature[3] = '2';

#ifdef VESA_DEBUG
	fprintf(stdout, "vesa begin\n");
#endif

	if(!loc_vesa_init(vds))
		return 0;
	if(!loc_vesa_check(vds, &vesainfos))
		return 0;

	vesamodesptr = (unsigned short *)((((unsigned long)vesainfos.VideoModePtr_seg) << 4) + ((unsigned long)vesainfos.VideoModePtr_off));
	vds->vesa_memory_size = ((unsigned long)vesainfos.TotalMemory) << 16;

#ifdef VESA_DEBUG
	fprintf(stdout, "%c%c%c%c (version %4.4X) video modes:\n", vesainfos.VESASignature[0], vesainfos.VESASignature[1], vesainfos.VESASignature[2], vesainfos.VESASignature[3], vesainfos.VESAVersion);
#endif

	vds->vesa_modes = 0;
	while((*vesamodesptr != 0xffff) && (vds->vesa_modes < VESA_MAX_MODES))
		vds->vesa_mode_list[vds->vesa_modes++] = *vesamodesptr++;

	if(!vds->vesa_modes)
		return 0;

#ifdef VESA_DEBUG
	for(i = 0; i < vds->vesa_modes; i++)
		if(loc_vesa_querymode(vds, vds->vesa_mode_list[i], &svgainfos))
			fprintf(stdout, "%2d. %4.4Xh %4dx%4dx%2d attr:%8.8X pages:%2d phys_addr:%8.8X\n", i + 1, vds->vesa_mode_list[i], svgainfos.XResolution, svgainfos.YResolution, svgainfos.BitsPerPixel,
					svgainfos.ModeAttributes, (long)svgainfos.NumberOfImagePages, svgainfos.PhysBasePtr);
#endif

	// set resulution by manual configuration
	if(voi->config_res_x) {
		voi->config_mode = videoout_vesa_searchmode(voi, voi->config_res_x, 0, voi->config_bpp);
		if(voi->config_mode && loc_vesa_querymode(vds, voi->config_mode, &svgainfos)) {
			voi->screen_res_x = svgainfos.XResolution;
			voi->screen_res_y = svgainfos.YResolution;
			voi->screen_bpp = svgainfos.BitsPerPixel;
			voi->screen_size = voi->screen_res_x * voi->screen_res_y * (voi->screen_bpp / 8);
			vds->vesa_mem_phys_base_ptr = svgainfos.PhysBasePtr;
		} else
			voi->config_res_x = 0;
	}
	// search for the highest resolution in the current bpp
	if(!voi->config_res_x) {
		unsigned int highestx = 0;
		for(i = 0; i < vds->vesa_modes; i++) {
			if(loc_vesa_querymode(vds, vds->vesa_mode_list[i], &svgainfos)) {
				if((svgainfos.ModeAttributes & VESAATTR_IS_GFX_MODE) && (highestx < svgainfos.XResolution) && (svgainfos.BitsPerPixel == voi->config_bpp)
				   && (svgainfos.ModeAttributes & VESAATTR_LFB_SUPPORTED)) {
					highestx = svgainfos.XResolution;
					voi->config_mode = vds->vesa_mode_list[i];
					voi->screen_res_x = svgainfos.XResolution;
					voi->screen_res_y = svgainfos.YResolution;
					voi->screen_bpp = svgainfos.BitsPerPixel;
					voi->screen_size = voi->screen_res_x * voi->screen_res_y * (voi->screen_bpp / 8);
					vds->vesa_mem_phys_base_ptr = svgainfos.PhysBasePtr;
				}
			}
		}
	}

	if(!voi->screen_size)
		return 0;

	voi->screen_linear_ptr = (unsigned char *)pds_dpmi_map_physical_memory(vds->vesa_mem_phys_base_ptr, voi->screen_size * VESA_FRAME_BUFFERS);

#ifdef VESA_DEBUG
	fprintf(stdout, "\n");
	fprintf(stdout, "selected: %4.4Xh %dx%dx%d memsize:%dMb phys:%8.8X linear_addr:%8.8X\n",
			voi->config_mode, voi->screen_res_x, voi->screen_res_y, voi->screen_bpp, vds->vesa_memory_size / 1048576, vds->vesa_mem_phys_base_ptr, (unsigned long)voi->screen_linear_ptr);
#endif
	if(!voi->screen_linear_ptr)
		return 0;

	//pds_cpu_mtrr_enable_wc(vds->vesa_mem_phys_base_ptr,4096);

	return 1;
}

static void videoout_vesa_close(struct mpxplay_videoout_info_s *voi)
{
	struct vesa_data_s *vds = (struct vesa_data_s *)voi->screen_private_data;
	if(vds) {
		//pds_cpu_mtrr_disable_wc(vds->vesa_mem_phys_base_ptr);
		if(voi->screen_linear_ptr) {
			pds_dpmi_unmap_physycal_memory((unsigned long)voi->screen_linear_ptr);
			voi->screen_linear_ptr = NULL;
		}
		loc_vesa_close(vds);
		free(vds);
		voi->screen_private_data = NULL;
	}
}

static void videoout_vesa_listmodes(struct mpxplay_videoout_info_s *voi)
{
	unsigned int i, c;
	struct vesa_data_s *vds = (struct vesa_data_s *)voi->screen_private_data;
	struct SVGAINFO_s svgainfos;
	char sout[128];

	pds_textdisplay_printf("List of (VESA) video display modes supported in Mpxplay:");
	pds_textdisplay_printf(" ");
	for(i = 0, c = 1; i < vds->vesa_modes; i++) {
		if(loc_vesa_querymode(vds, vds->vesa_mode_list[i], &svgainfos)) {
			if(svgainfos.BitsPerPixel == voi->config_bpp) {
				sprintf(sout, "%2d. mode:%4.4Xh  resolution: %4d x%4d  bits:%2d ", c++, vds->vesa_mode_list[i], svgainfos.XResolution, svgainfos.YResolution, svgainfos.BitsPerPixel);
				if(vds->vesa_mode_list[i] == voi->config_mode)
					pds_strcat(sout, " (selected) ");
				pds_textdisplay_printf(sout);
			}
		}
	}
}

static unsigned int videoout_vesa_searchmode(struct mpxplay_videoout_info_s *voi, unsigned int xres, unsigned int yres, unsigned int bpp)
{
	unsigned int i;
	struct vesa_data_s *vds = (struct vesa_data_s *)voi->screen_private_data;
	struct SVGAINFO_s svgainfos;

	if(!vds)
		return 0;

	for(i = 0; i < vds->vesa_modes; i++) {
		if(loc_vesa_querymode(vds, vds->vesa_mode_list[i], &svgainfos)) {
			if(svgainfos.ModeAttributes & VESAATTR_LFB_SUPPORTED)	// we need this
				if((!xres || (svgainfos.XResolution == xres)) && (!yres || (svgainfos.YResolution == yres)) && (!bpp || (svgainfos.BitsPerPixel == bpp)))
					return vds->vesa_mode_list[i];
		}
	}
	return 0;
}

static unsigned int videoout_vesa_set(struct mpxplay_videoout_info_s *voi, unsigned int clearmem)
{
	struct vesa_data_s *vds = (struct vesa_data_s *)voi->screen_private_data;
	unsigned long pict_size;
	unsigned int mode;
	struct rminfo RMI;
	struct SVGAINFO_s svgainfos;

	if(!vds)
		return 0;

	//verify mode
	mode = voi->config_mode;
	if(!loc_vesa_querymode(vds, mode, &svgainfos))
		return 0;
	// we need LFB (linear frame buffer) in GFX (video) mode
	if((svgainfos.ModeAttributes & VESAATTR_IS_GFX_MODE) && !(svgainfos.ModeAttributes & VESAATTR_LFB_SUPPORTED))
		return 0;
	if(!svgainfos.PhysBasePtr)
		return 0;

	pict_size = (unsigned long)svgainfos.XResolution * (unsigned long)svgainfos.XResolution * ((unsigned long)svgainfos.BitsPerPixel / 8);

	if((svgainfos.PhysBasePtr != vds->vesa_mem_phys_base_ptr) || (pict_size != voi->screen_size)) {
		if(voi->screen_linear_ptr)
			pds_dpmi_unmap_physycal_memory((unsigned long)voi->screen_linear_ptr);
		voi->screen_linear_ptr = (unsigned char *)pds_dpmi_map_physical_memory(svgainfos.PhysBasePtr, pict_size * VESA_FRAME_BUFFERS);
		if(!voi->screen_linear_ptr)
			return 0;
		vds->vesa_mem_phys_base_ptr = svgainfos.PhysBasePtr;
		voi->screen_size = pict_size;
	}

	loc_vesa_state_save(vds);

	if(!clearmem)
		mode |= 0x8000;

	mode |= 0x4000;				// set LFB

	//set mode
	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x00004f02;
	RMI.EBX = mode;
	pds_dpmi_realmodeint_call(0x10, &RMI);

	if(RMI.EAX != 0x004f)
		return 0;

	voi->screen_size = pict_size;
	voi->screen_res_x = svgainfos.XResolution;
	voi->screen_res_y = svgainfos.YResolution;
	voi->screen_bpp = svgainfos.BitsPerPixel;

	return 1;
}

static unsigned int videoout_vesa_reset(struct mpxplay_videoout_info_s *voi)
{
	struct vesa_data_s *vds = (struct vesa_data_s *)voi->screen_private_data;

	if(!vds)
		return 0;

	if(loc_vesa_state_restore(vds))
		return 1;

	return 0;
}

mpxplay_videoout_func_s VESA_videoout_funcs = {
	"VESA",
	0,
	&videoout_vesa_init,
	&videoout_vesa_close,
	&videoout_vesa_listmodes,
	&videoout_vesa_searchmode,
	&videoout_vesa_set,
	&videoout_vesa_reset,
	NULL						//&videoout_vesa_write
};

//-------------------------------------------------------------------
// VESA bios callings

static unsigned int loc_vesa_init(struct vesa_data_s *vds)
{
	if(!vds->dm_vesa.linearptr) {
		if(!pds_dpmi_dos_allocmem(&vds->dm_vesa, VESADOSMEM_SIZE))
			return 0;
	}
	return 1;
}

static void loc_vesa_close(struct vesa_data_s *vds)
{
	if(vds) {
		pds_dpmi_dos_freemem(&vds->dm_vesa);
	}
}

static unsigned int loc_vesa_check(struct vesa_data_s *vds, struct VESAINFO_s *vesainfos)
{
	struct rminfo RMI;

	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x00004f00;
	RMI.ES = vds->dm_vesa.segment;
	memcpy(vds->dm_vesa.linearptr, vesainfos, sizeof(struct VESAINFO_s));
	pds_dpmi_realmodeint_call(0x10, &RMI);
	RMI.EAX &= 0xffff;
	if(RMI.EAX != 0x004f)
		return 0;
	memcpy(vesainfos, vds->dm_vesa.linearptr, sizeof(struct VESAINFO_s));
	if(vesainfos->VESAVersion < 0x200)
		return 0;
	return 1;
}

static unsigned int loc_vesa_querymode(struct vesa_data_s *vds, unsigned int mode, struct SVGAINFO_s *svgainfos)
{
	struct rminfo RMI;

	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x00004f01;
	RMI.ECX = mode;
	RMI.ES = vds->dm_vesa.segment;
	memset(vds->dm_vesa.linearptr, 0, VESADOSMEM_SIZE);
	pds_dpmi_realmodeint_call(0x10, &RMI);
	RMI.EAX &= 0xffff;
	if(RMI.EAX != 0x004f)
		return 0;
	memcpy(svgainfos, vds->dm_vesa.linearptr, sizeof(struct SVGAINFO_s));
	return 1;
}

static unsigned int loc_vesa_state_save(struct vesa_data_s *vds)
{
	unsigned int bufsize;
	struct rminfo RMI;

	if(vds->vesa_state_saved)
		return 1;

	//get buffer size
	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x00004f04;
	RMI.ECX = 0x0f;				// save all
	RMI.EDX = 0x00;
	pds_dpmi_realmodeint_call(0x10, &RMI);

	if((RMI.EAX != 0x004f) || !RMI.EBX)
		return 0;

	bufsize = RMI.EBX * 64;

	if(vds->vesa_state_bufsize < bufsize) {
		if(!pds_dpmi_dos_allocmem(&vds->dm_statebuffer, bufsize))
			return 0;
		vds->vesa_state_bufsize = bufsize;
	}
	// save state
	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x00004f04;
	RMI.ECX = 0x0f;				// save all
	RMI.EDX = 0x01;
	RMI.ES = vds->dm_statebuffer.segment;
	pds_dpmi_realmodeint_call(0x10, &RMI);

	if(RMI.EAX != 0x004f)
		return 0;

	vds->vesa_state_saved = 1;

	return 1;
}

static unsigned int loc_vesa_state_restore(struct vesa_data_s *vds)
{
	struct rminfo RMI;

	if(!vds->vesa_state_saved || !vds->dm_statebuffer.segment)
		return 0;

	vds->vesa_state_saved = 0;

	//restore state
	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x00004f04;
	RMI.ECX = 0x0f;				// restore all
	RMI.EDX = 0x02;
	RMI.ES = vds->dm_statebuffer.segment;
	pds_dpmi_realmodeint_call(0x10, &RMI);

	if(RMI.EAX != 0x004f)
		return 0;

	return 1;
}

#else							//

mpxplay_videoout_func_s VESA_videoout_funcs = {
	"VESA",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

#endif							// __DOS__
