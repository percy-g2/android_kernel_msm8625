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
#include "hm5065_v4l2.h"
//#include <linux/leds.h>

#define SENSOR_NAME "hm5065"
#define PLATFORM_DRIVER_NAME "msm_camera_hm5065"
#define hm5065_obj hm5065_##obj
#define AF_OPEN
#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define HM5065_VERBOSE_DGB

#ifdef HM5065_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif
//static int32_t vfe_clk = 266667000;
static struct msm_sensor_ctrl_t hm5065_s_ctrl;
static int is_first_preview = 0;
static int effect_value = CAMERA_EFFECT_OFF;
//static int16_t hm5065_effect = CAMERA_EFFECT_OFF;
static unsigned int SAT_U = 0x80;
static unsigned int SAT_V = 0x80;
static unsigned int csi_config = 0;

#define  LED_MODE_OFF 0
#define  LED_MODE_AUTO 1
#define  LED_MODE_ON 2
#define  LED_MODE_TORCH 3
static int led_flash_mode = LED_MODE_OFF;
//extern struct rw_semaphore leds_list_lock;
//extern struct list_head leds_list;
DEFINE_MUTEX(hm5065_mut);

static struct msm_camera_i2c_conf_array hm5065_init_conf[] = {
	{&hm5065_init0_settings[0],
		ARRAY_SIZE(hm5065_init0_settings),200, MSM_CAMERA_I2C_BYTE_DATA},
	{&hm5065_af_init[0],
		ARRAY_SIZE(hm5065_af_init), 100, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array hm5065_confs[] = {
	{&hm5065_snap_settings[0],
		ARRAY_SIZE(hm5065_snap_settings), 200, MSM_CAMERA_I2C_BYTE_DATA},
	{&hm5065_prev_30fps_settings[0],
		ARRAY_SIZE(hm5065_prev_30fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&hm5065_prev_60fps_settings[0],
		ARRAY_SIZE(hm5065_prev_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&hm5065_prev_90fps_settings[0],
		ARRAY_SIZE(hm5065_prev_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},

};

static struct msm_camera_csi_params hm5065_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 2,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x14,
};

static struct v4l2_subdev_info hm5065_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t hm5065_dimensions[] = {
#ifdef HM5065_FAKE_8M
	{ /* For SNAPSHOT */
		.x_output = 2592,		  /*0x0a20=2592*/
		.y_output = 1936,		  /*0x790=1936,0x0798=1944*/
		.line_length_pclk = 2592,
		.frame_length_lines = 1936,
		.vt_pixel_clk = 79000000,	//79 20121127
		.op_pixel_clk =15800000,// 15800000,
		.binning_factor = 0x0,
	},
#else
	{ /* For SNAPSHOT */
		.x_output = 2592,         /*2592*/
		.y_output = 1944,         /*1944*/
		.line_length_pclk = 2592,
		.frame_length_lines = 1944,
		.vt_pixel_clk = 79000000,
		.op_pixel_clk = 15800000,
		.binning_factor = 0x0,
	},
#endif
#if 1
	{ /* For PREVIEW */
		.x_output = 1296,         /*1296*/
		.y_output = 972,         /*972*/
		.line_length_pclk = 1296,
		.frame_length_lines = 972,
		.vt_pixel_clk = 79000000,
		.op_pixel_clk = 15800000,
		.binning_factor = 0x0,
	},
#else
	{ /* For PREVIEW */
		.x_output = 640,         /*640*/
		.y_output =480,         /*480*/
		.line_length_pclk =640,
		.frame_length_lines =480,
		.vt_pixel_clk = 56000000,	//56 20121127
		.op_pixel_clk =56000000,// 56000000,
		.binning_factor = 0x0,
	},
#endif
};


static struct msm_camera_csi_params *hm5065_csi_params_array[] = {
	&hm5065_csi_params, /* Snapshot */
	&hm5065_csi_params, /* Preview */
	&hm5065_csi_params, /* 60fps */
	&hm5065_csi_params, /* 90fps */
	&hm5065_csi_params, /* ZSL */
};

static struct msm_sensor_id_info_t hm5065_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x039e,
};


static const struct i2c_device_id hm5065_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&hm5065_s_ctrl},
	{ }
};

int32_t hm5065_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
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
		CDBG("hm5065 match id ok\n");
	}
	
	return rc;
}

extern void camera_af_software_powerdown(struct i2c_client *client);

int32_t hm5065_sensor_i2c_probe(struct i2c_client *client,
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

static struct i2c_driver hm5065_i2c_driver = {
	.id_table = hm5065_i2c_id,
	.probe  = hm5065_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client hm5065_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&hm5065_i2c_driver);
}

static struct v4l2_subdev_core_ops hm5065_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops hm5065_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops hm5065_subdev_ops = {
	.core = &hm5065_subdev_core_ops,
	.video  = &hm5065_subdev_video_ops,
};

int32_t hm5065_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *info = NULL;

	CDBG("%s IN\r\n", __func__);

	info = s_ctrl->sensordata;

	msleep(20);
	gpio_direction_output(info->sensor_pwd, 0);
	msleep(20);
	gpio_direction_output(info->sensor_reset,0);
	usleep_range(5000, 5100);
	msm_sensor_power_down(s_ctrl);
	msleep(40);
	
	if (s_ctrl->sensordata->pmic_gpio_enable){
		lcd_camera_power_onoff(0);
	}

	return 0;
}

int32_t hm5065_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
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
	gpio_direction_output(info->sensor_pwd,1);
	msleep(20);
	gpio_direction_output(info->sensor_reset, 1);
	msleep(10);
	gpio_direction_output(info->sensor_reset, 0);
	msleep(20);
	gpio_direction_output(info->sensor_reset, 1);
	msleep(30);

	return rc;
}


void hm5065_af_setting(struct msm_sensor_ctrl_t *s_ctrl)
{
#if 1
	//msleep(100);
/*	
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x331e ,0x03,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(100);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070a ,0x00,MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070c ,0x00,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(100);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070c ,0x03,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(100);
*/	
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070a ,0x03,MSM_CAMERA_I2C_BYTE_DATA);
	//msleep(50);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070b ,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(150);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070b ,0x02,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(150);
#else
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070a ,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(100);
#endif
}
#if 0//not used
static int hm5065_set_flash_light(enum led_brightness brightness)
{
	struct led_classdev *led_cdev;

	CDBG("--CAMERA-- hm5065_set_flash_light brightness = %d\n", brightness);

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

static int hm5065_led_flash_auto(struct msm_camera_sensor_flash_data *fdata)
{
	//int tmp;
	CDBG("--CAMERA-- hm5065_led_flash_ctrl led_flash_mode = %d\n", led_flash_mode);
	
	//tmp = hm5065_read_i2c(0x56a1);
	//CDBG("--hm5065_led_flash_auto-- GAIN VALUE : %d\n", tmp);
	//if (tmp < 40) {
		msm_camera_flash_set_led_state(fdata, MSM_CAMERA_LED_HIGH);
	//}
	return 0;
}

int32_t hm5065_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
#if 0//def AF_OPEN
	uint16_t af_pos_h=0;
	//int32_t af_count = 0;
	uint16_t af_pos_l=0;
	//uint16_t af_status=0;
#endif
	if (update_type != MSM_SENSOR_REG_INIT)
	{
		if (csi_config == 0 || res == 0)
		msleep(66);
		else
		msleep(266);
	}

	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		//msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,hm5065_afinit_tbl, sizeof(hm5065_afinit_tbl)/sizeof(hm5065_afinit_tbl[0]), MSM_CAMERA_I2C_BYTE_DATA);
		csi_config = 0;
		is_first_preview = 1;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		msleep(30);
		if (!csi_config) {
			s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
			msleep(30);
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_CSIC_CFG,s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(30);
			csi_config = 1;
			s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		}
		if(is_first_preview)
		{
			msleep(50);
		}
		else
		{
			msleep(10);
			is_first_preview = 0;
		}

		if (res == MSM_SENSOR_RES_QTR){
			printk("preview setting\n");
			//turn off flash when preview
			//hm5065_set_flash_light(LED_OFF);
			msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_OFF);
			s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);

			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x02,MSM_CAMERA_I2C_BYTE_DATA);
			msleep(100);
			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0009 ,0x16,MSM_CAMERA_I2C_BYTE_DATA);///0x16:for 24m mclk, 0x10:for 12m mclk
			//msleep(200);
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_QTR);
		#if 0///30 fps for video and preview
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x02,MSM_CAMERA_I2C_BYTE_DATA);
			msleep(100);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b2 ,0x50,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b3 ,0xca,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b5 ,0x01,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0030 ,0x14,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00E8 ,0x00,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00ED ,0x19,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00EE ,0x1E,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00c8 ,0x00,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00c9 ,0x1E,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00ca ,0x01,MSM_CAMERA_I2C_BYTE_DATA);	
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x01,MSM_CAMERA_I2C_BYTE_DATA);
			msleep(100);
		#else///18 fps for video and preview
			/*msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x02,MSM_CAMERA_I2C_BYTE_DATA);
			msleep(100);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b2 ,0x50,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b3 ,0xca,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b5 ,0x01,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0030 ,0x14,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00E8 ,0x00,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00ED ,0x19,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00EE ,0x1E,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00c8 ,0x00,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00c9 ,0x12,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00ca ,0x01,MSM_CAMERA_I2C_BYTE_DATA);	
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x01,MSM_CAMERA_I2C_BYTE_DATA);
			msleep(100);
			*/
		#endif
		}
		else if ((res == MSM_SENSOR_RES_2)||(res == MSM_SENSOR_RES_3))
		{//now not work


			printk("video setting\n");
			s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x02,MSM_CAMERA_I2C_BYTE_DATA);
			msleep(100);			
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,
				s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_2);
		}
		else if(res==MSM_SENSOR_RES_FULL){
			printk(" enable af \n");
			if (led_flash_mode == LED_MODE_ON)msm_camera_flash_set_led_state(s_ctrl->sensordata->flash_data, MSM_CAMERA_LED_HIGH);
			else if(led_flash_mode == LED_MODE_AUTO)hm5065_led_flash_auto(s_ctrl->sensordata->flash_data);

		#if 0//def AF_OPEN
			
			hm5065_af_setting(s_ctrl);
			//wait the af complete
			while(af_count < 10){
				rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x07ae,&af_status,MSM_CAMERA_I2C_BYTE_ADDR);//0x00:False 0x01:True
				printk("HM5065 Get AF Status: %d.af_count = %d\n", af_status,af_count);
				if(1 == af_status) break;
				af_count++;
				msleep(200);
			};

			msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x06f0,&af_pos_h,MSM_CAMERA_I2C_BYTE_ADDR);
			msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x06f1,&af_pos_l,MSM_CAMERA_I2C_BYTE_ADDR);
			printk("brant read current af pos: %02x%02x.\n", af_pos_h, af_pos_l);
		#endif
			//s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x02,MSM_CAMERA_I2C_BYTE_DATA);
			//msleep(100);		
			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0009 ,0x10,MSM_CAMERA_I2C_BYTE_DATA);
			// msleep(50);
			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b2 ,0x4e,MSM_CAMERA_I2C_BYTE_DATA);
			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x00b3 ,0xca,MSM_CAMERA_I2C_BYTE_DATA);
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,  s_ctrl->msm_sensor_reg->mode_settings, MSM_SENSOR_RES_FULL);
			//msleep(100);
		#if 0//def AF_OPEN
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070a ,00,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0734 ,af_pos_h,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0735 ,af_pos_l,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070c ,00,MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070c ,05,MSM_CAMERA_I2C_BYTE_DATA);
		#endif
		}
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(200);
		//if (res == MSM_SENSOR_RES_4)
			//v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,NOTIFY_PCLK_CHANGE,&vfe_clk);

	}
	return rc;
}
static struct msm_camera_i2c_reg_conf hm5065_saturation[][1] = {
	#if 0
	{{0x0081, 0x4, 0x00}},
	{{0x0081, 0x14, 0x00}},
	{{0x0081, 0x24, 0x00}},
	{{0x0081, 0x6e, 0x00}},
	{{0x0081, 0x78, 0x00}},
	{{0x0081, 0x88, 0x00}},
	{{0x0081, 0x94, 0x00}},
	{{0x0081, 0xe4, 0x00}},
	{{0x0081, 0xff, 0x00}},
	#else
	{{0x0081, 0x4, 0x00}},
	{{0x0081, 0x14, 0x00}},
	{{0x0081, 0x24, 0x00}},
	{{0x0081, 0x48, 0x00}},
	{{0x0081, 0x50, 0x00}},
	{{0x0081, 0x58, 0x00}},
	{{0x0081, 0x94, 0x00}},
	{{0x0081, 0xe4, 0x00}},
	{{0x0081, 0xff, 0x00}},

	
	#endif
	

};
static struct msm_camera_i2c_conf_array hm5065_saturation_confs[][1] = {
	{{hm5065_saturation[0], ARRAY_SIZE(hm5065_saturation[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[1], ARRAY_SIZE(hm5065_saturation[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[2], ARRAY_SIZE(hm5065_saturation[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[3], ARRAY_SIZE(hm5065_saturation[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[4], ARRAY_SIZE(hm5065_saturation[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[5], ARRAY_SIZE(hm5065_saturation[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[6], ARRAY_SIZE(hm5065_saturation[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[7], ARRAY_SIZE(hm5065_saturation[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_saturation[8], ARRAY_SIZE(hm5065_saturation[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm5065_saturation_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array hm5065_saturation_enum_confs = {
	.conf = &hm5065_saturation_confs[0][0],
	.conf_enum = hm5065_saturation_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_saturation_enum_map),
	.num_index = ARRAY_SIZE(hm5065_saturation_confs),
	.num_conf = ARRAY_SIZE(hm5065_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm5065_contrast[][1] = {
	#if 0
	{{0x0080, 0x4, 0x00}},
	{{0x0080, 0x14, 0x00}},
	{{0x0080, 0x24, 0x00}},
	{{0x0080, 0x6e, 0x00}},
	{{0x0080, 0x78, 0x00}},
	{{0x0080, 0x88, 0x00}},
	{{0x0080, 0x94, 0x00}},
	{{0x0080, 0xe4, 0x00}},
	{{0x0080, 0xff, 0x00}},
	#else
	{{0x0080, 0x4, 0x00}},
	{{0x0080, 0x14, 0x00}},
	{{0x0080, 0x24, 0x00}},
	{{0x0080, 0x5c, 0x00}},
	{{0x0080, 0x64, 0x00}},
	{{0x0080, 0x6c, 0x00}},//6c
	{{0x0080, 0x94, 0x00}},
	{{0x0080, 0xe4, 0x00}},
	{{0x0080, 0xff, 0x00}},
	#endif
};

static struct msm_camera_i2c_conf_array hm5065_contrast_confs[][1] = {
	{{hm5065_contrast[0], ARRAY_SIZE(hm5065_contrast[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[1], ARRAY_SIZE(hm5065_contrast[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[2], ARRAY_SIZE(hm5065_contrast[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[3], ARRAY_SIZE(hm5065_contrast[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[4], ARRAY_SIZE(hm5065_contrast[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[5], ARRAY_SIZE(hm5065_contrast[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[6], ARRAY_SIZE(hm5065_contrast[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[7], ARRAY_SIZE(hm5065_contrast[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_contrast[8], ARRAY_SIZE(hm5065_contrast[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};


static int hm5065_contrast_enum_map[] = {
	MSM_V4L2_CONTRAST_L0,
	MSM_V4L2_CONTRAST_L1,
	MSM_V4L2_CONTRAST_L2,
	MSM_V4L2_CONTRAST_L3,
	MSM_V4L2_CONTRAST_L4,
	MSM_V4L2_CONTRAST_L5,
	MSM_V4L2_CONTRAST_L6,
	MSM_V4L2_CONTRAST_L7,
	MSM_V4L2_CONTRAST_L8,
};

static struct msm_camera_i2c_enum_conf_array hm5065_contrast_enum_confs = {
	.conf = &hm5065_contrast_confs[0][0],
	.conf_enum = hm5065_contrast_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_contrast_enum_map),
	.num_index = ARRAY_SIZE(hm5065_contrast_confs),
	.num_conf = ARRAY_SIZE(hm5065_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
static struct msm_camera_i2c_reg_conf hm5065_sharpness[][1] = {
	{{0X004C,0X00}},
	{{0X004C,0X04}},
	{{0X004C,0X08}},
	{{0X004C,0X18}},
	{{0X004C,0X50}},
	{{0X004C,0X66}},
	{{0X004C,0XFF}},
};

static struct msm_camera_i2c_conf_array hm5065_sharpness_confs[][1] = {
	{{hm5065_sharpness[0], ARRAY_SIZE(hm5065_sharpness[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_sharpness[1], ARRAY_SIZE(hm5065_sharpness[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_sharpness[2], ARRAY_SIZE(hm5065_sharpness[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_sharpness[3], ARRAY_SIZE(hm5065_sharpness[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_sharpness[4], ARRAY_SIZE(hm5065_sharpness[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_sharpness[5], ARRAY_SIZE(hm5065_sharpness[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_sharpness[6], ARRAY_SIZE(hm5065_sharpness[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm5065_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
	MSM_V4L2_SHARPNESS_L6,
};

static struct msm_camera_i2c_enum_conf_array hm5065_sharpness_enum_confs = {
	.conf = &hm5065_sharpness_confs[0][0],
	.conf_enum = hm5065_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_sharpness_enum_map),
	.num_index = ARRAY_SIZE(hm5065_sharpness_confs),
	.num_conf = ARRAY_SIZE(hm5065_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm5065_exposure[][1] = {
	{{0X0130,0XF9},},
	{{0X0130,0XFD},},
	{{0X0130,0X00},},
	{{0X0130,0X03},},
	{{0X0130,0X07},},

};

static struct msm_camera_i2c_conf_array hm5065_exposure_confs[][1] = {
	{{hm5065_exposure[0], ARRAY_SIZE(hm5065_exposure[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_exposure[1], ARRAY_SIZE(hm5065_exposure[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_exposure[2], ARRAY_SIZE(hm5065_exposure[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_exposure[3], ARRAY_SIZE(hm5065_exposure[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_exposure[4], ARRAY_SIZE(hm5065_exposure[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm5065_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array hm5065_exposure_enum_confs = {
	.conf = &hm5065_exposure_confs[0][0],
	.conf_enum = hm5065_exposure_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_exposure_enum_map),
	.num_index = ARRAY_SIZE(hm5065_exposure_confs),
	.num_conf = ARRAY_SIZE(hm5065_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm5065_iso[][8] = {
	{//AUTO
	{0x015c,0x3e,}, //4
	{0x015d,0x00,}, //4
	{0x015e,0x40,}, //4
	{0x015f,0x00,}, //4
	{0x02c0,0x00,},
	{0x02c1,0x00,},
	{0x02c2,0x00,},
	{0x02c3,0x80,},
	},
	{//DEFAULT
	{0x015c,0x3e,}, //4
	{0x015d,0x00,}, //4
	{0x015e,0x41,}, //4
	{0x015f,0x00,}, //4
	{0x02c0,0x00,},
	{0x02c1,0x00,},
	{0x02c2,0x00,},
	{0x02c3,0xe0,},
	},
	{//100
	{0x015c,0x3e,}, //4
	{0x015d,0x00,}, //4
	{0x015e,0x41,}, //4
	{0x015f,0x00,}, //4
	{0x02c0,0x00,},
	{0x02c1,0x00,},
	{0x02c2,0x00,},
	{0x02c3,0xe0,},
	},
	{//200
	{0x015c,0x3e,}, //4
	{0x015d,0x00,}, //4
	{0x015e,0x41,}, //4
	{0x015f,0x00,}, //4
	{0x02c0,0x00,},
	{0x02c1,0x80,},
	{0x02c2,0x00,},
	{0x02c3,0xe0,},
	},
	{//400
	{0x015c,0x3e,}, //4
	{0x015d,0x00,}, //4
	{0x015e,0x41,}, //4
	{0x015f,0x00,}, //4
	{0x02c0,0x00,},
	{0x02c1,0xc0,},
	{0x02c2,0x00,},
	{0x02c3,0xe0,},
	},
	{//800
	{0x015c,0x3e,}, //4
	{0x015d,0x00,}, //4
	{0x015e,0x4e,}, //4
	{0x015f,0x00,}, //4
	{0x02c0,0x00,},
	{0x02c1,0xe0,},
	{0x02c2,0x00,},
	{0x02c3,0xe0,},
	},
	{//1600
	{0x015c,0x40,}, //4
	{0x015d,0x00,}, //4
	{0x015e,0x41,}, //4
	{0x015f,0x00,}, //4
	{0x02c0,0x00,},
	{0x02c1,0xe0,},
	{0x02c2,0x00,},
	{0x02c3,0xe0,},
	},
};


static struct msm_camera_i2c_conf_array hm5065_iso_confs[][1] = {
	{{hm5065_iso[0], ARRAY_SIZE(hm5065_iso[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_iso[1], ARRAY_SIZE(hm5065_iso[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_iso[2], ARRAY_SIZE(hm5065_iso[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_iso[3], ARRAY_SIZE(hm5065_iso[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_iso[4], ARRAY_SIZE(hm5065_iso[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_iso[5], ARRAY_SIZE(hm5065_iso[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm5065_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};


static struct msm_camera_i2c_enum_conf_array hm5065_iso_enum_confs = {
	.conf = &hm5065_iso_confs[0][0],
	.conf_enum = hm5065_iso_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_iso_enum_map),
	.num_index = ARRAY_SIZE(hm5065_iso_confs),
	.num_conf = ARRAY_SIZE(hm5065_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};



static struct msm_camera_i2c_reg_conf hm5065_special_effect[][4] = {
	{ //MSM_V4L2_EFFECT_OFF
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x00,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_MONO
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x08,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_NEGATIVE
	{0x0380,0x01,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x00,}, //ColorEffect

	},
	{//MSM_V4L2_EFFECT_SOLARIZE
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x01,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x00,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_SEPIA
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x06,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_POSTERAIZE
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x00,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_WHITEBOARD
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x04,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_BLACKBOARD
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x05,}, //ColorEffect  7
	},
	{//MSM_V4L2_EFFECT_AQUA
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x08,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_EMBOSS
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x00,}, //ColorEffect

	},
	{//MSM_V4L2_EFFECT_SKETCH
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x01,}, //DisSketchEffect
	{0x0384,0x00,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_NEON
	{0x0380,0x00,}, //DisNegativeEffect
	{0x0381,0x00,}, //DisSolarizingEffect
	{0x0382,0x00,}, //DisSketchEffect
	{0x0384,0x00,}, //ColorEffect
	},
	{//MSM_V4L2_EFFECT_MAX
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	{-1,-1,-1},
	},
};

static struct msm_camera_i2c_conf_array hm5065_special_effect_confs[][1] = {
	{{hm5065_special_effect[0],  ARRAY_SIZE(hm5065_special_effect[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[1],  ARRAY_SIZE(hm5065_special_effect[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[2],  ARRAY_SIZE(hm5065_special_effect[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[3],  ARRAY_SIZE(hm5065_special_effect[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[4],  ARRAY_SIZE(hm5065_special_effect[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[5],  ARRAY_SIZE(hm5065_special_effect[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[6],  ARRAY_SIZE(hm5065_special_effect[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[7],  ARRAY_SIZE(hm5065_special_effect[7]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[8],  ARRAY_SIZE(hm5065_special_effect[8]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[9],  ARRAY_SIZE(hm5065_special_effect[9]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[10], ARRAY_SIZE(hm5065_special_effect[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[11], ARRAY_SIZE(hm5065_special_effect[11]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_special_effect[12], ARRAY_SIZE(hm5065_special_effect[12]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};



static int hm5065_special_effect_enum_map[] = {
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

static struct msm_camera_i2c_enum_conf_array hm5065_special_effect_enum_confs = {
	.conf = &hm5065_special_effect_confs[0][0],
	.conf_enum = hm5065_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_special_effect_enum_map),
	.num_index = ARRAY_SIZE(hm5065_special_effect_confs),
	.num_conf = ARRAY_SIZE(hm5065_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm5065_antibanding[][2] = {
	{{0x019C, 0x4B},  {0x019D, 0xc0},},   /*ANTIBANDING 60HZ*/
	{{0x019C, 0x4B},  {0x019D, 0x20},},   /*ANTIBANDING 50HZ*/
	{{0x019C, 0x4B},  {0x019D, 0x20},},   /* ANTIBANDING AUTO*/
};


static struct msm_camera_i2c_conf_array hm5065_antibanding_confs[][1] = {
	{{hm5065_antibanding[0], ARRAY_SIZE(hm5065_antibanding[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_antibanding[1], ARRAY_SIZE(hm5065_antibanding[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_antibanding[2], ARRAY_SIZE(hm5065_antibanding[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm5065_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};


static struct msm_camera_i2c_enum_conf_array hm5065_antibanding_enum_confs = {
	.conf = &hm5065_antibanding_confs[0][0],
	.conf_enum = hm5065_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_antibanding_enum_map),
	.num_index = ARRAY_SIZE(hm5065_antibanding_confs),
	.num_conf = ARRAY_SIZE(hm5065_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hm5065_wb_oem[][4] ={
	{{0X01A0,0X01},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//OFF
	{{0X01A0,0X01},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//AUTO
	{{0X01A0,0X01},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}},//CUSTOM
	{{0X01A0,0X03},{0x01a1,0xb0},{0x1a2,0x40},{0x1a3,0x80}},//INCANDESCENT
	{{0X01A0,0X03},{0x01a1,0xa0},{0x1a2,0x40},{0x1a3,0x30}},//FLUORESCENT
	{{0X01A0,0X03},{0x01a1,0xc0},{0x1a2,0x40},{0x1a3,0x00}},//DAYLIGHT,
	{{0X01A0,0X03},{0x01a1,0xff},{0x1a2,0x40},{0x1a3,0x00}},//CLOUDY_DAYLIGHT

};

static struct msm_camera_i2c_conf_array hm5065_wb_oem_confs[][1] = {
	{{hm5065_wb_oem[0], ARRAY_SIZE(hm5065_wb_oem[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_wb_oem[1], ARRAY_SIZE(hm5065_wb_oem[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_wb_oem[2], ARRAY_SIZE(hm5065_wb_oem[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_wb_oem[3], ARRAY_SIZE(hm5065_wb_oem[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_wb_oem[4], ARRAY_SIZE(hm5065_wb_oem[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_wb_oem[5], ARRAY_SIZE(hm5065_wb_oem[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hm5065_wb_oem[6], ARRAY_SIZE(hm5065_wb_oem[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hm5065_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array hm5065_wb_oem_enum_confs = {
	.conf = &hm5065_wb_oem_confs[0][0],
	.conf_enum = hm5065_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(hm5065_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(hm5065_wb_oem_confs),
	.num_conf = ARRAY_SIZE(hm5065_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};


int hm5065_saturation_msm_sensor_s_ctrl_by_enum(
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


int hm5065_contrast_msm_sensor_s_ctrl_by_enum(
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

int hm5065_sharpness_msm_sensor_s_ctrl_by_enum(
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
int hm5065_flash_mode_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	led_flash_mode = value;
	CDBG("--CAMERA-- %s flash mode = %d\n", __func__, led_flash_mode);
	return rc;
}

int hm5065_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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

int hm5065_wb_oem_msm_sensor_s_ctrl_by_enum(
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

int hm5065_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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
int hm5065_msm_iso_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
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
int hm5065_auto_focus_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{


/*
	int rc = 0;
#ifdef AF_OPEN
	uint16_t af_pos_h=0;
	int32_t af_count = 0;
	uint16_t af_pos_l=0;
	uint16_t af_status=0;
#endif
#ifdef AF_OPEN
	hm5065_af_setting(s_ctrl);
	//wait the af complete
	while(af_count < 10){
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x07ae,&af_status,MSM_CAMERA_I2C_BYTE_ADDR);//0x00:False 0x01:True
		printk("HM5065 Get AF Status: %d.af_count = %d\n", af_status,af_count);
		if(1 == af_status) break;
		af_count++;
		msleep(200);
	};
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x06f0,&af_pos_h,MSM_CAMERA_I2C_BYTE_ADDR);
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x06f1,&af_pos_l,MSM_CAMERA_I2C_BYTE_ADDR);
	printk("brant read current af pos: %02x%02x.\n", af_pos_h, af_pos_l);
#endif
	//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0010 ,0x02,MSM_CAMERA_I2C_BYTE_DATA);
	//msleep(200);
#ifdef AF_OPEN
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070a ,00,MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0734 ,af_pos_h,MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0735 ,af_pos_l,MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070c ,00,MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070c ,05,MSM_CAMERA_I2C_BYTE_DATA);
#endif
	CDBG("--CAMERA-- %s rc = %d(End...)\n", __func__, rc);
	return rc;
	*/
int rc = 0;
#ifdef AF_OPEN
	//	uint16_t af_pos_h=0;
		int32_t af_count = 0;
	//	uint16_t af_pos_l=0;
		uint16_t af_status=0;
#endif
#ifdef AF_OPEN
		hm5065_af_setting(s_ctrl);
		//wait the af complete
		while(af_count < 10){
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x07ae,&af_status,MSM_CAMERA_I2C_BYTE_ADDR);//0x00:False 0x01:True
			if(1 == af_status) break;
			af_count++;
			msleep(200);
		};
		printk("HM5065 Get AF Status: %d.af_count = %d\n", af_status,af_count);
#endif		
//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x070a ,0x01,MSM_CAMERA_I2C_BYTE_DATA);

return rc;


}

struct msm_sensor_v4l2_ctrl_info_t hm5065_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L8,
		.step = 1,
		.enum_cfg_settings = &hm5065_saturation_enum_confs,
		.s_v4l2_ctrl = hm5065_msm_sensor_s_ctrl_by_enum,//hm5065_saturation_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L8,
		.step = 1,
		.enum_cfg_settings = &hm5065_contrast_enum_confs,
		.s_v4l2_ctrl =hm5065_msm_sensor_s_ctrl_by_enum, //hm5065_contrast_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L6,
		.step = 1,
		.enum_cfg_settings = &hm5065_sharpness_enum_confs,
		.s_v4l2_ctrl =hm5065_msm_sensor_s_ctrl_by_enum, //hm5065_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &hm5065_exposure_enum_confs,
		.s_v4l2_ctrl = hm5065_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &hm5065_iso_enum_confs,
		.s_v4l2_ctrl = hm5065_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_MAX,
		.step = 1,
		.enum_cfg_settings = &hm5065_special_effect_enum_confs,
		.s_v4l2_ctrl = hm5065_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &hm5065_antibanding_enum_confs,
		.s_v4l2_ctrl = hm5065_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &hm5065_wb_oem_enum_confs,
		.s_v4l2_ctrl = hm5065_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_AUTO_FOCUS,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = hm5065_auto_focus_msm_sensor_s_ctrl_by_enum,
	},			
	{
		.ctrl_id = V4L2_CID_LED_FLASH_MODE,
		.min = 0,
		.max = 0,
		.step = 0,
		.enum_cfg_settings = NULL,
		.s_v4l2_ctrl = hm5065_flash_mode_msm_sensor_s_ctrl_by_enum,
	},
};


static struct msm_sensor_fn_t hm5065_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_csi_setting = hm5065_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = hm5065_sensor_power_up,
	.sensor_power_down = hm5065_sensor_power_down,
	.sensor_match_id = hm5065_sensor_match_id,
};

static struct msm_sensor_reg_t hm5065_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = hm5065_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(hm5065_start_settings),
	.stop_stream_conf = hm5065_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(hm5065_stop_settings),
	.group_hold_on_conf = hm5065_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(hm5065_groupon_settings),
	.group_hold_off_conf = hm5065_groupoff_settings,
	.group_hold_off_conf_size =ARRAY_SIZE(hm5065_groupoff_settings),
	.init_settings = &hm5065_init_conf[0],
	.init_size = ARRAY_SIZE(hm5065_init_conf),
	.mode_settings = &hm5065_confs[0],
	.output_settings = &hm5065_dimensions[0],
	.num_conf = ARRAY_SIZE(hm5065_confs),
};

static struct msm_sensor_ctrl_t hm5065_s_ctrl = {
	.msm_sensor_v4l2_ctrl_info = hm5065_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(hm5065_v4l2_ctrl_info),
	.msm_sensor_reg = &hm5065_regs,
	.sensor_i2c_client = &hm5065_sensor_i2c_client,
	.sensor_i2c_addr =  0x1f << 1 ,
	.sensor_id_info = &hm5065_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &hm5065_csi_params_array[0],
	.msm_sensor_mutex = &hm5065_mut,
	.sensor_i2c_driver = &hm5065_i2c_driver,
	.sensor_v4l2_subdev_info = hm5065_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(hm5065_subdev_info),
	.sensor_v4l2_subdev_ops = &hm5065_subdev_ops,
	.func_tbl = &hm5065_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_12HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("HIMAX YUV sensor driver");
MODULE_LICENSE("GPL v2");
