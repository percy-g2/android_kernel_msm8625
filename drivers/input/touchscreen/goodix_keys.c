/*---------------------------------------------------------------------------------------------------------
 * driver/input/keyboard/goodix_keys.c
 *
 * Copyright(c) 2010 Goodix Technology Corp.     
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
 *---------------------------------------------------------------------------------------------------------*/
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

#define GOODIX_KEYS_DEBUG 0
#define GOODIX_KEYS_VERSION_NEW

#define GOODIX_KEYS_MODNAME          "goodix-keys"
#define GOODIX_KEYS_MAX              3
#define GOODIX_KEYS_READ_BYTES       2

#ifdef GOODIX_KEYS_VERSION_NEW
#define GOODIX_KEYS_KEY_MENU         3
#define GOODIX_KEYS_KEY_HOME         4
#define GOODIX_KEYS_KEY_BACK         5
#else
#define GOODIX_KEYS_KEY_BACK         3
#define GOODIX_KEYS_KEY_MENU         4
#define GOODIX_KEYS_KEY_HOME         5
#endif

#define GOODIX_KEYS_TIMER_DELAY     (HZ/20)
#define GOODIX_TRIGGER_TYPE_IRQ     1
#define GOODIX_TRIGGER_TYPE_TIMER   2

static const unsigned int goodix_keys_code[GOODIX_KEYS_MAX] = {
	[0] = KEY_BACK, /* Touch Key 3, right */
	[1] = KEY_MENU, /* Touch Key 4, left  */
	[2] = KEY_HOME, /* Touch Key 5, mid   */
};

struct goodix_keys_data {
	int retry;
	int bad_data;
	int panel_type;
	char phys[32];		
	struct i2c_client *client;
	struct input_dev *input_dev;
	uint32_t gpio_irq;
	struct work_struct  work_irq;
	struct work_struct  work_timer;
	struct early_suspend early_suspend;
	int home_key_pressed;
	int timer_started;
	struct timer_list timer;
	
	int (*power)(struct goodix_keys_data * keys, int on);
};

static struct workqueue_struct * goodix_keys_wq;
const char *goodix_keys_name = "Goodix Keys";
static struct goodix_keys_data * goodix_keys_data_g = NULL;

	
#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_keys_early_suspend(struct early_suspend *h);
static void goodix_keys_late_resume(struct early_suspend *h);
#endif

static void goodix_keys_timer_func(unsigned long data);
static void goodix_keys_work_timer(struct work_struct *work);
static void goodix_keys_work_irq(struct work_struct *work);
static void goodix_keys_stop_timer(void);
static void goodix_keys_start_timer(void);
static void goodix_keys_read_key(struct work_struct * work);


static void goodix_keys_stop_timer(void)
{
	if (goodix_keys_data_g->timer_started)
	{
		del_timer(&goodix_keys_data_g->timer);
		goodix_keys_data_g->timer_started = 0;
	}
}

static void goodix_keys_start_timer(void)
{
	if (NULL == goodix_keys_data_g)
		return;

	goodix_keys_stop_timer();
	init_timer(&goodix_keys_data_g->timer);
	goodix_keys_data_g->timer.expires = jiffies + GOODIX_KEYS_TIMER_DELAY;
	goodix_keys_data_g->timer.function = goodix_keys_timer_func;
	goodix_keys_data_g->timer.data = (unsigned long) 0; 
	add_timer(&goodix_keys_data_g->timer);
	goodix_keys_data_g->timer_started = 1;
}


static int goodix_keys_i2c_read_bytes(struct i2c_client *client, uint8_t *buf, int len)
{
	int ret=-1;

	struct i2c_msg msg;

	msg.flags=I2C_M_RD;
	msg.addr=client->addr;
	msg.len=len;
	msg.buf=buf;
	
	ret=i2c_transfer(client->adapter,&msg,1);

	return ret;
}


static int goodix_keys_i2c_write_bytes(struct i2c_client *client,uint8_t *data,int len)
{
	struct i2c_msg msg;
	int ret=-1;

	msg.flags=!I2C_M_RD;
	msg.addr=client->addr;
	msg.len=len;
	msg.buf=data;		
	
	ret=i2c_transfer(client->adapter,&msg,1);
	return ret;
}


static int goodix_keys_init_keys(struct goodix_keys_data *keys)
{
	int ret = -1;
	int count;
	//There are some demo configs. May be changed as some different panels.

	uint8_t config_info[] = {
		0x88, 0x0c, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x0,  0x0,
		0xf8, 0x0,  0x85, 0x0,  0x2,  0x66, 0x3,  0xb4};

	for(count = 5; count > 0; count--)
	{
		ret = goodix_keys_i2c_write_bytes(keys->client,config_info, sizeof(config_info));	
		if(ret == 1)		//Initiall success
			break;
		else
			msleep(10);
	}					 	 
	
	return ret==1 ? 0 : ret;
}

static void goodix_keys_read_key(struct work_struct * work)
{
	uint8_t point_data[GOODIX_KEYS_READ_BYTES+1] = { 0 };
	int ret = -1; 
 	
	struct goodix_keys_data *keys = goodix_keys_data_g;

	if (keys == NULL)
		return;

	ret=goodix_keys_i2c_read_bytes(keys->client, point_data, sizeof(point_data));
	if(ret <= 0)	
	{
		if (keys->home_key_pressed)
		{
			keys->home_key_pressed = 0;
			goodix_keys_stop_timer();
			input_report_key(keys->input_dev,  goodix_keys_code[2], 0);
			input_sync(keys->input_dev);
		}
	
		//printk(KERN_ERR "goodix_keys, read nothing");
		//dev_err(&(keys->client->dev),"goodix, I2C transfer error. ERROR Number:%d\n ", ret);
		keys->retry++;
		if(keys->power)
		{
			keys->power(keys, 0);
			keys->power(keys, 1);
		}
		else
		{
			//goodix_keys_init_keys(keys);
			//msleep(260);
		}
		return;
	}


#if GOODIX_KEYS_DEBUG
	printk(KERN_ERR  "goodix_keys, keys->input 0x%p, point_data, [0] = 0x%x, [1] = 0x%x, [2] = 0x%x\n", 
		keys->input_dev, point_data[0], point_data[1], point_data[2]);
#endif


	if (point_data[0] != GOODIX_KEYS_KEY_HOME)
	{
		if (keys->home_key_pressed)
		{
			keys->home_key_pressed = 0;
			goodix_keys_stop_timer();
			input_report_key(keys->input_dev,  goodix_keys_code[2], 0);
			input_sync(keys->input_dev);
		}
	}

	switch (point_data[0])
	{
	case GOODIX_KEYS_KEY_BACK:
 		input_report_key(keys->input_dev,	 goodix_keys_code[0], 1);
		input_sync(keys->input_dev);
		input_report_key(keys->input_dev,	 goodix_keys_code[0], 0);
		input_sync(keys->input_dev);
		break;

	case GOODIX_KEYS_KEY_MENU:
 		input_report_key(keys->input_dev,	 goodix_keys_code[1], 1);
		input_sync(keys->input_dev);
		input_report_key(keys->input_dev,	 goodix_keys_code[1], 0);
		input_sync(keys->input_dev);
		break;

	case GOODIX_KEYS_KEY_HOME:
		if (!keys->home_key_pressed)
		{
			keys->home_key_pressed = 1;
			input_report_key(keys->input_dev,	 goodix_keys_code[2], 1);
			input_sync(keys->input_dev);
		}
		goodix_keys_start_timer();
		
		break;
			
	default:
		
		break;
	}

}

static void goodix_keys_work_timer(struct work_struct *work)
{	
	goodix_keys_read_key(work);
}


static void goodix_keys_work_irq(struct work_struct *work)
{	
	goodix_keys_read_key(work);
	if (goodix_keys_data_g != NULL)
		enable_irq(goodix_keys_data_g->client->irq);
}


static void goodix_keys_timer_func(unsigned long data)
{
	if (goodix_keys_data_g != NULL)
	{
		goodix_keys_data_g->timer_started = 0;
		queue_work(goodix_keys_wq, &goodix_keys_data_g->work_timer);
	}
	
}

static irqreturn_t goodix_keys_irq_handler(int irq, void *dev_id)
{
	struct goodix_keys_data *keys = dev_id;

#if GOODIX_KEYS_DEBUG
	printk(KERN_ERR  "goodix_keys, irq");
#endif
	
	if (goodix_keys_data_g != NULL)
	{
		disable_irq_nosync(keys->client->irq);
		queue_work(goodix_keys_wq, &keys->work_irq);
	}
	
	return IRQ_HANDLED;
}

static int goodix_keys_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct goodix_keys_data *keys;
	int ret = 0;
	int retry=0;
	int i;

#if GOODIX_KEYS_DEBUG
	printk(KERN_ERR  "goodix_keys, probe");
#endif
	
	dev_dbg(&client->dev,"Install goodix_keys driver for guitar.\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		dev_err(&client->dev, "goodix, System need I2C function.\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	
	keys = (struct goodix_keys_data *)kzalloc(sizeof(*keys), GFP_KERNEL);
	if (keys == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	goodix_keys_data_g = keys;

	for(retry=0; retry < 5; retry++)
	{
		ret =goodix_keys_i2c_write_bytes(client, NULL, 0);
		if (ret > 0)
			break;
	}
	if(ret < 0)
	{
		dev_err(&client->dev, "Warnning: I2C connection might be something wrong!\n");
		goto err_i2c_failed;
	}
	
	INIT_WORK(&keys->work_timer, goodix_keys_work_timer);
	INIT_WORK(&keys->work_irq, goodix_keys_work_irq);
	keys->client = client;
	i2c_set_clientdata(client, keys);
	
	keys->input_dev = input_allocate_device();
	
	/*
	printk(KERN_ERR  "goodix_keys, Proble, keys->input 0x%x\n", keys->input_dev);
	*/
	
	if (keys->input_dev == NULL) {
		ret = -ENOMEM;
		dev_dbg(&client->dev,"Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}

	set_bit(EV_KEY, keys->input_dev->evbit);
	//set_bit(EV_REP, keys->input_dev->evbit);

	/* Enable all supported keys */
	for (i = 0; i < GOODIX_KEYS_MAX ; i++)
		__set_bit(goodix_keys_code[i], keys->input_dev->keybit);

	__clear_bit(KEY_RESERVED, keys->input_dev->keybit);

	sprintf(keys->phys, "input/goodix_keys)");
	keys->input_dev->name = goodix_keys_name;
	keys->input_dev->phys = keys->phys;
	keys->input_dev->id.bustype = BUS_I2C;
	keys->input_dev->id.vendor = 0xDEAD;
	keys->input_dev->id.product = 0xBEEF;
	keys->input_dev->id.version = 0x1103;	

	ret = input_register_device(keys->input_dev);
	if (ret) {
		dev_err(&client->dev,"goodix, Probe: Unable to register %s input device\n", keys->input_dev->name);
		goto err_input_register_device_failed;
	}

	keys->retry = 0;
	keys->bad_data = 0;

	keys->home_key_pressed = 0;
	keys->timer_started = 0;

	keys->gpio_irq = TS_INT_PORT;
	client->irq = TS_INT;
			
	ret  = request_irq(client->irq, goodix_keys_irq_handler ,  IRQF_TRIGGER_RISING,
		client->name, keys);
	if (ret != 0) {
		dev_err(&client->dev,"goodix, Can't allocate keys's interrupt!ERRNO:%d\n", ret);
		gpio_direction_input(keys->gpio_irq);
		gpio_free(keys->gpio_irq);
		goto err_int_request_failed;
	}
	else 
	{	
		disable_irq(client->irq);
		dev_dbg(&client->dev,"goodix, Reques EIRQ %d succesd on GPIO:%d\n",client->irq, keys->gpio_irq);
	}

err_int_request_failed:	
	flush_workqueue(goodix_keys_wq);
	
	ret = goodix_keys_init_keys(keys);
	if(ret != 0) 
	{
		dev_err(&client->dev,"goodix, goodix_keys_init_keys return error1");
		goto err_init_godix_ts;
	}


	enable_irq(client->irq);
	
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	keys->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	keys->early_suspend.suspend = goodix_keys_early_suspend;
	keys->early_suspend.resume = goodix_keys_late_resume;
	register_early_suspend(&keys->early_suspend);
#endif
	dev_err(&client->dev,"goodix, %s Started\n", keys->input_dev->name);
	return 0;

err_init_godix_ts:
	free_irq(client->irq,keys);	
	gpio_free(keys->gpio_irq);

err_input_register_device_failed:
	input_free_device(keys->input_dev);

err_input_dev_alloc_failed:
	i2c_set_clientdata(client, NULL);
err_i2c_failed:
	kfree(keys);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}


static int goodix_keys_remove(struct i2c_client *client)
{
	struct goodix_keys_data *keys = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&keys->early_suspend);
#endif
	free_irq(client->irq, keys);	
	gpio_free(keys->gpio_irq);

	dev_notice(&client->dev,"The driver is removing...\n");
	i2c_set_clientdata(client, NULL);
	input_unregister_device(keys->input_dev);
	if(keys->input_dev)
		kfree(keys->input_dev);
	kfree(keys);
	goodix_keys_data_g = NULL;
	return 0;
}

static int goodix_keys_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct goodix_keys_data *keys = i2c_get_clientdata(client);
	
	disable_irq(client->irq);

	ret = cancel_work_sync(&keys->work_irq);	
	if (ret)
		enable_irq(client->irq);
		
	if (keys->power) {
		ret = keys->power(keys,0);
		if (ret < 0)
			dev_warn(&client->dev, "%s power off failed\n", goodix_keys_name);
	}
	return 0;
}
static int goodix_keys_resume(struct i2c_client *client)
{
	int ret;
	struct goodix_keys_data *keys = i2c_get_clientdata(client);
	
	if (keys->power) {
		ret = keys->power(keys, 1);
		if (ret < 0)
			dev_warn(&client->dev, "%s power on failed\n", goodix_keys_name);
	}

	enable_irq(client->irq);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_keys_early_suspend(struct early_suspend *h)
{
	struct goodix_keys_data *keys;
	keys = container_of(h, struct goodix_keys_data, early_suspend);
	goodix_keys_suspend(keys->client, PMSG_SUSPEND);
}

static void goodix_keys_late_resume(struct early_suspend *h)
{
	struct goodix_keys_data *keys;
	keys = container_of(h, struct goodix_keys_data, early_suspend);
	goodix_keys_resume(keys->client);
}
#endif

static const struct i2c_device_id goodix_keys_id[] = {
	{ GOODIX_KEYS_MODNAME, 0 },
	{ }
};

static struct i2c_driver goodix_keys_driver = {
	.probe		= goodix_keys_probe,
	.remove		= goodix_keys_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= goodix_keys_suspend,
	.resume		= goodix_keys_resume,
#endif
	.id_table	= goodix_keys_id,
	.driver = {
		.name	= GOODIX_KEYS_MODNAME,
		.owner = THIS_MODULE,
	},
};

static int __devinit goodix_keys_init(void)
{
	int ret;
	goodix_keys_wq = create_workqueue("goodix_keys_wq");
	if (!goodix_keys_wq) {
		printk(KERN_ALERT "Creat workqueue faiked\n");
		return -ENOMEM;
		
	}
	ret=i2c_add_driver(&goodix_keys_driver);
	return ret; 
}

static void __exit goodix_keys_exit(void)
{
	printk(KERN_DEBUG "%s is exiting...\n", goodix_keys_name);
	i2c_del_driver(&goodix_keys_driver);
	if (goodix_keys_wq)
		destroy_workqueue(goodix_keys_wq);
}


late_initcall(goodix_keys_init);
module_exit(goodix_keys_exit);

MODULE_DESCRIPTION("Goodix Keys Driver");
MODULE_LICENSE("GPL");

