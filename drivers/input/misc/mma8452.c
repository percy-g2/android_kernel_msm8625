/*
 * mma8452.c
 *
 * Copyright 2009 Freescale Semiconductor Inc. All Rights Reserved.
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

#define MMA8452_DEV_NAME	"mma8452"

#define MMA8452_I2C_NAME	"mma8452"

/* Use 'm' as magic number */
#define MMA8452_IOM			'm'

/* IOCTLs for MMA8452 device */
#define MMA8452_IOC_STDBY		    _IO (MMA8452_IOM, 0x00)
#define MMA8452_IOC_ACT			_IO (MMA8452_IOM, 0x01)
#define MMA8452_IOC_RESET		    _IO (MMA8452_IOM, 0x02)
#define MMA8452_IOC_READXYZ		_IOR(MMA8452_IOM, 0x03, int[3])

/* register enum for mma8452 registers */
enum
{
    MMA8452_STATUS = 0x00,
    MMA8452_OUT_X_MSB,
    MMA8452_OUT_X_LSB,
    MMA8452_OUT_Y_MSB,
    MMA8452_OUT_Y_LSB,
    MMA8452_OUT_Z_MSB,
    MMA8452_OUT_Z_LSB,

    MMA8452_SYSMOD = 0x0B,
    MMA8452_INT_SOURCE,
    MMA8452_WHO_AM_I,
    MMA8452_XYZ_DATA_CFG,
    MMA8452_HP_FILTER_CUTOFF,

    MMA8452_PL_STATUS,
    MMA8452_PL_CFG,
    MMA8452_PL_COUNT,
    MMA8452_PL_BF_ZCOMP,
    MMA8452_PL_P_L_THS_REG,

    MMA8452_FF_MT_CFG,
    MMA8452_FF_MT_SRC,
    MMA8452_FF_MT_THS,
    MMA8452_FF_MT_COUNT,

    MMA8452_TRANSIENT_CFG = 0x1D,
    MMA8452_TRANSIENT_SRC,
    MMA8452_TRANSIENT_THS,
    MMA8452_TRANSIENT_COUNT,

    MMA8452_PULSE_CFG,
    MMA8452_PULSE_SRC,
    MMA8452_PULSE_THSX,
    MMA8452_PULSE_THSY,
    MMA8452_PULSE_THSZ,
    MMA8452_PULSE_TMLT,
    MMA8452_PULSE_LTCY,
    MMA8452_PULSE_WIND,

    MMA8452_ASLP_COUNT,
    MMA8452_CTRL_REG1,
    MMA8452_CTRL_REG2,
    MMA8452_CTRL_REG3,
    MMA8452_CTRL_REG4,
    MMA8452_CTRL_REG5,

    MMA8452_OFF_X,
    MMA8452_OFF_Y,
    MMA8452_OFF_Z,

    MMA8452_REG_END,
};

enum
{
    MMA8452_MODE_2G = 0,
    MMA8452_MODE_4G,
    MMA8452_MODE_8G,
};

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

struct mma8452_data
{
    struct early_suspend early_suspend;
};

static const struct i2c_device_id mma8452_id[] =
{
    { MMA8452_I2C_NAME, 0 },
    { }
};

static int mma8452_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int mma8452_remove(struct i2c_client *client);
static int mma8452_suspend(struct i2c_client *client, pm_message_t state);
static int mma8452_resume(struct i2c_client *client);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mma8452_early_suspend(struct early_suspend *h);
static void mma8452_late_resume(struct early_suspend *h);
#endif

static struct i2c_client 	*mma8452_client;

static struct i2c_driver mma8452_driver =
{
    .probe 		= mma8452_probe,
    .remove 	= mma8452_remove,
    .id_table	= mma8452_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend 	= mma8452_suspend,
    .resume 	= mma8452_resume,
#endif
    .driver 	= {
        .owner	= THIS_MODULE,
        .name	= MMA8452_I2C_NAME,
    },
};
/* mma8452 status */
struct mma8452_status
{
    u8 mode;
    u8 ctl_reg1;
};

static struct mma8452_status mma_status =
{
    .mode 	= 0,
    .ctl_reg1	= 0
};


/* read sensor data from mma8452 */
static int mma8452_read_data(short *x, short *y, short *z)
{
    u8	tmp_data[7];

    if (i2c_smbus_read_i2c_block_data(mma8452_client, MMA8452_OUT_X_MSB, 7, tmp_data) < 7)
    {
        dev_err(&mma8452_client->dev, "i2c block read failed\n");
        return -3;
    }

    *x = ((tmp_data[0] << 8) & 0xff00) | tmp_data[1];
    *y = ((tmp_data[2] << 8) & 0xff00) | tmp_data[3];
    *z = ((tmp_data[4] << 8) & 0xff00) | tmp_data[5];

    *x = (short)(*x) >> 4;
    *y = (short)(*y) >> 4;
    *z = (short)(*z) >> 4;

    if (mma_status.mode == MMA8452_MODE_4G)
    {
        (*x)=(*x)<<1;
        (*y)=(*y)<<1;
        (*z)=(*z)<<1;
    }
    else if (mma_status.mode == MMA8452_MODE_8G)
    {
        (*x)=(*x)<<2;
        (*y)=(*y)<<2;
        (*z)=(*z)<<2;
    }

    return 0;
}


static int mma8452_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int mma8452_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long mma8452_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *pa = (void __user *)arg;
    short data[3] = {0};

    switch (cmd)
    {
    case MMA8452_IOC_STDBY:
        if(i2c_smbus_write_byte_data(mma8452_client, REG_MODE, 0x00) < 0)
        {
            pr_err("MMA8452_IOC_STDBY Error!\n");
            return -EFAULT;
        }
        /* wait STDBY done */
        msleep(MMA8452_IOC_STDBY);
        break;
    case MMA8452_IOC_ACT:
        if(i2c_smbus_write_byte_data(mma8452_client, REG_MODE, 0x01) < 0)
        {
            pr_err("MMA8452_IOC_ACT Error!\n");
            return -EFAULT;
        }
        /* wait ACTIVE done */
        msleep(MMA8452_IOC_ACT);
        break;
    case MMA8452_IOC_RESET:
        break;
    case MMA8452_IOC_READXYZ:
        mma8452_read_data(&data[0], &data[1], &data[2]);
#ifdef CONFIG_SENSORS_MMA8452_TARGET_TAIJI        
        data[1] = data[1] - 40;
#endif        

#if DEBUG
        printk("MMA8452: [X - %04x] [Y - %04x] [Z - %04x]\n",
               data[0], data[1], data[2]);
#endif

        if (copy_to_user(pa, data, sizeof(data)))
        {
            return -EFAULT;
        }
        break;
    default:
        break;
    }

    return 0;
}

static ssize_t mma8452_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    sprintf(buf, "mma8452");
    ret = strlen(buf) + 1;

    return ret;
}

static DEVICE_ATTR(mma8452, S_IRUGO, mma8452_show, NULL);

static struct file_operations mma8452_fops =
{
    .owner		= THIS_MODULE,
    .open		= mma8452_open,
    .release	= mma8452_release,
    .unlocked_ioctl		= mma8452_ioctl,
};

static struct miscdevice mma8452_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = MMA8452_DEV_NAME,
    .fops = &mma8452_fops,
};


static int mma8452_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int res = 0;
    struct mma8452_data *pdata = NULL;


    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
    {
        pr_err("%s: functionality check failed\n", __FUNCTION__);
        res = -ENODEV;
        goto out;
    }

    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (pdata == NULL)
    {
        res = -ENOMEM;
        goto out;
    }
    mma8452_client = client;
    i2c_set_clientdata(client, pdata);

    res = misc_register(&mma8452_device);
    if (res)
    {
        pr_err("%s: mma8452_device register failed\n", __FUNCTION__);
        goto out;
    }

    res = device_create_file(&client->dev, &dev_attr_mma8452);
    if (res)
    {
        pr_err("%s: device_create_file failed\n", __FUNCTION__);
        goto out_deregister;
    }

    mma_status.ctl_reg1 = 0x20;
    if (0 != i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1, mma_status.ctl_reg1))
    {
        pr_err("%s: i2c_smbus_write_byte error\n", __FUNCTION__);
        goto out_deregister;
    }

    mma_status.mode = MMA8452_MODE_2G;
    if (0 != i2c_smbus_write_byte_data(client, MMA8452_XYZ_DATA_CFG, MMA8452_MODE_2G))
    {
        pr_err("%s: i2c_smbus_write_byte error\n", __FUNCTION__);
        goto out_deregister;
    }

    mma_status.ctl_reg1 |= 0x01;
    if ( 0 != i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1, mma_status.ctl_reg1))
    {
        pr_err("%s: i2c_smbus_write_byte error\n", __FUNCTION__);
        goto out_deregister;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    pdata->early_suspend.suspend = mma8452_early_suspend;
    pdata->early_suspend.resume = mma8452_late_resume;
    register_early_suspend(&pdata->early_suspend);
#endif

    printk("%s: probe OK\n", __FUNCTION__);
    return 0;

out_deregister:
    misc_deregister(&mma8452_device);

out:
    if (pdata)
        kfree(pdata);

    printk("%s: probe failed\n", __FUNCTION__);
    return res;
}


static int mma8452_remove(struct i2c_client *client)
{
    device_remove_file(&client->dev, &dev_attr_mma8452);
    misc_deregister(&mma8452_device);

    return 0;
}

static int mma8452_suspend(struct i2c_client *client, pm_message_t state)
{
    int result;
    s32 ctl_reg;
    ctl_reg = i2c_smbus_read_byte_data(client, MMA8452_CTRL_REG1);
    if (ctl_reg < 0)
        return -1;

    mma_status.ctl_reg1 = (u8)ctl_reg;
    result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1,mma_status.ctl_reg1 & 0xFE);

    pr_info("MMA8452 suspended\n");
    return result;
}

static int mma8452_resume(struct i2c_client *client)
{

    pr_info("MMA8452 resumed\n");
    if (0 != i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1, mma_status.ctl_reg1 & 0xFE))
    {
	    return -1;
    }

    if (0 != i2c_smbus_write_byte_data(client, MMA8452_XYZ_DATA_CFG, MMA8452_MODE_2G))
    {
    	return -1;
    }
	
    if (0 != i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1, mma_status.ctl_reg1))
    {
    	return -1;
    }

    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mma8452_early_suspend(struct early_suspend *h)
{
    mma8452_suspend(mma8452_client, PMSG_SUSPEND);
}

static void mma8452_late_resume(struct early_suspend *h)
{
    mma8452_resume(mma8452_client);
}
#endif

static int __init mma8452_init(void)
{
    int res;

    res = i2c_add_driver(&mma8452_driver);
    if (res < 0)
    {
        pr_info("add mma8452 i2c driver failed\n");
        return -ENODEV;
    }
//    pr_info("add mma8452 i2c driver\n");

    return (res);
}

static void __exit mma8452_exit(void)
{
    pr_info("mma8452 driver: exit\n");
    i2c_del_driver(&mma8452_driver);
}

late_initcall(mma8452_init);
module_exit(mma8452_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA8452 sensor driver");
MODULE_LICENSE("GPL");

