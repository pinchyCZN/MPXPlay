/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: decoder.c,v 1.70 2003/09/20 00:00:00 PDSoft Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>

#include "decoder.h"
#include "mp4.h"
#include "syntax.h"
#include "specrec.h"
#include "tns.h"
#include "pns.h"
#include "ms_is.h"
#include "ic_predi.h"
#include "lt_predi.h"
#include "drc.h"
#include "output.h"
#ifdef SSR_DEC
#include "ssr.h"
#include "ssr_fb.h"
#endif
#ifdef SBR_DEC
#include "sbr_dec.h"
#endif

#ifdef ANALYSIS
uint16_t dbg_count;
#endif

uint32_t FAADAPI faacDecGetCapabilities()
{
	uint32_t cap = 0;

	cap += LC_DEC_CAP;

#ifdef MAIN_DEC
	cap += MAIN_DEC_CAP;
#endif
#ifdef LTP_DEC
	cap += LTP_DEC_CAP;
#endif
#ifdef LD_DEC
	cap += LD_DEC_CAP;
#endif
#ifdef ERROR_RESILIENCE
	cap += ERROR_RESILIENCE_CAP;
#endif

	return cap;
}

faacDecHandle FAADAPI faacDecOpen()
{
	faacDecHandle hDecoder;

	hDecoder = (faacDecHandle) malloc(sizeof(faacDecStruct));
	if(!hDecoder)
		return hDecoder;

	memset(hDecoder, 0, sizeof(faacDecStruct));

	hDecoder->config.outputFormat = FAAD_FMT_FLOAT;
	hDecoder->config.defObjectType = MAIN;
	hDecoder->config.defSampleRate = 44100;
	hDecoder->frameLength = 1024;

	return hDecoder;
}

faacDecConfigurationPtr FAADAPI faacDecGetCurrentConfiguration(faacDecHandle hDecoder)
{
	faacDecConfigurationPtr config = &(hDecoder->config);
	return config;
}

//----------------------------------------------------------------------

int32_t FAADAPI faacDecInit_frame(faacDecHandle hDecoder, uint8_t * buffer, uint32_t buffer_size, faacDecFrameInfo * frameInfo)
{
	bitfile ld;
	adif_header adif;
	adts_header adts;

	if((hDecoder == NULL) || (buffer == NULL) || (buffer_size < 4) || (frameInfo == NULL))
		return -1;

	memset((void *)frameInfo, 0, sizeof(faacDecFrameInfo));

	hDecoder->object_type = hDecoder->config.defObjectType;
	hDecoder->sbr_present_flag = hDecoder->config.defSBRpresentflag;
	hDecoder->channelConfiguration = hDecoder->config.defChannels;
	if(!hDecoder->channelConfiguration)
		hDecoder->channelConfiguration = MAX_CHANNELS;
	if(hDecoder->sbr_present_flag)
		if(hDecoder->config.defSampleRate > 24000)
			hDecoder->config.defSampleRate /= 2;

	hDecoder->sf_index = get_sr_index(hDecoder->config.defSampleRate);

	if(buffer) {

		faad_initbits(&ld, buffer, buffer_size);

		if((buffer[0] == 'A') && (buffer[1] == 'D') && (buffer[2] == 'I') && (buffer[3] == 'F')) {
			hDecoder->adif_header_present = 1;

			if(get_adif_header(&adif, &ld) != 0)
				return -1;
			faad_byte_align(&ld);

			hDecoder->sf_index = adif.pce[0].sf_index;
			hDecoder->object_type = adif.pce[0].object_type;
			hDecoder->channelConfiguration = min(MAX_CHANNELS, adif.pce[0].channels);

			memcpy(&(hDecoder->pce), &(adif.pce[0]), sizeof(program_config));
			hDecoder->pce_set = 1;

			frameInfo->bytesconsumed = bit2byte(faad_get_processed_bits(&ld));

		} else {
			if(faad_bits_show24(&ld, 12) == 0xfff) {
				hDecoder->adts_header_present = 1;

				adts_frame(&adts, &ld);

				hDecoder->sf_index = adts.sf_index;
				hDecoder->object_type = adts.profile;
				hDecoder->channelConfiguration = min(MAX_CHANNELS, adts.channel_configuration);
			}
		}
		if(ld.error)
			return -1;
	}

	frameInfo->samplerate = sample_rates[hDecoder->sf_index];
	if(hDecoder->sbr_present_flag) {
		if(frameInfo->samplerate <= 24000)
			frameInfo->samplerate *= 2;
	}

	frameInfo->channels = (hDecoder->channelConfiguration > MAX_CHANNELS) ? 2 : hDecoder->channelConfiguration;

	if(!can_decode_ot(hDecoder->object_type))
		return -1;

	frameInfo->object_type = hDecoder->object_type;
	frameInfo->sbr_present_flag = hDecoder->sbr_present_flag;
	frameInfo->sf_index = hDecoder->sf_index;
	frameInfo->frameLength = hDecoder->frameLength;
	if(frameInfo->sbr_present_flag)
		frameInfo->frameLength *= 2;

	return 0;
}

int32_t FAADAPI faacDecInit_dsi(faacDecHandle hDecoder, uint8_t * pBuffer, uint32_t SizeOfDecoderSpecificInfo, faacDecFrameInfo * frameInfo)
{
	int8_t rc;
	mp4AudioSpecificConfig mp4ASC;

	hDecoder->adif_header_present = 0;
	hDecoder->adts_header_present = 0;

	if((hDecoder == NULL) || (pBuffer == NULL) || (SizeOfDecoderSpecificInfo < 2) || (frameInfo == NULL))
		return -1;

	memset((void *)frameInfo, 0, sizeof(faacDecFrameInfo));

	rc = AudioSpecificConfig2(pBuffer, SizeOfDecoderSpecificInfo, &mp4ASC, &(hDecoder->pce));

	frameInfo->samplerate = mp4ASC.samplingFrequency;
	if(mp4ASC.channelsConfiguration) {
		frameInfo->channels = mp4ASC.channelsConfiguration;
	} else {
		frameInfo->channels = hDecoder->pce.channels;
		hDecoder->pce_set = 1;
	}

	hDecoder->sf_index = mp4ASC.samplingFrequencyIndex;
	hDecoder->object_type = mp4ASC.objectTypeIndex;
	hDecoder->aacSectionDataResilienceFlag = mp4ASC.aacSectionDataResilienceFlag;
	hDecoder->aacScalefactorDataResilienceFlag = mp4ASC.aacScalefactorDataResilienceFlag;
	hDecoder->aacSpectralDataResilienceFlag = mp4ASC.aacSpectralDataResilienceFlag;
#ifdef SBR_DEC
	hDecoder->sbr_present_flag = mp4ASC.sbr_present_flag;

	if(hDecoder->sbr_present_flag == 1)
		hDecoder->sf_index = get_sr_index(mp4ASC.samplingFrequency / 2);
#endif

	if(hDecoder->object_type && (hDecoder->object_type < 5))
		hDecoder->object_type--;	// For AAC differs from MPEG-4

	if(rc != 0)
		return rc;

	hDecoder->channelConfiguration = mp4ASC.channelsConfiguration;
	if(mp4ASC.frameLengthFlag)
		hDecoder->frameLength = 960;

	frameInfo->object_type = hDecoder->object_type;
	frameInfo->sbr_present_flag = hDecoder->sbr_present_flag;
	frameInfo->sf_index = hDecoder->sf_index;
	frameInfo->frameLength = hDecoder->frameLength;
	if(frameInfo->sbr_present_flag)
		frameInfo->frameLength *= 2;

	return 0;
}

//-------------------------------------------------------------------------

/*int32_t FAADAPI faacDecInit(faacDecHandle hDecoder, uint8_t *buffer,
                            uint32_t buffer_size,
                            uint32_t *samplerate, uint32_t *channels)
{
 uint32_t bits = 0;
 bitfile ld;
 adif_header adif;
 adts_header adts;

 hDecoder->object_type      = hDecoder->config.defObjectType;
 hDecoder->sbr_present_flag = hDecoder->config.defSBRpresentflag;
 hDecoder->channelConfiguration = hDecoder->config.defChannels;
 if(!hDecoder->channelConfiguration)
  hDecoder->channelConfiguration=MAX_CHANNELS;
 //hDecoder->frameLength      = hDecoder->config.defFrameLength;
 if(hDecoder->sbr_present_flag)
  if(hDecoder->config.defSampleRate>24000)
   hDecoder->config.defSampleRate/=2;

 hDecoder->sf_index         = get_sr_index(hDecoder->config.defSampleRate);

 if(buffer){

  faad_initbits(&ld, buffer, buffer_size);

  if((buffer[0]=='A')&&(buffer[1]=='D')&&(buffer[2]=='I')&&(buffer[3]=='F')){
   hDecoder->adif_header_present = 1;

   get_adif_header(&adif, &ld);
   faad_byte_align(&ld);

   hDecoder->sf_index = adif.pce[0].sf_index;
   hDecoder->object_type = adif.pce[0].object_type;
   hDecoder->channelConfiguration = min(MAX_CHANNELS,adif.pce[0].channels);

   memcpy(&(hDecoder->pce), &(adif.pce[0]), sizeof(program_config));
   hDecoder->pce_set = 1;

   bits = bit2byte(faad_get_processed_bits(&ld));

  }else{
   if(faad_bits_show24(&ld, 12) == 0xfff){
    hDecoder->adts_header_present = 1;

    adts_frame(&adts, &ld);

    hDecoder->sf_index = adts.sf_index;
    hDecoder->object_type = adts.profile;
    hDecoder->channelConfiguration = min(MAX_CHANNELS,adts.channel_configuration);
   }
  }
  if(ld.error)
   return -1;
 }

 *samplerate = sample_rates[hDecoder->sf_index];
 if(hDecoder->sbr_present_flag){                  // ???
  if(*samplerate<=24000)                          //
   *samplerate *=2;                               //
 }

 *channels=(hDecoder->channelConfiguration>6)? 2:hDecoder->channelConfiguration;

 if(!can_decode_ot(hDecoder->object_type))
  return -1;

 return bits;
}*/

/* Init the library using a DecoderSpecificInfo */
/*int32_t FAADAPI faacDecInit2(faacDecHandle hDecoder, uint8_t *pBuffer,
                            uint32_t SizeOfDecoderSpecificInfo,
                            uint32_t *samplerate, uint32_t *channels)
{
 int8_t rc;
 mp4AudioSpecificConfig mp4ASC;

 hDecoder->adif_header_present = 0;
 hDecoder->adts_header_present = 0;

 if((hDecoder == NULL) || (pBuffer == NULL) || (SizeOfDecoderSpecificInfo < 2)
   || (samplerate == NULL) || (channels == NULL)){
  return -1;
 }

 rc = AudioSpecificConfig2(pBuffer, SizeOfDecoderSpecificInfo, &mp4ASC,&(hDecoder->pce));

 *samplerate = mp4ASC.samplingFrequency;
 if(mp4ASC.channelsConfiguration){
  *channels = mp4ASC.channelsConfiguration;
 }else{
  *channels = hDecoder->pce.channels;
  hDecoder->pce_set = 1;
 }

 hDecoder->sf_index = mp4ASC.samplingFrequencyIndex;
 hDecoder->object_type = mp4ASC.objectTypeIndex;
 hDecoder->aacSectionDataResilienceFlag = mp4ASC.aacSectionDataResilienceFlag;
 hDecoder->aacScalefactorDataResilienceFlag = mp4ASC.aacScalefactorDataResilienceFlag;
 hDecoder->aacSpectralDataResilienceFlag = mp4ASC.aacSpectralDataResilienceFlag;
#ifdef SBR_DEC
 hDecoder->sbr_present_flag = mp4ASC.sbr_present_flag;

 if(hDecoder->sbr_present_flag == 1)
  hDecoder->sf_index = get_sr_index(mp4ASC.samplingFrequency / 2);
#endif

 if(hDecoder->object_type && (hDecoder->object_type<5))
  hDecoder->object_type--; // For AAC differs from MPEG-4

 if(rc!=0)
  return rc;

 hDecoder->channelConfiguration = mp4ASC.channelsConfiguration;
 if(mp4ASC.frameLengthFlag)
  hDecoder->frameLength = 960;

 return 0;
}*/

/*long FAADAPI faacDecGetframeLength(faacDecHandle hDecoder)
{
 long framelen=hDecoder->frameLength;
 if(hDecoder->sbr_present_flag==1)
  framelen*=2;
 return framelen;
}*/

//-----------------------------------------------------------------------

unsigned int FAADAPI faacDecInitFields(faacDecHandle hDecoder)
{
	int ch;

#ifdef AAC_DEBUGINFO
	fprintf(stdout, "ch:%d frl:%d \n", hDecoder->channelConfiguration, hDecoder->frameLength);
#endif

	if(hDecoder->frameLength < 16)
		return 0;

	hDecoder->drc = drc_init(REAL_CONST(1.0), REAL_CONST(1.0));
	if(!hDecoder->drc)
		return 0;

/*#if POW_TABLE_SIZE
 hDecoder->pow2_table = (real_t*)malloc(POW_TABLE_SIZE*sizeof(real_t));
 if(!hDecoder->pow2_table)
  return 0;
 build_tables(hDecoder->pow2_table);
#endif*/
	build_tables();

	/* must be done before frameLength is divided by 2 for LD */
#ifdef SSR_DEC
	if(hDecoder->object_type == SSR)
		hDecoder->fb = ssr_filter_bank_init(hDecoder->frameLength / SSR_BANDS);
	else
#endif
		hDecoder->fb = filter_bank_init(hDecoder->frameLength);
	if(!hDecoder->fb)
		return 0;

#ifdef LD_DEC
	if(hDecoder->object_type == LD)
		hDecoder->frameLength >>= 1;
#endif

	if(hDecoder->config.outputFormat == FAAD_FMT_DOUBLE)
		hDecoder->sample_buffer = malloc(2 * hDecoder->frameLength * hDecoder->channelConfiguration * sizeof(double));
	else
		hDecoder->sample_buffer = malloc(2 * hDecoder->frameLength * hDecoder->channelConfiguration * sizeof(real_t));

	if(!hDecoder->sample_buffer)
		return 0;

#ifdef ERROR_RESILIENCE
	hDecoder->rsd_datas = aacdec_hcr_init();
	if(!hDecoder->rsd_datas)
		return 0;
#endif

	hDecoder->window_shape_prev = (uint8_t *) calloc(hDecoder->channelConfiguration, sizeof(uint8_t));
	if(!hDecoder->window_shape_prev)
		return 0;

#ifdef LTP_DEC
	if((hDecoder->object_type == LTP)
#ifdef ERROR_RESILIENCE
	   || (hDecoder->object_type == ER_LTP)
#endif
#ifdef LD_DEC
	   || (hDecoder->object_type == LD)
#endif
		) {
		hDecoder->ltd = lt_pred_init(hDecoder->frameLength);
		hDecoder->ltp_lag = (uint16_t *) calloc(hDecoder->channelConfiguration, sizeof(uint16_t));
		hDecoder->lt_pred_stat = (real_t **) calloc(hDecoder->channelConfiguration, sizeof(real_t *));
		if(!hDecoder->ltd || !hDecoder->ltp_lag || !hDecoder->lt_pred_stat)
			return 0;
	}
#endif
	hDecoder->time_out = (real_t **) calloc(hDecoder->channelConfiguration, sizeof(real_t *));
	if(!hDecoder->time_out)
		return 0;
#ifdef SBR_DEC
	hDecoder->sbr_used = (uint8_t *) calloc(hDecoder->channelConfiguration, sizeof(uint8_t));
	if(!hDecoder->sbr_used)
		return 0;
	if(hDecoder->sbr_present_flag == 1) {
		hDecoder->time_out2 = (real_t **) calloc(hDecoder->channelConfiguration, sizeof(real_t *));
		hDecoder->sbr = (sbr_info **) calloc(hDecoder->channelConfiguration, sizeof(sbr_info *));
		if(!hDecoder->time_out2 || !hDecoder->sbr)
			return 0;
		//hDecoder->sbr_present_flag=0;
	}
#endif
#ifdef MAIN_DEC
	if(hDecoder->object_type == MAIN) {
		hDecoder->pred_stat = (pred_state **) calloc(hDecoder->channelConfiguration, sizeof(pred_state *));
		if(!hDecoder->pred_stat)
			return 0;
	}
#endif

	hDecoder->spec_data = (int16_t **) calloc(hDecoder->channelConfiguration, sizeof(int16_t *));
	hDecoder->spec_coef = (real_t **) calloc(hDecoder->channelConfiguration, sizeof(real_t *));
	hDecoder->syntax_elements = (element **) calloc(hDecoder->channelConfiguration, sizeof(element *));
	if(!hDecoder->spec_data || !hDecoder->spec_coef || !hDecoder->syntax_elements)
		return 0;

	for(ch = 0; ch < hDecoder->channelConfiguration; ch++) {

#ifdef MAIN_DEC
		if(hDecoder->pred_stat) {
			hDecoder->pred_stat[ch] = (pred_state *) malloc(hDecoder->frameLength * sizeof(pred_state));
			if(!hDecoder->pred_stat[ch])
				return 0;
			reset_all_predictors(hDecoder->pred_stat[ch], hDecoder->frameLength);
		}
#endif

#ifdef LTP_DEC
		if(hDecoder->lt_pred_stat) {
			hDecoder->lt_pred_stat[ch] = (real_t *) calloc(hDecoder->frameLength * 4, sizeof(real_t));
			if(!hDecoder->lt_pred_stat[ch])
				return 0;
		}
#endif

		hDecoder->time_out[ch] = (real_t *) calloc(hDecoder->frameLength * 2, sizeof(real_t));
		if(!hDecoder->time_out[ch])
			return 0;

#ifdef SBR_DEC
		if(hDecoder->time_out2 && hDecoder->sbr) {
			hDecoder->time_out2[ch] = (real_t *) calloc(hDecoder->frameLength * 2, sizeof(real_t));
			hDecoder->sbr[ch] = sbrDecodeInit();
			if(!hDecoder->time_out2[ch] || !hDecoder->sbr[ch])
				return 0;
		}
#endif

		hDecoder->spec_data[ch] = (int16_t *) malloc(hDecoder->frameLength * sizeof(int16_t));
		//hDecoder->spec_coef[ch]=(real_t *)calloc(hDecoder->frameLength,sizeof(real_t));
		hDecoder->spec_coef[ch] = (real_t *) malloc(hDecoder->frameLength * sizeof(real_t));
		hDecoder->syntax_elements[ch] = (element *) malloc(sizeof(element));
		if(!hDecoder->spec_data[ch] || !hDecoder->spec_coef[ch] || !hDecoder->syntax_elements[ch])
			return 0;

#ifdef SSR_DEC
		if(hDecoder->object_type == SSR) {
			hDecoder->ssr_overlap[ch] = (real_t *) calloc(hDecoder->frameLength * 2, sizeof(real_t));
			hDecoder->prev_fmd[ch] = (real_t *) malloc(2 * hDecoder->frameLength * sizeof(real_t));
			if(!hDecoder->ssr_overlap[ch] || !hDecoder->prev_fmd[ch])
				return 0;
			{
				int k;
				for(k = 0; k < 2 * hDecoder->frameLength; k++)
					hDecoder->prev_fmd[ch][k] = REAL_CONST(-1);
			}
		}
#endif
	}

	return 1;
}


//bitstream related mallocs only
unsigned int FAADAPI faacDecInitField_bs(faacDecHandle hDecoder)
{
	int ch;

#ifdef AAC_DEBUGINFO
	fprintf(stdout, "ch:%d frl:%d \n", hDecoder->channelConfiguration, hDecoder->frameLength);
#endif

	hDecoder->drc = drc_init(REAL_CONST(1.0), REAL_CONST(1.0));
	if(!hDecoder->drc)
		return 0;

#ifdef SBR_DEC
	hDecoder->sbr_used = (uint8_t *) calloc(hDecoder->channelConfiguration, sizeof(uint8_t));
	if(!hDecoder->sbr_used)
		return 0;
#endif

	hDecoder->spec_data = (int16_t **) calloc(hDecoder->channelConfiguration, sizeof(int16_t *));
	hDecoder->spec_coef = (real_t **) calloc(hDecoder->channelConfiguration, sizeof(real_t *));
	hDecoder->syntax_elements = (element **) calloc(hDecoder->channelConfiguration, sizeof(element *));
	if(!hDecoder->spec_data || !hDecoder->spec_coef || !hDecoder->syntax_elements)
		return 0;

	for(ch = 0; ch < hDecoder->channelConfiguration; ch++) {
		hDecoder->spec_data[ch] = (int16_t *) malloc(hDecoder->frameLength * sizeof(int16_t));
		hDecoder->spec_coef[ch] = (real_t *) malloc(hDecoder->frameLength * sizeof(real_t));
		hDecoder->syntax_elements[ch] = (element *) malloc(sizeof(element));
		if(!hDecoder->spec_data[ch] || !hDecoder->spec_coef[ch] || !hDecoder->syntax_elements[ch])
			return 0;
	}

	return 1;
}

void FAADAPI faacDecClose(faacDecHandle hDecoder)
{
	uint8_t ch;

	if(hDecoder) {

		for(ch = 0; ch < hDecoder->channelConfiguration; ch++) {
#ifdef LTP_DEC
			if(hDecoder->lt_pred_stat)
				if(hDecoder->lt_pred_stat[ch])
					free(hDecoder->lt_pred_stat[ch]);
#endif

			if(hDecoder->time_out)
				if(hDecoder->time_out[ch])
					free(hDecoder->time_out[ch]);
#ifdef SBR_DEC
			if(hDecoder->time_out2)
				if(hDecoder->time_out2[ch])
					free(hDecoder->time_out2[ch]);
			if(hDecoder->sbr)
				if(hDecoder->sbr[ch])
					sbrDecodeEnd(hDecoder->sbr[ch]);
#endif

#ifdef MAIN_DEC
			if(hDecoder->pred_stat)
				if(hDecoder->pred_stat[ch])
					free(hDecoder->pred_stat[ch]);
#endif

			if(hDecoder->spec_data)
				if(hDecoder->spec_data[ch])
					free(hDecoder->spec_data[ch]);
			if(hDecoder->spec_coef)
				if(hDecoder->spec_coef[ch])
					free(hDecoder->spec_coef[ch]);
			if(hDecoder->syntax_elements)
				if(hDecoder->syntax_elements[ch])
					free(hDecoder->syntax_elements[ch]);

#ifdef SSR_DEC
			if(hDecoder->ssr_overlap[ch])
				free(hDecoder->ssr_overlap[ch]);
			if(hDecoder->prev_fmd[ch])
				free(hDecoder->prev_fmd[ch]);
#endif
		}
#ifdef ERROR_RESILIENCE
		aacdec_hcr_close(hDecoder->rsd_datas);
#endif
#ifdef LTP_DEC
		lt_pred_close(hDecoder->ltd);
		if(hDecoder->ltp_lag)
			free(hDecoder->ltp_lag);
		if(hDecoder->lt_pred_stat)
			free(hDecoder->lt_pred_stat);
#endif
#ifdef SSR_DEC
		if(hDecoder->object_type == SSR)
			ssr_filter_bank_end(hDecoder->fb);
		else
#endif
			filter_bank_end(hDecoder->fb);
		drc_end(hDecoder->drc);

		if(hDecoder->sample_buffer)
			free(hDecoder->sample_buffer);
		if(hDecoder->window_shape_prev)
			free(hDecoder->window_shape_prev);
		if(hDecoder->time_out)
			free(hDecoder->time_out);
#ifdef SBR_DEC
		if(hDecoder->time_out2)
			free(hDecoder->time_out2);
		if(hDecoder->sbr_used)
			free(hDecoder->sbr_used);
		if(hDecoder->sbr)
			free(hDecoder->sbr);
#endif
#ifdef MAIN_DEC
		if(hDecoder->pred_stat)
			free(hDecoder->pred_stat);
#endif
		if(hDecoder->spec_data)
			free(hDecoder->spec_data);
		if(hDecoder->spec_coef)
			free(hDecoder->spec_coef);
		if(hDecoder->syntax_elements)
			free(hDecoder->syntax_elements);
		//if(hDecoder->pow2_table)        free(hDecoder->pow2_table);

		free(hDecoder);
	}
}

//-------------------------------------------------------------------------

static void create_channel_config(faacDecHandle hDecoder, faacDecFrameInfo * hInfo, uint8_t channelconfig)
{
	hInfo->num_front_channels = 0;
	hInfo->num_side_channels = 0;
	hInfo->num_back_channels = 0;
	hInfo->num_lfe_channels = 0;
	memset(hInfo->channel_position, 0, MAX_CHANNELS * sizeof(uint8_t));

	if(hDecoder->downMatrix) {
		hInfo->num_front_channels = 2;
		hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
		hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
		return;
	}

	if(hDecoder->pce_set) {
		uint8_t i, chpos = 0;
		uint8_t chdir, back_center = 0;

		hInfo->num_front_channels = hDecoder->pce.num_front_channels;
		hInfo->num_side_channels = hDecoder->pce.num_side_channels;
		hInfo->num_back_channels = hDecoder->pce.num_back_channels;
		hInfo->num_lfe_channels = hDecoder->pce.num_lfe_channels;

		chdir = hInfo->num_front_channels;
		if(chdir & 1) {
			hInfo->channel_position[chpos++] = FRONT_CHANNEL_CENTER;
			chdir--;
		}
		for(i = 0; i < chdir; i += 2) {
			hInfo->channel_position[chpos++] = FRONT_CHANNEL_LEFT;
			hInfo->channel_position[chpos++] = FRONT_CHANNEL_RIGHT;
		}

		for(i = 0; i < hInfo->num_side_channels; i += 2) {
			hInfo->channel_position[chpos++] = SIDE_CHANNEL_LEFT;
			hInfo->channel_position[chpos++] = SIDE_CHANNEL_RIGHT;
		}

		chdir = hInfo->num_back_channels;
		if(chdir & 1) {
			back_center = 1;
			chdir--;
		}
		for(i = 0; i < chdir; i += 2) {
			hInfo->channel_position[chpos++] = BACK_CHANNEL_LEFT;
			hInfo->channel_position[chpos++] = BACK_CHANNEL_RIGHT;
		}
		if(back_center) {
			hInfo->channel_position[chpos++] = BACK_CHANNEL_CENTER;
		}

		for(i = 0; i < hInfo->num_lfe_channels; i++) {
			hInfo->channel_position[chpos++] = LFE_CHANNEL;
		}

	} else {
		switch (channelconfig) {
		case 1:
			hInfo->num_front_channels = 1;
			hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
			break;
		case 2:
			hInfo->num_front_channels = 2;
			hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
			hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
			break;
		case 3:
			hInfo->num_front_channels = 3;
			hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
			hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
			hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
			break;
		case 4:
			hInfo->num_front_channels = 3;
			hInfo->num_back_channels = 1;
			hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
			hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
			hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
			hInfo->channel_position[3] = BACK_CHANNEL_CENTER;
			break;
		case 5:
			hInfo->num_front_channels = 3;
			hInfo->num_back_channels = 2;
			hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
			hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
			hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
			hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
			hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
			break;
		case 6:
			hInfo->num_front_channels = 3;
			hInfo->num_back_channels = 2;
			hInfo->num_lfe_channels = 1;
			hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
			hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
			hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
			hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
			hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
			hInfo->channel_position[5] = LFE_CHANNEL;
			break;
		case 7:
			hInfo->num_front_channels = 3;
			hInfo->num_side_channels = 2;
			hInfo->num_back_channels = 2;
			hInfo->num_lfe_channels = 1;
			hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
			hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
			hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
			hInfo->channel_position[3] = SIDE_CHANNEL_LEFT;
			hInfo->channel_position[4] = SIDE_CHANNEL_RIGHT;
			hInfo->channel_position[5] = BACK_CHANNEL_LEFT;
			hInfo->channel_position[6] = BACK_CHANNEL_RIGHT;
			hInfo->channel_position[7] = LFE_CHANNEL;
			break;
		default:				/* channelconfig == 0 || channelconfig > 7 */
			{
				uint8_t i;
				uint8_t ch = hDecoder->fr_channels - hDecoder->has_lfe;
				if(ch & 1) {	/* there's either a center front or a center back channel */
					uint8_t ch1 = (ch - 1) / 2;
					if(hDecoder->first_syn_ele == ID_SCE) {
						hInfo->num_front_channels = ch1 + 1;
						hInfo->num_back_channels = ch1;
						hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
						for(i = 1; i <= ch1; i += 2) {
							hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
						}
						for(i = ch1 + 1; i < ch; i += 2) {
							hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
						}
					} else {
						hInfo->num_front_channels = ch1;
						hInfo->num_back_channels = ch1 + 1;
						for(i = 0; i < ch1; i += 2) {
							hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
						}
						for(i = ch1; i < ch - 1; i += 2) {
							hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
						}
						hInfo->channel_position[ch - 1] = BACK_CHANNEL_CENTER;
					}
				} else {
					uint8_t ch1 = (ch) / 2;
					hInfo->num_front_channels = ch1;
					hInfo->num_back_channels = ch1;
					if(ch1 & 1) {
						hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
						for(i = 1; i <= ch1; i += 2) {
							hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
						}
						for(i = ch1 + 1; i < ch - 1; i += 2) {
							hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
						}
						hInfo->channel_position[ch - 1] = BACK_CHANNEL_CENTER;
					} else {
						for(i = 0; i < ch1; i += 2) {
							hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
						}
						for(i = ch1; i < ch; i += 2) {
							hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
							hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
						}
					}
				}
				hInfo->num_lfe_channels = hDecoder->has_lfe;
				for(i = ch; i < hDecoder->fr_channels; i++) {
					hInfo->channel_position[i] = LFE_CHANNEL;
				}
			}
			break;
		}
	}
}

#define AAC_SPECTRUM_ANALISER 1

#ifdef AAC_SPECTRUM_ANALISER

extern unsigned int displaymode, analtabnum;
extern unsigned long analtab[5][32];

static unsigned long lasts[32], currs[32];

void aac_analiser_clear(void)
{
	memset(&lasts[0], 0, 32 * sizeof(unsigned long));
	memset(&currs[0], 0, 32 * sizeof(unsigned long));
}

real_t *asm_band_add(unsigned int, real_t *);

static void aac_calculate_analiser_bands(real_t * pcm, unsigned int currbs, unsigned int lastch, float scale)
{
	unsigned int nbs, bands = 32;
	unsigned long anl, *ap, *lp, *cp = &currs[0];

	nbs = currbs >> 6;			// currbs/64 (/2 /32)
	if(!nbs)
		return;

	if(lastch) {
		ap = &analtab[analtabnum][0];
		lp = &lasts[0];
	}

	do {
#if defined(FAAD_USE_ASM) && defined(__WATCOMC__) && !defined(USE_DOUBLE_PRECISION)
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

			if(bands < 32) {
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

void *FAADAPI faacDecDecode(faacDecHandle hDecoder, faacDecFrameInfo * hInfo, uint8_t * buffer, uint32_t buffer_size)
{
	int32_t i;
	uint8_t ch;
	uint8_t channels;
	uint8_t output_channels;
	uint8_t channelconfig;
	uint16_t frame_len;
	bitfile ld;
	adts_header adts;

	memset(hInfo, 0, sizeof(faacDecFrameInfo));

	faad_initbits(&ld, buffer, buffer_size);

#ifdef DRM
	if(hDecoder->object_type == DRM_ER_LC)
		faad_bits_read24(&ld, 8 DEBUGVAR(1, 1, "faacDecDecode(): skip CRC"));
#endif

	if(hDecoder->adts_header_present)
		if((hInfo->error = adts_frame(&adts, &ld)) > 0)
			goto error;

#ifdef ANALYSIS
	dbg_count = 0;
#endif

	/* decode the complete bitstream */
	raw_data_block(hDecoder, hInfo, &ld, hDecoder->spec_data, hDecoder->spec_coef, &hDecoder->pce, hDecoder->drc);

	if(hInfo->error > 0)
		goto error;

	/* no more bit reading after this */
	hInfo->bytesconsumed = bit2byte(faad_get_processed_bits(&ld));
	if(ld.error) {
		hInfo->error = 14;
		goto error;
	}

	channels = hDecoder->fr_channels;
	frame_len = hDecoder->frameLength;

	channelconfig = hDecoder->channelConfiguration;

	if(channels != hDecoder->channelConfiguration)
		channelconfig = channels;

	if(channels == 8)			/* 7.1 */
		channelconfig = 7;
	if(channels == 7)			/* not a standard channelConfiguration */
		channelconfig = 0;

	if((channels == 5 || channels == 6) && hDecoder->config.downMatrix) {
		hDecoder->downMatrix = 1;
		output_channels = 2;
	} else {
		output_channels = channels;
	}

	create_channel_config(hDecoder, hInfo, channelconfig);

	hInfo->samples = frame_len * output_channels;
	hInfo->channels = output_channels;
	hInfo->samplerate = sample_rates[hDecoder->sf_index];

	if(channels == 0) {
		hDecoder->frame++;
		return NULL;
	}

	for(ch = 0; ch < channels; ch++) {
		ic_stream *ics;

		/* find the syntax element to which this channel belongs */
		if(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->channel == ch)
			ics = &(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->ics1);
		else if(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->paired_channel == ch)
			ics = &(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->ics2);

		//inverse_quantization(hDecoder->spec_coef[ch], hDecoder->spec_data[ch], frame_len);

		//apply_scalefactors(ics, hDecoder->spec_coef[ch],hDecoder->pow2_table, frame_len);

		inverse_quant_and_apply_scalefactors(ics, hDecoder->spec_coef[ch], hDecoder->spec_data[ch], frame_len);

		/* deinterleave short block grouping */
		if(ics->window_sequence == EIGHT_SHORT_SEQUENCE)
			quant_to_spec(ics, hDecoder->spec_coef[ch], frame_len);
	}

	/* Because for ms, is and pns both channels spectral coefficients are needed
	   we have to restart running through all channels here.
	 */
	for(ch = 0; ch < channels; ch++) {
		int16_t pch = -1;
		uint8_t right_channel;
		ic_stream *ics, *icsr;
		ltp_info *ltp;

		/* find the syntax element to which this channel belongs */
		if(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->channel == ch) {
			ics = &(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->ics1);
			icsr = &(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->ics2);
			ltp = &(ics->ltp);
			pch = hDecoder->syntax_elements[hDecoder->channel_element[ch]]->paired_channel;
			right_channel = 0;
		} else if(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->paired_channel == ch) {
			ics = &(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->ics2);
			if(hDecoder->syntax_elements[hDecoder->channel_element[ch]]->common_window)
				ltp = &(ics->ltp2);
			else
				ltp = &(ics->ltp);
			right_channel = 1;
		}

		if((!right_channel) && (pch != -1) && (ics->ms_mask_present))
			pns_decode(ics, icsr, hDecoder->spec_coef[ch], hDecoder->spec_coef[pch], frame_len, 1);
		else if((pch == -1) || ((pch != -1) && (!ics->ms_mask_present)))
			pns_decode(ics, NULL, hDecoder->spec_coef[ch], NULL, frame_len, 0);

		if(!right_channel && (pch != -1)) {
			ms_decode(ics, icsr, hDecoder->spec_coef[ch], hDecoder->spec_coef[pch], frame_len);
			is_decode(ics, icsr, hDecoder->spec_coef[ch], hDecoder->spec_coef[pch], frame_len);
		}
#ifdef MAIN_DEC
		if((hDecoder->object_type == MAIN) && hDecoder->pred_stat) {
			ic_prediction(ics, hDecoder->spec_coef[ch], hDecoder->pred_stat[ch], frame_len);
			pns_reset_pred_state(ics, hDecoder->pred_stat[ch]);
		}
#endif
#ifdef LTP_DEC
		if(((hDecoder->object_type == LTP)
#ifdef ERROR_RESILIENCE
			|| (hDecoder->object_type == ER_LTP)
#endif
#ifdef LD_DEC
			|| (hDecoder->object_type == LD)
#endif
		   ) && hDecoder->ltd) {
#ifdef LD_DEC
			if(hDecoder->object_type == LD) {
				if(ltp->data_present) {
					if(ltp->lag_update)
						hDecoder->ltp_lag[ch] = ltp->lag;
				}
				ltp->lag = hDecoder->ltp_lag[ch];
			}
#endif

			lt_prediction(hDecoder->ltd, ics, ltp, hDecoder->spec_coef[ch],
						  hDecoder->lt_pred_stat[ch], hDecoder->fb, ics->window_shape, hDecoder->window_shape_prev[ch], hDecoder->sf_index, hDecoder->object_type, frame_len);
		}
#endif

		tns_decode_frame(ics, &(ics->tns), hDecoder->sf_index, hDecoder->object_type, hDecoder->spec_coef[ch], frame_len);

		if(hDecoder->drc->present)
			if(!hDecoder->drc->exclude_mask[ch] || !hDecoder->drc->excluded_chns_present)
				drc_decode(hDecoder->drc, hDecoder->spec_coef[ch]);

#ifdef AAC_SPECTRUM_ANALISER
		if((displaymode & 16) && (displaymode & 32)) {
			if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
				real_t *sc = hDecoder->spec_coef[ch];
				unsigned int bc;
				for(bc = 0; bc < 8; bc++) {
					aac_calculate_analiser_bands(sc, 2 * frame_len / 8, (ch == (hDecoder->fr_channels - 1)), 0.032);
					sc += frame_len / 8;
				}
			} else
				aac_calculate_analiser_bands(hDecoder->spec_coef[ch], 2 * frame_len, (ch == (hDecoder->fr_channels - 1)), 0.004);
		}
#endif


#ifdef SSR_DEC
		if(hDecoder->object_type == SSR) {
			ssr_decode(&(ics->ssr), hDecoder->fb, ics->window_sequence, ics->window_shape,
					   hDecoder->window_shape_prev[ch], hDecoder->spec_coef[ch], hDecoder->time_out[ch], hDecoder->ssr_overlap[ch], hDecoder->ipqf_buffer[ch], hDecoder->prev_fmd[ch], frame_len);
		} else
#endif
			ifilter_bank(hDecoder->fb, ics->window_sequence, ics->window_shape, hDecoder->window_shape_prev[ch], hDecoder->spec_coef[ch], hDecoder->time_out[ch], hDecoder->object_type, frame_len);

		hDecoder->window_shape_prev[ch] = ics->window_shape;

#ifdef LTP_DEC
		if(((hDecoder->object_type == LTP)
#ifdef ERROR_RESILIENCE
			|| (hDecoder->object_type == ER_LTP)
#endif
#ifdef LD_DEC
			|| (hDecoder->object_type == LD)
#endif
		   ) && hDecoder->lt_pred_stat) {
			lt_update_state(hDecoder->lt_pred_stat[ch], hDecoder->time_out[ch], hDecoder->time_out[ch] + frame_len, frame_len, hDecoder->object_type);
		}
#endif
	}

#ifdef SBR_DEC
	if((hDecoder->sbr_present_flag == 1) && hDecoder->sbr) {
		for(i = 0; i < hDecoder->fr_ch_ele; i++) {
			if(hDecoder->syntax_elements[i]->paired_channel != -1) {
				memcpy(hDecoder->time_out2[hDecoder->syntax_elements[i]->channel], hDecoder->time_out[hDecoder->syntax_elements[i]->channel], frame_len * sizeof(real_t));
				memcpy(hDecoder->time_out2[hDecoder->syntax_elements[i]->paired_channel], hDecoder->time_out[hDecoder->syntax_elements[i]->paired_channel], frame_len * sizeof(real_t));
				sbrDecodeFrame(hDecoder->sbr[i],
							   hDecoder->time_out2[hDecoder->syntax_elements[i]->channel], hDecoder->time_out2[hDecoder->syntax_elements[i]->paired_channel], ID_CPE, hDecoder->postSeekResetFlag);
			} else {
				memcpy(hDecoder->time_out2[hDecoder->syntax_elements[i]->channel], hDecoder->time_out[hDecoder->syntax_elements[i]->channel], frame_len * sizeof(real_t));
				sbrDecodeFrame(hDecoder->sbr[i], hDecoder->time_out2[hDecoder->syntax_elements[i]->channel], NULL, ID_SCE, hDecoder->postSeekResetFlag);
			}
		}
		frame_len *= 2;
		hInfo->samples *= 2;
		hInfo->samplerate *= 2;

		output_to_PCM(hDecoder, hDecoder->time_out2, hDecoder->sample_buffer, output_channels, frame_len, hDecoder->config.outputFormat);
	} else
#endif
		output_to_PCM(hDecoder, hDecoder->time_out, hDecoder->sample_buffer, output_channels, frame_len, hDecoder->config.outputFormat);

	/* gapless playback */
	if(hDecoder->samplesLeft != 0)
		hInfo->samples = hDecoder->samplesLeft * channels;

	hDecoder->samplesLeft = 0;
	hDecoder->postSeekResetFlag = 0;
	hDecoder->frame++;

#ifdef LD_DEC
	if(hDecoder->object_type != LD) {
#endif
		if(hDecoder->frame <= 1)
			hInfo->samples = 0;

#ifdef LD_DEC
	} else {
		if(hDecoder->frame <= 0)
			hInfo->samples = 0;
	}
#endif

#ifdef ANALYSIS
	fflush(stdout);
#endif

	return hDecoder->sample_buffer;

  error:

#ifdef ANALYSIS
	fflush(stdout);
#endif

	return NULL;
}

//-----------------------------------------------------------------
//seek helpers

void FAADAPI faacDecPostSeekReset(faacDecHandle hDecoder, int32_t frame)
{
	if(hDecoder) {
		hDecoder->postSeekResetFlag = 1;

		if(frame != -1)
			hDecoder->frame = frame;
	}
}

void FAADAPI faacDecReadframe(faacDecHandle hDecoder, faacDecFrameInfo * hInfo, uint8_t * buffer, uint32_t buffer_size)
{
	bitfile ld;
	adts_header adts;

#ifdef AAC_DEBUGINFO
	fprintf(stdout, "faacDecReadFrame begin frame:%d buffer:%8.8X size:%d sf:%d\n", hDecoder->frame, buffer, buffer_size, hDecoder->sf_index);
#endif

	memset(hInfo, 0, sizeof(faacDecFrameInfo));

	faad_initbits(&ld, buffer, buffer_size);

#ifdef DRM
	if(hDecoder->object_type == DRM_ER_LC) {
		faad_bits_read24(&ld, 8 DEBUGVAR(1, 1, "faacDecDecode(): skip CRC"));
	}
#endif

	if(hDecoder->adts_header_present) {
		if((hInfo->error = adts_frame(&adts, &ld)) > 0) {
#ifdef AAC_DEBUGINFO
			fprintf(stdout, "faacDecReadFrame error 1. \n");
#endif
			return;
		}
	}
#ifdef AAC_DEBUGINFO
	fprintf(stdout, "bitpos:%d \n", ld.bitpos);
#endif

	raw_data_block(hDecoder, hInfo, &ld, hDecoder->spec_data, hDecoder->spec_coef, &hDecoder->pce, hDecoder->drc);

	if(hInfo->error > 0) {
#ifdef AAC_DEBUGINFO
		fprintf(stdout, "faacDecReadFrame error 2. \n");
#endif
		return;
	}

	hInfo->bytesconsumed = bit2byte(faad_get_processed_bits(&ld));

	if(ld.error)
		hInfo->error = 14;
	hDecoder->frame++;
	hDecoder->postSeekResetFlag = 0;

	hInfo->channels = max(hDecoder->pce.channels, hDecoder->fr_channels);
	hInfo->channels = min(hInfo->channels, MAX_CHANNELS);
	hInfo->object_type = hDecoder->object_type;
	hInfo->samplerate = sample_rates[hDecoder->sf_index];
	hInfo->sbr_present_flag = hDecoder->sbr_present_flag;
	hInfo->frameLength = hDecoder->frameLength;
	if(hInfo->sbr_present_flag) {
		hInfo->frameLength *= 2;
		if(hInfo->samplerate <= 24000)
			hInfo->samplerate *= 2;	// output freq at SBR (22k->44k)
		else
			hDecoder->sf_index = get_sr_index(hInfo->samplerate / 2);
	}
	hInfo->sf_index = hDecoder->sf_index;

#ifdef AAC_DEBUGINFO
	fprintf(stdout, "faacDecReadFrame end sf:%d sfd:%d ch:%d sb:%d o:%d bc:%d er:%d\n", hInfo->samplerate, sample_rates[hDecoder->sf_index], hInfo->channels, hInfo->sbr_present_flag,
			hInfo->object_type, hInfo->bytesconsumed, hInfo->error);
#endif
}
