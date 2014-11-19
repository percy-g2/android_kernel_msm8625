/* 
 * drivers/input/touchscreen/ft5x06_57.c
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
#define SCREEN_MAX_X    480
#define SCREEN_MAX_Y    800
#define PRESS_MAX        255

#define FT5X06_57_NAME 	"ft5x06_57"


enum ft5x06_57_regs {
	FT5X06_57_REG_THGROUP					= 0x80,
	FT5X06_57_REG_THPEAK						= 0x81,
//	FT5X06_57_REG_THCAL						= 0x82,
//	FT5X06_57_REG_THWATER					= 0x83,
//	FT5X06_57_REG_THTEMP					= 0x84,
//	FT5X06_57_REG_THDIFF						= 0x85,				
//	FT5X06_57_REG_CTRL						= 0x86,
	FT5X06_57_REG_TIMEENTERMONITOR			= 0x87,
	FT5X06_57_REG_PERIODACTIVE				= 0x88,
	FT5X06_57_REG_PERIODMONITOR			= 0x89,
//	FT5X06_57_REG_HEIGHT_B					= 0x8a,
//	FT5X06_57_REG_MAX_FRAME					= 0x8b,
//	FT5X06_57_REG_DIST_MOVE					= 0x8c,
//	FT5X06_57_REG_DIST_POINT				= 0x8d,
//	FT5X06_57_REG_FEG_FRAME					= 0x8e,
//	FT5X06_57_REG_SINGLE_CLICK_OFFSET		= 0x8f,
//	FT5X06_57_REG_DOUBLE_CLICK_TIME_MIN	= 0x90,
//	FT5X06_57_REG_SINGLE_CLICK_TIME			= 0x91,
//	FT5X06_57_REG_LEFT_RIGHT_OFFSET		= 0x92,
//	FT5X06_57_REG_UP_DOWN_OFFSET			= 0x93,
//	FT5X06_57_REG_DISTANCE_LEFT_RIGHT		= 0x94,
//	FT5X06_57_REG_DISTANCE_UP_DOWN		= 0x95,
//	FT5X06_57_REG_ZOOM_DIS_SQR				= 0x96,
//	FT5X06_57_REG_RADIAN_VALUE				=0x97,
//	FT5X06_57_REG_MAX_X_HIGH                       	= 0x98,
//	FT5X06_57_REG_MAX_X_LOW             			= 0x99,
//	FT5X06_57_REG_MAX_Y_HIGH            			= 0x9a,
//	FT5X06_57_REG_MAX_Y_LOW             			= 0x9b,
//	FT5X06_57_REG_K_X_HIGH            			= 0x9c,
//	FT5X06_57_REG_K_X_LOW             			= 0x9d,
//	FT5X06_57_REG_K_Y_HIGH            			= 0x9e,
//	FT5X06_57_REG_K_Y_LOW             			= 0x9f,
	FT5X06_57_REG_AUTO_CLB_MODE			= 0xa0,
//	FT5X06_57_REG_LIB_VERSION_H 				= 0xa1,
//	FT5X06_57_REG_LIB_VERSION_L 				= 0xa2,		
//	FT5X06_57_REG_CIPHER						= 0xa3,
//	FT5X06_57_REG_MODE						= 0xa4,
	FT5X06_57_REG_PMODE						= 0xa5,	/* Power Consume Mode		*/	
	FT5X06_57_REG_FIRMID						= 0xa6,
//	FT5X06_57_REG_STATE						= 0xa7,
//	FT5X06_57_REG_FT5201ID					= 0xa8,
	FT5X06_57_REG_ERR						= 0xa9,
	FT5X06_57_REG_CLB						= 0xaa,
};


//FT5X06_57_REG_PMODE
#define PMODE_ACTIVE        0x00
#define PMODE_MONITOR       0x01
#define PMODE_STANDBY       0x02
#define PMODE_HIBERNATE     0x03

static struct i2c_client * this_client;

#define FT5X06_57_FINGER_MAX_NUM      5
#define FT5X06_57_KEY_MAX_NUM         3
#define FT5X06_57_KEY_IDLE            0
#define FT5X06_57_KEY_PRESSED         1

#define FT5X06_57_TS_SWAP(a, b)          do {unsigned int temp; temp = a; a = b; b = temp;} while (0)

struct ft5x06_57_event {
	u16 x[FT5X06_57_FINGER_MAX_NUM];
	u16 y[FT5X06_57_FINGER_MAX_NUM];
	u16	pressure;
    u8  touch_point;
	u8  gesture_id;
};

struct ft5x06_57_data {
	struct input_dev	*input_dev;
	struct ft5x06_57_event		event;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;
	struct early_suspend	early_suspend;
};

struct ft5x06_57_key_def{
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

static struct ft5x06_57_key_def key_data[FT5X06_57_KEY_MAX_NUM] = {
	{KEY_MENU, "Menu", 130, 150, 830, 850, 30000, 30000, 30000, 30000, FT5X06_57_KEY_IDLE, 0x08},
	{KEY_HOME, "Home", 250, 270, 830, 850, 30000, 30000, 30000, 30000, FT5X06_57_KEY_IDLE, 0x04},
	{KEY_BACK, "Back", 370, 390, 830, 850, 30000, 30000, 30000, 30000, FT5X06_57_KEY_IDLE, 0x02},
};


/***********************************************************************************************
Name	:	ft5x06_57_i2c_rxdata 
Input	:	*rxdata
            *length
Output	:	ret
function	:	
***********************************************************************************************/
static int ft5x06_57_i2c_rxdata(char * rxdata, int length)
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
Name	:	ft5x06_57_read_reg 
Input	:	addr
            pdata
Output	:	
function	:	read register of ds

***********************************************************************************************/
static int ft5x06_57_read_reg(u8 addr, u8 *pdata)
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
Name	:	 ft5x06_57_read_fw_ver
Input	:	 void
Output	:	 firmware version 	
function	:	 read TP firmware version
***********************************************************************************************/
static unsigned char ft5x06_57_read_fw_ver(void)
{
	unsigned char ver;
	int ret;
	ret = ft5x06_57_read_reg(FT5X06_57_REG_FIRMID, &ver);
	if (ret < 0)
		ver = 0xAB;
	return(ver);
}

static void ft5x06_57_release_key(void)
{
	struct ft5x06_57_data *data = i2c_get_clientdata(this_client);
	int i;
	for (i = 0; i<FT5X06_57_KEY_MAX_NUM; i++)
	{
		if (key_data[i].status == FT5X06_57_KEY_PRESSED)
		{
			TSDBG(("Key %s Released\n", key_data[i].key_name));
			key_data[i].status = FT5X06_57_KEY_IDLE;
			input_report_key(data->input_dev,	 key_data[i].key_value, 0);
		}
	}
	
}

static struct ft5x06_57_key_def * ft5x06_57_get_key_p_by_gesture_id(u8 gesture_id)
{
	int i;
	for (i = 0; i<FT5X06_57_KEY_MAX_NUM; i++)
	{
		if (key_data[i].key_id & gesture_id)
		{
			//printk("gesture_id %d, index %d, key %s\n", gesture_id, i, key_data[i].key_name);
			return key_data + i;
		}
	}
	
	return NULL;
}

static struct ft5x06_57_key_def * ft5x06_57_get_key_p_by_point(u16 x, u16 y)
{
	int i;
	for (i = 0; i<FT5X06_57_KEY_MAX_NUM; i++)
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
Name:	  ft5x06_57_release
Input:    void
Output:	  void
function: no touch handle function
***********************************************************************************************/
static void ft5x06_57_release(void)
{
	struct ft5x06_57_data *data = i2c_get_clientdata(this_client);

	ft5x06_57_release_key();
	input_report_key(data->input_dev, BTN_TOUCH, 0);
	input_sync(data->input_dev);
}

static int ft5x06_57_read_data(void)
{
	struct ft5x06_57_data *data = i2c_get_clientdata(this_client);
	struct ft5x06_57_event *event = &data->event;
	u8 buf[32] = {0};
	int ret = -1;
	int i;

	if (data->input_dev == NULL)
		return -1;
	ret = ft5x06_57_i2c_rxdata(buf, 31);

    if (ret < 0) {
		TSDBG(("%s read_data i2c_rxdata failed: %d\n", __func__, ret));
		return ret;
	}

	memset(event, 0, sizeof(struct ft5x06_57_event));

	event->gesture_id = buf[1];
	event->touch_point = buf[2] & 0x07;// 000 0111
	if (event->touch_point > FT5X06_57_FINGER_MAX_NUM)
		event->touch_point = FT5X06_57_FINGER_MAX_NUM;
	
	TSDBG(("gesture_id %d, touch_point = %d\n", event->gesture_id, event->touch_point));

    if ((event->touch_point == 0) && (event->gesture_id == 0)) {
		TSDBG(("ft5x06_57_read_data, touch_point = 0, ft5x06_57_release\n"));
        ft5x06_57_release();
        return 1; 
    }
	else
	{
		TSDBG(("ft5x06_57_read_data, touch_point = %d, ft5x06_57_release\n", event->touch_point));
	}

	for (i=0; i<event->touch_point; i++)
	{
		event->x[i] = (s16)(buf[6 * i + 3] & 0x0F)<<8 | (s16)buf[6 * i + 4];
		event->y[i] = (s16)(buf[6 * i + 5] & 0x0F)<<8 | (s16)buf[6 * i + 6];
		FT5X06_57_TS_SWAP(event->x[i], event->y[i]);
		TSDBG(("Point %d, x = %d, y = %d\n", i + 1, event->x[i], event->y[i]));
	}

    event->pressure = 200;
    return 0;
}


/***********************************************************************************************
Name: ft5x06_57_report_value
Input: void	
Output:	void
function: report the point information to upper layer
***********************************************************************************************/
static void ft5x06_57_report_value(void)
{
	struct ft5x06_57_data *data = i2c_get_clientdata(this_client);
	struct ft5x06_57_event *event = &data->event;
	struct ft5x06_57_key_def * p = NULL;
	int i;
	int tracking_id = 0;
	int need_input_sync = 0;
	
	if (data->input_dev == NULL)
		return;
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
			p = ft5x06_57_get_key_p_by_point(event->x[i], event->y[i]);
			
			if (p)
			{
				if (p->status == FT5X06_57_KEY_IDLE)
				{
					p->status = FT5X06_57_KEY_PRESSED;
					TSDBG(("key %s pressed\n", p->key_name));
					input_report_key(data->input_dev,	 p->key_value, 1);
					need_input_sync = 1;
				}
			}
		}
		
	}


	if (event->touch_point == 0)
	{
		p = ft5x06_57_get_key_p_by_gesture_id(event->gesture_id);
	
		if (p)
		{
			if (p->status == FT5X06_57_KEY_IDLE)
			{
				p->status = FT5X06_57_KEY_PRESSED;
				TSDBG(("gesture key %s pressed\n", p->key_name));
				input_report_key(data->input_dev,	 p->key_value, 1);
				need_input_sync = 1;
			}
		}
	}

	if (need_input_sync)
	{
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		input_sync(data->input_dev);
	}
}	/*end ft5x06_57_report_value*/
/***********************************************************************************************
Name: ft5x06_57_pen_irq_work
Input: irq source
Output:	void
function: handle later half irq
***********************************************************************************************/
static void ft5x06_57_pen_irq_work(struct work_struct *work)
{
	int ret = -1;
	ret = ft5x06_57_read_data();	
	if (ret == 0) {	
		TSDBG(("call ft5x06_57_report_value\n"));
		ft5x06_57_report_value();
	}
	if (this_client)
		enable_irq(this_client->irq);
}
/***********************************************************************************************
Name: ft5x06_57_interrupt
Input: irq, dev_id
Output: IRQ_HANDLED
function: 
***********************************************************************************************/
static irqreturn_t ft5x06_57_interrupt(int irq, void *dev_id)
{
	struct ft5x06_57_data *ft5x06_57 = dev_id;
	disable_irq_nosync(this_client->irq);
	if (!work_pending(&ft5x06_57->pen_event_work)) {
		queue_work(ft5x06_57->ts_workqueue, &ft5x06_57->pen_event_work);
	}

	return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x06_57_suspend
Input: handler
Output: void
function: suspend function for power management
***********************************************************************************************/
static void ft5x06_57_suspend(struct early_suspend *handler)
{
	TSDBG(("==ft5x06_57_suspend=\n"));
	disable_irq(this_client->irq);

}
/***********************************************************************************************
Name: ft5x06_57_resume
Input:	handler
Output:	void
function: resume function for powermanagement
***********************************************************************************************/
static void ft5x06_57_resume(struct early_suspend *handler)
{
	TSDBG(("==ft5x06_57_resume=\n"));
	enable_irq(this_client->irq);
}
#endif  //CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x06_57_probe
Input: client, id
Output: 0 if OK, other value indicate error
function: probe
***********************************************************************************************/
static int ft5x06_57_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x06_57_data *ft5x06_57;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value; 
	int i;
	
	printk("ft5x06_57_probe\n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x06_57 = kzalloc(sizeof(*ft5x06_57), GFP_KERNEL);
	if (!ft5x06_57)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	ft5x06_57->input_dev = NULL;

	this_client = client;
	
	client->irq = TS_INT;
	i2c_set_clientdata(client, ft5x06_57);

    uc_reg_value = ft5x06_57_read_fw_ver();
	if (0xAB == uc_reg_value)
	{
		printk("ft5x06_57_probe get fw version error.\n");
		goto exit_get_fwver_failed;
	}
	else
	{
		TSDBG(("Firmware version = 0x%x\n", uc_reg_value));
	}

	INIT_WORK(&ft5x06_57->pen_event_work, ft5x06_57_pen_irq_work);

	ft5x06_57->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x06_57->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	err = request_irq(client->irq, ft5x06_57_interrupt, IRQF_TRIGGER_FALLING, "ft5x06_57", ft5x06_57);
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
	
	ft5x06_57->input_dev = input_dev;

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
	for (i=0; i<FT5X06_57_KEY_MAX_NUM; i++)
		__set_bit(key_data[i].key_value, input_dev->keybit);

	__clear_bit(KEY_RESERVED, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,  0, FT5X06_57_FINGER_MAX_NUM, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,   0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,   0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,  0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,  0, 200, 0, 0);

	input_dev->name		= FT5X06_57_NAME;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"ft5x06_57_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	TSDBG(("==register_early_suspend =\n"));
	ft5x06_57->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	ft5x06_57->early_suspend.suspend = ft5x06_57_suspend;
	ft5x06_57->early_suspend.resume	= ft5x06_57_resume;
	register_early_suspend(&ft5x06_57->early_suspend);
#endif

    msleep(50);

    enable_irq(this_client->irq);
    msm_tp_set_found_flag(1);

	printk("ft5x06_57 probe ok, I2C addr = 0x%x\n", this_client->addr);
    return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
//	free_irq(client->irq, ft5x06_57);
	free_irq(this_client->irq, ft5x06_57);
exit_irq_request_failed:
//exit_platform_data_null:
	cancel_work_sync(&ft5x06_57->pen_event_work);
	destroy_workqueue(ft5x06_57->ts_workqueue);
exit_create_singlethread:
exit_get_fwver_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ft5x06_57);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}
/***********************************************************************************************
Name: ft5x06_57_remove
Input: client
Output: always 0
function: remove the driver
***********************************************************************************************/
static int __devexit ft5x06_57_remove(struct i2c_client *client)
{
	struct ft5x06_57_data *ft5x06_57 = i2c_get_clientdata(client);
	TSDBG(("==ft5x06_57_remove=\n"));
	unregister_early_suspend(&ft5x06_57->early_suspend);
	free_irq(this_client->irq, ft5x06_57);
	input_unregister_device(ft5x06_57->input_dev);
	kfree(ft5x06_57);
	cancel_work_sync(&ft5x06_57->pen_event_work);
	destroy_workqueue(ft5x06_57->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id ft5x06_57_id[] = {
	{ FT5X06_57_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x06_57_id);

static struct i2c_driver ft5x06_57_driver = {
	.probe		= ft5x06_57_probe,
	.remove		= __devexit_p(ft5x06_57_remove),
	.id_table	= ft5x06_57_id,
	.driver	= {
		.name	= FT5X06_57_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init ft5x06_57_init(void)
{
	int ret;
	TSDBG(("==ft5x06_57_init==\n"));
    if (msm_tp_get_found_flag())
    {
        return -1;
    }
	ret = i2c_add_driver(&ft5x06_57_driver);
	TSDBG(("ret=%d\n",ret));
	return ret;
}

static void __exit ft5x06_57_exit(void)
{
	TSDBG(("==ft5x06_57_exit==\n"));
	i2c_del_driver(&ft5x06_57_driver);
}

late_initcall(ft5x06_57_init);
module_exit(ft5x06_57_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ds TouchScreen driver");
MODULE_LICENSE("GPL");

