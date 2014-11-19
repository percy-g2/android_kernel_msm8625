/*---------------------------------------------------------------------------------------------------------
 * driver/input/touchscreen/goodix_touch.c
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
 * Change Date: 2010.11.11, add point_queue's definiens.     
 *                             
 * Change Data: 2011.03.09, rewrite point_queue's definiens.  
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
#include "goodix_queue.h"

#define GOODIX_TOUCH_VERSION_NEW

#define TS_SWAP(a, b) do {unsigned int temp; temp = a; a = b; b = temp;} while (0)

#if 1
#define GPIO_SET_GOODIX_TS_SUSPEND gpio_set_value(TS_RESET_PORT, 0)
#define GPIO_SET_GOODIX_TS_WORK    gpio_set_value(TS_RESET_PORT, 1)
#else
#define GPIO_SET_GOODIX_TS_SUSPEND gpio_set_value(TS_RESET_PORT, 1)
#define GPIO_SET_GOODIX_TS_WORK    gpio_set_value(TS_RESET_PORT, 0)
#endif


#ifndef GUITAR_GT80X
#error The code does not match the hardware version.
#endif

static struct workqueue_struct *goodix_wq;

static struct point_queue  finger_list;	//record the fingers list 
/*************************************************/

const char *s3c_ts_name = "Goodix TouchScreen of GT80X";
/*used by guitar_update module */
struct i2c_client * i2c_connect_client = NULL;
EXPORT_SYMBOL(i2c_connect_client);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_ts_early_suspend(struct early_suspend *h);
static void goodix_ts_late_resume(struct early_suspend *h);
#endif

static int i2c_read_bytes(struct i2c_client *client, uint8_t *buf, int len)
{
	int ret=-1;

	struct i2c_msg msgs[2];
	msgs[0].flags=!I2C_M_RD;
	msgs[0].addr=client->addr;
	msgs[0].len=1;
	msgs[0].buf=&buf[0];

	msgs[1].flags=I2C_M_RD;
	msgs[1].addr=client->addr;
	msgs[1].len=len-1;
	msgs[1].buf=&buf[1];
	
	ret=i2c_transfer(client->adapter,msgs,2);
	return ret;
}


/*******************************************************	
功能ﺿFunction as i2c_master_send 
	向从机写数据
参数�?
	client:	i2c设备，包含设备地址
	buf[0]ﺿ首字节为写地址
	buf[1]~buf[len]：数据缓冲区
	lenﺿ数据长�?
return�?
	执行消息�?
*******************************************************/
static int i2c_write_bytes(struct i2c_client *client,uint8_t *data,int len)
{
	struct i2c_msg msg;
	int ret=-1;
	//发送设备地址
	msg.flags=!I2C_M_RD;//写消�?
	msg.addr=client->addr;
	msg.len=len;
	msg.buf=data;		
	
	ret=i2c_transfer(client->adapter,&msg,1);
	return ret;
}


/*******************************************************
功能�?
	Guitar初始化函数，用于发送配置信息，获取版本信息
参数�?
	ts:	client私有数据结构�?
return�?
	执行结果码，0表示正常执行
*******************************************************/
static int goodix_init_panel(struct goodix_ts_data *ts)
{
	int ret = -1;
	int count;
	//There are some demo configs. May be changed as some different panels.
#if 0
	uint8_t config_info[54] = {0x30, 0x19,0x05,0x03,0x28,0x02,0x14,0x14,0x10,0x50,
	                             0xB8, 0x14,0x00,0x1E,0x00,0x01,0x23,0x45,0x67,0x89,
	                             0xAB, 0xCD,0xE0,0x00,0x00,0x00,0x00,0x4D,0xC1,0x20,
	                             0x01, 0x01,0x83,0x50,0x3C,0x1E,0xB4,0x00,0x0A,0x50,
	                             0x82, 0x1E,0x00,0x6E,0x00,0x00,0x00,0x00,0x00,0x00,
	                             0x00, 0x00,0x00,0x01};
#endif

#if 0//GUITAR_CONFIG_43
	uint8_t config_info[54]={0x30,	0x19,  0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x50,
	                         0xB8,  TOUCH_MAX_WIDTH>>8,TOUCH_MAX_WIDTH&0xFF, TOUCH_MAX_HEIGHT>>8, TOUCH_MAX_HEIGHT&0xFF,0x01,0x23,0x45, 0x67,0x89,
	                         0xAB,0xCD,0xE1,0x00,0x00,0x00,0x00,0x05, 0xCF,0x20,
	                         0x07,0x0B,0x8B,0x50,0x3C,0x1E,0x28,0x00, 0x00,0x00,
	                         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,
	                         0x00,0x00,0x00,0x01};

#endif

#if 0 //TCL_5.0 inch
	uint8_t config_info[54]={0x30,	0x19,0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x40,0xB8,TOUCH_MAX_WIDTH>>8,TOUCH_MAX_WIDTH&0xFF,
									TOUCH_MAX_HEIGHT>>8,TOUCH_MAX_HEIGHT&0xFF,0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xE1,0x00,
									0x00,0x00,0x00,0x0D,0xCF,0x20,0x03,0x05,0x83,0x50,0x3C,0x1E,0x28,0x00,0x00,0x00,0x00,0x00,
									0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01};
#endif		
#ifdef GOODIX_TOUCH_VERSION_NEW
	uint8_t config_info[] = {
							0x30, 0x19, 0x05, 0x03, 0x28, 0x02, 0x14, 0x14, 
							0x10, 0x46, 0xB8, 0x14, 0x00, 0x1E, 0x00, 0x01,
							0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xE0, 0x00,
							0x00, 0x00, 0x00, 0x4D, 0xC1, 0x20, 0x01, 0x01,
							0x83, 0x50, 0x3C, 0x1E, 0xB4, 0x00, 0x0A, 0x50,
							0x82, 0x1E, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x01
	};
#else
	uint8_t config_info[] = {0x30, 0x19,0x05,0x03,0x28,0x02,0x14,0x14,0x10,0x50,
		                         0xB8, 0x14,0x00,0x1E,0x00,0xED,0xCB,0xA9,0x87,0x65,
		                         0x43, 0x21,0x01,0x00,0x00,0x00,0x00,0x4D,0xC1,0x20,
		                         0x01, 0x01,0x83,0x50,0x3C,0x1E,0xB4,0x00,0x0A,0x50,
		                         0x82, 0x1E,0x00,0x6E,0x00,0x00,0x00,0x00,0x00,0x00,
		                         0x00,0x00,0x00,0x01};
	
#endif
	for(count = 5; count > 0; count--)
	{
		ret = i2c_write_bytes(ts->client,config_info,sizeof(config_info));	
		if(ret == 1)		//Initiall success
			break;
		else
			msleep(10);
	}					 	 
	
	return ret==1 ? 0 : ret;
}

static int  goodix_read_version(struct goodix_ts_data *ts)
{
#define GT80X_VERSION_LENGTH	40	
	int ret;
	uint8_t version[2] = {0x69,0xff};	//command of reading Guitar's version 
	uint8_t version_data[GT80X_VERSION_LENGTH+1];		//store touchscreen version infomation
	memset(version_data+1, 0, GT80X_VERSION_LENGTH);
	ret=i2c_write_bytes(ts->client,version,2);
	if (ret < 0) 
	{
	    dev_err(&ts->client->dev,"goodix, goodix_read_version i2c_write_bytes error\n");
		goto error_i2c_version;
	}
	msleep(50);						//change: 16ms->50ms
	version_data[0] = 0x6A;
	ret=i2c_read_bytes(ts->client,version_data, GT80X_VERSION_LENGTH);
	if (ret < 0) 
	{
	    dev_err(&ts->client->dev,"goodix, goodix_read_version i2c_read_bytes error\n");
		goto error_i2c_version;
	}
	dev_dbg(&ts->client->dev, "goodix, Guitar Version: %s\n", version_data+1);
#if 0	
	{
		int i;
		for (i=0; i<GT80X_VERSION_LENGTH; i++)
		{
			printk(KERN_ERR"0x%02x ", version_data[i]);
		}
	}
#endif
	version[1] = 0x00;				//cancel the command
	i2c_write_bytes(ts->client, version, 2);
	return 0;
	
error_i2c_version:
	return ret;
}

/*******************************************************	
功能�?
	触摸屏工作函�?
	由中断触发，接受1组坐标数据，校验后再分析输出
参数�?
	ts:	client私有数据结构�?
return�?
	执行结果码，0表示正常执行
********************************************************/
static void goodix_ts_work_func(struct work_struct *work)
{	
	static uint8_t finger_bit=0;	//last time fingers' state
	struct point_data * p = NULL;
	uint8_t read_position = 0;
	uint8_t point_data[READ_BYTES_NUM]={ 0 };
	uint8_t finger=0;				//record which finger is changed
	uint8_t check_sum = 0;
	int ret = -1; 
	int count = 0;
	
	struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data, work);

	dev_dbg(&(ts->client->dev),"goodix goodix_ts_work_func");

	ret=i2c_read_bytes(ts->client, point_data, sizeof(point_data));
#if 0
	{
		int i;
		for (i=1; i<READ_BYTES_NUM-1; i++)
		{
			printk(KERN_ERR "[%02d] 0x%02x ", i, point_data[i]);
		}
	}
#endif	
	if(ret <= 0)	
	{
		dev_err(&(ts->client->dev),"goodix, I2C transfer error. ERROR Number:%d\n ", ret);
		ts->retry++;
		if(ts->power)
		{
			ts->power(ts, 0);
			ts->power(ts, 1);
		}
		else
		{
			goodix_init_panel(ts);
			msleep(260);
		}
		goto XFER_ERROR;
	}	
	
	//如果能够保证在INT中断后及时的读取坐标数据，可以不进行校验
	if(!ts->use_irq)	
	{
		switch(point_data[1]& 0x1f)
		{
		case 0:
			break;
		case 1:
			for(count=1; count<8; count++)
				check_sum += (int)point_data[count];
			read_position = 8;
			break;
		case 2:
		case 3:
			for(count=1; count<13;count++)
				check_sum += (int)point_data[count];
			read_position = 13;
			break;	
		default:		//(point_data[1]& 0x1f) > 3
			for(count=1; count<34;count++)
				check_sum += (int)point_data[count];
			read_position = 34;
		}
		if(check_sum != point_data[read_position])
			goto XFER_ERROR;
	}
	//The bits indicate which fingers pressed down
	point_data[1]&=0x1f;
	finger = finger_bit^point_data[1];
	if(finger == 0 && point_data[1] == 0)		
	{
		dev_dbg(&(ts->client->dev),"goodix, goto NO_ACTION1\n");
		goto NO_ACTION;						//no fingers and no action
	}
	else if(finger == 0)						//the same as last time
	{
		dev_dbg(&(ts->client->dev),"goodix, goto BIT_NO_CHANGE\n");
		goto BIT_NO_CHANGE;
	}
	
	dev_dbg(&(ts->client->dev),"goodix, call analyse_points\n");
	//check which point(s) DOWN or UP
	analyse_points(&finger_list, finger_bit, finger);

	if(finger_list.head == NULL)
	{
		dev_dbg(&(ts->client->dev),"goodix, goto NO_ACTION2\n");
		goto NO_ACTION;
	}
	else
		dev_dbg(&ts->client->dev, "fingers count:%d\n", finger_list.length);

BIT_NO_CHANGE:
	for(p = finger_list.head; p != NULL; p = p->next)
	{	
		if(p->state == FLAG_UP)
		{
			p->x = p->y = 0;
			p->pressure = 0;
			continue;
		}
		
		if(p->id < 3)
			read_position = p->id*5+3;
		else
			read_position = 29;

		
		if(p->id != 3)
		{
			p->x = (unsigned int) (point_data[read_position]<<8) + (unsigned int)( point_data[read_position+1]);
			p->y = (unsigned int)(point_data[read_position+2]<<8) + (unsigned int) (point_data[read_position+3]);
			p->pressure = point_data[read_position+4];
		}
		
		#if MAX_FINGER_NUM > 3
		else 
		{
			p->x = (unsigned int) (point_data[18]<<8) + (unsigned int)( point_data[25]);
			p->y = (unsigned int)(point_data[26]<<8) + (unsigned int) (point_data[27]);
			p->pressure = point_data[28];
		}
		#endif

		/* MAPPING to LCD, x is the short side, y is the long side, need to swap x, y*/
#ifdef GOODIX_TOUCH_VERSION_NEW		
#ifdef TS_LANDSCAPE
		p->x = p->x * SCREEN_MAX_HEIGHT / TOUCH_MAX_WIDTH;
		p->y = p->y * SCREEN_MAX_WIDTH / TOUCH_MAX_HEIGHT;
		TS_SWAP(p->x, p->y);
#endif

#ifdef TS_PORTRAIT
		p->x = (TOUCH_MAX_WIDTH - p->x) * SCREEN_MAX_WIDTH / TOUCH_MAX_WIDTH;
		p->y = p->y * SCREEN_MAX_HEIGHT / TOUCH_MAX_HEIGHT;
#endif		
		
#else
		p->x = (TOUCH_MAX_WIDTH - p->x) * SCREEN_MAX_HEIGHT / TOUCH_MAX_WIDTH;
		p->y = (TOUCH_MAX_HEIGHT - p->y) * SCREEN_MAX_WIDTH / TOUCH_MAX_HEIGHT;
		TS_SWAP(p->x, p->y);
#endif
	}

#ifndef GOODIX_MULTI_TOUCH	
		if(finger_list.head->state == FLAG_DOWN)
		{
			input_report_abs(ts->input_dev, ABS_X, finger_list.head->x);
			input_report_abs(ts->input_dev, ABS_Y, finger_list.head.y);	
			
		} 
		input_report_abs(ts->input_dev, ABS_PRESSURE, finger_list.head->pressure);
		input_report_key(ts->input_dev, BTN_TOUCH, finger_list.head->state);   
#else

	/* ABS_MT_TOUCH_MAJOR is used as ABS_MT_PRESSURE in android. */
	for(p = finger_list.head; p != NULL; p = p->next)
	{
		if(p->state == FLAG_DOWN)
		{
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, p->x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, p->y);
		} 
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, p->id);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, p->pressure);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, p->pressure);
		if (p->state == FLAG_DOWN)
			input_report_key(ts->input_dev, BTN_TOUCH, 1);		//Finger Down.
		else
			input_report_key(ts->input_dev, BTN_TOUCH, 0);		//Finger Down.
		input_mt_sync(ts->input_dev);	
	}

#endif
	input_sync(ts->input_dev);

	delete_points(&finger_list);
	finger_bit = point_data[1]&0x1f;	//restore last presse state.

XFER_ERROR:	
NO_ACTION:
	if(ts->use_irq)
	{
		enable_irq(ts->client->irq);
	}

}

/*******************************************************	
功能�?
	计时器响应函�?
	由计时器触发，调度触摸屏工作函数运行；之后重新计�?
参数�?
	timer：函数关联的计时�?
return�?
	计时器工作模式，HRTIMER_NORESTART表示不需要自动重�?
********************************************************/
static enum hrtimer_restart goodix_ts_timer_func(struct hrtimer *timer)
{
	struct goodix_ts_data *ts = container_of(timer, struct goodix_ts_data, timer);

	queue_work(goodix_wq, &ts->work);
	if(ts->timer.state != HRTIMER_STATE_INACTIVE)
		hrtimer_start(&ts->timer, ktime_set(0, 16000000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

/*******************************************************	
功能�?
	中断响应函数
	由中断触发，调度触摸屏处理函数运�?
参数�?
	timer：函数关联的计时�?
return�?
	计时器工作模式，HRTIMER_NORESTART表示不需要自动重�?
********************************************************/
#if defined(TS_INT_PORT)
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;
//	printk(KERN_ERR  "goodix isr\n");
	
	disable_irq_nosync(ts->client->irq);
	queue_work(goodix_wq, &ts->work);
	
	return IRQ_HANDLED;
}
#endif

/*******************************************************	
功能�?
	GT80X的电源管�?
参数�?
	on:设置GT80X运行模式ﺿ为进入Sleep模式
return�?
	是否设置成功，小丿表示设置失�?
********************************************************/
#if defined(TS_RESET_PORT)
static int goodix_ts_power(struct goodix_ts_data * ts, int on)
{
	int ret = 0;
	if(ts == NULL || (ts && !ts->use_shutdown))
		return -1;

	switch(on) 
	{
	case 0:
		GPIO_SET_GOODIX_TS_SUSPEND;
		break;
	case 1:
		GPIO_SET_GOODIX_TS_WORK;
		break;	
	}
	dev_dbg(&ts->client->dev, "Set Guitar's Shutdown %s. Ret:%d.\n", on?"HIGH":"LOW", ret);
	return ret;
}
#endif


/*******************************************************	
功能�?
	触摸屏探测函�?
	在注册驱动时调用（要求存在对应的client）；
	用于IO,中断等资源申请；设备注册；触摸屏初始化等工作
参数�?
	client：待驱动的设备结构体
	id：设备ID
return�?
	执行结果码，0表示正常执行
********************************************************/
static int goodix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct goodix_ts_data *ts;
	int ret = 0;
	int retry=0;
//	int count=0;

	struct goodix_i2c_platform_data *pdata;
	dev_dbg(&client->dev,"Install touchscreen driver for guitar.\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		dev_err(&client->dev, "goodix, System need I2C function.\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	
	ts = (struct goodix_ts_data *)kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	
	/* 获取预定义资�?/
	pdata = client->dev.platform_data;
	if(pdata != NULL) {
		//ts->gpio_shutdown = pdata->gpio_shutdown;
		//ts->gpio_irq = pdata->gpio_irq;
		
		//use as s3c_gpio_cfgpin(ts->gpio_shutdown, pdata->shutdown_cfg);		/* output */
	}
	
#ifdef TS_RESET_PORT	
	ts->gpio_shutdown = TS_RESET_PORT;
    if (ts->gpio_shutdown)
	{
		GPIO_SET_GOODIX_TS_WORK;
		ts->use_shutdown = 1;
		msleep(25);		//waiting for initialization of Guitar.
	}
#endif		
	
	i2c_connect_client = client;				//used by Guitar Updating.
	for(retry=0; retry < 5; retry++)
	{
		ret =i2c_write_bytes(client, NULL, 0);	//Test i2c.
		if (ret > 0)
			break;
	}
	if(ret < 0)
	{
		dev_err(&client->dev, "Warnning: I2C connection might be something wrong!\n");
		goto err_i2c_failed;
	}

	if(ts->use_shutdown)
	{
		GPIO_SET_GOODIX_TS_SUSPEND;		//suspend
	}
	
	INIT_WORK(&ts->work, goodix_ts_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		dev_dbg(&client->dev,"Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}

	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
#ifndef GOODIX_MULTI_TOUCH	
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y);

	input_set_abs_params(ts->input_dev, ABS_X, 0, SCREEN_MAX_WIDTH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, SCREEN_MAX_HEIGHT, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);	
#else
	ts->input_dev->absbit[0] = BIT_MASK(ABS_MT_TRACKING_ID) |
		BIT_MASK(ABS_MT_TOUCH_MAJOR)| BIT_MASK(ABS_MT_WIDTH_MAJOR) |
  		BIT_MASK(ABS_MT_POSITION_X) | BIT_MASK(ABS_MT_POSITION_Y); 	// for android
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_WIDTH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_HEIGHT, 0, 0);	

	//input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MAX_FINGER_NUM-1, 0, 0);	
	
#endif	

	sprintf(ts->phys, "input/ts)");
	ts->input_dev->name = s3c_ts_name;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 0x1103;	

	finger_list.length = 0;
	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&client->dev,"goodix, Probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	ts->use_irq = 0;
	ts->retry = 0;
	ts->bad_data = 0;
#if defined(TS_INT_PORT)	
	ts->gpio_irq = TS_INT_PORT;
	client->irq=TS_INT; //If not define in client
	if (client->irq)
	{
		ts->use_irq = 1;
		
		ret  = request_irq(client->irq, goodix_ts_irq_handler ,  /*IRQ_TYPE_EDGE_RISING*/IRQF_TRIGGER_RISING,
			client->name, ts);
		if (ret != 0) {
			dev_err(&client->dev,"goodix, Can't allocate touchscreen's interrupt!ERRNO:%d\n", ret);
			gpio_direction_input(ts->gpio_irq);
			gpio_free(ts->gpio_irq);
			goto err_int_request_failed;
		}
		else 
		{	
			disable_irq(client->irq);
			ts->use_irq = 1;
			dev_dbg(&client->dev,"goodix, Reques EIRQ %d succesd on GPIO:%d\n",client->irq, ts->gpio_irq);
		}
	}
#endif

err_int_request_failed:	
	if (!ts->use_irq) 
	{
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = goodix_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

	flush_workqueue(goodix_wq);
	if(ts->use_shutdown)
	{		
		msleep(100);
	
//	    printk(KERN_ERR  "goodix, GPIO_SET_GOODIX_TS_WORK %d\n", client->irq);
		GPIO_SET_GOODIX_TS_WORK;	
	#ifdef TS_RESET_PORT	
		ts->power = goodix_ts_power;
	#endif
		msleep(30);
	}	

//	printk(KERN_ERR  "goodix, call goodix_read_version 0\n");
	goodix_read_version(ts);
	
	ret = goodix_init_panel(ts);
	if(ret != 0) 
	{
		dev_err(&client->dev,"goodix, goodix_init_panel return error1");
		goto err_init_godix_ts;
	}


	if(ts->use_irq)
	{
		printk(KERN_ERR  "goodix, call enable_irq %d\n", client->irq);
		enable_irq(client->irq);
	}
	
	printk(KERN_ERR  "goodix, call goodix_read_version %d\n", client->irq);
	goodix_read_version(ts);
	//msleep(500);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = goodix_ts_early_suspend;
	ts->early_suspend.resume = goodix_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	dev_err(&client->dev,"goodix, Start  %s in %s mode\n", 
		ts->input_dev->name, ts->use_irq ? "Interrupt" : "Polling");
	return 0;

err_init_godix_ts:
	if(ts->use_irq)
	{
		free_irq(client->irq,ts);	
		gpio_free(ts->gpio_irq);
	}

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
	i2c_set_clientdata(client, NULL);
err_i2c_failed:
	if(ts->use_shutdown)
	{
		gpio_direction_input(ts->gpio_shutdown);
		gpio_free(ts->gpio_shutdown);
	}
//err_gpio_request:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}


/*******************************************************	
功能�?
	驱动资源释放
参数�?
	client：设备结构体
return�?
	执行结果码，0表示正常执行
********************************************************/
static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	if (ts->use_irq)
	{
		free_irq(client->irq, ts);	
		gpio_free(ts->gpio_irq);
	}	
	else
		hrtimer_cancel(&ts->timer);
	
	if(ts->use_shutdown)
	{
		gpio_direction_input(ts->gpio_shutdown);
		gpio_free(ts->gpio_shutdown);
	}

	dev_notice(&client->dev,"The driver is removing...\n");
	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);
	if(ts->input_dev)
		kfree(ts->input_dev);
	kfree(ts);
	return 0;
}

//停用设备
static int goodix_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	
	if (ts->use_irq)
		disable_irq(client->irq);
	else if(ts->timer.state)
		hrtimer_cancel(&ts->timer);
	ret = cancel_work_sync(&ts->work);	
	if(ret && ts->use_irq)		//irq was disabled twice.
		enable_irq(client->irq);
		
	if (ts->power) {
		ret = ts->power(ts,0);
		if (ret < 0)
			dev_warn(&client->dev, "%s power off failed\n", s3c_ts_name);
	}
	return 0;
}
//重新唤醒
static int goodix_ts_resume(struct i2c_client *client)
{
	int ret;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	
	if (ts->power) {
		ret = ts->power(ts, 1);
		if (ret < 0)
			dev_warn(&client->dev, "%s power on failed\n", s3c_ts_name);
	}

	goodix_init_panel(ts);
	msleep(120);
	if (ts->use_irq)
		enable_irq(client->irq);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_ts_early_suspend(struct early_suspend *h)
{
	struct goodix_ts_data *ts;
	ts = container_of(h, struct goodix_ts_data, early_suspend);
	goodix_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void goodix_ts_late_resume(struct early_suspend *h)
{
	struct goodix_ts_data *ts;
	ts = container_of(h, struct goodix_ts_data, early_suspend);
	goodix_ts_resume(ts->client);
}
#endif

//可用于该驱动瘿设备名—设备ID 列表
//only one client
static const struct i2c_device_id goodix_ts_id[] = {
	{ GOODIX_I2C_NAME, 0 },
	{ }
};

//设备驱动结构�?
static struct i2c_driver goodix_ts_driver = {
	.probe		= goodix_ts_probe,
	.remove		= goodix_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= goodix_ts_suspend,
	.resume		= goodix_ts_resume,
#endif
	.id_table	= goodix_ts_id,
	.driver = {
		.name	= GOODIX_I2C_NAME,
		.owner = THIS_MODULE,
	},
};

/*******************************************************	
功能�?
	驱动加载函数
return�?
	执行结果码，0表示正常执行
********************************************************/
static int __devinit goodix_ts_init(void)
{
	int ret;
	goodix_wq = create_workqueue("goodix_wq");
	if (!goodix_wq) {
		printk(KERN_ALERT "Creat workqueue faiked\n");
		return -ENOMEM;
		
	}
	ret=i2c_add_driver(&goodix_ts_driver);
	return ret; 
}

/*******************************************************	
功能�?
	驱动卸载函数
参数�?
	client：设备结构体
********************************************************/
static void __exit goodix_ts_exit(void)
{
	printk(KERN_DEBUG "%s is exiting...\n", s3c_ts_name);
	i2c_del_driver(&goodix_ts_driver);
	if (goodix_wq)
		destroy_workqueue(goodix_wq);
}

late_initcall(goodix_ts_init);
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Driver");
MODULE_LICENSE("GPL");
