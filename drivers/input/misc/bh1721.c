/*
 * bh1721.c
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

#define DEBUG			0

#define BH1721_DEV_NAME	"bh1721"

#define BH1721_I2C_NAME	"bh1721"
#define BH1721_I2C_ADDR 0x23

/* Use 'm' as magic number */
#define BH1721_IOM			'l'

/* IOCTLs for BH1721 device */
#define BH1721_IOC_STDBY		_IO (BH1721_IOM, 0x00)
#define BH1721_IOC_ACT			_IO (BH1721_IOM, 0x01)
#define BH1721_IOC_RESET		_IO (BH1721_IOM, 0x02)
#define BH1721_IOC_READXYZ		_IOR(BH1721_IOM, 0x03, int[1])


#define BH1721_DELAY_STDBY	10	/* ms */
#define BH1721_DELAY_ACT	10	/* ms */


enum {
	REG_XOUT = 0x00,
	REG_YOUT,
	REG_ZOUT,
	REG_TILT,
	REG_SRST,
	REG_SPCNT,
	REG_INTSU,
	REG_MODE,
	REG_SR,
	REG_PDET,
	REG_PD,
	REG_END,
};

struct bh1721_data
{
    struct early_suspend early_suspend;
};

static const struct i2c_device_id bh1721_id[] = {
	{ BH1721_I2C_NAME, BH1721_I2C_ADDR },
	{ }
};

static int bh1721_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int bh1721_remove(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void bh1721_early_suspend(struct early_suspend *h);
static void bh1721_late_resume(struct early_suspend *h);
#endif
static int bh1721_suspend(struct i2c_client *client, pm_message_t state);
static int bh1721_resume(struct i2c_client *client);

static struct i2c_client 	*bh1721_client = NULL;

static struct i2c_driver bh1721_driver = {
	.probe 		= bh1721_probe,
	.remove 	= bh1721_remove,
	.id_table	= bh1721_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend	= bh1721_suspend,
    .resume		= bh1721_resume,
#endif
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= BH1721_I2C_NAME,
	},
};


static int bh1721_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int bh1721_release(struct inode *inode, struct file *file)
{
	return 0;
}


static int bh1721_read_data(unsigned short *value)
{

	unsigned int highbyte = 0 ;
	unsigned int lowbyte = 0 ;
	unsigned short lux = 0;
	uint8_t buf[2];

	int ret;

    if (bh1721_client == NULL)
        return -1;
	
	ret = i2c_master_recv(bh1721_client, buf, sizeof(buf));
	
	if (ret != sizeof(buf)) 
	{
		pr_info("BH1721: read_data fail");
		return -1;
	} 
	highbyte = buf[0];
	lowbyte = buf[1];

	lux = (highbyte<<8) + lowbyte;

    *value = lux;
    return 0;
}

static long bh1721_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	unsigned short data[1] = {0};
	int vec[1] = {0};
	long ret;

	switch (cmd) {
	case BH1721_IOC_STDBY:
		break;
	case BH1721_IOC_ACT:
		break;
	case BH1721_IOC_RESET:
		break;
	case BH1721_IOC_READXYZ:
		ret = bh1721_read_data(&data[0]);
		if (ret == 0)
	    {
		    vec[0] = (int)data[0];
		
#if DEBUG
		    pr_info("BH1721: [X - %04x]\n",  vec[0]);
#endif

		    if (copy_to_user(pa, vec, sizeof(vec))) {
    			return -EFAULT;
		    }
	    }
		else
		{
#if DEBUG
		    pr_info("BH1721: return error");
#endif
		}
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t bh1721_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "bh1721");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(bh1721, S_IRUGO, bh1721_show, NULL);

static struct file_operations bh1721_fops = {
	.owner		= THIS_MODULE,
	.open		= bh1721_open,
	.release	= bh1721_release,
	.unlocked_ioctl		= bh1721_ioctl,
};

static struct miscdevice bh1721_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = BH1721_DEV_NAME,
	.fops = &bh1721_fops,
};


static int bh1721_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
    struct bh1721_data *pdata = NULL;

//	int i=0;
	
	printk("bh1721_probe\n");	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
#if DEBUG	
	pr_info("%s: functionality check OK\n", __FUNCTION__);
#endif
    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (pdata == NULL)
    {
        res = -ENOMEM;
        goto out;
    }
	bh1721_client = client;
    i2c_set_clientdata(client, pdata);

	res = misc_register(&bh1721_device);
	if (res) {
		pr_err("%s: bh1721_device register failed\n", __FUNCTION__);
		goto out;
	}
#if DEBUG	
	pr_info("%s: bh1721_device register OK\n", __FUNCTION__);
#endif
	res = device_create_file(&client->dev, &dev_attr_bh1721);
	if (res) {
		pr_err("%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister;
	}
#if DEBUG	
	pr_info("%s: device_create_file OK\n", __FUNCTION__);
#endif


    ////////////////////////////////////////////////////////////////////////
    // Init BH1721
    ////////////////////////////////////////////////////////////////////////

	if (0 != i2c_smbus_write_byte(client, 0x01))//0000 0001,	Power on
	{
		pr_err("%s: i2c_smbus_write_byte error\n", __FUNCTION__);
		goto out_deregister;
	}
	
	if (0 != i2c_smbus_write_byte(client, 0x49))	//0100 1001,	
	{
		pr_err("%s: i2c_smbus_write_byte error\n", __FUNCTION__);
		goto out_deregister;
	}
	
	if (0 != i2c_smbus_write_byte(client, 0x6C))	//0110 1100,	transimission rate is 100%
	{
		pr_err("%s: i2c_smbus_write_byte error\n", __FUNCTION__);
		goto out_deregister;
	}
	
	if (0 != i2c_smbus_write_byte(client, 0x10))	//0001 0000,	1lx/step H-Res Continuously
	{
		pr_err("%s: i2c_smbus_write_byte error\n", __FUNCTION__);
		goto out_deregister;
	}

	
	/* wait external capacitor charging done for next SET/RESET */
//	msleep(BH1721_DELAY_ACT);

#ifdef CONFIG_HAS_EARLYSUSPEND
    pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    pdata->early_suspend.suspend = bh1721_early_suspend;
    pdata->early_suspend.resume = bh1721_late_resume;
    register_early_suspend(&pdata->early_suspend);
#endif
	printk("bh1721_probe OK\n");	
	return 0;

out_deregister:
	misc_deregister(&bh1721_device);
out:
	if (pdata)
		kfree(pdata);
	return res;
}


static int bh1721_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_bh1721);
	misc_deregister(&bh1721_device);

	return 0;
}

static int bh1721_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("BH1721 suspended\n");
	return 0;
}

static int bh1721_resume(struct i2c_client *client)
{
	pr_info("BH1721 resumed\n");
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bh1721_early_suspend(struct early_suspend *h)
{
    bh1721_suspend(bh1721_client, PMSG_SUSPEND);
}

static void bh1721_late_resume(struct early_suspend *h)
{
    bh1721_resume(bh1721_client);
}
#endif

static int __init bh1721_init(void)
{
	int res;

	res = i2c_add_driver(&bh1721_driver);
	if (res < 0){
		pr_info("add bh1721 i2c driver failed\n");
		return -ENODEV;
	}

	return (res);
}

static void __exit bh1721_exit(void)
{
	i2c_del_driver(&bh1721_driver);
}

late_initcall(bh1721_init);
module_exit(bh1721_exit);

MODULE_AUTHOR("Rohm, Inc.");
MODULE_DESCRIPTION("BH1721 light sensor driver");
MODULE_LICENSE("GPL");

