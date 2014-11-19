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
#include "ov2655_v4l2.h"
//#include <linux/leds.h>

#define SENSOR_NAME "ov2655"
#define PLATFORM_DRIVER_NAME "msm_camera_ov2655"
#define ov2655_obj ov2655_##obj

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define OV2655_VERBOSE_DGB

#ifdef OV2655_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif
#define OV2655_MASTER_CLK_RATE             24000000
static int32_t vfe_clk = 266667000;
static struct msm_sensor_ctrl_t ov2655_s_ctrl;
static int is_first_preview = 0;
static int effect_value = CAMERA_EFFECT_OFF;
//static int16_t ov2655_effect = CAMERA_EFFECT_OFF;
static unsigned int SAT_U = 0x80;
static unsigned int SAT_V = 0x80;
static struct msm_sensor_ctrl_t * ov2655_v4l2_ctrl; //for OV2655 i2c read and write
static unsigned int ov2655_preview_shutter;
static unsigned int ov2655_preview_gain16;
static unsigned short ov2655_preview_binning;
static unsigned int ov2655_preview_sysclk;
static unsigned int ov2655_preview_HTS;

static unsigned int OV2655_CAMERA_WB_AUTO = 0;
static unsigned int OV2655_preview_R_gain;
static unsigned int OV2655_preview_G_gain;
static unsigned int OV2655_preview_B_gain;
extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;
static int is_autoflash = 0;
static int afinit = 1;

#define  LED_MODE_OFF 0
#define  LED_MODE_AUTO 1
#define  LED_MODE_ON 2
#define  LED_MODE_TORCH 3
static int led_flash_mode = LED_MODE_OFF;

DEFINE_MUTEX(ov2655_mut);




static struct msm_camera_i2c_conf_array ov2655_init_conf[] = {
	{&ov2655_init_settings[0],
		ARRAY_SIZE(ov2655_init_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov2655_confs[] = {
	{&ov2655_snap_settings[0],ARRAY_SIZE(ov2655_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov2655_prev_30fps_settings[0],ARRAY_SIZE(ov2655_prev_30fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov2655_prev_60fps_settings[0],ARRAY_SIZE(ov2655_prev_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov2655_prev_90fps_settings[0],ARRAY_SIZE(ov2655_prev_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_csi_params ov2655_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 6,
};

static struct v4l2_subdev_info ov2655_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t ov2655_dimensions[] = {
	{ /* For SNAPSHOT */
	.x_output = 1600,         /*1600*/
	.y_output = 1200,         /*1200*/
	.line_length_pclk = 1600,
	.frame_length_lines = 1200,
	.vt_pixel_clk = 42000000,
	.op_pixel_clk = 42000000,
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

static struct msm_sensor_output_reg_addr_t ov2655_reg_addr = {
//	.x_output = 0x3808,
//	.y_output = 0x380A,
//	.line_length_pclk = 0x380C,
//	.frame_length_lines = 0x380E,
};

static struct msm_camera_csi_params *ov2655_csi_params_array[] = {
	&ov2655_csi_params, /* Snapshot */
	&ov2655_csi_params, /* Preview */
	//&ov2655_csi_params, /* 60fps */
	//&ov2655_csi_params, /* 90fps */
	//&ov2655_csi_params, /* ZSL */
};

static struct msm_sensor_id_info_t ov2655_id_info = {
	.sensor_id_reg_addr = 0x300a,
	.sensor_id = 0x2656,
};

static struct msm_sensor_exp_gain_info_t ov2655_exp_gain_info = {
//	.coarse_int_time_addr = 0x3500,
//	.global_gain_addr = 0x350A,
//	.vert_offset = 4,
};



static int32_t ov2655_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
    CDBG("%s \n",__func__);

	return 0;

}


static int32_t ov2655_write_prev_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
						uint16_t gain, uint32_t line)
{
	return 0;
};


static const struct i2c_device_id ov2655_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov2655_s_ctrl},
	{ }
};

int32_t ov2655_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
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
		CDBG("ov2655 match id ok\n");
	}
	return rc;
}

extern void camera_af_software_powerdown(struct i2c_client *client);
int32_t ov2655_sensor_i2c_probe(struct i2c_client *client,
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

static struct i2c_driver ov2655_i2c_driver = {
	.id_table = ov2655_i2c_id,
	.probe  = ov2655_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov2655_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov2655_i2c_driver);
}

static struct v4l2_subdev_core_ops ov2655_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov2655_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov2655_subdev_ops = {
	.core = &ov2655_subdev_core_ops,
	.video  = &ov2655_subdev_video_ops,

};

int32_t ov2655_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *info = NULL;

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
	return 0;
}

int32_t ov2655_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
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

	return rc;

}
/* ov2655 dedicated code */
/********** Exposure optimization **********/
static int ov2655_read_i2c(unsigned int raddr, unsigned int *bdata)
{
	unsigned short data;
	int rc = msm_camera_i2c_read(ov2655_v4l2_ctrl->sensor_i2c_client,raddr, &data, MSM_CAMERA_I2C_BYTE_DATA);
	*bdata = data;
	return rc;
}
static int ov2655_write_i2c(unsigned int waddr, unsigned int bdata)
{
	return msm_camera_i2c_write(ov2655_v4l2_ctrl->sensor_i2c_client,waddr, (unsigned short)bdata, MSM_CAMERA_I2C_BYTE_DATA);
}

static int ov2655_af_setting(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	CDBG("--CAMERA-- ov2655_af_setting\n");
	//ov2655_afinit_tbl
	return rc;
}
static unsigned int OV2655_get_shutter(void)
{
  // read shutter, in number of line period
  unsigned int shutter = 0, extra_line = 0;
  unsigned int ret_l,ret_h;
  ret_l = ret_h = 0;
  ov2655_read_i2c(0x3002, &ret_h);
  ov2655_read_i2c(0x3003, &ret_l);
  shutter = (ret_h << 8) | (ret_l & 0xff) ;
  ret_l = ret_h = 0;
  ov2655_read_i2c(0x302d, &ret_h);
  ov2655_read_i2c(0x302e, &ret_l);
  extra_line = (ret_h << 8) | (ret_l & 0xff) ;
  return shutter + extra_line;
}

/******************************************************************************

******************************************************************************/
static int OV2655_set_shutter(unsigned int shutter)
{
  // write shutter, in number of line period
  int rc = 0;
  unsigned int temp;
  shutter = shutter & 0xffff;
  temp = shutter & 0xff;
  ov2655_write_i2c(0x3003, temp);
  temp = shutter >> 8;
  ov2655_write_i2c(0x3002, temp);
  return rc;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV2655_get_gain16(void)
{
  unsigned int gain16, temp;
  temp = 0;
  ov2655_read_i2c(0x3000, &temp);
  CDBG("%s:Reg(0x3000) = 0x%x\n",__func__,temp);
  gain16 = ((temp>>4) + 1) * (16 + (temp & 0x0f));
  return gain16;
}

/******************************************************************************

******************************************************************************/
static int OV2655_set_gain16(unsigned int gain16)
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
  rc = ov2655_write_i2c(0x3000,reg + 1);
  msleep(100);
  rc |= ov2655_write_i2c(0x3000,reg);
  return rc;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV2655_get_sysclk(void)
{
  // calculate sysclk
  unsigned int temp1, temp2, XVCLK;
  unsigned int Indiv2x, Bit8Div, FreqDiv2x, PllDiv, SensorDiv, ScaleDiv, DvpDiv, ClkDiv, VCO, sysclk;
  unsigned int Indiv2x_map[] = { 2, 3, 4, 6, 4, 6, 8, 12};
  unsigned int Bit8Div_map[] = { 1, 1, 4, 5};
  unsigned int FreqDiv2x_map[] = { 2, 3, 4, 6};
  unsigned int DvpDiv_map[] = { 1, 2, 8, 16};
  ov2655_read_i2c(0x300e, &temp1);
  // bit[5:0] PllDiv
  PllDiv = 64 - (temp1 & 0x3f);
  ov2655_read_i2c(0x300f, &temp1);
  // bit[2:0] Indiv
  temp2 = temp1 & 0x07;
  Indiv2x = Indiv2x_map[temp2];
  // bit[5:4] Bit8Div
  temp2 = (temp1 >> 4) & 0x03;
  Bit8Div = Bit8Div_map[temp2];
  // bit[7:6] FreqDiv
  temp2 = temp1 >> 6;
  FreqDiv2x = FreqDiv2x_map[temp2];
  ov2655_read_i2c(0x3010, &temp1);
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
  ov2655_read_i2c(0x3011, &temp1);
  // bit[5:0] ClkDiv
  temp2 = temp1 & 0x3f;
  ClkDiv = temp2 + 1;
  XVCLK = OV2655_MASTER_CLK_RATE/10000;
  CDBG("%s:XVCLK = 0x%x\n",__func__,XVCLK);
  CDBG("%s:Bit8Div = 0x%x\n",__func__,Bit8Div);
  CDBG("%s:FreqDiv2x = 0x%x\n",__func__,FreqDiv2x);
  CDBG("%s:PllDiv = 0x%x\n",__func__,PllDiv);
  CDBG("%s:Indiv2x = 0x%x\n",__func__,Indiv2x);
  VCO = XVCLK * Bit8Div * FreqDiv2x * PllDiv / Indiv2x;
  sysclk = VCO / Bit8Div / SensorDiv / ClkDiv / 4;
  CDBG("%s:ClkDiv = 0x%x\n",__func__,ClkDiv);
  CDBG("%s:SensorDiv = 0x%x\n",__func__,SensorDiv);
  CDBG("%s:sysclk = 0x%x\n",__func__,sysclk);
  return sysclk;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV2655_get_HTS(void)
{
  // read HTS from register settings
  unsigned int HTS, extra_HTS;
  unsigned int ret_l,ret_h;
  ret_l = ret_h = 0;
  ov2655_read_i2c(0x3028, &ret_h);
  ov2655_read_i2c(0x3029, &ret_l);
  HTS = (ret_h << 8) | (ret_l & 0xff) ;
  ov2655_read_i2c(0x302c, &ret_l);
  extra_HTS = ret_l;
  return HTS + extra_HTS;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV2655_get_VTS(void)
{
  // read VTS from register settings
  unsigned int VTS, extra_VTS;
  unsigned int ret_l,ret_h;
  ret_l = ret_h = 0;
  ov2655_read_i2c(0x302a, &ret_h);
  ov2655_read_i2c(0x302b, &ret_l);
  VTS = (ret_h << 8) | (ret_l & 0xff) ;
  ov2655_read_i2c(0x302d, &ret_h);
  ov2655_read_i2c(0x302e, &ret_l);
  extra_VTS = (ret_h << 8) | (ret_l & 0xff) ;
  return VTS + extra_VTS;
}

/******************************************************************************

******************************************************************************/
static int OV2655_set_VTS(unsigned int VTS)
{
  // write VTS to registers
  int rc = 0;
  unsigned int temp;
  temp = VTS & 0xff;
  rc = ov2655_write_i2c(0x302b, temp);
  temp = VTS>>8;
  rc |= ov2655_write_i2c(0x302a, temp);
  return rc;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV2655_get_binning(void)
{
  // write VTS to registers
  unsigned int temp, binning;
  ov2655_read_i2c(0x300b, &temp);
  if(temp==0x52){
    // OV2650
    binning = 2;
  } else {
    // OV2655
    binning = 1;
  }
  return binning;
}

/******************************************************************************

******************************************************************************/
static unsigned int OV2655_get_light_frequency(void)
{
  // get banding filter value
  unsigned int temp, light_frequency;
  ov2655_read_i2c(0x3014, &temp);
  if (temp & 0x40) {
    // auto
    ov2655_read_i2c(0x508e, &temp);
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

/******************************************************************************

******************************************************************************/
static int OV2655_set_bandingfilter(void)
{
  int rc = 0;
  unsigned int preview_VTS;
  unsigned int band_step60, max_band60, band_step50, max_band50;
  // read preview PCLK
  ov2655_preview_sysclk = OV2655_get_sysclk();
  // read preview HTS
  ov2655_preview_HTS = OV2655_get_HTS();
  // read preview VTS
  preview_VTS = OV2655_get_VTS();
  // calculate banding filter
  CDBG("%s:ov2655_preview_sysclk = 0x%x\n",__func__,ov2655_preview_sysclk);
  CDBG("%s:ov2655_preview_HTS = 0x%x\n",__func__,ov2655_preview_HTS);
  CDBG("%s:preview_VTS = 0x%x\n",__func__,preview_VTS);
  // 60Hz
  band_step60 = ov2655_preview_sysclk * 100/ov2655_preview_HTS * 100/120;
  rc = ov2655_write_i2c(0x3073, (band_step60 >> 8));
  rc |= ov2655_write_i2c(0x3072, (band_step60 & 0xff));
  max_band60 = ((preview_VTS-4)/band_step60);
  rc |= ov2655_write_i2c(0x301d, max_band60-1);
  // 50Hz
  CDBG("%s:band_step60 = 0x%x\n",__func__,band_step60);
  CDBG("%s:max_band60 = 0x%x\n",__func__,max_band60);
  band_step50 = ov2655_preview_sysclk * 100/ov2655_preview_HTS;
  rc |= ov2655_write_i2c(0x3071, (band_step50 >> 8));
  rc |= ov2655_write_i2c(0x3070, (band_step50 & 0xff));
  max_band50 = ((preview_VTS-4)/band_step50);
  rc |= ov2655_write_i2c(0x301c, max_band50-1);
  CDBG("%s:band_step50 = 0x%x\n",__func__,band_step50 );
  CDBG("%s:max_band50 = 0x%x\n",__func__,max_band50);
  return rc;
}

/******************************************************************************

******************************************************************************/
static int ov2655_set_nightmode(int NightMode)

{
  int rc = 0;
  unsigned int temp;
  switch (NightMode) {
    case 0:{//Off
        ov2655_read_i2c(0x3014, &temp);
        temp = temp & 0xf7;			// night mode off, bit[3] = 0
        ov2655_write_i2c(0x3014, temp);
        // clear dummy lines
        ov2655_write_i2c(0x302d, 0);
        ov2655_write_i2c(0x302e, 0);
      }
      break;
    case 1: {// On
        ov2655_read_i2c(0x3014, &temp);
        temp = temp | 0x08;			// night mode on, bit[3] = 1
        ov2655_write_i2c(0x3014, temp);
      }
      break;
    default:
      break;
  }
  return rc;
}

/******************************************************************************

******************************************************************************/
static int ov2655_get_preview_exposure_gain(void)
{
  int rc = 0;
  ov2655_preview_shutter = OV2655_get_shutter();
  // read preview gain
  ov2655_preview_gain16 = OV2655_get_gain16();
  ov2655_preview_binning = OV2655_get_binning();
  // turn off night mode for capture
  rc = ov2655_set_nightmode(0);
  return rc;
}

/******************************************************************************

******************************************************************************/
static int ov2655_set_preview_exposure_gain(void)
{
  int rc = 0;
  rc = OV2655_set_shutter(ov2655_preview_shutter);
  rc = OV2655_set_gain16(ov2655_preview_gain16);
  if(OV2655_CAMERA_WB_AUTO)
  {
	rc |= ov2655_write_i2c(0x3306, 0x00); //set to WB_AUTO
  }
  return rc;
}

/******************************************************************************

******************************************************************************/
static int ov2655_set_capture_exposure_gain(void)
{
  int rc = 0;
  unsigned int capture_shutter, capture_gain16, capture_sysclk, capture_HTS, capture_VTS;
  unsigned int light_frequency, capture_bandingfilter, capture_max_band;
  unsigned long capture_gain16_shutter;
  unsigned int temp;

  //Step3: calculate and set capture exposure and gain
  // turn off AEC, AGC
  ov2655_read_i2c(0x3013, &temp);
  temp = temp & 0xfa;
  ov2655_write_i2c(0x3013, temp);
  // read capture sysclk
  capture_sysclk = OV2655_get_sysclk();
  // read capture HTS
  capture_HTS = OV2655_get_HTS();
  // read capture VTS
  capture_VTS = OV2655_get_VTS();
  // calculate capture banding filter
  light_frequency = OV2655_get_light_frequency();
  if (light_frequency == 60) {
    // 60Hz
    capture_bandingfilter = capture_sysclk * 100 / capture_HTS * 100 / 120;
  } else {
    // 50Hz
    capture_bandingfilter = capture_sysclk * 100 / capture_HTS;
  }
  capture_max_band = ((capture_VTS-4)/capture_bandingfilter);
  // calculate capture shutter
  capture_shutter = ov2655_preview_shutter;
  // gain to shutter
  capture_gain16 = ov2655_preview_gain16 * capture_sysclk/ov2655_preview_sysclk
  * ov2655_preview_HTS/capture_HTS * ov2655_preview_binning;
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
  rc |= OV2655_set_gain16(capture_gain16);
  // write capture shutter
  if (capture_shutter > (capture_VTS - 4)) {
    capture_VTS = capture_shutter + 4;
    rc |= OV2655_set_VTS(capture_VTS);
  }
  rc |= OV2655_set_shutter(capture_shutter);
  if(OV2655_CAMERA_WB_AUTO)
  {
    rc |= ov2655_write_i2c(0x3306, 0x02);
    rc |= ov2655_write_i2c(0x3337, OV2655_preview_R_gain);
    rc |= ov2655_write_i2c(0x3338, OV2655_preview_G_gain);
    rc |= ov2655_write_i2c(0x3339, OV2655_preview_B_gain);
  }
  return rc;
}
#if 0//not used
static int ov2655_set_flash_light(enum led_brightness brightness)
{
  struct led_classdev *led_cdev;
	
  CDBG("%s brightness = %d\n",__func__ , brightness);

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
    CDBG("get flashlight device failed\n");
    return -1;
  }

  return 0;
}
#endif
static int ov2655_led_flash_auto(struct msm_camera_sensor_flash_data *fdata)
{
	//int tmp;
	CDBG("--CAMERA-- ov2655_led_flash_ctrl led_flash_mode = %d\n", led_flash_mode);
	
	//tmp = ov2655_read_i2c(0x56a1);
	//CDBG("--ov2655_led_flash_auto-- GAIN VALUE : %d\n", tmp);
	//if (tmp < 40) {
		msm_camera_flash_set_led_state(fdata, MSM_CAMERA_LED_HIGH);
	//}
	return 0;
}

int32_t ov2655_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;
	unsigned int temp = 0;

	ov2655_v4l2_ctrl = s_ctrl;

	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3012, 0x80, MSM_CAMERA_I2C_BYTE_DATA);
		msleep(5);
		msm_sensor_write_init_settings(s_ctrl);
		CDBG("AF_init: afinit = %d\n", afinit);
		if (afinit == 1) {
			rc = ov2655_af_setting(s_ctrl);
			if (rc < 0) {
				CDBG_HIGH("ov2655_af_setting failed\n");
				return rc;
			}
			afinit = 0;
		}
		csi_config = 0;
		is_first_preview = 1;
		ov2655_preview_shutter = OV2655_get_shutter();
		ov2655_preview_gain16 = OV2655_get_gain16();	
	} 
	else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		if(OV2655_CAMERA_WB_AUTO)
		{
		  rc |= ov2655_write_i2c(0x330c, 0x02);
		  ov2655_read_i2c(0x330f, &OV2655_preview_R_gain);
		  rc |= ov2655_write_i2c(0x330c, 0x03);
		  ov2655_read_i2c(0x330f, &OV2655_preview_G_gain);
		  rc |= ov2655_write_i2c(0x330c, 0x04);
		  ov2655_read_i2c(0x330f, &OV2655_preview_B_gain);
		}
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
		msleep(30);
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,NOTIFY_CSIC_CFG,s_ctrl->curr_csic_params);
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
			//ov2655_set_flash_light(LED_OFF);
			msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_OFF);
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_QTR);

			rc |= OV2655_set_bandingfilter();
			// turn on AEC, AGC
			rc |= ov2655_read_i2c(0x3013, &temp);
			temp = temp | 0x05;
			rc |= ov2655_write_i2c(0x3013, temp);
			ov2655_set_preview_exposure_gain();
			CDBG("%s, ov2655_preview_tbl_30fps is set\n",__func__);
		}else if(res==MSM_SENSOR_RES_FULL){
			CDBG("snapshot in, is_autoflash - %d\n", is_autoflash);
			if (led_flash_mode == LED_MODE_ON)msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_HIGH);
			else if(led_flash_mode == LED_MODE_AUTO)ov2655_led_flash_auto(s_ctrl->sensordata->flash_data);
			ov2655_get_preview_exposure_gain();
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_FULL);
			CDBG("%s, ov2655_capture_tbl is set\n",__func__);
			ov2655_set_capture_exposure_gain();
		}
		if (res == MSM_SENSOR_RES_4){
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,NOTIFY_PCLK_CHANGE,&vfe_clk);
		}
		msleep(100);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;

}
static struct msm_camera_i2c_reg_conf ov2655_saturation[][4] = {
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x08, 0x00},{0x3395, 0x08, 0x00},},	/* SATURATION LEVEL0*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x10, 0x00},{0x3395, 0x10, 0x00},},	/* SATURATION LEVEL1*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x18, 0x00},{0x3395, 0x18, 0x00},},	/* SATURATION LEVEL2*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x20, 0x00},{0x3395, 0x20, 0x00},},	/* SATURATION LEVEL3*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x30, 0x00},{0x3395, 0x30, 0x00},},	/* SATURATION LEVEL4*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x40, 0x00},{0x3395, 0x40, 0x00},},	/* SATURATION LEVEL5,default*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x50, 0x00},{0x3395, 0x50, 0x00},},	/* SATURATION LEVEL6*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x58, 0x00},{0x3395, 0x58, 0x00},},	/* SATURATION LEVEL7*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x60, 0x00},{0x3395, 0x60, 0x00},},	/* SATURATION LEVEL8*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x68, 0x00},{0x3395, 0x68, 0x00},},	/* SATURATION LEVEL9*/
	{{0x3301, 0x80, 0x7f}, {0x3391, 0x02, 0xfd}, {0x3394, 0x70, 0x00},{0x3395, 0x70, 0x00},},	/* SATURATION LEVEL10*/
};
static struct msm_camera_i2c_conf_array ov2655_saturation_confs[][1] = {
	{{ov2655_saturation[0], ARRAY_SIZE(ov2655_saturation[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[1], ARRAY_SIZE(ov2655_saturation[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[2], ARRAY_SIZE(ov2655_saturation[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[3], ARRAY_SIZE(ov2655_saturation[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[4], ARRAY_SIZE(ov2655_saturation[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[5], ARRAY_SIZE(ov2655_saturation[5]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[6], ARRAY_SIZE(ov2655_saturation[6]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[7], ARRAY_SIZE(ov2655_saturation[7]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[8], ARRAY_SIZE(ov2655_saturation[8]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[9], ARRAY_SIZE(ov2655_saturation[9]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_saturation[10], ARRAY_SIZE(ov2655_saturation[10]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov2655_saturation_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov2655_saturation_enum_confs = {
	.conf = &ov2655_saturation_confs[0][0],
	.conf_enum = ov2655_saturation_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_saturation_enum_map),
	.num_index = ARRAY_SIZE(ov2655_saturation_confs),
	.num_conf = ARRAY_SIZE(ov2655_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov2655_contrast[][4] = {
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x14,0x00},{0x3399,0x14,0x00},},	/* CONTRAST L0*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x14,0x00},{0x3399,0x14,0x00},}, /* CONTRAST L1*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x14,0x00},{0x3399,0x14,0x00},}, /* CONTRAST L2*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x18,0x00},{0x3399,0x18,0x00},}, /* CONTRAST L3*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x1c,0x00},{0x3399,0x1c,0x00},}, /* CONTRAST L4*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x20,0x00},{0x3399,0x20,0x00},}, /* CONTRAST L5,default*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x24,0x00},{0x3399,0x24,0x00},}, /* CONTRAST L6*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x28,0x00},{0x3399,0x28,0x00},}, /* CONTRAST L7*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x2c,0x00},{0x3399,0x2c,0x00},}, /* CONTRAST L8*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x30,0x00},{0x3399,0x30,0x00},}, /* CONTRAST L9*/
	{{0x3391,0x04,0xfb},{0x3390,0x45,0x00},{0x3398,0x30,0x00},{0x3399,0x30,0x00},}, /* CONTRAST L10*/
};

static struct msm_camera_i2c_conf_array ov2655_contrast_confs[][1] = {
	{{ov2655_contrast[0], ARRAY_SIZE(ov2655_contrast[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[1], ARRAY_SIZE(ov2655_contrast[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[2], ARRAY_SIZE(ov2655_contrast[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[3], ARRAY_SIZE(ov2655_contrast[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[4], ARRAY_SIZE(ov2655_contrast[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[5], ARRAY_SIZE(ov2655_contrast[5]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[6], ARRAY_SIZE(ov2655_contrast[6]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[7], ARRAY_SIZE(ov2655_contrast[7]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[8], ARRAY_SIZE(ov2655_contrast[8]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[9], ARRAY_SIZE(ov2655_contrast[9]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_contrast[10], ARRAY_SIZE(ov2655_contrast[10]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};


static int ov2655_contrast_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov2655_contrast_enum_confs = {
	.conf = &ov2655_contrast_confs[0][0],
	.conf_enum = ov2655_contrast_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_contrast_enum_map),
	.num_index = ARRAY_SIZE(ov2655_contrast_confs),
	.num_conf = ARRAY_SIZE(ov2655_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
static struct msm_camera_i2c_reg_conf ov2655_sharpness[][5] = {
	{{0x3306,0x00,0xf7},{0x3376, 0x01,0x00},{0x3377,0x00,0x00},{0x3378,0x10,0x00},{0x3379,0x80,0x00},},/* SHARPNESS LEVEL 0*/
	{{0x3306,0x00,0xf7},{0x3376, 0x02,0x00},{0x3377,0x00,0x00},{0x3378,0x08,0x00},{0x3379,0x80,0x00},},/* SHARPNESS LEVEL 1*/
	{{0x3306,0x00,0xf7},{0x3376, 0x04,0x00},{0x3377,0x00,0x00},{0x3378,0x04,0x00},{0x3379,0x80,0x00},},/* SHARPNESS LEVEL 2*/
	{{0x3306,0x00,0xf7},{0x3376, 0x06,0x00},{0x3377,0x00,0x00},{0x3378,0x04,0x00},{0x3379,0x80,0x00},},/* SHARPNESS LEVEL 3,default*/
	{{0x3306,0x00,0xf7},{0x3376, 0x08,0x00},{0x3377,0x00,0x00},{0x3378,0x04,0x00},{0x3379,0x80,0x00},},/* SHARPNESS LEVEL 4*/
	{{0x3306,0x00,0xf7},{0x3376, 0x0a,0x00},{0x3377,0x00,0x00},{0x3378,0x04,0x00},{0x3379,0x80,0x00},},/* SHARPNESS LEVEL 5*/
	{{0x3306,0x00,0xf7},{0x3376, 0x0c,0x00},{0x3377,0x00,0x00},{0x3378,0x04,0x00},{0x3379,0x80,0x00},},/* SHARPNESS LEVEL 6*/
};

static struct msm_camera_i2c_conf_array ov2655_sharpness_confs[][1] = {
	{{ov2655_sharpness[0], ARRAY_SIZE(ov2655_sharpness[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_sharpness[1], ARRAY_SIZE(ov2655_sharpness[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_sharpness[2], ARRAY_SIZE(ov2655_sharpness[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_sharpness[3], ARRAY_SIZE(ov2655_sharpness[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_sharpness[4], ARRAY_SIZE(ov2655_sharpness[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_sharpness[5], ARRAY_SIZE(ov2655_sharpness[5]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_sharpness[6], ARRAY_SIZE(ov2655_sharpness[6]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov2655_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
	MSM_V4L2_SHARPNESS_L6,
};

static struct msm_camera_i2c_enum_conf_array ov2655_sharpness_enum_confs = {
	.conf = &ov2655_sharpness_confs[0][0],
	.conf_enum = ov2655_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_sharpness_enum_map),
	.num_index = ARRAY_SIZE(ov2655_sharpness_confs),
	.num_conf = ARRAY_SIZE(ov2655_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov2655_exposure[][3] = {
	{{0x3018,0x98},{0x3019,0x88},{0x301a,0xd4}},//MSM_V4L2_EXPOSURE_N2
	{{0x3018,0x88},{0x3019,0x78},{0x301a,0xd4}},//MSM_V4L2_EXPOSURE_N1
	{{0x3018,0x78},{0x3019,0x68},{0x301a,0xa5}},//MSM_V4L2_EXPOSURE_D,default
	{{0x3018,0x6a},{0x3019,0x5a},{0x301a,0xd4}},//MSM_V4L2_EXPOSURE_P1
	{{0x3018,0x5a},{0x3019,0x4a},{0x301a,0xc2}},//MSM_V4L2_EXPOSURE_P2

};

static struct msm_camera_i2c_conf_array ov2655_exposure_confs[][1] = {
	{{ov2655_exposure[0], ARRAY_SIZE(ov2655_exposure[0]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_exposure[1], ARRAY_SIZE(ov2655_exposure[1]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_exposure[2], ARRAY_SIZE(ov2655_exposure[2]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_exposure[3], ARRAY_SIZE(ov2655_exposure[3]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_exposure[4], ARRAY_SIZE(ov2655_exposure[4]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov2655_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array ov2655_exposure_enum_confs = {
	.conf = &ov2655_exposure_confs[0][0],
	.conf_enum = ov2655_exposure_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_exposure_enum_map),
	.num_index = ARRAY_SIZE(ov2655_exposure_confs),
	.num_conf = ARRAY_SIZE(ov2655_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
#if 1//not support
static struct msm_camera_i2c_reg_conf ov2655_iso[][3] = {
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},   /*ISO_AUTO*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},   /*ISO_DEBLUR*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_100*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_200*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},   /*ISO_400*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_800*/
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},    /*ISO_1600*/
};


static struct msm_camera_i2c_conf_array ov2655_iso_confs[][1] = {
	{{ov2655_iso[0], ARRAY_SIZE(ov2655_iso[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_iso[1], ARRAY_SIZE(ov2655_iso[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_iso[2], ARRAY_SIZE(ov2655_iso[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_iso[3], ARRAY_SIZE(ov2655_iso[3]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_iso[4], ARRAY_SIZE(ov2655_iso[4]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_iso[5], ARRAY_SIZE(ov2655_iso[5]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov2655_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};


static struct msm_camera_i2c_enum_conf_array ov2655_iso_enum_confs = {
	.conf = &ov2655_iso_confs[0][0],
	.conf_enum = ov2655_iso_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_iso_enum_map),
	.num_index = ARRAY_SIZE(ov2655_iso_confs),
	.num_conf = ARRAY_SIZE(ov2655_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
#endif

static struct msm_camera_i2c_reg_conf ov2655_special_effect[][3] = {
	{{0x3391,0x00,0x87},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_OFF,
	{{0x3391,0x20,0x87},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_MONO,
	{{0x3391,0x40,0x87},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_NEGATIVE,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_SOLARIZE,
	{{0x3391,0x18,0x87},{0x3396,0x40,0x00},{0x3397,0xa6,0x00}},//MSM_V4L2_EFFECT_SEPIA,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_POSTERAIZE,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_WHITEBOARD,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_BLACKBOARD,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_AQUA,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_EMBOSS,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_SKETCH,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_NEON,
	{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//MSM_V4L2_EFFECT_MAX,
};

static struct msm_camera_i2c_conf_array ov2655_special_effect_confs[][1] = {
	{{ov2655_special_effect[0],  ARRAY_SIZE(ov2655_special_effect[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[1],  ARRAY_SIZE(ov2655_special_effect[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[2],  ARRAY_SIZE(ov2655_special_effect[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[3],  ARRAY_SIZE(ov2655_special_effect[3]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[4],  ARRAY_SIZE(ov2655_special_effect[4]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[5],  ARRAY_SIZE(ov2655_special_effect[5]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[6],  ARRAY_SIZE(ov2655_special_effect[6]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[7],  ARRAY_SIZE(ov2655_special_effect[7]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[8],  ARRAY_SIZE(ov2655_special_effect[8]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[9],  ARRAY_SIZE(ov2655_special_effect[9]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[10], ARRAY_SIZE(ov2655_special_effect[10]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[11], ARRAY_SIZE(ov2655_special_effect[11]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_special_effect[12], ARRAY_SIZE(ov2655_special_effect[12]), 0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};



static int ov2655_special_effect_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array ov2655_special_effect_enum_confs = {
	.conf = &ov2655_special_effect_confs[0][0],
	.conf_enum = ov2655_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_special_effect_enum_map),
	.num_index = ARRAY_SIZE(ov2655_special_effect_confs),
	.num_conf = ARRAY_SIZE(ov2655_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov2655_antibanding[][1] = {
	{{0x3014,0x00,0x3f},},   /*ANTIBANDING 60HZ*/
	{{0x3014,0x80,0x3f},},   /*ANTIBANDING 50HZ*/
	{{0x3014,0xc0,0x3f},},   /* ANTIBANDING AUTO*/
};


static struct msm_camera_i2c_conf_array ov2655_antibanding_confs[][1] = {
	{{ov2655_antibanding[0], ARRAY_SIZE(ov2655_antibanding[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_antibanding[1], ARRAY_SIZE(ov2655_antibanding[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_antibanding[2], ARRAY_SIZE(ov2655_antibanding[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov2655_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};


static struct msm_camera_i2c_enum_conf_array ov2655_antibanding_enum_confs = {
	.conf = &ov2655_antibanding_confs[0][0],
	.conf_enum = ov2655_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_antibanding_enum_map),
	.num_index = ARRAY_SIZE(ov2655_antibanding_confs),
	.num_conf = ARRAY_SIZE(ov2655_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov2655_wb_oem[][4] = {
	{{0x3306,0x00,0xfd},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_OFF,			  
	{{0x3306,0x00,0xfd},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},},//MSM_V4L2_WB_AUTO ,		  
	{{0x3306,0x02,0xfd},{0x3337,0x44,0x00},{0x3338,0x40,0x00},{0x3339,0x70,0x00},},//MSM_V4L2_WB_CUSTOM,		  
	{{0x3306,0x02,0xfd},{0x3337,0x52,0x00},{0x3338,0x40,0x00},{0x3339,0x58,0x00},},//MSM_V4L2_WB_INCANDESCENT,   
	{{0x3306,0x02,0xfd},{0x3337,0x44,0x00},{0x3338,0x40,0x00},{0x3339,0x70,0x00},},//MSM_V4L2_WB_FLUORESCENT,	  
	{{0x3306,0x02,0xfd},{0x3337,0x5e,0x00},{0x3338,0x40,0x00},{0x3339,0x46,0x00},},//MSM_V4L2_WB_DAYLIGHT, 	  
	{{0x3306,0x02,0xfd},{0x3337,0x68,0x00},{0x3338,0x40,0x00},{0x3339,0x4e,0x00},},//MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_conf_array ov2655_wb_oem_confs[][1] = {
	{{ov2655_wb_oem[0], ARRAY_SIZE(ov2655_wb_oem[0]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_wb_oem[1], ARRAY_SIZE(ov2655_wb_oem[1]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_wb_oem[2], ARRAY_SIZE(ov2655_wb_oem[2]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_wb_oem[3], ARRAY_SIZE(ov2655_wb_oem[3]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_wb_oem[4], ARRAY_SIZE(ov2655_wb_oem[4]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_wb_oem[5], ARRAY_SIZE(ov2655_wb_oem[5]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov2655_wb_oem[6], ARRAY_SIZE(ov2655_wb_oem[6]),  0,MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov2655_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array ov2655_wb_oem_enum_confs = {
	.conf = &ov2655_wb_oem_confs[0][0],
	.conf_enum = ov2655_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(ov2655_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(ov2655_wb_oem_confs),
	.num_conf = ARRAY_SIZE(ov2655_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

int ov2655_saturation_msm_sensor_s_ctrl_by_enum(
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


int ov2655_contrast_msm_sensor_s_ctrl_by_enum(
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

int ov2655_sharpness_msm_sensor_s_ctrl_by_enum(
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

int ov2655_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	effect_value = value;
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
	return rc;
}

int ov2655_wb_oem_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;

	CDBG("--CAMERA-- %s ...  effect_value = %d value = %d(End)\n", __func__,effect_value,value);
	CDBG("--CAMERA--  ...  num_conf= %d(End)\n",ctrl_info->enum_cfg_settings->num_conf);
	OV2655_CAMERA_WB_AUTO = 0;
	if(value == MSM_V4L2_WB_AUTO)
		OV2655_CAMERA_WB_AUTO = 1;
	else if(value == MSM_V4L2_WB_OFF){
		OV2655_CAMERA_WB_AUTO = 1;
	}
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	return rc;
}

int ov2655_flash_mode_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	led_flash_mode = value;
	CDBG("--CAMERA-- %s flash mode = %d\n", __func__, led_flash_mode);
	return rc;
}

int ov2655_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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

struct msm_sensor_v4l2_ctrl_info_t ov2655_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L8,
		.step = 1,
		.enum_cfg_settings = &ov2655_saturation_enum_confs,
		.s_v4l2_ctrl = ov2655_saturation_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L8,
		.step = 1,
		.enum_cfg_settings = &ov2655_contrast_enum_confs,
		.s_v4l2_ctrl =ov2655_contrast_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L6,
		.step = 1,
		.enum_cfg_settings = &ov2655_sharpness_enum_confs,
		.s_v4l2_ctrl =ov2655_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &ov2655_exposure_enum_confs,
		.s_v4l2_ctrl = ov2655_msm_sensor_s_ctrl_by_enum,
	},
#if 1//not support
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &ov2655_iso_enum_confs,
		.s_v4l2_ctrl = ov2655_msm_sensor_s_ctrl_by_enum,
	},
#endif
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_MAX,
		.step = 1,
		.enum_cfg_settings = &ov2655_special_effect_enum_confs,
		.s_v4l2_ctrl = ov2655_effect_msm_sensor_s_ctrl_by_enum, //ov2655_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &ov2655_antibanding_enum_confs,
		.s_v4l2_ctrl = ov2655_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &ov2655_wb_oem_enum_confs,
		.s_v4l2_ctrl = ov2655_wb_oem_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_LED_FLASH_MODE,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = ov2655_flash_mode_msm_sensor_s_ctrl_by_enum,
	},
};

static struct msm_sensor_fn_t ov2655_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = ov2655_write_prev_exp_gain,
	.sensor_write_snapshot_exp_gain = ov2655_write_pict_exp_gain,
	.sensor_csi_setting = ov2655_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = ov2655_sensor_power_up,
	.sensor_power_down = ov2655_sensor_power_down,
	.sensor_match_id = ov2655_sensor_match_id,
};

static struct msm_sensor_reg_t ov2655_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = ov2655_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov2655_start_settings),
	.stop_stream_conf = ov2655_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov2655_stop_settings),
	.group_hold_on_conf = ov2655_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(ov2655_groupon_settings),
	.group_hold_off_conf = ov2655_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(ov2655_groupoff_settings),
	.init_settings = &ov2655_init_conf[0],
	.init_size = ARRAY_SIZE(ov2655_init_conf),
	.mode_settings = &ov2655_confs[0],
	.output_settings = &ov2655_dimensions[0],
	.num_conf = ARRAY_SIZE(ov2655_confs),
};

static struct msm_sensor_ctrl_t ov2655_s_ctrl = {
	.msm_sensor_v4l2_ctrl_info = ov2655_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(ov2655_v4l2_ctrl_info),
	.msm_sensor_reg = &ov2655_regs,
	.sensor_i2c_client = &ov2655_sensor_i2c_client,
	.sensor_i2c_addr =  0x30<<1,
	.sensor_output_reg_addr = &ov2655_reg_addr,
	.sensor_id_info = &ov2655_id_info,
	.sensor_exp_gain_info = &ov2655_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &ov2655_csi_params_array[0],
	.msm_sensor_mutex = &ov2655_mut,
	.sensor_i2c_driver = &ov2655_i2c_driver,
	.sensor_v4l2_subdev_info = ov2655_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov2655_subdev_info),
	.sensor_v4l2_subdev_ops = &ov2655_subdev_ops,
	.func_tbl = &ov2655_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision YUV sensor driver");
MODULE_LICENSE("GPL v2");
