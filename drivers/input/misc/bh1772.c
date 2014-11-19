/*
 * bh1772.c
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
#include <linux/wakelock.h>

#define DEBUG			0

#define BH1772_DEV_NAME	"bh1772"

#define BH1772_I2C_NAME	"bh1772"
#define BH1772_I2C_ADDR   0x38

/* Use 'm' as magic number */
#define BH1772_IOM			'l'

/* IOCTLs for BH1772 device */
#define BH1772_IOC_PROXIMITY_STDBY		_IO (BH1772_IOM, 0x01)
#define BH1772_IOC_PROXIMITY_ACT		_IO (BH1772_IOM, 0x02)
#define BH1772_IOC_RESET		        _IO (BH1772_IOM, 0x03)
#define BH1772_IOC_READXYZ		        _IOR(BH1772_IOM, 0x04, int[1])


#define BH1772_DELAY_STDBY	10	/* ms */
#define BH1772_DELAY_ACT	10	/* ms */


enum
{
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

struct bh1772_data
{
    struct early_suspend early_suspend;
};

static const struct i2c_device_id bh1772_id[] =
{
    { BH1772_I2C_NAME, BH1772_I2C_ADDR },
    { }
};

struct wake_lock bh1772_wake_lock;

static int bh1772_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int bh1772_remove(struct i2c_client *client);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bh1772_early_suspend(struct early_suspend *h);
static void bh1772_late_resume(struct early_suspend *h);
#endif

static int bh1772_suspend(struct i2c_client *client, pm_message_t state);
static int bh1772_resume(struct i2c_client *client);

static struct i2c_client * bh1772_client = NULL;

static struct i2c_driver bh1772_driver =
{
    .probe 		= bh1772_probe,
    .remove 	= bh1772_remove,
    .id_table	= bh1772_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend	= bh1772_suspend,
    .resume		= bh1772_resume,
#endif
    .driver 	= {
        .owner	= THIS_MODULE,
        .name	= BH1772_I2C_NAME,
    },
};


static int bh1772_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int bh1772_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int bh1772_write_data(unsigned char * buf, int len)
{
    int ret;

    if (len > 0)
    {
        ret = i2c_master_send(bh1772_client, (char*)buf, len);
        if(ret < 0)
        {
            printk("bh1772_write_data, i2c_master_send data error %d\n", ret);
            return -1;
        }
    }

    return 0;
}


static int bh1772_read_data(unsigned char reg_addr, unsigned char * buf, int len)
{

    int ret;


    if (bh1772_client == NULL)
        return -1;

    ret = i2c_master_send(bh1772_client, (char*)&reg_addr, 1);
    if(ret < 0)
    {
        printk("bh1772_read_data, i2c_master_send error %d\n", ret);
        return -1;
    }

    ret = i2c_master_recv(bh1772_client, (char*)buf, len);

    if (ret != len)
    {
        pr_info("bh1772_read_data: i2c_master_recv fail, %d\n", ret);
        return -1;
    }

    return 0;
}

static int bh1772_set_light_stdby(void)
{
    uint8_t buf[2] = {0x40, 0x00}; //Light sensor standby mode
#if DEBUG
    printk("bh1772_set_light_stdby\n");
#endif
    if (0 != bh1772_write_data(buf, 2))
    {
        pr_err("%s: error\n", __FUNCTION__);
    }
    return 0;
}

static int bh1772_set_light_act(void)
{
    uint8_t buf[2] = {0x40, 0x03};//Light sensor stand alone mode
#if DEBUG
    printk("bh1772_set_light_act\n");
#endif
    if (0 != bh1772_write_data(buf, 2))
    {
        pr_err("%s, error\n", __FUNCTION__);
    }

    return 0;
}

static int bh1772_set_proximity_stdby(void)
{
    uint8_t buf[2] = {0x41, 0x00}; //Proximity standby mode
#if DEBUG
    printk("bh1772_set_proximity_stdby\n");
#endif
	wake_unlock(&bh1772_wake_lock);
    if (0 != bh1772_write_data(buf, 2))
    {
        pr_err("%s: error\n", __FUNCTION__);
    }
    return 0;
}

static int bh1772_set_proximity_act(void)
{
    uint8_t buf[2] = {0x41, 0x03};//Proximity stand alone mode
#if DEBUG
    printk("bh1772_set_proximity_act\n");
#endif
	wake_lock(&bh1772_wake_lock);
    if (0 != bh1772_write_data(buf, 2))
    {
        pr_err("%s: error\n", __FUNCTION__);
    }

    return 0;
}

static long bh1772_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *pa = (void __user *)arg;
    unsigned short data[2] = {0};
    int vec[2] = {0};
    int ret;
    uint8_t buf[4];
    unsigned char reg_addr;;


    switch (cmd)
    {
    case BH1772_IOC_PROXIMITY_STDBY:
        bh1772_set_proximity_stdby();
        break;
    case BH1772_IOC_PROXIMITY_ACT:
        bh1772_set_proximity_act();
        break;
    case BH1772_IOC_RESET:
        break;
    case BH1772_IOC_READXYZ:
        reg_addr = 0x4C;
        ret = bh1772_read_data(reg_addr, buf, sizeof(buf));

        if (ret == 0)
        {
            /**
            	Addr 0x4C, Lux Low byte, buf[0]
            	Addr 0x4D, Lux High byte, buf[1]
            	Addr 0x4E, not used, buf[2]
            	Addr 0x4F, proximity data, buf[3]
            */

            data[0] = buf[1];
            data[0] = (data[0] << 8) + buf[0];

            data[1] = buf[3];

            vec[0] = (int)data[0];
            vec[1] = (int)data[1];

#if DEBUG
            printk("bh1772_read_data, lux %d, ps data %d\n", data[0], data[1]);
#endif

            if (copy_to_user(pa, vec, sizeof(vec)))
            {
                return -EFAULT;
            }
        }
        else
        {
#if DEBUG
            pr_info("BH1772: return error");
#endif
        }
        break;
    default:
        break;
    }

    return 0;
}

static ssize_t bh1772_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    sprintf(buf, "bh1772");
    ret = strlen(buf) + 1;

    return ret;
}

static DEVICE_ATTR(bh1772, S_IRUGO, bh1772_show, NULL);

static struct file_operations bh1772_fops =
{
    .owner		= THIS_MODULE,
    .open		= bh1772_open,
    .release	= bh1772_release,
    .unlocked_ioctl		= bh1772_ioctl,
};

static struct miscdevice bh1772_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = BH1772_DEV_NAME,
    .fops = &bh1772_fops,
};

/*
static void bh1772_dump_data(uint8_t * buf, int len)
{
	int i;

	for (i=0; i<len; i++)
	{
		printk("%02x ", buf[i]);
		if ( (i+1) % 16  == 0)
			printk("\n");
	}

	if (len % 16)
		printk("\n");
}
*/

static int bh1772_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int res = 0;
    unsigned char buf[2] = {0};
    struct bh1772_data *pdata = NULL;

//	int i=0;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
    {
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

    bh1772_client = client;
    i2c_set_clientdata(client, pdata);

    res = misc_register(&bh1772_device);
    if (res)
    {
        pr_err("%s: bh1772_device register failed\n", __FUNCTION__);
        goto out;
    }
#if DEBUG
    pr_info("%s: bh1772_device register OK\n", __FUNCTION__);
#endif
    res = device_create_file(&client->dev, &dev_attr_bh1772);
    if (res)
    {
        pr_err("%s: device_create_file failed\n", __FUNCTION__);
        goto out_deregister;
    }
#if DEBUG
    pr_info("%s: device_create_file OK\n", __FUNCTION__);
#endif


    ////////////////////////////////////////////////////////////////////////
    // Init BH1772
    ////////////////////////////////////////////////////////////////////////
    buf[0] = 0x5A;
    buf[1] = 0x53;
    if (0 != bh1772_write_data(buf, 2))	//ALS sensitivity is 100%
    {
        pr_err("%s: bh1772_write_data 0x5A, error\n", __FUNCTION__);
        res = -ENODEV;
        goto out_deregister;
    }


    buf[0] = 0x40;
    buf[1] = 0x3;

    if (0 != bh1772_write_data(buf, 2))	//ALS standalone mode
    {
        pr_err("%s: i2c_smbus_write_byte 0x40, error\n", __FUNCTION__);
        res = -ENODEV;
        goto out_deregister;
    }

    buf[0] = 0x41;
    buf[1] = 0x00;
    if (0 != bh1772_write_data(buf, 2))	//ALS PS standalone mode
    {
        pr_err("%s: i2c_smbus_write_byte 0x41, error\n", __FUNCTION__);
        res = -ENODEV;
        goto out_deregister;
    }

    buf[0] = 0x42;
    buf[1] = 0x1B;
    if (0 != bh1772_write_data(buf, 2))	//Iled=100mA
    {
        pr_err("%s: i2c_smbus_write_byte 0x42, error\n", __FUNCTION__);
        res = -ENODEV;
        goto out_deregister;
    }


    buf[0] = 0x45;
    buf[1] = 0x06;
    if (0 != bh1772_write_data(buf, 2))	//PS Meas Rate, 0x06, 200ms
    {
        pr_err("%s: i2c_smbus_write_byte 0x45, error\n", __FUNCTION__);
        res = -ENODEV;
        goto out_deregister;
    }
    /*
    	{
    		unsigned char uc_read_data[32] = {0};
    		unsigned char uc_reg_addr =0x40;
    		int i_ret;

    		msleep(10);
    		i_ret = bh1772_read_data(uc_reg_addr, uc_read_data, sizeof(uc_read_data));

    		printk("bh1772_read_data, i_ret = %d\n", i_ret);
    		bh1772_dump_data(uc_read_data, 32);

    	}
    */
#ifdef CONFIG_HAS_EARLYSUSPEND
    pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    pdata->early_suspend.suspend = bh1772_early_suspend;
    pdata->early_suspend.resume = bh1772_late_resume;
    register_early_suspend(&pdata->early_suspend);
#endif

    printk("bh1772_probe, OK\n");
    return 0;

out_deregister:
    printk("bh1772_probe, failed\n");
    misc_deregister(&bh1772_device);
out:
	if (pdata)
		kfree(pdata);
    return res;
}


static int bh1772_remove(struct i2c_client *client)
{
    device_remove_file(&client->dev, &dev_attr_bh1772);
    misc_deregister(&bh1772_device);

    return 0;
}

static int bh1772_suspend(struct i2c_client *client, pm_message_t state)
{
#if DEBUG
    pr_info("BH1772 suspended\n");
#endif	
    bh1772_set_light_stdby();
    return 0;
}

static int bh1772_resume(struct i2c_client *client)
{
#if DEBUG
    pr_info("BH1772 resumed\n");
#endif	
    bh1772_set_light_act();
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bh1772_early_suspend(struct early_suspend *h)
{
    bh1772_suspend(bh1772_client, PMSG_SUSPEND);
}

static void bh1772_late_resume(struct early_suspend *h)
{
    bh1772_resume(bh1772_client);
}
#endif
static int __init bh1772_init(void)
{
    int res;

    res = i2c_add_driver(&bh1772_driver);
    if (res < 0)
    {
        pr_err("add bh1772 i2c driver failed\n");
        return -ENODEV;
    }

	wake_lock_init(&bh1772_wake_lock, WAKE_LOCK_SUSPEND, "BH1772");
    return (res);
}

static void __exit bh1772_exit(void)
{
    i2c_del_driver(&bh1772_driver);
	wake_lock_destroy(&bh1772_wake_lock);
}

module_init(bh1772_init);
module_exit(bh1772_exit);

MODULE_AUTHOR("Rohm, Inc.");
MODULE_DESCRIPTION("BH1772 light sensor driver");
MODULE_LICENSE("GPL");

