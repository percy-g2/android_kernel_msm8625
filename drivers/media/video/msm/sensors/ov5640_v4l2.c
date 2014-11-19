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
#include "ov5640.h"
//#include <linux/leds.h>

#define SENSOR_NAME "ov5640"
#define PLATFORM_DRIVER_NAME "msm_camera_ov5640"
#define ov5640_obj ov5640_##obj
static int effect_value = CAMERA_EFFECT_OFF;

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

//#define OV5640_FAKE_8M

#define OV5640_VERBOSE_DGB
#ifdef OV5640_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif

static struct msm_sensor_ctrl_t * ov5640_v4l2_ctrl; //for OV5640 i2c read and write
#define AVG_LUM_THRESH 5
static int afinit = 1;
static int is_autoflash = 0;
static int is_autoflash_working = 0;
static int is_awb_enabled = 0;  //default auto
static int current_avg_luminance = AVG_LUM_THRESH +1;
static int is_effect_auto = 1;  //default auto

//const awb gain for RGB
static const int reg3400val = 0x07;
static const int reg3401val = 0x30;
static const int reg3402val = 0x04;
static const int reg3403val = 0x00;
static const int reg3404val = 0x05;
static const int reg3405val = 0x50;

#define	AE_TARGET	0x34
static int XVCLK = 2400;    // real clock/10000
static int current_target = AE_TARGET;
static int preview_sysclk, preview_HTS;
static int AE_high, AE_low;
static int preview_shutter, preview_gain16;

static struct msm_sensor_ctrl_t ov5640_s_ctrl;

#define  LED_MODE_OFF 0
#define  LED_MODE_AUTO 1
#define  LED_MODE_ON 2
#define  LED_MODE_TORCH 3
static int led_flash_mode = LED_MODE_OFF;
//extern struct rw_semaphore leds_list_lock;
//extern struct list_head leds_list;


DEFINE_MUTEX(ov5640_mut);

static struct msm_camera_i2c_reg_conf ov5640_start_settings[] = {
	{0x4202, 0x00},  /* streaming on */
};

static struct msm_camera_i2c_reg_conf ov5640_stop_settings[] = {
	{0x4202, 0x0f},  /* streaming off*/
};

#if 1
static struct msm_camera_i2c_reg_conf ov5640_prev_30fps_settings[] = {
	//@@ MIPI_2lane_5M to vga(YUV) 30fps
	//99 1280 960
	//98 0 0
	//{0x3008, 0x40, INVMASK(0x40)},
	{0x3503, 0x00}, //enable AE back from capture to preview
	{0x3035, 0x11},
	{0x3036, 0x38},
	{0x3820, 0x41},
	{0x3821, 0x07},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3803, 0x04},
	{0x3807, 0x9b},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x03},
	{0x380b, 0xc0},
	{0x380c, 0x07},
	{0x380d, 0x68},
	{0x380e, 0x03},
	{0x380f, 0xd8},
	{0x3813, 0x06},
	{0x3618, 0x00},
	{0x3612, 0x29},
	{0x3708, 0x64},
	{0x3709, 0x52},
	{0x370c, 0x03},
	{0x5001, 0xa3},
	{0x4004, 0x02},
	{0x4005, 0x18},
	{0x4837, 0x15},
	{0x4713, 0x03},
	{0x4407, 0x04},
	{0x460b, 0x35},
	{0x460c, 0x22},
	{0x3824, 0x02},
	//{0x3008, 0x00, INVMASK(0x40)},
};
#else
static struct msm_camera_i2c_reg_conf ov5640_prev_30fps_settings[] = {
	//sys_clk 24*70/3/2/2/2.5/2=28M, dvp_pclk=28M
	//frame rate: 28000000/1896/984=15fps
	//640x480 30fps
    {0x3503, 0x00}, //enable AE back from capture to preview
    {0x3035, 0x14},
    {0x3036, 0x38},
    //{0x3820, 0x41},
    //{0x3821, 0x07},
    {0x3820, 0x47},///0x3820, 0x41,Bit[2]:ISP vflip,Bit[1]:Sensor vflip
    {0x3821, 0x01},///0x3821, 0x07,Bit[2]:ISP mirror,Bit[1]:Sensor mirror
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3803, 0x04},
    {0x3807, 0x9b},
    {0x3808, 0x02},
    {0x3809, 0x80},
    {0x380a, 0x01},
    {0x380b, 0xe0},
    {0x380c, 0x07},
    {0x380d, 0x68},
    {0x380e, 0x03},
    {0x380f, 0xd8},
    {0x3813, 0x06},
    {0x3618, 0x00},
    {0x3612, 0x29},
    {0x3708, 0x64},
    {0x3709, 0x52},
    {0x370c, 0x03},
    {0x5001, 0xa3},
    {0x4004, 0x02},
    {0x4005, 0x18},
    {0x4837, 0x15},
    {0x4713, 0x03},
    {0x4407, 0x04},
    {0x460b, 0x35},
    {0x460c, 0x22},
    {0x3824, 0x02},
};
#endif
static struct msm_camera_i2c_reg_conf ov5640_prev_60fps_settings[] = {
	//{0x3008, 0x40, INVMASK(0x40)},
	{0x3814, 0x71},
	{0x3815, 0x35},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0x9f},
	{0x3808, 0x02},
	{0x3809, 0x80},
	{0x380a, 0x01},
	{0x380b, 0xe0},
	{0x380c, 0x07},
	{0x380d, 0x58},
	{0x380e, 0x01},
	{0x380f, 0xf0},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x5001, 0x83},
	{0x3034, 0x18},
	{0x3035, 0x22},
	{0x3036, 0x70},
	{0x3a02, 0x01},
	{0x3a03, 0xf0},
	{0x3a08, 0x01},
	{0x3a09, 0x2a},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf8},
	{0x3a0e, 0x01},
	{0x3a0d, 0x02},
	{0x3a13, 0x43},
	{0x3a14, 0x01},
	{0x3a15, 0xf0},
	{0x4837, 0x44},
	//{0x3008, 0x00, INVMASK(0x40)},
};

static struct msm_camera_i2c_reg_conf ov5640_prev_90fps_settings[] = {
	//{0x3008, 0x40, INVMASK(0x40)},

	{0x3814, 0x71},
	{0x3815, 0x35},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0x9f},
	{0x3808, 0x02},
	{0x3809, 0x80},
	{0x380a, 0x01},
	{0x380b, 0xe0},
	{0x380c, 0x07},
	{0x380d, 0x58},
	{0x380e, 0x01},
	{0x380f, 0xf0},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x5001, 0x83},
	{0x3034, 0x18},
	{0x3035, 0x12},
	{0x3036, 0x54},
	{0x3a02, 0x01},
	{0x3a03, 0xf0},
	{0x3a08, 0x01},
	{0x3a09, 0xbe},
	{0x3a0a, 0x01},
	{0x3a0b, 0x74},
	{0x3a0e, 0x01},
	{0x3a0d, 0x01},
	{0x3a13, 0x43},
	{0x3a14, 0x01},
	{0x3a15, 0xf0},
	{0x4837, 0x20},

	//{0x3008, 0x00, INVMASK(0x40)},
};

static struct msm_camera_i2c_reg_conf ov5640_snap_settings[] = {
	//@@ MIPI_2lane_5M(YUV) 7.5/15fps
	//99 2592 1944
	//98 0 0
	//{0x3008, 0x40, INVMASK(0x40)},

	{0x3035, 0x21}, //11
	{0x3036, 0x54},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3803, 0x00},
	{0x3807, 0x9f},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
#ifdef OV5640_FAKE_8M
	{0x380b, 0x90},
#else
	{0x380b, 0x98},
#endif
	{0x380c, 0x0b},
	{0x380d, 0x1c},
	{0x380e, 0x07},
	{0x380f, 0xb0},
	{0x3813, 0x04},
	{0x3618, 0x04},
	{0x3612, 0x2b},
	{0x3708, 0x21},
	{0x3709, 0x12},
	{0x370c, 0x00},
	{0x5001, 0x83},
	{0x4004, 0x06},
	{0x4005, 0x1a},
	{0x4837, 0x15}, //0a
	{0x4713, 0x02},
	{0x4407, 0x0c},
	{0x460b, 0x37},
	{0x460c, 0x20},
	{0x3824, 0x01},

	//{0x3008, 0x00, INVMASK(0x40)},

};

//set sensor init setting
static struct msm_camera_i2c_reg_conf ov5640_init_settings[] = {
	//VGA(YUV) 30fps
	//{0x3103, 0x11},
	//{0x3008, 0x82},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x11},
	{0x3036, 0x38},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x08},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x41},
	{0x3821, 0x07},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0x9b},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x03},
	{0x380b, 0xc0},
	{0x380c, 0x07},
	{0x380d, 0x68},
	{0x380e, 0x03},
	{0x380f, 0xd8},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x06},
	{0x3618, 0x00},
	{0x3612, 0x29},
	{0x3708, 0x64},
	{0x3709, 0x52},
	{0x370c, 0x03},
	{0x3a02, 0x03},
	{0x3a03, 0xd8},
	{0x3a08, 0x01},
	{0x3a09, 0x27},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0e, 0x03},
	{0x3a0d, 0x04},
	{0x3a14, 0x03},
	{0x3a15, 0xd8},
	{0x4001, 0x02},
	{0x4004, 0x02},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x45},
	{0x302e, 0x08},
	{0x4300, 0x30},
	{0x501f, 0x00},
	{0x4713, 0x03},
	{0x4407, 0x04},
	{0x440e, 0x00},
	{0x460b, 0x35},
	{0x460c, 0x22},
	{0x4837, 0x15},
	{0x3824, 0x02},
	{0x5000, 0xa7},
	{0x5001, 0xa3},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x26},
	{0x3a11, 0x60},
	{0x3a1f, 0x14},
	{0x3008, 0x02},
};

//image quality setting
static struct msm_camera_i2c_reg_conf ov5640_init_iq_settings[] = {
#if 0
	//Lens correction
	//OV5640 LENC setting
	{0x5800,0x3f},
	{0x5801,0x20},
	{0x5802,0x1a},
	{0x5803,0x1a},
	{0x5804,0x23},
	{0x5805,0x3f},
	{0x5806,0x11},
	{0x5807,0x0c},
	{0x5808,0x09},
	{0x5809,0x08},
	{0x580a,0x0d},
	{0x580b,0x12},
	{0x580c,0x0d},
	{0x580d,0x04},
	{0x580e,0x00},
	{0x580f,0x00},
	{0x5810,0x05},
	{0x5811,0x0d},
	{0x5812,0x0d},
	{0x5813,0x04},
	{0x5814,0x00},
	{0x5815,0x00},
	{0x5816,0x04},
	{0x5817,0x0d},
	{0x5818,0x13},
	{0x5819,0x0d},
	{0x581a,0x08},
	{0x581b,0x08},
	{0x581c,0x0c},
	{0x581d,0x13},
	{0x581e,0x3f},
	{0x581f,0x1f},
	{0x5820,0x1b},
	{0x5821,0x1c},
	{0x5822,0x23},
	{0x5823,0x3f},
	{0x5824,0x6a},
	{0x5825,0x06},
	{0x5826,0x08},
	{0x5827,0x06},
	{0x5828,0x2a},
	{0x5829,0x08},
	{0x582a,0x24},
	{0x582b,0x24},
	{0x582c,0x24},
	{0x582d,0x08},
	{0x582e,0x08},
	{0x582f,0x22},
	{0x5830,0x40},
	{0x5831,0x22},
	{0x5832,0x06},
	{0x5833,0x08},
	{0x5834,0x24},
	{0x5835,0x24},
	{0x5836,0x04},
	{0x5837,0x0a},
	{0x5838,0x86},
	{0x5839,0x08},
	{0x583a,0x28},
	{0x583b,0x28},
	{0x583c,0x66},
	{0x583d,0xce},
	//AEC
	{0x3a0f,0x38},
	{0x3a10,0x30},
	{0x3a11,0x61},
	{0x3a1b,0x38},
	{0x3a1e,0x30},
	{0x3a1f,0x10},
	//AWB
	{0x5180,0xff},
	{0x5181,0xf2},
	{0x5182,0x00},
	{0x5183,0x14},
	{0x5184,0x25},
	{0x5185,0x24},
	{0x5186,0x09},
	{0x5187,0x09},
	{0x5188,0x09},
	{0x5189,0x88},
	{0x518a,0x54},
	{0x518b,0xee},
	{0x518c,0xb2},
	{0x518d,0x50},
	{0x518e,0x34},
	{0x518f,0x6b},
	{0x5190,0x46},
	{0x5191,0xf8},
	{0x5192,0x04},
	{0x5193,0x70},
	{0x5194,0xf0},
	{0x5195,0xf0},
	{0x5196,0x03},
	{0x5197,0x01},
	{0x5198,0x04},
	{0x5199,0x6c},
	{0x519a,0x04},
	{0x519b,0x00},
	{0x519c,0x09},
	{0x519d,0x2b},
	{0x519e,0x38},

	//UV Adjust Auto Mode
	{0x5580,0x02}, //02 ;Sat enable
	{0x5588,0x01}, //40 ;enable UV adj
	{0x5583,0x40},	//	;offset high
	{0x5584,0x18},	//	;offset low
	{0x5589,0x18},	//	;gth1
	{0x558a,0x00},
	{0x358b,0xf8},	//	;gth2
	//OV5640 LENC setting
	{0x5800, 0x35},
	{0x5801, 0x16},
	{0x5802, 0x0f},
	{0x5803, 0x10},
	{0x5804, 0x17},
	{0x5805, 0x35},
	{0x5806, 0x0c},
	{0x5807, 0x07},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x09},
	{0x580b, 0x10},
	{0x580c, 0x06},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x0b},
	{0x5812, 0x06},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x00},
	{0x5816, 0x04},
	{0x5817, 0x09},
	{0x5818, 0x0d},
	{0x5819, 0x07},
	{0x581a, 0x05},
	{0x581b, 0x05},
	{0x581c, 0x09},
	{0x581d, 0x12},
	{0x581e, 0x34},
	{0x581f, 0x1a},
	{0x5820, 0x13},
	{0x5821, 0x13},
	{0x5822, 0x1b},
	{0x5823, 0x2f},
	{0x5824, 0x16},
	{0x5825, 0x25},
	{0x5826, 0x06},
	{0x5827, 0x25},
	{0x5828, 0x24},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x23},
	{0x582d, 0x24},
	{0x582e, 0x08},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x06},
	{0x5833, 0x28},
	{0x5834, 0x26},
	{0x5835, 0x23},
	{0x5836, 0x24},
	{0x5837, 0x25},
	{0x5838, 0x25},
	{0x5839, 0x46},
	{0x583a, 0x2a},
	{0x583b, 0x38},
	{0x583c, 0x50},
	{0x583d, 0xce},
#else
	{0x5490,0x20},
	{0x5481,0x3 },
	{0x5482,0xd },
	{0x5483,0x22},
	{0x5484,0x4b},
	{0x5485,0x60},
	{0x5486,0x6e},
	{0x5487,0x7a},
	{0x5488,0x85},
	{0x5489,0x8f},
	{0x548a,0x97},
	{0x548b,0xa6},
	{0x548c,0xb4},
	{0x548d,0xc9},
	{0x548e,0xda},
	{0x548f,0xe8},
	{0x5381,0x1c},
	{0x5382,0x5a},
	{0x5383,0x6 },
	{0x5384,0x9 },
	{0x5385,0x71},
	{0x5386,0x7a},
	{0x5387,0x7c},
	{0x5388,0x6c},
	{0x5389,0x10},
	{0x538b,0x98},
	{0x538a,0x1 },
	{0x5688,0x11},
	{0x5689,0x11},
	{0x569a,0x31},
	{0x569b,0x13},
	{0x569c,0x31},
	{0x569d,0x13},
	{0x569e,0x11},
	{0x569f,0x11},
	{0x583e,0x20},
	{0x583f,0x08},
	{0x5840,0x02},
	{0x5841,0x0d},
	{0x3a0f,0x33},
	{0x3a10,0x2e},
	{0x3a11,0x61},
	{0x3a1b,0x33},
	{0x3a1e,0x2e},
	{0x3a1f,0x10},
	//{0x5580,0x02},
	//{0x5588,0x00},
	//{0x5583,0x40},
	//{0x5584,0x28},
	//{0x5589,0x20},
	//{0x558a,0x00},
	//{0x558b,0x80},
	{0x5300,0x08},
	{0x5301,0x28},
	{0x5302,0x15},
	{0x5303,0x00},
	{0x5180,0xff},
	{0x5181,0xf2},
	{0x5182,0x0 },
	{0x5183,0x14},
	{0x5184,0x25},
	{0x5185,0x24},
	{0x5186,0xd },
	{0x5187,0x21},
	{0x5188,0x12},
	{0x5189,0x73},
	{0x518a,0x58},
	{0x518b,0xf3},
	{0x518c,0xce},
	{0x518d,0x3c},
	{0x518e,0x33},
	{0x518f,0x50},
	{0x5190,0x44},
	{0x5191,0xf8},
	{0x5192,0x4 },
	{0x5193,0xf0},
	{0x5194,0xf0},
	{0x5195,0xf0},
	{0x5196,0x3 },
	{0x5197,0x1 },
	{0x5198,0x6 },
	{0x5199,0xd0},
	{0x519a,0x4 },
	{0x519b,0x0 },
	{0x519c,0x4 },
	{0x519d,0x5f},
	{0x519e,0x38},
	{0x5800,0x3f},
	{0x5801,0x24},
	{0x5802,0x1b},
	{0x5803,0x1a},
	{0x5804,0x25},
	{0x5805,0x3f},
	{0x5806,0x13},
	{0x5807,0xb },
	{0x5808,0x7 },
	{0x5809,0x7 },
	{0x580a,0xb },
	{0x580b,0x15},
	{0x580c,0x9 },
	{0x580d,0x5 },
	{0x580e,0x0 },
	{0x580f,0x0 },
	{0x5810,0x5 },
	{0x5811,0xb },
	{0x5812,0xa },
	{0x5813,0x5 },
	{0x5814,0x0 },
	{0x5815,0x0 },
	{0x5816,0x4 },
	{0x5817,0xc },
	{0x5818,0x13},
	{0x5819,0xc },
	{0x581a,0x8 },
	{0x581b,0x8 },
	{0x581c,0xb },
	{0x581d,0x15},
	{0x581e,0x3f},
	{0x581f,0x25},
	{0x5820,0x1b},
	{0x5821,0x1b},
	{0x5822,0x24},
	{0x5823,0x3f},
	{0x5824,0x46},
	{0x5825,0x48},
	{0x5826,0x2c},
	{0x5827,0x48},
	{0x5828,0x26},
	{0x5829,0x39},
	{0x582a,0x39},
	{0x582b,0x36},
	{0x582c,0x38},
	{0x582d,0x3b},
	{0x582e,0x9 },
	{0x582f,0x33},
	{0x5830,0x40},
	{0x5831,0x33},
	{0x5832,0x2c},
	{0x5833,0x29},
	{0x5834,0x27},
	{0x5835,0x25},
	{0x5836,0x27},
	{0x5837,0x29},
	{0x5838,0x39},
	{0x5839,0x39},
	{0x583a,0x3a},
	{0x583b,0x39},
	{0x583c,0x59},
	{0x583d,0xce},
#endif

};


static struct msm_camera_i2c_conf_array ov5640_init_conf[] = {
	{&ov5640_init_settings[0],
	ARRAY_SIZE(ov5640_init_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov5640_init_iq_settings[0],
	ARRAY_SIZE(ov5640_init_iq_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array ov5640_confs[] = {
	{&ov5640_snap_settings[0],
	ARRAY_SIZE(ov5640_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov5640_prev_30fps_settings[0],
	ARRAY_SIZE(ov5640_prev_30fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov5640_prev_60fps_settings[0],
	ARRAY_SIZE(ov5640_prev_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov5640_prev_90fps_settings[0],
	ARRAY_SIZE(ov5640_prev_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_csi_params ov5640_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 2,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x6,
};

static struct v4l2_subdev_info ov5640_subdev_info[] = {
	{
	.code	= V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt	= 1,
	.order	  = 0,
	}
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t ov5640_dimensions[] = {
	{ /* For SNAPSHOT */
		.x_output = 0xA20,  /*2592*/  /*for 5Mp*/
#ifdef OV5640_FAKE_8M
		.y_output = 0x790,  /*1936*/
#else
		.y_output = 0x798,  /*1944*/
#endif
		.line_length_pclk = 0xb1c,///2844
		.frame_length_lines = 0x7b0,///1968
		.vt_pixel_clk = 84000000,
		.op_pixel_clk = 84000000,
		.binning_factor = 0x0,
	},
#if 1
	{ /* For PREVIEW 30fps*/
		.x_output = 0x500,  /*1280*/  /*for 5Mp*/
		.y_output = 0x3C0,  /*960*/
		.line_length_pclk = 0x768,
		.frame_length_lines = 0x3D8,
		.vt_pixel_clk = 56000000,
		.op_pixel_clk = 56000000,
		.binning_factor = 0x0,
	},
#else
	{ /* For PREVIEW 30 fps*/
		.x_output = 0x280,	/*640*/  /*for 5Mp*/
		.y_output = 0x1e0,	/*480*/
		.line_length_pclk = 0x768,
		.frame_length_lines = 0x3D8,
		.vt_pixel_clk = 28000000,
		.op_pixel_clk = 28000000,
		.binning_factor = 0x0,
	},
#endif

	{ /* For PREVIEW 60 fps*/
		.x_output = 0x280,	/*640*/  /*for 5Mp*/
		.y_output = 0x1e0,	/*480*/
		.line_length_pclk = 0x758,
		.frame_length_lines = 0x1f0,
		.vt_pixel_clk = 56000000,
		.op_pixel_clk = 56000000,
		.binning_factor = 0x0,
	},

	{ /* For PREVIEW 90 fps*/
		.x_output = 0x280,	/*640*/  /*for 5Mp*/
		.y_output = 0x1e0,	/*480*/
		.line_length_pclk = 0x758,
		.frame_length_lines = 0x1f0,
		.vt_pixel_clk = 84000000,
		.op_pixel_clk = 84000000,
		.binning_factor = 0x0,
	},

};

static struct msm_sensor_output_reg_addr_t ov5640_reg_addr = {
	.x_output = 0x3808,
	.y_output = 0x380A,
	.line_length_pclk = 0x380C,
	.frame_length_lines = 0x380E,
};

static struct msm_camera_csi_params *ov5640_csi_params_array[] = {
	&ov5640_csi_params,
	&ov5640_csi_params,
	&ov5640_csi_params, //60fps
	&ov5640_csi_params, //90fps

};

static struct msm_sensor_id_info_t ov5640_id_info = {
	.sensor_id_reg_addr = 0x300a,
	.sensor_id = 0x5640,
};

static struct msm_sensor_exp_gain_info_t ov5640_exp_gain_info = {
	.coarse_int_time_addr = 0x3500,
	.global_gain_addr = 0x350A,
	.vert_offset = 4,
};

static int32_t ov5640_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	CDBG("ov5640_write_exp_gain : Not supported\n");
	return 0;
}

int32_t ov5640_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
		struct fps_cfg *fps)
{
	CDBG("ov5640_sensor_set_fps: Not supported\n");
	return 0;
}

static const struct i2c_device_id ov5640_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov5640_s_ctrl},
	{ }
};

int32_t ov5640_sensor_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("%s IN\r\n", __func__);

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	s_ctrl->sensor_i2c_addr = s_ctrl->sensor_i2c_addr + 2;//offset to avoid i2c conflicts

	rc = msm_sensor_i2c_probe(client, id);

	if (client->dev.platform_data == NULL) {
		CDBG_HIGH("%s: NULL sensor data\n", __func__);
		return -EFAULT;
	}

	usleep_range(5000, 5100);

	return rc;
}

static struct i2c_driver ov5640_i2c_driver = {
	.id_table = ov5640_i2c_id,
	.probe  = ov5640_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov5640_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov5640_i2c_driver);
}

static struct v4l2_subdev_core_ops ov5640_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core = &ov5640_subdev_core_ops,
	.video  = &ov5640_subdev_video_ops,
};
int32_t ov5640_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = NULL;
	info = s_ctrl->sensordata;

	CDBG("%s IN\r\n", __func__);
	CDBG("%s, sensor_pwd:%d, sensor_reset:%d\r\n",__func__, info->sensor_pwd, info->sensor_reset);
	gpio_direction_output(info->sensor_pwd, 1);
	gpio_direction_output(info->sensor_reset, 0);
	usleep_range(10000, 11000);
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

int32_t ov5640_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t data = 0;

	struct msm_camera_sensor_info *info = NULL;

	CDBG("%s IN\r\n", __func__);
	info = s_ctrl->sensordata;

	msm_sensor_stop_stream(s_ctrl);
	msleep(40);

	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			0x300e, &data, MSM_CAMERA_I2C_BYTE_DATA);

	data |= 0x18; //set bit 3 bit 4 to 1

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x300e, data, MSM_CAMERA_I2C_BYTE_DATA);

	msleep(40);
	gpio_direction_output(info->sensor_reset,0);
	msleep(10);
	gpio_direction_output(info->sensor_reset,1);
	msleep(10);
	gpio_direction_output(info->sensor_reset,0);

	gpio_direction_output(info->sensor_pwd, 1);
	usleep_range(5000, 5100);

	rc = msm_sensor_power_down(s_ctrl);
	msleep(40);
	if (s_ctrl->sensordata->pmic_gpio_enable){
		lcd_camera_power_onoff(0);
	}
	return rc;
}

/* OV5640 dedicated code */
/********** Exposure optimization **********/
static int OV5640_read_i2c(unsigned int raddr)
{
	unsigned short data;
	msm_camera_i2c_read(ov5640_v4l2_ctrl->sensor_i2c_client,
		raddr, &data, MSM_CAMERA_I2C_BYTE_DATA);
	return (int)data;
}
static int OV5640_write_i2c(unsigned int waddr, unsigned int bdata)
{
	return msm_camera_i2c_write(ov5640_v4l2_ctrl->sensor_i2c_client,
		waddr, (unsigned short)bdata, MSM_CAMERA_I2C_BYTE_DATA);
}

int OV5640_get_sysclk(void)
{
	// calculate sysclk
	int temp1, temp2;
	int Multiplier, PreDiv, VCO, SysDiv, Pll_rdiv, sclk_rdiv, sysclk;
	int Bit_div2x = 4;//default
	int sclk_rdiv_map[] = {1, 2, 4, 8};

	temp1 = OV5640_read_i2c(0x3034);
	temp2 = temp1 & 0x0f;
	if (temp2 == 8 || temp2 == 10) {
		Bit_div2x = temp2 / 2;
	}

	temp1 = OV5640_read_i2c(0x3035);
	SysDiv = temp1>>4;
	if(SysDiv == 0) {
		SysDiv = 16;
	}

	temp1 = OV5640_read_i2c(0x3036);
	Multiplier = temp1;

	temp1 = OV5640_read_i2c(0x3037);
	PreDiv = temp1 & 0x0f;
	Pll_rdiv = ((temp1 >> 4) & 0x01) + 1;

	temp1 = OV5640_read_i2c(0x3108);
	temp2 = temp1 & 0x03;
	sclk_rdiv = sclk_rdiv_map[temp2];

	VCO = XVCLK * Multiplier / PreDiv;

	sysclk = VCO / SysDiv / Pll_rdiv * 2 / Bit_div2x / sclk_rdiv;

	CDBG("--CAMERA-- current sysclk is = %d\n", sysclk);

	return sysclk;
}


int OV5640_get_HTS(void)
{
	// read HTS from register settings
	int HTS;

	HTS = OV5640_read_i2c(0x380c);
	HTS = (HTS<<8) + OV5640_read_i2c(0x380d);
	CDBG("--CAMERA-- get_HTS = %d\n", HTS);

	return HTS;
}

int OV5640_get_VTS(void)
{
	// read VTS from register settings
	int VTS;

	VTS = OV5640_read_i2c(0x380e);
	VTS = (VTS<<8) + OV5640_read_i2c(0x380f);
	CDBG("--CAMERA-- get_VTS = %d\n", VTS);

	return VTS;
}

int OV5640_set_VTS(int VTS)
{
	// write VTS to registers
	int temp;

	CDBG("--CAMERA-- set_VTS = %d\n", VTS);
	temp = VTS & 0xff;
	OV5640_write_i2c(0x380f, temp);

	temp = VTS>>8;
	OV5640_write_i2c(0x380e, temp);

	return 0;
}


int OV5640_get_shutter(void)
{
	// read shutter, in number of line period
	int shutter;

	shutter = (OV5640_read_i2c(0x03500) & 0x0f);
	shutter = (shutter<<8) + OV5640_read_i2c(0x3501);
	shutter = (shutter<<4) + (OV5640_read_i2c(0x3502)>>4);

	CDBG("--CAMERA-- get_shutter: = %d\n", shutter);

	return shutter;
}

int OV5640_set_shutter(int shutter)
{
	// write shutter, in number of line period
	int temp;

	CDBG("--CAMERA-- set_shutter: = %d\n", shutter);
	shutter = shutter & 0xffff;

	temp = shutter & 0x0f;
	temp = temp<<4;
	OV5640_write_i2c(0x3502, temp);

	temp = shutter & 0xfff;
	temp = temp>>4;
	OV5640_write_i2c(0x3501, temp);

	temp = shutter>>12;
	OV5640_write_i2c(0x3500, temp);

	return 0;
}

int OV5640_get_gain16(void)
{
	// read gain, 16 = 1x
	int gain16;

	gain16 = OV5640_read_i2c(0x350a) & 0x03;
	gain16 = (gain16<<8) + OV5640_read_i2c(0x350b);

	CDBG("--CAMERA-- get_gain16: = %d\n", gain16);

	return gain16;
}

int OV5640_set_gain16(int gain16)
{
	// write gain, 16 = 1x
	int temp;

	CDBG("--CAMERA-- set_gain16: = %d\n", gain16);
	gain16 = gain16 & 0x3ff;

	temp = gain16 & 0xff;
	OV5640_write_i2c(0x350b, temp);

	temp = gain16>>8;
	OV5640_write_i2c(0x350a, temp);

	return 0;
}

//For exposure conversion
void OV5640_set_bandingfilter(void)
{
	int preview_VTS;
	int band_step60, max_band60, band_step50, max_band50;

	// read preview PCLK
	preview_sysclk = OV5640_get_sysclk();

	// read preview HTS
	preview_HTS = OV5640_get_HTS();

	// read preview VTS
	preview_VTS = OV5640_get_VTS();

	// calculate banding filter
	// 60Hz
	band_step60 = preview_sysclk * 100/preview_HTS * 100/120;
	OV5640_write_i2c(0x3a0a, (band_step60 >> 8));
	OV5640_write_i2c(0x3a0b, (band_step60 & 0xff));

	max_band60 = (int)((preview_VTS-4)/band_step60);
	OV5640_write_i2c(0x3a0d, max_band60);

	// 50Hz
	band_step50 = preview_sysclk * 100/preview_HTS;
	OV5640_write_i2c(0x3a08, (band_step50 >> 8));
	OV5640_write_i2c(0x3a09, (band_step50 & 0xff));

	max_band50 = (int)((preview_VTS-4)/band_step50);
	OV5640_write_i2c(0x3a0e, max_band50);
}


int OV5640_set_AE_target(int target)
{
	// stable in high
	int fast_high, fast_low;

	CDBG("--CAMERA-- set_AE_target: = %d\n", target);

	AE_low = target * 23 / 25;    // 0.92
	AE_high = target * 27 / 25;    // 1.08

	fast_high = AE_high<<1;
	if(fast_high>255)
		fast_high = 255;

	fast_low = AE_low>>1;

	OV5640_write_i2c(0x3a0f, AE_high);
	OV5640_write_i2c(0x3a10, AE_low);
	OV5640_write_i2c(0x3a1b, AE_high);
	OV5640_write_i2c(0x3a1e, AE_low);
	OV5640_write_i2c(0x3a11, fast_high);
	OV5640_write_i2c(0x3a1f, fast_low);

	return 0;
}

void OV5640_set_EV(int ev)
{
	CDBG("--CAMERA-- set_EV: = %d\n", ev);

	switch (ev)
	{
		case 0://+5
			current_target = AE_TARGET * 16/5;
			OV5640_set_AE_target(current_target);
			break;

		case 1://+4
			current_target = AE_TARGET * 5/2;
			OV5640_set_AE_target(current_target);
			break;

		case 2://+3
			current_target = AE_TARGET * 2;
			OV5640_set_AE_target(current_target);
			break;

		case 3://+2
			current_target = AE_TARGET * 8/5;
			OV5640_set_AE_target(current_target);
			break;

		case 4://+1
			current_target = AE_TARGET * 5/4;
			OV5640_set_AE_target(current_target);
			break;

		case 5://+0
			current_target = AE_TARGET;
			OV5640_set_AE_target(current_target);
			break;

		case 6://-1
			current_target = AE_TARGET * 4/5;
			OV5640_set_AE_target(current_target);
			break;

		case 7://-2
			current_target = AE_TARGET * 5/8;
			OV5640_set_AE_target(current_target);
			break;

		case 8://-3
			current_target = AE_TARGET / 2;
			OV5640_set_AE_target(current_target);
			break;

		case 9://-4
			current_target = AE_TARGET * 2/5;
			OV5640_set_AE_target(current_target);
			break;

		case 10://-5
			current_target = AE_TARGET * 5/16;
			OV5640_set_AE_target(current_target);
			break;

		default:
			break;
	}
}


int OV5640_get_light_frequency(void)
{
	// get banding filter value
	int temp, temp1, light_frequency;

	temp = OV5640_read_i2c(0x3c01);

	if (temp & 0x80) {
		// manual
		temp1 = OV5640_read_i2c(0x3c00);
		if (temp1 & 0x04) {
		// 50Hz
		light_frequency = 50;
		CDBG("--CAMERA-- get_light_frequency: 50Hz Manual\n");
		}
		else {
			// 60Hz
			light_frequency = 60;
			CDBG("--CAMERA-- get_light_frequency: 60Hz Manual\n");
		}
	}
	else
	{
		// auto
		temp1 = OV5640_read_i2c(0x3c0c);
		if (temp1 & 0x01) {
			// 50Hz
			light_frequency = 50;
			CDBG("--CAMERA-- get_light_frequency: 50Hz Auto\n");
		}
		else {
			// 60Hz
			light_frequency = 60;
			CDBG("--CAMERA-- get_light_frequency: 60Hz Auto\n");
		}
	}
	return light_frequency;
}

#if 0
static int ov5640_read_average_luminance(void)
{
	int rc = 0;
	CDBG("--CAMERA-- %s (Start...)\n",__func__);

	current_avg_luminance = OV5640_read_i2c(0x56a1);
	CDBG("--CAMERA-- %s current_avg_luminance = %d \n", __func__, current_avg_luminance);

	return rc;
}
#endif

int OV5640_capture(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;
	unsigned short tmp;
	long exposure_adj;

	//int preview_shutter, preview_gain16, average;
	unsigned short average;
	int capture_shutter, capture_gain16;
	int capture_sysclk, capture_HTS, capture_VTS;
	int light_frequency, capture_bandingfilter, capture_max_band;
	long capture_gain16_shutter;

	CDBG("--CAMERA-- ======================================\n");

	CDBG("--CAMERA-- snapshot in, is_autoflash - %d\n", is_autoflash);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,//AE manual mode
		0x3503, 0x3, MSM_CAMERA_I2C_BYTE_DATA);

	//AE below
	// read preview shutter
	preview_shutter = OV5640_get_shutter();

	// read preview gain
	preview_gain16 = OV5640_get_gain16();

	// get average
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			0x56a1, &average, MSM_CAMERA_I2C_BYTE_DATA);

	CDBG("OV5640_capture.. 0x56a1 =%d \n", average);

#if 0
	if (is_autoflash == 1) {
		OV5640_read_i2c( 0x350b, &tmp);
		CDBG("--CAMERA-- GAIN VALUE : %x\n", tmp);
		if ((tmp & 0x80) != 0) {
			ov5640_set_flash_light(LED_FULL);
		}
	}
#endif

	/* snapshot setting */
	msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_FULL);

	mdelay(3);

	//if awb mode is auto, check the average luminace
	if(is_awb_enabled){
		CDBG("%s: is_awb_enabled =%d, current_avg_luminance =%d, is_effect_auto=%d \n",
			__func__, is_awb_enabled, current_avg_luminance, is_effect_auto);

		if(current_avg_luminance <= AVG_LUM_THRESH){
			msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
				0x3406, &tmp, MSM_CAMERA_I2C_BYTE_DATA);
			tmp = tmp | 0x01;
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3406,tmp, MSM_CAMERA_I2C_BYTE_DATA);

			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3400,reg3400val, MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3401,reg3401val, MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3402,reg3402val, MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3403,reg3403val, MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3404,reg3404val, MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3405,reg3405val, MSM_CAMERA_I2C_BYTE_DATA);

			CDBG("%s: reg3400val =%d, reg3401val=%d, reg3402val=%d, reg3403val=%d, reg3404val=%d,reg3405val=%d \n",
				__func__,reg3400val,reg3401val, reg3402val,reg3403val,reg3404val,reg3405val);

			if(is_effect_auto){
				msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					0x5583,0x28, MSM_CAMERA_I2C_BYTE_DATA);
				msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					0x5584,0x28, MSM_CAMERA_I2C_BYTE_DATA);

				msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
					0x5588, &tmp, MSM_CAMERA_I2C_BYTE_DATA);
				tmp = tmp | 0x40;

				msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					0x5588, tmp, MSM_CAMERA_I2C_BYTE_DATA);
				CDBG("OV5640_capture. effect is auto \n");
			}
		}
	}

	// read capture VTS
	capture_HTS = OV5640_get_HTS();
	capture_VTS = OV5640_get_VTS();
	capture_sysclk = OV5640_get_sysclk();

	// calculate capture banding filter
	light_frequency = OV5640_get_light_frequency();
	if (light_frequency == 60) {
		// 60Hz
		capture_bandingfilter = capture_sysclk * 100 / capture_HTS * 100 / 120;
	}
	else {
		// 50Hz
		capture_bandingfilter = capture_sysclk * 100 / capture_HTS;
	}
	capture_max_band = (int)((capture_VTS - 4)/capture_bandingfilter);

	// calculate capture shutter/gain16
	if (average > AE_low && average < AE_high) {
		// in stable range
		capture_gain16_shutter = preview_gain16 * preview_shutter * capture_sysclk/preview_sysclk * preview_HTS/capture_HTS * AE_TARGET / average;
	}
	else {
		capture_gain16_shutter = preview_gain16 * preview_shutter * capture_sysclk/preview_sysclk * preview_HTS/capture_HTS;
	}

	if(is_autoflash_working)
	{
		//auto flash working, re-calibrate exposure
		//*0.8
		exposure_adj = capture_gain16_shutter * 4;
		capture_gain16_shutter = exposure_adj / 5;
	}

	// gain to shutter
	if(capture_gain16_shutter < (capture_bandingfilter * 16)) {
		// shutter < 1/100
		capture_shutter = capture_gain16_shutter/16;
		if(capture_shutter <1)
			capture_shutter = 1;

		capture_gain16 = capture_gain16_shutter/capture_shutter;
		if(capture_gain16 < 16)
			capture_gain16 = 16;
	}
	else {
		if(capture_gain16_shutter > (capture_bandingfilter*capture_max_band*16)) {
			// exposure reach max
			capture_shutter = capture_bandingfilter*capture_max_band;
			capture_gain16 = capture_gain16_shutter / capture_shutter;
		}
		else {
			// 1/100 < capture_shutter =< max, capture_shutter = n/100
			capture_shutter = ((int) (capture_gain16_shutter/16/capture_bandingfilter)) * capture_bandingfilter;
			capture_gain16 = capture_gain16_shutter / capture_shutter;
		}
	}

	// write capture gain
	OV5640_set_gain16(capture_gain16);

	// write capture shutter
	if (capture_shutter > (capture_VTS - 4)) {
		capture_VTS = capture_shutter + 4;
		OV5640_set_VTS(capture_VTS);
	}
	OV5640_set_shutter(capture_shutter);

	//OV5640_set_EV(8);

	return rc;
}

static int ov5640_af_setting(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	CDBG("--CAMERA-- ov5640_af_setting\n");

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x3000, 0x20, MSM_CAMERA_I2C_BYTE_DATA);

	rc = msm_camera_i2c_txdata(s_ctrl->sensor_i2c_client,
			ov5640_afinit_tbl, sizeof(ov5640_afinit_tbl)/sizeof(ov5640_afinit_tbl[0]));
	if (rc < 0)
	{
		CDBG_HIGH("--CAMERA-- AF_init failed\n");
		return rc;
	}

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_MAIN,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_ACK,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_PARA0,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_PARA1,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_PARA2,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_PARA3,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_PARA4,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		OV5640_CMD_FW_STATUS,0x7f, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x3000,0x00, MSM_CAMERA_I2C_BYTE_DATA);

	return rc;
}

#if 0//not used
static int ov5640_set_flash_light(enum led_brightness brightness)
{
	struct led_classdev *led_cdev;

	CDBG("--CAMERA-- ov5640_set_flash_light brightness = %d\n", brightness);

	down_read(&leds_list_lock);
	list_for_each_entry(led_cdev, &leds_list, node) {
		if (!strncmp(led_cdev->name, "flashlight", 10)) {
			break;
		}
	}
	up_read(&leds_list_lock);

	if (led_cdev) {
		led_brightness_set(led_cdev, brightness);
	} else {
		CDBG_HIGH("--CAMERA-- get flashlight device failed\n");
		return -1;
	}
	return 0;
}
#endif
static int ov5640_led_flash_auto(struct msm_camera_sensor_flash_data *fdata)
{
	int tmp;
	CDBG("--CAMERA-- ov5640_led_flash_ctrl led_flash_mode = %d\n", led_flash_mode);
	
	tmp = OV5640_read_i2c(0x56a1);
	CDBG("--ov5640_led_flash_auto-- GAIN VALUE : %d\n", tmp);
	if (tmp < 40) {
		//ov5640_set_flash_light(LED_FULL);
		msm_camera_flash_set_led_state(fdata, MSM_CAMERA_LED_HIGH);
	}
	return 0;
}

/*sensor settings for ov5640*/
int32_t ov5640_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	uint16_t data = 0;
	static int csi_config;

	CDBG("--CAMERA-- update_type = %d, res = %d\n", update_type, res);
	if(NULL == s_ctrl)
	{
		CDBG("%s: parameter error\n", __func__);
		return -1;
	}
	//Prepare clk and data stop
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x4800, 0x24, MSM_CAMERA_I2C_BYTE_DATA);
	msleep(10);

	ov5640_v4l2_ctrl = s_ctrl;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);//standby

	if (update_type == MSM_SENSOR_REG_INIT)
	{
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3103, 0x11, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x3008, 0x82, MSM_CAMERA_I2C_BYTE_DATA);
		msleep(5);

		msm_sensor_enable_debugfs(s_ctrl);

		//set sensor init&quality setting
		CDBG("--CAMERA-- set sensor init setting\n");
		msm_sensor_write_init_settings(s_ctrl);

		csi_config = 0;

		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
				0x4740, &data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			CDBG("%s: read data from sensor failed\n", __func__);
			return rc;
		}

		CDBG("--CAMERA-- init 0x4740 value=0x%x\n", data);

		if (data != 0x21)
		{
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					0x4740, 0x21, MSM_CAMERA_I2C_BYTE_DATA);
			msleep(10);
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
					0x4740, &data, MSM_CAMERA_I2C_BYTE_DATA);
			CDBG("--CAMERA-- WG 0x4740 value=0x%x\n", data);
		}

		CDBG("--CAMERA-- AF_init: afinit = %d\n", afinit);
		if (afinit == 1)
		{
			rc = ov5640_af_setting(s_ctrl);
			if (rc < 0) {
				CDBG_HIGH("ov5640_af_setting failed\n");
				return rc;
			}
			afinit = 0;
		}
	}
	else if (update_type == MSM_SENSOR_UPDATE_PERIODIC)
	{
		CDBG("PERIODIC : %d\n", res);
		//wait for clk/data really stop
		if ((res == MSM_SENSOR_RES_FULL) || (csi_config == 0) )
		{
			msleep(66);
		}
		else
		{
			msleep(266);
		}

		if (!csi_config)
		{
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
				CDBG("CSI config in progress\n");
				v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
					NOTIFY_CSIC_CFG, s_ctrl->curr_csic_params);
				CDBG("CSI config is done\n");
				mb();
				msleep(30);
				csi_config = 1;
		}

		if (res != MSM_SENSOR_RES_FULL)
		{
			//turn off flash when preview
			//ov5640_set_flash_light(LED_OFF);
			msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_OFF);
			is_autoflash_working = 0;
		}

		if (res == MSM_SENSOR_RES_QTR)
		{
			/* preview setting */
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,
				s_ctrl->msm_sensor_reg->mode_settings, res);

			//Test Begin:restore preview shutter
			OV5640_set_gain16(preview_gain16);
			OV5640_set_shutter(preview_shutter);
			//Test End
			//mdelay(30);

			// calculate banding filter
			OV5640_set_bandingfilter();

			// set ae_target
			OV5640_set_AE_target(current_target);
			OV5640_write_i2c(0x3503, 0x0); //AE auto mode
		} else if (res == MSM_SENSOR_RES_FULL)
		{
			if (led_flash_mode == LED_MODE_ON)msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_HIGH);
			else if(led_flash_mode == LED_MODE_AUTO)ov5640_led_flash_auto(s_ctrl->sensordata->flash_data);
			
			rc = OV5640_capture(s_ctrl);
			/* Delay to get Snapshot Working */
			//msleep(260);
		} else if ((res == MSM_SENSOR_RES_2)||(res == MSM_SENSOR_RES_3))
		{ //60fps and 90fps
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,
				s_ctrl->msm_sensor_reg->mode_settings, res);
		}

		//if awb mode is auto, check status of 0x3406
		if((res != MSM_SENSOR_RES_FULL) && is_awb_enabled)
		{
			CDBG("--CAMERA-- set auto awb mode. \n");
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
					0x3406, &data, MSM_CAMERA_I2C_BYTE_DATA);
			data = data & 0xfe;

			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					0x3406, data, MSM_CAMERA_I2C_BYTE_DATA);

			current_avg_luminance = AVG_LUM_THRESH +1; //set default avg lum larger than threshold
		}
		//open stream
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(10);

		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x4800, 0x04, MSM_CAMERA_I2C_BYTE_DATA);
		msleep(500);
	}

	return rc;
}

static struct msm_camera_i2c_reg_conf ov5640_saturation[][5] = {
	{
		{0x5001, 0xa7},
		{0x5583, 0x00},
		{0x5584, 0x00},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL0*/
	{
		{0x5001, 0xa7},
		{0x5583, 0x10},
		{0x5584, 0x10},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL1*/
		{
		{0x5001, 0xa7},
		{0x5583, 0x15},
		{0x5584, 0x15},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL2*/
		{
		{0x5001, 0xa7},
		{0x5583, 0x20},
		{0x5584, 0x20},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL3*/
		{
		{0x5001, 0xa7},
		{0x5583, 0x30},
		{0x5584, 0x30},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL4*/
		{
		{0x5001, 0xa7},
		{0x5583, 0x40},
		{0x5584, 0x40},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL5*/
		{
		{0x5001, 0xa7},
		{0x5583, 0x60},
		{0x5584, 0x60},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL6*/
		{
		{0x5001, 0xa7},
		{0x5583, 0x80},
		{0x5584, 0x80},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL7*/
		{
		{0x5001, 0xa7},
		{0x5583, 0xa0},
		{0x5584, 0xa0},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL8*/
		{
		{0x5001, 0xa7},
		{0x5583, 0xc0},
		{0x5584, 0xc0},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL9*/
		{
		{0x5001, 0xa7},
		{0x5583, 0xe0},
		{0x5584, 0xe0},
		{0x5580, 0x02},
		{0x5588, 0x40},
	},	/* SATURATION LEVEL10*/
};
static struct msm_camera_i2c_conf_array ov5640_saturation_confs[][1] = {
	{{ov5640_saturation[0], ARRAY_SIZE(ov5640_saturation[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[1], ARRAY_SIZE(ov5640_saturation[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[2], ARRAY_SIZE(ov5640_saturation[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[3], ARRAY_SIZE(ov5640_saturation[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[4], ARRAY_SIZE(ov5640_saturation[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[5], ARRAY_SIZE(ov5640_saturation[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[6], ARRAY_SIZE(ov5640_saturation[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[7], ARRAY_SIZE(ov5640_saturation[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[8], ARRAY_SIZE(ov5640_saturation[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[9], ARRAY_SIZE(ov5640_saturation[9]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_saturation[10], ARRAY_SIZE(ov5640_saturation[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov5640_saturation_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov5640_saturation_enum_confs = {
	.conf = &ov5640_saturation_confs[0][0],
	.conf_enum = ov5640_saturation_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_saturation_enum_map),
	.num_index = ARRAY_SIZE(ov5640_saturation_confs),
	.num_conf = ARRAY_SIZE(ov5640_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov5640_contrast[][5] = {
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x0a}, {0x5585, 0x0a},{0x5588, 0x00},},	/* CONTRAST L0*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x10}, {0x5585, 0x10},{0x5588, 0x00},},	/* CONTRAST L1*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x14}, {0x5585, 0x14},{0x5588, 0x00},},	/* CONTRAST L2*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x18}, {0x5585, 0x18},{0x5588, 0x00},},	/* CONTRAST L3*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x1c}, {0x5585, 0x1c},{0x5588, 0x00},},	/* CONTRAST L4*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x20}, {0x5585, 0x00},{0x5588, 0x00},},	/* CONTRAST L5*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x24}, {0x5585, 0x10},{0x5588, 0x00},},	/* CONTRAST L6*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x28}, {0x5585, 0x10},{0x5588, 0x00},},	/* CONTRAST L7*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x30}, {0x5585, 0x14},{0x5588, 0x00},},	/* CONTRAST L8*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x34}, {0x5585, 0x1a},{0x5588, 0x00},}, /* CONTRAST L9*/
	{{0x5001, 0x83},{0x5580, 0x04},{0x5586, 0x38}, {0x5585, 0x20},{0x5588, 0x00},},	/* CONTRAST L10*/
};

static struct msm_camera_i2c_conf_array ov5640_contrast_confs[][1] = {
	{{ov5640_contrast[0], ARRAY_SIZE(ov5640_contrast[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[1], ARRAY_SIZE(ov5640_contrast[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[2], ARRAY_SIZE(ov5640_contrast[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[3], ARRAY_SIZE(ov5640_contrast[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[4], ARRAY_SIZE(ov5640_contrast[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[5], ARRAY_SIZE(ov5640_contrast[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[6], ARRAY_SIZE(ov5640_contrast[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[7], ARRAY_SIZE(ov5640_contrast[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[8], ARRAY_SIZE(ov5640_contrast[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[9], ARRAY_SIZE(ov5640_contrast[9]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_contrast[10], ARRAY_SIZE(ov5640_contrast[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};


static int ov5640_contrast_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov5640_contrast_enum_confs = {
	.conf = &ov5640_contrast_confs[0][0],
	.conf_enum = ov5640_contrast_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_contrast_enum_map),
	.num_index = ARRAY_SIZE(ov5640_contrast_confs),
	.num_conf = ARRAY_SIZE(ov5640_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
static struct msm_camera_i2c_reg_conf ov5640_sharpness[][9] = {
		{{0x5308, 0x75,},{0x5302, 0x00},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},	    /* SHARPNESS LEVEL 0*/
		{{0x5308, 0x75,},{0x5302, 0x03},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},	  /* SHARPNESS LEVEL 1*/
		{{0x5308, 0x00},{0x5300, 0x08},{0x5301, 0x30},{0x5302, 0x10},{0x5303, 0x00},{0x5309, 0x08},{0x530a, 0x30},{0x530b, 0x04},{0x530c, 0x06}},		  /* SHARPNESS LEVEL 2*/
		{{0x5308, 0x75,},{0x5302, 0x30},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},		   /* SHARPNESS LEVEL 3*/
		{{0x5308, 0x75,},{0x5302, 0x40},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},	  /* SHARPNESS LEVEL 4*/
		{{0x5308, 0x75,},{0x5302, 0x50},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},	   /* SHARPNESS LEVEL 5*/
		{{0x5308, 0x75,},{0x5302, 0xf0},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},
};

static struct msm_camera_i2c_conf_array ov5640_sharpness_confs[][1] = {
	{{ov5640_sharpness[0], ARRAY_SIZE(ov5640_sharpness[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_sharpness[1], ARRAY_SIZE(ov5640_sharpness[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_sharpness[2], ARRAY_SIZE(ov5640_sharpness[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_sharpness[3], ARRAY_SIZE(ov5640_sharpness[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_sharpness[4], ARRAY_SIZE(ov5640_sharpness[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_sharpness[5], ARRAY_SIZE(ov5640_sharpness[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_sharpness[6], ARRAY_SIZE(ov5640_sharpness[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov5640_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
	MSM_V4L2_SHARPNESS_L6,
};

static struct msm_camera_i2c_enum_conf_array ov5640_sharpness_enum_confs = {
	.conf = &ov5640_sharpness_confs[0][0],
	.conf_enum = ov5640_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_sharpness_enum_map),
	.num_index = ARRAY_SIZE(ov5640_sharpness_confs),
	.num_conf = ARRAY_SIZE(ov5640_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov5640_exposure[][6] = {
	{
		{0x3a0f,0x10},
		{0x3a10,0x08},
		{0x3a11,0x20},
		{0x3a1b,0x10},
		{0x3a1e,0x08},
		{0x3a1f,0x10},
	},
	{
		{0x3a0f,0x20},
		{0x3a10,0x18},
		{0x3a11,0x41},
		{0x3a1b,0x20},
		{0x3a1e,0x18},
		{0x3a1f,0x10},
	},
	{
		{0x3a0f,0x30},
		{0x3a10,0x28},
		{0x3a11,0x60},
		{0x3a1b,0x30},
		{0x3a1e,0x28},
		{0x3a1f,0x14},
	},

	{
		{0x3a0f,0x60},
		{0x3a10,0x58},
		{0x3a11,0xa0},
		{0x3a1b,0x60},
		{0x3a1e,0x58},
		{0x3a1f,0x20},
	},
	{
		{0x3a0f,0x50},
		{0x3a10,0x48},
		{0x3a11,0x90},
		{0x3a1b,0x50},
		{0x3a1e,0x48},
		{0x3a1f,0x20},
	},
};

static struct msm_camera_i2c_conf_array ov5640_exposure_confs[][1] = {
	{{ov5640_exposure[0], ARRAY_SIZE(ov5640_exposure[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_exposure[1], ARRAY_SIZE(ov5640_exposure[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_exposure[2], ARRAY_SIZE(ov5640_exposure[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_exposure[3], ARRAY_SIZE(ov5640_exposure[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_exposure[4], ARRAY_SIZE(ov5640_exposure[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov5640_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array ov5640_exposure_enum_confs = {
	.conf = &ov5640_exposure_confs[0][0],
	.conf_enum = ov5640_exposure_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_exposure_enum_map),
	.num_index = ARRAY_SIZE(ov5640_exposure_confs),
	.num_conf = ARRAY_SIZE(ov5640_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov5640_iso[][2] = {
	{ {0x3a18,0x00}, {0x3a19,0xfc},},   /*ISO_AUTO*/
	{ {0x3a18, 0x00},{0x3a19, 0xfc},},   /*ISO_DEBLUR*/
	{ {0x3a18, 0x00},{0x3a19, 0x6c},},    /*ISO_100*/
	{ {0x3a18, 0x00},{0x3a19, 0x8c},},    /*ISO_200*/
	{ {0x3a18, 0x00},{0x3a19, 0xcc},},   /*ISO_400*/
	{ {0x3a18, 0x00},{0x3a19, 0xfc},},    /*ISO_800*/
	{ {0x3a18, 0x00},{0x3a19, 0xfc},},    /*ISO_1600*/
};


static struct msm_camera_i2c_conf_array ov5640_iso_confs[][1] = {
	{{ov5640_iso[0], ARRAY_SIZE(ov5640_iso[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_iso[1], ARRAY_SIZE(ov5640_iso[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_iso[2], ARRAY_SIZE(ov5640_iso[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_iso[3], ARRAY_SIZE(ov5640_iso[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_iso[4], ARRAY_SIZE(ov5640_iso[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_iso[5], ARRAY_SIZE(ov5640_iso[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_iso[6], ARRAY_SIZE(ov5640_iso[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov5640_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};

static struct msm_camera_i2c_enum_conf_array ov5640_iso_enum_confs = {
	.conf = &ov5640_iso_confs[0][0],
	.conf_enum = ov5640_iso_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_iso_enum_map),
	.num_index = ARRAY_SIZE(ov5640_iso_confs),
	.num_conf = ARRAY_SIZE(ov5640_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};



static struct msm_camera_i2c_reg_conf ov5640_special_effect[][5] = {
	{ //MSM_V4L2_EFFECT_OFF
	{0x5001, 0xa7},
	{0x5580, 0x00},
	{0x5003, 0x08},
	{0x5583, 0x40},
	{0x5584, 0x40},
	},
	{//MSM_V4L2_EFFECT_MONO
	{0x5001, 0x83},
	{0x5580, 0x20},
	{0x5003, 0x08},
	{0x5583, 0x40},
	{0x5584, 0x40},

	},
	{//MSM_V4L2_EFFECT_NEGATIVE
	{0x5001, 0x83},
	{0x5580, 0x40},
	{0x5003, 0x08},
	{-1,-1,-1},
	{-1,-1,-1},
	},
	{//MSM_V4L2_EFFECT_SOLARIZE
	{0x5001, 0x83},
	{0x5580, 0x00},
	{0x5003, 0x09},
	{-1,-1,-1},
	{-1,-1,-1},

	},
	{//MSM_V4L2_EFFECT_SEPIA
	{0x5001, 0x83},
	{0x5580, 0x18},
	{0x5003, 0x08},
	{0x5583, 0x40},
	{0x5584, 0xa0},
	},
	{//MSM_V4L2_EFFECT_POSTERAIZE
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},
	{//MSM_V4L2_EFFECT_WHITEBOARD
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},
	{//MSM_V4L2_EFFECT_BLACKBOARD
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},
	{//MSM_V4L2_EFFECT_AQUA
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},

	},
	{//MSM_V4L2_EFFECT_EMBOSS
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},

	},
	{//MSM_V4L2_EFFECT_SKETCH
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},

	},
	{//MSM_V4L2_EFFECT_NEON
	{0x5001, 0xa7},
	{0x5580, 0x00},
	{0x5003, 0x08},
	{0x5583, 0x40},
	{0x5584, 0x40},

	},
	{//MSM_V4L2_EFFECT_MAX
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},

	},
};

static struct msm_camera_i2c_conf_array ov5640_special_effect_confs[][1] = {
	{{ov5640_special_effect[0],  ARRAY_SIZE(ov5640_special_effect[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[1],  ARRAY_SIZE(ov5640_special_effect[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[2],  ARRAY_SIZE(ov5640_special_effect[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[3],  ARRAY_SIZE(ov5640_special_effect[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[4],  ARRAY_SIZE(ov5640_special_effect[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[5],  ARRAY_SIZE(ov5640_special_effect[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[6],  ARRAY_SIZE(ov5640_special_effect[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[7],  ARRAY_SIZE(ov5640_special_effect[7]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[8],  ARRAY_SIZE(ov5640_special_effect[8]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[9],  ARRAY_SIZE(ov5640_special_effect[9]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[10], ARRAY_SIZE(ov5640_special_effect[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[11], ARRAY_SIZE(ov5640_special_effect[11]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_special_effect[12], ARRAY_SIZE(ov5640_special_effect[12]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov5640_special_effect_enum_map[] = {
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
		 ov5640_special_effect_enum_confs = {
	.conf = &ov5640_special_effect_confs[0][0],
	.conf_enum = ov5640_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_special_effect_enum_map),
	.num_index = ARRAY_SIZE(ov5640_special_effect_confs),
	.num_conf = ARRAY_SIZE(ov5640_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov5640_antibanding[][14] = {
	{{0x3c01, 0x80},{0x3c00, 0x04},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},  /*ANTIBANDING 60HZ*/
	{{0x3c01, 0x80},{0x3c00, 0x00},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},   /*ANTIBANDING 50HZ*/
	{
	{0x3622,0x01}, //PD-sel
	{0x3635,0x1c}, //VMREF 3635[2:0]
	{0x3634,0x40}, //I_5060 3643[2:0]
	{0x3c01,0x34},
	{0x3c00,0x00},
	{0x3c04,0x28},
	{0x3c05,0x98},
	{0x3c06,0x00},
	{0x3c07,0x08},
	{0x3c08,0x00},
	{0x3c09,0x1c},
	{0x300c,0x22}, //50/60div 300c[2:0]
	{0x3c0a,0x9c},
	{0x3c0b,0x40},
	}
};


static struct msm_camera_i2c_conf_array ov5640_antibanding_confs[][1] = {
	{{ov5640_antibanding[0], ARRAY_SIZE(ov5640_antibanding[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_antibanding[1], ARRAY_SIZE(ov5640_antibanding[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_antibanding[2], ARRAY_SIZE(ov5640_antibanding[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov5640_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};


static struct msm_camera_i2c_enum_conf_array ov5640_antibanding_enum_confs = {
	.conf = &ov5640_antibanding_confs[0][0],
	.conf_enum = ov5640_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_antibanding_enum_map),
	.num_index = ARRAY_SIZE(ov5640_antibanding_confs),
	.num_conf = ARRAY_SIZE(ov5640_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov5640_wb_oem[][7] = {
	{
	{0x3406,0x00,0},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},/*WHITEBALNACE OFF*/
	{
	{0x3406,0x00,0},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	}, /*WHITEBALNACE AUTO*/
	{
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},/*WHITEBALNACE CUSTOM*/
	{
	{0x3406, 0x01},
	{0x3400, 0x04},
	{0x3401, 0x38},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x08},
	{0x3405, 0x18},
	},/*INCANDISCENT*/
	{
	{0x3406, 0x01,},
	{0x3400, 0x04},
	{0x3401, 0x96},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x06},
	{0x3405, 0xd8},
	},	/*FLOURESECT */
	{
	{0x3406, 0x01},
	{0x3400, 0x06},
	{0x3401, 0x62},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x74},
	},	/*DAYLIGHT*/
	{
	{0x3406, 0x01},
	{0x3400, 0x06},
	{0x3401, 0x9f},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x2d},
	},	/*CLOUDY*/
};

static struct msm_camera_i2c_conf_array ov5640_wb_oem_confs[][1] = {
	{{ov5640_wb_oem[0], ARRAY_SIZE(ov5640_wb_oem[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_wb_oem[1], ARRAY_SIZE(ov5640_wb_oem[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_wb_oem[2], ARRAY_SIZE(ov5640_wb_oem[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_wb_oem[3], ARRAY_SIZE(ov5640_wb_oem[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_wb_oem[4], ARRAY_SIZE(ov5640_wb_oem[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_wb_oem[5], ARRAY_SIZE(ov5640_wb_oem[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov5640_wb_oem[6], ARRAY_SIZE(ov5640_wb_oem[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov5640_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array ov5640_wb_oem_enum_confs = {
	.conf = &ov5640_wb_oem_confs[0][0],
	.conf_enum = ov5640_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(ov5640_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(ov5640_wb_oem_confs),
	.num_conf = ARRAY_SIZE(ov5640_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};


int ov5640_saturation_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	if (value <= MSM_V4L2_SATURATION_L8)

	CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
	return rc;
}


int ov5640_contrast_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}

	CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
	return rc;
}

int ov5640_sharpness_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;

	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);


	return rc;
}

int ov5640_auto_focus_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int i;
	unsigned int af_st = 0;
	unsigned int af_ack = 0;
	unsigned int tmp = 0;
	int rc = 0;

	af_st = OV5640_read_i2c(OV5640_CMD_FW_STATUS);
	CDBG("--CAMERA-- %s af_st = %d\n", __func__, af_st);

	OV5640_write_i2c(OV5640_CMD_ACK, 0x01);
	OV5640_write_i2c(OV5640_CMD_MAIN, 0x03);

	for (i = 0; i < 50; i++) {
		af_ack = OV5640_read_i2c(OV5640_CMD_ACK);
		if (af_ack == 0)
			break;
		msleep(50);
	}
	CDBG("--CAMERA-- %s af_ack = 0x%x\n", __func__, af_ack);

	//    if(af_ack == 0)
	{
	//        mdelay(1000);
		af_st = OV5640_read_i2c( OV5640_CMD_FW_STATUS);
		CDBG("--CAMERA-- %s af_st = %d\n", __func__, af_st);

		if (af_st == 0x10)
		{
			CDBG("--CAMERA-- %s AF ok and release AF setting~!!\n", __func__);
		}
		else {
			CDBG_HIGH("--CAMERA-- %s AF not ready!!\n", __func__);
		}
	}

	//  OV5640_write_i2c(OV5640_CMD_ACK,0x01);
	//  OV5640_write_i2c(OV5640_CMD_MAIN,0x08);
	OV5640_write_i2c(OV5640_CMD_ACK,0x01);
	OV5640_write_i2c(OV5640_CMD_MAIN,0x07);

	for (i = 0; i < 70; i++)
	{
		af_ack = OV5640_read_i2c( OV5640_CMD_ACK);
		if (af_ack == 0)
			break;
		msleep(25);
	}

	tmp = OV5640_read_i2c( OV5640_CMD_PARA0);
	CDBG("--CAMERA--0x3024 = %x \n", tmp);
	rc = ((tmp == 0) ? 1 : 0);

	tmp = OV5640_read_i2c( OV5640_CMD_PARA1);
	CDBG("--CAMERA--0x3025 = %x \n", tmp);
	rc = ((tmp == 0) ? 1 : 0);

	tmp = OV5640_read_i2c( OV5640_CMD_PARA2);
	CDBG("--CAMERA--0x3026 = %x \n", tmp);
	rc = ((tmp == 0) ? 1 : 0);

	tmp = OV5640_read_i2c( OV5640_CMD_PARA3);
	CDBG("--CAMERA--0x3027 = %x \n", tmp);
	rc = ((tmp == 0) ? 1 : 0) ;

	tmp = OV5640_read_i2c( OV5640_CMD_PARA4);
	CDBG("--CAMERA--0x3028 = %x \n", tmp);
	rc = ((tmp == 0) ? 1 : 0) ;

	CDBG("--CAMERA-- %s rc = %d(End...)\n", __func__, rc);
	return rc;
}

int ov5640_flash_mode_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	led_flash_mode = value;
	CDBG("--CAMERA-- %s flash mode = %d\n", __func__, led_flash_mode);
	return rc;
}
int ov5640_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	effect_value = value;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->no_effect_settings, 0);
		if (rc < 0) {
			CDBG("write faield\n");
			return rc;
		}

	} else {
        printk("%s num_conf = %d value = %d\n",__func__,ctrl_info->enum_cfg_settings->num_conf,value);
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
	return rc;
}

int ov5640_wb_oem_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
		int rc = 0;

		CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
		CDBG("--CAMERA--  ...  num_conf= %d(End)\n",ctrl_info->enum_cfg_settings->num_conf);
        if(value==1)
        	effect_value=CAMERA_EFFECT_OFF;
	    else if(value == 0){
			effect_value = CAMERA_EFFECT_OFF;
			value = 1;
	    	}
		else{
			effect_value = value;}
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
		return rc;
}

int ov5640_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;

	CDBG("--CAMERA-- %s ...  ctrl_info->enum_cfg_settings->conf_enum = %d value = %d \
		     ctrl_id  = %x(End)\n", __func__,ctrl_info->enum_cfg_settings->num_conf,value,ctrl_info->ctrl_id);
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		CDBG("write faield\n");
		return rc;
	}
	return rc;
}
int ov5640_msm_iso_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
    if(effect_value==CAMERA_EFFECT_OFF){
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
    	}
	if (rc < 0) {
		CDBG("write faield\n");
		return rc;
	}
	return rc;
}

struct msm_sensor_v4l2_ctrl_info_t ov5640_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L8,
		.step = 1,
		.enum_cfg_settings = &ov5640_saturation_enum_confs,
		.s_v4l2_ctrl = ov5640_msm_sensor_s_ctrl_by_enum,//ov5640_saturation_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L8,
		.step = 1,
		.enum_cfg_settings = &ov5640_contrast_enum_confs,
		.s_v4l2_ctrl =ov5640_msm_sensor_s_ctrl_by_enum, //ov5640_contrast_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L6,
		.step = 1,
		.enum_cfg_settings = &ov5640_sharpness_enum_confs,
		.s_v4l2_ctrl =ov5640_msm_sensor_s_ctrl_by_enum, //ov5640_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &ov5640_exposure_enum_confs,
		.s_v4l2_ctrl = ov5640_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &ov5640_iso_enum_confs,
		.s_v4l2_ctrl = ov5640_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_MAX,
		.step = 1,
		.enum_cfg_settings = &ov5640_special_effect_enum_confs,
		.s_v4l2_ctrl = ov5640_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &ov5640_antibanding_enum_confs,
		.s_v4l2_ctrl = ov5640_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &ov5640_wb_oem_enum_confs,
		.s_v4l2_ctrl = ov5640_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_AUTO_FOCUS,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = ov5640_auto_focus_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_LED_FLASH_MODE,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = ov5640_flash_mode_msm_sensor_s_ctrl_by_enum,
	},
};

static struct msm_sensor_fn_t ov5640_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = ov5640_sensor_set_fps,

	.sensor_write_exp_gain = ov5640_write_exp_gain,
	.sensor_write_snapshot_exp_gain = ov5640_write_exp_gain,

	.sensor_csi_setting = ov5640_sensor_setting,//ov5640_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = ov5640_sensor_power_up,
	.sensor_power_down = ov5640_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t ov5640_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = ov5640_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov5640_start_settings),
	.stop_stream_conf = ov5640_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov5640_stop_settings),
	.group_hold_on_conf = NULL,
	.group_hold_on_conf_size = 0,
	.group_hold_off_conf = NULL,
	.group_hold_off_conf_size = 0,
	.init_settings = &ov5640_init_conf[0],
	.init_size = ARRAY_SIZE(ov5640_init_conf),
	.mode_settings = &ov5640_confs[0],
	.output_settings = &ov5640_dimensions[0],
	.num_conf = ARRAY_SIZE(ov5640_confs),
};

static struct msm_sensor_ctrl_t ov5640_s_ctrl = {
	.msm_sensor_v4l2_ctrl_info = ov5640_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(ov5640_v4l2_ctrl_info),
	.msm_sensor_reg = &ov5640_regs,
	.sensor_i2c_client = &ov5640_sensor_i2c_client,
	.sensor_i2c_addr =  0x78 - 2,//original 0x78, sub offset to avoid i2c conflicts
	.sensor_output_reg_addr = &ov5640_reg_addr,
	.sensor_id_info = &ov5640_id_info,
	.sensor_exp_gain_info = &ov5640_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &ov5640_csi_params_array[0],
	.msm_sensor_mutex = &ov5640_mut,
	.sensor_i2c_driver = &ov5640_i2c_driver,
	.sensor_v4l2_subdev_info = ov5640_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov5640_subdev_info),
	.sensor_v4l2_subdev_ops = &ov5640_subdev_ops,
	.func_tbl = &ov5640_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision WXGA YUV sensor driver");
MODULE_LICENSE("GPL v2");
