/*
 * include/linux/goodix_touch.h
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef 	_LINUX_GOODIX_TOUCH_H
#define		_LINUX_GOODIX_TOUCH_H

#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <mach/socinfo.h>

#define GOODIX_I2C_NAME "Goodix-TS"
#define GUITAR_GT80X
//触摸屏的分辨�?
#define TOUCH_MAX_HEIGHT 	7680	
#define TOUCH_MAX_WIDTH	 	5120

//显示屏的分辨率，根据具体平台更改，与触摸屏映射坐标相�?

//#define TS_LANDSCAPE
#define TS_PORTRAIT


#ifdef TS_LANDSCAPE
#define SCREEN_MAX_WIDTH	800
#define SCREEN_MAX_HEIGHT	480
#endif

//for portait screen

#ifdef TS_PORTRAIT
#define SCREEN_MAX_WIDTH	480
#define SCREEN_MAX_HEIGHT	800
#endif


#define TS_RESET_PORT 			26			//SHUTDOWN管脚�?
#define TS_INT_PORT  			48			            //Int IO port
#ifdef TS_INT_PORT
	#define TS_INT 				gpio_to_irq(TS_INT_PORT)	//Interrupt Number,EINT18 as 119
	//#define INT_CFG    	S3C_GPIO_SFN(3)			//IO configer,EINT type
#endif

#define GOODIX_MULTI_TOUCH
#ifndef GOODIX_MULTI_TOUCH
	#define MAX_FINGER_NUM 1
#else
	#define MAX_FINGER_NUM 5				//最大支持手指数(<=5)
#endif
#if defined(TS_INT_PORT)
	#if MAX_FINGER_NUM <= 3
	#define READ_BYTES_NUM 1+2+MAX_FINGER_NUM*5
	#elif MAX_FINGER_NUM == 4
	#define READ_BYTES_NUM 1+28
	#elif MAX_FINGER_NUM == 5
	#define READ_BYTES_NUM 1+34
	#endif
#else	
	#define READ_BYTES_NUM 1+34
#endif

enum key_state {
	FLAG_UP = 0,
	FLAG_DOWN = 1,
};

struct goodix_ts_data {
	int retry;
	int bad_data;
	int panel_type;
	char phys[32];		
	struct i2c_client *client;
	struct input_dev *input_dev;
	uint8_t use_irq;
	uint8_t use_shutdown;
	uint32_t gpio_shutdown;
	uint32_t gpio_irq;
	struct hrtimer timer;
	struct work_struct  work;
	struct early_suspend early_suspend;
	int (*power)(struct goodix_ts_data * ts, int on);
};

/* Notice:该结构体用于以下信息在平台代码中定义时使�?
 * 如果需要将下列平台相关信息与驱动分离，请将其放至平台定义文件（如mach-s3c6410.h�?
 * 并在probe函数中处�?
 */
struct goodix_i2c_platform_data {
	uint32_t gpio_irq;			//IRQ port, use macro such as "gpio_to_irq" to get Interrupt Number.
	uint32_t irq_cfg;			//IRQ port config, must refer to master's Datasheet.
	uint32_t gpio_shutdown;		//Shutdown port number
	uint32_t shutdown_cfg;		//Shutdown port config
	uint32_t screen_width;		//screen width
	uint32_t screen_height;		//screen height
};

#endif /* _LINUX_GOODIX_TOUCH_H */
