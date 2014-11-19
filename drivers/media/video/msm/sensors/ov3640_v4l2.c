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
#include "ov3640_v4l2.h"
//#include <linux/leds.h>

#define SENSOR_NAME "ov3640"
#define PLATFORM_DRIVER_NAME "msm_camera_ov3640"
#define ov3640_obj ov3640_##obj

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define OV3640_VERBOSE_DGB

#ifdef OV3640_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif
#define OV3640_MASTER_CLK_RATE             24000000
static int32_t vfe_clk = 266667000;//??
static struct msm_sensor_ctrl_t ov3640_s_ctrl;
static int is_first_preview = 0;
static int effect_value = CAMERA_EFFECT_OFF;
//static int16_t ov3640_effect = CAMERA_EFFECT_OFF;
static unsigned int SAT_U = 0x80;//??
static unsigned int SAT_V = 0x80;//??
static struct msm_sensor_ctrl_t * ov3640_v4l2_ctrl; //for OV3640 i2c read and write
static unsigned int ov3640_preview_shutter;
static unsigned int ov3640_preview_gain16;
//static unsigned short ov3640_preview_binning;
static unsigned int ov3640_preview_sysclk;
static unsigned int ov3640_preview_HTS;

static unsigned int OV3640_CAMERA_WB_AUTO = 0;//not support
static unsigned int OV3640_preview_R_gain;
static unsigned int OV3640_preview_G_gain;
static unsigned int OV3640_preview_B_gain;
extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;
static int is_autoflash = 0;
static int afinit = 1;
#define INVMASK(v)  0//(0xff-v)
#define  LED_MODE_OFF 0
#define  LED_MODE_AUTO 1
#define  LED_MODE_ON 2
#define  LED_MODE_TORCH 3
static int led_flash_mode = LED_MODE_OFF;
//extern struct rw_semaphore leds_list_lock;
//extern struct list_head leds_list;


DEFINE_MUTEX(ov3640_mut);




static struct msm_camera_i2c_conf_array ov3640_init_conf[] = {
	{&ov3640_init_settings[0],
		ARRAY_SIZE(ov3640_init_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov3640_confs[] = {
	{&ov3640_snap_settings[0],ARRAY_SIZE(ov3640_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov3640_prev_30fps_settings[0],ARRAY_SIZE(ov3640_prev_30fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov3640_prev_60fps_settings[0],ARRAY_SIZE(ov3640_prev_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov3640_prev_90fps_settings[0],ARRAY_SIZE(ov3640_prev_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_csi_params ov3640_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 15,
};

static struct v4l2_subdev_info ov3640_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t ov3640_dimensions[] = {
	{ /* For SNAPSHOT */
	.x_output = 2048,         /*2048*/
	.y_output = 1536,         /*1536*/
	.line_length_pclk = 2048,
	.frame_length_lines = 1536,
	.vt_pixel_clk = 42000000,//??
	.op_pixel_clk = 42000000,//??
	.binning_factor = 0x0,
	},
	{ /* For PREVIEW */
	.x_output = 640,         /*640*/
	.y_output = 480,         /*480*/
	.line_length_pclk = 640,
	.frame_length_lines = 480,
	.vt_pixel_clk = 56000000,
	.op_pixel_clk = 56000000,
	.binning_factor = 0x0,
	},

	{ /* For 60fps */

	},
	{ /* For 90fps */

	},
	{ /* For ZSL */

	},

};

static struct msm_sensor_output_reg_addr_t ov3640_reg_addr = {
//	.x_output = 0x3088,
//	.y_output = 0x308a,
//	.line_length_pclk = ,
//	.frame_length_lines = ,
};

static struct msm_camera_csi_params *ov3640_csi_params_array[] = {
	&ov3640_csi_params, /* Snapshot */
	&ov3640_csi_params, /* Preview */
	//&ov3640_csi_params, /* 60fps */
	//&ov3640_csi_params, /* 90fps */
	//&ov3640_csi_params, /* ZSL */
};

static struct msm_sensor_id_info_t ov3640_id_info = {
	.sensor_id_reg_addr = 0x300A,
	.sensor_id = 0x364c,
};

static struct msm_sensor_exp_gain_info_t ov3640_exp_gain_info = {
//	.coarse_int_time_addr = ,
//	.global_gain_addr = ,
//	.vert_offset = 4,
};



static int32_t ov3640_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
    CDBG("%s \n",__func__);

	return 0;

}


static int32_t ov3640_write_prev_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
						uint16_t gain, uint32_t line)
{
	return 0;
};


static const struct i2c_device_id ov3640_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov3640_s_ctrl},
	{ }
};

int32_t ov3640_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid_high_byte=0,chipid_low_byte = 0;
	uint16_t chipid = 0;
	rc = msm_camera_i2c_read(
		s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid_high_byte,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBG("%s: read id failed\n", __func__);
		return rc;
	}
	rc = msm_camera_i2c_read(
		s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_id_info->sensor_id_reg_addr+1, &chipid_low_byte,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBG("%s: read id failed\n", __func__);
		return rc;
	}
	chipid =(chipid_high_byte << 8) + chipid_low_byte;

	CDBG("msm_sensor id: 0x%x\n", chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		CDBG("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}else{
		CDBG("ov3640 match id ok\n");
	}
	return rc;
}

extern void camera_af_software_powerdown(struct i2c_client *client);
int32_t ov3640_sensor_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("%s IN\r\n", __func__);
	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	s_ctrl->sensor_i2c_addr = s_ctrl->sensor_i2c_addr;

	rc = msm_sensor_i2c_probe(client, id);

	if (client->dev.platform_data == NULL) {
		CDBG_HIGH("%s: NULL sensor data\n", __func__);
		return -EFAULT;
	}

	/* send software powerdown cmd to AF motor, avoid current leak */
	if(0 == rc)
	{
		camera_af_software_powerdown(client);
	}
	usleep_range(5000, 5100);

	return rc;
}

static struct i2c_driver ov3640_i2c_driver = {
	.id_table = ov3640_i2c_id,
	.probe  = ov3640_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov3640_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov3640_i2c_driver);
}

static struct v4l2_subdev_core_ops ov3640_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov3640_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov3640_subdev_ops = {
	.core = &ov3640_subdev_core_ops,
	.video  = &ov3640_subdev_video_ops,

};

int32_t ov3640_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *info = NULL;
	int32_t rc = 0;

	CDBG("%s IN\r\n", __func__);

	info = s_ctrl->sensordata;

	msleep(20);
	gpio_direction_output(info->sensor_pwd, 1);
	usleep_range(5000, 5100);
	msm_sensor_power_down(s_ctrl);
	msleep(40);
	if (s_ctrl->sensordata->pmic_gpio_enable){
		lcd_camera_power_onoff(0);
	}
	if (info->vcm_enable)
	{
	  rc = gpio_direction_output(info->vcm_pwd, 0);
	  if (!rc)
		  gpio_free(info->vcm_pwd);
	}
	return 0;
}

int32_t ov3640_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;
	CDBG("%s IN\r\n", __func__);
	CDBG("%s, sensor_pwd:%d, sensor_reset:%d\r\n",__func__, info->sensor_pwd, info->sensor_reset);
	usleep_range(5000, 6000);
	if (info->pmic_gpio_enable) {
		lcd_camera_power_onoff(1);
	}
	usleep_range(5000, 6000);
	rc = msm_sensor_power_up(s_ctrl);
	if (rc < 0) {
		CDBG("%s: msm_sensor_power_up failed\n", __func__);
		return rc;
	}
	/* turn on ldo and vreg */
	gpio_direction_output(info->sensor_pwd, 0);
	msleep(20);
	gpio_direction_output(info->sensor_reset, 1);
	msleep(10);
	gpio_direction_output(info->sensor_reset, 0);
	msleep(10);
	gpio_direction_output(info->sensor_reset, 1);
	msleep(10);
	if (info->vcm_enable)
	{
		rc = gpio_request(info->vcm_pwd, "cam_back_vcm");
		if (!rc) {
			CDBG("Enable VCM PWD\n");
			gpio_direction_output(info->vcm_pwd, 1);
		}
	}
	afinit = 1;

	return rc;

}
/* OV3640 dedicated code */
/********** Exposure optimization **********/
static int ov3640_read_i2c(unsigned int raddr, unsigned int *bdata)
{
	unsigned short data;
	int rc = msm_camera_i2c_read(ov3640_v4l2_ctrl->sensor_i2c_client,raddr, &data, MSM_CAMERA_I2C_BYTE_DATA);
	*bdata = data;
	return rc;
}
static int ov3640_write_i2c(unsigned int waddr, unsigned int bdata)
{
	return msm_camera_i2c_write(ov3640_v4l2_ctrl->sensor_i2c_client,waddr, (unsigned short)bdata, MSM_CAMERA_I2C_BYTE_DATA);
}

static int ov3640_af_setting(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	CDBG("--CAMERA-- ov3640_af_setting\n");

	//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3000, 0x20, MSM_CAMERA_I2C_BYTE_DATA);
	ov3640_write_i2c(0x308c, 0x00);
	ov3640_write_i2c(0x3104, 0x02);
	ov3640_write_i2c(0x3105, 0xff);
	ov3640_write_i2c(0x3106, 0x00);
	ov3640_write_i2c(0x3107, 0xff);

	rc = msm_camera_i2c_txdata(s_ctrl->sensor_i2c_client,
			ov3640_afinit_tbl, sizeof(ov3640_afinit_tbl)/sizeof(ov3640_afinit_tbl[0]));

	//rc = msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,ov3640_afinit_tbl, sizeof(ov3640_afinit_tbl)/sizeof(ov3640_afinit_tbl[0]), MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0)
	{
		CDBG_HIGH("--CAMERA-- AF_init failed\n");
		return rc;
	}

	ov3640_write_i2c(0x3F00,0x00);
	ov3640_write_i2c(0x3F01,0x00);
	ov3640_write_i2c(0x3F02,0x00);
	ov3640_write_i2c(0x3F03,0x00);
	ov3640_write_i2c(0x3F04,0x00);
	ov3640_write_i2c(0x3F05,0x00);
	ov3640_write_i2c(0x3F06,0x00);
	ov3640_write_i2c(0x3F07,0xFF);
	ov3640_write_i2c(0x3104,0x00);

	return rc;
}
static unsigned int OV3640_get_shutter(void)
{
  // read shutter, in number of line period
  unsigned int shutter = 0, extra_line = 0;
  unsigned int ret_l,ret_h;
  ret_l = ret_h = 0;
  ov3640_read_i2c(0x3002, &ret_h);
  ov3640_read_i2c(0x3003, &ret_l);
  shutter = (ret_h << 8) | (ret_l & 0xff) ;
  ret_l = ret_h = 0;
  ov3640_read_i2c(0x302d, &ret_h);
  ov3640_read_i2c(0x302e, &ret_l);
  extra_line = (ret_h << 8) | (ret_l & 0xff) ;
  return shutter + extra_line;
}

/******************************************************************************

******************************************************************************/
static int OV3640_set_shutter(unsigned int shutter)
{
  // write shutter, in number of line period
  int rc = 0;
  unsigned int temp;
  shutter = shutter & 0xffff;
  temp = shutter & 0xff;
  ov3640_write_i2c(0x3003, temp);
  temp = shutter >> 8;
  ov3640_write_i2c(0x3002, temp);
  return rc;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV3640_get_gain16(void)
{
  unsigned int gain16, temp;
  temp = 0;
  ov3640_read_i2c(0x3000, &temp);//ov3640_read_i2c(0x3001, &temp);
  CDBG("%s:Reg(0x3000) = 0x%x\n",__func__,temp);
  gain16 = ((temp>>4) + 1) * (16 + (temp & 0x0f));
  return gain16;
}

/******************************************************************************

******************************************************************************/
static int OV3640_set_gain16(unsigned int gain16)
{
  int rc = 0;
  unsigned int reg;
  gain16 = gain16 & 0x1ff;	// max gain is 32x
  reg = 0;
  if (gain16 > 32){
    gain16 = gain16 /2;
    reg = 0x10;
  }
  if (gain16 > 32){
    gain16 = gain16 /2;
    reg = reg | 0x20;
  }
  if (gain16 > 32){
    gain16 = gain16 /2;
    reg = reg | 0x40;
  }
  if (gain16 > 32){
    gain16 = gain16 /2;
    reg = reg | 0x80;
  }
  reg = reg | (gain16 -16);
  rc = ov3640_write_i2c(0x3000,reg + 1);
  msleep(100);
  rc |= ov3640_write_i2c(0x3000,reg);
  return rc;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV3640_get_sysclk(void)
{
  // calculate sysclk
  unsigned int temp1, temp2, XVCLK;
  unsigned int Indiv2x, Bit8Div, FreqDiv2x, PllDiv, SensorDiv, ScaleDiv, DvpDiv, ClkDiv, VCO, sysclk;
  unsigned int Indiv2x_map[] = { 2, 3, 4, 6, 4, 6, 8, 12};//{ 2, 3, 4, 6};
  unsigned int Bit8Div_map[] = { 1, 1, 4, 5};
  unsigned int FreqDiv2x_map[] = { 2, 3, 4, 6};
  unsigned int DvpDiv_map[] = { 1, 2, 8, 16};
  ov3640_read_i2c(0x300e, &temp1);
  // bit[5:0] PllDiv
  PllDiv = 64 - (temp1 & 0x3f);
  ov3640_read_i2c(0x300f, &temp1);
  // bit[2:0] Indiv
  temp2 = temp1 & 0x07;///0x03
  Indiv2x = Indiv2x_map[temp2];
  // bit[5:4] Bit8Div
  temp2 = (temp1 >> 4) & 0x03;
  Bit8Div = Bit8Div_map[temp2];
  // bit[7:6] FreqDiv
  temp2 = temp1 >> 6;
  FreqDiv2x = FreqDiv2x_map[temp2];
  ov3640_read_i2c(0x3010, &temp1);
  //bit[3:0] ScaleDiv
  temp2 = temp1 & 0x0f;
  if(temp2==0) {
    ScaleDiv = 1;
  } else {
    ScaleDiv = temp2 * 2;
  }
  // bit[4] SensorDiv
  if(temp1 & 0x10) {
    SensorDiv = 2;
  } else {
    SensorDiv = 1;
  }
  // bit[5] LaneDiv
  // bit[7:6] DvpDiv
  temp2 = temp1 >> 6;
  DvpDiv = DvpDiv_map[temp2];
  ov3640_read_i2c(0x3011, &temp1);
  // bit[5:0] ClkDiv
  temp2 = temp1 & 0x3f;
  ClkDiv = temp2 + 1;
  XVCLK = OV3640_MASTER_CLK_RATE/10000;
  CDBG("%s:XVCLK = 0x%x\n",__func__,XVCLK);
  CDBG("%s:Bit8Div = 0x%x\n",__func__,Bit8Div);
  CDBG("%s:FreqDiv2x = 0x%x\n",__func__,FreqDiv2x);
  CDBG("%s:PllDiv = 0x%x\n",__func__,PllDiv);
  CDBG("%s:Indiv2x = 0x%x\n",__func__,Indiv2x);
  VCO = XVCLK * Bit8Div * FreqDiv2x * PllDiv / Indiv2x ;
  sysclk = VCO / Bit8Div / SensorDiv / ClkDiv / 4;
  CDBG("%s:ClkDiv = 0x%x\n",__func__,ClkDiv);
  CDBG("%s:SensorDiv = 0x%x\n",__func__,SensorDiv);
  CDBG("%s:sysclk = 0x%x\n",__func__,sysclk);
  return sysclk;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV3640_get_HTS(void)
{
  // read HTS from register settings
  unsigned int HTS, extra_HTS;
  unsigned int ret_l,ret_h;
  ret_l = ret_h = 0;
  ov3640_read_i2c(0x3028, &ret_h);
  ov3640_read_i2c(0x3029, &ret_l);
  HTS = (ret_h << 8) | (ret_l & 0xff) ;
  ov3640_read_i2c(0x302c, &ret_l);
  extra_HTS = ret_l;
  return HTS + extra_HTS;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV3640_get_VTS(void)
{
  // read VTS from register settings
  unsigned int VTS, extra_VTS;
  unsigned int ret_l,ret_h;
  ret_l = ret_h = 0;
  ov3640_read_i2c(0x302a, &ret_h);
  ov3640_read_i2c(0x302b, &ret_l);
  VTS = (ret_h << 8) | (ret_l & 0xff) ;
  ov3640_read_i2c(0x302d, &ret_h);
  ov3640_read_i2c(0x302e, &ret_l);
  extra_VTS = (ret_h << 8) | (ret_l & 0xff) ;
  return VTS + extra_VTS;
}

/******************************************************************************

******************************************************************************/
/* not used
static int OV3640_set_VTS(unsigned int VTS)
{
  // write VTS to registers
  int rc = 0;
  unsigned int temp;
  temp = VTS & 0xff;
  rc = ov3640_write_i2c(0x302b, temp);
  temp = VTS>>8;
  rc |= ov3640_write_i2c(0x302a, temp);
  return rc;
}
*/
/******************************************************************************

******************************************************************************/
/* not used
static unsigned int OV3640_get_binning(void)
{
  // write VTS to registers
  unsigned int temp, binning;
  ov3640_read_i2c(0x300b, &temp);
  if(temp==0x52){
    // OV2650
    binning = 2;
  } else {
    // OV3640
    binning = 1;
  }
  return binning;
}
*/
/******************************************************************************

******************************************************************************/
/* not used
static unsigned int OV3640_get_light_frequency(void)
{
  // get banding filter value
  unsigned int temp, light_frequency;
  ov3640_read_i2c(0x3014, &temp);
  if (temp & 0x40) {
    // auto
    ov3640_read_i2c(0x508e, &temp);
    if (temp & 0x01){
      light_frequency = 50;
    } else {
      light_frequency = 60;
    }
  } else {
    // manual
    if (temp & 0x80){
      // 50Hz
      light_frequency = 50;
    } else {
      // 60Hz
      light_frequency = 60;
    }
  }
  return light_frequency;
}
*/
/******************************************************************************

******************************************************************************/
static int OV3640_set_bandingfilter(void)
{
  int rc = 0;
  unsigned int preview_VTS;
  unsigned int band_step60, max_band60, band_step50, max_band50;
  // read preview PCLK
  ov3640_preview_sysclk = OV3640_get_sysclk();
  // read preview HTS
  ov3640_preview_HTS = OV3640_get_HTS();
  // read preview VTS
  preview_VTS = OV3640_get_VTS();
  // calculate banding filter
  CDBG("%s:ov3640_preview_sysclk = 0x%x\n",__func__,ov3640_preview_sysclk);
  CDBG("%s:ov3640_preview_HTS = 0x%x\n",__func__,ov3640_preview_HTS);
  CDBG("%s:preview_VTS = 0x%x\n",__func__,preview_VTS);
  // 60Hz
  band_step60 = ov3640_preview_sysclk * 100/ov3640_preview_HTS * 100/120;
  rc = ov3640_write_i2c(0x3072, (band_step60 >> 8));
  rc |= ov3640_write_i2c(0x3073, (band_step60 & 0xff));
  max_band60 = ((preview_VTS-4)/band_step60);
  rc |= ov3640_write_i2c(0x301d, max_band60-1);
  // 50Hz
  CDBG("%s:band_step60 = 0x%x\n",__func__,band_step60);
  CDBG("%s:max_band60 = 0x%x\n",__func__,max_band60);
  band_step50 = ov3640_preview_sysclk * 100/ov3640_preview_HTS;
  rc |= ov3640_write_i2c(0x3070, (band_step50 >> 8));
  rc |= ov3640_write_i2c(0x3071, (band_step50 & 0xff));
  max_band50 = ((preview_VTS-4)/band_step50);
  rc |= ov3640_write_i2c(0x301c, max_band50-1);
  CDBG("%s:band_step50 = 0x%x\n",__func__,band_step50 );
  CDBG("%s:max_band50 = 0x%x\n",__func__,max_band50);
  return rc;
}

/******************************************************************************

******************************************************************************/
/*
static int ov3640_set_nightmode(int NightMode)

{
  int rc = 0;
  unsigned int temp;
  switch (NightMode) {
    case 0:{//Off
        ov3640_read_i2c(0x3014, &temp);
        temp = temp & 0xf7;			// night mode off, bit[3] = 0
        ov3640_write_i2c(0x3014, temp);
        // clear dummy lines
        //ov3640_write_i2c(0x302d, 0);//Commit that the function of auto reduce frame frequecy,because low frame frequecy result into longer AF time.
        //ov3640_write_i2c(0x302e, 0);
      }
      break;
    case 1: {// On
        ov3640_read_i2c(0x3014, &temp);
        temp = temp | 0x08;			// night mode on, bit[3] = 1
        ov3640_write_i2c(0x3014, temp);
      }
      break;
    default:
      break;
  }
  return rc;
}
*/
/******************************************************************************

******************************************************************************/
/* not used
static int ov3640_get_preview_exposure_gain(void)
{
  int rc = 0;
  ov3640_preview_shutter = OV3640_get_shutter();
  // read preview gain
  ov3640_preview_gain16 = OV3640_get_gain16();
  ov3640_preview_binning = OV3640_get_binning();
  // turn off night mode for capture
  rc = ov3640_set_nightmode(0);
  return rc;
}
*/
/******************************************************************************

******************************************************************************/
static int ov3640_set_preview_exposure_gain(void)
{
  int rc = 0;
  rc = OV3640_set_shutter(ov3640_preview_shutter);
  rc = OV3640_set_gain16(ov3640_preview_gain16);
  if(OV3640_CAMERA_WB_AUTO)
  {
	rc |= ov3640_write_i2c(0x3306, 0x00); //set to WB_AUTO,0x3306??
  }
  return rc;
}

/******************************************************************************

******************************************************************************/
/* not used
static int ov3640_set_capture_exposure_gain(void)
{
  int rc = 0;
  unsigned int capture_shutter, capture_gain16, capture_sysclk, capture_HTS, capture_VTS;
  unsigned int light_frequency, capture_bandingfilter, capture_max_band;
  unsigned long capture_gain16_shutter;
  unsigned int temp;

  //Step3: calculate and set capture exposure and gain
  // turn off AEC, AGC
  ov3640_read_i2c(0x3013, &temp);
  temp = temp & 0xfa;
  ov3640_write_i2c(0x3013, temp);
  // read capture sysclk
  capture_sysclk = OV3640_get_sysclk();
  // read capture HTS
  capture_HTS = OV3640_get_HTS();
  // read capture VTS
  capture_VTS = OV3640_get_VTS();
  // calculate capture banding filter
  light_frequency = OV3640_get_light_frequency();
  if (light_frequency == 60) {
    // 60Hz
    capture_bandingfilter = capture_sysclk * 100 / capture_HTS * 100 / 120;
  } else {
    // 50Hz
    capture_bandingfilter = capture_sysclk * 100 / capture_HTS;
  }
  capture_max_band = ((capture_VTS-4)/capture_bandingfilter);
  // calculate capture shutter
  capture_shutter = ov3640_preview_shutter;
  // gain to shutter
  capture_gain16 = ov3640_preview_gain16 * capture_sysclk/ov3640_preview_sysclk
  * ov3640_preview_HTS/capture_HTS * ov3640_preview_binning;//??
  if (capture_gain16 < 16) {
    capture_gain16 = 16;
  }
  capture_gain16_shutter = capture_gain16 * capture_shutter;
  if(capture_gain16_shutter < (capture_bandingfilter * 16)) {
    // shutter < 1/100
    capture_shutter = capture_gain16_shutter/16;
    capture_gain16 = capture_gain16_shutter/capture_shutter;
  } else {
    if(capture_gain16_shutter > (capture_bandingfilter*capture_max_band*16)) {
      // exposure reach max
      capture_shutter = capture_bandingfilter*capture_max_band;
      capture_gain16 = capture_gain16_shutter / capture_shutter;
    } else {
      // 1/100 < capture_shutter < max, capture_shutter = n/100
      capture_shutter = (capture_gain16_shutter/16/capture_bandingfilter)
        * capture_bandingfilter;
      capture_gain16 = capture_gain16_shutter / capture_shutter;
    }
  }
  // write capture gain
  rc |= OV3640_set_gain16(capture_gain16);
  // write capture shutter
  if (capture_shutter > (capture_VTS - 4)) {
    capture_VTS = capture_shutter + 4;
    rc |= OV3640_set_VTS(capture_VTS);
  }
  rc |= OV3640_set_shutter(capture_shutter);
  if(OV3640_CAMERA_WB_AUTO)
  {
    rc |= ov3640_write_i2c(0x3306, 0x02);//??
    rc |= ov3640_write_i2c(0x3337, OV3640_preview_R_gain);
    rc |= ov3640_write_i2c(0x3338, OV3640_preview_G_gain);
    rc |= ov3640_write_i2c(0x3339, OV3640_preview_B_gain);
  }
  return rc;
}
*/
static int OV3640_CalGainExposure(struct msm_sensor_ctrl_t *s_ctrl)
{


	unsigned int reg0x3001;
	unsigned int reg0x3002,reg0x3003,reg0x3013;        
	unsigned int reg0x3028,reg0x3029; 
	unsigned int reg0x302a,reg0x302b; 
	unsigned int reg0x302d,reg0x302e;
	unsigned int shutter; 
	unsigned int extra_lines, Preview_Exposure; 
	unsigned int Preview_Gain16; 
	unsigned int Preview_dummy_pixel; 
	unsigned int Capture_max_gain16, Capture_banding_Filter;
	unsigned int Preview_line_width,Capture_line_width,Capture_maximum_shutter; 
	unsigned int Capture_Exposure; 
	
	unsigned int Mclk =24; //MHz 
	unsigned int Preview_PCLK_frequency, Capture_PCLK_frequency; 
	unsigned int Gain_Exposure,Capture_Gain16; 
	unsigned int Gain; 
	
	unsigned int capture_max_gain = 127;// parm,from 4* gain to 2* gain 
	//uint8_t Default_Reg0x3028 = 0x09; 
	//uint8_t Default_Reg0x3029 = 0x47; 
	unsigned int Cap_Default_Reg0x302a = 0x06; 
	unsigned int Cap_Default_Reg0x302b = 0x20; 
	unsigned int capture_Dummy_pixel = 0; 
	unsigned int capture_dummy_lines = 0; 
	//uint16_t Default_XGA_Line_Width = 1188;
	unsigned int Default_QXGA_Line_Width = 2376; 
	unsigned int Default_QXGA_maximum_shutter = 1563; 
	
    int rc = 0;


	// 1. Stop Preview 
	//Stop AE/AG         
	ov3640_read_i2c( 0x3013, &reg0x3013);
	reg0x3013 = reg0x3013 & 0xf8;
	ov3640_write_i2c( 0x3013, reg0x3013);


	//Read back preview shutter 
	ov3640_read_i2c( 0x3002, &reg0x3002);
	ov3640_read_i2c( 0x3003, &reg0x3003);
	shutter = (reg0x3002 << 8) + reg0x3003; 
	
	//Read back extra line 
	ov3640_read_i2c( 0x302d, &reg0x302d);
	ov3640_read_i2c( 0x302e, &reg0x302e);
	//extra_lines = reg0x302e + (reg0x302d << 8); 
	extra_lines =0;
	Preview_Exposure = shutter + extra_lines; 
	
	//Read Back Gain for preview 
	ov3640_read_i2c( 0x3001, &reg0x3001);
	Preview_Gain16 = (((reg0x3001 & 0xf0)>>4) + 1) * (16 + (reg0x3001 & 0x0f)); 

        
    
	//Read back dummy pixels 
	ov3640_read_i2c( 0x3028, &reg0x3028);
	ov3640_read_i2c( 0x3029, &reg0x3029);
	// Preview_dummy_pixel = (((reg0x3028 - Default_Reg0x3028) & 0xf0)<<8) + reg0x3029-Default_Reg0x3029; 
	Preview_dummy_pixel=0;
	
	Preview_PCLK_frequency = (64 - 57) * 1 * Mclk / 1.5 / 2 / 3;  
	Capture_PCLK_frequency = (64 - 50) * 1 * Mclk / 1.5 / 2 / 3;  // 7.5fps 56MHz
	

	
	// 2.Calculate Capture Exposure 
	Capture_max_gain16 = capture_max_gain;
	
	//In common, Preview_dummy_pixel,Preview_dummy_line,Capture_dummy_pixel and 
	//Capture_dummy_line can be set to zero. 
	Preview_line_width = Default_QXGA_Line_Width + Preview_dummy_pixel ;
       
	Capture_line_width = Default_QXGA_Line_Width + capture_Dummy_pixel; 
	if(extra_lines>5000)
	{
		Capture_Exposure = 16*11/10*Preview_Exposure * Capture_PCLK_frequency/Preview_PCLK_frequency *Preview_line_width/Capture_line_width;
	}
	else if(extra_lines>2000)
	{
		Capture_Exposure = 16*18/10*Preview_Exposure * Capture_PCLK_frequency/Preview_PCLK_frequency *Preview_line_width/Capture_line_width;
	}
	else if(extra_lines>100)
	{
		Capture_Exposure = 16*20/10*Preview_Exposure * Capture_PCLK_frequency/Preview_PCLK_frequency *Preview_line_width/Capture_line_width;
	}  
	else
	{
		Capture_Exposure =5*22/100*Preview_Exposure * Capture_PCLK_frequency/Preview_PCLK_frequency *Preview_line_width/Capture_line_width;
	}

    CDBG("Capture_Exposure = %d \n", Capture_Exposure);
    CDBG("Preview_Exposure = %d \n", Preview_Exposure);
    CDBG("Capture_PCLK_frequency = %d \n", Capture_PCLK_frequency);
    CDBG("Preview_PCLK_frequency = %d \n", Preview_PCLK_frequency);
    CDBG("Preview_line_width = %d \n", Preview_line_width);
    CDBG("Capture_line_width = %d \n", Capture_line_width);
    CDBG("reg0x3001 = %d \n", reg0x3001);
    CDBG("reg0x3002 = %d \n", reg0x3002);
    CDBG("reg0x3003 = %d \n", reg0x3003);
    
	if(Capture_Exposure == 0)
	{ 
		Capture_Exposure =1 ;
	}

    Capture_banding_Filter = 117;   //  7.5 fps * 1563 / 100 = 117   (for 50hz)
       
	Capture_maximum_shutter = (Default_QXGA_maximum_shutter + capture_dummy_lines)/Capture_banding_Filter; 
	Capture_maximum_shutter = Capture_maximum_shutter * Capture_banding_Filter; 

	
	
	//redistribute gain and exposure 
	Gain_Exposure = Preview_Gain16 * Capture_Exposure; 
	if( Gain_Exposure ==0)
	{ 
		Gain_Exposure =1; 
	}
       
	if (Gain_Exposure < (Capture_banding_Filter * 16)) 
	{        // very bright
		// Exposure < 1/100 
		Capture_Exposure = Gain_Exposure /16; 
		Capture_Gain16 = (Gain_Exposure*2 + 1)/Capture_Exposure/2; 

	} 
	else 
	{ 
		if (Gain_Exposure > Capture_maximum_shutter * 16) 
		{      // very dark 
			// Exposure > Capture_Maximum_Shutter 
			Capture_Exposure = Capture_maximum_shutter; 
			Capture_Gain16 = (Gain_Exposure*2 + 1)/Capture_maximum_shutter/2; 

		
			if (Capture_Gain16 > Capture_max_gain16) 
			{   
               // Capture_Gain16 = Capture_max_gain16;
                Capture_Gain16 = Capture_max_gain16*3/2;//20121110
			} 

		} 
		else 
		{        
                     // normal 
			// 1/100(120) < Exposure < Capture_Maximum_Shutter, Exposure = n/100(120) 
			Capture_Exposure = Gain_Exposure/16/Capture_banding_Filter; 
			Capture_Exposure = Capture_Exposure * Capture_banding_Filter; 
			Capture_Gain16 = (Gain_Exposure*2 +1) / Capture_Exposure/2; 

		} 
	} 

	rc = msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_FULL);

	// 4.Write Registers 
	//write dummy pixels 
	ov3640_read_i2c( 0x3029, &reg0x3029);
	reg0x3029 = reg0x3029 + (capture_Dummy_pixel & 0x00ff); 
	
	ov3640_read_i2c( 0x3028, &reg0x3028);
	reg0x3028 = (reg0x3028 & 0x0f) | ((capture_Dummy_pixel & 0x0f00)>>4); 
		
	//Write Dummy Lines 
	reg0x302b = (capture_dummy_lines & 0x00ff ) + Cap_Default_Reg0x302b; 
	reg0x302a =( capture_dummy_lines >>8 ) + Cap_Default_Reg0x302a; 
       
	//Write Exposure 
	if (Capture_Exposure > Capture_maximum_shutter) 
	{ 
		shutter = Capture_maximum_shutter; 
		extra_lines = Capture_Exposure - Capture_maximum_shutter;
	}
	else 
	{ 
		shutter = Capture_Exposure; 
		extra_lines = 0; 
	} 
  
	reg0x3003 = shutter & 0x00ff; 
	reg0x3002 = (shutter>>8) & 0x00ff; 
	ov3640_write_i2c( 0x3003, reg0x3003);
	ov3640_write_i2c( 0x3002, reg0x3002);
   
	// Write Gain 
	Gain = 0; 
	if (Capture_Gain16 > 31) 
	{ 
		Capture_Gain16 = Capture_Gain16 /2; 
		Gain = 0x10; 
	} 
	if (Capture_Gain16 > 31) 
	{ 
		Capture_Gain16 = Capture_Gain16 /2; 
		Gain = Gain| 0x20; 
	} 
	if (Capture_Gain16 > 31) 
	{ 
		Capture_Gain16 = Capture_Gain16 /2; 
		Gain = Gain | 0x40; 
	} 
	if (Capture_Gain16 > 31) 
	{ 
		Capture_Gain16 = Capture_Gain16 /2; 
		Gain = Gain | 0x80; 
	} 
	if (Capture_Gain16 > 16) 
	{ 
		Gain = Gain | ((uint32_t)Capture_Gain16 -16); 
	} 

	ov3640_write_i2c( 0x3001, Gain);
 
	/*// Wait for 2 Vsync 
	// Capture the 3rd frame.*/ // must capture the third frame
	msleep(300); 

	return 0;
}

#if 0//not used
static int ov3640_set_flash_light(enum led_brightness brightness)
{
	struct led_classdev *led_cdev;

	CDBG("--CAMERA-- ov3640_set_flash_light brightness = %d\n", brightness);

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
static int ov3640_led_flash_auto(struct msm_camera_sensor_flash_data *fdata)
{
	//int tmp;
	CDBG("--CAMERA-- ov3640_led_flash_ctrl led_flash_mode = %d\n", led_flash_mode);
	
	//tmp = ov3640_read_i2c(0x56a1);
	//CDBG("--ov3640_led_flash_auto-- GAIN VALUE : %d\n", tmp);
	//if (tmp < 40) {
		msm_camera_flash_set_led_state(fdata, MSM_CAMERA_LED_HIGH);
	//}
	return 0;
}

int32_t ov3640_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;
	unsigned int temp = 0;

	ov3640_v4l2_ctrl = s_ctrl;

	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3012, 0x80, MSM_CAMERA_I2C_BYTE_DATA);
		msleep(20);
		msm_sensor_write_init_settings(s_ctrl);
		CDBG("AF_init: afinit = %d\n", afinit);
		if (afinit == 1) {
			rc = ov3640_af_setting(s_ctrl);
			if (rc < 0) {
				CDBG_HIGH("ov3640_af_setting failed\n");
				return rc;
			}
			afinit = 0;
		}
		csi_config = 0;
		is_first_preview = 1;
		ov3640_preview_shutter = OV3640_get_shutter();
		ov3640_preview_gain16 = OV3640_get_gain16();	
	} 
	else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		if(OV3640_CAMERA_WB_AUTO)
		{
		  rc |= ov3640_write_i2c(0x330c, 0x02);//??
		  ov3640_read_i2c(0x330f, &OV3640_preview_R_gain);
		  rc |= ov3640_write_i2c(0x330c, 0x03);
		  ov3640_read_i2c(0x330f, &OV3640_preview_G_gain);
		  rc |= ov3640_write_i2c(0x330c, 0x04);
		  ov3640_read_i2c(0x330f, &OV3640_preview_B_gain);
		}
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
		msleep(130);
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_CSIC_CFG,
			s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(30);
			csi_config = 1;
		}
		if(is_first_preview)
		{
			msleep(100);
		}
		else
		{
			msleep(10);
			is_first_preview = 0;
		}
		
		if (res == MSM_SENSOR_RES_QTR){
			//turn off flash when preview
			//ov3640_set_flash_light(LED_OFF);
			msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_OFF);
			rc |= ov3640_write_i2c(CMD_MAIN,0x08);
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_QTR);

			rc |= OV3640_set_bandingfilter();
			// turn on AEC, AGC
			rc |= ov3640_read_i2c(0x3013, &temp);
			temp = temp | 0x05;
			rc |= ov3640_write_i2c(0x3013, temp);
			ov3640_set_preview_exposure_gain();
			CDBG("%s, ov3640_preview_tbl_30fps is set\n",__func__);
		}else if(res==MSM_SENSOR_RES_FULL){
			CDBG("snapshot in, is_autoflash - %d\n", is_autoflash);
			if (led_flash_mode == LED_MODE_ON)msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_HIGH);
			else if(led_flash_mode == LED_MODE_AUTO)ov3640_led_flash_auto(s_ctrl->sensordata->flash_data);
			//ov3640_get_preview_exposure_gain();
			//if (is_autoflash == 1) {
			//  ov3640_set_flash_light(LED_FULL);
			//}
			//msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_FULL); //move it into OV3640_CalGainExposure
			CDBG("%s, ov3640_capture_tbl is set\n",__func__);
			//ov3640_set_capture_exposure_gain();//replace it with OV3640_CalGainExposure, to add the lightness of the taken photoes
			OV3640_CalGainExposure(s_ctrl);
		}
		if (res == MSM_SENSOR_RES_4){
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,NOTIFY_PCLK_CHANGE,&vfe_clk);
		}
		msleep(20);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;

}
static struct msm_camera_i2c_reg_conf ov3640_saturation[][4] = {
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x00, 0x00},{0x3359, 0x00, 0x00},},	/* SATURATION LEVEL0*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x08, 0x00},{0x3359, 0x08, 0x00},},	/* SATURATION LEVEL1*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x10, 0x00},{0x3359, 0x10, 0x00},},	/* SATURATION LEVEL2*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x20, 0x00},{0x3359, 0x20, 0x00},},	/* SATURATION LEVEL3*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x30, 0x00},{0x3359, 0x30, 0x00},},	/* SATURATION LEVEL4*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x40, 0x00},{0x3359, 0x40, 0x00},},	/* SATURATION LEVEL5,default*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x50, 0x00},{0x3359, 0x50, 0x00},},	/* SATURATION LEVEL6*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x60, 0x00},{0x3359, 0x60, 0x00},},	/* SATURATION LEVEL7*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x70, 0x00},{0x3359, 0x70, 0x00},},	/* SATURATION LEVEL8*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x78, 0x00},{0x3359, 0x78, 0x00},},	/* SATURATION LEVEL9*/
	{{0x3302, 0xef, 0x00}, {0x3355, 0x02, 0x00}, {0x3358, 0x80, 0x00},{0x3359, 0x80, 0x00},},	/* SATURATION LEVEL10*/
};
static struct msm_camera_i2c_conf_array ov3640_saturation_confs[][1] = {
	{{ov3640_saturation[0], ARRAY_SIZE(ov3640_saturation[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[1], ARRAY_SIZE(ov3640_saturation[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[2], ARRAY_SIZE(ov3640_saturation[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[3], ARRAY_SIZE(ov3640_saturation[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[4], ARRAY_SIZE(ov3640_saturation[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[5], ARRAY_SIZE(ov3640_saturation[5]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[6], ARRAY_SIZE(ov3640_saturation[6]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[7], ARRAY_SIZE(ov3640_saturation[7]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[8], ARRAY_SIZE(ov3640_saturation[8]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[9], ARRAY_SIZE(ov3640_saturation[9]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_saturation[10], ARRAY_SIZE(ov3640_saturation[10]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov3640_saturation_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov3640_saturation_enum_confs = {
	.conf = &ov3640_saturation_confs[0][0],
	.conf_enum = ov3640_saturation_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_saturation_enum_map),
	.num_index = ARRAY_SIZE(ov3640_saturation_confs),
	.num_conf = ARRAY_SIZE(ov3640_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov3640_contrast[][5] = {
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x10,0x00},{0x335d,0x10,0x00},}, /* CONTRAST L0*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x10,0x00},{0x335d,0x10,0x00},}, /* CONTRAST L1*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x14,0x00},{0x335d,0x14,0x00},}, /* CONTRAST L2*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x18,0x00},{0x335d,0x18,0x00},}, /* CONTRAST L3*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x1c,0x00},{0x335d,0x1c,0x00},}, /* CONTRAST L4*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x20,0x00},{0x335d,0x20,0x00},}, /* CONTRAST L5,default*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x24,0x00},{0x335d,0x24,0x00},}, /* CONTRAST L6*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x28,0x00},{0x335d,0x28,0x00},}, /* CONTRAST L7*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x2c,0x00},{0x335d,0x2c,0x00},}, /* CONTRAST L8*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x30,0x00},{0x335d,0x30,0x00},}, /* CONTRAST L9*/
	{{0x3302,0xef,0x00},{0x3355,0x04,0x00},{0x3354,0x01,0x00},{0x335c,0x30,0x00},{0x335d,0x30,0x00},}, /* CONTRAST L10*/
};

static struct msm_camera_i2c_conf_array ov3640_contrast_confs[][1] = {
	{{ov3640_contrast[0], ARRAY_SIZE(ov3640_contrast[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[1], ARRAY_SIZE(ov3640_contrast[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[2], ARRAY_SIZE(ov3640_contrast[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[3], ARRAY_SIZE(ov3640_contrast[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[4], ARRAY_SIZE(ov3640_contrast[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[5], ARRAY_SIZE(ov3640_contrast[5]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[6], ARRAY_SIZE(ov3640_contrast[6]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[7], ARRAY_SIZE(ov3640_contrast[7]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[8], ARRAY_SIZE(ov3640_contrast[8]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[9], ARRAY_SIZE(ov3640_contrast[9]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_contrast[10], ARRAY_SIZE(ov3640_contrast[10]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};


static int ov3640_contrast_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov3640_contrast_enum_confs = {
	.conf = &ov3640_contrast_confs[0][0],
	.conf_enum = ov3640_contrast_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_contrast_enum_map),
	.num_index = ARRAY_SIZE(ov3640_contrast_confs),
	.num_conf = ARRAY_SIZE(ov3640_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
static struct msm_camera_i2c_reg_conf ov3640_sharpness[][2] = {
	{{0x332d,0x41,0x00},{0x332d, 0x41,0x00},},/* SHARPNESS LEVEL 0*/
	{{0x332d,0x43,0x00},{0x332d, 0x43,0x00},},/* SHARPNESS LEVEL 1*/
	{{0x332d,0x44,0x00},{0x332d, 0x44,0x00},},/* SHARPNESS LEVEL 2*/
	{{0x332d,0x60,0x00},{0x332f, 0x03,0x00},},/* SHARPNESS LEVEL 3,default*/
	{{0x332d,0x45,0x00},{0x332d, 0x45,0x00},},/* SHARPNESS LEVEL 4*/
	{{0x332d,0x47,0x00},{0x332d, 0x47,0x00},},/* SHARPNESS LEVEL 5*/
	{{0x332d,0x49,0x00},{0x332d, 0x49,0x00},},/* SHARPNESS LEVEL 6*/
};

static struct msm_camera_i2c_conf_array ov3640_sharpness_confs[][1] = {
	{{ov3640_sharpness[0], ARRAY_SIZE(ov3640_sharpness[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_sharpness[1], ARRAY_SIZE(ov3640_sharpness[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_sharpness[2], ARRAY_SIZE(ov3640_sharpness[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_sharpness[3], ARRAY_SIZE(ov3640_sharpness[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_sharpness[4], ARRAY_SIZE(ov3640_sharpness[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_sharpness[5], ARRAY_SIZE(ov3640_sharpness[5]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_sharpness[6], ARRAY_SIZE(ov3640_sharpness[6]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov3640_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
	MSM_V4L2_SHARPNESS_L6,
};

static struct msm_camera_i2c_enum_conf_array ov3640_sharpness_enum_confs = {
	.conf = &ov3640_sharpness_confs[0][0],
	.conf_enum = ov3640_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_sharpness_enum_map),
	.num_index = ARRAY_SIZE(ov3640_sharpness_confs),
	.num_conf = ARRAY_SIZE(ov3640_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov3640_exposure[][3] = {
	{{0x3018,0x10},{0x3019,0x08},{0x301a,0x21}},//MSM_V4L2_EXPOSURE_N2
	{{0x3018,0x20},{0x3019,0x18},{0x301a,0x41}},//MSM_V4L2_EXPOSURE_N1
	{{0x3018,0x38},{0x3019,0x30},{0x301a,0x61}},//MSM_V4L2_EXPOSURE_D,default
	{{0x3018,0x50},{0x3019,0x48},{0x301a,0x91}},//MSM_V4L2_EXPOSURE_P1
	{{0x3018,0x60},{0x3019,0x58},{0x301a,0xa1}},//MSM_V4L2_EXPOSURE_P2
};

static struct msm_camera_i2c_conf_array ov3640_exposure_confs[][1] = {
	{{ov3640_exposure[0], ARRAY_SIZE(ov3640_exposure[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_exposure[1], ARRAY_SIZE(ov3640_exposure[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_exposure[2], ARRAY_SIZE(ov3640_exposure[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_exposure[3], ARRAY_SIZE(ov3640_exposure[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_exposure[4], ARRAY_SIZE(ov3640_exposure[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov3640_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array ov3640_exposure_enum_confs = {
	.conf = &ov3640_exposure_confs[0][0],
	.conf_enum = ov3640_exposure_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_exposure_enum_map),
	.num_index = ARRAY_SIZE(ov3640_exposure_confs),
	.num_conf = ARRAY_SIZE(ov3640_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
#if 1//not support
static struct msm_camera_i2c_reg_conf ov3640_iso[][3] = {
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},   /*ISO_AUTO*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},   /*ISO_DEBLUR*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_100*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_200*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},   /*ISO_400*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_800*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_1600*/
};


static struct msm_camera_i2c_conf_array ov3640_iso_confs[][1] = {
	{{ov3640_iso[0], ARRAY_SIZE(ov3640_iso[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_iso[1], ARRAY_SIZE(ov3640_iso[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_iso[2], ARRAY_SIZE(ov3640_iso[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_iso[3], ARRAY_SIZE(ov3640_iso[3]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_iso[4], ARRAY_SIZE(ov3640_iso[4]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_iso[5], ARRAY_SIZE(ov3640_iso[5]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov3640_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};


static struct msm_camera_i2c_enum_conf_array ov3640_iso_enum_confs = {
	.conf = &ov3640_iso_confs[0][0],
	.conf_enum = ov3640_iso_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_iso_enum_map),
	.num_index = ARRAY_SIZE(ov3640_iso_confs),
	.num_conf = ARRAY_SIZE(ov3640_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
#endif

static struct msm_camera_i2c_reg_conf ov3640_special_effect[][4] = {
	{{0x3302,0xef,0x00},{0x3355,0x00,0},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_OFF,
	{{0x3302,0xef,0x00},{0x3355,0x18,0},{0x335a,0x80,0},{0x335b,0x80,0}},//MSM_V4L2_EFFECT_MONO,
	{{0x3302,0xef,0x00},{0x3355,0x40,0},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_NEGATIVE,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_SOLARIZE,
	{{0x3302,0xef,0x00},{0x3355,0x18,0},{0x335a,0x40,0},{0x335b,0xa6,0}},//MSM_V4L2_EFFECT_SEPIA,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_POSTERAIZE,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_WHITEBOARD,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_BLACKBOARD,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_AQUA,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_EMBOSS,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_SKETCH,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_NEON,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_MAX,
};

static struct msm_camera_i2c_conf_array ov3640_special_effect_confs[][1] = {
	{{ov3640_special_effect[0],  ARRAY_SIZE(ov3640_special_effect[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[1],  ARRAY_SIZE(ov3640_special_effect[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[2],  ARRAY_SIZE(ov3640_special_effect[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[3],  ARRAY_SIZE(ov3640_special_effect[3]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[4],  ARRAY_SIZE(ov3640_special_effect[4]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[5],  ARRAY_SIZE(ov3640_special_effect[5]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[6],  ARRAY_SIZE(ov3640_special_effect[6]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[7],  ARRAY_SIZE(ov3640_special_effect[7]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[8],  ARRAY_SIZE(ov3640_special_effect[8]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[9],  ARRAY_SIZE(ov3640_special_effect[9]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[10], ARRAY_SIZE(ov3640_special_effect[10]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[11], ARRAY_SIZE(ov3640_special_effect[11]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_special_effect[12], ARRAY_SIZE(ov3640_special_effect[12]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};



static int ov3640_special_effect_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov3640_special_effect_enum_confs = {
	.conf = &ov3640_special_effect_confs[0][0],
	.conf_enum = ov3640_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_special_effect_enum_map),
	.num_index = ARRAY_SIZE(ov3640_special_effect_confs),
	.num_conf = ARRAY_SIZE(ov3640_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov3640_antibanding[][5] = {
//	{{0x3014,0x00,0xc0},{0x3013,0x20,0x20},{0x3072,0x00,0},{0x3073,0x62,0},{0x301D,0x06,0}},   /* ANTIBANDING 60HZ*/
//	{{0x3014,0x80,0xc0},{0x3013,0x20,0x20},{0x3070,0x00,0},{0x3071,0x75,0},{0x301C,0x05,0}},   /*ANTIBANDING 50HZ*/
//	{{0x3014,0x40,0xc0},{0x3013,0x20,0x20},{0x3014,0x44,0},{-1,-1,-1},{-1,-1,-1}},	 /*ANTIBANDING AUTO*/
	{{0x3014,0x00,INVMASK(0xc0)},{0x3013,0x20,INVMASK(0x20)},{0x3072,0x00,0},{0x3073,0x62,0},{0x301D,0x06,0}},   /* ANTIBANDING 60HZ*/
	{{0x3014,0x80,INVMASK(0xc0)},{0x3013,0x20,INVMASK(0x20)},{0x3070,0x00,0},{0x3071,0x75,0},{0x301C,0x05,0}},   /*ANTIBANDING 50HZ*/
	{{0x3014,0x40,INVMASK(0xc0)},{0x3013,0x20,INVMASK(0x20)},{0x3014,0x44,0},{-1,-1,-1},{-1,-1,-1}},	 /*ANTIBANDING AUTO*/
};


static struct msm_camera_i2c_conf_array ov3640_antibanding_confs[][1] = {
	{{ov3640_antibanding[0], ARRAY_SIZE(ov3640_antibanding[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_antibanding[1], ARRAY_SIZE(ov3640_antibanding[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_antibanding[2], ARRAY_SIZE(ov3640_antibanding[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov3640_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};


static struct msm_camera_i2c_enum_conf_array ov3640_antibanding_enum_confs = {
	.conf = &ov3640_antibanding_confs[0][0],
	.conf_enum = ov3640_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_antibanding_enum_map),
	.num_index = ARRAY_SIZE(ov3640_antibanding_confs),
	.num_conf = ARRAY_SIZE(ov3640_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
/*
static struct msm_camera_i2c_reg_conf ov3640_wb_oem[][4] = {
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_OFF,			  
	{{0x332b,0x00,0x08},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_AUTO ,		  
	{{0x332b,0x08,0x08},{0x33a7,0x44,0x00},{0x33a8,0x40,0x00},{0x33a9,0x70,0x00},},//MSM_V4L2_WB_CUSTOM,		  
	{{0x332b,0x00,0x08},{0x33a7,0x52,0x00},{0x33a8,0x40,0x00},{0x33a9,0x58,0x00},},//MSM_V4L2_WB_INCANDESCENT,   
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_FLUORESCENT,	  
	{{0x332b,0x00,0x08},{0x33a7,0x5e,0x00},{0x33a8,0x40,0x00},{0x33a9,0x46,0x00},},//MSM_V4L2_WB_DAYLIGHT, 	  
	{{0x332b,0x00,0x08},{0x33a7,0x68,0x00},{0x33a8,0x40,0x00},{0x33a9,0x4e,0x00},},//MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};
*/
/*
static struct msm_camera_i2c_reg_conf ov3640_wb_oem[][4] = {
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_OFF,			  
	{{0x332b,0x00,INVMASK(0x08)},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_AUTO ,		  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x44,0x00},{0x33a8,0x40,0x00},{0x33a9,0x70,0x00},},//MSM_V4L2_WB_CUSTOM,		  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x52,0x00},{0x33a8,0x40,0x00},{0x33a9,0x58,0x00},},//MSM_V4L2_WB_INCANDESCENT,   
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_FLUORESCENT,	  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x5e,0x00},{0x33a8,0x40,0x00},{0x33a9,0x46,0x00},},//MSM_V4L2_WB_DAYLIGHT, 	  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x68,0x00},{0x33a8,0x40,0x00},{0x33a9,0x4e,0x00},},//MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};
*/
static struct msm_camera_i2c_reg_conf ov3640_wb_oem[][4] = {
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_OFF,			  
	{{0x332b,0x00,INVMASK(0x08)},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_AUTO ,		  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x52,0x00},{0x33a8,0x40,0x00},{0x33a9,0x58,0x00},},//MSM_V4L2_WB_CUSTOM,		  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x44,0x00},{0x33a8,0x40,0x00},{0x33a9,0x70,0x00},},//MSM_V4L2_WB_INCANDESCENT,   
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_FLUORESCENT,	  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x5e,0x00},{0x33a8,0x40,0x00},{0x33a9,0x48,0x00},},//MSM_V4L2_WB_DAYLIGHT, 	  
	{{0x332b,0x08,INVMASK(0x08)},{0x33a7,0x68,0x00},{0x33a8,0x40,0x00},{0x33a9,0x4e,0x00},},//MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_conf_array ov3640_wb_oem_confs[][1] = {
	{{ov3640_wb_oem[0], ARRAY_SIZE(ov3640_wb_oem[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_wb_oem[1], ARRAY_SIZE(ov3640_wb_oem[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_wb_oem[2], ARRAY_SIZE(ov3640_wb_oem[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_wb_oem[3], ARRAY_SIZE(ov3640_wb_oem[3]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_wb_oem[4], ARRAY_SIZE(ov3640_wb_oem[4]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_wb_oem[5], ARRAY_SIZE(ov3640_wb_oem[5]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov3640_wb_oem[6], ARRAY_SIZE(ov3640_wb_oem[6]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov3640_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array ov3640_wb_oem_enum_confs = {
	.conf = &ov3640_wb_oem_confs[0][0],
	.conf_enum = ov3640_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(ov3640_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(ov3640_wb_oem_confs),
	.num_conf = ARRAY_SIZE(ov3640_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};


int ov3640_saturation_msm_sensor_s_ctrl_by_enum(
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
		SAT_U = SAT_V = value * 0x10;
		CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
	return rc;
}


int ov3640_contrast_msm_sensor_s_ctrl_by_enum(
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

int ov3640_sharpness_msm_sensor_s_ctrl_by_enum(
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

int ov3640_flash_mode_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	led_flash_mode = value;
	CDBG("--CAMERA-- %s flash mode = %d\n", __func__, led_flash_mode);
	return rc;
}

int ov3640_auto_focus_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	unsigned int i,af_ack = 0;
	
	rc |= ov3640_read_i2c(STA_FOCUS, &af_ack);
	CDBG("%s enter,rc=%d, STA_FOCUS = %d\n",__func__,rc,af_ack);
	rc = ov3640_write_i2c(CMD_TAG,0x01);
	rc |= ov3640_write_i2c(CMD_MAIN,0x03);
	msleep(50);
	for (i = 0; i < 50; i++) {
	  rc |= ov3640_read_i2c(STA_FOCUS, &af_ack);
	  CDBG("%s,rc=%d, STA_FOCUS = %d\n",__func__,rc,af_ack);
	  if (rc >= 0 && af_ack == 0x02){
		  CDBG_HIGH("%s end, time=%d, rc=%d, STA_FOCUS = %d\n",__func__, i*50, rc,af_ack);
		  //rc |= ov3640_write_i2c(CMD_MAIN,0x08);//just do it when entering preview mode, it returns to infinite scene
		  return 0;
	  }
	  msleep(50);
	}
	CDBG_HIGH("%s end, time=%d, rc=%d, STA_FOCUS = %d\n",__func__, i*50, rc,af_ack);
	//rc |= ov3640_write_i2c(CMD_MAIN,0x08);//just do it when entering preview mode, it returns to infinite scene
	return -1;
}

int ov3640_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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

int ov3640_wb_oem_msm_sensor_s_ctrl_by_enum(
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
		effect_value = value;
	}
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	return rc;
}

int ov3640_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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
#if 1//not support
int ov3640_msm_iso_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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
#endif
struct msm_sensor_v4l2_ctrl_info_t ov3640_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L8,
		.step = 1,
		.enum_cfg_settings = &ov3640_saturation_enum_confs,
		.s_v4l2_ctrl = ov3640_msm_sensor_s_ctrl_by_enum,//ov3640_saturation_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L8,
		.step = 1,
		.enum_cfg_settings = &ov3640_contrast_enum_confs,
		.s_v4l2_ctrl =ov3640_msm_sensor_s_ctrl_by_enum, //ov3640_contrast_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L6,
		.step = 1,
		.enum_cfg_settings = &ov3640_sharpness_enum_confs,
		.s_v4l2_ctrl =ov3640_msm_sensor_s_ctrl_by_enum, //ov3640_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &ov3640_exposure_enum_confs,
		.s_v4l2_ctrl = ov3640_msm_sensor_s_ctrl_by_enum,
	},
#if 1//not support
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &ov3640_iso_enum_confs,
		.s_v4l2_ctrl = ov3640_msm_iso_sensor_s_ctrl_by_enum,
	},
#endif
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_MAX,
		.step = 1,
		.enum_cfg_settings = &ov3640_special_effect_enum_confs,
		.s_v4l2_ctrl = ov3640_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &ov3640_antibanding_enum_confs,
		.s_v4l2_ctrl = ov3640_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &ov3640_wb_oem_enum_confs,
		.s_v4l2_ctrl = ov3640_wb_oem_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_AUTO_FOCUS,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = ov3640_auto_focus_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_LED_FLASH_MODE,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = ov3640_flash_mode_msm_sensor_s_ctrl_by_enum,
	},
};

static struct msm_sensor_fn_t ov3640_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = ov3640_write_prev_exp_gain,
	.sensor_write_snapshot_exp_gain = ov3640_write_pict_exp_gain,
	.sensor_csi_setting = ov3640_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = ov3640_sensor_power_up,
	.sensor_power_down = ov3640_sensor_power_down,
	.sensor_match_id = ov3640_sensor_match_id,
};

static struct msm_sensor_reg_t ov3640_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = ov3640_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov3640_start_settings),
	.stop_stream_conf = ov3640_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov3640_stop_settings),
	.group_hold_on_conf = ov3640_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(ov3640_groupon_settings),
	.group_hold_off_conf = ov3640_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(ov3640_groupoff_settings),
	.init_settings = &ov3640_init_conf[0],
	.init_size = ARRAY_SIZE(ov3640_init_conf),
	.mode_settings = &ov3640_confs[0],
	.output_settings = &ov3640_dimensions[0],
	.num_conf = ARRAY_SIZE(ov3640_confs),
};

static struct msm_sensor_ctrl_t ov3640_s_ctrl = {
	.msm_sensor_v4l2_ctrl_info = ov3640_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(ov3640_v4l2_ctrl_info),
	.msm_sensor_reg = &ov3640_regs,
	.sensor_i2c_client = &ov3640_sensor_i2c_client,
	.sensor_i2c_addr =  0x78,
	.sensor_output_reg_addr = &ov3640_reg_addr,
	.sensor_id_info = &ov3640_id_info,
	.sensor_exp_gain_info = &ov3640_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &ov3640_csi_params_array[0],
	.msm_sensor_mutex = &ov3640_mut,
	.sensor_i2c_driver = &ov3640_i2c_driver,
	.sensor_v4l2_subdev_info = ov3640_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov3640_subdev_info),
	.sensor_v4l2_subdev_ops = &ov3640_subdev_ops,
	.func_tbl = &ov3640_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision YUV sensor driver");
MODULE_LICENSE("GPL v2");
