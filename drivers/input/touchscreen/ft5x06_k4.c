/*
 * drivers/input/touchscreen/ft5x06_k4.c
 *
 * FocalTech ft5x06_k4 TouchScreen driver.
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
 */

/**
   This driver is for touchscreen provided by WW Fortune Ship
   Chip, FocalTech
   I2C addr, 0x39
   Multitouch, 5 finger
   Touch Key, Menu, Home, Back, Search
   Resolution, 480 * 800
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

#define FT5X06_K4_NAME	 "ft5x06_k4"

enum ft5x06_k4_ts_regs
{
    FT5X06_K4_REG_THGROUP					= 0x80,
    FT5X06_K4_REG_THPEAK					= 0x81,
    FT5X06_K4_REG_TIMEENTERMONITOR		= 0x87,
    FT5X06_K4_REG_PERIODACTIVE			= 0x88,
    FT5X06_K4_REG_PERIODMONITOR			= 0x89,
    FT5X06_K4_REG_AUTO_CLB_MODE			= 0xa0,
    FT5X06_K4_REG_PMODE					= 0xa5,
    FT5X06_K4_REG_FIRMID					= 0xa6,
    FT5X06_K4_REG_ERR						= 0xa9,
    FT5X06_K4_REG_CLB						= 0xaa,
};

static struct i2c_client * ft5x06_k4_client;

#define FT5X06_K4_FINGER_MAX_NUM      5
#define FT5X06_K4_KEY_MAX_NUM         4
#define FT5X06_K4_KEY_IDLE            0
#define FT5X06_K4_KEY_PRESSED         1
#define FT5X06_K4_PRESSURE_MAX          255

#define FT5X06_K4_TS_SUSPEND         gpio_set_value(TS_RESET_PORT, 0)
#define FT5X06_K4_TS_WORK             gpio_set_value(TS_RESET_PORT, 1)
#define FT5X06_K4_TS_SWAP(a, b) do {unsigned int temp; temp = a; a = b; b = temp;} while (0)

typedef struct _FT5X06_K4_FINGER_INFO
{
    u8 last_press;
    u8 curr_press;
    s16 x;
    s16 y;
} FT5X06_K4_FINGER_INFO;



struct ft5x06_k4_data
{
    struct input_dev	*input_dev;
    struct work_struct 	pen_event_work;
    struct workqueue_struct *ts_workqueue;
    struct early_suspend	early_suspend;
    u8  touch_point;
    FT5X06_K4_FINGER_INFO finger_info[FT5X06_K4_FINGER_MAX_NUM];
};

struct ft5x06_k4_key_def
{
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
};

/* G618 */
static struct ft5x06_k4_key_def key_data[FT5X06_K4_KEY_MAX_NUM] =
{
    {KEY_MENU, "Menu",     40,  79, 830, 900, 30000, 30000, 30000, 30000,  FT5X06_K4_KEY_IDLE},
    {KEY_HOME, "Home",     160, 199, 830, 900, 30000, 30000, 30000, 30000,  FT5X06_K4_KEY_IDLE},
    {KEY_BACK, "Back",     280, 319, 830, 900, 30000, 30000, 30000, 30000,  FT5X06_K4_KEY_IDLE},
    {KEY_SEARCH, "Search", 400, 439, 830, 900, 30000, 30000, 30000, 30000,  FT5X06_K4_KEY_IDLE},
};


/***********************************************************************************************
Name	:	ft5x06_k4_i2c_rxdata
Input	:	*rxdata
            *length

Output	:	ret
function	:

***********************************************************************************************/
static int ft5x06_k4_i2c_rxdata(char *rxdata, int length)
{
    int ret;

    struct i2c_msg msgs[] =
    {
        {
            .addr	= ft5x06_k4_client->addr,
            .flags	= 0,
            .len	= 1,
            .buf	= rxdata,
        },
        {
            .addr	= ft5x06_k4_client->addr,
            .flags	= I2C_M_RD,
            .len	= length,
            .buf	= rxdata,
        },
    };

    ret = i2c_transfer(ft5x06_k4_client->adapter, msgs, 2);
    if (ret < 0)
        pr_err("msg %s i2c read error: %d\n", __func__, ret);

    return ret;
}


/***********************************************************************************************
Name	:	ft5x06_k4_read_reg
Input	:	addr
            pdata
Output	:
function	:	read register of ft5x06_k4

***********************************************************************************************/
static int ft5x06_k4_read_reg(u8 addr, u8 *pdata)
{
    int ret;
    u8 buf[2] = {0};

    struct i2c_msg msgs[] =
    {
        {
            .addr	= ft5x06_k4_client->addr,
            .flags	= 0,
            .len	= 1,
            .buf	= buf,
        },
        {
            .addr	= ft5x06_k4_client->addr,
            .flags	= I2C_M_RD,
            .len	= 1,
            .buf	= buf,
        },
    };

    buf[0] = addr;
    ret = i2c_transfer(ft5x06_k4_client->adapter, msgs, 2);
    if (ret < 0)
    {
        pr_err("%s i2c read error: %d\n", __func__, ret);
    }
    else
    {
        *pdata = buf[0];
    }
    return ret;

}


/***********************************************************************************************
Name	:	 ft5x06_k4_read_fw_ver
Input	:	 void
Output	:	 firmware version
function	:	 read TP firmware version
***********************************************************************************************/
static unsigned char ft5x06_k4_read_fw_ver(void)
{
    unsigned char ver;
    int ret;
    ret = ft5x06_k4_read_reg(FT5X06_K4_REG_FIRMID, &ver);
    if (ret < 0)
        ver = 0xAB;
    return(ver);
}

/***********************************************************************************************
Name	:	 ft5x06_k4_release_key
Input	:	 void
Output	:	 void
function	:	 release key, key up

***********************************************************************************************/
static void ft5x06_k4_release_key(void)
{
    struct ft5x06_k4_data *data = i2c_get_clientdata(ft5x06_k4_client);
    int i;
    for (i = 0; i<FT5X06_K4_KEY_MAX_NUM; i++)
    {
        if (key_data[i].status == FT5X06_K4_KEY_PRESSED)
        {
            TSDBG(("Key %s Released\n", key_data[i].key_name));
            key_data[i].status = FT5X06_K4_KEY_IDLE;
            input_report_key(data->input_dev,	 key_data[i].key_value, 0);
        }
    }
}

/***********************************************************************************************
Name:	  ft5x06_k4_release
Input:    void
Output:	  void
function: no touch handle function
***********************************************************************************************/
static void ft5x06_k4_release(void)
{
    struct ft5x06_k4_data *data = i2c_get_clientdata(ft5x06_k4_client);
    ft5x06_k4_release_key();
    input_report_key(data->input_dev, BTN_TOUCH, 0);
    input_sync(data->input_dev);
}

static int ft5x06_k4_read_data(void)
{
    struct ft5x06_k4_data *ts = i2c_get_clientdata(ft5x06_k4_client);
    FT5X06_K4_FINGER_INFO * finger_info = ts->finger_info;
    u8 buf[32] = {0};
    int ret = -1;
    int i;

	if (ts->input_dev == NULL)
		return -1;
	
    ret = ft5x06_k4_i2c_rxdata(buf, 31);

    if (ret < 0)
    {
        TSDBG(("%s read_data i2c_rxdata failed: %d\n", __func__, ret));
        return ret;
    }

    ts->touch_point = buf[2] & 0x07;// 000 0111

    if (ts->touch_point == 0)
    {
        TSDBG(("ft5x06_k4_read_data, touch_point = 0, call ft5x06_k4_release\n"));
		for (i=0; i<FT5X06_K4_FINGER_MAX_NUM; i++)
			finger_info[i].last_press = 0;
        ft5x06_k4_release();
        return 1;
    }
    else
    {
        TSDBG(("ft5x06_k4_read_data, touch_point = %d\n", ts->touch_point));
    }

    switch (ts->touch_point)
    {
    case 5:
        finger_info[4].x = (s16)(buf[0x1b] & 0x0F)<<8 | (s16)buf[0x1c];
        finger_info[4].y = (s16)(buf[0x1d] & 0x0F)<<8 | (s16)buf[0x1e];
    case 4:
        finger_info[3].x = (s16)(buf[0x15] & 0x0F)<<8 | (s16)buf[0x16];
        finger_info[3].y = (s16)(buf[0x17] & 0x0F)<<8 | (s16)buf[0x18];
    case 3:
        finger_info[2].x = (s16)(buf[0x0f] & 0x0F)<<8 | (s16)buf[0x10];
        finger_info[2].y = (s16)(buf[0x11] & 0x0F)<<8 | (s16)buf[0x12];
    case 2:
        finger_info[1].x = (s16)(buf[9] & 0x0F)<<8 | (s16)buf[10];
        finger_info[1].y = (s16)(buf[11] & 0x0F)<<8 | (s16)buf[12];
    case 1:
        finger_info[0].x = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
        finger_info[0].y = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
        break;
    default:
        return -1;
    }

#if 0
    for (i=0; i<ts->touch_point; i++)
    {
        TSDBG(("Point %d, x = %d, y = %d\n", i + 1, finger_info[i].x, finger_info[i].y));
    }
#endif

    return 0;
}

static struct ft5x06_k4_key_def * ft5x06_k4_get_key_p(int x, int y)
{
    int i;
    for (i = 0; i<FT5X06_K4_KEY_MAX_NUM; i++)
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
Name: ft5x06_k4_report_value
Input: void
Output:	void
function: report the point information to upper layer
***********************************************************************************************/
static void ft5x06_k4_report_value(void)
{
    struct ft5x06_k4_data *ts = i2c_get_clientdata(ft5x06_k4_client);
    FT5X06_K4_FINGER_INFO *finger_info = ts->finger_info;
    struct ft5x06_k4_key_def * p = NULL;
    int i;
    int key_pressed = 0;
    int finger_num = 0;
    int point_up = 0;
    int first_touch = 1;

	if (ts->input_dev == NULL)
		return;

    for (i=0; i<FT5X06_K4_FINGER_MAX_NUM; i++)
    {
        ts->finger_info[i].curr_press = 0;

        if (i < ts->touch_point)
        {
            if ((finger_info[i].x < SCREEN_MAX_X) && (finger_info[i].y < SCREEN_MAX_Y))
            {
                ts->finger_info[i].curr_press = 1;
                finger_num++;
            }
        }
    }

    if (finger_num != 0)
    {
        for (i=0; i<FT5X06_K4_FINGER_MAX_NUM; i++)
        {
            if (finger_info[i].last_press)
            {
                first_touch = 0; //not first touch
                break;
            }
        }
    }

	if (first_touch)
		TSDBG(("First Touch\n"));
		

    for (i=0; i<FT5X06_K4_FINGER_MAX_NUM; i++)
    {
        if (finger_info[i].curr_press)
        {
            input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,1);
            if (first_touch)
            {
                first_touch = 0;
				TSDBG(("First Touch Report\n"));
                input_report_key(ts->input_dev, BTN_TOUCH, 1);
            }

            input_report_abs(ts->input_dev, ABS_MT_POSITION_X, finger_info[i].x);
            input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, finger_info[i].y);
            input_mt_sync(ts->input_dev);
        }
        else
        {
            if (finger_info[i].last_press != 0)
            {
                point_up = 1;
            }
        }

        finger_info[i].last_press = finger_info[i].curr_press;
    }

    /* Last finger up */
    if (!finger_num && point_up)
    {
        input_report_key(ts->input_dev, BTN_TOUCH, 0);
        input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
        input_mt_sync(ts->input_dev);
    }

    /* Handle Key */
    key_pressed = 0;

    /* if no point pressed, handle key */
    if (!finger_num)
    {
        for (i=0; i<ts->touch_point; i++)
        {
            p = ft5x06_k4_get_key_p(finger_info[i].x, finger_info[i].y);

            if (p)
            {
                if (p->status == FT5X06_K4_KEY_IDLE)
                {
                    p->status = FT5X06_K4_KEY_PRESSED;
                    TSDBG(("key %s pressed\n", p->key_name));
                    input_report_key(ts->input_dev, BTN_TOUCH, 1);
                    input_report_key(ts->input_dev, p->key_value, 1);
                }
                key_pressed = 1;
            }
        }
    }

    if (!key_pressed)
    {
        ft5x06_k4_release_key();
    }

	input_report_key(ts->input_dev, BTN_TOUCH, !!ts->touch_point);
    input_sync(ts->input_dev);
}	/*end ft5x06_k4_report_value*/
/***********************************************************************************************
Name: ft5x06_k4_pen_irq_work
Input: irq source
Output:	void
function: handle later half irq
***********************************************************************************************/
static void ft5x06_k4_pen_irq_work(struct work_struct *work)
{
    int ret = -1;
    ret = ft5x06_k4_read_data();
    if (ret == 0)
    {
        TSDBG(("call ft5x06_k4_report_value\n"));
        ft5x06_k4_report_value();
    }
    if (ft5x06_k4_client)
        enable_irq(ft5x06_k4_client->irq);
}
/***********************************************************************************************
Name: ft5x06_k4_interrupt
Input: irq, dev_id
Output: IRQ_HANDLED
function:
***********************************************************************************************/
static irqreturn_t ft5x06_k4_interrupt(int irq, void *dev_id)
{
    struct ft5x06_k4_data *ft5x06_k4 = dev_id;
    disable_irq_nosync(ft5x06_k4_client->irq);
//	TSDBG("==int=\n");
    if (!work_pending(&ft5x06_k4->pen_event_work))
    {
        queue_work(ft5x06_k4->ts_workqueue, &ft5x06_k4->pen_event_work);
    }

    return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x06_k4_suspend
Input: handler
Output: void
function: suspend function for power management
***********************************************************************************************/
static void ft5x06_k4_suspend(struct early_suspend *handler)
{
//	struct ft5x06_k4_data *ts;
//	ts =  container_of(handler, struct ft5x06_k4_data, early_suspend);

    TSDBG(("ft5x06_k4_suspend\n"));
    disable_irq(ft5x06_k4_client->irq);

}
/***********************************************************************************************
Name: ft5x06_k4_resume
Input:	handler
Output:	void
function: resume function for powermanagement
***********************************************************************************************/
static void ft5x06_k4_resume(struct early_suspend *handler)
{
    FT5X06_K4_TS_SUSPEND;
    msleep(10);
    FT5X06_K4_TS_WORK;
    msleep(50);
    TSDBG(("ft5x06_k4_resume\n"));
    enable_irq(ft5x06_k4_client->irq);
}
#endif  //CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x06_k4_probe
Input: client, id
Output: 0 if OK, other value indicate error
function: probe
***********************************************************************************************/
static int ft5x06_k4_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ft5x06_k4_data *ft5x06_k4;
    struct input_dev *input_dev;
    int err = 0;
    unsigned char uc_reg_value;
    int i;

    printk("ft5x06_k4_probe\n");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        err = -ENODEV;
        goto exit_check_functionality_failed;
    }

    ft5x06_k4 = kzalloc(sizeof(*ft5x06_k4), GFP_KERNEL);
    if (!ft5x06_k4)
    {
        err = -ENOMEM;
        goto exit_alloc_data_failed;
    }

	ft5x06_k4->input_dev = NULL;
    ft5x06_k4_client = client;

	client->irq = TS_INT;
    i2c_set_clientdata(client, ft5x06_k4);

//    FT5X06_K4_TS_WORK;
    msleep(50);
    FT5X06_K4_TS_SUSPEND;
    msleep(20);
    FT5X06_K4_TS_WORK;
    msleep(50);
    uc_reg_value = ft5x06_k4_read_fw_ver();
    if (0xAB == uc_reg_value)
    {
        printk("ft5x06_k4_probe get fw version error.\n");
        goto exit_get_fwver_failed;
    }
    else
    {
        printk("ft5x06_k4_probe Firmware version = 0x%x\n", uc_reg_value);
    }

    INIT_WORK(&ft5x06_k4->pen_event_work, ft5x06_k4_pen_irq_work);

    ft5x06_k4->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!ft5x06_k4->ts_workqueue)
    {
        err = -ESRCH;
        goto exit_create_singlethread;
    }

    err = request_irq(client->irq, ft5x06_k4_interrupt, IRQF_TRIGGER_FALLING, "ft5x06_k4", ft5x06_k4);
    if (err < 0)
    {
        dev_err(&client->dev, "ft5x06_k4_probe: request irq failed\n");
        goto exit_irq_request_failed;
    }

    disable_irq(client->irq);

    input_dev = input_allocate_device();
    if (!input_dev)
    {
        err = -ENOMEM;
        dev_err(&client->dev, "failed to allocate input device\n");
        goto exit_input_dev_alloc_failed;
    }

    ft5x06_k4->input_dev = input_dev;

    set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
    set_bit(ABS_MT_POSITION_X,  input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y,  input_dev->absbit);
    set_bit(BTN_TOUCH,          input_dev->keybit);
    set_bit(EV_KEY,             input_dev->evbit);
    set_bit(EV_ABS,             input_dev->evbit);
	set_bit(BTN_TOUCH,          input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT,  input_dev->propbit);

    /* Enable all supported keys */
    for (i=0; i<FT5X06_K4_KEY_MAX_NUM; i++)
        __set_bit(key_data[i].key_value, input_dev->keybit);

    __clear_bit(KEY_RESERVED, input_dev->keybit);

    input_set_abs_params(input_dev, ABS_MT_POSITION_X,   0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y,   0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,  0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,  0, 200, 0, 0);

    input_dev->name		= FT5X06_K4_NAME;		//dev_name(&client->dev)
    err = input_register_device(input_dev);
    if (err)
    {
        dev_err(&client->dev,
                "ft5x06_k4_probe: failed to register input device: %s\n",
                dev_name(&client->dev));
        goto exit_input_register_device_failed;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    TSDBG(("ft5x06_k4_probe register_early_suspend\n"));
    ft5x06_k4->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    ft5x06_k4->early_suspend.suspend = ft5x06_k4_suspend;
    ft5x06_k4->early_suspend.resume	= ft5x06_k4_resume;
    register_early_suspend(&ft5x06_k4->early_suspend);
#endif

    msleep(50);

    enable_irq(ft5x06_k4_client->irq);

    msm_tp_set_found_flag(1);
    printk("ft5x06_k4_probe probe OK, I2C addr 0x%x\n", ft5x06_k4_client->addr);
    return 0;

exit_input_register_device_failed:
    input_free_device(input_dev);
exit_input_dev_alloc_failed:
    free_irq(ft5x06_k4_client->irq, ft5x06_k4);
exit_irq_request_failed:
    cancel_work_sync(&ft5x06_k4->pen_event_work);
    destroy_workqueue(ft5x06_k4->ts_workqueue);
exit_create_singlethread:
exit_get_fwver_failed:
    i2c_set_clientdata(client, NULL);
    kfree(ft5x06_k4);
exit_alloc_data_failed:
exit_check_functionality_failed:
    return err;
}
/***********************************************************************************************
Name: ft5x06_k4_remove
Input: client
Output: always 0
function: remove the driver
***********************************************************************************************/
static int __devexit ft5x06_k4_remove(struct i2c_client *client)
{
    struct ft5x06_k4_data *ft5x06_k4 = i2c_get_clientdata(client);
    TSDBG(("ft5x06_k4_remove\n"));
    unregister_early_suspend(&ft5x06_k4->early_suspend);
    free_irq(ft5x06_k4_client->irq, ft5x06_k4);
    input_unregister_device(ft5x06_k4->input_dev);
    kfree(ft5x06_k4);
    cancel_work_sync(&ft5x06_k4->pen_event_work);
    destroy_workqueue(ft5x06_k4->ts_workqueue);
    i2c_set_clientdata(client, NULL);
    return 0;
}

static const struct i2c_device_id ft5x06_k4_id[] =
{
    {FT5X06_K4_NAME, 0},
    { }
};


MODULE_DEVICE_TABLE(i2c, ft5x06_k4_id);

static struct i2c_driver ft5x06_k4_driver =
{
    .probe		= ft5x06_k4_probe,
    .remove		= __devexit_p(ft5x06_k4_remove),
    .id_table	= ft5x06_k4_id,
    .driver	= {
        .name	= FT5X06_K4_NAME,
        .owner	= THIS_MODULE,
    },
};

static int __init ft5x06_k4_init(void)
{
    int ret;
    TSDBG(("ft5x06_k4_init\n"));
    if (msm_tp_get_found_flag())
        return -1;
    ret = i2c_add_driver(&ft5x06_k4_driver);
    TSDBG(("ft5x06_k4_init ret=%d\n", ret));
    return ret;
}

static void __exit ft5x06_k4_exit(void)
{
    TSDBG(("ft5x06_k4_exit\n"));
    i2c_del_driver(&ft5x06_k4_driver);
}

late_initcall(ft5x06_k4_init);
module_exit(ft5x06_k4_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x06_k4 TouchScreen driver");
MODULE_LICENSE("GPL");

