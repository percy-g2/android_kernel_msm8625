/*
 * leds-msm-pmic.c - MSM PMIC LEDs driver.
 *
 * Copyright (c) 2009, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#include <mach/pmic.h>
#include <mach/mpp.h>

#define MAX_KEYPAD_BL_LEVEL	16
#define LED_GREEN 109
#define LED_FLASH 96
#define LCDC_GPIO_PORT_LOGO 109

extern int logo_light_turnon_flag;
static void msm_keypad_bl_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
    if (value == 101) {
        printk("turn OFF logo light\n");
        logo_light_turnon_flag = 0;
        gpio_set_value(LCDC_GPIO_PORT_LOGO, 0);
    }
    else if (value == 102){
        printk("turn ON logo light\n");
        logo_light_turnon_flag = 1;
        gpio_set_value(LCDC_GPIO_PORT_LOGO, 1);
    }
#if 0
	int ret;
	
	if (value > 64)
		value = 64;

	ret = pmic_set_led_intensity(LED_KEYPAD, value / MAX_KEYPAD_BL_LEVEL);
	if (ret)
		dev_err(led_cdev->dev, "can't set keypad backlight\n");
#else
#if 0
	if (value)
	{
		pmic_secure_mpp_control_digital_output(PM_MPP_7, PM_MPP__DLOGIC__LVL_MSMP, PM_MPP__DLOGIC_OUT__CTRL_LOW); //VREG_LCD_EV_ENABLE
	} 
	else 
	{
		pmic_secure_mpp_control_digital_output(PM_MPP_7, PM_MPP__DLOGIC__LVL_MSMP, PM_MPP__DLOGIC_OUT__CTRL_HIGH); //VREG_LCD_EV_DISABLE
	}
#endif	
#endif
}

static void msm_green_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
/*	
	if (value > 0)
	{
		gpio_set_value(LED_GREEN, 1);
	}
	else
	{
		gpio_set_value(LED_GREEN, 0);
	}
*/	
}

#ifdef CONFIG_LEDS_CAMERA
#define LED_MODE_GPIO 33//L: TORCH mode;H: FLASH mode
#define LED_ENABLE_GPIO 96//L:DISABLE; H:ENABLE
static void msm_camera_flash_set(struct led_classdev *led_cdev,
	enum led_brightness value);
static struct timer_list camera_flash_timer;
static struct led_classdev msm_camera_flash = {
	.name			= "cameraflash",
	.brightness_set		= msm_camera_flash_set,
	.brightness		= LED_OFF,
};
static void msm_camera_flash_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	printk("msm_camera_flash_set, brightness=%d,delay_on = %d\n", value, msm_camera_flash.brightness);
	if(value > 0){
		//mod_timer(&camera_flash_timer, jiffies + msecs_to_jiffies(msm_camera_flash.brightness));
		gpio_set_value(LED_ENABLE_GPIO, 1);
		gpio_set_value(LED_MODE_GPIO, 1);
	}
	else{
		gpio_set_value(LED_ENABLE_GPIO, 0);
		gpio_set_value(LED_MODE_GPIO, 0);
	}
}
static void camera_flash_timer_func(unsigned long data)
{
	printk("camera_flash_timer_func, LED_ENABLE(GPIO_%d)=%d, LED_MODE(GPIO_%d)=%d\n", LED_ENABLE_GPIO, msm_camera_flash.brightness, LED_MODE_GPIO, msm_camera_flash.brightness);
	if(msm_camera_flash.brightness > 0){
		gpio_set_value(LED_ENABLE_GPIO, 1);
		gpio_set_value(LED_MODE_GPIO, 1);
		msm_camera_flash.brightness = LED_OFF;
		mod_timer(&camera_flash_timer, jiffies + msecs_to_jiffies(500));
	}
	else{
		gpio_set_value(LED_ENABLE_GPIO, 0);
		gpio_set_value(LED_MODE_GPIO, 0);
	}
}
static void msm_flash_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	printk("msm_flash_led_set, LED_ENABLE(GPIO_%d)=%d, LED_MODE(GPIO_%d)=0\n", LED_ENABLE_GPIO, value, LED_MODE_GPIO);
	if (value > 0)
	{
		gpio_set_value(LED_ENABLE_GPIO, 1);
		gpio_set_value(LED_MODE_GPIO, 0);
	}
	else
	{
		gpio_set_value(LED_ENABLE_GPIO, 0);
		gpio_set_value(LED_MODE_GPIO, 0);
	}
}
#else
static void msm_flash_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	printk("msm_flash_led_set, LED_flash(GPIO_%d)=%d\n", LED_FLASH, value);
	if (value > 0)
	{
		gpio_set_value(LED_FLASH, 1);
	}
	else
	{
		gpio_set_value(LED_FLASH, 0);
	}
}
#endif
static struct led_classdev msm_kp_bl_led = {
	.name			= "button-backlight",
	.brightness_set		= msm_keypad_bl_led_set,
	.brightness		= LED_OFF,
};

static struct led_classdev msm_green_led = {
	.name			= "green",
	.brightness_set		= msm_green_led_set,
	.brightness		= LED_OFF,
};

static struct led_classdev msm_flash_led = {
	.name			= "flashlight",
	.brightness_set		= msm_flash_led_set,
	.brightness		= LED_OFF,
};

static int msm_pmic_led_probe(struct platform_device *pdev)
{
	int rc;

	rc = led_classdev_register(&pdev->dev, &msm_kp_bl_led);
	if (rc) {
		dev_err(&pdev->dev, "unable to register led class driver\n");
		return rc;
	}
	msm_keypad_bl_led_set(&msm_kp_bl_led, LED_OFF);

	rc = led_classdev_register(&pdev->dev, &msm_green_led);
	if (rc) {
		dev_err(&pdev->dev, "unable to register green led class driver\n");
		return rc;
	}
	msm_green_led_set(&msm_green_led, LED_OFF);

	rc = led_classdev_register(&pdev->dev, &msm_flash_led);
	if (rc) {
		dev_err(&pdev->dev, "unable to register green led class driver\n");
		return rc;
	}
	msm_flash_led_set(&msm_flash_led, LED_OFF);

#ifdef CONFIG_LEDS_CAMERA
	rc = led_classdev_register(&pdev->dev, &msm_camera_flash);
	if (rc) {
		dev_err(&pdev->dev, "unable to register camera flash class driver\n");
		return rc;
	}
	msm_camera_flash_set(&msm_camera_flash, LED_OFF);
	setup_timer(&camera_flash_timer, camera_flash_timer_func, 0);
#endif
	return rc;
}

static int __devexit msm_pmic_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&msm_kp_bl_led);
	led_classdev_unregister(&msm_green_led);
	led_classdev_unregister(&msm_flash_led);
#ifdef CONFIG_LEDS_CAMERA
	led_classdev_unregister(&msm_camera_flash);
#endif

	return 0;
}

#ifdef CONFIG_PM
static int msm_pmic_led_suspend(struct platform_device *dev,
		pm_message_t state)
{
	led_classdev_suspend(&msm_kp_bl_led);

	return 0;
}

static int msm_pmic_led_resume(struct platform_device *dev)
{
	led_classdev_resume(&msm_kp_bl_led);

	return 0;
}
#else
#define msm_pmic_led_suspend NULL
#define msm_pmic_led_resume NULL
#endif

static struct platform_driver msm_pmic_led_driver = {
	.probe		= msm_pmic_led_probe,
	.remove		= __devexit_p(msm_pmic_led_remove),
	.suspend	= msm_pmic_led_suspend,
	.resume		= msm_pmic_led_resume,
	.driver		= {
		.name	= "pmic-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init msm_pmic_led_init(void)
{
	return platform_driver_register(&msm_pmic_led_driver);
}
module_init(msm_pmic_led_init);

static void __exit msm_pmic_led_exit(void)
{
	platform_driver_unregister(&msm_pmic_led_driver);
}
module_exit(msm_pmic_led_exit);

MODULE_DESCRIPTION("MSM PMIC LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pmic-leds");
