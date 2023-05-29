/*  pynq-z1-video.c - The simplest kernel module.

* Copyright (C) 2013 - 2016 Xilinx, Inc
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.

*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along
*   with this program. If not, see <http://www.gnu.org/licenses/>.

*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_dma.h>

#include <linux/dmaengine.h>
#include <linux/dma/xilinx_frmbuf.h>
#include <drm/drm_fourcc.h>

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Evgeny Kudryashov");
MODULE_DESCRIPTION("ekudr-video - loadable module Video IP");

#define DRIVER_NAME "ekudr-video"

/* Simple example of how to receive command line parameters to your module.
   Delete if you don't need them */
unsigned myint = 0xdeadbeef;
char *mystr = "default";

module_param(myint, int, S_IRUGO);
module_param(mystr, charp, S_IRUGO);

struct pynq_z1_video_local {
	int irq;
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;
};


struct ekvideo_device {
	struct device *dev;
	struct dma_chan	*dma_chan;
	struct xvtc_device *vtc;
};


static void ekudr_video_dma_tx_callback(void *data)
{
//	struct ekvideo_device *ekvideo;

//	ekvideo = data;
//	dev_info(ekvideo->dev, "callback\n");
}

static void ekudr_video_enable(struct ekvideo_device *ekvideo)
{
	unsigned long flags;
	struct dma_chan *dma_chan = ekvideo->dma_chan;
	struct dma_slave_config cfg = {};
	int ret;
	dma_cookie_t cookie;

	struct dma_interleaved_template *xt;
	struct dma_async_tx_descriptor *desc;

//	struct xvtc_device *vtc = ekvideo->vtc;
//	struct xvtc_config *vtc_cfg;

//        vtc_cfg = devm_kzalloc(ekvideo->dev, sizeof(struct xvtc_config), GFP_KERNEL);
//        if (!vtc_cfg) {
//                dev_err(ekvideo->dev, "can't allocate memory for xvtc_config\n");
//                return;
//        }

//	vtc_cfg->hblank_start = 1024;
//	vtc_cfg->hsync_start = 1192;
//	vtc_cfg->hsync_end = 1224;
//	vtc_cfg->hsize = 1349;
//	vtc_cfg->vblank_start = 600;
//	vtc_cfg->vsync_start = 615;
//	vtc_cfg->vsync_end = 621;
//	vtc_cfg->vsize = 637;
//	vtc_cfg->fps = 57;
/*
	ret = xvtc_generator_start(vtc, vtc_cfg);
	if (!ret) {
		dev_err(ekvideo->dev, " VTC config is failed %08x\n", ret);
		return;
	}

*/
        xt = devm_kzalloc(ekvideo->dev, (sizeof(struct dma_interleaved_template)+sizeof(struct data_chunk)), GFP_KERNEL);
        if (!xt) {
                dev_err(ekvideo->dev, "can't allocate memory for dma_interleave_template\n");
		return;
	}

	xilinx_xdma_drm_config(dma_chan, DRM_FORMAT_BGR888);
	xilinx_xdma_set_mode(dma_chan, AUTO_RESTART);


	cfg.direction	= DMA_MEM_TO_DEV;
	cfg.src_addr	= 0x1fe00000;
	cfg.dst_addr	= 0;
	cfg.src_addr_width = 32;
	cfg.dst_addr_width = 32;

	ret = dmaengine_slave_config(dma_chan, &cfg);
	if (!ret) {
		dev_err(ekvideo->dev, "failed to config slave\n");
		return;
	}

	xt->src_start = 0x1fe00000;
	xt->dst_start = 0;
	xt->numf = 600;
	xt->frame_size = 1;
	xt->sgl[0].size = 1024*3;
	xt->sgl[0].icg = 0;

	xt->dir = DMA_MEM_TO_DEV;
	xt->src_sgl = true;
	xt->src_inc = false;
	xt->dst_sgl = true;
	xt->dst_inc = false;

	flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	desc = dmaengine_prep_interleaved_dma(dma_chan, xt, flags);
	if (!desc) {
		dev_err(ekvideo->dev, "failed to prepare DMA descriptor\n");
		return;
	}

	desc->callback = ekudr_video_dma_tx_callback;
	desc->callback_param = ekvideo;

	cookie = dmaengine_submit(desc);
	if (cookie < 0) {
		dev_err(ekvideo->dev, " dmaengine submit error\n");
		return;
	}

        xilinx_xdma_drm_config(dma_chan, DRM_FORMAT_BGR888);
        xilinx_xdma_set_mode(dma_chan, AUTO_RESTART);
	dma_async_issue_pending(dma_chan);
}

static int ekudr_video_probe(struct platform_device *pdev)
{
	struct resource *r_irq; /* Interrupt resources */
	struct resource *r_mem; /* IO mem resources */
	struct device *dev = &pdev->dev;


	int rc = 0;

	struct ekvideo_device *ekvideo;
	struct dma_chan *dma_chan;


	ekvideo = devm_kzalloc(&pdev->dev, sizeof(*ekvideo), GFP_KERNEL);
	if (!ekvideo)
		return -ENOMEM;

	ekvideo->dev = dev;

	dma_chan = of_dma_request_slave_channel(dev->of_node, "dma0");
	if (IS_ERR_OR_NULL(dma_chan)) {
		dev_err(dev, "faild to request dma channel\n");
		return PTR_ERR(dma_chan);
	}

	ekvideo->dma_chan = dma_chan;
/*
	ekvideo->vtc = xvtc_of_get(pdev->dev.of_node);
	if (IS_ERR(ekvideo->vtc)) {
		rc = PTR_ERR(ekvideo->vtc);
		goto error1;
	}

*/
	ekudr_video_enable(ekvideo);

	dev_set_drvdata(dev, ekvideo);

	dev_info(dev,"ekudr-video Video IP Probed!!!\n");
	return 0;

error1:
	kfree(ekvideo);
	dev_set_drvdata(dev, NULL);
	return rc;
}

static int ekudr_video_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pynq_z1_video_local *lp = dev_get_drvdata(dev);
	free_irq(lp->irq, lp);
	iounmap(lp->base_addr);
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id ekudr_video_of_match[] = {
	{ .compatible = "ekudr,ekudr-video", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, ekudr_video_of_match);
#else
# define ekudr_video_of_match
#endif


static struct platform_driver ekudr_video_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= ekudr_video_of_match,
	},
	.probe		= ekudr_video_probe,
	.remove		= ekudr_video_remove,
};

static int __init ekudr_video_init(void)
{

	return platform_driver_register(&ekudr_video_driver);
}


static void __exit ekudr_video_exit(void)
{
	platform_driver_unregister(&ekudr_video_driver);
}

module_init(ekudr_video_init);
module_exit(ekudr_video_exit);

