/* Copyright (c) 2012-2013, The Linux Foundation. All Rights Reserved.
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

#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio_event.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <mach/rpc_server_handset.h>
#include <mach/pmic.h>
#include <mach/msm_tsif.h>
#include <mach/dma.h>

#include "devices.h"
#include "board-msm7627a.h"
#include "devices-msm7x2xa.h"


static unsigned int kp_row_gpios[1] = {
	38
};

static unsigned int kp_col_gpios[2] = {//volume down, up key
	37, 36
};

#define KP_INDEX(row, col) ((row)*ARRAY_SIZE(kp_col_gpios) + (col))

static const unsigned short keymap[ARRAY_SIZE(kp_col_gpios) *
					  ARRAY_SIZE(kp_row_gpios)] = {
        [KP_INDEX(0, 0)] = KEY_VOLUMEDOWN,
        [KP_INDEX(0, 1)] = KEY_VOLUMEUP,
};

/* SURF keypad platform device information */
static struct gpio_event_matrix_info kp_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keymap,
	.output_gpios	= kp_row_gpios,
	.input_gpios	= kp_col_gpios,
	.noutputs	= ARRAY_SIZE(kp_row_gpios),
	.ninputs	= ARRAY_SIZE(kp_col_gpios),
	.settle_time.tv64 = 40 * NSEC_PER_USEC,
	.poll_time.tv64 = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS,
};

static struct gpio_event_info *kp_info[] = {
	&kp_matrix_info.info
};

static struct gpio_event_platform_data kp_pdata = {
	.name		= "7x27a_kp",
	.info		= kp_info,
	.info_count	= ARRAY_SIZE(kp_info)
};

static struct platform_device kp_pdev = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &kp_pdata,
	},
};

static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "7k_handset",
	.pwr_key_delay_ms = 0, /* 0 will disable end key */
};

static struct platform_device hs_pdev = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};


#define LED_GREEN 109
#ifdef CONFIG_LEDS_CAMERA
#define LED_MODE_GPIO 33//L: TORCH mode;H: FLASH mode
#define LED_ENABLE_GPIO 96//L:DISABLE; H:ENABLE
#else
#define LED_FLASH 96
#endif

static struct msm_gpio led_gpio_cfg_data[] = {
/*	
	{
		GPIO_CFG(LED_GREEN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"green led"
	},
*/	
#ifdef CONFIG_LEDS_CAMERA
	{
		GPIO_CFG(LED_ENABLE_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"cameraflash"
	},
	{
		GPIO_CFG(LED_MODE_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"flashlight"
	},
#else
	{
		GPIO_CFG(LED_FLASH, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"flashlight"
	},
#endif
};

static int led_gpio_setup(void) {
	int ret = 0;
	ret = msm_gpios_request_enable(led_gpio_cfg_data,
				 sizeof(led_gpio_cfg_data)/sizeof(struct msm_gpio));
	if( ret<0 )
		printk(KERN_ERR "%s: Failed to obtain led GPIO . Code: %d\n",
				__func__, ret);
	return ret;
}

static struct platform_device pmic_led_pdev = {
	.name	= "pmic-leds",
	.id	= -1,
};


#ifdef CONFIG_INPUT_TOUCHSCREEN

static struct msm_gpio tp_cfg_data[] = {
{GPIO_CFG(48, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_6MA),"tp_irq"},
{GPIO_CFG(26, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_6MA),"tp_reset"},
};

static struct i2c_board_info i2c_touchscreen_devices[] = {
#ifdef CONFIG_TOUCHSCREEN_FT5X06
 	{
		I2C_BOARD_INFO("ft5x06_ts", 0x3d),
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_DINGSHENG
{
		I2C_BOARD_INFO("dingsheng_ts", 0x3b),
},
#endif
#ifdef CONFIG_TOUCHSCREEN_FT5X06_57
{
	I2C_BOARD_INFO("ft5x06_57", 0x39),
},
#endif
#ifdef CONFIG_TOUCHSCREEN_SSD253X
	{		
		I2C_BOARD_INFO("ssd253x_ts", 0x48),	
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_GT811
	{		
		I2C_BOARD_INFO("gt811", 0x5d),	
	},
#endif

#ifdef CONFIG_TOUCHSCREEN_GSLX680
{
	I2C_BOARD_INFO("gslx680", 0x40),
},
#endif
};

static void msm_tp_init(void)
{
	static bool b_init = 0;
	int retval = 0;

	if (b_init == 0)
	{
		b_init = 1;
		pr_err("msm_tp_init\n");

		retval = msm_gpios_request_enable(tp_cfg_data, sizeof(tp_cfg_data)/sizeof(struct msm_gpio));
		if(retval) {
			pr_err("%s: Failed to obtain touchpad GPIO. Code: %d.", __func__, retval);
		}
		
		i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
								i2c_touchscreen_devices,
								ARRAY_SIZE(i2c_touchscreen_devices));
	}
}

static int g_msm_tp_found  = 0;
void msm_tp_set_found_flag(int flag)
{
	g_msm_tp_found = flag;
}

int msm_tp_get_found_flag(void)
{
	return g_msm_tp_found;
}

#endif

/* TSIF begin */
#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
#define TSIF_B_CLK       GPIO_CFG(84, 3, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
//#define TSIF_B_EN        GPIO_CFG(85, 2, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_B_DATA      GPIO_CFG(86, 2, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_B_SYNC      GPIO_CFG(87, 3, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)

#define MSM_TSIF_PHYS        (0xa0100000)
#define MSM_TSIF_SIZE        (0x200)


static const struct msm_gpio tsif_gpios[] = {
	{ .gpio_cfg = TSIF_B_CLK,  .label =  "tsif_clk", },
//	{ .gpio_cfg = TSIF_B_EN,   .label =  "tsif_en", },
	{ .gpio_cfg = TSIF_B_DATA, .label =  "tsif_data", },
	{ .gpio_cfg = TSIF_B_SYNC, .label =  "tsif_sync", },
};

static struct msm_tsif_platform_data tsif_platform_data = {
	.num_gpios = ARRAY_SIZE(tsif_gpios),
	.gpios = tsif_gpios,
	.tsif_clk = "tsif_clk",
	.tsif_pclk = "tsif_pclk",
	.tsif_ref_clk = "tsif_ref_clk",
};

static struct i2c_board_info i2c_tdmb_mtv818_devices[] = {
	{
		I2C_BOARD_INFO("mtvi2c", 0x43),
	},	
};
static struct resource tsif_resources[] = {
	[0] = {
		.flags = IORESOURCE_IRQ,
		.start = INT_TSIF_IRQ,
		.end   = INT_TSIF_IRQ,
	},
	[1] = {
		.flags = IORESOURCE_MEM,
		.start = MSM_TSIF_PHYS,
		.end   = MSM_TSIF_PHYS + MSM_TSIF_SIZE - 1,
	},
	[2] = {
		.flags = IORESOURCE_DMA,
		.start = DMOV_TSIF_CHAN,
		.end   = DMOV_TSIF_CRCI,
	},
};

static void tsif_release(struct device *dev)
{
	dev_info(dev, "release\n");
}

static struct platform_device msm_device_tsif = {
	.name          = "msm_tsif",
	.id            = 0,
	.num_resources = ARRAY_SIZE(tsif_resources),
	.resource      = tsif_resources,
	.dev = {
		.release       = tsif_release,
	},
};

static void msm_dtv_init(void)
{
	static bool b_init = 0;
//	int retval = 0;

	if (b_init == 0)
	{
		b_init = 1;
		pr_err("msm_dtv_init\n");

//		retval = msm_gpios_request_enable(tsif_gpios, sizeof(tsif_gpios)/sizeof(struct msm_gpio));
//		if(retval) {
//			pr_err("%s: Failed to obtain touchpad GPIO. Code: %d.", __func__, retval);
//		}
		
		i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
								i2c_tdmb_mtv818_devices,
								ARRAY_SIZE(i2c_tdmb_mtv818_devices));
		
		msm_device_tsif.dev.platform_data = &tsif_platform_data;
		platform_device_register(&msm_device_tsif);
		
	}
}
#endif /* defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE) */

void __init msm7627a_add_io_devices(void)
{
	if(machine_is_msm8625q_skud() || machine_is_msm8625q_skue() || machine_is_msm8625q_evbd()){
		kp_col_gpios[0] = 37;///37
		kp_col_gpios[1] = 123;///36
	}
#ifdef CONFIG_INPUT_TOUCHSCREEN
	/* touch screen */
	msm_tp_init();
#endif

	/* keypad */
	platform_device_register(&kp_pdev);

	/* headset */
	platform_device_register(&hs_pdev);

	/* LED */
	platform_device_register(&pmic_led_pdev);
	led_gpio_setup();

	/* Vibrator */
	msm_init_pmic_vibrator();

	/* dtv */
#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
	msm_dtv_init();
#endif
}
