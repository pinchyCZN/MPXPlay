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
//function: Intel HD audio driver for Mpxplay
//based on ALSA (http://www.alsa-project.org)

#define MPXPLAY_USE_DEBUGF 1
#define IHD_DEBUG_OUTPUT stdout

#include "au_cards.h"

#ifdef AU_CARDS_LINK_IHD

#include <newfunc\newfunc.h>
#include <string.h>
#include "dmairq.h"
#include "pcibios.h"
#include "sc_inthd.h"

#define INTHD_MAX_CHANNELS 8
#define AZX_PERIOD_SIZE 4096

struct intelhd_card_s {
	unsigned long iobase;
	struct pci_config_s *pci_dev;
	unsigned int board_driver_type;
	long codec_vendor_id;
	unsigned long codec_mask;
	unsigned int codec_index;
	hda_nid_t afg_root_nodenum;
	int afg_num_nodes;
	struct hda_gnode *afg_nodes;
	unsigned int def_amp_out_caps;
	unsigned int def_amp_in_caps;
	struct hda_gnode *dac_node[2];	// DAC nodes
	struct hda_gnode *out_pin_node[2];	// Output pin (Line-Out) nodes
	unsigned int pcm_num_vols;	// number of PCM volumes
	struct pcm_vol_s pcm_vols[MAX_PCM_VOLS];	// PCM volume nodes

	dosmem_t *dm;
	uint32_t *table_buffer;
	char *pcmout_buffer;
	long pcmout_bufsize;
	unsigned long pcmout_dmasize;
	unsigned int pcmout_num_periods;
	unsigned long pcmout_period_size;
	unsigned long sd_addr;		// stream io address (one playback stream only)
	unsigned int format_val;	// stream type
	unsigned int dacout_num_bits;
	unsigned long supported_formats;
	unsigned long supported_max_freq;
	unsigned int supported_max_bits;
};

struct codec_vendor_list_s {
	unsigned short vendor_id;
	char *vendor_name;
};

struct hda_rate_tbl {
	unsigned int hz;
	unsigned int hda_fmt;
};

static struct hda_rate_tbl rate_bits[] = {
	{8000, 0x0500},				// 1/6 x 48
	{11025, 0x4300},			// 1/4 x 44
	{16000, 0x0200},			// 1/3 x 48
	{22050, 0x4100},			// 1/2 x 44
	{32000, 0x0a00},			// 2/3 x 48
	{44100, 0x4000},			// 44
	{48000, 0x0000},			// 48
	{88200, 0x4800},			// 2 x 44
	{96000, 0x0800},			// 2 x 48
	{176400, 0x5800},			// 4 x 44
	{192000, 0x1800},			// 4 x 48
	{0xffffffff, 0x1800},		// 192000
	{0, 0}
};

static aucards_onemixerchan_s ihd_master_vol = {
	AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER, AU_MIXCHANFUNC_VOLUME), MAX_PCM_VOLS, {
																				   {0, 0x00, 0, 0},	// card->pcm_vols[0]
																				   {0, 0x00, 0, 0},	// card->pcm_vols[1]
																				   }
};

//Intel HDA codec has memory mapping only (by specification)

#define azx_writel(chip,reg,value) PDS_PUTB_LE32((char *)((chip)->iobase + ICH6_REG_##reg),value)
#define azx_readl(chip,reg) PDS_GETB_LE32((char *)((chip)->iobase + ICH6_REG_##reg))
#define azx_writew(chip,reg,value) PDS_PUTB_LE16((char *)((chip)->iobase + ICH6_REG_##reg), value)
#define azx_readw(chip,reg) PDS_GETB_LE16((char *)((chip)->iobase + ICH6_REG_##reg))
#define azx_writeb(chip,reg,value) *((unsigned char *)((chip)->iobase + ICH6_REG_##reg))=value
#define azx_readb(chip,reg) PDS_GETB_8U((char *)((chip)->iobase + ICH6_REG_##reg))

#define azx_sd_writel(dev,reg,value) PDS_PUTB_LE32((char *)((dev)->sd_addr + ICH6_REG_##reg),value)
#define azx_sd_readl(dev,reg) PDS_GETB_LE32((char *)((dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_writew(dev,reg,value) PDS_PUTB_LE16((char*)((dev)->sd_addr + ICH6_REG_##reg),value)
#define azx_sd_readw(dev,reg) PDS_GETB_LE16((char *)((dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_writeb(dev,reg,value) *((unsigned char *)((dev)->sd_addr + ICH6_REG_##reg))=value
#define azx_sd_readb(dev,reg) PDS_GETB_8U((char *)((dev)->sd_addr + ICH6_REG_##reg))

//-------------------------------------------------------------------------
static void update_pci_byte(pci_config_s * pci, unsigned int reg, unsigned char mask, unsigned char val)
{
	unsigned char data;

	data = pcibios_ReadConfig_Byte(pci, reg);
	data &= ~mask;
	data |= (val & mask);
	pcibios_WriteConfig_Byte(pci, reg, data);
}

static void azx_init_pci(struct intelhd_card_s *card)
{
	unsigned int snoop;
	update_pci_byte(card->pci_dev, ICH6_PCIREG_TCSEL, 0x07, 0);

	switch (card->board_driver_type) {
	case AZX_DRIVER_ATI:
		update_pci_byte(card->pci_dev, ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR, 0x07, ATI_SB450_HDAUDIO_ENABLE_SNOOP);
		break;
	case AZX_DRIVER_NVIDIA:
		update_pci_byte(card->pci_dev, NVIDIA_HDA_TRANSREG_ADDR, 0x0f, NVIDIA_HDA_ENABLE_COHBITS);
		update_pci_byte(card->pci_dev, NVIDIA_HDA_ISTRM_COH, 0x01, NVIDIA_HDA_ENABLE_COHBIT);
		update_pci_byte(card->pci_dev, NVIDIA_HDA_OSTRM_COH, 0x01, NVIDIA_HDA_ENABLE_COHBIT);
		break;
	case AZX_DRIVER_SCH:
	case AZX_DRIVER_PCH:
		snoop = pcibios_ReadConfig_Word(card->pci_dev, INTEL_SCH_HDA_DEVC);
		if(snoop & INTEL_SCH_HDA_DEVC_NOSNOOP)
			pcibios_WriteConfig_Word(card->pci_dev, INTEL_SCH_HDA_DEVC, snoop & (~INTEL_SCH_HDA_DEVC_NOSNOOP));
		break;
	}
}

//-------------------------------------------------------------------------
static int azx_single_send_cmd(struct intelhd_card_s *chip, uint32_t val)
{
	int timeout = 50;

	while(timeout--) {
		if(!((azx_readw(chip, IRS) & ICH6_IRS_BUSY))) {
			azx_writew(chip, IRS, azx_readw(chip, IRS) | ICH6_IRS_VALID);
			azx_writel(chip, IC, val);
			azx_writew(chip, IRS, azx_readw(chip, IRS) | ICH6_IRS_BUSY);
			return 0;
		}
		pds_delay_10us(1);
	}
	mpxplay_debugf(IHD_DEBUG_OUTPUT, "send cmd timeout");
	return -1;
}

static int snd_hda_codec_write(struct intelhd_card_s *chip, hda_nid_t nid, uint32_t direct, unsigned int verb, unsigned int parm)
{
	uint32_t val;

	val = (uint32_t) (chip->codec_index & 0x0f) << 28;
	val |= (uint32_t) direct << 27;
	val |= (uint32_t) nid << 20;
	val |= verb << 8;
	val |= parm;

	return azx_single_send_cmd(chip, val);
}

static unsigned int azx_get_response(struct intelhd_card_s *chip)
{
	int timeout = 50;

	while(timeout--) {
		uint16_t irs = azx_readw(chip, IRS);
		if((irs & ICH6_IRS_VALID) && !(irs & ICH6_IRS_BUSY))
			return azx_readl(chip, IR);
		pds_delay_10us(1);
	}
	return 0xffffffff;
}

static unsigned int snd_hda_codec_read(struct intelhd_card_s *chip, hda_nid_t nid, uint32_t direct, unsigned int verb, unsigned int parm)
{
	if(snd_hda_codec_write(chip, nid, direct, verb, parm) == 0)
		return azx_get_response(chip);
	return 0xffffffff;
}

#define snd_hda_param_read(codec,nid,param) snd_hda_codec_read(codec,nid,0,AC_VERB_PARAMETERS,param)

static void snd_hda_codec_setup_stream(struct intelhd_card_s *chip, hda_nid_t nid, uint32_t stream_tag, int channel_id, int format)
{
	snd_hda_codec_write(chip, nid, 0, AC_VERB_SET_CHANNEL_STREAMID, (stream_tag << 4) | channel_id);
	pds_delay_10us(150);		// was 100
	snd_hda_codec_write(chip, nid, 0, AC_VERB_SET_STREAM_FORMAT, format);
	pds_delay_10us(100);
}

//------------------------------------------------------------------------
static unsigned int snd_hda_get_sub_nodes(struct intelhd_card_s *card, hda_nid_t nid, hda_nid_t * start_id)
{
	int parm;

	parm = snd_hda_param_read(card, nid, AC_PAR_NODE_COUNT);
	if(parm < 0)
		return 0;
	*start_id = (parm >> 16) & 0x7fff;
	return (parm & 0x7fff);
}

static void snd_hda_search_audio_node(struct intelhd_card_s *card)
{
	int i, total_nodes;
	hda_nid_t nid;

	total_nodes = snd_hda_get_sub_nodes(card, AC_NODE_ROOT, &nid);
	for(i = 0; i < total_nodes; i++, nid++) {
		if((snd_hda_param_read(card, nid, AC_PAR_FUNCTION_TYPE) & 0xff) == AC_GRP_AUDIO_FUNCTION) {
			card->afg_root_nodenum = nid;
			break;
		}
	}

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "total_nodes:%d afg_nodenum:%d", total_nodes, (int)card->afg_root_nodenum);
}

static int snd_hda_get_connections(struct intelhd_card_s *card, hda_nid_t nid, hda_nid_t * conn_list, int max_conns)
{
	unsigned int parm;
	int i, conn_len, conns;
	unsigned int shift, num_elems, mask;
	hda_nid_t prev_nid;

	parm = snd_hda_param_read(card, nid, AC_PAR_CONNLIST_LEN);
	if(parm & AC_CLIST_LONG) {
		shift = 16;
		num_elems = 2;
	} else {
		shift = 8;
		num_elems = 4;
	}

	conn_len = parm & AC_CLIST_LENGTH;
	if(!conn_len)
		return 0;

	mask = (1 << (shift - 1)) - 1;

	if(conn_len == 1) {
		parm = snd_hda_codec_read(card, nid, 0, AC_VERB_GET_CONNECT_LIST, 0);
		conn_list[0] = parm & mask;
		return 1;
	}

	conns = 0;
	prev_nid = 0;
	for(i = 0; i < conn_len; i++) {
		int range_val;
		hda_nid_t val, n;

		if(i % num_elems == 0)
			parm = snd_hda_codec_read(card, nid, 0, AC_VERB_GET_CONNECT_LIST, i);

		range_val = !!(parm & (1 << (shift - 1)));
		val = parm & mask;
		parm >>= shift;
		if(range_val) {
			if(!prev_nid || prev_nid >= val)
				continue;
			for(n = prev_nid + 1; n <= val; n++) {
				if(conns >= max_conns)
					return -1;
				conn_list[conns++] = n;
			}
		} else {
			if(conns >= max_conns)
				return -1;
			conn_list[conns++] = val;
		}
		prev_nid = val;
	}
	return conns;
}

static int snd_hda_add_new_node(struct intelhd_card_s *card, struct hda_gnode *node, hda_nid_t nid)
{
	int nconns;

	node->nid = nid;
	nconns = snd_hda_get_connections(card, nid, &node->conn_list[0], HDA_MAX_CONNECTIONS);
	if(nconns >= 0) {
		node->nconns = nconns;
		node->wid_caps = snd_hda_param_read(card, nid, AC_PAR_AUDIO_WIDGET_CAP);
		node->type = (node->wid_caps & AC_WCAP_TYPE) >> AC_WCAP_TYPE_SHIFT;

		if(node->type == AC_WID_PIN) {
			node->pin_caps = snd_hda_param_read(card, node->nid, AC_PAR_PIN_CAP);
			node->pin_ctl = snd_hda_codec_read(card, node->nid, 0, AC_VERB_GET_PIN_WIDGET_CONTROL, 0);
			node->def_cfg = snd_hda_codec_read(card, node->nid, 0, AC_VERB_GET_CONFIG_DEFAULT, 0);
		}

		if(node->wid_caps & AC_WCAP_OUT_AMP) {
			if(node->wid_caps & AC_WCAP_AMP_OVRD)
				node->amp_out_caps = snd_hda_param_read(card, node->nid, AC_PAR_AMP_OUT_CAP);
			if(!node->amp_out_caps)
				node->amp_out_caps = card->def_amp_out_caps;
		}
		if(node->wid_caps & AC_WCAP_IN_AMP) {
			if(node->wid_caps & AC_WCAP_AMP_OVRD)
				node->amp_in_caps = snd_hda_param_read(card, node->nid, AC_PAR_AMP_IN_CAP);
			if(!node->amp_in_caps)
				node->amp_in_caps = card->def_amp_in_caps;
		}
		node->supported_formats = snd_hda_param_read(card, node->nid, AC_PAR_PCM);
	}

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "node:%2d cons:%2d wc:%8.8X t:%2d aoc:%8.8X ot:%2d sf:%8.8X st:%2d of:%2d",
				   (int)nid, nconns, node->wid_caps,
				   node->type, node->amp_out_caps, (node->wid_caps & AC_WCAP_OUT_AMP), node->supported_formats, ((node->amp_out_caps >> 8) & 0x7f), (node->amp_out_caps & AC_AMPCAP_OFFSET));

	return nconns;
}

static struct hda_gnode *hda_get_node(struct intelhd_card_s *card, hda_nid_t nid)
{
	struct hda_gnode *node = card->afg_nodes;
	unsigned int i;

	for(i = 0; i < card->afg_num_nodes; i++, node++)
		if(node->nid == nid)
			return node;

	return NULL;
}

static void snd_hda_put_vol_mute(struct intelhd_card_s *card, hda_nid_t nid, int ch, int direction, int index, int val)
{
	uint32_t parm;

	parm = (ch) ? AC_AMP_SET_RIGHT : AC_AMP_SET_LEFT;
	parm |= (direction == HDA_OUTPUT) ? AC_AMP_SET_OUTPUT : AC_AMP_SET_INPUT;
	parm |= index << AC_AMP_SET_INDEX_SHIFT;
	parm |= val;
	snd_hda_codec_write(card, nid, 0, AC_VERB_SET_AMP_GAIN_MUTE, parm);
}

static unsigned int snd_hda_get_vol_mute(struct intelhd_card_s *card, hda_nid_t nid, int ch, int direction, int index)
{
	uint32_t val, parm;

	parm = (ch) ? AC_AMP_GET_RIGHT : AC_AMP_GET_LEFT;
	parm |= (direction == HDA_OUTPUT) ? AC_AMP_GET_OUTPUT : AC_AMP_GET_INPUT;
	parm |= index;
	val = snd_hda_codec_read(card, nid, 0, AC_VERB_GET_AMP_GAIN_MUTE, parm);
	return (val & 0xff);
}

static int snd_hda_codec_amp_update(struct intelhd_card_s *card, hda_nid_t nid, int ch, int direction, int idx, int mask, int val)
{
	val &= mask;
	val |= snd_hda_get_vol_mute(card, nid, ch, direction, idx) & ~mask;
	snd_hda_put_vol_mute(card, nid, ch, direction, idx, val);
	return 1;
}

static int snd_hda_codec_amp_stereo(struct intelhd_card_s *card, hda_nid_t nid, int direction, int idx, int mask, int val)
{
	int ch, ret = 0;
	for(ch = 0; ch < 2; ch++)
		ret |= snd_hda_codec_amp_update(card, nid, ch, direction, idx, mask, val);
	return ret;
}

static void snd_hda_unmute_output(struct intelhd_card_s *card, struct hda_gnode *node)
{
	unsigned int val = (node->amp_out_caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
	snd_hda_codec_amp_stereo(card, node->nid, HDA_OUTPUT, 0, 0xff, val);
}

static void snd_hda_unmute_input(struct intelhd_card_s *card, struct hda_gnode *node, unsigned int index)
{
	unsigned int val = (node->amp_in_caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
	snd_hda_codec_amp_stereo(card, node->nid, HDA_INPUT, index, 0xff, val);
}

static int select_input_connection(struct intelhd_card_s *card, struct hda_gnode *node, unsigned int index)
{
	return snd_hda_codec_write(card, node->nid, 0, AC_VERB_SET_CONNECT_SEL, index);
}

static void clear_check_flags(struct intelhd_card_s *card)
{
	struct hda_gnode *node = card->afg_nodes;
	unsigned int i;

	for(i = 0; i < card->afg_num_nodes; i++, node++)
		node->checked = 0;
}

static int parse_output_path(struct intelhd_card_s *card, struct hda_gnode *node, int dac_idx)
{
	int i, err;
	struct hda_gnode *child;

	if(node->checked)
		return 0;

	node->checked = 1;
	if(node->type == AC_WID_AUD_OUT) {
		if(node->wid_caps & AC_WCAP_DIGITAL)
			return 0;
		if(card->dac_node[dac_idx])
			return node == card->dac_node[dac_idx];

		card->dac_node[dac_idx] = node;
		if((node->wid_caps & AC_WCAP_OUT_AMP) && (card->pcm_num_vols < MAX_PCM_VOLS)) {
			card->pcm_vols[card->pcm_num_vols].node = node;
			card->pcm_vols[card->pcm_num_vols].index = 0;
			card->pcm_num_vols++;
		}
		return 1;
	}

	for(i = 0; i < node->nconns; i++) {
		child = hda_get_node(card, node->conn_list[i]);
		if(!child)
			continue;
		err = parse_output_path(card, child, dac_idx);
		if(err < 0)
			return err;
		else if(err > 0) {
			if(node->nconns > 1)
				select_input_connection(card, node, i);
			snd_hda_unmute_input(card, node, i);
			snd_hda_unmute_output(card, node);
			if(card->dac_node[dac_idx] && (card->pcm_num_vols < MAX_PCM_VOLS) && !(card->dac_node[dac_idx]->wid_caps & AC_WCAP_OUT_AMP)) {
				if((node->wid_caps & AC_WCAP_IN_AMP) || (node->wid_caps & AC_WCAP_OUT_AMP)) {
					int n = card->pcm_num_vols;
					card->pcm_vols[n].node = node;
					card->pcm_vols[n].index = i;
					card->pcm_num_vols++;
				}
			}
			return 1;
		}
	}
	return 0;
}

static struct hda_gnode *parse_output_jack(struct intelhd_card_s *card, int jack_type)
{
	struct hda_gnode *node = card->afg_nodes;
	int err, i;

	for(i = 0; i < card->afg_num_nodes; i++, node++) {
		if(node->type != AC_WID_PIN)
			continue;
		if(!(node->pin_caps & AC_PINCAP_OUT))
			continue;
		if(defcfg_port_conn(node) == AC_JACK_PORT_NONE)
			continue;
		if(jack_type >= 0) {
			if(jack_type != defcfg_type(node))
				continue;
			if(node->wid_caps & AC_WCAP_DIGITAL)
				continue;
		} else {
			if(!(node->pin_ctl & AC_PINCTL_OUT_EN))
				continue;
		}
		clear_check_flags(card);
		err = parse_output_path(card, node, 0);
		if(err < 0)
			return NULL;
		if(!err && card->out_pin_node[0]) {
			err = parse_output_path(card, node, 1);
			if(err < 0)
				return NULL;
		}
		if(err > 0) {
			snd_hda_unmute_output(card, node);
			snd_hda_codec_write(card, node->nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL, AC_PINCTL_OUT_EN | ((node->pin_caps & AC_PINCAP_HP_DRV) ? AC_PINCTL_HP_EN : 0));
			return node;
		}
	}
	return NULL;
}

static int snd_hda_parse_output(struct intelhd_card_s *card)
{
	struct hda_gnode *node;

	node = parse_output_jack(card, AC_JACK_LINE_OUT);
	if(node)
		card->out_pin_node[0] = node;
	else {
		node = parse_output_jack(card, AC_JACK_SPEAKER);
		if(node)
			card->out_pin_node[0] = node;
	}

	node = parse_output_jack(card, AC_JACK_HP_OUT);
	if(node) {
		if(!card->out_pin_node[0])
			card->out_pin_node[0] = node;
		else
			card->out_pin_node[1] = node;
	}
	if(!card->out_pin_node[0]) {
		card->out_pin_node[0] = parse_output_jack(card, -1);	// parse 1st output
		if(!card->out_pin_node[0])
			return 0;
	}

	return 1;
}

//------------------------------------------------------------------------

static unsigned int azx_reset(struct intelhd_card_s *chip)
{
	int count;

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "azx_reset start");
	mpxplay_debugf(IHD_DEBUG_OUTPUT, "gctl1:%8.8X", azx_readl(chip, GCTL));

	azx_writeb(chip, STATESTS, STATESTS_INT_MASK);
	azx_writel(chip, GCTL, azx_readl(chip, GCTL) & ~ICH6_GCTL_RESET);

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "gctl2b:%8.8X gctl2d:%8.8X", (unsigned long)azx_readb(chip, GCTL), azx_readl(chip, GCTL));

	count = 50;
	while(azx_readb(chip, GCTL) && (--count))
		pds_delay_10us(100);

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "gctl3:%8.8X count:%d", azx_readl(chip, GCTL), count);

	pds_delay_10us(300);

	azx_writeb(chip, GCTL, azx_readb(chip, GCTL) | ICH6_GCTL_RESET);

	count = 50;
	while(!azx_readb(chip, GCTL) && --count)
		pds_delay_10us(100);

	pds_delay_10us(300);

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "gctl4:%8.8X count:%d ", (unsigned long)azx_readb(chip, GCTL), count);

	if(!azx_readb(chip, GCTL))
		return 0;

	// !!! disable unsolicited responses
	azx_writel(chip, GCTL, (azx_readl(chip, GCTL) & (~ICH6_GCTL_UREN)));
	//azx_writel(chip, GCTL, azx_readl(chip, GCTL) | ICH6_GCTL_UREN); // was enabled in ALSA

	pds_delay_10us(100);

	chip->codec_mask = azx_readw(chip, STATESTS);

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "codec_mask:%8.8X", chip->codec_mask);
	mpxplay_debugf(IHD_DEBUG_OUTPUT, "azx_reset end");

	return 1;
}

//-------------------------------------------------------------------------
static unsigned long snd_hda_get_max_freq(struct intelhd_card_s *card)
{
	unsigned long i, freq = 0;
	for(i = 0; rate_bits[i].hz; i++)
		if(card->supported_formats & (1 << i) && (rate_bits[i].hz < 0xffffffff))
			freq = rate_bits[i].hz;
	return freq;
}

static unsigned int snd_hda_get_max_bits(struct intelhd_card_s *card)
{
	unsigned int bits = 16;
	if(card->supported_formats & AC_SUPPCM_BITS_32)
		bits = 32;
	else if(card->supported_formats & AC_SUPPCM_BITS_24)
		bits = 24;
	else if(card->supported_formats & AC_SUPPCM_BITS_20)
		bits = 20;
	return bits;
}

//-------------------------------------------------------------------------
// init & close
static unsigned int snd_ihd_buffer_init(struct mpxplay_audioout_info_s *aui, struct intelhd_card_s *card)
{
	unsigned int bytes_per_sample = (aui->bits_set > 16) ? 4 : 2;
	unsigned long allbufsize = BDL_SIZE + 1024;
	unsigned int beginmem_aligned;

	allbufsize += card->pcmout_bufsize = MDma_get_max_pcmoutbufsize(aui, 0, AZX_PERIOD_SIZE, bytes_per_sample * aui->chan_card / 2, aui->freq_set);
	card->dm = MDma_alloc_dosmem(allbufsize);
	if(!card->dm)
		return 0;

	beginmem_aligned = (((unsigned long)card->dm->linearptr + 1023) & (~1023));
	card->table_buffer = (uint32_t *) beginmem_aligned;
	card->pcmout_buffer = (char *)(beginmem_aligned + BDL_SIZE);

	card->sd_addr = card->iobase + 0x100;	// !!! we use 1 playback stream only
	card->pcmout_period_size = AZX_PERIOD_SIZE;
	card->pcmout_num_periods = card->pcmout_bufsize / card->pcmout_period_size;

	return 1;
}

static void snd_ihd_hw_init(struct intelhd_card_s *card)
{
	azx_init_pci(card);
	azx_reset(card);

	azx_sd_writeb(card, SD_STS, SD_INT_MASK);
	azx_writeb(card, STATESTS, STATESTS_INT_MASK);
	azx_writeb(card, RIRBSTS, RIRB_INT_MASK);
	azx_writel(card, INTSTS, ICH6_INT_CTRL_EN | ICH6_INT_ALL_STREAM);
	azx_writel(card, INTCTL, azx_readl(card, INTCTL) | ICH6_INT_CTRL_EN | ICH6_INT_GLOBAL_EN);

	azx_writel(card, DPLBASE, 0);
	azx_writel(card, DPUBASE, 0);
}

static unsigned int snd_ihd_mixer_init(struct intelhd_card_s *card)
{
	unsigned int i;
	hda_nid_t nid;

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "snd_ihd_mixer_init start");

	card->codec_vendor_id = snd_hda_param_read(card, AC_NODE_ROOT, AC_PAR_VENDOR_ID);
	if(card->codec_vendor_id <= 0)
		card->codec_vendor_id = snd_hda_param_read(card, AC_NODE_ROOT, AC_PAR_VENDOR_ID);
	mpxplay_debugf(IHD_DEBUG_OUTPUT, "codec vendor id:%8.8X", card->codec_vendor_id);

	snd_hda_search_audio_node(card);
	if(!card->afg_root_nodenum)
		goto err_out_mixinit;

	card->def_amp_out_caps = snd_hda_param_read(card, card->afg_root_nodenum, AC_PAR_AMP_OUT_CAP);
	card->def_amp_in_caps = snd_hda_param_read(card, card->afg_root_nodenum, AC_PAR_AMP_IN_CAP);
	card->afg_num_nodes = snd_hda_get_sub_nodes(card, card->afg_root_nodenum, &nid);
	if((card->afg_num_nodes <= 0) || !nid)
		goto err_out_mixinit;

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "outcaps:%8.8X incaps:%8.8X afgsubnodes:%d anid:%d", card->def_amp_out_caps, card->def_amp_in_caps, card->afg_num_nodes, (int)nid);

	card->afg_nodes = (struct hda_gnode *)calloc(card->afg_num_nodes, sizeof(struct hda_gnode));
	if(!card->afg_nodes)
		goto err_out_mixinit;

	for(i = 0; i < card->afg_num_nodes; i++, nid++)
		snd_hda_add_new_node(card, &card->afg_nodes[i], nid);

	if(!snd_hda_parse_output(card))
		goto err_out_mixinit;

	if(card->dac_node[0]) {
		card->supported_formats = card->dac_node[0]->supported_formats;
		if(!card->supported_formats)
			card->supported_formats = 0xffffffff;	// !!! then manual try
		card->supported_max_freq = snd_hda_get_max_freq(card);
		card->supported_max_bits = snd_hda_get_max_bits(card);
	}

	for(i = 0; i < MAX_PCM_VOLS; i++)
		if(card->pcm_vols[i].node) {
			ihd_master_vol.submixerchans[i].submixch_register = card->pcm_vols[i].node->nid;
			ihd_master_vol.submixerchans[i].submixch_max = (card->pcm_vols[i].node->amp_out_caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
		}

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "dac0:%d dac1:%d out0:%d out1:%d vol0:%d vol1:%d",
				   (int)((card->dac_node[0]) ? card->dac_node[0]->nid : 0),
				   (int)((card->dac_node[1]) ? card->dac_node[1]->nid : 0),
				   (int)((card->out_pin_node[0]) ? card->out_pin_node[0]->nid : 0),
				   (int)((card->out_pin_node[1]) ? card->out_pin_node[1]->nid : 0),
				   (int)((card->pcm_vols[0].node) ? card->pcm_vols[0].node->nid : 0), (int)((card->pcm_vols[1].node) ? card->pcm_vols[1].node->nid : 0));

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "snd_ihd_mixer_init end with success");

	return 1;

  err_out_mixinit:
	if(card->afg_nodes) {
		free(card->afg_nodes);
		card->afg_nodes = NULL;
	}
	mpxplay_debugf(IHD_DEBUG_OUTPUT, "snd_ihd_mixer_init failed");
	return 0;
}

static void snd_ihd_hw_close(struct intelhd_card_s *card)
{
	azx_writel(card, DPLBASE, 0);
	azx_writel(card, DPUBASE, 0);
	azx_sd_writel(card, SD_BDLPL, 0);
	azx_sd_writel(card, SD_BDLPU, 0);
	azx_sd_writel(card, SD_CTL, 0);
}

static void azx_setup_periods(struct intelhd_card_s *card)
{
	uint32_t *bdl = card->table_buffer;
	unsigned int i;

	card->pcmout_num_periods = card->pcmout_dmasize / card->pcmout_period_size;

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "setup_periods: dmasize:%d periods:%d prsize:%d", card->pcmout_dmasize, card->pcmout_num_periods, card->pcmout_period_size);

	azx_sd_writel(card, SD_BDLPL, 0);
	azx_sd_writel(card, SD_BDLPU, 0);

	for(i = 0; i < card->pcmout_num_periods; i++) {
		unsigned int off = i << 2;
		unsigned int addr = ((unsigned int)card->pcmout_buffer) + i * card->pcmout_period_size;
		PDS_PUTB_LE32(&bdl[off], (uint32_t) addr);
		PDS_PUTB_LE32(&bdl[off + 1], 0);
		PDS_PUTB_LE32(&bdl[off + 2], card->pcmout_period_size);
		PDS_PUTB_LE32(&bdl[off + 3], 0x00);	// 0x01 enable interrupt
	}
}

static void azx_setup_controller(struct intelhd_card_s *card)
{
	unsigned char val;
	unsigned int stream_tag = 1;
	int timeout;

	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) & ~SD_CTL_DMA_START);
	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_STREAM_RESET);
	pds_delay_10us(100);

	timeout = 300;
	while(!((val = azx_sd_readb(card, SD_CTL)) & SD_CTL_STREAM_RESET) && --timeout)
		pds_delay_10us(1);

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "controller timeout1:%d ", timeout);

	val &= ~SD_CTL_STREAM_RESET;
	azx_sd_writeb(card, SD_CTL, val);
	pds_delay_10us(100);

	timeout = 300;
	while(((val = azx_sd_readb(card, SD_CTL)) & SD_CTL_STREAM_RESET) && --timeout)
		pds_delay_10us(1);

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "timeout2:%d format:%8.8X", timeout, (int)card->format_val);

	azx_sd_writel(card, SD_CTL, (azx_sd_readl(card, SD_CTL) & ~SD_CTL_STREAM_TAG_MASK) | (stream_tag << SD_CTL_STREAM_TAG_SHIFT));
	azx_sd_writel(card, SD_CBL, card->pcmout_dmasize);
	azx_sd_writew(card, SD_LVI, card->pcmout_num_periods - 1);
	azx_sd_writew(card, SD_FORMAT, card->format_val);
	azx_sd_writel(card, SD_BDLPL, (uint32_t) card->table_buffer);
	azx_sd_writel(card, SD_BDLPU, 0);	// upper 32 bit
	//azx_sd_writel(card, SD_CTL,azx_sd_readl(card, SD_CTL) | SD_INT_MASK);
	pds_delay_10us(100);

	if(card->dac_node[0])
		snd_hda_codec_setup_stream(card, card->dac_node[0]->nid, stream_tag, 0, card->format_val);
	if(card->dac_node[1])
		snd_hda_codec_setup_stream(card, card->dac_node[1]->nid, stream_tag, 0, card->format_val);
}

static unsigned int snd_hda_calc_stream_format(struct mpxplay_audioout_info_s *aui, struct intelhd_card_s *card)
{
	unsigned int i, val = 0;

	if((aui->freq_card < 44100) && !aui->freq_set)	// under 44100 it sounds terrible on my ALC888, rather we use the freq converter of Mpxplay
		aui->freq_card = 44100;
	else if(card->supported_max_freq && (aui->freq_card > card->supported_max_freq))
		aui->freq_card = card->supported_max_freq;

	for(i = 0; rate_bits[i].hz; i++)
		if((aui->freq_card <= rate_bits[i].hz) && (card->supported_formats & (1 << i))) {
			aui->freq_card = rate_bits[i].hz;
			val = rate_bits[i].hda_fmt;
			break;
		}

	val |= aui->chan_card - 1;

	if(card->dacout_num_bits > card->supported_max_bits)
		card->dacout_num_bits = card->supported_max_bits;

	if((card->dacout_num_bits <= 16) && (card->supported_formats & AC_SUPPCM_BITS_16)) {
		val |= 0x10;
		card->dacout_num_bits = 16;
		aui->bits_card = 16;
	} else if((card->dacout_num_bits <= 20) && (card->supported_formats & AC_SUPPCM_BITS_20)) {
		val |= 0x20;
		card->dacout_num_bits = 20;
		aui->bits_card = 32;
	} else if((card->dacout_num_bits <= 24) && (card->supported_formats & AC_SUPPCM_BITS_24)) {
		val |= 0x30;
		card->dacout_num_bits = 24;
		aui->bits_card = 32;
	} else if((card->dacout_num_bits <= 32) && (card->supported_formats & AC_SUPPCM_BITS_32)) {
		val |= 0x40;
		card->dacout_num_bits = 32;
		aui->bits_card = 32;
	}

	return val;
}

//-------------------------------------------------------------------------
static pci_device_s intelhda_devices[] = {
	{"Intel ICH6", 0x8086, 0x2668, AZX_DRIVER_ICH},
	{"Intel ICH7", 0x8086, 0x27d8, AZX_DRIVER_ICH},
	{"Intel ESB2", 0x8086, 0x269a, AZX_DRIVER_ICH},
	{"Intel ICH8", 0x8086, 0x284b, AZX_DRIVER_ICH},
	{"Intel ICH", 0x8086, 0x2911, AZX_DRIVER_ICH},
	{"Intel ICH9", 0x8086, 0x293e, AZX_DRIVER_ICH},
	{"Intel ICH9", 0x8086, 0x293f, AZX_DRIVER_ICH},
	{"Intel ICH", 0x8086, 0x3a3e, AZX_DRIVER_ICH},
	{"Intel ICH", 0x8086, 0x3a6e, AZX_DRIVER_ICH},
	{"Intel PCH", 0x8086, 0x3b56, AZX_DRIVER_ICH},
	{"Intel PCH", 0x8086, 0x3b57, AZX_DRIVER_ICH},
	{"Intel CPT", 0x8086, 0x1c20, AZX_DRIVER_PCH},
	{"Intel SCH", 0x8086, 0x811b, AZX_DRIVER_SCH},
	{"ATI SB450", 0x1002, 0x437b, AZX_DRIVER_ATI},
	{"ATI SB600", 0x1002, 0x4383, AZX_DRIVER_ATI},
	{"ATI RS600", 0x1002, 0x793b, AZX_DRIVER_ATIHDMI},
	{"ATI RS690", 0x1002, 0x7919, AZX_DRIVER_ATIHDMI},
	{"ATI RS780", 0x1002, 0x960c, AZX_DRIVER_ATIHDMI},	// ???
	{"ATI HDMI", 0x1002, 0x960f, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0x970f, AZX_DRIVER_ATIHDMI},
	{"ATI R600", 0x1002, 0xaa00, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa08, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa10, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa18, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa20, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa28, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa30, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa38, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa40, AZX_DRIVER_ATIHDMI},
	{"ATI HDMI", 0x1002, 0xaa48, AZX_DRIVER_ATIHDMI},
	{"VIA 82xx", 0x1106, 0x3288, AZX_DRIVER_VIA},
	{"SIS 966", 0x1039, 0x7502, AZX_DRIVER_SIS},
	{"ULI M5461", 0x10b9, 0x5461, AZX_DRIVER_ULI},
	{"NVidia MCP51", 0x10de, 0x026c, AZX_DRIVER_NVIDIA},
	{"NVidia MCP55", 0x10de, 0x0371, AZX_DRIVER_NVIDIA},
	{"NVidia MCP61", 0x10de, 0x03e4, AZX_DRIVER_NVIDIA},
	{"NVidia MCP61", 0x10de, 0x03f0, AZX_DRIVER_NVIDIA},
	{"NVidia MCP65", 0x10de, 0x044a, AZX_DRIVER_NVIDIA},
	{"NVidia MCP65", 0x10de, 0x044b, AZX_DRIVER_NVIDIA},
	{"NVidia MCP67", 0x10de, 0x055c, AZX_DRIVER_NVIDIA},
	{"NVidia MCP67", 0x10de, 0x055d, AZX_DRIVER_NVIDIA},
	{"NVidia MCP77", 0x10de, 0x0774, AZX_DRIVER_NVIDIA},
	{"NVidia MCP77", 0x10de, 0x0775, AZX_DRIVER_NVIDIA},
	{"NVidia MCP77", 0x10de, 0x0776, AZX_DRIVER_NVIDIA},
	{"NVidia MCP77", 0x10de, 0x0777, AZX_DRIVER_NVIDIA},
	{"NVidia MCP73", 0x10de, 0x07fc, AZX_DRIVER_NVIDIA},
	{"NVidia MCP73", 0x10de, 0x07fd, AZX_DRIVER_NVIDIA},
	{"NVidia MCP79", 0x10de, 0x0ac0, AZX_DRIVER_NVIDIA},
	{"NVidia MCP79", 0x10de, 0x0ac1, AZX_DRIVER_NVIDIA},
	{"NVidia MCP79", 0x10de, 0x0ac2, AZX_DRIVER_NVIDIA},
	{"NVidia MCP79", 0x10de, 0x0ac3, AZX_DRIVER_NVIDIA},
	{"NVidia MCP", 0x10de, 0x0d94, AZX_DRIVER_NVIDIA},
	{"NVidia MCP", 0x10de, 0x0d95, AZX_DRIVER_NVIDIA},
	{"NVidia MCP", 0x10de, 0x0d96, AZX_DRIVER_NVIDIA},
	{"NVidia MCP", 0x10de, 0x0d97, AZX_DRIVER_NVIDIA},
	{"Teradici", 0x6549, 0x1200, AZX_DRIVER_TERA},
	{"Creative", 0x1102, 0x0009, AZX_DRIVER_GENERIC},
	{"Intel ICH6", 0x17f3, 0x3010, AZX_DRIVER_ICH},	// user request http://www.norhtec.com/products/gecko
	//{"AMD Generic",  0x1002, 0x0000, AZX_DRIVER_GENERIC }, // ??? cannot define this

	{NULL, 0, 0, 0}
};

static struct codec_vendor_list_s codecvendorlist[] = {
	{0x1002, "ATI"},
	{0x1013, "Cirrus Logic"},
	{0x1057, "Motorola"},
	{0x1095, "Silicon Image"},
	{0x10de, "Nvidia"},
	{0x10ec, "Realtek"},
	{0x1102, "Creative"},
	{0x1106, "VIA"},
	{0x111d, "IDT"},
	{0x11c1, "LSI"},
	{0x11d4, "Analog Devices"},
	{0x13f6, "C-Media"},
	{0x14f1, "Conexant"},
	{0x17e8, "Chrontel"},
	{0x1854, "LG"},
	{0x1aec, "Wolfson"},
	{0x434d, "C-Media"},
	{0x8086, "Intel"},
	{0x8384, "SigmaTel"},
	{0x0000, "Unknown"}
};

static void INTELHD_close(struct mpxplay_audioout_info_s *aui);

static char *ihd_search_vendorname(unsigned int vendorid)
{
	struct codec_vendor_list_s *cl = &codecvendorlist[0];
	do {
		if(cl->vendor_id == vendorid)
			break;
		cl++;
	} while(cl->vendor_id);
	return cl->vendor_name;
}

static void INTELHD_card_info(struct mpxplay_audioout_info_s *aui)
{
	struct intelhd_card_s *card = aui->card_private_data;
	char sout[100];
	sprintf(sout, "IHD : %s (%4.4X%4.4X) -> %s (%8.8X) (max %dkHz/%dbit%s/%dch)",
			card->pci_dev->device_name,
			(long)card->pci_dev->vendor_id, (long)card->pci_dev->device_id,
			ihd_search_vendorname(card->codec_vendor_id >> 16), card->codec_vendor_id,
			(card->supported_max_freq / 1000), card->supported_max_bits, ((card->supported_formats == 0xffffffff) ? "?" : ""), min(INTHD_MAX_CHANNELS, PCM_MAX_CHANNELS)
		);
	pds_textdisplay_printf(sout);
}

static int INTELHD_adetect(struct mpxplay_audioout_info_s *aui)
{
	struct intelhd_card_s *card;
	unsigned int i;

	card = (struct intelhd_card_s *)calloc(1, sizeof(struct intelhd_card_s));
	if(!card)
		return 0;
	aui->card_private_data = card;

	card->pci_dev = (struct pci_config_s *)calloc(1, sizeof(struct pci_config_s));
	if(!card->pci_dev)
		goto err_adetect;
	if(pcibios_search_devices(&intelhda_devices, card->pci_dev) != PCI_SUCCESSFUL)
		goto err_adetect;

	pcibios_enable_memmap_set_master(card->pci_dev);	// Intel HDA chips uses memory mapping only (by the spec)

	card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
	if(card->iobase & 0x1)		// we handle memory mapping only
		card->iobase = 0;
	if(!card->iobase)
		goto err_adetect;
	card->iobase &= 0xfffffff8;
	card->iobase = pds_dpmi_map_physical_memory(card->iobase, 16384);
	if(!card->iobase)
		goto err_adetect;

	card->board_driver_type = card->pci_dev->device_type;
	if(!snd_ihd_buffer_init(aui, card))
		goto err_adetect;

	aui->card_DMABUFF = card->pcmout_buffer;

	mpxplay_debugf(IHD_DEBUG_OUTPUT, "IHD board type: %s (%4.4X%4.4X) ", card->pci_dev->device_name, (long)card->pci_dev->vendor_id, (long)card->pci_dev->device_id);

	snd_ihd_hw_init(card);

	for(i = 0; i < AZX_MAX_CODECS; i++) {
		if(card->codec_mask & (1 << i)) {
			card->codec_index = i;
			if(snd_ihd_mixer_init(card))
				break;
		}
	}

	return 1;

  err_adetect:
	INTELHD_close(aui);
	return 0;
}

static void INTELHD_close(struct mpxplay_audioout_info_s *aui)
{
	struct intelhd_card_s *card = aui->card_private_data;
	if(card) {
		if(card->iobase) {
			snd_ihd_hw_close(card);
			pds_dpmi_unmap_physycal_memory(card->iobase);
		}
		if(card->afg_nodes)
			free(card->afg_nodes);
		MDma_free_dosmem(card->dm);
		if(card->pci_dev)
			free(card->pci_dev);
		free(card);
		aui->card_private_data = NULL;
	}
}

static void INTELHD_setrate(struct mpxplay_audioout_info_s *aui)
{
	struct intelhd_card_s *card = aui->card_private_data;

	aui->card_wave_id = 0x0001;
	aui->chan_card = (aui->chan_set) ? aui->chan_set : 2;
	if(aui->chan_card > INTHD_MAX_CHANNELS)
		aui->chan_card = INTHD_MAX_CHANNELS;
	if(!card->dacout_num_bits)	// first initialization
		card->dacout_num_bits = (aui->bits_set) ? aui->bits_set : 16;

	card->format_val = snd_hda_calc_stream_format(aui, card);
	card->pcmout_dmasize = MDma_init_pcmoutbuf(aui, card->pcmout_bufsize, AZX_PERIOD_SIZE, 0);

	azx_setup_periods(card);
	azx_setup_controller(card);
}

static void INTELHD_start(struct mpxplay_audioout_info_s *aui)
{
	struct intelhd_card_s *card = aui->card_private_data;
	//const unsigned int stream_index=0;
	//azx_writeb(card, INTCTL, azx_readb(card, INTCTL) | (1 << stream_index)); // enable SIE
	//azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_DMA_START | SD_INT_MASK); // start DMA
	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_DMA_START);	// start DMA
}

static void INTELHD_stop(struct mpxplay_audioout_info_s *aui)
{
	struct intelhd_card_s *card = aui->card_private_data;
	//const unsigned int stream_index=0;
	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) & ~(SD_CTL_DMA_START | SD_INT_MASK));	// stop DMA
	//azx_sd_writeb(card, SD_STS, SD_INT_MASK); // to be sure
	//azx_writeb(card, INTCTL,azx_readb(card, INTCTL) & ~(1 << stream_index));
}

static long INTELHD_getbufpos(struct mpxplay_audioout_info_s *aui)
{
	struct intelhd_card_s *card = aui->card_private_data;
	unsigned long bufpos;

	bufpos = azx_sd_readl(card, SD_LPIB);

	//mpxplay_debugf(IHD_DEBUG_OUTPUT,"bufpos1:%d sts:%8.8X ctl:%8.8X cbl:%d ds:%d ps:%d pn:%d",bufpos,azx_sd_readb(card, SD_STS),azx_sd_readl(card, SD_CTL),azx_sd_readl(card, SD_CBL),aui->card_dmasize,
	// card->pcmout_period_size,card->pcmout_num_periods);

	if(bufpos < aui->card_dmasize)
		aui->card_dma_lastgoodpos = bufpos;

	return aui->card_dma_lastgoodpos;
}

//--------------------------------------------------------------------------
//mixer

static void INTELHD_writeMIXER(struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
	struct intelhd_card_s *card = aui->card_private_data;
	snd_hda_put_vol_mute(card, reg, 0, HDA_OUTPUT, 0, val);
	snd_hda_put_vol_mute(card, reg, 1, HDA_OUTPUT, 0, val);
}

static unsigned long INTELHD_readMIXER(struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
	struct intelhd_card_s *card = aui->card_private_data;
	return snd_hda_get_vol_mute(card, reg, 0, HDA_OUTPUT, 0);
}

static aucards_allmixerchan_s ihd_mixerset = {
	&ihd_master_vol,
	NULL
};

one_sndcard_info IHD_sndcard_info = {
	"IHD",
	SNDCARD_LOWLEVELHAND | SNDCARD_INT08_ALLOWED,

	NULL,						// card_config
	NULL,						// no init
	&INTELHD_adetect,			// only autodetect
	&INTELHD_card_info,
	&INTELHD_start,
	&INTELHD_stop,
	&INTELHD_close,
	&INTELHD_setrate,

	&MDma_writedata,
	&INTELHD_getbufpos,
	&MDma_clearbuf,
	&MDma_interrupt_monitor,
	NULL,

	&INTELHD_writeMIXER,
	&INTELHD_readMIXER,
	&ihd_mixerset
};

#endif							// AU_CARDS_LINK_IHD
