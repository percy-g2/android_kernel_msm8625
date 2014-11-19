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
#include "hm2056front_v4l2.h"

#define SENSOR_NAME "hm2056front"
#define PLATFORM_DRIVER_NAME "msm_camera_hm2056front"
#define hm2056front_obj hm2056front_##obj

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define HM2056FRONT_VERBOSE_DGB

#ifdef HM2056FRONT_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif
static int32_t vfe_clk = 266667000;
static struct msm_sensor_ctrl_t hm2056front_s_ctrl;
static int is_first_preview = 0;
static int effect_value = CAMERA_EFFECT_OFF;
//static int16_t hm2056front_effect = CAMERA_EFFECT_OFF;
//static unsigned int SAT_U = 0x80;
//static unsigned int SAT_V = 0x80;

DEFINE_MUTEX(hm2056front_mut);




static struct msm_camera_i2c_conf_array hm2056front_init_conf[] = {
	{&hm2056front_init_settings[0],
		ARRAY_SIZE(hm2056front_init_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array hm2056front_confs[] = {
	{&hm2056front_snap_settings[0],
		ARRAY_SIZE(hm2056front_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&hm2056front_prev_30fps_settings[0],
		ARRAY_SIZE(hm2056front_prev_30fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&hm2056front_prev_60fps_settings[0],
		ARRAY_SIZE(hm2056front_prev_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&hm2056front_prev_90fps_settings[0],
		ARRAY_SIZE(hm2056front_prev_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_csi_params hm2056front_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x14,
};

static struct v4l2_subdev_info hm2056front_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t hm2056front_dimensions[] = {
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

static struct msm_sensor_output_reg_addr_t hm2056front_reg_addr = {
//	.x_output = 0x3808,
//	.y_output = 0x380A,
//	.line_length_pclk = 0x380C,
//	.frame_length_lines = 0x380E,
};

static struct msm_camera_csi_params *hm2056front_csi_params_array[] = {
	&hm2056front_csi_params, /* Snapshot */
	&hm2056front_csi_params, /* Preview */
	//&hm2056front_csi_params, /* 60fps */
	//&hm2056front_csi_params, /* 90fps */
	//&hm2056front_csi_params, /* ZSL */
};

static struct msm_sensor_id_info_t hm2056front_id_info = {
	.sensor_id_reg_addr = 0x0001,
	.sensor_id = 0x2056,
};

static struct msm_sensor_exp_gain_info_t hm2056front_exp_gain_info = {
//	.coarse_int_time_addr = 0x3500,
//	.global_gain_addr = 0x350A,
//	.vert_offset = 4,
};



static int32_t hm2056front_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
    CDBG("%s \n",__func__);

	return 0;

}


static int32_t hm2056front_write_prev_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
						uint16_t gain, uint32_t line)
{
	return 0;
};


static const struct i2c_device_id hm2056front_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&hm2056front_s_ctrl},
	{ }
};

static int32_t hm2056front_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
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
		CDBG("hm2056front match id ok\n");
	}
	return rc;
}

extern void camera_af_software_powerdown(struct i2c_client *client);
static int32_t hm2056front_sensor_i2c_probe(struct i2c_client *client,
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

static struct i2c_driver hm2056front_i2c_driver = {
	.id_table = hm2056front_i2c_id,
	.probe  = hm2056front_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client hm2056front_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&hm2056front_i2c_driver);
}

static struct v4l2_subdev_core_ops hm2056front_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops hm2056front_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops hm2056front_subdev_ops = {
	.core = &hm2056front_subdev_core_ops,
	.video  = &hm2056front_subdev_video_ops,

};

static int32_t hm2056front_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
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

static int32_t hm2056front_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
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


static int32_t hm2056front_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;


	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		csi_config = 0;
		is_first_preview = 1;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
		msleep(30);
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
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,
				s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_QTR);
		}else if(res==MSM_SENSOR_RES_FULL){
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,
				s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_FULL);
		}
		if (res == MSM_SENSOR_RES_4){
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,NOTIFY_PCLK_CHANGE,&vfe_clk);
		}
		msleep(10);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;

}
static struct msm_camera_i2c_reg_conf hm2056front_saturation[][4] = {
	{{0x0480, 0x00, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL0*/
	{{0x0480, 0x10, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL1*/
	{{0x0480, 0x24, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL2*/
	{{0x0480, 0x38, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL3*/
	{{0x0480, 0x4c, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL4*/
	{{0x0480, 0x60, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL5*/
	{{0x0480, 0x74, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL6*/
	{{0x0480, 0x88, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL7*/
	{{0x0480, 0x9c, 0x00}, {0x0000, 0x01, 0x00}, {0x0100, 0x01, 0x00},
	{0x0101, 0x01, 0x00},},	/* SATURATION LEVEL8*/
};
static struct msm_camera_i2c_conf_array hm2056front_saturation_confs[][1] = {
	{{hm2056front_saturation[0], ARRAY_SIZE(hm2056front_saturation[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[1], ARRAY_SIZE(hm2056front_saturation[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[2], ARRAY_SIZE(hm2056front_saturation[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[3], ARRAY_SIZE(hm2056front_saturation[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[4], ARRAY_SIZE(hm2056front_saturation[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[5], ARRAY_SIZE(hm2056front_saturation[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[6], ARRAY_SIZE(hm2056front_saturation[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[7], ARRAY_SIZE(hm2056front_saturation[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_saturation[8], ARRAY_SIZE(hm2056front_saturation[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm2056front_saturation_enum_map[] = {
	MSM_V4L2_SATURATION_L0,
	MSM_V4L2_SATURATION_L1,
	MSM_V4L2_SATURATION_L2,
	MSM_V4L2_SATURATION_L3,
	MSM_V4L2_SATURATION_L4,
	MSM_V4L2_SATURATION_L5,
	MSM_V4L2_SATURATION_L6,
	MSM_V4L2_SATURATION_L7,
	MSM_V4L2_SATURATION_L8,
};

static struct msm_camera_i2c_enum_conf_array hm2056front_saturation_enum_confs = {
	.conf = &hm2056front_saturation_confs[0][0],
	.conf_enum = hm2056front_saturation_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_saturation_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_saturation_confs),
	.num_conf = ARRAY_SIZE(hm2056front_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm2056front_contrast[][4] = {
	{{0x04B0,0x10},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},	/* CONTRAST L0*/
	{{0x04B0,0x24},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L1*/
	{{0x04B0,0x38},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L2*/
	{{0x04B0,0x4C},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L3*/
	{{0x04B0,0x54},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L4*/
	{{0x04B0,0x58},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L5*/
	{{0x04B0,0x70},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L6*/
	{{0x04B0,0xa0},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L7*/
	{{0x04B0,0xb0},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L8*/
	{{0x04B0,0xc0},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L9*/
	{{0x04B0,0xff},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},		/* CONTRAST L10*/
};

static struct msm_camera_i2c_conf_array hm2056front_contrast_confs[][1] = {
	{{hm2056front_contrast[0], ARRAY_SIZE(hm2056front_contrast[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[1], ARRAY_SIZE(hm2056front_contrast[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[2], ARRAY_SIZE(hm2056front_contrast[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[3], ARRAY_SIZE(hm2056front_contrast[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[4], ARRAY_SIZE(hm2056front_contrast[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[5], ARRAY_SIZE(hm2056front_contrast[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[6], ARRAY_SIZE(hm2056front_contrast[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[7], ARRAY_SIZE(hm2056front_contrast[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[8], ARRAY_SIZE(hm2056front_contrast[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[8], ARRAY_SIZE(hm2056front_contrast[9]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_contrast[8], ARRAY_SIZE(hm2056front_contrast[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};


static int hm2056front_contrast_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array hm2056front_contrast_enum_confs = {
	.conf = &hm2056front_contrast_confs[0][0],
	.conf_enum = hm2056front_contrast_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_contrast_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_contrast_confs),
	.num_conf = ARRAY_SIZE(hm2056front_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
static struct msm_camera_i2c_reg_conf hm2056front_sharpness[][5] = {
	{{0x069C,0x00},{0x069e, 0x00},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},	    /* SHARPNESS LEVEL 0*/
	{{0x069C,0x1E},{0x069e, 0x08},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},	  /* SHARPNESS LEVEL 1*/
	{{0x069C,0x22},{0x069e, 0x10},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},	  /* SHARPNESS LEVEL 2*/
	{{0x069C,0x36},{0x069e, 0x36},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},	   /* SHARPNESS LEVEL 3*/
	{{0x069C,0x4A},{0x069e, 0x4A},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},	  /* SHARPNESS LEVEL 4*/
	{{0x069C,0x6E},{0x069e, 0x6E},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},	   /* SHARPNESS LEVEL 5*/
	{{0x069C,0xAE},{0x069e, 0xAE},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},
};

static struct msm_camera_i2c_conf_array hm2056front_sharpness_confs[][1] = {
	{{hm2056front_sharpness[0], ARRAY_SIZE(hm2056front_sharpness[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_sharpness[1], ARRAY_SIZE(hm2056front_sharpness[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_sharpness[2], ARRAY_SIZE(hm2056front_sharpness[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_sharpness[3], ARRAY_SIZE(hm2056front_sharpness[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_sharpness[4], ARRAY_SIZE(hm2056front_sharpness[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_sharpness[5], ARRAY_SIZE(hm2056front_sharpness[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_sharpness[6], ARRAY_SIZE(hm2056front_sharpness[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm2056front_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
	MSM_V4L2_SHARPNESS_L6,
};

static struct msm_camera_i2c_enum_conf_array hm2056front_sharpness_enum_confs = {
	.conf = &hm2056front_sharpness_confs[0][0],
	.conf_enum = hm2056front_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_sharpness_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_sharpness_confs),
	.num_conf = ARRAY_SIZE(hm2056front_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm2056front_exposure[][4] = {
	{{0x04C0,0xe0},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},
	{{0x04C0,0xa0},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},
	{{0x04C0,0x80},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},
	{{0x04C0,0x30},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},
	{{0x04C0,0x70},{0x0000,0x01},{0x0100,0x01},{0x0101,0x01},},

};

static struct msm_camera_i2c_conf_array hm2056front_exposure_confs[][1] = {
	{{hm2056front_exposure[0], ARRAY_SIZE(hm2056front_exposure[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_exposure[1], ARRAY_SIZE(hm2056front_exposure[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_exposure[2], ARRAY_SIZE(hm2056front_exposure[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_exposure[3], ARRAY_SIZE(hm2056front_exposure[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_exposure[4], ARRAY_SIZE(hm2056front_exposure[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm2056front_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array hm2056front_exposure_enum_confs = {
	.conf = &hm2056front_exposure_confs[0][0],
	.conf_enum = hm2056front_exposure_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_exposure_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_exposure_confs),
	.num_conf = ARRAY_SIZE(hm2056front_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm2056front_iso[][5] = {
	{ 
	//ISO Auto 
			{0x0392,0x03}, 
			{0x0393,0x7F}, 
			{0x0000,0x01}, 
			{0x0100,0x01}, 
			{0x0101,0x01}, 
	},
	{ 
	//ISO_DEBLUR
			{0x0392,0x00}, 
			{0x0393,0x40}, 
			{0x0000,0x01}, 
			{0x0100,0x01}, 
			{0x0101,0x01}, 
	},
	{ 
	//ISO 100 
			{0x0392,0x00}, 
			{0x0393,0x40}, 
			{0x0000,0x01}, 
			{0x0100,0x01}, 
			{0x0101,0x01}, 
	},
	{ 
	//ISO 200 
			{0x0392,0x00}, 
			{0x0393,0x7F}, 
			{0x0000,0x01}, 
			{0x0100,0x01}, 
			{0x0101,0x01}, 
	},
	{ 
	//ISO 400 
			{0x0392,0x01}, 
			{0x0393,0x7F}, 
			{0x0000,0x01}, 
			{0x0100,0x01}, 
			{0x0101,0x01}, 
	},
	{ 
	//ISO 800 
			{0x0392,0x02}, 
			{0x0393,0x7F}, 
			{0x0000,0x01}, 
			{0x0100,0x01}, 
			{0x0101,0x01}, 
	},
	{ 
	//ISO 1600 
			{0x0392,0x03}, 
			{0x0393,0x7F}, 
			{0x0000,0x01}, 
			{0x0100,0x01}, 
			{0x0101,0x01}, 
	},
#if 0
	{{0x0380, 0xFF,0},{-1,-1,-1},{-1,-1,-1},},   /*ISO_AUTO*/
	{{0x0380, 0xFF,0},{0x0018,0x00,0},{0x001D,0x40,0},},   /*ISO_DEBLUR*/
	{{0x0380, 0xFF,0},{0x0018,0x00,0},{0x001D,0x80,0},},    /*ISO_100*/
	{{0x0380, 0xFF,0},{0x0018,0x01,0},{0x001D,0x80,0},},    /*ISO_200*/
	{{0x0380, 0xFF,0},{0x0018,0x02,0},{0x001D,0x80,0},},   /*ISO_400*/
	{{0x0380, 0xFF,0},{0x0018,0x03,0},{0x001D,0x80,0},},    /*ISO_800*/
	{{0x0380, 0xFF,0},{0x0018,0x04,0},{0x001D,0x80,0},},    /*ISO_1600*/
#endif
};


static struct msm_camera_i2c_conf_array hm2056front_iso_confs[][1] = {
	{{hm2056front_iso[0], ARRAY_SIZE(hm2056front_iso[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_iso[1], ARRAY_SIZE(hm2056front_iso[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_iso[2], ARRAY_SIZE(hm2056front_iso[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_iso[3], ARRAY_SIZE(hm2056front_iso[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_iso[4], ARRAY_SIZE(hm2056front_iso[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_iso[5], ARRAY_SIZE(hm2056front_iso[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm2056front_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};


static struct msm_camera_i2c_enum_conf_array hm2056front_iso_enum_confs = {
	.conf = &hm2056front_iso_confs[0][0],
	.conf_enum = hm2056front_iso_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_iso_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_iso_confs),
	.num_conf = ARRAY_SIZE(hm2056front_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};



static struct msm_camera_i2c_reg_conf hm2056front_special_effect[][13] = {
	{ //MSM_V4L2_EFFECT_OFF
	{0x0005, 0x00},
	{0x0134, 0x00},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	{0x0488, 0x10},
	{0x0486, 0x00},
	{0x0487, 0xFF},
	{0x0120, 0x36},
	{0x0005, 0x01},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	},
	{//MSM_V4L2_EFFECT_MONO
	{0x0005, 0x00,0},
	{0x0000, 0x01,0},
	{0x0100, 0xFF,0},
	{0x0101, 0xFF,0},
	{0x0486, 0x80,0},
	{0x0487, 0x80,0},
	{0x0488, 0x11,0},
	{0x0120, 0x27,0},
	{0x0005, 0x01,0},
	{0x0000, 0x01,0},
	{0x0100, 0xFF,0},
	{0x0101, 0xFF,0},
	{-1,-1,-1},
	},
	{//MSM_V4L2_EFFECT_NEGATIVE
	{0x0005, 0x00},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	{0x0488, 0x12},
	{0x0486, 0x00},
	{0x0487, 0xFF},
	{0x0120, 0x36},
	{0x0134, 0x00},
	{0x0005, 0x01},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	},
	{//MSM_V4L2_EFFECT_SOLARIZE
	{0x0005, 0x00},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	{0x0488, 0x10},
	{0x0486, 0x00},
	{0x0487, 0xFF},
	{0x0120, 0x37},
	{0x0134, 0x04},
	{0x0005, 0x01},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	},
	{//MSM_V4L2_EFFECT_SEPIA
	{0x0005, 0x00,0},
	{0x0000, 0x01,0},
	{0x0100, 0xFF,0},
	{0x0101, 0xFF,0},
	{0x0486, 0x40,0},
	{0x0487, 0xA0,0},
	{0x0488, 0x11,0},
	{0x0120, 0x27,0},
	{0x0005, 0x01,0},
	{0x0000, 0x01,0},
	{0x0100, 0xFF,0},
	{0x0101, 0xFF,0},
	{-1,-1,-1},
	},
	{//MSM_V4L2_EFFECT_POSTERAIZE
	{0x0005, 0x00},
	{0x0134, 0x00},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	{0x0488, 0x10},
	{0x0486, 0x00},
	{0x0487, 0xFF},
	{0x0120, 0x36},
	{0x0005, 0x01},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	},
	{//MSM_V4L2_EFFECT_WHITEBOARD
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
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
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},
	{//MSM_V4L2_EFFECT_AQUA
	{0x0005, 0x00},
	{0x0134, 0x00},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	{0x0488, 0x11},
	{0x0486, 0x70},
	{0x0487, 0x50},
	{0x0120, 0x27},
	{0x0005, 0x01},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	},
	{//MSM_V4L2_EFFECT_EMBOSS
	{0x0005, 0x00,0},
	{0x0000, 0x01,0},
	{0x0100, 0x00,0},
	{0x0101, 0xFF,0},
	{0x0486, 0x00,0},
	{0x0487, 0xFF,0},
	{0x0488, 0x10,0},
	{0x0120, 0x37,0},
	{0x0134, 0x01,0},
	{0x0005, 0x01,0},
	{0x0000, 0x01,0},
	{0x0100, 0xFF,0},
	{0x0101, 0xFF,0},
	},
	{//MSM_V4L2_EFFECT_SKETCH
	{0x0005, 0x00,0},
	{0x0000, 0x01,0},
	{0x0100, 0xFF,0},
	{0x0101, 0xFF,0},
	{0x0486, 0x00,0},
	{0x0487, 0xFF,0},
	{0x0488, 0x10,0},
	{0x0120, 0x37,0},
	{0x0134, 0x02,0},
	{0x0005, 0x01,0},
	{0x0000, 0x01,0},
	{0x0100, 0xFF,0},
	{0x0101, 0xFF,0},
	},
	{//MSM_V4L2_EFFECT_NEON
	{0x0005, 0x00},
	{0x0134, 0x00},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	{0x0488, 0x10},
	{0x0486, 0x00},
	{0x0487, 0xFF},
	{0x0120, 0x37},
	{0x0005, 0x01},
	{0x0000, 0x01},
	{0x0100, 0xFF},
	{0x0101, 0xFF},
	},
	{//MSM_V4L2_EFFECT_MAX
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},
};

static struct msm_camera_i2c_conf_array hm2056front_special_effect_confs[][1] = {
	{{hm2056front_special_effect[0],  ARRAY_SIZE(hm2056front_special_effect[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[1],  ARRAY_SIZE(hm2056front_special_effect[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[2],  ARRAY_SIZE(hm2056front_special_effect[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[3],  ARRAY_SIZE(hm2056front_special_effect[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[4],  ARRAY_SIZE(hm2056front_special_effect[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[5],  ARRAY_SIZE(hm2056front_special_effect[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[6],  ARRAY_SIZE(hm2056front_special_effect[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[7],  ARRAY_SIZE(hm2056front_special_effect[7]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[8],  ARRAY_SIZE(hm2056front_special_effect[8]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[9],  ARRAY_SIZE(hm2056front_special_effect[9]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[10], ARRAY_SIZE(hm2056front_special_effect[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[11], ARRAY_SIZE(hm2056front_special_effect[11]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_special_effect[12], ARRAY_SIZE(hm2056front_special_effect[12]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};



static int hm2056front_special_effect_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array hm2056front_special_effect_enum_confs = {
	.conf = &hm2056front_special_effect_confs[0][0],
	.conf_enum = hm2056front_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_special_effect_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_special_effect_confs),
	.num_conf = ARRAY_SIZE(hm2056front_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm2056front_antibanding[][4] = {
	{{0x0120, 0x37}, {0x0000, 0x01},{0x0100, 0xFF}, {0x0101, 0xFF},},   /*ANTIBANDING 60HZ*/
	{{0x0120, 0x36}, {0x0000, 0x01},{0x0100, 0xFF}, {0x0101, 0xFF},},   /*ANTIBANDING 50HZ*/
	{{0x0120, 0x36}, {0x0000, 0x01},{0x0100, 0xFF}, {0x0101, 0xFF},},   /* ANTIBANDING AUTO*/
};


static struct msm_camera_i2c_conf_array hm2056front_antibanding_confs[][1] = {
	{{hm2056front_antibanding[0], ARRAY_SIZE(hm2056front_antibanding[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_antibanding[1], ARRAY_SIZE(hm2056front_antibanding[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_antibanding[2], ARRAY_SIZE(hm2056front_antibanding[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm2056front_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};


static struct msm_camera_i2c_enum_conf_array hm2056front_antibanding_enum_confs = {
	.conf = &hm2056front_antibanding_confs[0][0],
	.conf_enum = hm2056front_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_antibanding_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_antibanding_confs),
	.num_conf = ARRAY_SIZE(hm2056front_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm2056front_wb_oem[][8] = {
	{{0x380,0xff,0},
	{0x0101,0xff,0},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},/*WHITEBALNACE OFF*/
	{{0x380,0xff,0},
	{0x0101,0xff,0},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	}, /*WHITEBALNACE AUTO*/
	{{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},/*WHITEBALNACE CUSTOM*/
	{
	{0x0380, 0xFD,0},
	{0x032D, 0x00,0},
	{0x032E, 0x01,0},
	{0x032F, 0x14,0},
	{0x0330, 0x01,0},
	{0x0331, 0xD6,0},
	{0x0332, 0x01,0},
	{0x0101, 0xFF,0},
	},/*INCANDISCENT*/
	{
	{0x0380, 0xFD,0},
	{0x032D, 0x34,0},
	{0x032E, 0x01,0},
	{0x032F, 0x00,0},
	{0x0330, 0x01,0},
	{0x0331, 0x92,0},
	{0x0332, 0x01,0},
	{0x0101, 0xFF,0},
	},	/*FLOURESECT */
	{
	{0x0380, 0xFD,0},
	{0x032D, 0x60,0},
	{0x032E, 0x01,0},
	{0x032F, 0x00,0},
	{0x0330, 0x01,0},
	{0x0331, 0x20,0},
	{0x0332, 0x01,0},
	{0x0101, 0xFF,0},
	},	/*DAYLIGHT*/
	{
	{0x0380, 0xFD,0},
	{0x032D, 0x70,0},
	{0x032E, 0x01,0},
	{0x032F, 0x00,0},
	{0x0330, 0x01,0},
	{0x0331, 0x08,0},
	{0x0332, 0x01,0},
	{0x0101, 0xFF,0},
	},	/*CLOUDY*/
};

static struct msm_camera_i2c_conf_array hm2056front_wb_oem_confs[][1] = {
	{{hm2056front_wb_oem[0], ARRAY_SIZE(hm2056front_wb_oem[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_wb_oem[1], ARRAY_SIZE(hm2056front_wb_oem[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_wb_oem[2], ARRAY_SIZE(hm2056front_wb_oem[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_wb_oem[3], ARRAY_SIZE(hm2056front_wb_oem[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_wb_oem[4], ARRAY_SIZE(hm2056front_wb_oem[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_wb_oem[5], ARRAY_SIZE(hm2056front_wb_oem[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm2056front_wb_oem[6], ARRAY_SIZE(hm2056front_wb_oem[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm2056front_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array hm2056front_wb_oem_enum_confs = {
	.conf = &hm2056front_wb_oem_confs[0][0],
	.conf_enum = hm2056front_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(hm2056front_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(hm2056front_wb_oem_confs),
	.num_conf = ARRAY_SIZE(hm2056front_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

#if 0//not used
static int hm2056front_saturation_msm_sensor_s_ctrl_by_enum(
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


static int hm2056front_contrast_msm_sensor_s_ctrl_by_enum(
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

static int hm2056front_sharpness_msm_sensor_s_ctrl_by_enum(
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

static int hm2056front_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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
#endif
static int hm2056front_wb_oem_msm_sensor_s_ctrl_by_enum(
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

static int hm2056front_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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
static int hm2056front_msm_iso_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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

static struct msm_sensor_v4l2_ctrl_info_t hm2056front_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L8,
		.step = 1,
		.enum_cfg_settings = &hm2056front_saturation_enum_confs,
		.s_v4l2_ctrl = hm2056front_msm_sensor_s_ctrl_by_enum,//hm2056front_saturation_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L8,
		.step = 1,
		.enum_cfg_settings = &hm2056front_contrast_enum_confs,
		.s_v4l2_ctrl =hm2056front_msm_sensor_s_ctrl_by_enum, //hm2056front_contrast_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L6,
		.step = 1,
		.enum_cfg_settings = &hm2056front_sharpness_enum_confs,
		.s_v4l2_ctrl =hm2056front_msm_sensor_s_ctrl_by_enum, //hm2056front_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &hm2056front_exposure_enum_confs,
		.s_v4l2_ctrl = hm2056front_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &hm2056front_iso_enum_confs,
		.s_v4l2_ctrl = hm2056front_msm_iso_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_MAX,
		.step = 1,
		.enum_cfg_settings = &hm2056front_special_effect_enum_confs,
		.s_v4l2_ctrl = hm2056front_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &hm2056front_antibanding_enum_confs,
		.s_v4l2_ctrl = hm2056front_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &hm2056front_wb_oem_enum_confs,
		.s_v4l2_ctrl = hm2056front_wb_oem_msm_sensor_s_ctrl_by_enum,
	},

};

static struct msm_sensor_fn_t hm2056front_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = hm2056front_write_prev_exp_gain,
	.sensor_write_snapshot_exp_gain = hm2056front_write_pict_exp_gain,
	.sensor_csi_setting = hm2056front_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = hm2056front_sensor_power_up,
	.sensor_power_down = hm2056front_sensor_power_down,
	.sensor_match_id = hm2056front_sensor_match_id,
};

static struct msm_sensor_reg_t hm2056front_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = hm2056front_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(hm2056front_start_settings),
	.stop_stream_conf = hm2056front_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(hm2056front_stop_settings),
	.group_hold_on_conf = hm2056front_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(hm2056front_groupon_settings),
	.group_hold_off_conf = hm2056front_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(hm2056front_groupoff_settings),
	.init_settings = &hm2056front_init_conf[0],
	.init_size = ARRAY_SIZE(hm2056front_init_conf),
	.mode_settings = &hm2056front_confs[0],
	.output_settings = &hm2056front_dimensions[0],
	.num_conf = ARRAY_SIZE(hm2056front_confs),
};

static struct msm_sensor_ctrl_t hm2056front_s_ctrl = {
	.msm_sensor_v4l2_ctrl_info = hm2056front_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(hm2056front_v4l2_ctrl_info),
	.msm_sensor_reg = &hm2056front_regs,
	.sensor_i2c_client = &hm2056front_sensor_i2c_client,
	.sensor_i2c_addr =  0x24 << 1 ,
	.sensor_output_reg_addr = &hm2056front_reg_addr,
	.sensor_id_info = &hm2056front_id_info,
	.sensor_exp_gain_info = &hm2056front_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &hm2056front_csi_params_array[0],
	.msm_sensor_mutex = &hm2056front_mut,
	.sensor_i2c_driver = &hm2056front_i2c_driver,
	.sensor_v4l2_subdev_info = hm2056front_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(hm2056front_subdev_info),
	.sensor_v4l2_subdev_ops = &hm2056front_subdev_ops,
	.func_tbl = &hm2056front_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

late_initcall(msm_sensor_init_module);
MODULE_DESCRIPTION("HIMAX YUV sensor driver");
MODULE_LICENSE("GPL v2");
