/*
 * sound\soc\sunxi\sun8iw19-pcm.c
 * (C) Copyright 2014-2019
 * AllWinner Technology Co., Ltd. <www.allwinnertech.com>
 * wolfgang huang <huangjinhui@allwinnertech.com>
 * yumingfeng <yumingfeng@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma/sunxi-dma.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <asm/dma.h>
#include "sun8iw19-pcm.h"

#define SUN8IW19_PCM_DEBUG	0

static u64 sunxi_pcm_mask = DMA_BIT_MASK(32);

static int sunxi_set_runtime_hwparams(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_platform *platform = rtd->platform;
	struct device_node *np = platform->component.dev->of_node;
	unsigned int temp_val;
	size_t temp_long;
	int ret = 0;

	snd_soc_platform_get_drvdata(platform);

	runtime->hw.info 		= SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME;

	runtime->hw.formats 		= SNDRV_PCM_FMTBIT_S8
					| SNDRV_PCM_FMTBIT_S16_LE
					| SNDRV_PCM_FMTBIT_S20_3LE
					| SNDRV_PCM_FMTBIT_S24_LE
					| SNDRV_PCM_FMTBIT_S32_LE;
	runtime->hw.rates		= SNDRV_PCM_RATE_8000_192000
					| SNDRV_PCM_RATE_KNOT;
	runtime->hw.rate_min		= 8000;
	runtime->hw.channels_min	= 1;
	runtime->hw.period_bytes_min	= 256;
	runtime->hw.periods_min		= 1;

	ret = of_property_read_u32(np, "rate_max", &temp_val);
	if (ret < 0) {
		runtime->hw.rate_max = 192000;
		pr_err("audio_driver: [%s] hwparams get failed!\n", __func__);
	} else {
		runtime->hw.rate_max = temp_val;
	}

	ret = of_property_read_u32(np, "channels_max", &temp_val);
	if (ret < 0) {
		runtime->hw.channels_max = 8;
		pr_err("audio_driver: [%s] hwparams get failed!\n", __func__);
	} else {
		runtime->hw.channels_max = temp_val;
	}

	ret = of_property_read_u32(np, "buffer_bytes_max", &temp_long);
	if (ret < 0) {
		runtime->hw.buffer_bytes_max = 1024*256;
		pr_err("audio_driver: [%s] hwparams get failed!\n", __func__);
	} else {
		runtime->hw.buffer_bytes_max = temp_long;
	}

	ret = of_property_read_u32(np, "period_bytes_max", &temp_long);
	if (ret < 0) {
		runtime->hw.period_bytes_max = 1024*128;
		pr_err("audio_driver: [%s] hwparams get failed!\n", __func__);
	} else {
		runtime->hw.period_bytes_max = temp_long;
	}

	ret = of_property_read_u32(np, "periods_max", &temp_val);
	if (ret < 0) {
		runtime->hw.periods_max = 8;
		pr_err("audio_driver: [%s] hwparams get failed!\n", __func__);
	} else {
		runtime->hw.periods_max = temp_val;
	}

	ret = of_property_read_u32(np, "fifo_size", &temp_long);
	if (ret < 0) {
		runtime->hw.fifo_size = 128;
		pr_err("audio_driver: [%s] hwparams get failed!\n", __func__);
	} else {
		runtime->hw.fifo_size = temp_long;
	}
#if SUN8IW19_PCM_DEBUG
	pr_info("audio_driver: rate_min: %d, rate_max: %d, channels_min: %d, "
		"channels_max: %d\n",
		runtime->hw.rate_min, runtime->hw.rate_max,
		runtime->hw.channels_min, runtime->hw.channels_max);

	pr_info("audio_driver: buffer_bytes_max: 0x%x, period_bytes_min: 0x%x"
		" period_bytes_max: 0x%x, periods_min: %d, periods_max: %d"
		" fifo_size: 0x%x\n\n",
		runtime->hw.buffer_bytes_max, runtime->hw.period_bytes_min,
		runtime->hw.period_bytes_max, runtime->hw.periods_min,
		runtime->hw.periods_max, runtime->hw.fifo_size);
#endif
	return 0;
};

static int sunxi_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct sunxi_dma_params *dmap;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct dma_slave_config slave_config;
	int ret;

	if (strcmp(rtd->card->name, "sun8iw19-codec") == 0) {
		dmap = snd_soc_dai_get_dma_data(rtd->codec_dai, substream);
	} else {
		dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	}

	ret = snd_hwparams_to_dma_slave_config(substream, params,
					&slave_config);
	if (ret) {
		dev_err(dev, "hw params config failed with err %d\n", ret);
		return ret;
	}

	slave_config.dst_maxburst = dmap->dst_maxburst;
	slave_config.src_maxburst = dmap->src_maxburst;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config.dst_addr = dmap->dma_addr;
		slave_config.src_addr_width = slave_config.dst_addr_width;
		slave_config.slave_id = sunxi_slave_id(dmap->dma_drq_type_num,
						DRQSRC_SDRAM);
	} else {
		slave_config.src_addr =	dmap->dma_addr;
		slave_config.dst_addr_width = slave_config.src_addr_width;
		slave_config.slave_id = sunxi_slave_id(DRQDST_SDRAM,
						dmap->dma_drq_type_num);
	}

	ret = dmaengine_slave_config(chan, &slave_config);
	if (ret < 0) {
		dev_err(dev, "dma slave config failed with err %d\n", ret);
		return ret;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int sunxi_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int sunxi_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			snd_dmaengine_pcm_trigger(substream,
					SNDRV_PCM_TRIGGER_START);
			break;
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			snd_dmaengine_pcm_trigger(substream,
					SNDRV_PCM_TRIGGER_STOP);
			break;
		}
	} else {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			snd_dmaengine_pcm_trigger(substream,
					SNDRV_PCM_TRIGGER_START);
			break;
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			snd_dmaengine_pcm_trigger(substream,
					SNDRV_PCM_TRIGGER_STOP);
			break;
		}
	}
	return 0;
}

static int sunxi_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	int ret = 0;

	/* Set HW params now that initialization is complete */
	/* dma platform Hw params get from dtsi config */
	ret = sunxi_set_runtime_hwparams(substream);
	if (ret < 0) {
		pr_err("audio_driver: [%s] dma platform hwparams set failed\n",
								__func__);
		return ret;
	}

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;
	ret = snd_dmaengine_pcm_open_request_chan(substream, NULL, NULL);
	if (ret)
		dev_err(dev, "dmaengine pcm open failed with err %d\n", ret);

	return 0;
}

static int sunxi_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = NULL;

	if (substream->runtime != NULL) {
		runtime = substream->runtime;

		return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					     runtime->dma_area,
					     runtime->dma_addr,
					     runtime->dma_bytes);
	} else {
		return -1;
	}

}

static struct snd_pcm_ops sunxi_pcm_ops = {
	.open		= sunxi_pcm_open,
	.close		= snd_dmaengine_pcm_close_release_chan,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= sunxi_pcm_hw_params,
	.hw_free	= sunxi_pcm_hw_free,
	.trigger	= sunxi_pcm_trigger,
	.pointer	= snd_dmaengine_pcm_pointer,
	.mmap		= sunxi_pcm_mmap,
};

static int sunxi_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream,
					struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct snd_soc_platform *platform = rtd->platform;
	struct device_node *np = platform->component.dev->of_node;
	size_t temp_long;
	size_t size = 0;
	int ret = 0;

	/* To get the device_node */
	snd_soc_platform_get_drvdata(platform);

	ret = of_property_read_u32(np, "buffer_bytes_max", &temp_long);
	if (ret < 0) {
		size = 1024 * 256;
		pr_err("audio_driver: [%s] buffer_bytes_max get failed!\n",
							__func__);
	} else {
		size = temp_long;
	}

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;

	return 0;
}

static void sunxi_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_coherent(pcm->card->dev, buf->bytes,
				buf->area, buf->addr);
		buf->area = NULL;
	}
}

static int sunxi_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sunxi_pcm_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = sunxi_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK, rtd);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = sunxi_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE, rtd);
		if (ret)
			goto out;
	}

out:
	return ret;
}

static struct snd_soc_platform_driver sunxi_soc_platform = {
	.ops		= &sunxi_pcm_ops,
	.pcm_new	= sunxi_pcm_new,
	.pcm_free	= sunxi_pcm_free_dma_buffers,
};

int asoc_dma_platform_register(struct device *dev, unsigned int flags)
{
	return snd_soc_register_platform(dev, &sunxi_soc_platform);
}
EXPORT_SYMBOL_GPL(asoc_dma_platform_register);

void asoc_dma_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(asoc_dma_platform_unregister);

MODULE_AUTHOR("huangxin, liushaohua");
MODULE_DESCRIPTION("sunxi ASoC DMA Driver");
MODULE_LICENSE("GPL");
