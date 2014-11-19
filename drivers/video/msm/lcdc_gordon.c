/* Copyright (c) 2009-2010, 2012 The Linux Foundation. All rights reserved.
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

#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/pmic.h>
#include <mach/vreg.h>
#include "msm_fb.h"

#ifdef CONFIG_USE_HIRES_LCD
#define LCDC_PIXCLK_IN_HZ 40000000

#define LCDC_FB_WIDTH 1024
#define LCDC_FB_HEIGHT 600

#define LCDC_HSYNC_PULSE_WIDTH_DCLK 48
#define LCDC_HSYNC_BACK_PORCH_DCLK 40
#define LCDC_HSYNC_FRONT_PORCH_DCLK 40
#define LCDC_HSYNC_SKEW_DCLK 0

#define LCDC_VSYNC_PULSE_WIDTH_LINES 3
#define LCDC_VSYNC_BACK_PORCH_LINES 29
#define LCDC_VSYNC_FRONT_PORCH_LINES 13
#endif

#ifdef CONFIG_USE_SKY_LCD_001
#define LCDC_PIXCLK_IN_HZ 40000000

#define LCDC_FB_WIDTH 800
#define LCDC_FB_HEIGHT 480

#define LCDC_HSYNC_PULSE_WIDTH_DCLK 1                 /* 1 ~ 40 */
#define LCDC_HSYNC_BACK_PORCH_DCLK  46                 /* 46 */
#define LCDC_HSYNC_FRONT_PORCH_DCLK 200                 /* 16 ~ 354 */
#define LCDC_HSYNC_SKEW_DCLK 0

#define LCDC_VSYNC_PULSE_WIDTH_LINES 4                /* 1 ~ 20 */
#define LCDC_VSYNC_BACK_PORCH_LINES  23                /* 23 */
#define LCDC_VSYNC_FRONT_PORCH_LINES 147                /* 7 ~ 147 */
#endif

#ifdef CONFIG_USE_SKY_LCD_002                        /* Jing Hua*/
#define LCDC_PIXCLK_IN_HZ 40000000

#define LCDC_FB_WIDTH 800
#define LCDC_FB_HEIGHT 480

#define LCDC_HSYNC_PULSE_WIDTH_DCLK 40                 /* 1 ~ 40 */
#define LCDC_HSYNC_BACK_PORCH_DCLK  46                 /* 46 */
#define LCDC_HSYNC_FRONT_PORCH_DCLK 210                 /* 16 ~ 354, typical 210 */
#define LCDC_HSYNC_SKEW_DCLK 0

#define LCDC_VSYNC_PULSE_WIDTH_LINES 8                /* 1 ~ 20 */
#define LCDC_VSYNC_BACK_PORCH_LINES  23                /* 23 */
#define LCDC_VSYNC_FRONT_PORCH_LINES 45                /* 7 ~ 147, typical 45 */
#endif
#ifdef CONFIG_USE_DXK_LCD_001
#define LCDC_PIXCLK_IN_HZ 40000000

#define LCDC_FB_WIDTH 800
#define LCDC_FB_HEIGHT 480

#define LCDC_HSYNC_PULSE_WIDTH_DCLK 1                 /* 1 ~ 40 */
#define LCDC_HSYNC_BACK_PORCH_DCLK  46                 /* 46 */
#define LCDC_HSYNC_FRONT_PORCH_DCLK 200                 /* 16 ~ 354 */
#define LCDC_HSYNC_SKEW_DCLK 0

#define LCDC_VSYNC_PULSE_WIDTH_LINES 1                /* 1 ~ 20 */
#define LCDC_VSYNC_BACK_PORCH_LINES  20                /* 23 */
#define LCDC_VSYNC_FRONT_PORCH_LINES 147                /* 7 ~ 147 */
#endif

#ifdef CONFIG_USE_DXK_LCD_002
#define LCDC_PIXCLK_IN_HZ 40000000

#define LCDC_FB_WIDTH 800
#define LCDC_FB_HEIGHT 480

#define LCDC_HSYNC_PULSE_WIDTH_DCLK 1                 /* 1 ~ 40 */
#define LCDC_HSYNC_BACK_PORCH_DCLK  42                 /* 46 */
#define LCDC_HSYNC_FRONT_PORCH_DCLK 213                 /* 16 ~ 354 */
#define LCDC_HSYNC_SKEW_DCLK 0

#define LCDC_VSYNC_PULSE_WIDTH_LINES 1                /* 1 ~ 20 */
#define LCDC_VSYNC_BACK_PORCH_LINES  18                /* 23 */
#define LCDC_VSYNC_FRONT_PORCH_LINES 26                /* 7 ~ 147 */
#endif
#define LCDC_BACKLIGHT_LEVELS 16

#define LCDC_GPIO_PORT_BACKLIGHT 30
#define LCDC_GPIO_PORT_SDO 31
#define LCDC_GPIO_PORT_SDI 34
#define LCDC_GPIO_PORT_CS 32
#define LCDC_GPIO_PORT_P_OFF 35
#define LCDC_GPIO_PORT_LOGO 109
#define LCDC_GPIO_PORT_VREG_EV_EN 28
#define LCDC_GPIO_PIN30_STATUS  109
#define LCDC_GPIO_PIN38_STATUS  85

static struct msm_panel_common_pdata *lcdc_gordon_pdata;
volatile unsigned int lcd_backlight_level;
volatile unsigned int lcd_panel_status;
static struct timer_list lcd_backlight_timer;
static int lcd_is_boot_flag = true;
int logo_light_turnon_flag = true;
EXPORT_SYMBOL(logo_light_turnon_flag);

void lcdc_gordon_set_hw_backlight(unsigned int level)
{
	int count;

	gpio_set_value(LCDC_GPIO_PORT_BACKLIGHT, 0);
	mdelay(6);

	if (level > 0) {
		for (count = level; count <= LCDC_BACKLIGHT_LEVELS; count++) {
			gpio_set_value(LCDC_GPIO_PORT_BACKLIGHT, 0);
			udelay(6);
			gpio_set_value(LCDC_GPIO_PORT_BACKLIGHT, 1);
			udelay(6);
		}
                gpio_set_value(LCDC_GPIO_PORT_VREG_EV_EN, 1);
                
                gpio_set_value(LCDC_GPIO_PIN30_STATUS, 1); // Teb on lcd on
                gpio_set_value(LCDC_GPIO_PIN38_STATUS, 1); // Teb on lcd on
                
                if (logo_light_turnon_flag) gpio_set_value(LCDC_GPIO_PORT_LOGO, 1);
	} else {
                gpio_set_value(LCDC_GPIO_PORT_LOGO, 0);
                //gpio_set_value(LCDC_GPIO_PORT_VREG_EV_EN, 0);
                
                gpio_set_value(LCDC_GPIO_PIN30_STATUS, 0); // Teb on lcd off
                gpio_set_value(LCDC_GPIO_PIN38_STATUS, 1); // Teb on lcd off
	}
	
}

static void lcd_backlight_timer_func(unsigned long data)
{
	lcd_panel_status = lcd_backlight_level;
	lcdc_gordon_set_hw_backlight(lcd_backlight_level);
}

#ifdef CONFIG_AW9523
enum {
	AW9523_I2C_GPIO_CAMIF_EN_F = 0, 	/*P0_6: CAMIF_EN_F*/
	AW9523_I2C_GPIO_CAMIF_EN_B, 		/*P0_7: CAMIF_EN_B*/

	AW9523_I2C_GPIO_IO_SWITCH  = 100,	/*P1_0: IO_SWITCH*/
	AW9523_I2C_GPIO_LCD_P_OFF, 			/*P1_1: LCD_P_OFF*/
	AW9523_I2C_GPIO_VREG_LCD_EV_EN, 	/*P1_2: VREG_LCD_EV_EN*/
	AW9523_I2C_GPIO_CHARGE_ON, 			/*P1_3: CHARGE_ON*/
	AW9523_I2C_GPIO_OTG_VDD_OFF,		/*P1_4: OTG_VDD_OFF*/
	AW9523_I2C_GPIO_BT_SYS_RESET_N,		/*P1_5: BT_SYS_RESET_N*/
	AW9523_I2C_GPIO_DTV_IO,				/*P1_6: DTV_IO*/
	AW9523_I2C_GPIO_A_PA_SHD,			/*P1_7: A_PA_SHD*/
};
extern int aw9523_i2c_gpio_output(unsigned char gpio_i2c_type, unsigned char val);
#endif

static int lcdc_gordon_panel_on(struct platform_device *pdev)
{   
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	if (!mfd->cont_splash_done) {
		mfd->cont_splash_done = 1;
	}

#ifdef CONFIG_AW9523
	aw9523_i2c_gpio_output(AW9523_I2C_GPIO_VREG_LCD_EV_EN, 1); //gp5
	aw9523_i2c_gpio_output(AW9523_I2C_GPIO_LCD_P_OFF, 1); //PM_MPP_2
	aw9523_i2c_gpio_output(AW9523_I2C_GPIO_IO_SWITCH, 1); 
#else
	pmic_secure_mpp_control_digital_output(PM_MPP_4, PM_MPP__DLOGIC__LVL_MSMP, PM_MPP__DLOGIC_OUT__CTRL_HIGH); //VREG_LCD_EV_EN
	gpio_set_value(LCDC_GPIO_PORT_P_OFF, 1);
	gpio_set_value(LCDC_GPIO_PORT_SDO, 1);
	#ifdef CONFIG_USE_HIRES_LCD
	gpio_set_value(LCDC_GPIO_PORT_SDI, 0);
	gpio_set_value(LCDC_GPIO_PORT_CS, 0);
	#else
	gpio_set_value(LCDC_GPIO_PORT_CS, 1);
	#endif
#endif

    if (lcd_is_boot_flag) {
        lcd_is_boot_flag = false;
        mod_timer(&lcd_backlight_timer, jiffies + msecs_to_jiffies(100));
    }
    else {
        lcd_backlight_timer_func(0);
    }
	return 0;
}

static int lcdc_gordon_panel_off(struct platform_device *pdev)
{
#ifdef CONFIG_AW9523
	aw9523_i2c_gpio_output(AW9523_I2C_GPIO_IO_SWITCH, 0); 
	aw9523_i2c_gpio_output(AW9523_I2C_GPIO_LCD_P_OFF, 0); //PM_MPP_2
	aw9523_i2c_gpio_output(AW9523_I2C_GPIO_VREG_LCD_EV_EN, 0); //gp5
#else
	#ifdef CONFIG_USE_HIRES_LCD
	gpio_set_value(LCDC_GPIO_PORT_CS, 1);
	gpio_set_value(LCDC_GPIO_PORT_SDI, 1);
	#else
	gpio_set_value(LCDC_GPIO_PORT_CS, 0);
	#endif
	gpio_set_value(LCDC_GPIO_PORT_SDO, 0);
	gpio_set_value(LCDC_GPIO_PORT_P_OFF, 0);
	//Not poweroff for TP
//	pmic_secure_mpp_control_digital_output(PM_MPP_4, PM_MPP__DLOGIC__LVL_MSMP, PM_MPP__DLOGIC_OUT__CTRL_LOW); //VREG_LCD_EV_EN
#endif
	lcd_panel_status = 0;
	lcd_backlight_level = lcd_panel_status;
	lcdc_gordon_set_hw_backlight(lcd_backlight_level);

	return 0;
}

static void lcdc_gordon_set_backlight(struct msm_fb_data_type *mfd)
{
	lcd_backlight_level = (mfd->bl_level <= LCDC_BACKLIGHT_LEVELS) ? mfd->bl_level : LCDC_BACKLIGHT_LEVELS;
	if (lcd_panel_status || !lcd_backlight_level) {
		lcd_panel_status = lcd_backlight_level;
		lcdc_gordon_set_hw_backlight(lcd_backlight_level);
	} else {
		mod_timer(&lcd_backlight_timer, jiffies + msecs_to_jiffies(150));
	}
}

static int __devinit gordon_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_gordon_pdata = pdev->dev.platform_data;
		return 0;
	}
	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = gordon_probe,
	.driver = {
		.name   = "lcdc_gordon_vga",
	},
};

static struct msm_fb_panel_data gordon_panel_data = {
	.on = lcdc_gordon_panel_on,
	.off = lcdc_gordon_panel_off,
	.set_backlight = lcdc_gordon_set_backlight,
};

static struct platform_device this_device = {
	.name   = "lcdc_gordon_vga",
	.id	= 1,
	.dev	= {
		.platform_data = &gordon_panel_data,
	}
};

static int __init lcdc_gordon_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &gordon_panel_data.panel_info;
	pinfo->xres = LCDC_FB_WIDTH;
	pinfo->yres = LCDC_FB_HEIGHT;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = LCDC_PIXCLK_IN_HZ;

	pinfo->bl_max = LCDC_BACKLIGHT_LEVELS;
	pinfo->bl_min = 1;

	pinfo->lcdc.h_back_porch = LCDC_HSYNC_BACK_PORCH_DCLK;
	pinfo->lcdc.h_front_porch = LCDC_HSYNC_FRONT_PORCH_DCLK;
	pinfo->lcdc.h_pulse_width = LCDC_HSYNC_PULSE_WIDTH_DCLK;
	
	pinfo->lcdc.v_back_porch = LCDC_VSYNC_BACK_PORCH_LINES;
	pinfo->lcdc.v_front_porch = LCDC_VSYNC_FRONT_PORCH_LINES;
	pinfo->lcdc.v_pulse_width = LCDC_VSYNC_PULSE_WIDTH_LINES;
	
	pinfo->lcdc.border_clr = 0;     /* black */
	pinfo->lcdc.underflow_clr = 0xff;       /* blue */
	pinfo->lcdc.hsync_skew = LCDC_HSYNC_SKEW_DCLK;

	lcd_backlight_level = pinfo->bl_max;
	//lcd_backlight_level = 0;
	lcd_panel_status = 0;

	setup_timer(&lcd_backlight_timer, lcd_backlight_timer_func, 0);

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lcdc_gordon_panel_init);
