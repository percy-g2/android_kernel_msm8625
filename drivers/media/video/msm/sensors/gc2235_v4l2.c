/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "gc2235_v4l2.h"
#include "msm_sensor.h"
#include "msm.h"
#define SENSOR_NAME "gc2235"
#define PLATFORM_DRIVER_NAME "msm_camera_gc2235"
#define gc2235_obj gc2235_##obj

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define GC2235_VERBOSE_DGB

#ifdef GC2235_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif

DEFINE_MUTEX(gc2235_mut);
static struct msm_sensor_ctrl_t gc2235_s_ctrl;

static struct v4l2_subdev_info gc2235_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array gc2235_init_conf[] = {
	{&gc2235_init_settings[0],
	ARRAY_SIZE(gc2235_init_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array gc2235_confs[] = {
	{&gc2235_snap_settings[0],
	ARRAY_SIZE(gc2235_snap_settings), 50, MSM_CAMERA_I2C_BYTE_DATA},
	{&gc2235_prev_settings[0],
	ARRAY_SIZE(gc2235_prev_settings), 300, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t gc2235_dimensions[] = {
	{ /* For SNAPSHOT */
		.x_output = 0x640,  /*1600*/  /*for 2Mp*/
		.y_output = 0x4B0,  /*1200*/
		.line_length_pclk = 0x41a,
		.frame_length_lines = 667,
		.vt_pixel_clk = 21000000,
		.op_pixel_clk = 21000000,
		.binning_factor = 1,
	},
	{ /* For PREVIEW 30fps*/
		.x_output = 0x640,  /*1600*/  /*for 2Mp*/
		.y_output = 0x4B0,  /*1200*/
		.line_length_pclk = 0x41a,
		.frame_length_lines = 667,
		.vt_pixel_clk = 21000000,
		.op_pixel_clk = 21000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csi_params gc2235_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x14,
};

static struct msm_camera_csi_params *gc2235_csi_params_array[] = {
	&gc2235_csi_params,
	&gc2235_csi_params,
};

static struct msm_sensor_output_reg_addr_t gc2235_reg_addr = {
	.x_output = 0xff,
	.y_output = 0xff,
	.line_length_pclk = 0xff,
	.frame_length_lines = 0xff,
};

static struct msm_sensor_id_info_t gc2235_id_info = {
	.sensor_id_reg_addr = 0xf0,
	.sensor_id = 0x2235,
};

static struct msm_sensor_exp_gain_info_t gc2235_exp_gain_info = {
	.coarse_int_time_addr = 0x03,
	.global_gain_addr = 0xb1,
	.vert_offset = 4,
};

static const struct i2c_device_id gc2235_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&gc2235_s_ctrl},
	{}
};

int32_t gc2235_sensor_i2c_probe(struct i2c_client *client,
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

	usleep_range(5000, 5100);

	return rc;
}

static struct i2c_driver gc2235_i2c_driver = {
	.id_table = gc2235_i2c_id,
	.probe  = gc2235_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client gc2235_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	int rc = 0;
	CDBG("GC2235\n");

	rc = i2c_add_driver(&gc2235_i2c_driver);

	return rc;
}

static struct v4l2_subdev_core_ops gc2235_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops gc2235_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops gc2235_subdev_ops = {
	.core = &gc2235_subdev_core_ops,
	.video  = &gc2235_subdev_video_ops,
};

int32_t gc2235_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = NULL;

	CDBG("%s: %d\n", __func__, __LINE__);

	info = s_ctrl->sensordata;

	CDBG("%s, sensor_pwd:%d, sensor_reset:%d\r\n",__func__, info->sensor_pwd, info->sensor_reset);

	gpio_direction_output(info->sensor_pwd, 1);
	//if (info->sensor_reset_enable){
		gpio_direction_output(info->sensor_reset, 0);
	//}
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
	//if (info->sensor_reset_enable){
		gpio_direction_output(info->sensor_reset, 1);
	//}
	msleep(25);

	return rc;
}

int32_t gc2235_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = NULL;

	CDBG("%s IN\r\n", __func__);
	info = s_ctrl->sensordata;

	msm_sensor_stop_stream(s_ctrl);
	msleep(40); // 20

	gpio_direction_output(info->sensor_pwd, 1);
	usleep_range(5000, 5100);

	rc = msm_sensor_power_down(s_ctrl);
	msleep(40);
	if (s_ctrl->sensordata->pmic_gpio_enable){
		lcd_camera_power_onoff(0);
	}

	return rc;
}


static int32_t gc2235_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
  uint16_t gain, uint32_t line)
{
  int rc = 0;
  uint16_t gain_reg1,gain_reg2;
  unsigned int  intg_time_msb, intg_time_lsb;
  CDBG("gc2235_write_pict_exp_gain,gain=%d, line=%d\n",gain,line);
  intg_time_msb = (unsigned int ) ((line & 0x1F00) >> 8);
  intg_time_lsb = (unsigned int ) (line& 0x00FF);
  if(gain<0xff)
  {
	gain_reg1 = gain;
	gain_reg2 = 0x40;
  }
  else
  {
	gain_reg1 = 0xff;
	gain_reg2 = gain/4;
  }
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0xb1,(gain_reg1),MSM_CAMERA_I2C_BYTE_DATA);
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0xb0,(gain_reg2),MSM_CAMERA_I2C_BYTE_DATA);
    //CDBG("gc2235_write_pict_exp_gain,gain_reg1=%d, gain_reg2=%d\n",gain_reg1,gain_reg2);

  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0x03,(intg_time_msb),MSM_CAMERA_I2C_BYTE_DATA);
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0x04,(intg_time_lsb),MSM_CAMERA_I2C_BYTE_DATA);
  return rc;
}
static int32_t gc2235_write_prev_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
  uint16_t gain, uint32_t line)
{
  int rc = 0;
  uint16_t gain_reg1,gain_reg2;
  unsigned int  intg_time_msb, intg_time_lsb;
  CDBG("gc2235_write_prev_exp_gain,gain=%d, line=%d\n",gain,line);
  intg_time_msb = (unsigned int ) ((line & 0x1F00) >> 8);
  intg_time_lsb = (unsigned int ) (line& 0x00FF);
  if(gain<0xff)
  {
	gain_reg1 = gain;
	gain_reg2 = 0x40;
  }
  else
  {
	gain_reg1 = 0xff;
	gain_reg2 = gain/4;
  }
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0xb1,(gain_reg1),MSM_CAMERA_I2C_BYTE_DATA);
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0xb0,(gain_reg2),MSM_CAMERA_I2C_BYTE_DATA);
   // CDBG("gc2235_write_prev_exp_gain,gain_reg1=%d, gain_reg2=%d\n",gain_reg1,gain_reg2);

  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0x03,(intg_time_msb),MSM_CAMERA_I2C_BYTE_DATA);
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
    0x04,(intg_time_lsb),MSM_CAMERA_I2C_BYTE_DATA);
  return rc;
}

int32_t gc2235_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
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

		//if (res == MSM_SENSOR_RES_FULL)
			//gc2235_set_shutter(s_ctrl);

		msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE,
			&s_ctrl->sensordata->pdata->ioclk.vfe_clk_rate);

		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;
}

static struct msm_sensor_fn_t gc2235_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = gc2235_write_prev_exp_gain,
	.sensor_write_snapshot_exp_gain = gc2235_write_pict_exp_gain,
	.sensor_csi_setting = gc2235_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = gc2235_sensor_power_up,
	.sensor_power_down = gc2235_sensor_power_down,
};

static struct msm_sensor_reg_t gc2235_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = gc2235_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(gc2235_start_settings),
	.stop_stream_conf = gc2235_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(gc2235_stop_settings),
	.init_settings = &gc2235_init_conf[0],
	.init_size = ARRAY_SIZE(gc2235_init_conf),
	.mode_settings = &gc2235_confs[0],
	.output_settings = &gc2235_dimensions[0],
	.num_conf = ARRAY_SIZE(gc2235_confs),
};

static struct msm_sensor_ctrl_t gc2235_s_ctrl = {
	.msm_sensor_reg = &gc2235_regs,
	.sensor_i2c_client = &gc2235_sensor_i2c_client,
	.sensor_i2c_addr = 0x78,
	.sensor_output_reg_addr = &gc2235_reg_addr,
	.sensor_id_info = &gc2235_id_info,
	.sensor_exp_gain_info = &gc2235_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &gc2235_csi_params_array[0],
	.msm_sensor_mutex = &gc2235_mut,
	.sensor_i2c_driver = &gc2235_i2c_driver,
	.sensor_v4l2_subdev_info = gc2235_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(gc2235_subdev_info),
	.sensor_v4l2_subdev_ops = &gc2235_subdev_ops,
	.func_tbl = &gc2235_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Galaxycore 2M RAW sensor driver");
MODULE_LICENSE("GPL v2");
