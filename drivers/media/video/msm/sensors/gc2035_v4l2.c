/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "msm_sensor.h"
#include "msm.h"
#include "gc2035_v4l2.h"
#include <../../../../../../build/buildplus/target/QRDExt_target.h>
#define SENSOR_NAME "gc2035"
#define PLATFORM_DRIVER_NAME "msm_camera_gc2035"
#define gc2035_obj gc2035_##obj

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define GC2035_VERBOSE_DGB

#ifdef GC2035_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif

#define INVMASK(v)  (0xff-v)

static struct msm_sensor_ctrl_t gc2035_s_ctrl;
static int effect_value = CAMERA_EFFECT_OFF;
static unsigned int SAT_U = 0x80; /* DEFAULT SATURATION VALUES*/
static unsigned int SAT_V = 0x80; /* DEFAULT SATURATION VALUES*/
#define  LED_MODE_OFF 0
#define  LED_MODE_AUTO 1
#define  LED_MODE_ON 2
#define  LED_MODE_TORCH 3
static int led_flash_mode = LED_MODE_AUTO;

DEFINE_MUTEX(gc2035_mut);

static struct msm_camera_i2c_reg_conf gc2035_start_settings[] = {
	{0xfe, 0x03},
	{0x10, 0x94},
	{0xfe, 0x00},
};

static struct msm_camera_i2c_reg_conf gc2035_stop_settings[] = {
	{0xfe, 0x03},
	{0x10, 0x00},
	{0xfe, 0x00},
};

static struct msm_camera_i2c_reg_conf gc2035_prev_30fps_settings[] = {
	
        {0xfe, 0x00},
        {0xfa, 0x00},
        {0xb6, 0x03},//turn on AEC
        {0xc8, 0x00},
           
        {0x99, 0x22},// 1/2 subsample
        {0x9a, 0x06},
        {0x9b, 0x02},
        {0x9c, 0x00},
        {0x9d, 0x02},
        {0x9e, 0x00},
        {0x9f, 0x02},
        {0xa0, 0x00},
        {0xa1, 0x02},
        {0xa2, 0x00},
        
        {0x90, 0x01},
        {0x95, 0x02},
        {0x96, 0x58},
        {0x97, 0x03},
        {0x98, 0x20}, 

        {0xfe, 0x03},
        {0x12, 0x40},
        {0x13, 0x06},
        {0x04, 0x90},
        {0x05, 0x01},
        {0xfe, 0x00},
};

static struct msm_camera_i2c_reg_conf gc2035_snap_settings[] = {
        {0xfe, 0x00},
        {0xb6, 0x00},
        {0xc8, 0x00},
        {0xfa, 0x11},

	 {0x99, 0x11},// 1/1 subsample
        {0x9a, 0x06},
        {0x9b, 0x00},
        {0x9c, 0x00},
        {0x9d, 0x00},
        {0x9e, 0x00},	 
        {0x9f, 0x00},
        {0xa0, 0x00},
        {0xa1, 0x00},
        {0xa2, 0x00},

	 {0x90, 0x01},	 
        {0x95, 0x04},
        {0x96, 0xb0},
        {0x97, 0x06},
        {0x98, 0x40},
            
        {0xfe, 0x03},
        {0x12, 0x80},
        {0x13, 0x0c},
        {0x04, 0x20},
        {0x05, 0x00},
        {0xfe, 0x00},
        
};

//set sensor init setting
static struct msm_camera_i2c_reg_conf gc2035_init_settings[] = {
	{0xfe, 0x80},  
	{0xfe, 0x80},  
	{0xfe, 0x80},  
	{0xfc, 0x06},  
	{0xf9, 0xfe},  
	{0xfa, 0x00}, 
	
	{0xad, 0x80},  
	{0xae, 0x7c},//0d  
	{0xaf, 0x80}, //82  86	
	 
	{0xf6, 0x00},  
	{0xf7, 0x05},//0d  
	{0xf8, 0x84}, //82  86
	{0xfe, 0x00},  
	{0x82, 0x00},  
	{0xb3, 0x60},  
	{0xb4, 0x40},  
	{0xb5, 0x60},  
	{0x03, 0x05},  
	{0x04, 0x08},  
	{0xfe, 0x00},  
	{0xec, 0x04},  
	{0xed, 0x04},  
	{0xee, 0x60},  
	{0xef, 0x90},  
	{0x0a, 0x00},  
	{0x0c, 0x00},  
	{0x0d, 0x04},  
	{0x0e, 0xc0},  
	{0x0f, 0x06},  
	{0x10, 0x58},  
	{0x17, 0x14},//0x14  flip
	{0x18, 0x0a},  
	{0x19, 0x0c},  
	{0x1a, 0x01},  
	{0x1b, 0x48},  
	{0x1e, 0x88},  
	{0x1f, 0x0f},  
	{0x20, 0x05},  
	{0x21, 0x0f},  
	{0x22, 0xf0},  
	{0x23, 0xc3},  
	{0x24, 0x16},  
	{0xfe, 0x01},  
	{0x09, 0x00},  
	{0x0b, 0x90},  
	{0x1f, 0xa0},//c0  
	{0x20, 0x66},//60  	
	{0x13, 0x74},  
	{0xfe, 0x00},  
	{0xfe, 0x00},  
/*

*/
	{0x05, 0x01},//
	{0x06, 0x25},  
	{0x07, 0x00},//
	{0x08, 0x14},  
	{0xfe, 0x01},  
	{0x27, 0x00},//
	{0x28, 0x83},  
	{0x29, 0x04},//12.2
	{0x2a, 0x9b},  
	{0x2b, 0x04},//9.9
	{0x2c, 0x9b},  
	{0x2d, 0x05},//7.6
	{0x2e, 0xa1},  
	{0x2f, 0x07},//5.2
	{0x30, 0x2a},  

	{0x3e, 0x40},//
	{0xfe, 0x00},  
	{0xb6, 0x03},  
	{0xfe, 0x00},  
	{0x3f, 0x00},  
	{0x40, 0x77},  
	{0x42, 0x7f},  
	{0x43, 0x30},  
	{0x5c, 0x08},  
	{0x5e, 0x20},  
	{0x5f, 0x20},  
	{0x60, 0x20},  
	{0x61, 0x20},  
	{0x62, 0x20},  
	{0x63, 0x20},  
	{0x64, 0x20},  
	{0x65, 0x20},  
	{0x66, 0x20},  
	{0x67, 0x20},  
	{0x68, 0x20},  
	{0x69, 0x20},  
	{0x90, 0x01},  
	{0x95, 0x04},  
	{0x96, 0xb0},  
	{0x97, 0x06},  
	{0x98, 0x40},  
	{0xfe, 0x03},  
	{0x42, 0x80},  
	{0x43, 0x06},  
	{0x41, 0x00},  
	{0x40, 0x00},  
	{0x17, 0x01},  
	{0xfe, 0x00},  
	{0x80, 0xff},  
	{0x81, 0x26},  
	{0x03, 0x05},  
	{0x04, 0x2e},  
	{0x84, 0x02},  //uv
	{0x86, 0x02},  
	{0x87, 0x80},  
	{0x8b, 0xbc},  
	{0xa7, 0x80},  
	{0xa8, 0x80},  
	{0xb0, 0x80},  
	{0xc0, 0x40},  
	
	#if 0
	{0xfe, 0x01},
	{0xc2, 0x14},
	{0xc3, 0x10},
	{0xc4, 0x0c},
	{0xc8, 0x22},
	{0xc9, 0x16},
	{0xca, 0x0b},
	{0xbc, 0x2f},
	{0xbd, 0x20},
	{0xbe, 0x19},
	{0xb6, 0x3a},
	{0xb7, 0x24},
	{0xb8, 0x20},
	{0xc5, 0x00},
	{0xc6, 0x00},
	{0xc7, 0x00},
	{0xcb, 0x00},
	{0xcc, 0x00},
	{0xcd, 0x10},
	{0xbf, 0x00},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xb9, 0x00},
	{0xba, 0x00},
	{0xbb, 0x00},
	{0xaa, 0x04},
	{0xab, 0x08},
	{0xac, 0x09},
	{0xad, 0x01},
	{0xae, 0x00},
	{0xaf, 0x08},
	{0xb0, 0x09},
	{0xb1, 0x09},
	{0xb2, 0x09},
	{0xb3, 0x0b},
	{0xb4, 0x0b},
	{0xb5, 0x0b},
	{0xd0, 0x00},
	{0xd2, 0x00},
	{0xd3, 0x00},
	{0xd8, 0x00},
	{0xda, 0x00},
	{0xdb, 0x00},
	{0xdc, 0x00},
	{0xde, 0x00},
	{0xdf, 0x0e},
	{0xd4, 0x00},
	{0xd6, 0x00},
	{0xd7, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0x00},
	{0xa7, 0x00},
	{0xa8, 0x00},
	{0xa9, 0x00},
	{0xa1, 0x80},
	{0xa2, 0x80},
	#else
	{0xfe, 0x01},	
	{0xa1, 0x80}, 
	{0xa2, 0x80}, 
	{0xa4, 0x22}, 
	{0xa5, 0x77}, 
	{0xa6, 0x26}, 
	{0xa7, 0x72}, 
	{0xa8, 0x20}, 
	{0xa9, 0x70}, 
	{0xaa, 0x02},//02 
	{0xab, 0x05}, 
	{0xac, 0x03}, 
	{0xad, 0x07},//07 
	{0xae, 0x0d}, //0d
	{0xaf, 0x04}, 
	{0xb0, 0x00}, 
	{0xb1, 0x15}, 
	{0xb2, 0x0e}, 
	{0xb3, 0x02},//02 
	{0xb4, 0x09}, 
	{0xb5, 0x02}, 
	{0xb6, 0x41}, 
	{0xb7, 0x4e}, 
	{0xb8, 0x2a}, 
	{0xb9, 0x10}, 
	{0xba, 0x1a}, 
	{0xbb, 0x0e}, 
	{0xbc, 0x50}, 
	{0xbd, 0x3b}, 
	{0xbe, 0x29}, 
	{0xbf, 0x02}, 
	{0xc0, 0x13}, 
	{0xc1, 0x00}, 
	{0xc2, 0x31}, //31
	{0xc3, 0x32}, //32
	{0xc4, 0x1f}, 
	{0xc5, 0x04}, 
	{0xc6, 0x21}, 
	{0xc7, 0x0f}, 
	{0xc8, 0x20}, 
	{0xc9, 0x28}, 
	{0xca, 0x19}, 
	{0xcb, 0x15}, 
	{0xcc, 0x0b}, 
	{0xcd, 0x08}, 
	{0xd0, 0x13}, 
	{0xd2, 0x00}, 
	{0xd3, 0x08}, 
	{0xd4, 0x23}, 
	{0xd6, 0x23}, 
	{0xd7, 0x3a}, 
	{0xd8, 0x2a}, 
	{0xda, 0x05}, 
	{0xdb, 0x09}, 
	{0xdc, 0x00}, 
	{0xde, 0x17}, 
	{0xdf, 0x2b}, 
	{0xfe, 0x00},
	#endif
	
	
	{0xfe, 0x02},  
	{0xa4, 0x00},  
	{0xfe, 0x00},  
	{0xfe, 0x02},  
	{0xc0, 0x01},  
	{0xc1, 0x3d},  //3d 
	{0xc2, 0xfc},  
	{0xc3, 0x07},  //05
	{0xc4, 0xec},  
	{0xc5, 0x42},  
	{0xc6, 0xf8},  
	{0xc7, 0x40},  
	{0xc8, 0xf8},  
	{0xc9, 0x06},  
	{0xca, 0xfd},  
	{0xcb, 0x3e},  
	{0xcc, 0xf3},  
	{0xcd, 0x36},  
	{0xce, 0xf6},  
	{0xcf, 0x04},  
	{0xe3, 0x0c},  
	{0xe4, 0x44},  
	{0xe5, 0xe5}, 
	{0xfe, 0x00}, 
	//////AWB clear
	{0xfe, 0x01},
	{0x4f, 0x00}, 
	{0x4d, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x10}, 	
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4d, 0x20},  ///////////////20
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x30}, //////////////////30
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x02},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x40},  //////////////////////40
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x04},
	{0x4e, 0x02},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x50}, //////////////////50
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x08}, 
	{0x4e, 0x08},
	{0x4e, 0x04},
	{0x4e, 0x04},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x60}, /////////////////60
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x20},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x70}, ///////////////////70
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x20},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x80}, /////////////////////80
	{0x4e, 0x00},
	{0x4e, 0x20},
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 		  
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0x90}, //////////////////////90
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0xa0}, /////////////////a0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0xb0}, //////////////////////////////////b0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0xc0}, //////////////////////////////////c0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0xd0}, ////////////////////////////d0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0xe0}, /////////////////////////////////e0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0xf0}, /////////////////////////////////f0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4f, 0x01},   
	{0x50, 0x80},
	{0x56, 0x06},
	
	//{0x50, 0x80},
	{0x52, 0x40},
	{0x54, 0x60},
	//{0x56, 0x00},
	{0x57, 0x20},
	{0x58, 0x01}, 
	{0x5b, 0x08},
	{0x61, 0xaa},
	{0x62, 0xaa},
	{0x71, 0x00},
	{0x72, 0x25},
	{0x74, 0x10},
	{0x77, 0x08},
	{0x78, 0xfd},
	{0x86, 0x30},
	{0x87, 0x00},
	{0x88, 0x04},
	{0x8a, 0xc0},
	{0x89, 0x71},
	{0x84, 0x08},
	{0x8b, 0x00},
	
	{0x8d, 0x70},//70
	{0x8e, 0x70},
	{0x8f, 0xf4},

	{0xfe, 0x00},  
	{0x82, 0x02},  
	{0xfe, 0x01},  
	{0x21, 0xbf},  
	{0xfe, 0x02},  
	{0xa5, 0x50}, 
	{0xa2, 0xb0},  
	{0xa6, 0x50},  
	{0xa7, 0x30},  
	{0xab, 0x31},  
	{0x88, 0x1c},// 0x15 
	{0xa9, 0x6c},  
	{0xb0, 0x55},  
	{0xb3, 0x70},  
	{0x8c, 0xf6},  
	{0x89, 0x03},  
	{0xde, 0xb6},  
	{0x38, 0x08},  
	{0x39, 0x50},  

	{0xfe, 0x00},  
	{0x81, 0x26},  
	{0x87, 0x80}, 


 
	{0xfe, 0x02},  
	{0x83, 0x00},  
	{0x84, 0x45},  
	{0xd1, 0x2b},  
	{0xd2, 0x2b},  
	{0xd5, 0xf6},
	{0xdd, 0x38},  
	{0xfe, 0x00},  
	{0xfe, 0x02},  
	{0x2b, 0x00},  
	{0x2c, 0x04},  
	{0x2d, 0x09},  
	{0x2e, 0x18},  
	{0x2f, 0x27},  
	{0x30, 0x37},  
	{0x31, 0x49},  
	{0x32, 0x5c},  
	{0x33, 0x7e},  
	{0x34, 0xa0},  
	{0x35, 0xc0},  
	{0x36, 0xe0},  
	{0x37, 0xff},  
	{0xfe, 0x00},  
	{0x82, 0xfe},
//MIPI
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf4, 0x00},
	{0xf5, 0x00},
	{0xfe, 0x01},
	{0x0b, 0x90},
	{0x87, 0x10},
	{0xfe, 0x00},

	{0xfe, 0x03},
	{0x01, 0x03},
	{0x02, 0x37},
	{0x03, 0x10},
	{0x06, 0x90},//leo changed
	{0x11, 0x1E},
	{0x12, 0x80},
	{0x13, 0x0c},
	{0x15, 0x10},
	{0x04, 0x20},
	{0x05, 0x00},
	{0x17, 0x00},

	{0x21, 0x01},
	{0x22, 0x03},
	{0x23, 0x01},
	{0x29, 0x03},
	{0x2a, 0x01},
	//{0x2b, 0x06},

	//{0x10, 0x94},
	{0xfe, 0x00},
	
	
	
	
	
	
	
	//debug mode   1009
	
	
	/*{0xfe , 0x00},
	{0x80 , 0x08},
	{0x81 , 0x00},
	{0x82 , 0x00},
	{0xa3 , 0x80},
	{0xa4 , 0x80},
	{0xa5 , 0x80},
	{0xa6 , 0x80},
	{0xa7 , 0x80},
	{0xa8 , 0x80},
	{0xa9 , 0x80},
	{0xaa , 0x80},
	{0xad , 0x80},
	{0xae , 0x80},
	{0xaf , 0x80},
	{0xb3 , 0x40},
	{0xb4 , 0x40},
	{0xb5 , 0x40},
	{0xfe , 0x01},
	{0x0a , 0x40},
	{0x13 , 0x48},
	{0x9f , 0x40},
	{0xfe , 0x02},
	{0xd0 , 0x40},
	{0xd1 , 0x20},
	{0xd2 , 0x20},
	{0xd3 , 0x40},
	{0xd5 , 0x00},
	{0xdd , 0x00},
	{0xfe , 0x00},*/
};

static struct msm_camera_i2c_conf_array gc2035_init_conf[] = {
	{&gc2035_init_settings[0],
	ARRAY_SIZE(gc2035_init_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array gc2035_confs[] = {
	{&gc2035_snap_settings[0], 
	ARRAY_SIZE(gc2035_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&gc2035_prev_30fps_settings[0],
	ARRAY_SIZE(gc2035_prev_30fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_csi_params gc2035_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 20,
};

static struct v4l2_subdev_info gc2035_subdev_info[] = {
	{
	.code	= V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt	= 1,
	.order	  = 0,
	}
	/* more can be supported, to be added later */
};

/*renwei add it for the gc2035 function at 2012-8-11*/
static int gc2035_led_flash_auto(struct msm_sensor_ctrl_t *s_ctrl)
{
	unsigned short shutter = 0;
	
	printk("Sukha_gc2035 gc2035_led_flash_ctrl led_flash_mode = %d\n", led_flash_mode);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xFE, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x14, &shutter, MSM_CAMERA_I2C_BYTE_DATA);
	
	printk("gc2035_led_flash_auto: shutter = %d\n", shutter);
	if (shutter < 70) {
		msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_HIGH);
	}
	
	return 0;
}

/*renwei add it for the gc2035 function at 2012-8-11*/
static struct msm_camera_i2c_reg_conf gc2035_saturation[][5] = {
	{{0xfe, 0x02},{0xd1, 0x10},{0xd2, 0x10},{0xfe, 0x00}},//Saturation -5
	{{0xfe, 0x02},{0xd1, 0x18},{0xd2, 0x18},{0xfe, 0x00}},//Saturation -4
	{{0xfe, 0x02},{0xd1, 0x20},{0xd2, 0x20},{0xfe, 0x00}},//Saturation -3
	{{0xfe, 0x02},{0xd1, 0x28},{0xd2, 0x28},{0xfe, 0x00}},//Saturation -2
	{{0xfe, 0x02},{0xd1, 0x2b},{0xd2, 0x2b},{0xfe, 0x00}},//Saturation -1
	{{0xfe, 0x02},{0xd1, 0x30},{0xd2, 0x30},{0xfe, 0x00}},//Saturation
	{{0xfe, 0x02},{0xd1, 0x40},{0xd2, 0x40},{0xfe, 0x00}},//Saturation +1
	{{0xfe, 0x02},{0xd1, 0x48},{0xd2, 0x48},{0xfe, 0x00}},//Saturation +2
	{{0xfe, 0x02},{0xd1, 0x50},{0xd2, 0x50},{0xfe, 0x00}},//Saturation +3
	{{0xfe, 0x02},{0xd1, 0x58},{0xd2, 0x58},{0xfe, 0x00}},//Saturation +4
	{{0xfe, 0x02},{0xd1, 0x60},{0xd2, 0x60},{0xfe, 0x00}},//Saturation +5
};
static struct msm_camera_i2c_conf_array gc2035_saturation_confs[][1] = {
	{{gc2035_saturation[0], ARRAY_SIZE(gc2035_saturation[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[1], ARRAY_SIZE(gc2035_saturation[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[2], ARRAY_SIZE(gc2035_saturation[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[3], ARRAY_SIZE(gc2035_saturation[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[4], ARRAY_SIZE(gc2035_saturation[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[5], ARRAY_SIZE(gc2035_saturation[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[6], ARRAY_SIZE(gc2035_saturation[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[7], ARRAY_SIZE(gc2035_saturation[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[8], ARRAY_SIZE(gc2035_saturation[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[9], ARRAY_SIZE(gc2035_saturation[9]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_saturation[10], ARRAY_SIZE(gc2035_saturation[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int gc2035_saturation_enum_map[] = {
	MSM_V4L2_SATURATION_L0,
	MSM_V4L2_SATURATION_L1,
	MSM_V4L2_SATURATION_L2,
	MSM_V4L2_SATURATION_L3,
	MSM_V4L2_SATURATION_L4,
	MSM_V4L2_SATURATION_L5,
	MSM_V4L2_SATURATION_L6,
	MSM_V4L2_SATURATION_L7,
	MSM_V4L2_SATURATION_L8,
	MSM_V4L2_SATURATION_L9,
	MSM_V4L2_SATURATION_L10,
};
/*add end*/
static struct msm_sensor_output_info_t gc2035_dimensions[] = {
	{ /* For SNAPSHOT */
		.x_output = 0x640,  /*1600*/  /*for 2Mp*/ /*0x640*/
		.y_output = 0x4B0,  /*1200*/ /*0x4B0*/
		.line_length_pclk = 0x793,
		.frame_length_lines = 0x4D4,
		.vt_pixel_clk = 18000000,
		.op_pixel_clk = 18000000,
		.binning_factor = 0x0,
	},
	{ /* For PREVIEW 30fps*/
		.x_output = 0x320,  /*640*/  /*for 2Mp*/
		.y_output = 0x258,  /*480*/
		.line_length_pclk = 0x793,
		.frame_length_lines = 0x26A,
		.vt_pixel_clk = 36000000,
		.op_pixel_clk = 36000000,
		.binning_factor = 0x0,
	},
};

/*renwei add it for the gc2035 effect at 2012-8-11*/
static struct msm_camera_i2c_enum_conf_array gc2035_saturation_enum_confs = {
	.conf = &gc2035_saturation_confs[0][0],
	.conf_enum = gc2035_saturation_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_saturation_enum_map),
	.num_index = ARRAY_SIZE(gc2035_saturation_confs),
	.num_conf = ARRAY_SIZE(gc2035_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf gc2035_contrast[][3] = {
	{{0xfe, 0x02},{0xd3, 0x18},{0xfe, 0x00}},//Contrast -5
	{{0xfe, 0x02},{0xd3, 0x20},{0xfe, 0x00}},//Contrast -4
	{{0xfe, 0x02},{0xd3, 0x28},{0xfe, 0x00}},//Contrast -3
	{{0xfe, 0x02},{0xd3, 0x30},{0xfe, 0x00}},//Contrast -2
	{{0xfe, 0x02},{0xd3, 0x38},{0xfe, 0x00}},//Contrast -1
	{{0xfe, 0x02},{0xd3, 0x44},{0xfe, 0x00}},//Contrast
	{{0xfe, 0x02},{0xd3, 0x48},{0xfe, 0x00}},//Contrast -1
	{{0xfe, 0x02},{0xd3, 0x50},{0xfe, 0x00}},//Contrast -2
	{{0xfe, 0x02},{0xd3, 0x58},{0xfe, 0x00}},//Contrast -3
	{{0xfe, 0x02},{0xd3, 0x60},{0xfe, 0x00}},//Contrast -4
	{{0xfe, 0x02},{0xd3, 0x68},{0xfe, 0x00}},//Contrast -5
};

static struct msm_camera_i2c_conf_array gc2035_contrast_confs[][1] = {
	{{gc2035_contrast[0], ARRAY_SIZE(gc2035_contrast[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[1], ARRAY_SIZE(gc2035_contrast[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[2], ARRAY_SIZE(gc2035_contrast[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[3], ARRAY_SIZE(gc2035_contrast[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[4], ARRAY_SIZE(gc2035_contrast[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[5], ARRAY_SIZE(gc2035_contrast[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[6], ARRAY_SIZE(gc2035_contrast[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[7], ARRAY_SIZE(gc2035_contrast[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[8], ARRAY_SIZE(gc2035_contrast[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[9], ARRAY_SIZE(gc2035_contrast[9]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_contrast[10], ARRAY_SIZE(gc2035_contrast[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};


static int gc2035_contrast_enum_map[] = {
	MSM_V4L2_CONTRAST_L0,
	MSM_V4L2_CONTRAST_L1,
	MSM_V4L2_CONTRAST_L2,
	MSM_V4L2_CONTRAST_L3,
	MSM_V4L2_CONTRAST_L4,
	MSM_V4L2_CONTRAST_L5,
	MSM_V4L2_CONTRAST_L6,
	MSM_V4L2_CONTRAST_L7,
	MSM_V4L2_CONTRAST_L8,
	MSM_V4L2_CONTRAST_L9,
	MSM_V4L2_CONTRAST_L10,
};

static struct msm_camera_i2c_enum_conf_array gc2035_contrast_enum_confs = {
	.conf = &gc2035_contrast_confs[0][0],
	.conf_enum = gc2035_contrast_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_contrast_enum_map),
	.num_index = ARRAY_SIZE(gc2035_contrast_confs),
	.num_conf = ARRAY_SIZE(gc2035_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
static struct msm_camera_i2c_reg_conf gc2035_sharpness[][3] = {
	{{0xfe, 0x02},{0x97, 0x26},{0xfe, 0x00}},//Sharpness -2
	{{0xfe, 0x02},{0x97, 0x37},{0xfe, 0x00}},//Sharpness -1
	{{0xfe, 0x02},{0x97, 0x48},{0xfe, 0x00}},//Sharpness
	{{0xfe, 0x02},{0x97, 0x59},{0xfe, 0x00}},//Sharpness +1
	{{0xfe, 0x02},{0x97, 0x6a},{0xfe, 0x00}},//Sharpness +2
	{{0xfe, 0x02},{0x97, 0x7b},{0xfe, 0x00}},//Sharpness +3
};

static struct msm_camera_i2c_conf_array gc2035_sharpness_confs[][1] = {
	{{gc2035_sharpness[0], ARRAY_SIZE(gc2035_sharpness[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_sharpness[1], ARRAY_SIZE(gc2035_sharpness[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_sharpness[2], ARRAY_SIZE(gc2035_sharpness[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_sharpness[3], ARRAY_SIZE(gc2035_sharpness[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_sharpness[4], ARRAY_SIZE(gc2035_sharpness[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_sharpness[5], ARRAY_SIZE(gc2035_sharpness[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int gc2035_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
};

static struct msm_camera_i2c_enum_conf_array gc2035_sharpness_enum_confs = {
	.conf = &gc2035_sharpness_confs[0][0],
	.conf_enum = gc2035_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_sharpness_enum_map),
	.num_index = ARRAY_SIZE(gc2035_sharpness_confs),
	.num_conf = ARRAY_SIZE(gc2035_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf gc2035_exposure[][3] = {
	{{0xfe, 0x01},{0x13, 0x54},{0xfe, 0x00}},//Exposure -2
	{{0xfe, 0x01},{0x13, 0x64},{0xfe, 0x00}},//Exposure -1
	{{0xfe, 0x01},{0x13, 0x74},{0xfe, 0x00}},//Exposure
	{{0xfe, 0x01},{0x13, 0x84},{0xfe, 0x00}},//Exposure +1
	{{0xfe, 0x01},{0x13, 0xa0},{0xfe, 0x00}},//Exposure +2
};

static struct msm_camera_i2c_conf_array gc2035_exposure_confs[][1] = {
	{{gc2035_exposure[0], ARRAY_SIZE(gc2035_exposure[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_exposure[1], ARRAY_SIZE(gc2035_exposure[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_exposure[2], ARRAY_SIZE(gc2035_exposure[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_exposure[3], ARRAY_SIZE(gc2035_exposure[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_exposure[4], ARRAY_SIZE(gc2035_exposure[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int gc2035_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array gc2035_exposure_enum_confs = {
	.conf = &gc2035_exposure_confs[0][0],
	.conf_enum = gc2035_exposure_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_exposure_enum_map),
	.num_index = ARRAY_SIZE(gc2035_exposure_confs),
	.num_conf = ARRAY_SIZE(gc2035_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
#if 0
static struct msm_camera_i2c_reg_conf gc2035_iso[][3] = {

	{{0x3015, 0x02, INVMASK(0x0f)},{-1, -1, -1},{-1, -1, -1},}, /*ISO_AUTO*/
	{{-1, -1, -1},{-1, -1, -1},{-1, -1, -1},},		  /*ISO_DEBLUR*/
	{{0x3015, 0x01, INVMASK(0x0f)},}, /*ISO_100*/
	//{{0x3391, 0x04, INVMASK(0x04)},{0x3390, 0x49}, {0x339a, 0x30},},
	{{0x3015, 0x02, INVMASK(0x0f)},{-1, -1, -1},{-1, -1, -1},}, /*ISO_200*/
	{{0x3015, 0x04, INVMASK(0x0f)},{-1, -1, -1},{-1, -1, -1},}, /*ISO_400*/
	{{0x3015, 0x05, INVMASK(0x0f)},{-1, -1, -1},{-1, -1, -1},}, /*ISO_800*/
	{{0x3015, 0x05, INVMASK(0x0f)},}, /*ISO_1600*/
	//{{0x3391, 0x04, INVMASK(0x04)},{0x3390, 0x41}, {0x339a, 0x30},},
};


static struct msm_camera_i2c_conf_array gc2035_iso_confs[][1] = {
	{{gc2035_iso[0], ARRAY_SIZE(gc2035_iso[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_iso[1], ARRAY_SIZE(gc2035_iso[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_iso[2], ARRAY_SIZE(gc2035_iso[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_iso[3], ARRAY_SIZE(gc2035_iso[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_iso[4], ARRAY_SIZE(gc2035_iso[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_iso[5], ARRAY_SIZE(gc2035_iso[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int gc2035_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};


static struct msm_camera_i2c_enum_conf_array gc2035_iso_enum_confs = {
	.conf = &gc2035_iso_confs[0][0],
	.conf_enum = gc2035_iso_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_iso_enum_map),
	.num_index = ARRAY_SIZE(gc2035_iso_confs),
	.num_conf = ARRAY_SIZE(gc2035_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
#endif
static struct msm_camera_i2c_reg_conf gc2035_no_effect[] = {
	{0x83, 0xe0},
};

static struct msm_camera_i2c_conf_array gc2035_no_effect_confs[] = {
	{&gc2035_no_effect[0],
	ARRAY_SIZE(gc2035_no_effect), 0,
	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},
};

static struct msm_camera_i2c_reg_conf gc2035_special_effect[][1] = {
	{{0x83, 0xe0}},
	{{0x83, 0x12}},/*mono*/
	{{0x83, 0x01}},/*negative */
	{{-1, -1}},
	{{0x83, 0x82}},/*sepia*/
	{{-1, -1}},
	{{-1, -1}},
	{{-1, -1}},
	{{0x83, 0x52}}, /*greenish*/
	{{-1, -1}},
	{{-1, -1}},
	{{-1, -1}},
	{{-1, -1}},
};

static struct msm_camera_i2c_conf_array gc2035_special_effect_confs[][1] = {
	{{gc2035_special_effect[0],  ARRAY_SIZE(gc2035_special_effect[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[1],  ARRAY_SIZE(gc2035_special_effect[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[2],  ARRAY_SIZE(gc2035_special_effect[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[3],  ARRAY_SIZE(gc2035_special_effect[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[4],  ARRAY_SIZE(gc2035_special_effect[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[5],  ARRAY_SIZE(gc2035_special_effect[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[6],  ARRAY_SIZE(gc2035_special_effect[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[7],  ARRAY_SIZE(gc2035_special_effect[7]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[8],  ARRAY_SIZE(gc2035_special_effect[8]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[9],  ARRAY_SIZE(gc2035_special_effect[9]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[10], ARRAY_SIZE(gc2035_special_effect[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[11], ARRAY_SIZE(gc2035_special_effect[11]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_special_effect[12], ARRAY_SIZE(gc2035_special_effect[12]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int gc2035_special_effect_enum_map[] = {
	MSM_V4L2_EFFECT_OFF,
	MSM_V4L2_EFFECT_MONO,
	MSM_V4L2_EFFECT_NEGATIVE,
	MSM_V4L2_EFFECT_SOLARIZE,
	MSM_V4L2_EFFECT_SEPIA,
	MSM_V4L2_EFFECT_POSTERAIZE,
	MSM_V4L2_EFFECT_WHITEBOARD,
	MSM_V4L2_EFFECT_BLACKBOARD,
	MSM_V4L2_EFFECT_AQUA,
	MSM_V4L2_EFFECT_EMBOSS,
	MSM_V4L2_EFFECT_SKETCH,
	MSM_V4L2_EFFECT_NEON,
	MSM_V4L2_EFFECT_MAX,
};

static struct msm_camera_i2c_enum_conf_array
		 gc2035_special_effect_enum_confs = {
	.conf = &gc2035_special_effect_confs[0][0],
	.conf_enum = gc2035_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_special_effect_enum_map),
	.num_index = ARRAY_SIZE(gc2035_special_effect_confs),
	.num_conf = ARRAY_SIZE(gc2035_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf gc2035_antibanding[][17] = {
	{{0xfe, 0x00},{0x05, 0x01},{0x06, 0x25},{0x07, 0x00},{0x08, 0x14},{0xfe, 0x01},{0x27, 0x00},{0x28, 0x83},  
	{0x29, 0x04},{0x2a, 0x9b},{0x2b, 0x04},{0x2c, 0x9b},{0x2d, 0x05},{0x2e, 0xa1},{0x2f, 0x07},{0x30, 0x2a},{0xfe, 0x00}}, /*ANTIBANDING 60HZ*/
	{{0xfe, 0x00},{0x05, 0x01},{0x06, 0x08},{0x07, 0x00},{0x08, 0x14},{0xfe, 0x01},{0x27, 0x00},{0x28, 0x70},
	{0x29, 0x04},{0x2a, 0xd0},{0x2b, 0x04},{0x2c, 0xd0},{0x2d, 0x05},{0x2e, 0xb0},{0x2f, 0x07},{0x30, 0x00},{0xfe, 0x00}}, /*ANTIBANDING 50HZ*/
	{{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1},{-1, -1}}, /*ANTIBANDING AUTO*/
};


static struct msm_camera_i2c_conf_array gc2035_antibanding_confs[][1] = {
	{{gc2035_antibanding[0], ARRAY_SIZE(gc2035_antibanding[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_antibanding[1], ARRAY_SIZE(gc2035_antibanding[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_antibanding[2], ARRAY_SIZE(gc2035_antibanding[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int gc2035_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};


static struct msm_camera_i2c_enum_conf_array gc2035_antibanding_enum_confs = {
	.conf = &gc2035_antibanding_confs[0][0],
	.conf_enum = gc2035_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_antibanding_enum_map),
	.num_index = ARRAY_SIZE(gc2035_antibanding_confs),
	.num_conf = ARRAY_SIZE(gc2035_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf gc2035_wb_oem[][4] = {
	{{0xb3, 0x61},{0xb4, 0x40},{0xb5, 0x61},{0x82, 0xfc}},/*WHITEBALNACE OFF*/
	{{0xb3, 0x61},{0xb4, 0x40},{0xb5, 0x61},{0x82, 0xfe}},/*WHITEBALNACE AUTO*/
	{{0x82, 0xfc},{0xb3, 0xa0},{0xb4, 0x45},{0xb5, 0x40}},/*WHITEBALNACE CUSTOM*/
	{{0x82, 0xfc},{0xb3, 0x50},{0xb4, 0x40},{0xb5, 0xa8}},/*INCANDISCENT*/
	{{0x82, 0xfc},{0xb3, 0x72},{0xb4, 0x40},{0xb5, 0x5b}},/*FLOURESECT*/
	{{0x82, 0xfc},{0xb3, 0x58},{0xb4, 0x40},{0xb5, 0x50}},/*DAYLIGHT*/
	{{0x82, 0xfc},{0xb3, 0x8c},{0xb4, 0x50},{0xb5, 0x40}},/*CLOUDY*/
};

static struct msm_camera_i2c_conf_array gc2035_wb_oem_confs[][1] = {
	{{gc2035_wb_oem[0], ARRAY_SIZE(gc2035_wb_oem[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_wb_oem[1], ARRAY_SIZE(gc2035_wb_oem[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_wb_oem[2], ARRAY_SIZE(gc2035_wb_oem[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_wb_oem[3], ARRAY_SIZE(gc2035_wb_oem[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_wb_oem[4], ARRAY_SIZE(gc2035_wb_oem[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_wb_oem[5], ARRAY_SIZE(gc2035_wb_oem[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{gc2035_wb_oem[6], ARRAY_SIZE(gc2035_wb_oem[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int gc2035_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array gc2035_wb_oem_enum_confs = {
	.conf = &gc2035_wb_oem_confs[0][0],
	.conf_enum = gc2035_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(gc2035_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(gc2035_wb_oem_confs),
	.num_conf = ARRAY_SIZE(gc2035_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};


int gc2035_saturation_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
         printk("renwei %s\n",__func__);
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	if (value <= MSM_V4L2_SATURATION_L8)
		SAT_U = SAT_V = value * 0x10;
	printk("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}


int gc2035_contrast_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
         printk("renwei %s\n",__func__);
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int gc2035_sharpness_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
      	 printk("renwei %s\n",__func__);
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int gc2035_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	effect_value = value;
	 printk("renwei %s\n",__func__);
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->no_effect_settings, 0);
		if (rc < 0) {
			CDBG("write faield\n");
			return rc;
		}
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xda, SAT_U,
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xdb, SAT_V,
			MSM_CAMERA_I2C_BYTE_DATA);
	} else {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int gc2035_antibanding_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	 printk("renwei %s\n",__func__);
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int gc2035_flash_mode_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	led_flash_mode = value;
	printk("Sukha_gc2035 %s flash mode = %d\n", __func__, led_flash_mode);
	return rc;
}

int gc2035_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	 printk("renwei %s\n",__func__);
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		CDBG("write faield\n");
		return rc;
	}
	return rc;
}

struct msm_sensor_v4l2_ctrl_info_t gc2035_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L8,
		.step = 1,
		.enum_cfg_settings = &gc2035_saturation_enum_confs,
		.s_v4l2_ctrl = gc2035_saturation_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L8,
		.step = 1,
		.enum_cfg_settings = &gc2035_contrast_enum_confs,
		.s_v4l2_ctrl = gc2035_contrast_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L5,
		.step = 1,
		.enum_cfg_settings = &gc2035_sharpness_enum_confs,
		.s_v4l2_ctrl = gc2035_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &gc2035_exposure_enum_confs,
		.s_v4l2_ctrl = gc2035_msm_sensor_s_ctrl_by_enum,
	},
	#if 0
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &gc2035_iso_enum_confs,
		.s_v4l2_ctrl = gc2035_msm_sensor_s_ctrl_by_enum,
	},
	#endif
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_NEGATIVE,
		.step = 1,
		.enum_cfg_settings = &gc2035_special_effect_enum_confs,
		.s_v4l2_ctrl = gc2035_effect_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &gc2035_antibanding_enum_confs,
		.s_v4l2_ctrl = gc2035_antibanding_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &gc2035_wb_oem_enum_confs,
		.s_v4l2_ctrl = gc2035_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_LED_FLASH_MODE,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = gc2035_flash_mode_msm_sensor_s_ctrl_by_enum,
	},
};

/*add end*/
#if 0
static struct msm_sensor_output_reg_addr_t gc2035_reg_addr = {
	.x_output = 0x3808,
	.y_output = 0x380A,
	.line_length_pclk = 0x380C,
	.frame_length_lines = 0x380E,
};
#endif
static struct msm_camera_csi_params *gc2035_csi_params_array[] = {
	&gc2035_csi_params,
	&gc2035_csi_params,
};

static struct msm_sensor_id_info_t gc2035_id_info = {
	.sensor_id_reg_addr = 0xf0,
	.sensor_id = 0x2035,
};
#if 0
static struct msm_sensor_exp_gain_info_t gc2035_exp_gain_info = {
	.coarse_int_time_addr = 0x3002,
	.global_gain_addr = 0x3000,
	.vert_offset = 4,
};
#endif
static int32_t gc2035_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	CDBG_HIGH("gc2035_write_exp_gain : Not supported\n");
	return 0;
}

int32_t gc2035_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
		struct fps_cfg *fps)
{
	CDBG("gc2035_sensor_set_fps: Not supported\n");
	return 0;
}

static const struct i2c_device_id gc2035_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&gc2035_s_ctrl},
	{ }
};

int32_t gc2035_sensor_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("%s IN\r\n", __func__);

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);

	rc = msm_sensor_i2c_probe(client, id);

	if (client->dev.platform_data == NULL) {
		CDBG_HIGH("%s: NULL sensor data\n", __func__);
		return -EFAULT;
	}

	usleep_range(5000, 5100);

	return rc;
}

static struct i2c_driver gc2035_i2c_driver = {
	.id_table = gc2035_i2c_id,
	.probe  = gc2035_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client gc2035_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&gc2035_i2c_driver);
}

static struct v4l2_subdev_core_ops gc2035_subdev_core_ops = {
	/*renwei add it for the gc2035 effect at 2012-8-11*/
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	/*add end*/
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops gc2035_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops gc2035_subdev_ops = {
	.core = &gc2035_subdev_core_ops,
	.video  = &gc2035_subdev_video_ops,
};
int32_t gc2035_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = NULL;
	info = s_ctrl->sensordata;

	CDBG("%s IN\r\n", __func__);
	CDBG("%s, sensor_pwd:%d, sensor_reset:%d\r\n",__func__, info->sensor_pwd, info->sensor_reset);
	gpio_direction_output(info->sensor_pwd, 1);
	gpio_direction_output(info->sensor_reset, 0);
	//usleep_range(10000, 11000);
	msleep(20);
	if (info->pmic_gpio_enable) {
		lcd_camera_power_onoff(1);
	}
	usleep_range(10000, 11000);

	rc = msm_sensor_power_up(s_ctrl);
	if (rc < 0) {
		CDBG("%s: msm_sensor_power_up failed\n", __func__);
		return rc;
	}

	usleep_range(1000, 1100);
	/* turn on ldo and vreg */
	gpio_direction_output(info->sensor_pwd, 0);
	msleep(20);
	gpio_direction_output(info->sensor_reset, 1);
	msleep(25);

	return rc;

}

int32_t gc2035_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	struct msm_camera_sensor_info *info = NULL;

	CDBG("%s IN\r\n", __func__);
	info = s_ctrl->sensordata;

	msm_sensor_stop_stream(s_ctrl);
	msleep(40);

	gpio_direction_output(info->sensor_pwd, 1);
	usleep_range(5000, 5100);

	rc = msm_sensor_power_down(s_ctrl);
	msleep(40);
	if (s_ctrl->sensordata->pmic_gpio_enable){
		lcd_camera_power_onoff(0);
	}
	return rc;
}

void gc2035_set_shutter(struct msm_sensor_ctrl_t *s_ctrl)
{
	// write shutter, in number of line period
	unsigned short temp = 0;
	unsigned int shutter = 0;
	unsigned short ret_l,ret_h;

	ret_l = ret_h = 0;

	// turn off AEC & AGC
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0xb6, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			0x03, &ret_h, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			0x04, &ret_l, MSM_CAMERA_I2C_BYTE_DATA);
	shutter = (ret_h << 8) | (ret_l & 0xff) ;

	CDBG("gc2035_set_shutter shutter = %d\n",shutter);

	shutter = shutter / 2;

	if (shutter < 1)
		shutter = 1;
	shutter = shutter & 0x1fff;
	
	temp = shutter & 0xff;
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x04, temp, MSM_CAMERA_I2C_BYTE_DATA);
	temp = shutter >> 8;
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x03, temp, MSM_CAMERA_I2C_BYTE_DATA);
	CDBG("gc2035_set_shutter done\n");
}

int32_t gc2035_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(150);
	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msleep(5);
		msm_sensor_write_init_settings(s_ctrl);
		csi_config = 0;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
				s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(50);
			csi_config = 1;
		}

		if (res == MSM_SENSOR_RES_FULL)
			gc2035_set_shutter(s_ctrl);
		
		msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);
		
		if (res == MSM_SENSOR_RES_QTR)
		{
			//turn off flash when preview
            //Sukha_Camera
            printk("Sukha_gc2035 %s MSM_SENSOR_RES_QTR\n", __func__);
			msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_OFF);
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client, s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_QTR);
			msleep(300);
		}
		else if (res == MSM_SENSOR_RES_FULL)
		{
            printk("Sukha_gc2035 %s MSM_SENSOR_RES_FULL: led_flash_mode=%d\n", __func__, led_flash_mode);
			if (led_flash_mode == LED_MODE_ON)msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_HIGH);
			else if(led_flash_mode == LED_MODE_AUTO)gc2035_led_flash_auto(s_ctrl);
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client, s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_FULL);
			msleep(300);
		}
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE,
			&s_ctrl->sensordata->pdata->ioclk.vfe_clk_rate);

		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);

                
	}
	return rc;
}

static struct msm_sensor_fn_t gc2035_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = gc2035_sensor_set_fps,

	.sensor_write_exp_gain = gc2035_write_exp_gain,
	.sensor_write_snapshot_exp_gain = gc2035_write_exp_gain,

	.sensor_csi_setting = gc2035_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = gc2035_sensor_power_up,
	.sensor_power_down = gc2035_sensor_power_down,
};

static struct msm_sensor_reg_t gc2035_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = gc2035_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(gc2035_start_settings),
	.stop_stream_conf = gc2035_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(gc2035_stop_settings),
	.group_hold_on_conf = NULL,
	.group_hold_on_conf_size = 0,
	.group_hold_off_conf = NULL,
	.group_hold_off_conf_size = 0,
	.init_settings = &gc2035_init_conf[0],
	.init_size = ARRAY_SIZE(gc2035_init_conf),
	.mode_settings = &gc2035_confs[0],
	.no_effect_settings = &gc2035_no_effect_confs[0],
	.output_settings = &gc2035_dimensions[0],
	.num_conf = ARRAY_SIZE(gc2035_confs),
};

static struct msm_sensor_ctrl_t gc2035_s_ctrl = {
	.msm_sensor_reg = &gc2035_regs,
	.msm_sensor_v4l2_ctrl_info = gc2035_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(gc2035_v4l2_ctrl_info),
	.sensor_i2c_client = &gc2035_sensor_i2c_client,
	.sensor_i2c_addr = 0x78,
	.sensor_output_reg_addr = NULL,//&gc2035_reg_addr,
	.sensor_id_info = &gc2035_id_info,
	.sensor_exp_gain_info = NULL,//&gc2035_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &gc2035_csi_params_array[0],
	.msm_sensor_mutex = &gc2035_mut,
	.sensor_i2c_driver = &gc2035_i2c_driver,
	.sensor_v4l2_subdev_info = gc2035_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(gc2035_subdev_info),
	.sensor_v4l2_subdev_ops = &gc2035_subdev_ops,
	.func_tbl = &gc2035_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Galaxycore WXGA YUV sensor driver");
MODULE_LICENSE("GPL v2");
