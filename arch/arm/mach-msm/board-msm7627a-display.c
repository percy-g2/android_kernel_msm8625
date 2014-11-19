/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include <mach/rpc_pmapp.h>
#include "devices.h"
#include "board-msm7627a.h"
#include <linux/kernel.h>
#include <linux/module.h>

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_SIZE		0x708000///0x708000=1024*600*4*3,0x465000=800*480*4*3
#else
#define MSM_FB_SIZE		0x4b0000///0x4b0000=1024*600*4*2,0x2EE000=800*480*4*2
#endif

/*
 * Reserve enough v4l2 space for a double buffered full screen
 * res image (800x480x1.5x2)
 */
#define MSM_V4L2_VIDEO_OVERLAY_BUF_SIZE 0x1c2000///0x119400=1152000=800x480x1.5x2

static unsigned fb_size = MSM_FB_SIZE;
static int __init fb_size_setup(char *p)
{
	fb_size = memparse(p, NULL);
	return 0;
}

early_param("fb_size", fb_size_setup);

static struct resource msm_fb_resources[] = {
	{
		.flags	= IORESOURCE_DMA,
	}
};

static int msm_fb_detect_panel(const char *name)
{
	int ret = -ENODEV;

	if (!strncmp(name, "lcdc_gordon_vga", 15))
		ret = 0;

	return ret;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

static uint32_t lcdc_gpio_initialized = 0;

static uint32_t lcdc_gpio_table[] = {
	30,
	31,
	32,
	34,
	35,
	109
};

static char *lcdc_gpio_name_table[] = {
	"spi_sdi",
	"spi_clk",
	"spi_cs",
	"backlight_en",
	"disp_reset",
	"logo"
};

static void lcdc_gordon_gpio_init(void)
{
	int i;
	int rc = 0;

	pr_info("%s: lcdc_gpio_initialized = %d\n", __func__, lcdc_gpio_initialized);

	if (!lcdc_gpio_initialized) {
		for (i = 0; i < ARRAY_SIZE(lcdc_gpio_table); i++) {
			rc = gpio_request(lcdc_gpio_table[i], lcdc_gpio_name_table[i]);
			if (rc < 0) {
				pr_err("Error request gpio %s\n", lcdc_gpio_name_table[i]);
				break;
			}
			rc = gpio_tlmm_config(GPIO_CFG(lcdc_gpio_table[i], 0,
				GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			if (rc < 0) {
				pr_err("Error config lcdc gpio:%d\n", lcdc_gpio_table[i]);
				break;
			}

			rc = gpio_direction_output(lcdc_gpio_table[i], 0);
			if (rc < 0) {
				pr_err("Error direct lcdc gpio:%d\n", lcdc_gpio_table[i]);
				break;
			}
		}

		if (rc < 0) {
			for (; i >= 0; i--) {
				gpio_free(lcdc_gpio_table[i]);
			}
			lcdc_gpio_initialized = 0;
		} else {
			lcdc_gpio_initialized = 1;
		}
	}
}

static struct msm_panel_common_pdata lcdc_gordon_panel_data;

static struct platform_device lcdc_gordon_panel_device = {
	.name   = "lcdc_gordon_vga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_gordon_panel_data,
	}
};

static struct platform_device *msm_fb_devices[] __initdata = {
	&msm_fb_device,
	&lcdc_gordon_panel_device,
};

void __init msm_msm7627a_allocate_memory_regions(void)
{
	void *addr;
	unsigned long fb_size;

	fb_size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(fb_size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + fb_size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n", fb_size,
						addr, __pa(addr));

#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
	fb_size = MSM_V4L2_VIDEO_OVERLAY_BUF_SIZE;
	addr = alloc_bootmem_align(fb_size, 0x1000);
	msm_v4l2_video_overlay_resources[0].start = __pa(addr);
	msm_v4l2_video_overlay_resources[0].end =
		msm_v4l2_video_overlay_resources[0].start + fb_size - 1;
	pr_debug("allocating %lu bytes at %p (%lx physical) for v4l2\n",
		fb_size, addr, __pa(addr));
#endif
}

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
	.mdp_rev = MDP_REV_303,
	.cont_splash_enabled = 0x1,
	.splash_screen_addr = 0x00,
	.splash_screen_size = 0x00,
};

static struct lcdc_platform_data lcdc_pdata;


void __init msm_fb_add_devices(void)
{
	/* Using continuous splash or not */
	if (!mdp_pdata.cont_splash_enabled) {
		lcdc_gordon_gpio_init();
	}

	platform_add_devices(msm_fb_devices, ARRAY_SIZE(msm_fb_devices));

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("lcdc", &lcdc_pdata);
}

