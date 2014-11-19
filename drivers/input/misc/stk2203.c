/*
 * stk2203.c
 *
 * Copyright 2009 ROHM Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>

//#define ALS_DEBUG
#ifdef ALS_DEBUG
#define ALSDBG(x) printk x
#else
#define ALSDBG(x)
#endif

#define STK2203_DEV_NAME	"stk2203"

#define STK2203_I2C_NAME	"stk2203"
#define STK2203_I2C_ADDR 0x10

/* Use 'm' as magic number */
#define STK2203_IOM			'l'

/* IOCTLs for STK2203 device */
#define STK2203_IOC_STDBY		_IO (STK2203_IOM, 0x00)
#define STK2203_IOC_ACT			_IO (STK2203_IOM, 0x01)
#define STK2203_IOC_RESET		_IO (STK2203_IOM, 0x02)
#define STK2203_IOC_READXYZ		_IOR(STK2203_IOM, 0x03, int[1])

#define STK2203_DELAY_STDBY	10	/* ms */
#define STK2203_DELAY_ACT	10	/* ms */

#define CONFIG_STK_ALS_TRANSMITTANCE 500
/* Define Reg Address */
#define STK_ALS_CMD_REG 0x01
#define STK_ALS_DT1_REG 0x02
#define STK_ALS_DT2_REG 0x03
#define STK_ALS_INT_REG 0x04


/* Define CMD */
#define STK_ALS_CMD_GAIN_SHIFT 6
#define STK_ALS_CMD_IT_SHIFT 2
#define STK_ALS_CMD_SD_SHIFT 0

#define STK_ALS_CMD_GAIN(x) ((x)<<STK_ALS_CMD_GAIN_SHIFT)
#define STK_ALS_CMD_IT(x) ((x)<<STK_ALS_CMD_IT_SHIFT)
#define STK_ALS_CMD_SD(x) ((x)<<STK_ALS_CMD_SD_SHIFT)

#define STK_ALS_CMD_GAIN_MASK 0xC0
#define STK_ALS_CMD_IT_MASK 0x0C
#define STK_ALS_CMD_SD_MASK 0x1

/* Define Data */
#define STK_ALS_DATA(DT1,DT2) ((DT1<<4)|(DT2)>>4)

#define stk2203_writeb(x,y) i2c_smbus_write_byte_data(stk2203_client,x,y)
#define stk2203_readb(x) i2c_smbus_read_byte_data(stk2203_client,x)

struct stk2203_data
{
    struct early_suspend early_suspend;
};

static const struct i2c_device_id stk2203_id[] = {
	{ STK2203_I2C_NAME, STK2203_I2C_ADDR },
	{ }
};

static int stk2203_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int stk2203_remove(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void stk2203_early_suspend(struct early_suspend *h);
static void stk2203_late_resume(struct early_suspend *h);
#endif
static int stk2203_suspend(struct i2c_client *client, pm_message_t state);
static int stk2203_resume(struct i2c_client *client);

static struct i2c_client 	*stk2203_client = NULL;

static struct i2c_driver stk2203_driver = {
	.probe 		= stk2203_probe,
	.remove 	= stk2203_remove,
	.id_table	= stk2203_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend	= stk2203_suspend,
    .resume		= stk2203_resume,
#endif
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= STK2203_I2C_NAME,
	},
};


static int stk2203_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int stk2203_release(struct inode *inode, struct file *file)
{
	return 0;
}

int alscode2lux(int alscode)
{
    //R-set Def --> 500KOhm ==> x 5
    //IT --> x1
    //Gain --> x2
    // Total x 10

    alscode<<=10; // x 1024
    // Org : 1 code (100KOhm, IT = x1 , Gain = x1) ~ 1 lux
    // x10 x1024 ==> 10240 code = 1 lux
    return alscode/CONFIG_STK_ALS_TRANSMITTANCE;
}


int stk2203_get_reading(void)
{
	return STK_ALS_DATA(stk2203_readb(STK_ALS_DT1_REG),stk2203_readb(STK_ALS_DT2_REG));
}

static int stk2203_get_lux(void)
{
    return  alscode2lux(stk2203_get_reading());
}
static int stk2203_set_gain(uint32_t gain)
{
	int val;
	val = stk2203_readb(STK_ALS_CMD_REG);
	val &= (~STK_ALS_CMD_GAIN_MASK);
	val |= STK_ALS_CMD_GAIN(gain);
	return stk2203_writeb(STK_ALS_CMD_REG,val);
}

static int stk2203_set_it(uint32_t it)
{
	int val;
	val = stk2203_readb(STK_ALS_CMD_REG);
	val &= (~STK_ALS_CMD_IT_MASK);
	val |= STK_ALS_CMD_IT(it);
	return stk2203_writeb(STK_ALS_CMD_REG,val);
}

static int stk2203_set_power_state(uint32_t nShutdown)
{
	int val;
	val = stk2203_readb(STK_ALS_CMD_REG);
	val &= (~STK_ALS_CMD_SD_MASK);
	val |= STK_ALS_CMD_SD(nShutdown);
	return stk2203_writeb(STK_ALS_CMD_REG,val);
}

static int stk2203_enable_als(uint32_t enable)
{
	int ret;
	ret = stk2203_set_power_state(enable?0:1);
	return ret;
}

static int stk2203_read_data(unsigned short *value)
{
    int lux;
    lux = stk2203_get_lux();

    *value = (unsigned short)lux;
    return 0;
}

static int stk2203_init_setting(void)
{
	int val;
    
	stk2203_enable_als(0);
	stk2203_set_gain(1); //x2
	stk2203_set_it(1); //x1
	val = stk2203_readb(STK_ALS_CMD_REG);
	ALSDBG(("Init ALS Setting --> CMDREG = 0x%x\n",val));
	stk2203_enable_als(1);
    return 0;
}

static long stk2203_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	unsigned short data[1] = {0};
	int vec[1] = {0};
	long ret;

	switch (cmd) {
	case STK2203_IOC_STDBY:
		break;
	case STK2203_IOC_ACT:
		break;
	case STK2203_IOC_RESET:
		break;
	case STK2203_IOC_READXYZ:
		ret = stk2203_read_data(&data[0]);
		if (ret == 0)
	    {
		    vec[0] = (int)data[0];
		
		    ALSDBG(("STK2203: [X - %04x]\n",  vec[0]));

		    if (copy_to_user(pa, vec, sizeof(vec))) {
    			return -EFAULT;
		    }
	    }
		else
		{
		    ALSDBG(("STK2203: return error"));
		}
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t stk2203_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "stk2203");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(stk2203, S_IRUGO, stk2203_show, NULL);

static struct file_operations stk2203_fops = {
	.owner		= THIS_MODULE,
	.open		= stk2203_open,
	.release	= stk2203_release,
	.unlocked_ioctl		= stk2203_ioctl,
};

static struct miscdevice stk2203_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = STK2203_DEV_NAME,
	.fops = &stk2203_fops,
};


static int stk2203_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
    struct stk2203_data *pdata = NULL;

//	int i=0;
	
	printk("stk2203_probe\n");	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
	ALSDBG(("%s: functionality check OK\n", __FUNCTION__));
	
	if (i2c_smbus_read_byte_data(client,STK_ALS_CMD_REG)<0)
	{
		pr_err("STKALS : no device found\n");
		res = -ENODEV;
		goto out;
	}
	
    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (pdata == NULL)
    {
        res = -ENOMEM;
        goto out;
    }
	stk2203_client = client;
    i2c_set_clientdata(client, pdata);

	res = misc_register(&stk2203_device);
	if (res) {
		pr_err("%s: stk2203_device register failed\n", __FUNCTION__);
		goto out;
	}
	ALSDBG(("%s: stk2203_device register OK\n", __FUNCTION__));
	res = device_create_file(&client->dev, &dev_attr_stk2203);
	if (res) {
		pr_err("%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister;
	}
	ALSDBG(("%s: device_create_file OK\n", __FUNCTION__));

    ////////////////////////////////////////////////////////////////////////
    // Init STK2203
    ////////////////////////////////////////////////////////////////////////
    res = stk2203_init_setting();
	if (res) {
		pr_err("%s: stk2203_init_setting failed\n", __FUNCTION__);
		goto out_deregister;
	}
    
	
	/* wait external capacitor charging done for next SET/RESET */
//	msleep(STK2203_DELAY_ACT);

#ifdef CONFIG_HAS_EARLYSUSPEND
    pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    pdata->early_suspend.suspend = stk2203_early_suspend;
    pdata->early_suspend.resume = stk2203_late_resume;
    register_early_suspend(&pdata->early_suspend);
#endif
	printk("stk2203_probe OK\n");	
	return 0;

out_deregister:
	misc_deregister(&stk2203_device);
out:
	if (pdata)
		kfree(pdata);
	printk("stk2203_probe failed\n");
	return res;
}


static int stk2203_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_stk2203);
	misc_deregister(&stk2203_device);

	return 0;
}

static int stk2203_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("STK2203 suspended\n");
	stk2203_enable_als(0);
	return 0;
}

static int stk2203_resume(struct i2c_client *client)
{
	pr_info("STK2203 resumed\n");
  stk2203_init_setting();	
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void stk2203_early_suspend(struct early_suspend *h)
{
    stk2203_suspend(stk2203_client, PMSG_SUSPEND);
}

static void stk2203_late_resume(struct early_suspend *h)
{
    stk2203_resume(stk2203_client);
}
#endif

static int __init stk2203_init(void)
{
	int res;

	res = i2c_add_driver(&stk2203_driver);
	if (res < 0){
		pr_info("add stk2203 i2c driver failed\n");
		return -ENODEV;
	}

	return (res);
}

static void __exit stk2203_exit(void)
{
	i2c_del_driver(&stk2203_driver);
}

late_initcall(stk2203_init);
module_exit(stk2203_exit);

MODULE_AUTHOR("STK");
MODULE_DESCRIPTION("STK2203 light sensor driver");
MODULE_LICENSE("GPL");

