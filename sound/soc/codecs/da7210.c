/*
 * DA7210 ALSA Soc codec driver
 *
 * Copyright (c) 2009 Dialog Semiconductor
 * Written by David Chen <Dajun.chen@diasemi.com>
 *
 * Copyright (C) 2009 Renesas Solutions Corp.
 * Cleanups by Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * Tested on SuperH Ecovec24 board with S16/S24 LE in 48KHz using I2S
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define DA7210_CONTROL			0x01
#define DA7210_STATUS			0x02
#define DA7210_STARTUP1			0x03
#define DA7210_STARTUP2			0x04
#define DA7210_STARTUP3			0x05
#define DA7210_MIC_L			0x07
#define DA7210_MIC_R			0x08
#define DA7210_AUX1_L			0x09
#define DA7210_AUX1_R			0x0A
#define DA7210_AUX2			0x0B
#define DA7210_IN_GAIN			0x0C
#define DA7210_INMIX_L			0x0D
#define DA7210_INMIX_R			0x0E
#define DA7210_ADC_HPF			0x0F
#define DA7210_ADC			0x10
#define DA7210_ADC_EQ1_2		0X11
#define DA7210_ADC_EQ3_4		0x12
#define DA7210_ADC_EQ5			0x13
#define DA7210_DAC_HPF			0x14
#define DA7210_DAC_L			0x15
#define DA7210_DAC_R			0x16
#define DA7210_DAC_SEL			0x17
#define DA7210_SOFTMUTE			0x18
#define DA7210_DAC_EQ1_2		0x19
#define DA7210_DAC_EQ3_4		0x1A
#define DA7210_DAC_EQ5			0x1B
#define DA7210_OUTMIX_L			0x1C
#define DA7210_OUTMIX_R			0x1D
#define DA7210_OUT1_L			0x1E
#define DA7210_OUT1_R			0x1F
#define DA7210_OUT2			0x20
#define DA7210_HP_L_VOL			0x21
#define DA7210_HP_R_VOL			0x22
#define DA7210_HP_CFG			0x23
#define DA7210_ZERO_CROSS		0x24
#define DA7210_DAI_SRC_SEL		0x25
#define DA7210_DAI_CFG1			0x26
#define DA7210_DAI_CFG3			0x28
#define DA7210_PLL_DIV1			0x29
#define DA7210_PLL_DIV2			0x2A
#define DA7210_PLL_DIV3			0x2B
#define DA7210_PLL			0x2C
#define DA7210_ALC_MAX			0x83
#define DA7210_ALC_MIN			0x84
#define DA7210_ALC_NOIS			0x85
#define DA7210_ALC_ATT			0x86
#define DA7210_ALC_REL			0x87
#define DA7210_ALC_DEL			0x88
#define DA7210_A_HID_UNLOCK		0x8A
#define DA7210_A_TEST_UNLOCK		0x8B
#define DA7210_A_PLL1			0x90
#define DA7210_A_CP_MODE		0xA7

#define DA7210_SC_MST_EN		(1 << 0)

#define DA7210_MICBIAS_EN		(1 << 6)
#define DA7210_MIC_L_EN			(1 << 7)

#define DA7210_MIC_R_EN			(1 << 7)

#define DA7210_IN_L_EN			(1 << 7)

#define DA7210_IN_R_EN			(1 << 7)

#define DA7210_ADC_ALC_EN		(1 << 0)
#define DA7210_ADC_L_EN			(1 << 3)
#define DA7210_ADC_R_EN			(1 << 7)

#define DA7210_VOICE_F0_MASK		(0x7 << 4)
#define DA7210_VOICE_F0_25		(1 << 4)
#define DA7210_VOICE_EN			(1 << 7)

#define DA7210_DAC_L_SRC_DAI_L		(4 << 0)
#define DA7210_DAC_L_EN			(1 << 3)
#define DA7210_DAC_R_SRC_DAI_R		(5 << 4)
#define DA7210_DAC_R_EN			(1 << 7)

#define DA7210_OUT_L_EN			(1 << 7)

#define DA7210_OUT_R_EN			(1 << 7)

#define DA7210_HP_2CAP_MODE		(1 << 1)
#define DA7210_HP_SENSE_EN		(1 << 2)
#define DA7210_HP_L_EN			(1 << 3)
#define DA7210_HP_MODE			(1 << 6)
#define DA7210_HP_R_EN			(1 << 7)

#define DA7210_DAI_OUT_L_SRC		(6 << 0)
#define DA7210_DAI_OUT_R_SRC		(7 << 4)

#define DA7210_DAI_WORD_S16_LE		(0 << 0)
#define DA7210_DAI_WORD_S20_3LE		(1 << 0)
#define DA7210_DAI_WORD_S24_LE		(2 << 0)
#define DA7210_DAI_WORD_S32_LE		(3 << 0)
#define DA7210_DAI_FLEN_64BIT		(1 << 2)
#define DA7210_DAI_MODE_SLAVE		(0 << 7)
#define DA7210_DAI_MODE_MASTER		(1 << 7)

#define DA7210_DAI_FORMAT_I2SMODE	(0 << 0)
#define DA7210_DAI_FORMAT_LEFT_J	(1 << 0)
#define DA7210_DAI_FORMAT_RIGHT_J	(2 << 0)
#define DA7210_DAI_OE			(1 << 3)
#define DA7210_DAI_EN			(1 << 7)

#define DA7210_MCLK_RANGE_10_20_MHZ	(1 << 4)
#define DA7210_PLL_BYP			(1 << 6)

#define DA7210_PLL_FS_MASK		(0xF << 0)
#define DA7210_PLL_FS_8000		(0x1 << 0)
#define DA7210_PLL_FS_11025		(0x2 << 0)
#define DA7210_PLL_FS_12000		(0x3 << 0)
#define DA7210_PLL_FS_16000		(0x5 << 0)
#define DA7210_PLL_FS_22050		(0x6 << 0)
#define DA7210_PLL_FS_24000		(0x7 << 0)
#define DA7210_PLL_FS_32000		(0x9 << 0)
#define DA7210_PLL_FS_44100		(0xA << 0)
#define DA7210_PLL_FS_48000		(0xB << 0)
#define DA7210_PLL_FS_88200		(0xE << 0)
#define DA7210_PLL_FS_96000		(0xF << 0)
#define DA7210_PLL_EN			(0x1 << 7)

#define DA7210_RAMP_EN			(1 << 6)

#define DA7210_NOISE_SUP_EN		(1 << 3)

#define DA7210_INPGA_L_VOL		(0x0F << 0)
#define DA7210_INPGA_R_VOL		(0xF0 << 0)

#define DA7210_AUX1_L_ZC		(1 << 0)
#define DA7210_AUX1_R_ZC		(1 << 1)
#define DA7210_HP_L_ZC			(1 << 6)
#define DA7210_HP_R_ZC			(1 << 7)

#define DA7210_AUX1_L_VOL		(0x3F << 0)
#define DA7210_AUX1_L_EN		(1 << 7)

#define DA7210_AUX1_R_VOL		(0x3F << 0)
#define DA7210_AUX1_R_EN		(1 << 7)

#define DA7210_AUX2_EN			(1 << 3)

#define DA7210_INPGA_MIN_VOL_NS		0x0A  
#define DA7210_AUX1_MIN_VOL_NS		0x35  

#define DA7210_OUT1_L_EN		(1 << 7)

#define DA7210_OUT1_R_EN		(1 << 7)

#define DA7210_OUT2_OUTMIX_R		(1 << 5)
#define DA7210_OUT2_OUTMIX_L		(1 << 6)
#define DA7210_OUT2_EN			(1 << 7)

#define DA7210_VERSION "0.0.1"

static const unsigned int hp_out_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x0, 0x10, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
	
	0x11, 0x3f, TLV_DB_SCALE_ITEM(-5400, 150, 0),
};

static const unsigned int lineout_vol_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x0, 0x10, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
	
	0x11, 0x3f, TLV_DB_SCALE_ITEM(-5400, 150, 0)
};

static const unsigned int mono_vol_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x0, 0x2, TLV_DB_SCALE_ITEM(-1800, 0, 1),
	
	0x3, 0x7, TLV_DB_SCALE_ITEM(-1800, 600, 0)
};

static const unsigned int aux1_vol_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x0, 0x10, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
	
	0x11, 0x3f, TLV_DB_SCALE_ITEM(-4800, 150, 0)
};

static const DECLARE_TLV_DB_SCALE(eq_gain_tlv, -1050, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_eq_master_gain_tlv, -1800, 600, 1);
static const DECLARE_TLV_DB_SCALE(dac_gain_tlv, -7725, 75, 0);
static const DECLARE_TLV_DB_SCALE(mic_vol_tlv, -600, 600, 0);
static const DECLARE_TLV_DB_SCALE(aux2_vol_tlv, -600, 600, 0);
static const DECLARE_TLV_DB_SCALE(inpga_gain_tlv, -450, 150, 0);

static const char * const da7210_hpf_cutoff_txt[] = {
	"Fs/8192*pi", "Fs/4096*pi", "Fs/2048*pi", "Fs/1024*pi"
};

static const struct soc_enum da7210_dac_hpf_cutoff =
	SOC_ENUM_SINGLE(DA7210_DAC_HPF, 0, 4, da7210_hpf_cutoff_txt);

static const struct soc_enum da7210_adc_hpf_cutoff =
	SOC_ENUM_SINGLE(DA7210_ADC_HPF, 0, 4, da7210_hpf_cutoff_txt);

static const char * const da7210_vf_cutoff_txt[] = {
	"2.5Hz", "25Hz", "50Hz", "100Hz", "150Hz", "200Hz", "300Hz", "400Hz"
};

static const struct soc_enum da7210_dac_vf_cutoff =
	SOC_ENUM_SINGLE(DA7210_DAC_HPF, 4, 8, da7210_vf_cutoff_txt);

static const struct soc_enum da7210_adc_vf_cutoff =
	SOC_ENUM_SINGLE(DA7210_ADC_HPF, 4, 8, da7210_vf_cutoff_txt);

static const char *da7210_hp_mode_txt[] = {
	"Class H", "Class G"
};

static const struct soc_enum da7210_hp_mode_sel =
	SOC_ENUM_SINGLE(DA7210_HP_CFG, 0, 2, da7210_hp_mode_txt);

static int da7210_put_alc_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (ucontrol->value.integer.value[0]) {
		
		if (snd_soc_read(codec, DA7210_CONTROL) & DA7210_NOISE_SUP_EN) {
			dev_dbg(codec->dev,
				"Disable noise suppression to enable ALC\n");
			return -EINVAL;
		}
	}
	
	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int da7210_put_noise_sup_sw(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u8 val;

	if (ucontrol->value.integer.value[0]) {
		
		if (snd_soc_read(codec, DA7210_ADC) & DA7210_ADC_ALC_EN)
			goto err;

		
		if ((snd_soc_read(codec, DA7210_ZERO_CROSS) &
			(DA7210_AUX1_L_ZC | DA7210_AUX1_R_ZC | DA7210_HP_L_ZC |
			DA7210_HP_R_ZC)) != (DA7210_AUX1_L_ZC |
			DA7210_AUX1_R_ZC | DA7210_HP_L_ZC | DA7210_HP_R_ZC))
			goto err;

		
		val = snd_soc_read(codec, DA7210_IN_GAIN);
		if (((val & DA7210_INPGA_L_VOL) < DA7210_INPGA_MIN_VOL_NS) ||
			(((val & DA7210_INPGA_R_VOL) >> 4) <
			DA7210_INPGA_MIN_VOL_NS))
			goto err;

		
		if (((snd_soc_read(codec, DA7210_AUX1_L) & DA7210_AUX1_L_VOL) <
		    DA7210_AUX1_MIN_VOL_NS) ||
		    ((snd_soc_read(codec, DA7210_AUX1_R) & DA7210_AUX1_R_VOL) <
		    DA7210_AUX1_MIN_VOL_NS))
			goto err;
	}
	
	return snd_soc_put_volsw(kcontrol, ucontrol);

err:
	return -EINVAL;
}

static const struct snd_kcontrol_new da7210_snd_controls[] = {

	SOC_DOUBLE_R_TLV("HeadPhone Playback Volume",
			 DA7210_HP_L_VOL, DA7210_HP_R_VOL,
			 0, 0x3F, 0, hp_out_tlv),
	SOC_DOUBLE_R_TLV("Digital Playback Volume",
			 DA7210_DAC_L, DA7210_DAC_R,
			 0, 0x77, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("Lineout Playback Volume",
			 DA7210_OUT1_L, DA7210_OUT1_R,
			 0, 0x3f, 0, lineout_vol_tlv),
	SOC_SINGLE_TLV("Mono Playback Volume", DA7210_OUT2, 0, 0x7, 0,
		       mono_vol_tlv),

	SOC_DOUBLE_R_TLV("Mic Capture Volume",
			 DA7210_MIC_L, DA7210_MIC_R,
			 0, 0x5, 0, mic_vol_tlv),
	SOC_DOUBLE_R_TLV("Aux1 Capture Volume",
			 DA7210_AUX1_L, DA7210_AUX1_R,
			 0, 0x3f, 0, aux1_vol_tlv),
	SOC_SINGLE_TLV("Aux2 Capture Volume", DA7210_AUX2, 0, 0x3, 0,
		       aux2_vol_tlv),
	SOC_DOUBLE_TLV("In PGA Capture Volume", DA7210_IN_GAIN, 0, 4, 0xF, 0,
		       inpga_gain_tlv),

	
	SOC_SINGLE("DAC EQ Switch", DA7210_DAC_EQ5, 7, 1, 0),
	SOC_SINGLE_TLV("DAC EQ1 Volume", DA7210_DAC_EQ1_2, 0, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ2 Volume", DA7210_DAC_EQ1_2, 4, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ3 Volume", DA7210_DAC_EQ3_4, 0, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ4 Volume", DA7210_DAC_EQ3_4, 4, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ5 Volume", DA7210_DAC_EQ5, 0, 0xf, 1,
		       eq_gain_tlv),

	
	SOC_SINGLE("ADC EQ Switch", DA7210_ADC_EQ5, 7, 1, 0),
	SOC_SINGLE_TLV("ADC EQ Master Volume", DA7210_ADC_EQ5, 4, 0x3,
		       1, adc_eq_master_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ1 Volume", DA7210_ADC_EQ1_2, 0, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ2 Volume", DA7210_ADC_EQ1_2, 4, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ3 Volume", DA7210_ADC_EQ3_4, 0, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ4 Volume", DA7210_ADC_EQ3_4, 4, 0xf, 1,
		       eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ5 Volume", DA7210_ADC_EQ5, 0, 0xf, 1,
		       eq_gain_tlv),

	SOC_SINGLE("DAC HPF Switch", DA7210_DAC_HPF, 3, 1, 0),
	SOC_ENUM("DAC HPF Cutoff", da7210_dac_hpf_cutoff),
	SOC_SINGLE("DAC Voice Mode Switch", DA7210_DAC_HPF, 7, 1, 0),
	SOC_ENUM("DAC Voice Cutoff", da7210_dac_vf_cutoff),

	SOC_SINGLE("ADC HPF Switch", DA7210_ADC_HPF, 3, 1, 0),
	SOC_ENUM("ADC HPF Cutoff", da7210_adc_hpf_cutoff),
	SOC_SINGLE("ADC Voice Mode Switch", DA7210_ADC_HPF, 7, 1, 0),
	SOC_ENUM("ADC Voice Cutoff", da7210_adc_vf_cutoff),

	
	SOC_DOUBLE_R("Mic Capture Switch", DA7210_MIC_L, DA7210_MIC_R, 3, 1, 0),
	SOC_SINGLE("Aux2 Capture Switch", DA7210_AUX2, 2, 1, 0),
	SOC_DOUBLE("ADC Capture Switch", DA7210_ADC, 2, 6, 1, 0),
	SOC_SINGLE("Digital Soft Mute Switch", DA7210_SOFTMUTE, 7, 1, 0),
	SOC_SINGLE("Digital Soft Mute Rate", DA7210_SOFTMUTE, 0, 0x7, 0),

	
	SOC_DOUBLE("Aux1 ZC Switch", DA7210_ZERO_CROSS, 0, 1, 1, 0),
	SOC_DOUBLE("In PGA ZC Switch", DA7210_ZERO_CROSS, 2, 3, 1, 0),
	SOC_DOUBLE("Lineout ZC Switch", DA7210_ZERO_CROSS, 4, 5, 1, 0),
	SOC_DOUBLE("Headphone ZC Switch", DA7210_ZERO_CROSS, 6, 7, 1, 0),

	SOC_ENUM("Headphone Class", da7210_hp_mode_sel),

	
	SOC_SINGLE_EXT("ALC Enable Switch", DA7210_ADC, 0, 1, 0,
		       snd_soc_get_volsw, da7210_put_alc_sw),
	SOC_SINGLE("ALC Capture Max Volume", DA7210_ALC_MAX, 0, 0x3F, 0),
	SOC_SINGLE("ALC Capture Min Volume", DA7210_ALC_MIN, 0, 0x3F, 0),
	SOC_SINGLE("ALC Capture Noise Volume", DA7210_ALC_NOIS, 0, 0x3F, 0),
	SOC_SINGLE("ALC Capture Attack Rate", DA7210_ALC_ATT, 0, 0xFF, 0),
	SOC_SINGLE("ALC Capture Release Rate", DA7210_ALC_REL, 0, 0xFF, 0),
	SOC_SINGLE("ALC Capture Release Delay", DA7210_ALC_DEL, 0, 0xFF, 0),

	SOC_SINGLE_EXT("Noise Suppression Enable Switch", DA7210_CONTROL, 3, 1,
		       0, snd_soc_get_volsw, da7210_put_noise_sup_sw),
};

static const struct snd_kcontrol_new da7210_dapm_inmixl_controls[] = {
	SOC_DAPM_SINGLE("Mic Left Switch", DA7210_INMIX_L, 0, 1, 0),
	SOC_DAPM_SINGLE("Mic Right Switch", DA7210_INMIX_L, 1, 1, 0),
	SOC_DAPM_SINGLE("Aux1 Left Switch", DA7210_INMIX_L, 2, 1, 0),
	SOC_DAPM_SINGLE("Aux2 Switch", DA7210_INMIX_L, 3, 1, 0),
	SOC_DAPM_SINGLE("Outmix Left Switch", DA7210_INMIX_L, 4, 1, 0),
};

static const struct snd_kcontrol_new da7210_dapm_inmixr_controls[] = {
	SOC_DAPM_SINGLE("Mic Right Switch", DA7210_INMIX_R, 0, 1, 0),
	SOC_DAPM_SINGLE("Mic Left Switch", DA7210_INMIX_R, 1, 1, 0),
	SOC_DAPM_SINGLE("Aux1 Right Switch", DA7210_INMIX_R, 2, 1, 0),
	SOC_DAPM_SINGLE("Aux2 Switch", DA7210_INMIX_R, 3, 1, 0),
	SOC_DAPM_SINGLE("Outmix Right Switch", DA7210_INMIX_R, 4, 1, 0),
};

static const struct snd_kcontrol_new da7210_dapm_outmixl_controls[] = {
	SOC_DAPM_SINGLE("Aux1 Left Switch", DA7210_OUTMIX_L, 0, 1, 0),
	SOC_DAPM_SINGLE("Aux2 Switch", DA7210_OUTMIX_L, 1, 1, 0),
	SOC_DAPM_SINGLE("INPGA Left Switch", DA7210_OUTMIX_L, 2, 1, 0),
	SOC_DAPM_SINGLE("INPGA Right Switch", DA7210_OUTMIX_L, 3, 1, 0),
	SOC_DAPM_SINGLE("DAC Left Switch", DA7210_OUTMIX_L, 4, 1, 0),
};

static const struct snd_kcontrol_new da7210_dapm_outmixr_controls[] = {
	SOC_DAPM_SINGLE("Aux1 Right Switch", DA7210_OUTMIX_R, 0, 1, 0),
	SOC_DAPM_SINGLE("Aux2 Switch", DA7210_OUTMIX_R, 1, 1, 0),
	SOC_DAPM_SINGLE("INPGA Left Switch", DA7210_OUTMIX_R, 2, 1, 0),
	SOC_DAPM_SINGLE("INPGA Right Switch", DA7210_OUTMIX_R, 3, 1, 0),
	SOC_DAPM_SINGLE("DAC Right Switch", DA7210_OUTMIX_R, 4, 1, 0),
};

static const struct snd_kcontrol_new da7210_dapm_monomix_controls[] = {
	SOC_DAPM_SINGLE("INPGA Right Switch", DA7210_OUT2, 3, 1, 0),
	SOC_DAPM_SINGLE("INPGA Left Switch", DA7210_OUT2, 4, 1, 0),
	SOC_DAPM_SINGLE("Outmix Right Switch", DA7210_OUT2, 5, 1, 0),
	SOC_DAPM_SINGLE("Outmix Left Switch", DA7210_OUT2, 6, 1, 0),
};

static const struct snd_soc_dapm_widget da7210_dapm_widgets[] = {
	
	
	SND_SOC_DAPM_INPUT("MICL"),
	SND_SOC_DAPM_INPUT("MICR"),
	SND_SOC_DAPM_INPUT("AUX1L"),
	SND_SOC_DAPM_INPUT("AUX1R"),
	SND_SOC_DAPM_INPUT("AUX2"),

	
	SND_SOC_DAPM_PGA("Mic Left", DA7210_STARTUP3, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Mic Right", DA7210_STARTUP3, 1, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Aux1 Left", DA7210_STARTUP3, 2, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Aux1 Right", DA7210_STARTUP3, 3, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Aux2 Mono", DA7210_STARTUP3, 4, 1, NULL, 0),

	SND_SOC_DAPM_PGA("INPGA Left", DA7210_INMIX_L, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INPGA Right", DA7210_INMIX_R, 7, 0, NULL, 0),

	
	SND_SOC_DAPM_SUPPLY("Mic Bias", DA7210_MIC_L, 6, 0, NULL, 0),

	
	SND_SOC_DAPM_MIXER("In Mixer Left", SND_SOC_NOPM, 0, 0,
		&da7210_dapm_inmixl_controls[0],
		ARRAY_SIZE(da7210_dapm_inmixl_controls)),

	SND_SOC_DAPM_MIXER("In Mixer Right", SND_SOC_NOPM, 0, 0,
		&da7210_dapm_inmixr_controls[0],
		ARRAY_SIZE(da7210_dapm_inmixr_controls)),

	
	SND_SOC_DAPM_ADC("ADC Left", "Capture", DA7210_STARTUP3, 5, 1),
	SND_SOC_DAPM_ADC("ADC Right", "Capture", DA7210_STARTUP3, 6, 1),

	
	
	SND_SOC_DAPM_DAC("DAC Left", "Playback", DA7210_STARTUP2, 5, 1),
	SND_SOC_DAPM_DAC("DAC Right", "Playback", DA7210_STARTUP2, 6, 1),

	
	SND_SOC_DAPM_MIXER("Out Mixer Left", SND_SOC_NOPM, 0, 0,
		&da7210_dapm_outmixl_controls[0],
		ARRAY_SIZE(da7210_dapm_outmixl_controls)),

	SND_SOC_DAPM_MIXER("Out Mixer Right", SND_SOC_NOPM, 0, 0,
		&da7210_dapm_outmixr_controls[0],
		ARRAY_SIZE(da7210_dapm_outmixr_controls)),

	SND_SOC_DAPM_MIXER("Mono Mixer", SND_SOC_NOPM, 0, 0,
		&da7210_dapm_monomix_controls[0],
		ARRAY_SIZE(da7210_dapm_monomix_controls)),

	
	SND_SOC_DAPM_PGA("OUTPGA Left Enable", DA7210_OUTMIX_L, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("OUTPGA Right Enable", DA7210_OUTMIX_R, 7, 0, NULL, 0),

	SND_SOC_DAPM_PGA("Out1 Left", DA7210_STARTUP2, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Out1 Right", DA7210_STARTUP2, 1, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Out2 Mono", DA7210_STARTUP2, 2, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Headphone Left", DA7210_STARTUP2, 3, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Headphone Right", DA7210_STARTUP2, 4, 1, NULL, 0),

	
	SND_SOC_DAPM_OUTPUT("OUT1L"),
	SND_SOC_DAPM_OUTPUT("OUT1R"),
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("OUT2"),
};

static const struct snd_soc_dapm_route da7210_audio_map[] = {
	
	
	{"Mic Left", NULL, "MICL"},
	{"Mic Right", NULL, "MICR"},
	{"Aux1 Left", NULL, "AUX1L"},
	{"Aux1 Right", NULL, "AUX1R"},
	{"Aux2 Mono", NULL, "AUX2"},

	{"In Mixer Left", "Mic Left Switch", "Mic Left"},
	{"In Mixer Left", "Mic Right Switch", "Mic Right"},
	{"In Mixer Left", "Aux1 Left Switch", "Aux1 Left"},
	{"In Mixer Left", "Aux2 Switch", "Aux2 Mono"},
	{"In Mixer Left", "Outmix Left Switch", "Out Mixer Left"},

	{"In Mixer Right", "Mic Right Switch", "Mic Right"},
	{"In Mixer Right", "Mic Left Switch", "Mic Left"},
	{"In Mixer Right", "Aux1 Right Switch", "Aux1 Right"},
	{"In Mixer Right", "Aux2 Switch", "Aux2 Mono"},
	{"In Mixer Right", "Outmix Right Switch", "Out Mixer Right"},

	{"INPGA Left", NULL, "In Mixer Left"},
	{"ADC Left", NULL, "INPGA Left"},

	{"INPGA Right", NULL, "In Mixer Right"},
	{"ADC Right", NULL, "INPGA Right"},

	
	{"Out Mixer Left", "Aux1 Left Switch", "Aux1 Left"},
	{"Out Mixer Left", "Aux2 Switch", "Aux2 Mono"},
	{"Out Mixer Left", "INPGA Left Switch", "INPGA Left"},
	{"Out Mixer Left", "INPGA Right Switch", "INPGA Right"},
	{"Out Mixer Left", "DAC Left Switch", "DAC Left"},

	{"Out Mixer Right", "Aux1 Right Switch", "Aux1 Right"},
	{"Out Mixer Right", "Aux2 Switch", "Aux2 Mono"},
	{"Out Mixer Right", "INPGA Right Switch", "INPGA Right"},
	{"Out Mixer Right", "INPGA Left Switch", "INPGA Left"},
	{"Out Mixer Right", "DAC Right Switch", "DAC Right"},

	{"Mono Mixer", "INPGA Right Switch", "INPGA Right"},
	{"Mono Mixer", "INPGA Left Switch", "INPGA Left"},
	{"Mono Mixer", "Outmix Right Switch", "Out Mixer Right"},
	{"Mono Mixer", "Outmix Left Switch", "Out Mixer Left"},

	{"OUTPGA Left Enable", NULL, "Out Mixer Left"},
	{"OUTPGA Right Enable", NULL, "Out Mixer Right"},

	{"Out1 Left", NULL, "OUTPGA Left Enable"},
	{"OUT1L", NULL, "Out1 Left"},

	{"Out1 Right", NULL, "OUTPGA Right Enable"},
	{"OUT1R", NULL, "Out1 Right"},

	{"Headphone Left", NULL, "OUTPGA Left Enable"},
	{"HPL", NULL, "Headphone Left"},

	{"Headphone Right", NULL, "OUTPGA Right Enable"},
	{"HPR", NULL, "Headphone Right"},

	{"Out2 Mono", NULL, "Mono Mixer"},
	{"OUT2", NULL, "Out2 Mono"},
};

struct da7210_priv {
	struct regmap *regmap;
};

static struct reg_default da7210_reg_defaults[] = {
	{ 0x01, 0x11 },
	{ 0x03, 0x00 },
	{ 0x04, 0x00 },
	{ 0x05, 0x00 },
	{ 0x06, 0x00 },
	{ 0x07, 0x00 },
	{ 0x08, 0x00 },
	{ 0x09, 0x00 },
	{ 0x0a, 0x00 },
	{ 0x0b, 0x00 },
	{ 0x0c, 0x00 },
	{ 0x0d, 0x00 },
	{ 0x0e, 0x00 },
	{ 0x0f, 0x08 },
	{ 0x10, 0x00 },
	{ 0x11, 0x00 },
	{ 0x12, 0x00 },
	{ 0x13, 0x00 },
	{ 0x14, 0x08 },
	{ 0x15, 0x10 },
	{ 0x16, 0x10 },
	{ 0x17, 0x54 },
	{ 0x18, 0x40 },
	{ 0x19, 0x00 },
	{ 0x1a, 0x00 },
	{ 0x1b, 0x00 },
	{ 0x1c, 0x00 },
	{ 0x1d, 0x00 },
	{ 0x1e, 0x00 },
	{ 0x1f, 0x00 },
	{ 0x20, 0x00 },
	{ 0x21, 0x00 },
	{ 0x22, 0x00 },
	{ 0x23, 0x02 },
	{ 0x24, 0x00 },
	{ 0x25, 0x76 },
	{ 0x26, 0x00 },
	{ 0x27, 0x00 },
	{ 0x28, 0x04 },
	{ 0x29, 0x00 },
	{ 0x2a, 0x00 },
	{ 0x2b, 0x30 },
	{ 0x2c, 0x2A },
	{ 0x83, 0x00 },
	{ 0x84, 0x00 },
	{ 0x85, 0x00 },
	{ 0x86, 0x00 },
	{ 0x87, 0x00 },
	{ 0x88, 0x00 },
};

static bool da7210_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case DA7210_A_HID_UNLOCK:
	case DA7210_A_TEST_UNLOCK:
	case DA7210_A_PLL1:
	case DA7210_A_CP_MODE:
		return false;
	default:
		return true;
	}
}

static bool da7210_volatile_register(struct device *dev,
				    unsigned int reg)
{
	switch (reg) {
	case DA7210_STATUS:
		return true;
	default:
		return false;
	}
}

static int da7210_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	u32 dai_cfg1;
	u32 fs, bypass;

	
	snd_soc_write(codec, DA7210_DAI_SRC_SEL,
		     DA7210_DAI_OUT_R_SRC | DA7210_DAI_OUT_L_SRC);

	
	snd_soc_write(codec, DA7210_DAI_CFG3, DA7210_DAI_OE | DA7210_DAI_EN);

	dai_cfg1 = 0xFC & snd_soc_read(codec, DA7210_DAI_CFG1);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dai_cfg1 |= DA7210_DAI_WORD_S16_LE;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		dai_cfg1 |= DA7210_DAI_WORD_S20_3LE;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dai_cfg1 |= DA7210_DAI_WORD_S24_LE;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dai_cfg1 |= DA7210_DAI_WORD_S32_LE;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, DA7210_DAI_CFG1, dai_cfg1);

	switch (params_rate(params)) {
	case 8000:
		fs		= DA7210_PLL_FS_8000;
		bypass		= DA7210_PLL_BYP;
		break;
	case 11025:
		fs		= DA7210_PLL_FS_11025;
		bypass		= 0;
		break;
	case 12000:
		fs		= DA7210_PLL_FS_12000;
		bypass		= DA7210_PLL_BYP;
		break;
	case 16000:
		fs		= DA7210_PLL_FS_16000;
		bypass		= DA7210_PLL_BYP;
		break;
	case 22050:
		fs		= DA7210_PLL_FS_22050;
		bypass		= 0;
		break;
	case 32000:
		fs		= DA7210_PLL_FS_32000;
		bypass		= DA7210_PLL_BYP;
		break;
	case 44100:
		fs		= DA7210_PLL_FS_44100;
		bypass		= 0;
		break;
	case 48000:
		fs		= DA7210_PLL_FS_48000;
		bypass		= DA7210_PLL_BYP;
		break;
	case 88200:
		fs		= DA7210_PLL_FS_88200;
		bypass		= 0;
		break;
	case 96000:
		fs		= DA7210_PLL_FS_96000;
		bypass		= DA7210_PLL_BYP;
		break;
	default:
		return -EINVAL;
	}

	
	snd_soc_update_bits(codec, DA7210_STARTUP1, DA7210_SC_MST_EN, 0);

	snd_soc_update_bits(codec, DA7210_PLL, DA7210_PLL_FS_MASK, fs);
	snd_soc_update_bits(codec, DA7210_PLL_DIV3, DA7210_PLL_BYP, bypass);

	
	snd_soc_update_bits(codec, DA7210_STARTUP1,
			    DA7210_SC_MST_EN, DA7210_SC_MST_EN);

	return 0;
}

static int da7210_set_dai_fmt(struct snd_soc_dai *codec_dai, u32 fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u32 dai_cfg1;
	u32 dai_cfg3;

	dai_cfg1 = 0x7f & snd_soc_read(codec, DA7210_DAI_CFG1);
	dai_cfg3 = 0xfc & snd_soc_read(codec, DA7210_DAI_CFG3);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		dai_cfg1 |= DA7210_DAI_MODE_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		dai_cfg1 |= DA7210_DAI_MODE_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		dai_cfg3 |= DA7210_DAI_FORMAT_I2SMODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		dai_cfg3 |= DA7210_DAI_FORMAT_LEFT_J;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		dai_cfg3 |= DA7210_DAI_FORMAT_RIGHT_J;
		break;
	default:
		return -EINVAL;
	}

	dai_cfg1 |= DA7210_DAI_FLEN_64BIT;

	snd_soc_write(codec, DA7210_DAI_CFG1, dai_cfg1);
	snd_soc_write(codec, DA7210_DAI_CFG3, dai_cfg3);

	return 0;
}

static int da7210_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 mute_reg = snd_soc_read(codec, DA7210_DAC_HPF) & 0xFB;

	if (mute)
		snd_soc_write(codec, DA7210_DAC_HPF, mute_reg | 0x4);
	else
		snd_soc_write(codec, DA7210_DAC_HPF, mute_reg);
	return 0;
}

#define DA7210_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops da7210_dai_ops = {
	.hw_params	= da7210_hw_params,
	.set_fmt	= da7210_set_dai_fmt,
	.digital_mute	= da7210_mute,
};

static struct snd_soc_dai_driver da7210_dai = {
	.name = "da7210-hifi",
	
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = DA7210_FORMATS,
	},
	
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = DA7210_FORMATS,
	},
	.ops = &da7210_dai_ops,
	.symmetric_rates = 1,
};

static int da7210_probe(struct snd_soc_codec *codec)
{
	struct da7210_priv *da7210 = snd_soc_codec_get_drvdata(codec);
	int ret;

	dev_info(codec->dev, "DA7210 Audio Codec %s\n", DA7210_VERSION);

	codec->control_data = da7210->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}


	snd_soc_write(codec, DA7210_STARTUP1, 0);
	snd_soc_write(codec, DA7210_PLL_DIV3,
		     DA7210_MCLK_RANGE_10_20_MHZ | DA7210_PLL_BYP);


	
	snd_soc_write(codec, DA7210_MIC_L, DA7210_MIC_L_EN | DA7210_MICBIAS_EN);
	snd_soc_write(codec, DA7210_MIC_R, DA7210_MIC_R_EN);

	
	snd_soc_write(codec, DA7210_INMIX_L, DA7210_IN_L_EN);
	snd_soc_write(codec, DA7210_INMIX_R, DA7210_IN_R_EN);

	
	snd_soc_write(codec, DA7210_ADC, DA7210_ADC_L_EN | DA7210_ADC_R_EN);


	
	snd_soc_write(codec, DA7210_DAC_SEL,
		     DA7210_DAC_L_SRC_DAI_L | DA7210_DAC_L_EN |
		     DA7210_DAC_R_SRC_DAI_R | DA7210_DAC_R_EN);

	
	snd_soc_write(codec, DA7210_OUTMIX_L, DA7210_OUT_L_EN);
	snd_soc_write(codec, DA7210_OUTMIX_R, DA7210_OUT_R_EN);

	
	snd_soc_write(codec, DA7210_HP_CFG,
		     DA7210_HP_2CAP_MODE | DA7210_HP_SENSE_EN |
		     DA7210_HP_L_EN | DA7210_HP_MODE | DA7210_HP_R_EN);

	
	snd_soc_write(codec, DA7210_SOFTMUTE, DA7210_RAMP_EN);


	
	snd_soc_write(codec, DA7210_OUT1_L, DA7210_OUT1_L_EN);
	snd_soc_write(codec, DA7210_OUT1_R, DA7210_OUT1_R_EN);
	snd_soc_write(codec, DA7210_OUT2, DA7210_OUT2_EN |
		     DA7210_OUT2_OUTMIX_L | DA7210_OUT2_OUTMIX_R);

	
	snd_soc_write(codec, DA7210_AUX1_L, DA7210_AUX1_L_EN);
	snd_soc_write(codec, DA7210_AUX1_R, DA7210_AUX1_R_EN);
	
	snd_soc_write(codec, DA7210_AUX2, DA7210_AUX2_EN);

	
	snd_soc_write(codec, DA7210_PLL, DA7210_PLL_FS_48000);

	snd_soc_write(codec, DA7210_PLL_DIV1, 0xE5); 
	snd_soc_write(codec, DA7210_PLL_DIV2, 0x99);
	snd_soc_write(codec, DA7210_PLL_DIV3, 0x0A |
		     DA7210_MCLK_RANGE_10_20_MHZ | DA7210_PLL_BYP);
	snd_soc_update_bits(codec, DA7210_PLL, DA7210_PLL_EN, DA7210_PLL_EN);

	
	
	regmap_write(da7210->regmap, DA7210_A_HID_UNLOCK,	0x8B);
	regmap_write(da7210->regmap, DA7210_A_TEST_UNLOCK,	0xB4);
	regmap_write(da7210->regmap, DA7210_A_PLL1,		0x01);
	regmap_write(da7210->regmap, DA7210_A_CP_MODE,		0x7C);
	
	regmap_write(da7210->regmap, DA7210_A_HID_UNLOCK,	0x00);
	regmap_write(da7210->regmap, DA7210_A_TEST_UNLOCK,	0x00);

	
	snd_soc_write(codec, DA7210_STARTUP1, DA7210_SC_MST_EN);

	dev_info(codec->dev, "DA7210 Audio Codec %s\n", DA7210_VERSION);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_da7210 = {
	.probe			= da7210_probe,

	.controls		= da7210_snd_controls,
	.num_controls		= ARRAY_SIZE(da7210_snd_controls),

	.dapm_widgets		= da7210_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(da7210_dapm_widgets),
	.dapm_routes		= da7210_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(da7210_audio_map),
};

static struct regmap_config da7210_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.reg_defaults = da7210_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(da7210_reg_defaults),
	.volatile_reg = da7210_volatile_register,
	.readable_reg = da7210_readable_register,
	.cache_type = REGCACHE_RBTREE,
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static int __devinit da7210_i2c_probe(struct i2c_client *i2c,
			   	      const struct i2c_device_id *id)
{
	struct da7210_priv *da7210;
	int ret;

	da7210 = devm_kzalloc(&i2c->dev, sizeof(struct da7210_priv),
			      GFP_KERNEL);
	if (!da7210)
		return -ENOMEM;

	i2c_set_clientdata(i2c, da7210);

	da7210->regmap = regmap_init_i2c(i2c, &da7210_regmap);
	if (IS_ERR(da7210->regmap)) {
		ret = PTR_ERR(da7210->regmap);
		dev_err(&i2c->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}

	ret =  snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_da7210, &da7210_dai, 1);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
		goto err_regmap;
	}
	return ret;

err_regmap:
	regmap_exit(da7210->regmap);

	return ret;
}

static int __devexit da7210_i2c_remove(struct i2c_client *client)
{
	struct da7210_priv *da7210 = i2c_get_clientdata(client);

	snd_soc_unregister_codec(&client->dev);
	regmap_exit(da7210->regmap);
	return 0;
}

static const struct i2c_device_id da7210_i2c_id[] = {
	{ "da7210", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, da7210_i2c_id);

static struct i2c_driver da7210_i2c_driver = {
	.driver = {
		.name = "da7210-codec",
		.owner = THIS_MODULE,
	},
	.probe		= da7210_i2c_probe,
	.remove		= __devexit_p(da7210_i2c_remove),
	.id_table	= da7210_i2c_id,
};
#endif

static int __init da7210_modinit(void)
{
	int ret = 0;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&da7210_i2c_driver);
#endif
	return ret;
}
module_init(da7210_modinit);

static void __exit da7210_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&da7210_i2c_driver);
#endif
}
module_exit(da7210_exit);

MODULE_DESCRIPTION("ASoC DA7210 driver");
MODULE_AUTHOR("David Chen, Kuninori Morimoto");
MODULE_LICENSE("GPL");