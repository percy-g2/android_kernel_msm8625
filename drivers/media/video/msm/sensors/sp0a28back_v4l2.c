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
#define SENSOR_NAME "sp0a28back"

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define SP0A28BACK_VERBOSE_DGB

#ifdef SP0A28BACK_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif
//#define preview_8_14_FPS
DEFINE_MUTEX(sp0a28back_mut);
static struct msm_sensor_ctrl_t sp0a28back_s_ctrl;

#define SP0A28BACK_P0_0xdd	0x78 //0x80
#define SP0A28BACK_P0_0xde	0x98 //0x80  //0x90  PZT 2012-12-14

//sharpness                          
#define SP0A28BACK_P1_0xe8	0x20//10//;sharp_fac_pos_outdoor  //PZT 2012-12-14
#define SP0A28BACK_P1_0xec	0x30//20//;sharp_fac_neg_outdoor
#define SP0A28BACK_P1_0xe9	0x10//0a//;sharp_fac_pos_nr
#define SP0A28BACK_P1_0xed	0x30//20//;sharp_fac_neg_nr
#define SP0A28BACK_P1_0xea	0x10//08//;sharp_fac_pos_dummy
#define SP0A28BACK_P1_0xef	0x20//18//;sharp_fac_neg_dummy
#define SP0A28BACK_P1_0xeb	0x10//08//;sharp_fac_pos_low
#define SP0A28BACK_P1_0xf0	0x10//18//;sharp_fac_neg_low 
//saturation
#define SP0A28BACK_P0_0xd3	0x74 // PZT 2012-12-14
#define SP0A28BACK_P0_0xd4	0x74
#define SP0A28BACK_P0_0xd6	0x6c
#define SP0A28BACK_P0_0xd7	0x60
#define SP0A28BACK_P0_0xd8	0x74 //0x90
#define SP0A28BACK_P0_0xd9	0x74 //0x90
#define SP0A28BACK_P0_0xda	0x64 //0x90
#define SP0A28BACK_P0_0xdb	0x58
                               
//Ae target
#define SP0A28BACK_P0_0xf7	0x78//0x80
#define SP0A28BACK_P0_0xf8	0x70//0x78
#define SP0A28BACK_P0_0xf9	0x78//0x80 
#define SP0A28BACK_P0_0xfa	0x70//0x78 

static struct msm_camera_i2c_reg_conf sp0a28back_start_settings[] = {
};

static struct msm_camera_i2c_reg_conf sp0a28back_stop_settings[] = {
};

static struct msm_camera_i2c_reg_conf sp0a28back_full_settings[] = {
	{0xfd,0x01},
	{0x7c,0x6c},
	{0xfd,0x00},
	{0x1C,0x00},
	{0x32,0x00},
	{0x0e,0x00},
	{0x0f,0x40},
	{0x10,0x40},
	{0x11,0x10},
	{0x12,0xa0},
	{0x13,0x40},
	{0x15,0x00},
	{0x1A,0x37},
	{0x1B,0x17},
	{0x1C,0x2f},  //close yuv
	{0x1d,0x00},
	{0x1E,0x57},
	{0x21,0x2f},
	{0x22,0x12},
	{0x24,0x80},
	{0x25,0x02},
	{0x26,0x03},
	{0x27,0xeb},
	{0x28,0x5f},
	{0x2f,0x01},
	{0x5f,0x02},
	{0xfb,0x33}, 
	{0xe7,0x03},
	{0xe7,0x00},
#ifdef preview_8_14_FPS
//24 M 50hz 14-8fpx
//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xd2},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x56},
{0xf0,0x46},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x0c},
{0x92,0x01},
{0x98,0x46},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x48},
{0xcf,0x03},
{0xd0,0x48},
{0xd1,0x03},
{0xfd,0x00},

#else
//24M 50hz 6-14fps
//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xd2},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x56},
{0xf0,0x46},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x10},
{0x92,0x01},
{0x98,0x46},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status 
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x60},
{0xcf,0x04},
{0xd0,0x60},
{0xd1,0x04},
{0xfd,0x00},

#endif	
	//lsc  for SX5044module
	{0xfd,0x01},
	{0x35,0x0e},//0x0a
	{0x36,0x14},//0x20
	{0x37,0x20},
	{0x38,0x1c},//0x22
	{0x39,0x09},//0x06
	{0x3a,0x10},//0x1a
	{0x3b,0x19},
	{0x3c,0x1a},//0x18
	{0x3d,0x08},//0x09
	{0x3e,0x10},//0x1c
	{0x3f,0x14},//0x18
	{0x40,0x22},
	{0x41,0x0a},//0x00
	{0x42,0x00},//0x18
	{0x43,0x0a},//0x02
	{0x44,0xfc},
	{0x45,0x00},
	{0x46,0xfe},//0x14
	{0x47,0x00},
	{0x48,0xf8},
	{0x49,0x00},//0xfc
	{0x4a,0xf8},//0x12
	{0x4b,0x00},
	{0x4c,0xf2},
	{0xfd,0x00},
	{0xa1,0x20},
	{0xa2,0x20},
	{0xa3,0x20},
	{0xa4,0xff},  
	//smooth  
	{0xfd,0x01},
	{0xde,0x0f},
	{0xfd,0x00},
	//单通道间平滑阈值	
{0x57,0x04},	//	;raw_dif_thr_outdoor
{0x58,0x0a},//0x0a	// ;raw_dif_thr_normal
{0x56,0x10},//0x10	// ;raw_dif_thr_dummy
{0x59,0x10},	// ;raw_dif_thr_lowlight
//GrGb平滑阈值
{0x89,0x04},	//;raw_grgb_thr_outdoor 
{0x8a,0x0a}, //;raw_grgb_thr_normal  
{0x9c,0x10}, //;raw_grgb_thr_dummy   
{0x9d,0x10}, //;raw_grgb_thr_lowlight
//Gr\Gb之间平滑强度
{0x81,0xe0}, //   ;raw_gflt_fac_outdoor
{0x82,0xa0}, //;80;raw_gflt_fac_normal
{0x83,0x80}, //   ;raw_gflt_fac_dummy
{0x84,0x80}, //   ;raw_gflt_fac_lowlight
//Gr、Gb单通道内平滑强度  
{0x85,0xe0}, //;raw_gf_fac_outdoor  
{0x86,0xa0}, //;raw_gf_fac_normal  
{0x87,0x80}, //;raw_gf_fac_dummy   
{0x88,0x80}, //;raw_gf_fac_lowlight
//R、B平滑强度  
{0x5a,0xff},		//;raw_rb_fac_outdoor
{0x5b,0xc0}, 		//;raw_rb_fac_normal
{0x5c,0xa0}, 	  //;raw_rb_fac_dummy
{0x5d,0xa0}, 	  //;raw_rb_fac_lowlight
	//adt 平滑阈值自适应
	{0xa7,0xff},
	{0xa8,0xff},	//;0x2f
	{0xa9,0xff},	//;0x2f
	{0xaa,0xff},	//;0x2f
	//dem_morie_thr 去摩尔纹
	{0x9e,0x10},
	//sharpen 
	{0xfd,0x01},	//
	{0xe2,0x30},	//	;sharpen_y_base
	{0xe4,0xa0},	//	;sharpen_y_max
	{0xe5,0x08},	// ;rangek_neg_outdoor
	{0xd3,0x10},	// ;rangek_pos_outdoor   
	{0xd7,0x08},	// ;range_base_outdoor   
	{0xe6,0x08},//0x08	// ;rangek_neg_normal
	{0xd4,0x10},//0x10	// ;rangek_pos_normal 
	{0xd8,0x08},	// ;range_base_normal  
	{0xe7,0x10},	// ;rangek_neg_dummy
	{0xd5,0x10},	// ;rangek_pos_dummy
	{0xd9,0x10},	// ;range_base_dummy 
	{0xd2,0x10},	// ;rangek_neg_lowlight
	{0xd6,0x10},	// ;rangek_pos_lowlight
	{0xda,0x10},	// ;range_base_lowlight
	{0xe8,SP0A28BACK_P1_0xe8},//0x20	//;sharp_fac_pos_outdoor
	{0xec,SP0A28BACK_P1_0xec},//0x30	//;sharp_fac_neg_outdoor
	{0xe9,SP0A28BACK_P1_0xe9},//0x10	//;sharp_fac_pos_nr
	{0xed,SP0A28BACK_P1_0xed},//0x30	//;sharp_fac_neg_nr
	{0xea,SP0A28BACK_P1_0xea},//0x10	//;sharp_fac_pos_dummy
	{0xef,SP0A28BACK_P1_0xef},//0x20	//;sharp_fac_neg_dummy
	{0xeb,SP0A28BACK_P1_0xeb},//0x10	//;sharp_fac_pos_low
	{0xf0,SP0A28BACK_P1_0xf0},//0x20	//;sharp_fac_neg_low 
	//CCM
	{0xfd,0x01},	//
	{0xa0,0x80},	//;8c;80;80;80(红色接近，肤色不理想)
	{0xa1,0x00},	//;0c;00;0 ;0 
	{0xa2,0x00},	//;e8;00;0 ;0 
	{0xa3,0xf6},	//;ec;ff;f2;f3;f0
	{0xa4,0x99},	//;99;9a;8e;a6
	{0xa5,0xf2},	//;fb;e7;0 ;ea
	{0xa6,0x0d},	//;0d;0c;0 ;0 
	{0xa7,0xda},	//;da;da;e6;e6
	{0xa8,0x98},	//;98;9a;9a;9a
	{0xa9,0x00},	//;30;00;0 ;0 
	{0xaa,0x33},	//;33;33;3 ;33
	{0xab,0x0c},	//;0c;0c;c ;c 
	{0xfd,0x00},	//;00	 
	//gamma  
	{0xfd,0x00},	//00
	{0x8b,0x00},	//0       
	{0x8c,0x0f},	//12
	{0x8d,0x21},	//1f
	{0x8e,0x3b},	//31
	{0x8f,0x64},	//4c
	{0x90,0x84},	//62
	{0x91,0xa0},	//77
	{0x92,0xb6},	//89
	{0x93,0xc7},	//9b
	{0x94,0xd2},	//a8
	{0x95,0xda},	//b5
	{0x96,0xe0},	//c0
	{0x97,0xe6},	//ca
	{0x98,0xea},	//d4
	{0x99,0xef},	//dd
	{0x9a,0xf4},	//e6
	{0x9b,0xf7},	//ef
	{0xfd,0x01},	//01
	{0x8d,0xfb},	//f7
	{0x8e,0xff},	//ff
	{0xfd,0x00},	//00

	/*//gamma 灰阶分布好  
	{0xfd,0x00},	
	{0x8b,0x00},	      
	{0x8c,0x11},	
	{0x8d,0x24},	
	{0x8e,0x3f},	
	{0x8f,0x64},	
	{0x90,0x7f},	
	{0x91,0x93},	
	{0x92,0xa4},	
	{0x93,0xb2},	
	{0x94,0xbb},	
	{0x95,0xc4},	
	{0x96,0xcb},	
	{0x97,0xd1},	
	{0x98,0xd5},	
	{0x99,0xdc},	
	{0x9a,0xe2},	
	{0x9b,0xe9},	
	{0xfd,0x01},	
	{0x8d,0xf2},	
	{0x8e,0xff},	
	{0xfd,0x00},	
	*/
	//awb for 后摄
	{0xfd,0x01},
	{0x10,0x08},
	{0x11,0x13},	
	{0x12,0x13},
	{0x2e,0x0d},	
	{0x2f,0x0d},
	{0x16,0x1c},
	{0x17,0x1a},
	{0x18,0x1a},	
	{0x19,0x54},	
	{0x1a,0xa9},  
	{0x1b,0x97},
	{0x2a,0xef},

	/*// awb for 前摄      
	{0xfd,0x01},    
	{0x11,0x08},    
	{0x12,0x08},    
	{0x2e,0x04},    
	{0x2f,0x04},    
	{0x16,0x1c},    
	{0x17,0x1a},    
	{0x18,0x16},    
	{0x19,0x54},    
	{0x1a,0x90},    
	{0x1b,0x9b},    
	{0x2a,0xef},    
	{0x2b,0x30},    
	{0x21,0x96},    
	{0x22,0x9a},    
	*/
	//AE;rpc 
	{0xfd,0x00},
	{0xe0,0x3a},//
	{0xe1,0x2c},//24,
	{0xe2,0x26},//
	{0xe3,0x22},//
	{0xe4,0x22},//
	{0xe5,0x20},//
	{0xe6,0x20},//
	{0xe8,0x20},//19,
	{0xe9,0x20},//19,
	{0xea,0x20},//19,
	{0xeb,0x1e},//18,
	{0xf5,0x1e},//18,
	{0xf6,0x1e},//18,
	//ae min gain  
	{0xfd,0x01},
	{0x94,0x60},
	{0x95,0x1e},//0x18
	{0x9c,0x60},
	{0x9d,0x1e},//0x18   
	//ae target
	{0xfd,0x00},
	{0xed,SP0A28BACK_P0_0xf7 + 0x04},//0x84 
	{0xf7,SP0A28BACK_P0_0xf7},		//0x80 
	{0xf8,SP0A28BACK_P0_0xf8},		//0x78 
	{0xec,SP0A28BACK_P0_0xf8 - 0x04},//0x74  
	{0xef,SP0A28BACK_P0_0xf9 + 0x04},//0x84
	{0xf9,SP0A28BACK_P0_0xf9},		//0x80
	{0xfa,SP0A28BACK_P0_0xfa},		//0x78
	{0xee,SP0A28BACK_P0_0xfa - 0x04},//0x74
	{0xfd,0x00},
	{0x31,0x70},
	//gray detect
	{0xfd,0x01},
	{0x30,0x40},
	{0x31,0x10},
	{0x32,0x20},
	{0x33,0xef},
	{0x34,0x02},
	{0x4d,0x40},
	{0x4e,0x15},
	{0x4f,0x13},
	//saturation
	{0xfd,0x00},
	{0xbe,0xaa},  
	{0xc0,0xff},
	{0xc1,0xff},
	{0xd3,SP0A28BACK_P0_0xd3},
	{0xd4,SP0A28BACK_P0_0xd4}, 
	{0xd6,SP0A28BACK_P0_0xd6},
	{0xd7,SP0A28BACK_P0_0xd7},
	{0xd8,SP0A28BACK_P0_0xd8},
	{0xd9,SP0A28BACK_P0_0xd9}, 
	{0xda,SP0A28BACK_P0_0xda},
	{0xdb,SP0A28BACK_P0_0xdb},
	//heq   
	{0xfd,0x00},
	{0xdc,0x00},	//;heq_offset
	{0xdd,0x78},	//;ku 0x80
	{0xde,0x80},	//;90;kl
	{0xdf,0x80},	//;heq_mean
	//YCnr  
	{0xfd,0x00},
	{0xc2,0x08},	//Ynr_thr_outdoor
	{0xc3,0x08},	//Ynr_thr_normal
	{0xc4,0x08},	//Ynr_thr_dummy
	{0xc5,0x10},	//Ynr_thr_lowlight
	{0xc6,0x80},	//cnr_thr_outdoor
	{0xc7,0x80},	//cnr_thr_normal  
	{0xc8,0x80},	//cnr_thr_dummy   
	{0xc9,0x80},	//cnr_thr_lowlight  
	//auto lum
	{0xfd,0x00},
	{0xb2,0x18},
	{0xb3,0x1f},
	{0xb4,0x30},
	{0xb5,0x45},
	//func enable
	{0xfd,0x00},
	{0x32,0x0d},
	{0x34,0x1e},
	{0x33,0xef},
	{0x35,0x40},
};

static struct v4l2_subdev_info sp0a28back_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t sp0a28back_dimensions[] = {
	{
		.x_output = 0x280,
		.y_output = 0x1E0,
		.line_length_pclk = 0x290,
		.frame_length_lines = 0x1EC,
		.vt_pixel_clk = 24000000,
		.op_pixel_clk = 24000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_i2c_conf_array sp0a28back_init_conf[] = {
};

static struct msm_camera_i2c_conf_array sp0a28back_confs[] = {
	{&sp0a28back_full_settings[0],
	ARRAY_SIZE(sp0a28back_full_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_reg_conf sp0a28back_saturation[][9] = {
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 - 0x6c}, {0xd4, SP0A28BACK_P0_0xd4 - 0x6c}, {0xd6, SP0A28BACK_P0_0xd6 - 0x64}, 
	{0xd7, SP0A28BACK_P0_0xd7 - 0x58}, {0xd8, SP0A28BACK_P0_0xd8 - 0x6c}, {0xd9, SP0A28BACK_P0_0xd9 - 0x6c}, 
	{0xda, SP0A28BACK_P0_0xda - 0x64}, {0xdb, SP0A28BACK_P0_0xdb - 0x58},}, /* SATURATION LEVEL0*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 - 0x40}, {0xd4, SP0A28BACK_P0_0xd4 - 0x40}, {0xd6, SP0A28BACK_P0_0xd6 - 0x40}, 
	{0xd7, SP0A28BACK_P0_0xd7 - 0x40}, {0xd8, SP0A28BACK_P0_0xd8 - 0x40}, {0xd9, SP0A28BACK_P0_0xd9 - 0x40}, 
	{0xda, SP0A28BACK_P0_0xda - 0x40}, {0xdb, SP0A28BACK_P0_0xdb - 0x40},}, /* SATURATION LEVEL1*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 - 0x30}, {0xd4, SP0A28BACK_P0_0xd4 - 0x30}, {0xd6, SP0A28BACK_P0_0xd6 - 0x30},
	{0xd7, SP0A28BACK_P0_0xd7 - 0x30}, {0xd8, SP0A28BACK_P0_0xd8 - 0x30}, {0xd9, SP0A28BACK_P0_0xd9 - 0x30}, 
	{0xda, SP0A28BACK_P0_0xda - 0x30}, {0xdb, SP0A28BACK_P0_0xdb - 0x30},}, /* SATURATION LEVEL2*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 - 0x20}, {0xd4, SP0A28BACK_P0_0xd4 - 0x20}, {0xd6, SP0A28BACK_P0_0xd6 - 0x20}, 
	{0xd7, SP0A28BACK_P0_0xd7 - 0x20}, {0xd8, SP0A28BACK_P0_0xd8 - 0x20}, {0xd9, SP0A28BACK_P0_0xd9 - 0x20}, 
	{0xda, SP0A28BACK_P0_0xda - 0x20}, {0xdb, SP0A28BACK_P0_0xdb - 0x20},}, /* SATURATION LEVEL3*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 - 0x10}, {0xd4, SP0A28BACK_P0_0xd4 - 0x10}, {0xd6, SP0A28BACK_P0_0xd6 - 0x10}, 
	{0xd7, SP0A28BACK_P0_0xd7 - 0x10}, {0xd8, SP0A28BACK_P0_0xd8 - 0x10}, {0xd9, SP0A28BACK_P0_0xd9 - 0x10}, 
	{0xda, SP0A28BACK_P0_0xda - 0x10}, {0xdb, SP0A28BACK_P0_0xdb - 0x10},}, /* SATURATION LEVEL4*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3}, {0xd4, SP0A28BACK_P0_0xd4}, {0xd6, SP0A28BACK_P0_0xd6}, 
	{0xd7, SP0A28BACK_P0_0xd7}, {0xd8, SP0A28BACK_P0_0xd8}, {0xd9, SP0A28BACK_P0_0xd9},
	{0xda, SP0A28BACK_P0_0xda}, {0xdb, SP0A28BACK_P0_0xdb},}, /* SATURATION LEVEL5*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 + 0x18}, {0xd4, SP0A28BACK_P0_0xd4 + 0x18}, {0xd6, SP0A28BACK_P0_0xd6 + 0x18}, 
	{0xd7, SP0A28BACK_P0_0xd7 + 0x18}, {0xd8, SP0A28BACK_P0_0xd8 + 0x18}, {0xd9, SP0A28BACK_P0_0xd9 + 0x18}, 
	{0xda, SP0A28BACK_P0_0xda + 0x18}, {0xdb, SP0A28BACK_P0_0xdb + 0x18},}, /* SATURATION LEVEL6*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 + 0x30}, {0xd4, SP0A28BACK_P0_0xd4 + 0x30}, {0xd6, SP0A28BACK_P0_0xd6 + 0x30},
	{0xd7, SP0A28BACK_P0_0xd7 + 0x30}, {0xd8, SP0A28BACK_P0_0xd8 + 0x30}, {0xd9, SP0A28BACK_P0_0xd9 + 0x30}, 
	{0xda, SP0A28BACK_P0_0xda + 0x30}, {0xdb, SP0A28BACK_P0_0xdb + 0x30},}, /* SATURATION LEVEL7*/
	
	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 + 0x48}, {0xd4, SP0A28BACK_P0_0xd4 + 0x48}, {0xd6, SP0A28BACK_P0_0xd6 + 0x48}, 
	{0xd7, SP0A28BACK_P0_0xd7 + 0x48}, {0xd8, SP0A28BACK_P0_0xd8 + 0x48}, {0xd9, SP0A28BACK_P0_0xd9 + 0x48}, 
	{0xda, SP0A28BACK_P0_0xda + 0x48}, {0xdb, SP0A28BACK_P0_0xdb + 0x48},}, /* SATURATION LEVEL8*/

	{{0xfd, 0x00}, {0xd3, SP0A28BACK_P0_0xd3 + 0x60}, {0xd4, SP0A28BACK_P0_0xd4 + 0x60}, {0xd6, SP0A28BACK_P0_0xd6 + 0x60}, 
	{0xd7, SP0A28BACK_P0_0xd7 + 0x60}, {0xd8, SP0A28BACK_P0_0xd8 + 0x60}, {0xd9, SP0A28BACK_P0_0xd9 + 0x60}, 
	{0xda, SP0A28BACK_P0_0xda + 0x60}, {0xdb, SP0A28BACK_P0_0xdb + 0x60},}, /* SATURATION LEVEL9*/
};

static struct msm_camera_i2c_conf_array sp0a28back_saturation_confs[][1] = {
	{{sp0a28back_saturation[0], ARRAY_SIZE(sp0a28back_saturation[0]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[1], ARRAY_SIZE(sp0a28back_saturation[1]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[2], ARRAY_SIZE(sp0a28back_saturation[2]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[3], ARRAY_SIZE(sp0a28back_saturation[3]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[4], ARRAY_SIZE(sp0a28back_saturation[4]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[5], ARRAY_SIZE(sp0a28back_saturation[5]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[6], ARRAY_SIZE(sp0a28back_saturation[6]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[7], ARRAY_SIZE(sp0a28back_saturation[7]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[8], ARRAY_SIZE(sp0a28back_saturation[8]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_saturation[9], ARRAY_SIZE(sp0a28back_saturation[9]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_saturation_enum_map[] = {
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
};

static struct msm_camera_i2c_enum_conf_array sp0a28back_saturation_enum_confs = {
	.conf = &sp0a28back_saturation_confs[0][0],
	.conf_enum = sp0a28back_saturation_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_saturation_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_saturation_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf sp0a28back_contrast[][3] = {
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd - 0x28},
	{0xde, SP0A28BACK_P0_0xde - 0x28},
	}, /* CONTRAST L0*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd - 0x20},
	{0xde, SP0A28BACK_P0_0xde - 0x20},
	}, /* CONTRAST L1*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd - 0x18},
	{0xde, SP0A28BACK_P0_0xde - 0x18},
	}, /* CONTRAST L2*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd - 0x10},
	{0xde, SP0A28BACK_P0_0xde - 0x10},
	}, /* CONTRAST L3*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd - 0x08},
	{0xde, SP0A28BACK_P0_0xde - 0x08},
	}, /* CONTRAST L4*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd},
	{0xde, SP0A28BACK_P0_0xde},
	}, /* CONTRAST L5*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd + 0x08},
	{0xde, SP0A28BACK_P0_0xde + 0x08},
	}, /* CONTRAST L6*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd + 0x10},
	{0xde, SP0A28BACK_P0_0xde + 0x10},
	}, /* CONTRAST L7*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd + 0x18},
	{0xde, SP0A28BACK_P0_0xde + 0x18},
	}, /* CONTRAST L8*/
	{
	{0xfd, 0x00},
	{0xdd, SP0A28BACK_P0_0xdd + 0x20},
	{0xde, SP0A28BACK_P0_0xde + 0x20},
	}, /* CONTRAST L9*/
};

static struct msm_camera_i2c_conf_array sp0a28back_contrast_confs[][1] = {
	{{sp0a28back_contrast[0], ARRAY_SIZE(sp0a28back_contrast[0]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[1], ARRAY_SIZE(sp0a28back_contrast[1]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[2], ARRAY_SIZE(sp0a28back_contrast[2]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[3], ARRAY_SIZE(sp0a28back_contrast[3]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[4], ARRAY_SIZE(sp0a28back_contrast[4]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[5], ARRAY_SIZE(sp0a28back_contrast[5]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[6], ARRAY_SIZE(sp0a28back_contrast[6]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[7], ARRAY_SIZE(sp0a28back_contrast[7]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[8], ARRAY_SIZE(sp0a28back_contrast[8]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_contrast[9], ARRAY_SIZE(sp0a28back_contrast[9]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_contrast_enum_map[] = {
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
};

static struct msm_camera_i2c_enum_conf_array sp0a28back_contrast_enum_confs = {
	.conf = &sp0a28back_contrast_confs[0][0],
	.conf_enum = sp0a28back_contrast_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_contrast_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_contrast_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf sp0a28back_sharpness[][9] = {
	{
	{0xfd, 0x01},
	{0xe8, SP0A28BACK_P1_0xe8 - 0x10},
	{0xec, SP0A28BACK_P1_0xec - 0x10},
	{0xe9, SP0A28BACK_P1_0xe9 - 0x10},
	{0xed, SP0A28BACK_P1_0xed - 0x10},
	{0xea, SP0A28BACK_P1_0xea - 0x10},
	{0xef, SP0A28BACK_P1_0xef - 0x10},
	{0xeb, SP0A28BACK_P1_0xeb - 0x10},
	{0xf0, SP0A28BACK_P1_0xf0 - 0x10},
	}, /* SHARPNESS LEVEL 0*/
	{
	{0xfd, 0x01},
	{0xe8, SP0A28BACK_P1_0xe8 - 0x08},
	{0xec, SP0A28BACK_P1_0xec - 0x08},
	{0xe9, SP0A28BACK_P1_0xe9 - 0x08},
	{0xed, SP0A28BACK_P1_0xed - 0x08},
	{0xea, SP0A28BACK_P1_0xea - 0x08},
	{0xef, SP0A28BACK_P1_0xef - 0x08},
	{0xeb, SP0A28BACK_P1_0xeb - 0x08},
	{0xf0, SP0A28BACK_P1_0xf0 - 0x08},
	}, /* SHARPNESS LEVEL 1*/
	{
	{0xfd, 0x01},
	{0xe8, SP0A28BACK_P1_0xe8},
	{0xec, SP0A28BACK_P1_0xec},
	{0xe9, SP0A28BACK_P1_0xe9},
	{0xed, SP0A28BACK_P1_0xed},
	{0xea, SP0A28BACK_P1_0xea},
	{0xef, SP0A28BACK_P1_0xef},
	{0xeb, SP0A28BACK_P1_0xeb},
	{0xf0, SP0A28BACK_P1_0xf0},
	}, /* SHARPNESS LEVEL 2*/
	{
	{0xfd, 0x01},
	{0xe8, SP0A28BACK_P1_0xe8 + 0x08},
	{0xec, SP0A28BACK_P1_0xec + 0x08},
	{0xe9, SP0A28BACK_P1_0xe9 + 0x08},
	{0xed, SP0A28BACK_P1_0xed + 0x08},
	{0xea, SP0A28BACK_P1_0xea + 0x08},
	{0xef, SP0A28BACK_P1_0xef + 0x08},
	{0xeb, SP0A28BACK_P1_0xeb + 0x08},
	{0xf0, SP0A28BACK_P1_0xf0 + 0x08},
	}, /* SHARPNESS LEVEL 3*/
	{
	{0xfd, 0x01},
	{0xe8, SP0A28BACK_P1_0xe8 + 0x18},
	{0xec, SP0A28BACK_P1_0xec + 0x18},
	{0xe9, SP0A28BACK_P1_0xe9 + 0x18},
	{0xed, SP0A28BACK_P1_0xed + 0x18},
	{0xea, SP0A28BACK_P1_0xea + 0x18},
	{0xef, SP0A28BACK_P1_0xef + 0x18},
	{0xeb, SP0A28BACK_P1_0xeb + 0x18},
	{0xf0, SP0A28BACK_P1_0xf0 + 0x18},
	}, /* SHARPNESS LEVEL 4*/
	{
	{0xfd, 0x01},
	{0xe8, SP0A28BACK_P1_0xe8 + 0x28},
	{0xec, SP0A28BACK_P1_0xec + 0x28},
	{0xe9, SP0A28BACK_P1_0xe9 + 0x28},
	{0xed, SP0A28BACK_P1_0xed + 0x28},
	{0xea, SP0A28BACK_P1_0xea + 0x28},
	{0xef, SP0A28BACK_P1_0xef + 0x28},
	{0xeb, SP0A28BACK_P1_0xeb + 0x28},
	{0xf0, SP0A28BACK_P1_0xf0 + 0x28},
	}, /* SHARPNESS LEVEL 5*/
	{
	{0xfd, 0x01},
	{0xe8, SP0A28BACK_P1_0xe8 + 0x48},
	{0xec, SP0A28BACK_P1_0xec + 0x48},
	{0xe9, SP0A28BACK_P1_0xe9 + 0x48},
	{0xed, SP0A28BACK_P1_0xed + 0x48},
	{0xea, SP0A28BACK_P1_0xea + 0x48},
	{0xef, SP0A28BACK_P1_0xef + 0x48},
	{0xeb, SP0A28BACK_P1_0xeb + 0x48},
	{0xf0, SP0A28BACK_P1_0xf0 + 0x48},
	}, /* SHARPNESS LEVEL 6*/
};

static struct msm_camera_i2c_conf_array sp0a28back_sharpness_confs[][1] = {
	{{sp0a28back_sharpness[0], ARRAY_SIZE(sp0a28back_sharpness[0]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_sharpness[1], ARRAY_SIZE(sp0a28back_sharpness[1]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_sharpness[2], ARRAY_SIZE(sp0a28back_sharpness[2]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_sharpness[3], ARRAY_SIZE(sp0a28back_sharpness[3]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_sharpness[4], ARRAY_SIZE(sp0a28back_sharpness[4]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_sharpness[5], ARRAY_SIZE(sp0a28back_sharpness[5]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_sharpness[6], ARRAY_SIZE(sp0a28back_sharpness[6]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
	MSM_V4L2_SHARPNESS_L6,
};

static struct msm_camera_i2c_enum_conf_array sp0a28back_sharpness_enum_confs = {
	.conf = &sp0a28back_sharpness_confs[0][0],
	.conf_enum = sp0a28back_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_sharpness_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_sharpness_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf sp0a28back_exposure[][2] = {
	{{0xfd,0x00},{0xdc,0xe0},}, /*EXPOSURECOMPENSATIONN2*/
	{{0xfd,0x00},{0xdc,0xf0},}, /*EXPOSURECOMPENSATIONN1*/
	{{0xfd,0x00},{0xdc,0x00},}, /*EXPOSURECOMPENSATIOND*/
	{{0xfd,0x00},{0xdc,0x10},}, /*EXPOSURECOMPENSATIONP1*/
	{{0xfd,0x00},{0xdc,0x20},}, /*EXPOSURECOMPENSATIONP2*/
};

static struct msm_camera_i2c_conf_array sp0a28back_exposure_confs[][1] = {
	{{sp0a28back_exposure[0], ARRAY_SIZE(sp0a28back_exposure[0]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_exposure[1], ARRAY_SIZE(sp0a28back_exposure[1]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_exposure[2], ARRAY_SIZE(sp0a28back_exposure[2]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_exposure[3], ARRAY_SIZE(sp0a28back_exposure[3]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_exposure[4], ARRAY_SIZE(sp0a28back_exposure[4]), 0,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array sp0a28back_exposure_enum_confs = {
	.conf = &sp0a28back_exposure_confs[0][0],
	.conf_enum = sp0a28back_exposure_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_exposure_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_exposure_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

// sp0a28back not suport!
static struct msm_camera_i2c_reg_conf sp0a28back_iso[][2] = {
	{{0xfd, 0x00},{0x24, 0x80},},/*ISO_AUTO*/
	{{0xfd, 0x00},{0x24, 0x80},},/*ISO_DEBLUR*/
	{{0xfd, 0x00},{0x24, 0x20},},/*ISO_100*/
	{{0xfd, 0x00},{0x24, 0x30},},/*ISO_200*/
	{{0xfd, 0x00},{0x24, 0x40},},/*ISO_400*/
	{{0xfd, 0x00},{0x24, 0x50},},/*ISO_800*/
	{{0xfd, 0x00},{0x24, 0x60},},/*ISO_1600*/
};

static struct msm_camera_i2c_conf_array sp0a28back_iso_confs[][1] = {
	{{sp0a28back_iso[0], ARRAY_SIZE(sp0a28back_iso[0]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_iso[1], ARRAY_SIZE(sp0a28back_iso[1]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_iso[2], ARRAY_SIZE(sp0a28back_iso[2]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_iso[3], ARRAY_SIZE(sp0a28back_iso[3]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_iso[4], ARRAY_SIZE(sp0a28back_iso[4]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_iso[5], ARRAY_SIZE(sp0a28back_iso[5]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};

static struct msm_camera_i2c_enum_conf_array sp0a28back_iso_enum_confs = {
	.conf = &sp0a28back_iso_confs[0][0],
	.conf_enum = sp0a28back_iso_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_iso_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_iso_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf sp0a28back_no_effect[] = {
	{0xfd, 0x00},{0x62, 0x00},{0x63, 0x80},{0x64, 0x80},
};

static struct msm_camera_i2c_conf_array sp0a28back_no_effect_confs[] = {
	{&sp0a28back_no_effect[0], ARRAY_SIZE(sp0a28back_no_effect), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_reg_conf sp0a28back_special_effect[][4] = {
	{{0xfd, 0x00},{0x62, 0x00},{0x63, 0x80},{0x64, 0x80},}, /*for special effect OFF*/
	{{0xfd, 0x00},{0x62, 0x20},{0x63, 0x80},{0x64, 0x80},}, /*for special effect MONO*/
	{{0xfd, 0x00},{0x62, 0x08},{0x63, 0x80},{0x64, 0x80},}, /*for special efefct Negative*/
	{{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1},}, /*Solarize is not supported by sensor*/
	{{0xfd, 0x00},{0x62, 0x10},{0x63, 0xd0},{0x64, 0x40},}, /*for sepia*/
	{{0xfd, 0x00},{0x62, 0x00},{0x63, 0x80},{0x64, 0x80},}, /*Posteraize*/
	{{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1},}, /* White board not supported*/
	{{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1},}, /*Blackboard not supported*/
	{{0xfd, 0x00},{0x62, 0x10},{0x63, 0x20},{0x64, 0xb0},}, /*Aqua */
	{{0xfd, 0x00},{0x62, 0x01},{0x63, 0x80},{0x64, 0x80},}, /*Emboss */
	{{0xfd, 0x00},{0x62, 0x02},{0x63, 0x80},{0x64, 0x80},}, /*sketch */
	{{0xfd, 0x00},{0x62, 0x10},{0x63, 0xc0},{0x64, 0x38},}, /*Neno*/
	{{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1},}, /*MAX not supported*/
};

static struct msm_camera_i2c_conf_array sp0a28back_special_effect_confs[][1] = {
	{{sp0a28back_special_effect[0],  ARRAY_SIZE(sp0a28back_special_effect[0]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[1],  ARRAY_SIZE(sp0a28back_special_effect[1]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[2],  ARRAY_SIZE(sp0a28back_special_effect[2]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[3],  ARRAY_SIZE(sp0a28back_special_effect[3]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[4],  ARRAY_SIZE(sp0a28back_special_effect[4]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[5],  ARRAY_SIZE(sp0a28back_special_effect[5]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[6],  ARRAY_SIZE(sp0a28back_special_effect[6]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[7],  ARRAY_SIZE(sp0a28back_special_effect[7]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[8],  ARRAY_SIZE(sp0a28back_special_effect[8]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[9],  ARRAY_SIZE(sp0a28back_special_effect[9]),  100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[10], ARRAY_SIZE(sp0a28back_special_effect[10]), 100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[11], ARRAY_SIZE(sp0a28back_special_effect[11]), 100,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_special_effect[12], ARRAY_SIZE(sp0a28back_special_effect[12]), 100,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_special_effect_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array sp0a28back_special_effect_enum_confs = {
	.conf = &sp0a28back_special_effect_confs[0][0],
	.conf_enum = sp0a28back_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_special_effect_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_special_effect_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf sp0a28back_antibanding[][28] = {
	{
#ifdef preview_8_14_FPS	
//24 M 50hz 14-8fpx
//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xd2},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x56},
{0xf0,0x46},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x0c},
{0x92,0x01},
{0x98,0x46},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x48},
{0xcf,0x03},
{0xd0,0x48},
{0xd1,0x03},
{0xfd,0x00},
#else
//24M 50hz 6-14fps
//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xd2},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x56},
{0xf0,0x46},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x10},
{0x92,0x01},
{0x98,0x46},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status 
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x60},
{0xcf,0x04},
{0xd0,0x60},
{0xd1,0x04},
{0xfd,0x00},
#endif	
	}, /*ANTIBANDING 60HZ*/
	{
#ifdef preview_8_14_FPS	
//24 M 60hz 14.16-8fps

//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xb1},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x43},
{0xf0,0x3b},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x0f},
{0x92,0x01},
{0x98,0x3b},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x75},
{0xcf,0x03},
{0xd0,0x75},
{0xd1,0x03},
{0xfd,0x00},
#else
//24M 50hz 6-14.16fps
//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xb1},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x43},
{0xf0,0x3b},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x14},
{0x92,0x01},
{0x98,0x3b},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x9c},
{0xcf,0x04},
{0xd0,0x9c},
{0xd1,0x04},
{0xfd,0x00},

#endif
	}, /*ANTIBANDING 50HZ*/
	{
#ifdef preview_8_14_FPS
//24 M 50hz 14-8fpx
//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xd2},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x56},
{0xf0,0x46},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x0c},
{0x92,0x01},
{0x98,0x46},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x48},
{0xcf,0x03},
{0xd0,0x48},
{0xd1,0x03},
{0xfd,0x00},
#else
//24M 50hz 6-14fps
//ae setting
{0xfd,0x00},
{0x03,0x00},
{0x04,0xd2},
{0x05,0x00},
{0x06,0x00},
{0x09,0x03},
{0x0a,0x56},
{0xf0,0x46},
{0xf1,0x00},
{0xfd,0x01},
{0x90,0x10},
{0x92,0x01},
{0x98,0x46},
{0x99,0x00},
{0x9a,0x01},
{0x9b,0x00},
//Status 
{0xfd,0x01},
{0xc4,0x6c},
{0xc5,0x7c},
{0xca,0x30},
{0xcb,0x45},
{0xcc,0x60},//0x70
{0xcd,0x60},//0x70
{0xce,0x60},
{0xcf,0x04},
{0xd0,0x60},
{0xd1,0x04},
{0xfd,0x00},

#endif
	}, /*ANTIBANDING AUTO*/
};

static struct msm_camera_i2c_conf_array sp0a28back_antibanding_confs[][1] = {
	{{sp0a28back_antibanding[0], ARRAY_SIZE(sp0a28back_antibanding[0]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_antibanding[1], ARRAY_SIZE(sp0a28back_antibanding[1]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_antibanding[2], ARRAY_SIZE(sp0a28back_antibanding[2]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};

static struct msm_camera_i2c_enum_conf_array sp0a28back_antibanding_enum_confs = {
	.conf = &sp0a28back_antibanding_confs[0][0],
	.conf_enum = sp0a28back_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_antibanding_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_antibanding_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf sp0a28back_wb_oem[][5] = {
	{{0xfd, 0x00},{0xfd, 0x00},{0xfd, 0x00},{0xfd, 0x00},{0xfd, 0x00},}, /*WHITEBALNACE OFF*/
	{{0xfd, 0x01},{0x28, 0xc4},{0x29, 0x9e},{0xfd, 0x00},{0xfd, 0x00},}, /*WHITEBALNACE AUTO*/
	{{0xfd, 0x01},{0x28, 0xc4},{0x29, 0x9e},{0xfd, 0x00},{0xfd, 0x00},}, /*WHITEBALNACE CUSTOM*/
	{{0xfd, 0x00},{0xfd, 0x01},{0x28, 0x7b},{0x29, 0xd3},{0xfd, 0x00},}, /*INCANDISCENT*/
	{{0xfd, 0x00},{0xfd, 0x01},{0x28, 0xb4},{0x29, 0xc4},{0xfd, 0x00},}, /*FLOURESECT*/
	{{0xfd, 0x00},{0xfd, 0x01},{0x28, 0xc1},{0x29, 0x88},{0xfd, 0x00},}, /*DAYLIGHT*/
	{{0xfd, 0x00},{0xfd, 0x01},{0x28, 0xe2},{0x29, 0x82},{0xfd, 0x00},}, /*CLOUDY*/
};

static struct msm_camera_i2c_conf_array sp0a28back_wb_oem_confs[][1] = {
	{{sp0a28back_wb_oem[0], ARRAY_SIZE(sp0a28back_wb_oem[0]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_wb_oem[1], ARRAY_SIZE(sp0a28back_wb_oem[1]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_wb_oem[2], ARRAY_SIZE(sp0a28back_wb_oem[2]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_wb_oem[3], ARRAY_SIZE(sp0a28back_wb_oem[3]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_wb_oem[4], ARRAY_SIZE(sp0a28back_wb_oem[4]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_wb_oem[5], ARRAY_SIZE(sp0a28back_wb_oem[5]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
	{{sp0a28back_wb_oem[6], ARRAY_SIZE(sp0a28back_wb_oem[6]),  0,
		MSM_CAMERA_I2C_BYTE_DATA},},
};

static int sp0a28back_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array sp0a28back_wb_oem_enum_confs = {
	.conf = &sp0a28back_wb_oem_confs[0][0],
	.conf_enum = sp0a28back_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(sp0a28back_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(sp0a28back_wb_oem_confs),
	.num_conf = ARRAY_SIZE(sp0a28back_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static int sp0a28back_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;

	rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);

	return rc;
}

static int sp0a28back_msm_sensor_s_iso_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	unsigned short temp = 0;

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0xfd, 0x00, MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
		0x32, &temp, MSM_CAMERA_I2C_BYTE_DATA);

	switch (value)
	{
	case MSM_V4L2_ISO_AUTO:
	case MSM_V4L2_ISO_DEBLUR:
		temp |= 0x05;
		break;

	case MSM_V4L2_ISO_100:
	case MSM_V4L2_ISO_200:
	case MSM_V4L2_ISO_400:
	case MSM_V4L2_ISO_800:
	case MSM_V4L2_ISO_1600:
		temp &= (~(0x05));
		break;
	} 

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x32, temp, MSM_CAMERA_I2C_BYTE_DATA);

	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);

	return rc;
}

static int sp0a28back_msm_sensor_s_awb_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	unsigned short temp = 0;

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0xfd, 0x00, MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
		0x32, &temp, MSM_CAMERA_I2C_BYTE_DATA);

	switch (value)
	{
	case MSM_V4L2_WB_AUTO:
	case MSM_V4L2_WB_CUSTOM:
		temp |= 0x08;
		break;

	case MSM_V4L2_WB_OFF:
	case MSM_V4L2_WB_INCANDESCENT:
	case MSM_V4L2_WB_FLUORESCENT:
	case MSM_V4L2_WB_DAYLIGHT:
	case MSM_V4L2_WB_CLOUDY_DAYLIGHT:
		temp &= (~(0x08));
		break;
	} 

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x32, temp, MSM_CAMERA_I2C_BYTE_DATA);

	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);

	return rc;
}

static struct msm_sensor_v4l2_ctrl_info_t sp0a28back_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L9,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_saturation_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L9,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_contrast_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L6,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_sharpness_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_exposure_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_iso_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_iso_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_NEGATIVE,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_special_effect_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_antibanding_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &sp0a28back_wb_oem_enum_confs,
		.s_v4l2_ctrl = sp0a28back_msm_sensor_s_awb_ctrl_by_enum,
	},
};

static struct msm_camera_csi_params sp0a28back_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x09,
};

static struct msm_camera_csi_params *sp0a28back_csi_params_array[] = {
	&sp0a28back_csi_params,
};

static struct msm_sensor_output_reg_addr_t sp0a28back_reg_addr = {
	.x_output = 0xCC,
	.y_output = 0xCE,
	.line_length_pclk = 0xC8,
	.frame_length_lines = 0xCA,
};

static struct msm_sensor_id_info_t sp0a28back_id_info = {
	.sensor_id_reg_addr = 0x02,
	.sensor_id = 0xA200,
};

static const struct i2c_device_id sp0a28back_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&sp0a28back_s_ctrl},
	{ }
};

static int32_t sp0a28back_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("%s IN\r\n", __func__);
	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	s_ctrl->sensor_i2c_addr = s_ctrl->sensor_i2c_addr << 1;

	rc = msm_sensor_i2c_probe(client, id);

	if (client->dev.platform_data == NULL) {
		CDBG_HIGH("%s: NULL sensor data\n", __func__);
		return -EFAULT;
	}

	usleep_range(5000, 5100);

	return rc;
}

static struct i2c_driver sp0a28back_i2c_driver = {
	.id_table = sp0a28back_i2c_id,
	.probe  = sp0a28back_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client sp0a28back_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	int rc = 0;
	CDBG("SP0A28BACK\n");

	rc = i2c_add_driver(&sp0a28back_i2c_driver);

	return rc;
}

static struct v4l2_subdev_core_ops sp0a28back_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops sp0a28back_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops sp0a28back_subdev_ops = {
	.core = &sp0a28back_subdev_core_ops,
	.video  = &sp0a28back_subdev_video_ops,
};

static int32_t sp0a28back_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = NULL;

	CDBG("%s: %d\n", __func__, __LINE__);

	info = s_ctrl->sensordata;

	CDBG("%s, sensor_pwd:%d, sensor_reset:%d\r\n",__func__, info->sensor_pwd, info->sensor_reset);

	gpio_direction_output(info->sensor_pwd, 1);
	if (info->sensor_reset_enable){
		gpio_direction_output(info->sensor_reset, 0);
	}
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

	/* turn on ldo and vreg */
	usleep_range(1000, 1100);
	gpio_direction_output(info->sensor_pwd, 0);
	msleep(20);
	if (info->sensor_reset_enable){
		gpio_direction_output(info->sensor_reset, 1);
	}
	msleep(25);

	return rc;
}

static int32_t sp0a28back_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = NULL;

	CDBG("%s IN\r\n", __func__);
	info = s_ctrl->sensordata;

	gpio_direction_output(info->sensor_pwd, 1);
	msleep(40);

	rc = msm_sensor_power_down(s_ctrl);
	msleep(40);
	if (s_ctrl->sensordata->pmic_gpio_enable){
		lcd_camera_power_onoff(0);
	}

	return rc;
}

static int32_t sp0a28back_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		csi_config = 0;
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
				s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(50);
			msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);
			msleep(300);
			csi_config = 1;
		}
	}
	else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		/* move it into MSM_SENSOR_REG_INIT
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
				s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(50);
			msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);
			msleep(300);
			csi_config = 1;
		}*/

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE,
			&s_ctrl->sensordata->pdata->ioclk.vfe_clk_rate);

		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;
}

static struct msm_sensor_fn_t sp0a28back_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_csi_setting = sp0a28back_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = sp0a28back_sensor_power_up,
	.sensor_power_down = sp0a28back_sensor_power_down,
};

static struct msm_sensor_reg_t sp0a28back_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = sp0a28back_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(sp0a28back_start_settings),
	.stop_stream_conf = sp0a28back_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(sp0a28back_stop_settings),
	.init_settings = &sp0a28back_init_conf[0],
	.init_size = ARRAY_SIZE(sp0a28back_init_conf),
	.mode_settings = &sp0a28back_confs[0],
	.no_effect_settings = &sp0a28back_no_effect_confs[0],
	.output_settings = &sp0a28back_dimensions[0],
	.num_conf = ARRAY_SIZE(sp0a28back_confs),
};

static struct msm_sensor_ctrl_t sp0a28back_s_ctrl = {
	.msm_sensor_reg = &sp0a28back_regs,
	.msm_sensor_v4l2_ctrl_info = sp0a28back_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(sp0a28back_v4l2_ctrl_info),
	.sensor_i2c_client = &sp0a28back_sensor_i2c_client,
	.sensor_i2c_addr = 0x3d,
	.sensor_output_reg_addr = &sp0a28back_reg_addr,
	.sensor_id_info = &sp0a28back_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &sp0a28back_csi_params_array[0],
	.msm_sensor_mutex = &sp0a28back_mut,
	.sensor_i2c_driver = &sp0a28back_i2c_driver,
	.sensor_v4l2_subdev_info = sp0a28back_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sp0a28back_subdev_info),
	.sensor_v4l2_subdev_ops = &sp0a28back_subdev_ops,
	.func_tbl = &sp0a28back_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("SupperPix VGA YUV sensor driver");
MODULE_LICENSE("GPL v2");
