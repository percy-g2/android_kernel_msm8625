/* drivers/input/touchscreen/gt811_touch.c
 *
 * Copyright (C) 2011 Goodix, Inc.
 *
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <asm/uaccess.h>
#include "goodix_touch.h"

//#define GT811_DEBUG

#ifdef GT811_DEBUG
#define GT811_PRINT(x) printk x
#else
#define GT811_PRINT(x)
#endif

#define GT811_I2C_NAME "gt811"
extern void msm_tp_set_found_flag(int flag);
extern int msm_tp_get_found_flag(void);

/* define resolution of the LCD */
#define GT811_SCREEN_MAX_WIDTH	480
#define GT811_SCREEN_MAX_HEIGHT	800

#define GT811_ORG_FINGER_DATA_LEN  8
#define GT811_POINT_DATA_LEN        58
#define GT811_PRESSURE_MAX          255

#define GT811_MAX_FINGER_NUM	    5
#define GT811_SWAP(x, y) do { typeof(x) z = x; x = y; y = z; } while (0)

#define READ_TOUCH_ADDR_H 0x07
#define READ_TOUCH_ADDR_L 0x21
#define READ_ID_ADDR_H    0x00
#define READ_ID_ADDR_L    0xff
#define GT811_ADDR_LENGTH 0x02
#define GT811_READ_BYTES  (GT811_ADDR_LENGTH + 34)

#define GT811_TS_SUSPEND    gpio_set_value(TS_RESET_PORT, 1)
#define GT811_TS_WORK       gpio_set_value(TS_RESET_PORT, 0)

typedef struct _GT811_FINGER_INFO
{
    u8 last_press;
    u8 curr_press;
    uint16_t x;
    uint16_t y;
} GT811_FINGER_INFO;

struct gt811_ts_data
{

    uint16_t addr;
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct work_struct  work;
    char phys[32];
    struct early_suspend early_suspend;
    int (*power)(struct gt811_ts_data * ts, int on);

    GT811_FINGER_INFO finger[GT811_MAX_FINGER_NUM];
};

struct gt811_ts_key_def
{
    int key_value;
    char * key_name;
    int status;
    int key_id;
};

#define GT811_KEY_IDLE            0
#define GT811_KEY_PRESSED         1

static struct gt811_ts_key_def gt811_key_info[] =
{
    {KEY_HOME,   "Home",	GT811_KEY_IDLE, 0x02},
    {KEY_MENU,   "Menu",	GT811_KEY_IDLE, 0x01},
    {KEY_BACK,   "Back",	GT811_KEY_IDLE, 0x04},
};

#define GT811_KEY_MAX_NUM (sizeof(gt811_key_info)/sizeof(gt811_key_info[0]))

static struct workqueue_struct * gt811_wq;
static const char *gt811_ts_name = GT811_I2C_NAME;
static struct i2c_client * gt811_i2c_client = NULL;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gt811_ts_early_suspend(struct early_suspend *h);
static void gt811_ts_late_resume(struct early_suspend *h);
#endif
static struct i2c_client * p_client_gt811 = NULL;

/*
#define GT811_VERSION_0 0x005A
#define GT811_VERSION_1 0x0083

static unsigned short gt811_version = GT811_VERSION_1;
*/

static int i2c_read_bytes(struct i2c_client *client, u8 *buf, int len)
{
    struct i2c_msg msgs[2];
    int ret=-1;

    msgs[0].flags=!I2C_M_RD;
    msgs[0].addr=client->addr;
    msgs[0].len=2;
    msgs[0].buf=&buf[0];

    msgs[1].flags=I2C_M_RD;
    msgs[1].addr=client->addr;
    msgs[1].len=len-2;
    msgs[1].buf=&buf[2];

    ret=i2c_transfer(client->adapter,msgs, 2);
    return ret;
}

static int i2c_write_bytes(struct i2c_client *client,u8 *data,int len)
{
    struct i2c_msg msg;
    int ret=-1;
    msg.flags=!I2C_M_RD;
    msg.addr=client->addr;
    msg.len=len;
    msg.buf=data;

    ret=i2c_transfer(client->adapter,&msg, 1);
    return ret;
}

static int i2c_end_cmd(struct gt811_ts_data *ts)
{
	u8 end_cmd_data[2]= {0x80, 0x00};
    return i2c_write_bytes(ts->client,end_cmd_data,2);
}

static int gt811_init_panel(struct gt811_ts_data *ts)
{
    int ret;

#ifdef CONFIG_USE_SKY_TP_001
    u8 config_info_version_0[] = {0x06,0xA2,
    	  0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,
    	  0x02,0x00,0x11,0x11,0x21,0x11,0x31,0x11,
    	  0x41,0x11,0x51,0x11,0x61,0x11,0x71,0x11,
    	  0x81,0x11,0x91,0x11,0xA1,0x11,0xB1,0x11,
    	  0xC1,0x11,0xD1,0x11,0xE1,0x11,0xF1,0x11,
    	  0x08,0x88,0x1F,0x03,0x00,0x00,0x00,0x15,
    	  0x15,0x15,0x10,0x0F,0x0A,0x48,0x30,0x15,
    	  0x03,0x00,0x05,0xE0,0x01,0x20,0x03,0x00,
    	  0x00,0x3B,0x33,0x37,0x30,0x00,0x00,0x24,
    	  0x14,0x43,0x08,0x00,0x00,0x00,0x00,0x0C,
    	  0x14,0x10,0x4E,0x02,0x0C,0x40,0x37,0x00,
    	  0x2E,0x0C,0xA0,0x90,0x80,0x00,0x0C,0x40,
    	  0x30,0x3C,0x28,0x00,0x00,0x00,0x00,0x00,
    	  0x00,0x01};
#endif		
#ifdef CONFIG_USE_SKY_TP_002
    u8 config_info_version_0[] = {0x06,0xA2,
        0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,
        0x02,0x00,0x12,0x22,0x22,0x22,0x32,0x22,
        0x42,0x22,0x52,0x22,0x62,0x22,0x72,0x22,
        0x82,0x22,0x92,0x22,0xA2,0x22,0xB2,0x22,
        0xC2,0x22,0xD2,0x22,0xE2,0x22,0xF2,0x22,
        0x02,0x22,0x3B,0x03,0x28,0x28,0x28,0x16,
        0x16,0x16,0x0F,0x0F,0x0A,0x45,0x2A,0x05,
        0x03,0x00,0x05,0xE0,0x01,0x20,0x03,0x00,
        0x00,0x3A,0x33,0x37,0x30,0x00,0x00,0x05,
        0x20,0x05,0x0A,0x00,0x00,0x00,0x00,0xBC,
        0x14,0x10,0x44,0x02,0xBC,0x00,0x00,0x00,
        0x00,0xBC,0x00,0x00,0x00,0x00,0x00,0x40,
        0x30,0x20,0x14,0x00,0x00,0x00,0x00,0x00,
    	0x00,0x01};
		
#endif		

   	ret = i2c_write_bytes(ts->client,config_info_version_0, (sizeof(config_info_version_0)/sizeof(config_info_version_0[0])));
                            
    if (ret < 0)
        return ret;

    msleep(10);
    return 0;
}


static int  gt811_read_version(struct gt811_ts_data *ts)
{
    int ret;
    u8 version_data[5]= {0};	 //store touchscreen version infomation
    version_data[0]=0x07;
    version_data[1]=0x15;
    ret=i2c_read_bytes(ts->client, version_data, 4);
    if (ret < 0)
        return ret;
    dev_info(&ts->client->dev,"Guitar Version: %d.%d\n",version_data[3],version_data[2]);
//    gt811_version = version_data[3];
//    gt811_version = gt811_version << version_data[2];
    return 0;

}

static void gt811_ts_key_down(struct input_dev *input_dev, int index)
{
    if (gt811_key_info[index].status == GT811_KEY_IDLE)
    {
        gt811_key_info[index].status = GT811_KEY_PRESSED;
        GT811_PRINT(("%s down\n", gt811_key_info[index].key_name));
        input_report_key(input_dev,gt811_key_info[index].key_value, 1);

    }
}

static void gt811_ts_key_up(struct input_dev *input_dev, int index)
{
    if (gt811_key_info[index].status == GT811_KEY_PRESSED)
    {
        gt811_key_info[index].status = GT811_KEY_IDLE;
        GT811_PRINT(("%s up\n", gt811_key_info[index].key_name));
        input_report_key(input_dev,gt811_key_info[index].key_value, 0);
    }

}


static void gt811_ts_work_func(struct work_struct *work)
{
    u8  touch_data[GT811_READ_BYTES] = {READ_TOUCH_ADDR_H,READ_TOUCH_ADDR_L,0};
    u8  key = 0;
    int ret = -1;
    int i;
    u8  track_id[GT811_MAX_FINGER_NUM] = {0};
    u16 x = 0;
    u16 y = 0;
    u8  w = 0;
    int point_up = 0;
    int point_down = 0;
	int first_touch = 1;
    u8  point_index = 0;
    u8  point_tmp = 0;
    u8  touch_num = 0;
    u32 position = 0;	

    struct gt811_ts_data *ts = container_of(work, struct gt811_ts_data, work);

    for (i=0; i<GT811_MAX_FINGER_NUM; i++)
        ts->finger[i].curr_press = 0;

    ret=i2c_read_bytes(ts->client, touch_data,sizeof(touch_data)/sizeof(touch_data[0]));
    if (ret <= 0)
    {
        dev_err(&(ts->client->dev),"I2C transfer error. Number: %d\n ", ret);
        goto XFER_ERROR;
    }

    if (touch_data[2] & 0x20)
    {
    	if (touch_data[3] == 0xF0)
    	{
			ret=gt811_init_panel(ts);
			goto TP_ABNORMAL;
    	}
    }

    point_index = touch_data[2] & 0x1f;
    point_tmp = point_index;
    for(i=0; (i<GT811_MAX_FINGER_NUM) && point_tmp; i++)
    {
        if (point_tmp & 0x01)
        {
            track_id[touch_num++] = i;
        }
        point_tmp >>= 1;
    }

    if (touch_num != 0)
    {
        for(i=0; i<touch_num; i++)
        {
            if(track_id[i] != 3)
            {
                if(track_id[i] < 3)
                {
                    position = 4 + track_id[i] * 5;
                }
                else
                {
                    position = 30;
                }
                x = (u16)(touch_data[position]<<8)+(u16)touch_data[position+1];
                y = (u16)(touch_data[position+2]<<8)+(u16)touch_data[position+3];
                w = touch_data[position+4];
            }
            else
            {
                x = (u16)(touch_data[19]<<8)+(u16)touch_data[26];
                y = (u16)(touch_data[27]<<8)+(u16)touch_data[28];
                w = touch_data[29];	
            }

            if ((x > GT811_SCREEN_MAX_WIDTH)||(y > GT811_SCREEN_MAX_HEIGHT))
            {
                printk("%s Coor overflow:X=%d, Y=%d", __func__, x, y);
                continue;
            }

			ts->finger[i].curr_press = 1;
			ts->finger[i].x = x;
			ts->finger[i].y = y;
		}
    	
    }
	else
	{
		key = touch_data[3]&0x0f;
	}

	if (touch_num != 0)
	{
		for (i=0; i<GT811_MAX_FINGER_NUM; i++)
		{
			if (ts->finger[i].last_press)
			{
				first_touch = 0;
				break;
			}
		}
	}
    for (i=0; i<GT811_MAX_FINGER_NUM; i++)
    {
        if (ts->finger[i].curr_press)
        {
            point_down = 1;
            input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,1);
			if (first_touch)
			{
				first_touch = 0;
				input_report_abs(ts->input_dev, ABS_PRESSURE, GT811_PRESSURE_MAX);
				input_report_key(ts->input_dev, BTN_TOUCH, 1);		
			}
            input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->finger[i].x);
            input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->finger[i].y);
            input_mt_sync(ts->input_dev);
        }
        else
        {
            if (ts->finger[i].last_press != 0)
            {
                point_up = 1;
            }
        }

        ts->finger[i].last_press = ts->finger[i].curr_press;
    }

    if (!point_down && point_up)
    {
		input_report_abs(ts->input_dev, ABS_PRESSURE, 0);
		input_report_key(ts->input_dev, BTN_TOUCH, 0);		
        input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
        input_mt_sync(ts->input_dev);
    }

    for (i=0; i<GT811_KEY_MAX_NUM; i++)
    {
        if (gt811_key_info[i].key_id & key) // key pressed
        {
            gt811_ts_key_down(ts->input_dev, i);
        }
        else // key up
        {
            gt811_ts_key_up(ts->input_dev, i);
        }
    }

    input_sync(ts->input_dev);

XFER_ERROR:
    i2c_end_cmd(ts);

TP_ABNORMAL:
    enable_irq(ts->client->irq);

}

static irqreturn_t gt811_ts_isr(int irq, void *dev_id)
{
    struct gt811_ts_data *ts = dev_id;

    disable_irq_nosync(ts->client->irq);
    queue_work(gt811_wq, &ts->work);

    return IRQ_HANDLED;
}

static int gt811_ts_power(struct gt811_ts_data * ts, int on)
{
    GT811_PRINT(("gt811_ts_power, on = %d\n", on));

    switch (on)
    {
    case 0:
        GT811_TS_SUSPEND;
        return 0;

    case 1:
        GT811_TS_WORK;
        msleep(100);
        return 0;

    default:
        GT811_PRINT(("%s: Cant't support this command.", gt811_ts_name));
        return -EINVAL;
    }

}

static int gt811_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    int retry=0;
    struct gt811_ts_data *ts;
    int i;

    printk("%s...\n", __func__);

    p_client_gt811 = client;

    //Check I2C function
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        dev_err(&client->dev, "Must have I2C_FUNC_I2C.\n");
        ret = -ENODEV;
        goto err_check_functionality_failed;
    }


    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if (ts == NULL)
    {
        ret = -ENOMEM;
        goto err_alloc_data_failed;
    }

    for (i=0; i<GT811_MAX_FINGER_NUM; i++)
    {
        ts->finger[i].last_press = 0;
    }

    gt811_i2c_client = client;
	ts->client = client;

    msleep(50);
    GT811_TS_SUSPEND;
    msleep(10);
    GT811_TS_WORK;
    msleep(50);

    for (retry=0; retry < 3; retry++)
    {
        ret = i2c_end_cmd(ts);
        if (ret > 0)
            break;
    }

    if (ret <= 0)
    {
        dev_err(&client->dev, "Warnning: I2C communication might be ERROR!\n");
        goto err_i2c_failed;
    }

    INIT_WORK(&ts->work, gt811_ts_work_func);
    ts->client = client;
    i2c_set_clientdata(client, ts);

    ts->input_dev = input_allocate_device();
    if (ts->input_dev == NULL)
    {
        ret = -ENOMEM;
        dev_dbg(&client->dev,"gt811_ts_probe: Failed to allocate input device\n");
        goto err_input_dev_alloc_failed;
    }

    set_bit(ABS_MT_TOUCH_MAJOR, ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_X,  ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y,  ts->input_dev->absbit);
    set_bit(EV_KEY,			 ts->input_dev->evbit);
    set_bit(EV_ABS,			 ts->input_dev->evbit);
    set_bit(BTN_TOUCH,          ts->input_dev->keybit);
    set_bit(INPUT_PROP_DIRECT,  input_dev->propbit);

    /* Enable all supported keys */
    for (retry = 0; retry < GT811_KEY_MAX_NUM; retry++)
    {
        __set_bit(gt811_key_info[retry].key_value, ts->input_dev->keybit);

    }
    __clear_bit(KEY_RESERVED, ts->input_dev->keybit);

    input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, GT811_SCREEN_MAX_WIDTH, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, GT811_SCREEN_MAX_HEIGHT, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, GT811_PRESSURE_MAX, 0, 0);

    sprintf(ts->phys, "input/ts");
    ts->input_dev->name = gt811_ts_name;
    ts->input_dev->phys = ts->phys;
    ts->input_dev->id.bustype = BUS_I2C;
    ts->input_dev->id.vendor = 0xDEAD;
    ts->input_dev->id.product = 0xBEEF;
    ts->input_dev->id.version = 10427;

    ret = input_register_device(ts->input_dev);
    if (ret)
    {
        dev_err(&client->dev,"Probe: Unable to register %s input device\n", ts->input_dev->name);
        goto err_input_register_device_failed;
    }
    client->irq = TS_INT;
    if (client->irq)
    {

        ret  = request_irq(client->irq, gt811_ts_isr ,  IRQ_TYPE_EDGE_RISING,
                           client->name, ts);
        if (ret != 0)
        {
            dev_err(&client->dev,"Cannot allocate ts INT!ERRNO:%d\n", ret);
            goto err_request_irq_failed;
        }
        else
        {
            disable_irq(client->irq);
            dev_dbg(&client->dev,"Reques EIRQ %d succesd on GPIO:%d\n",TS_INT, TS_INT_PORT);
        }
    }

    flush_workqueue(gt811_wq);

    for (retry=0; retry<3; retry++)
    {
        ret = gt811_init_panel(ts);
        if (ret != 0)	 //Initiall failed
            continue;
        else
            break;
    }

    if (ret != 0)
    {
        goto err_init_godix_ts;
    }

    ts->power = gt811_ts_power;
    gt811_read_version(ts);

    i2c_end_cmd(ts);

#ifdef CONFIG_HAS_EARLYSUSPEND
    ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    ts->early_suspend.suspend = gt811_ts_early_suspend;
    ts->early_suspend.resume = gt811_ts_late_resume;
    register_early_suspend(&ts->early_suspend);
#endif

    enable_irq(client->irq);
    msm_tp_set_found_flag(1);
	printk("%s, ok, I2C addr = 0x%x\n", __func__, client->addr);
    return 0;

err_init_godix_ts:
    i2c_end_cmd(ts);
    free_irq(client->irq, ts);

err_request_irq_failed:
err_input_register_device_failed:
    input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
    i2c_set_clientdata(client, NULL);

err_i2c_failed:
    kfree(ts);

err_alloc_data_failed:
err_check_functionality_failed:
	printk("%s, failed\n", __func__);
    return ret;
}

static int gt811_ts_remove(struct i2c_client *client)
{
    struct gt811_ts_data *ts = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&ts->early_suspend);
#endif
    gpio_free(TS_INT_PORT);
    free_irq(client->irq, ts);

    dev_notice(&client->dev,"The driver is removing...\n");
    i2c_set_clientdata(client, NULL);
    input_unregister_device(ts->input_dev);
    kfree(ts);
    return 0;
}

static int gt811_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
    int ret;
    struct gt811_ts_data *ts = i2c_get_clientdata(client);


    GT811_PRINT(("gt811_ts_suspend\n"));

    disable_irq(client->irq);

    ret = cancel_work_sync(&ts->work);

    if (ret) //irq was disabled twice.
        enable_irq(client->irq);

    if (ts->power)
    {
        ret = ts->power(ts, 0);
        if (ret < 0)
            GT811_PRINT(("gt811_ts_resume power off failed\n"));
    }
    return 0;
}

static int gt811_ts_resume(struct i2c_client *client)
{
    int ret;
    struct gt811_ts_data *ts = i2c_get_clientdata(client);


    GT811_PRINT(("gt811_ts_resume\n"));
    if (ts->power)
    {
        ret = ts->power(ts, 1);
        if (ret < 0)
            GT811_PRINT(("gt811_ts_resume power on failed\n"));
    }

    ret=gt811_init_panel(ts);

    enable_irq(client->irq);

    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gt811_ts_early_suspend(struct early_suspend *h)
{
    struct gt811_ts_data *ts;
    ts = container_of(h, struct gt811_ts_data, early_suspend);
    gt811_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void gt811_ts_late_resume(struct early_suspend *h)
{
    struct gt811_ts_data *ts;
    ts = container_of(h, struct gt811_ts_data, early_suspend);
    gt811_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id gt811_ts_id[] =
{
    { GT811_I2C_NAME, 0 },
    { }
};

static struct i2c_driver gt811_ts_driver =
{
    .probe 	 = gt811_ts_probe,
    .remove	 = gt811_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend	 = gt811_ts_suspend,
    .resume	 = gt811_ts_resume,
#endif
    .id_table	 = gt811_ts_id,
    .driver = {
        .name	 = GT811_I2C_NAME,
        .owner = THIS_MODULE,
    },
};

static int __devinit gt811_ts_init(void)
{
    int ret;
    if (msm_tp_get_found_flag())
    {
        return -1;
    }

    /* create a work queue and worker thread */
    gt811_wq = create_workqueue("gt811_wq");
    if (!gt811_wq)
    {
        GT811_PRINT(("creat workqueue faiked\n"));
        return -ENOMEM;

    }
    ret = i2c_add_driver(&gt811_ts_driver);

    return ret;
}

static void __exit gt811_ts_exit(void)
{
    GT811_PRINT(("GT811 Touchscreen driver exited.\n"));
    i2c_del_driver(&gt811_ts_driver);
    if (gt811_wq)
        destroy_workqueue(gt811_wq);		 //release our work queue
}

late_initcall(gt811_ts_init);
module_exit(gt811_ts_exit);

MODULE_DESCRIPTION("GT811 Touchscreen Driver");
MODULE_LICENSE("GPL");


