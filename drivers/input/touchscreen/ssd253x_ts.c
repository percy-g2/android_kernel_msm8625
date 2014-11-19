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

//#define SSD_DEBUG

#ifdef SSD_DEBUG
#define SSD_DBG(x) printk x
#else
#define SSD_DBG(x)
#endif

#define SSD_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define DEVICE_ID_REG               2
#define VERSION_ID_REG              3
#define AUTO_INIT_RST_REG           68
#define EVENT_STATUS                121
#define EVENT_MSK_REG               122
#define IRQ_MSK_REG                 123
#define FINGER01_REG                124
#define EVENT_STACK                 128
#define EVENT_FIFO_SCLR             135
#define TIMESTAMP_REG               136
#define SELFCAP_STATUS_REG          185

struct ChipSetting
{
    char No;
    char Reg;
    char Data1;
    char Data2;
};

/**************************************************************
使用前注意通道数，驱动默认使用通道是sense
大于drive否则需要将使用到的DRIVENO与SENSENO调换
此情况包括0x66和0x67寄存器，但不必修改。
***************************************************************/
#define GPIO_SET_SSD_TS_SUSPEND gpio_set_value(TS_RESET_PORT, 1)
#define GPIO_SET_SSD_TS_WORK    gpio_set_value(TS_RESET_PORT, 0)

#define DRIVENO                15
#define SENSENO                10
#define EdgeDisable            1    // if Edge Disable, set it to 1, else reset to 0
#define MicroTimeTInterupt    20000000// 100Hz - 10,000,000us
#define SSD253X_MAX_FINGER_NUM 5
#define SSD253X_PRESSURE_MAX   255
#define SSD253X_FINGER_MASK   0x1F

#define SCREEN_MAX_X           480
#define SCREEN_MAX_Y           800

#define USE_CUT_EDGE    //0x8b must be 0x00;  EdgeDisable set 0

#ifdef USE_CUT_EDGE
#define YPOS_MAX (DRIVENO - EdgeDisable) *64
#define XPOS_MAX (SENSENO - EdgeDisable) *64
#endif

static int ssd253x_write_cmd(struct i2c_client *client,unsigned char Reg,unsigned char Data1,unsigned char Data2,int ByteNo);
static void ssd253x_write_cmd_list(struct i2c_client *client, struct ChipSetting *p, int length);
static void ssd253x_device_reset(struct i2c_client *client);
static void ssd253x_device_resume(struct i2c_client *client);
static void ssd253x_device_suspend(struct i2c_client *client);
static void ssd253x_device_init(struct i2c_client *client);
static int  ssd253x_ts_suspend(struct i2c_client *client, pm_message_t mesg);
static int  ssd253x_ts_resume(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssd253x_ts_early_suspend(struct early_suspend *h);
static void ssd253x_ts_late_resume(struct early_suspend *h);
#endif /* CONFIG_HAS_EARLYSUSPEND */

static irqreturn_t ssd253x_ts_isr(int irq, void *dev_id);
static void ssd253x_isr_stop_timer(void);
static void ssd253x_isr_start_timer(void);
static void ssd253x_isr_timer_func(unsigned long data);
static void ssd253x_calkey_init_timer(void);
static void ssd253x_calkey_do_timer(unsigned long data);

// SSD2533 Setting
struct ChipSetting ssd253x_cmd_cfg_table[]=
{
	{0, 0x04, 0x00,	0x00},	//	  1
	{1,0x06,0x0e,0x00},
	{1,0x06,0x0e,0x00},
	{1,0x07,0x09,0x00},
	{2,0x08,0x00,0x26},
	{2,0x09,0x00,0x07},
	{2,0x0a,0x00,0x08},
	{2,0x0b,0x00,0x09},
	{2,0x0c,0x00,0x0a},
	{2,0x0d,0x00,0x0b},
	{2,0x0e,0x00,0x0c},
	{2,0x0f,0x00,0x0d},
	{2,0x10,0x00,0x0e},
	{2,0x11,0x00,0x0f},
	{2,0x12,0x00,0x10},
	{2,0x13,0x00,0x11},
	{2,0x14,0x00,0x12},
	{2,0x15,0x00,0x13},
	{2,0x16,0x00,0x14},
	{1,0x1f,0x00,0x00},
	{1,0xd5,0x04,0x00},
	{1,0xd8,0x01,0x00},
	{1,0x2a,0x07,0x00},
	{1,0x2c,0x01,0x00},
	{1,0x2e,0x0b,0x00},
	{1,0x2f,0x01,0x00},
	{1,0x30,0x03,0x00},
	{1,0x31,0x06,0x00},
	{1,0xd7,0x04,0x00},
	{1,0xdb,0x02,0x00},
	{2,0x33,0x00,0x00},
	{2,0x34,0x00,0x48},
	{2,0x35,0x00,0x20},
	{2,0x36,0x00,0x10},
	{1,0x37,0x00,0x00},
	{1,0x39,0x02,0x00},
	{1,0x3d,0x02,0x00},
	{1,0x53,0x16,0x00},
	{2,0x54,0x00,0x50},
	{2,0x55,0x00,0x50},
	{1,0x56,0x01,0x00},
	{1,0x5b,0x20,0x00},
	{2,0x5e,0x00,0x40},
	{1,0x40,0xf0,0x00},
	{1,0x44,0x01,0x00},
	{1,0x8a,0x05,0x00},
	{1,0x65,0x00,0x00},
	{2,0x66,0xff,0xf0},
	{2,0x67,0xff,0xf0},
	{2,0x7a,0xff,0xff},
	{2,0x7b,0x00,0x00},
	{1,0x8b,0x00,0x00},
	{1,0xab,0xa0,0x00},
	{1,0xac,0x01,0x00},
	{1,0xad,0x03,0x00},
	{1,0xae,0x0e,0x00},
	{1,0xaf,0x40,0x00},
	{1,0xb0,0x00,0x00},
	{1,0xbb,0x00,0x00},
	{1,0xbc,0x01,0x00},
	{1,0x25,0x0c,0x00},
};

struct ChipSetting ssd253x_cmd_reset[] =
{
    { 0, 0x04, 0x00, 0x00},    // SSD2533
};

struct ChipSetting ssd253x_cmd_resume[] =
{
    { 0, 0x04, 0x00, 0x00},    // SSD2533
    { 1, 0x25, 0x10, 0x00},    // Set Operation Mode   //Set from int setting
};

struct ChipSetting ssd253x_cmd_suspend[] =
{
    { 1, 0x25, 0x00, 0x00},    // Set Operation Mode
    { 0, 0x05, 0x00, 0x00},    // SSD2533
};

typedef struct _SSD253X_FINGER_INFO
{
    uint8_t last_press;
    uint8_t curr_press;
    uint16_t x;
    uint16_t y;
} SSD253X_FINGER_INFO;

struct ssl_ts_priv
{
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct hrtimer timeraa;
    struct work_struct  isr_work;
    struct work_struct calkey_work;
    struct timer_list calkey_timer;

	int isr_timer_started;
	struct timer_list   isr_timer_list;
	struct work_struct  isr_timer_work;

#ifdef    CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif

    int irq;
    int Resolution;

    SSD253X_FINGER_INFO finger[SSD253X_MAX_FINGER_NUM];
};

#define CAL_RANGE (0x40)

struct ssd253x_ts_key_def
{
    int key_value;
    char * key_name;
    int status;
    int key_id;
};

#define SSD253X_KEY_IDLE            0
#define SSD253X_KEY_PRESSED         1
#define SSD253X_KEY_MAX_NUM         SSD_ARRAY_SIZE(ssd253x_key_info)

static struct ssd253x_ts_key_def ssd253x_key_info[] =
{
    {KEY_SEARCH, "Search",  SSD253X_KEY_IDLE, 0x01},
    {KEY_MENU,	 "Menu",	SSD253X_KEY_IDLE, 0x02},
    {KEY_HOME,   "Home",	SSD253X_KEY_IDLE, 0x04},
	{KEY_BACK,	 "Back",	SSD253X_KEY_IDLE, 0x08},
};

static struct workqueue_struct *calkey_workq;
static unsigned short base_1,base_2,base_3,base_4;
static int calkey_timer_status = 0;
static struct workqueue_struct *ssd253x_wq = NULL;

static int ssd253x_internal_timer_inited = 0;
static struct ssl_ts_priv * p_ssl_ts_priv = NULL;

static int ssd253x_i2c_read_bytes(struct i2c_client *client, unsigned char reg, unsigned char *buf, int len)
{
    int ret=-1;

    struct i2c_msg msgs[2];
    msgs[0].flags = !I2C_M_RD;
    msgs[0].addr = client->addr;
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr = client->addr;
    msgs[1].len = len;
    msgs[1].buf = buf;

    ret = i2c_transfer(client->adapter,msgs,2);
    return ret;
}


static int ssd253x_i2c_write_bytes(struct i2c_client *client,unsigned char *data,int len)
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

static int ssd253x_write_cmd(struct i2c_client *client,unsigned char Reg,unsigned char Data1,unsigned char Data2,int ByteNo)
{
    unsigned char buf[4];

    buf[0]=Reg;
    buf[1]=Data1;
    buf[2]=Data2;
    buf[3]=0;

    return ssd253x_i2c_write_bytes(client, buf, ByteNo+1);
}

static int ssd253x_read_key_base(struct i2c_client *client,unsigned char fir_nor,unsigned short * data)
{
    int ret = 0;
    unsigned char buf[2];

    ret = ssd253x_write_cmd(client,0xBD,fir_nor,0x00,1);
    if (ret < 0)
        return ret;

    ret = ssd253x_i2c_read_bytes(client, 0xB5, buf, 2);
    if (ret < 0)
        return ret;
    data[0] = ((unsigned short)buf[0]<<8) + buf[1];

    ret = ssd253x_i2c_read_bytes(client, 0xB6, buf, 2);
    if (ret < 0)
        return ret;
    data[1] = ((unsigned short)buf[0]<<8) + buf[1];

    ret = ssd253x_i2c_read_bytes(client, 0xB7, buf, 2);
    if (ret < 0)
        return ret;
    data[2] = ((unsigned short)buf[0]<<8) + buf[1];

    ret = ssd253x_i2c_read_bytes(client, 0xB8, buf, 2);
    if (ret < 0)
        return ret;
    data[3] = ((unsigned short)buf[0]<<8) + buf[1];

    return ret;
}

static void ssd253x_write_cmd_list(struct i2c_client *client, struct ChipSetting *p, int length)
{
    int i;
    for (i=0; i<length; i++)
        ssd253x_write_cmd(client, p[i].Reg, p[i].Data1, p[i].Data2, p[i].No);
}

static void ssd253x_device_init(struct i2c_client *client)
{
    ssd253x_write_cmd_list(client, ssd253x_cmd_cfg_table, SSD_ARRAY_SIZE(ssd253x_cmd_cfg_table));
    mdelay(100);
}

static void ssd253x_device_reset(struct i2c_client *client)
{
    ssd253x_write_cmd_list(client, ssd253x_cmd_reset, SSD_ARRAY_SIZE(ssd253x_cmd_reset));
    mdelay(80);
}

static void ssd253x_device_resume(struct i2c_client *client)
{
    ssd253x_write_cmd_list(client, ssd253x_cmd_resume, SSD_ARRAY_SIZE(ssd253x_cmd_resume));
}

static void ssd253x_device_suspend(struct i2c_client *client)
{
    ssd253x_write_cmd_list(client, ssd253x_cmd_suspend, SSD_ARRAY_SIZE(ssd253x_cmd_suspend));
}

static void ssd253x_device_reinit(void)
{
	struct i2c_client * client;

	client = p_ssl_ts_priv->client;

    GPIO_SET_SSD_TS_SUSPEND;
    mdelay(5);
    GPIO_SET_SSD_TS_WORK;
    mdelay(5);
	
    ssd253x_device_reset(client);
    ssd253x_device_init(client);
 	ssd253x_calkey_init_timer();
}

#ifdef USE_CUT_EDGE
static int ssd253x_ts_cut_edge(unsigned short pos,unsigned short x_y)
{
    u8 cut_value = 10; //cut_value < 32
    if(pos == 0xfff)
    {
        return pos;
    }
    if(x_y) //xpos
    {
        //SSD_DBG(("X: rude data %d\n",pos);
        if(pos < 16)
            pos = cut_value + pos*(48 - cut_value) / 16;
        else if(pos > (XPOS_MAX - 16) )
            pos = XPOS_MAX + 16 + (pos - (XPOS_MAX -16))*(48 - cut_value) / 16;
        else
            pos = pos + 32;

        pos = SCREEN_MAX_X * pos / (SENSENO * 64);
        //SSD_DBG(("X: changed data %d\n",pos);
        return pos;
    }
    else    //ypos
    {
        //SSD_DBG(("Y: rude data %d\n",pos);
        if(pos < 16)
            pos = cut_value + pos*(48 - cut_value) / 16;
        else if(pos > (YPOS_MAX - 16) )
            pos = YPOS_MAX + 16 + (pos - (YPOS_MAX -16))*(48 - cut_value) / 16;
        else
            pos = pos + 32;
        //SSD_DBG(("Y: rude data %d\n",pos);
        pos = SCREEN_MAX_Y* pos / (DRIVENO * 64);
        //SSD_DBG(("Y: changed data %d\n",pos);
        return pos;
    }


}
#endif


static void ssd253x_write_auto_init_rst_reg(struct i2c_client *client)
{
    int ret = 0;
    unsigned char buf[4];
    int current_time;
    static unsigned short t1;
    static unsigned short t2;

    if (!ssd253x_internal_timer_inited)
    {
        ret = ssd253x_i2c_read_bytes(client,TIMESTAMP_REG, buf, 2);
        if (ret < 0)
        {
            SSD_DBG(("ssd253x_ts_work, TIMESTAMP_REG ret value %d!\n", ret));
            return;
        }

        current_time = ((unsigned short)buf[0]<<8) + buf[1];


        if (!t1)
        {
            t1 = current_time/1000;
        }

        t2 = current_time/1000;

        if((t2 - t1) > 10)
        {
            ssd253x_write_cmd(client, AUTO_INIT_RST_REG, 0x00, 0x00, 1);
            ssd253x_internal_timer_inited = 1;
        }
    }
}

static void ssd253x_ts_key_down(struct input_dev *input_dev, int index)
{
    if (ssd253x_key_info[index].status == SSD253X_KEY_IDLE)
    {
        ssd253x_key_info[index].status = SSD253X_KEY_PRESSED;
        SSD_DBG(("%s down\n", ssd253x_key_info[index].key_name));
        input_report_key(input_dev,ssd253x_key_info[index].key_value, 1);
		calkey_timer_status = 1;
    }
}

static void ssd253x_ts_key_up(struct input_dev *input_dev, int index)
{
    if (ssd253x_key_info[index].status == SSD253X_KEY_PRESSED)
    {
        ssd253x_key_info[index].status = SSD253X_KEY_IDLE;
        SSD_DBG(("%s up\n", ssd253x_key_info[index].key_name));
        input_report_key(input_dev,ssd253x_key_info[index].key_value, 0);
    }

}


static int ssd253x_ts_handle_finger_data(void)
{
	int i;
	unsigned short xpos = 0;
	unsigned short ypos = 0;
	int FingerInfo;
	int finger_status = 0;
	int ret = 0;
	unsigned char buf[4];
	int point_up = 0;
	int point_down = 0;
	int first_touch = 1;

	struct ssl_ts_priv *ts = p_ssl_ts_priv;

	SSD_DBG(("ssd253x_ts_work!\n"));


	for (i=0; i<SSD253X_MAX_FINGER_NUM; i++)
		ts->finger[i].curr_press = 0;

	/* Process touch area finger information 480 * 800 */
	ret = ssd253x_i2c_read_bytes(ts->client, EVENT_STATUS, buf, 2);
	if (ret < 0)
	{
		SSD_DBG(("ssd253x_ts_work, EVENT_STATUS ret value %d!\n", ret));
		return -1;
	}

	finger_status = ((unsigned short)buf[0]<<8) + buf[1];
	finger_status = (finger_status >> 4) & SSD253X_FINGER_MASK;

	SSD_DBG(("ssd253x finger status 0x%x\n", finger_status));


	for(i=0; i<SSD253X_MAX_FINGER_NUM; i++)
	{
		xpos = 0xFFF;
		ypos = 0xFFF;
		if((finger_status>>i)&0x1)
		{
			ret = ssd253x_i2c_read_bytes(ts->client,FINGER01_REG+i, buf, 4);
			if (ret < 0)
			{
				SSD_DBG(("ssd253x_ts_work, FINGER01_REG ret value %d!\n", ret));
				return -1;
			}

			FingerInfo = ((unsigned int)buf[3]<<0)|((unsigned int)buf[2]<<8)|((unsigned int)buf[1]<<16)|(buf[0]<<24);

			xpos = ((FingerInfo>>4)&0xF00)|((FingerInfo>>24)&0xFF);
			ypos = ((FingerInfo>>0)&0xF00)|((FingerInfo>>16)&0xFF);

#ifdef USE_CUT_EDGE
			xpos = ssd253x_ts_cut_edge(xpos, 1);
			ypos = ssd253x_ts_cut_edge(ypos, 0);
#endif
			if (xpos == 0xFFF)
			{
				finger_status = finger_status & (~(1<<i));
			}
			else
			{
				ts->finger[i].curr_press = 1;
				ts->finger[i].x = xpos;
				ts->finger[i].y = ypos;
			}
		}
	}

	ssd253x_write_cmd(ts->client, EVENT_FIFO_SCLR, 0x01, 0x00,1);

	for (i=0; i<SSD253X_MAX_FINGER_NUM; i++)
	{
		if (ts->finger[i].last_press)
		{
			first_touch = 0;
			break;
		}
	}


	for (i=0; i<SSD253X_MAX_FINGER_NUM; i++)
	{
		if (ts->finger[i].curr_press)
		{
			point_down = 1;
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,1);
			if (first_touch)
			{
				first_touch = 0;
				input_report_abs(ts->input_dev, ABS_PRESSURE, SSD253X_PRESSURE_MAX);
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

	return 0;
}

static int ssd253x_ts_handle_touch_key_data(void)
{
	int i;
	unsigned char key_status = 0;
	int ret = 0;

	struct ssl_ts_priv *ts = p_ssl_ts_priv;
    /* Process touch key event */
    ret = ssd253x_i2c_read_bytes(ts->client,SELFCAP_STATUS_REG, &key_status, 1);

    if (ret < 0)
    {
        SSD_DBG(("ssd253x_ts_work, SELFCAP_STATUS_REG, ret value %d!\n", ret));
        return -1;
    }

	key_status = key_status & 0x0F;
    SSD_DBG(("key status: %x\n", key_status));

    for (i=0; i<SSD253X_KEY_MAX_NUM; i++)
    {
        if (ssd253x_key_info[i].key_id & key_status) // key pressed
        {
            ssd253x_ts_key_down(ts->input_dev, i);
        }
        else // key up
        {
            ssd253x_ts_key_up(ts->input_dev, i);
        }
    }
	
	return key_status;
	
}

static void ssd253x_ts_isr_timer_work(struct work_struct *work)
{
	int i;
	int point_up = 0;
	struct ssl_ts_priv *ts = p_ssl_ts_priv;
	int key_status;

	SSD_DBG(("ssd253x_ts_isr_timer_work!\n"));
	
	ssd253x_write_auto_init_rst_reg(ts->client);
	
	for (i=0; i<SSD253X_MAX_FINGER_NUM; i++)
		ts->finger[i].curr_press = 0;

	for (i=0; i<SSD253X_MAX_FINGER_NUM; i++)
	{
		if (ts->finger[i].last_press != 0)
		{
			point_up = 1;
		}

		ts->finger[i].last_press = ts->finger[i].curr_press;
	}

	if (point_up)
	{
		input_report_abs(ts->input_dev, ABS_PRESSURE, 0);
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_mt_sync(ts->input_dev);
	}

	key_status = ssd253x_ts_handle_touch_key_data();

	if ((point_up) || (key_status == 0))
		input_sync(ts->input_dev);

	if (key_status > 0)
		ssd253x_isr_start_timer();

	return;
}

static void ssd253x_ts_work(struct work_struct *work)
{
	int ret;
	struct ssl_ts_priv *ts = p_ssl_ts_priv;
	ret = ssd253x_ts_handle_finger_data();
	if (ret < 0)
		goto i2c_transfer_error;
	
	ret = ssd253x_ts_handle_touch_key_data();
	if (ret < 0)
		goto i2c_transfer_error;
	
    ssd253x_write_auto_init_rst_reg(ts->client);
    input_sync(ts->input_dev);
	ssd253x_isr_start_timer();

i2c_transfer_error:
    enable_irq(ts->irq);

	return;
}

static void ssd253x_calkey_init_timer(void)
{
    unsigned short data[4] = {0};
    ssd253x_read_key_base(p_ssl_ts_priv->client,0x01,data);
    base_1 = data[0];
    base_2 = data[1];
    base_3 = data[2];
    base_4 = data[3];
	SSD_DBG(("calkey read base_1 = %d, base_2 = %d, base_3 = %d, base_4 = %d\n", base_1, base_2, base_3, base_4));
    init_timer(&p_ssl_ts_priv->calkey_timer);
    p_ssl_ts_priv->calkey_timer.function = ssd253x_calkey_do_timer;
    p_ssl_ts_priv->calkey_timer.data = (unsigned long)p_ssl_ts_priv;
    p_ssl_ts_priv->calkey_timer.expires = 0;
    mod_timer(&p_ssl_ts_priv->calkey_timer, jiffies + HZ);

}
static void ssd253x_calkey_do_timer(unsigned long data)
{
    struct ssl_ts_priv *ts = (struct ssl_ts_priv *)data;
    queue_work(calkey_workq, &ts->calkey_work);
}

static void ssd253x_calkey_do_work(struct work_struct *work)
{
    unsigned short data[4];
    int ret = 0;
    char buf[2];
    struct ssl_ts_priv *ts = container_of(work,struct ssl_ts_priv,calkey_work);

    ret = ssd253x_read_key_base(ts->client,0x00,data);
    if (ret < 0)
        return;

	SSD_DBG(("ssd253x_calkey_do_work base_1 = %d, base_2 = %d, base_3 = %d, base_4 = %d\n", data[0], data[1], data[2], data[3]));

#if 0
    if (data[0] < base_1)
    {
        if ((base_1 - data[0]) > CAL_RANGE)
        {
            SSD_DBG(("calkey old base_1 = %d, new base_1 = %d\n", base_1, data[0]));
            buf[1] = (char)(0xff & data[0]);
            buf[0] = (char)(data[0] >> 8);
            ssd253x_write_cmd(ts->client,0xb1,buf[0],buf[1],2);
            if(ret < 0)
            {
                SSD_DBG(("write b1 error!\n"));
                goto quit;
            }
            base_1 = data[0];
			calkey_timer_status = 1;
        }
    }
#endif

    if (data[1] < base_2)
    {
        if ((base_2 - data[1]) > CAL_RANGE)
        {
            SSD_DBG(("calkey old base_2 = %d, new base_2 = %d\n", base_2, data[1]));
            buf[1] = (char)(0xff & data[1]);
            buf[0] = (char)(data[1] >> 8);
            ssd253x_write_cmd(ts->client,0xb2,buf[0],buf[1],2);
            if(ret < 0)
            {
                SSD_DBG(("write b2 error!\n"));
                goto quit;
            }
            base_2 = data[1];
			calkey_timer_status = 1;
        }
    }

    if (data[2] < base_3)
    {
        if((base_3 - data[2]) > CAL_RANGE)
        {
            SSD_DBG(("calkey old base_3 = %d, new base_3 = %d\n", base_3, data[2]));
            buf[1] = (char)(0xff & data[2]);
            buf[0] = (char)(data[2] >> 8);
            ssd253x_write_cmd(ts->client,0xb3,buf[0],buf[1],2);
            if(ret < 0)
            {
                SSD_DBG(("write b3 error!\n"));
                goto quit;
            }
            base_3 = data[2];
			calkey_timer_status = 1;
        }
    }

    if (data[3] < base_4)
    {
        if((base_4 - data[3]) > CAL_RANGE)
        {
            SSD_DBG(("calkey old base_4 = %d, new base_4 = %d\n", base_4, data[3]));
            buf[1] = (char)(0xff & data[3]);
            buf[0] = (char)(data[3] >> 8);
            ssd253x_write_cmd(ts->client,0xb4,buf[0],buf[1],2);
            if(ret < 0)
            {
                SSD_DBG(("write b4 error!\n"));
            }
            base_4 = data[3];
			calkey_timer_status = 1;
        }
    }
quit:
    if (calkey_timer_status == 1)//Paul added for touch-key check 2012-01-12
        return;
    mod_timer(&ts->calkey_timer,  jiffies + 200);
	return;
}

static int ssd253x_ts_probe(struct i2c_client *client,const struct i2c_device_id *idp)
{
    struct ssl_ts_priv * ts;
    struct input_dev * ssl_input = NULL;
    int i;
    int ret = -1;
    unsigned char buf[4];

    printk(("ssd253x_ts_probe\n"));

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        SSD_DBG(("ssd253x_ts_probe: need I2C_FUNC_I2C\n"));
        return -ENODEV;
    }
    else
    {
        SSD_DBG(("ssd253x_ts_probe: i2c_client name: %s\n", client->name));
    }

    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if (!ts)
    {
        SSD_DBG(("ssd253x_ts_probe: kzalloc Error!\n"));
        ret = -ENODEV;
        goto    err0;
    }
    else
    {
        SSD_DBG(("ssd253x_ts_probe: kzalloc OK!\n"));
    }
	p_ssl_ts_priv = ts;
	
    dev_set_drvdata(&client->dev, ts);

    GPIO_SET_SSD_TS_SUSPEND;
    mdelay(5);
    GPIO_SET_SSD_TS_WORK;
    mdelay(5);

    for (i=0; i<5; i++)
    {
	    unsigned char test_data = 0;
        ret =ssd253x_i2c_write_bytes(client, &test_data, 1);
        if (ret >= 0)
            break;
    }

    if(ret < 0)
    {
        dev_err(&client->dev, "ssd253x_ts_probe: I2C connection wrong!\n");
        goto err_i2c_failed;
    }

    ssl_input = input_allocate_device();
    if (!ssl_input)
    {
        SSD_DBG(("ssd253x_ts_probe: input_allocate_device Error\n"));
        ret = -ENODEV;
        goto    err1;
    }
    else
    {
        SSD_DBG(("ssd253x_ts_probe: input_allocate_device OK\n"));
    }

    ssl_input->name = client->name;
    ssl_input->id.bustype = BUS_I2C;
    ssl_input->id.vendor  = 0x2878; // Modify for Vendor ID
    ssl_input->dev.parent = &client->dev;

    input_set_drvdata(ssl_input, ts);
    ts->client = client;
    ts->input_dev = ssl_input;
	ts->irq = TS_INT;
    ts->Resolution = 64;

    ssd253x_device_reset(client);

    ret = ssd253x_i2c_read_bytes(ts->client,DEVICE_ID_REG, buf, 2);
    if (ret < 0)
    {
        SSD_DBG(("ssd253x_ts_work, TIMESTAMP_REG ret value %d!\n", ret));
    }
    ssl_input->id.product = ((unsigned short)buf[0]<<8) + buf[1];

    ret = ssd253x_i2c_read_bytes(ts->client,VERSION_ID_REG, buf, 2);
    if (ret < 0)
    {
        SSD_DBG(("ssd253x_ts_work, TIMESTAMP_REG ret value %d!\n", ret));
    }
    ssl_input->id.version = ((unsigned short)buf[0]<<8) + buf[1];

    SSD_DBG(("ssd253x_ts_probe device id  : 0x%04x\n",ssl_input->id.product));
    SSD_DBG(("ssd253x_ts_probe version id : 0x%04x\n",ssl_input->id.version));

    if	(ts->input_dev->id.product==0x2531)
        ts->Resolution=32;
    else
    {
        if(ts->input_dev->id.product==0x2533)
            ts->Resolution=64;
        else
        {
            SSD_DBG(("ssd253x_ts_probe: ssl_input->id.product Error\n"));
            ret = -ENODEV;
            goto    err1;
        }
    }

    // ssd_id_check(); //Paul added for ID check 2012-01-10
    ssd253x_device_init(client);
    ssd253x_write_cmd(client,EVENT_FIFO_SCLR,0x01,0x00,1); // clear Event FiFo
    SSD_DBG(("ssd253x_ts_probe: %04x deviceInit OK!\n",ssl_input->id.product));

    set_bit(ABS_MT_TOUCH_MAJOR, ssl_input->absbit);
    set_bit(ABS_MT_POSITION_X,  ssl_input->absbit);
    set_bit(ABS_MT_POSITION_Y,  ssl_input->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, ssl_input->absbit);
    set_bit(ABS_MT_TRACKING_ID, ssl_input->absbit);
    set_bit(EV_KEY,             ssl_input->evbit);
    set_bit(EV_ABS,             ssl_input->evbit);
	set_bit(BTN_TOUCH,          ssl_input->keybit);
	set_bit(INPUT_PROP_DIRECT,  ssl_input->propbit);

    /* Enable all supported keys */
    for (i=0; i<SSD253X_KEY_MAX_NUM; i++)
    {
        __set_bit(ssd253x_key_info[i].key_value, ssl_input->keybit);

    }
    __clear_bit(KEY_RESERVED, ssl_input->keybit);

	input_set_abs_params(ssl_input, ABS_MT_TRACKING_ID,  0, SSD253X_MAX_FINGER_NUM, 0, 0);
    input_set_abs_params(ssl_input, ABS_MT_POSITION_X,   0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(ssl_input, ABS_MT_POSITION_Y,   0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(ssl_input, ABS_MT_TOUCH_MAJOR,  0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, SSD253X_PRESSURE_MAX, 0, 0);

    INIT_WORK(&ts->isr_work, ssd253x_ts_work);
	INIT_WORK(&ts->isr_timer_work, ssd253x_ts_isr_timer_work);
	
    ret = input_register_device(ssl_input);
    if (ret)
    {
        SSD_DBG(("ssd253x_ts_probe: input_register_device input_dev Error!\n"));
        ret = -ENODEV;
        goto err1;
    }
    else
    {
        SSD_DBG(("ssd253x_ts_probe: input_register_device input_dev OK!\n"));
    }

    ret = request_irq(ts->irq, ssd253x_ts_isr, IRQF_TRIGGER_FALLING, client->name,ts);

    if (ret)
    {
        SSD_DBG(("ssd253x_ts_probe: request_irq Error!\n"));
        ret = -ENODEV;
        goto err2;
    }
    else
    {
        disable_irq(ts->irq);
        SSD_DBG(("ssd253x_ts_probe: request_irq OK!\n"));
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    ts->early_suspend.suspend = ssd253x_ts_early_suspend;
    ts->early_suspend.resume  = ssd253x_ts_late_resume;
    ts->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
    register_early_suspend(&ts->early_suspend);
#endif
    calkey_workq = create_workqueue ("calkey_workq");
    if (!calkey_workq)
    {
        ret = -ENOMEM;
        SSD_DBG(("%s, %d\n", __func__, __LINE__));
        goto err2;
    }
    INIT_WORK(&ts->calkey_work,ssd253x_calkey_do_work);
	ssd253x_calkey_init_timer();
    enable_irq(ts->irq);
    msm_tp_set_found_flag(1);

    printk("Touchscreen ssd253x is ready now.\n");
    return 0;

err2:
    input_unregister_device(ssl_input);

err1:
    input_free_device(ssl_input);

err_i2c_failed:
    kfree(ts);

err0:
    dev_set_drvdata(&client->dev, NULL);
    return ret;
}

static int ssd253x_ts_resume(struct i2c_client *client)
{
    struct ssl_ts_priv *ts = dev_get_drvdata(&client->dev);
    SSD_DBG(("ssd253x_ts_resume\n"));
    ssd253x_internal_timer_inited = 0;
    calkey_timer_status = 0; //Paul added for touch-key check 2012-01-12
//    mod_timer(&ts->calkey_timer,jiffies + 200);
	ssd253x_device_reinit();
	ssd253x_device_resume(client);
    enable_irq(ts->irq);
    return 0;
}

static int ssd253x_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct ssl_ts_priv *ts = dev_get_drvdata(&client->dev);
    SSD_DBG(("ssd253x_ts_suspend\n"));
    calkey_timer_status = 1;    //Paul added for touch-key check 2012-01-12
    disable_irq(ts->irq);
    ssd253x_internal_timer_inited = 0;
    ssd253x_device_suspend(client);
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssd253x_ts_late_resume(struct early_suspend *h)
{
    struct ssl_ts_priv *ts = container_of(h, struct ssl_ts_priv, early_suspend);
    SSD_DBG(("ssd253x_ts_late_resume\n"));
    ssd253x_ts_resume(ts->client);
}
static void ssd253x_ts_early_suspend(struct early_suspend *h)
{
    struct ssl_ts_priv *ts = container_of(h, struct ssl_ts_priv, early_suspend);
    SSD_DBG(("ssd253x_ts_early_suspend\n"));
    ssd253x_ts_suspend(ts->client, PMSG_SUSPEND);
}
#endif

static int ssd253x_ts_remove(struct i2c_client *client)
{
    struct ssl_ts_priv *ts = dev_get_drvdata(&client->dev);
    SSD_DBG(("ssd253x_ts_remove\n"));
    free_irq(ts->irq, ts);
    input_unregister_device(ts->input_dev);
    input_free_device(ts->input_dev);
    kfree(ts);
    dev_set_drvdata(&client->dev, NULL);
    return 0;
}

static irqreturn_t ssd253x_ts_isr(int irq, void *dev_id)
{
    struct ssl_ts_priv *ts = dev_id;
    SSD_DBG(("ssd253x_ts_isr\n"));
    disable_irq_nosync(ts->irq);
    queue_work(ssd253x_wq, &ts->isr_work);
    return IRQ_HANDLED;
}

static void ssd253x_isr_stop_timer(void)
{
	if (p_ssl_ts_priv->isr_timer_started)
	{
		del_timer(&p_ssl_ts_priv->isr_timer_list);
		p_ssl_ts_priv->isr_timer_started = 0;
	}
}

static void ssd253x_isr_start_timer(void)
{
	SSD_DBG(("ssd253x_isr_start_timer\n"));
	ssd253x_isr_stop_timer();
	init_timer(&p_ssl_ts_priv->isr_timer_list);
	p_ssl_ts_priv->isr_timer_list.expires = jiffies + (HZ/25);
	p_ssl_ts_priv->isr_timer_list.function = ssd253x_isr_timer_func;
	p_ssl_ts_priv->isr_timer_list.data = (unsigned long) 0; 
	add_timer(&p_ssl_ts_priv->isr_timer_list);
	p_ssl_ts_priv->isr_timer_started = 1;
}

static void ssd253x_isr_timer_func(unsigned long data)
{
	SSD_DBG(("ssd253x_isr_timer_func\n"));
	p_ssl_ts_priv->isr_timer_started = 0;
	queue_work(ssd253x_wq, &p_ssl_ts_priv->isr_timer_work);
}


static const struct i2c_device_id ssd253x_ts_id[] =
{
    {"ssd253x_ts", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, ssd253x_ts_id);

static struct i2c_driver ssd253x_ts_driver =
{
    .driver = {
        .name = "ssd253x_ts",
    },
    .probe = ssd253x_ts_probe,
    .remove = ssd253x_ts_remove,
#ifndef    CONFIG_HAS_EARLYSUSPEND
    .suspend = ssd253x_ts_suspend,
    .resume = ssd253x_ts_resume,
#endif
    .id_table = ssd253x_ts_id,
};

static int __init ssd253x_ts_init(void)
{
    int ret;

    if (msm_tp_get_found_flag())
    {
        return -1;
    }

    ssd253x_wq = create_singlethread_workqueue("ssd253x_wq");
    if (!ssd253x_wq)
    {
        SSD_DBG(("ssd253x_ts_init: create_singlethread_workqueue Error!\n"));
        return -ENOMEM;
    }
    else
    {
        SSD_DBG(("ssd253x_ts_init: create_singlethread_workqueue OK!\n"));
    }

    ret = i2c_add_driver(&ssd253x_ts_driver);
    if (ret)
        SSD_DBG(("ssd253x_ts_init: i2c_add_driver Error!\n"));
    else
        SSD_DBG(("ssd253x_ts_init: i2c_add_driver OK! \n"));

    return ret;
}

static void __exit ssd253x_ts_exit(void)
{
    SSD_DBG(("ssd253x_ts_exit\n"));
    i2c_del_driver(&ssd253x_ts_driver);
    if (ssd253x_wq)
        destroy_workqueue(ssd253x_wq);
}

late_initcall(ssd253x_ts_init);
module_exit(ssd253x_ts_exit);

MODULE_AUTHOR("Solomon Systech Ltd - Design Technology, Icarus Choi");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ssd253x Touchscreen Driver 1.3");

