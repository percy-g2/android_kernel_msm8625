/* 
 * drivers/input/touchscreen/ds_ts.c
 *
 * FocalTech ds TouchScreen driver. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
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
 * VERSION      	DATE			AUTHOR
 *    1.0		  2010-01-05			WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/syscalls.h>
#include <linux/gpio.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <asm/uaccess.h>
#include "goodix_touch.h"

extern void msm_tp_set_found_flag(int flag);
extern int msm_tp_get_found_flag(void);
//#define TS_DEBUG

#ifdef TS_DEBUG
#define TSDBG(x) printk x
#else
#define TSDBG(x)
#endif

#ifdef CONFIG_USE_HIRES_LCD
#define SCREEN_MAX_X    600
#define SCREEN_MAX_Y    1024
#else
#define SCREEN_MAX_X    480
#define SCREEN_MAX_Y    800
#endif

#define PRESS_MAX        255

#define DS_NAME 	"dingsheng_ts"


enum ds_ts_regs {
	DS_REG_THGROUP					= 0x80,
	DS_REG_THPEAK						= 0x81,
//	DS_REG_THCAL						= 0x82,
//	DS_REG_THWATER					= 0x83,
//	DS_REG_THTEMP					= 0x84,
//	DS_REG_THDIFF						= 0x85,				
//	DS_REG_CTRL						= 0x86,
	DS_REG_TIMEENTERMONITOR			= 0x87,
	DS_REG_PERIODACTIVE				= 0x88,
	DS_REG_PERIODMONITOR			= 0x89,
//	DS_REG_HEIGHT_B					= 0x8a,
//	DS_REG_MAX_FRAME					= 0x8b,
//	DS_REG_DIST_MOVE					= 0x8c,
//	DS_REG_DIST_POINT				= 0x8d,
//	DS_REG_FEG_FRAME					= 0x8e,
//	DS_REG_SINGLE_CLICK_OFFSET		= 0x8f,
//	DS_REG_DOUBLE_CLICK_TIME_MIN	= 0x90,
//	DS_REG_SINGLE_CLICK_TIME			= 0x91,
//	DS_REG_LEFT_RIGHT_OFFSET		= 0x92,
//	DS_REG_UP_DOWN_OFFSET			= 0x93,
//	DS_REG_DISTANCE_LEFT_RIGHT		= 0x94,
//	DS_REG_DISTANCE_UP_DOWN		= 0x95,
//	DS_REG_ZOOM_DIS_SQR				= 0x96,
//	DS_REG_RADIAN_VALUE				=0x97,
//	DS_REG_MAX_X_HIGH                       	= 0x98,
//	DS_REG_MAX_X_LOW             			= 0x99,
//	DS_REG_MAX_Y_HIGH            			= 0x9a,
//	DS_REG_MAX_Y_LOW             			= 0x9b,
//	DS_REG_K_X_HIGH            			= 0x9c,
//	DS_REG_K_X_LOW             			= 0x9d,
//	DS_REG_K_Y_HIGH            			= 0x9e,
//	DS_REG_K_Y_LOW             			= 0x9f,
	DS_REG_AUTO_CLB_MODE			= 0xa0,
//	DS_REG_LIB_VERSION_H 				= 0xa1,
//	DS_REG_LIB_VERSION_L 				= 0xa2,		
//	DS_REG_CIPHER						= 0xa3,
//	DS_REG_MODE						= 0xa4,
	DS_REG_PMODE						= 0xa5,	/* Power Consume Mode		*/	
	DS_REG_FIRMID						= 0xa6,
//	DS_REG_STATE						= 0xa7,
//	DS_REG_FT5201ID					= 0xa8,
	DS_REG_ERR						= 0xa9,
	DS_REG_CLB						= 0xaa,
};


//DS_REG_PMODE
#define PMODE_ACTIVE        0x00
#define PMODE_MONITOR       0x01
#define PMODE_STANDBY       0x02
#define PMODE_HIBERNATE     0x03

static struct i2c_client * this_client;

#define DS_FINGER_MAX_NUM      5
#define DS_KEY_MAX_NUM         3
#define DS_KEY_IDLE            0
#define DS_KEY_PRESSED         1

#define DS_TS_SWAP(a, b)          do {unsigned int temp; temp = a; a = b; b = temp;} while (0)

struct ds_ts_event {
	u16 x[DS_FINGER_MAX_NUM];
	u16 y[DS_FINGER_MAX_NUM];
	u16	pressure;
    u8  touch_point;
	u8  gesture_id;
};

struct ds_ts_data {
	struct input_dev	*input_dev;
	struct ds_ts_event		event;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;
	struct early_suspend	early_suspend;
};

struct ds_ts_key_def{
	int key_value;
	char * key_name;
	u16 x0_min;
	u16 x0_max;
	u16 y0_min;
	u16 y0_max;

	u16 x1_min;
	u16 x1_max;
	u16 y1_min;
	u16 y1_max;
	int status;
	int key_id;
};

static struct ds_ts_key_def key_data[DS_KEY_MAX_NUM] = {
	{KEY_MENU, "Menu", 460, 480, 820, 840, 30000, 30000, 30000, 30000, DS_KEY_IDLE, 0x08},
	{KEY_HOME, "Home", 410, 430, 820, 840, 30000, 30000, 30000, 30000, DS_KEY_IDLE, 0x04},
	{KEY_BACK, "Back", 370, 390, 820, 840, 30000, 30000, 30000, 30000, DS_KEY_IDLE, 0x02},
};


/***********************************************************************************************
Name	:	ds_i2c_rxdata 
Input	:	*rxdata
            *length
Output	:	ret
function	:	
***********************************************************************************************/
static int ds_i2c_rxdata(char * rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	
	return ret;
}

/***********************************************************************************************
Name	:	ds_read_reg 
Input	:	addr
            pdata
Output	:	
function	:	read register of ds

***********************************************************************************************/
static int ds_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf,
		},
	};

	buf[0] = addr;
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
	{
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	}
	else
	{
		*pdata = buf[0];
	}
	return ret;
  
}


/***********************************************************************************************
Name	:	 ds_read_fw_ver
Input	:	 void
Output	:	 firmware version 	
function	:	 read TP firmware version
***********************************************************************************************/
static unsigned char ds_read_fw_ver(void)
{
	unsigned char ver;
	int ret;
	ret = ds_read_reg(DS_REG_FIRMID, &ver);
	if (ret < 0)
		ver = 0xAB;
	return(ver);
}

static void ds_release_key(void)
{
	struct ds_ts_data *data = i2c_get_clientdata(this_client);
	int i;
	for (i = 0; i<DS_KEY_MAX_NUM; i++)
	{
		if (key_data[i].status == DS_KEY_PRESSED)
		{
			TSDBG(("Key %s Released\n", key_data[i].key_name));
			key_data[i].status = DS_KEY_IDLE;
			input_report_key(data->input_dev, key_data[i].key_value, 0);
		}
	}
	
}

static struct ds_ts_key_def * ds_get_key_p_by_gesture_id(u8 gesture_id)
{
	int i;
	for (i = 0; i<DS_KEY_MAX_NUM; i++)
	{
		if (key_data[i].key_id & gesture_id)
		{
			//printk("gesture_id %d, index %d, key %s\n", gesture_id, i, key_data[i].key_name);
			return key_data + i;
		}
	}
	
	return NULL;
}

static struct ds_ts_key_def * ds_get_key_p_by_point(u16 x, u16 y)
{
	int i;
	for (i = 0; i<DS_KEY_MAX_NUM; i++)
	{
		if (   (x >= key_data[i].x0_min) 
			&& (x <= key_data[i].x0_max) 
			&& (y >= key_data[i].y0_min)
			&& (y <= key_data[i].y0_max))
			return key_data + i;

		if (   (x >= key_data[i].x1_min) 
			&& (x <= key_data[i].x1_max) 
			&& (y >= key_data[i].y1_min)
			&& (y <= key_data[i].y1_max))
			return key_data + i;
	}
	
	return NULL;
}

/***********************************************************************************************
Name:	  ds_ts_release
Input:    void
Output:	  void
function: no touch handle function
***********************************************************************************************/
static void ds_ts_release(void)
{
	struct ds_ts_data *data = i2c_get_clientdata(this_client);
	ds_release_key();
	input_report_key(data->input_dev, BTN_TOUCH, 0);
	input_sync(data->input_dev);
}

static int ds_read_data(void)
{
	struct ds_ts_data *data = i2c_get_clientdata(this_client);
	struct ds_ts_event *event = &data->event;
	u8 buf[32] = {0};
	int ret = -1;
	int i;

	if (data->input_dev == NULL)
		return -1;

	ret = ds_i2c_rxdata(buf, 31);

    if (ret < 0) {
		TSDBG(("%s read_data i2c_rxdata failed: %d\n", __func__, ret));
		return ret;
	}

	memset(event, 0, sizeof(struct ds_ts_event));

	event->gesture_id = buf[1];
	event->touch_point = buf[2] & 0x07;// 000 0111
	if (event->touch_point > DS_FINGER_MAX_NUM)
		event->touch_point = DS_FINGER_MAX_NUM;
	
	TSDBG(("gesture_id %d, touch_point = %d\n", event->gesture_id, event->touch_point));

    if ((event->touch_point == 0) && (event->gesture_id == 0)) {
		TSDBG(("ds_read_data, touch_point = 0, ds_ts_release\n"));
        ds_ts_release();
        return 1; 
    }
	else
	{
		TSDBG(("ds_read_data, touch_point = %d, ds_ts_release\n", event->touch_point));
	}

	for (i=0; i<event->touch_point; i++)
	{
		event->x[i] = (s16)(buf[6 * i + 3] & 0x0F)<<8 | (s16)buf[6 * i + 4];
		event->y[i] = (s16)(buf[6 * i + 5] & 0x0F)<<8 | (s16)buf[6 * i + 6];
		DS_TS_SWAP(event->x[i], event->y[i]);
		TSDBG(("Point %d, x = %d, y = %d\n", i + 1, event->x[i], event->y[i]));
	}

    event->pressure = 200;
    return 0;
}


/***********************************************************************************************
Name: ds_report_value
Input: void	
Output:	void
function: report the point information to upper layer
***********************************************************************************************/
static void ds_report_value(void)
{
	struct ds_ts_data *data = i2c_get_clientdata(this_client);
	struct ds_ts_event *event = &data->event;
	struct ds_ts_key_def * p = NULL;
	int i;
	int tracking_id = 0;
	int need_input_sync = 0;
	
	if (data->input_dev == NULL)
		return ;
	
	for (i=0; i<event->touch_point; i++)
	{
		if ((event->x[i] < SCREEN_MAX_X) && (event->y[i] < SCREEN_MAX_Y))
		{
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, tracking_id++);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y[i]);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_mt_sync(data->input_dev);
			need_input_sync = 1;
		}
		else
		{
			p = ds_get_key_p_by_point(event->x[i], event->y[i]);
			
			if (p)
			{
				if (p->status == DS_KEY_IDLE)
				{
					p->status = DS_KEY_PRESSED;
					TSDBG(("key %s pressed\n", p->key_name));
					input_report_key(data->input_dev,	 p->key_value, 1);
					need_input_sync = 1;
				}
			}
		}
	}

	if (event->touch_point == 0)
	{
		p = ds_get_key_p_by_gesture_id(event->gesture_id);
	
		if (p)
		{
			if (p->status == DS_KEY_IDLE)
			{
				p->status = DS_KEY_PRESSED;
				TSDBG(("key %s pressed\n", p->key_name));
				input_report_key(data->input_dev, p->key_value, 1);
				need_input_sync = 1;
			}
		}
	}

	if (need_input_sync)
	{
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		input_sync(data->input_dev);
	}
}	/*end ds_report_value*/
/***********************************************************************************************
Name: ds_ts_pen_irq_work
Input: irq source
Output:	void
function: handle later half irq
***********************************************************************************************/
static void ds_ts_pen_irq_work(struct work_struct *work)
{
	int ret = -1;
	ret = ds_read_data();	
	if (ret == 0) {	
		TSDBG(("call ds_report_value\n"));
		ds_report_value();
	}
	if (this_client)
		enable_irq(this_client->irq);
}
/***********************************************************************************************
Name: ds_ts_interrupt
Input: irq, dev_id
Output: IRQ_HANDLED
function: 
***********************************************************************************************/
static irqreturn_t ds_ts_interrupt(int irq, void *dev_id)
{
	struct ds_ts_data *ds_ts = dev_id;
	disable_irq_nosync(this_client->irq);
	if (!work_pending(&ds_ts->pen_event_work)) {
		queue_work(ds_ts->ts_workqueue, &ds_ts->pen_event_work);
	}

	return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ds_ts_suspend
Input: handler
Output: void
function: suspend function for power management
***********************************************************************************************/
static void ds_ts_suspend(struct early_suspend *handler)
{
	TSDBG(("==ds_ts_suspend=\n"));
	disable_irq(this_client->irq);

}
/***********************************************************************************************
Name: ds_ts_resume
Input:	handler
Output:	void
function: resume function for powermanagement
***********************************************************************************************/
static void ds_ts_resume(struct early_suspend *handler)
{
	TSDBG(("==ds_ts_resume=\n"));
	enable_irq(this_client->irq);
}
#endif  //CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ds_ts_probe
Input: client, id
Output: 0 if OK, other value indicate error
function: probe
***********************************************************************************************/
static int ds_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ds_ts_data *ds_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value; 
	int i;
	
	printk("ds_ts_probe\n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ds_ts = kzalloc(sizeof(*ds_ts), GFP_KERNEL);
	if (!ds_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	ds_ts->input_dev = NULL;
	
	this_client = client;
	
	client->irq = TS_INT;
	i2c_set_clientdata(client, ds_ts);

    uc_reg_value = ds_read_fw_ver();
	if (0xAB == uc_reg_value)
	{
		printk("ds_ts_probe get fw version error.\n");
		goto exit_get_fwver_failed;
	}
	else
	{
		TSDBG(("Firmware version = 0x%x\n", uc_reg_value));
	}

	INIT_WORK(&ds_ts->pen_event_work, ds_ts_pen_irq_work);

	ds_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ds_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	err = request_irq(client->irq, ds_ts_interrupt, IRQF_TRIGGER_FALLING, "ds_ts", ds_ts);
	if (err < 0) {
		dev_err(&client->dev, "ds_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}

	disable_irq(client->irq);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	ds_ts->input_dev = input_dev;

	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X,  input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y,  input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
	set_bit(EV_KEY,             input_dev->evbit);
	set_bit(EV_ABS,             input_dev->evbit);
	set_bit(BTN_TOUCH,          input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT,  input_dev->propbit);

	/* Enable all supported keys */
	for (i=0; i<DS_KEY_MAX_NUM; i++)
		__set_bit(key_data[i].key_value, input_dev->keybit);

	__clear_bit(KEY_RESERVED, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,  0, DS_FINGER_MAX_NUM, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,   0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,   0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,  0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,  0, 200, 0, 0);

	input_dev->name		= DS_NAME;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"ds_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	TSDBG(("==register_early_suspend =\n"));
	ds_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	ds_ts->early_suspend.suspend = ds_ts_suspend;
	ds_ts->early_suspend.resume	= ds_ts_resume;
	register_early_suspend(&ds_ts->early_suspend);
#endif

    msleep(50);

    enable_irq(this_client->irq);
    msm_tp_set_found_flag(1);

	printk("ds_ts probe over\n");
    return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
//	free_irq(client->irq, ds_ts);
	free_irq(this_client->irq, ds_ts);
exit_irq_request_failed:
//exit_platform_data_null:
	cancel_work_sync(&ds_ts->pen_event_work);
	destroy_workqueue(ds_ts->ts_workqueue);
exit_create_singlethread:
exit_get_fwver_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ds_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}
/***********************************************************************************************
Name: ds_ts_remove
Input: client
Output: always 0
function: remove the driver
***********************************************************************************************/
static int __devexit ds_ts_remove(struct i2c_client *client)
{
	struct ds_ts_data *ds_ts = i2c_get_clientdata(client);
	TSDBG(("==ds_ts_remove=\n"));
	unregister_early_suspend(&ds_ts->early_suspend);
	free_irq(this_client->irq, ds_ts);
	input_unregister_device(ds_ts->input_dev);
	kfree(ds_ts);
	cancel_work_sync(&ds_ts->pen_event_work);
	destroy_workqueue(ds_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id ds_ts_id[] = {
	{ DS_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ds_ts_id);

static struct i2c_driver ds_ts_driver = {
	.probe		= ds_ts_probe,
	.remove		= __devexit_p(ds_ts_remove),
	.id_table	= ds_ts_id,
	.driver	= {
		.name	= DS_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init ds_ts_init(void)
{
	int ret;
	TSDBG(("==ds_ts_init==\n"));
    if (msm_tp_get_found_flag())
    {
        return -1;
    }
	ret = i2c_add_driver(&ds_ts_driver);
	TSDBG(("ret=%d\n",ret));
	return ret;
}

static void __exit ds_ts_exit(void)
{
	TSDBG(("==ds_ts_exit==\n"));
	i2c_del_driver(&ds_ts_driver);
}

late_initcall(ds_ts_init);
module_exit(ds_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ds TouchScreen driver");
MODULE_LICENSE("GPL");

