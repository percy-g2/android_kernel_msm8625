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
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <linux/i2c.h>
#include <devices-msm7x2xa.h>

#ifdef CONFIG_SENSORS_KIONIX_ACCEL 
#include <linux/input/kionix_accel.h>

static struct kionix_accel_platform_data kionix_accel_pdata = {
	.min_interval	= 5,
	.poll_interval	= 200,

	.accel_direction =7,
	.accel_irq_use_drdy =0,
	.accel_res = KIONIX_ACCEL_RES_12BIT,
	.accel_g_range	=  KIONIX_ACCEL_G_2G,
}; 
#endif /* CONFIG_SENSORS_KIONIX_ACCEL */ 


static struct i2c_board_info i2c_sensors_devices[] = {
#ifdef CONFIG_SENSORS_MMC328X	
	{
		I2C_BOARD_INFO("mmc328x", 0x30),
	},
#endif

#ifdef CONFIG_SENSORS_BH1721
	{
		I2C_BOARD_INFO("bh1721", 0x23),
	},
#endif

#ifdef CONFIG_SENSORS_MMA8452
	{
		I2C_BOARD_INFO("mma8452", 0x1C),
	},
#endif

#ifdef CONFIG_SENSORS_KIONIX_ACCEL
	{
		I2C_BOARD_INFO("kionix_accel", 0x0E),
            .platform_data = &kionix_accel_pdata,
	},
#endif

#ifdef CONFIG_SENSORS_STK2203
	{
		I2C_BOARD_INFO("stk2203", 0x10),
	},
#endif

#ifdef CONFIG_SENSORS_BH1772
	{
		I2C_BOARD_INFO("bh1772", 0x38),
	},
#endif

#ifdef CONFIG_SENSORS_MAG3110
	{
		I2C_BOARD_INFO("mag3110", 0x0E),
	},
#endif

#ifdef CONFIG_SENSORS_CM3623
	{
		I2C_BOARD_INFO("cm3623", 0x49),
	},
#endif
#ifdef CONFIG_INPUT_YAS_ACCELEROMETER
	{
		I2C_BOARD_INFO("accelerometer", (0x0c<<1)),
	},
#endif
};

void __init msm7627a_sensor_init(void)
{
	i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
							i2c_sensors_devices,
							ARRAY_SIZE(i2c_sensors_devices));
}
