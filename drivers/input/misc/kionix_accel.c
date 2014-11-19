/*
 * kionix_accel.c
 *
 * Copyright 2009 KIONIX Inc. All Rights Reserved.
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

#define DEBUG			1

#define KIONIX_ACCEL_DEV_NAME	"kionix_accel"

#define KIONIX_ACCEL_I2C_NAME	"kionix_accel"

/* Use 'm' as magic number */
#define KIONIX_ACCEL_IOM			'm'

/* IOCTLs for KIONIX_ACCEL device */
#define KIONIX_ACCEL_IOC_STDBY		    _IO (KIONIX_ACCEL_IOM, 0x00)
#define KIONIX_ACCEL_IOC_ACT			_IO (KIONIX_ACCEL_IOM, 0x01)
#define KIONIX_ACCEL_IOC_RESET		    _IO (KIONIX_ACCEL_IOM, 0x02)
#define KIONIX_ACCEL_IOC_READXYZ		_IOR(KIONIX_ACCEL_IOM, 0x03, int[3])

/* Debug Message Flags */
#define KIONIX_KMSG_ERR	1	/* Print kernel debug message for error */
#define KIONIX_KMSG_INF	1	/* Print kernel debug message for info */

#if KIONIX_KMSG_ERR
#define KMSGERR(format, ...)	\
		dev_err(format, ## __VA_ARGS__)
#else
#define KMSGERR(format, ...)
#endif

#if KIONIX_KMSG_INF
#define KMSGINF(format, ...)	\
		dev_info(format, ## __VA_ARGS__)
#else
#define KMSGINF(format, ...)
#endif


/******************************************************************************
 * Accelerometer WHO_AM_I return value
 *****************************************************************************/
#define KIONIX_ACCEL_WHO_AM_I_KXTE9 		0x00
#define KIONIX_ACCEL_WHO_AM_I_KXTF9 		0x01
#define KIONIX_ACCEL_WHO_AM_I_KXTI9_1001 	0x04
#define KIONIX_ACCEL_WHO_AM_I_KXTIK_1004 	0x05
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005 	0x07
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007 	0x08
#define KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008 	0x0A
#define KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009 	0x09
#define KIONIX_ACCEL_WHO_AM_I_KXCJK_1013 	0x11

/******************************************************************************
 * Accelerometer Grouping
 *****************************************************************************/
#define KIONIX_ACCEL_GRP1	1	/* KXTE9 */
#define KIONIX_ACCEL_GRP2	2	/* KXTF9/I9-1001/J9-1005 */
#define KIONIX_ACCEL_GRP3	3	/* KXTIK-1004 */
#define KIONIX_ACCEL_GRP4	4	/* KXTJ9-1007/KXCJ9-1008 */
#define KIONIX_ACCEL_GRP5	5	/* KXTJ2-1009 */
#define KIONIX_ACCEL_GRP6	6	/* KXCJK-1013 */

/******************************************************************************
 * Registers for Accelerometer Group 1 & 2 & 3
 *****************************************************************************/
#define ACCEL_WHO_AM_I		0x0F

/*****************************************************************************/
/* Registers for Accelerometer Group 1 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP1_XOUT			0x12
/* Control Registers */
#define ACCEL_GRP1_CTRL_REG1	0x1B
/* CTRL_REG1 */
#define ACCEL_GRP1_PC1_OFF		0x7F
#define ACCEL_GRP1_PC1_ON		(1 << 7)
#define ACCEL_GRP1_ODR40		(3 << 3)
#define ACCEL_GRP1_ODR10		(2 << 3)
#define ACCEL_GRP1_ODR3			(1 << 3)
#define ACCEL_GRP1_ODR1			(0 << 3)
#define ACCEL_GRP1_ODR_MASK		(3 << 3)

/*****************************************************************************/
/* Registers for Accelerometer Group 2 & 3 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP2_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP2_INT_REL		0x1A
#define ACCEL_GRP2_CTRL_REG1	0x1B
#define ACCEL_GRP2_INT_CTRL1	0x1E
#define ACCEL_GRP2_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP2_PC1_OFF		0x7F
#define ACCEL_GRP2_PC1_ON		(1 << 7)
#define ACCEL_GRP2_DRDYE		(1 << 5)
#define ACCEL_GRP2_G_8G			(2 << 3)
#define ACCEL_GRP2_G_4G			(1 << 3)
#define ACCEL_GRP2_G_2G			(0 << 3)
#define ACCEL_GRP2_G_MASK		(3 << 3)
#define ACCEL_GRP2_RES_8BIT		(0 << 6)
#define ACCEL_GRP2_RES_12BIT	(1 << 6)
#define ACCEL_GRP2_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP2_IEA			(1 << 4)
#define ACCEL_GRP2_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP2_ODR12_5		0x00
#define ACCEL_GRP2_ODR25		0x01
#define ACCEL_GRP2_ODR50		0x02
#define ACCEL_GRP2_ODR100		0x03
#define ACCEL_GRP2_ODR200		0x04
#define ACCEL_GRP2_ODR400		0x05
#define ACCEL_GRP2_ODR800		0x06
/*****************************************************************************/

/*****************************************************************************/
/* Registers for Accelerometer Group 4 & 5 & 6 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP4_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP4_INT_REL		0x1A
#define ACCEL_GRP4_CTRL_REG1	0x1B
#define ACCEL_GRP4_INT_CTRL1	0x1E
#define ACCEL_GRP4_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP4_PC1_OFF		0x7F
#define ACCEL_GRP4_PC1_ON		(1 << 7)
#define ACCEL_GRP4_DRDYE		(1 << 5)
#define ACCEL_GRP4_G_8G			(2 << 3)
#define ACCEL_GRP4_G_4G			(1 << 3)
#define ACCEL_GRP4_G_2G			(0 << 3)
#define ACCEL_GRP4_G_MASK		(3 << 3)
#define ACCEL_GRP4_RES_8BIT		(0 << 6)
#define ACCEL_GRP4_RES_12BIT	(1 << 6)
#define ACCEL_GRP4_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP4_IEA			(1 << 4)
#define ACCEL_GRP4_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP4_ODR0_781		0x08
#define ACCEL_GRP4_ODR1_563		0x09
#define ACCEL_GRP4_ODR3_125		0x0A
#define ACCEL_GRP4_ODR6_25		0x0B
#define ACCEL_GRP4_ODR12_5		0x00
#define ACCEL_GRP4_ODR25		0x01
#define ACCEL_GRP4_ODR50		0x02
#define ACCEL_GRP4_ODR100		0x03
#define ACCEL_GRP4_ODR200		0x04
#define ACCEL_GRP4_ODR400		0x05
#define ACCEL_GRP4_ODR800		0x06
#define ACCEL_GRP4_ODR1600		0x07
/*****************************************************************************/

/* Input Event Constants */
#define ACCEL_G_MAX			8096
#define ACCEL_FUZZ			3
#define ACCEL_FLAT			3
/* I2C Retry Constants */
#define KIONIX_I2C_RETRY_COUNT		10 	/* Number of times to retry i2c */
#define KIONIX_I2C_RETRY_TIMEOUT	1	/* Timeout between retry (miliseconds) */

/* Earlysuspend Contants */
#define KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT	5000	/* Timeout (miliseconds) */

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp4_odr_table[] = {
	{ 2,	ACCEL_GRP4_ODR1600 },
	{ 3,	ACCEL_GRP4_ODR800 },
	{ 5,	ACCEL_GRP4_ODR400 },
	{ 10,	ACCEL_GRP4_ODR200 },
	{ 20,	ACCEL_GRP4_ODR100 },
	{ 40,	ACCEL_GRP4_ODR50  },
	{ 80,	ACCEL_GRP4_ODR25  },
	{ 160,	ACCEL_GRP4_ODR12_5},
	{ 320,	ACCEL_GRP4_ODR6_25},
	{ 640,	ACCEL_GRP4_ODR3_125},
	{ 1280,	ACCEL_GRP4_ODR1_563},
	{ 0,	ACCEL_GRP4_ODR0_781},
};

enum {
	accel_grp4_ctrl_reg1 = 0,
	accel_grp4_data_ctrl,
	accel_grp4_int_ctrl,
	accel_grp4_regs_count,
};

//static u8 kionix_accel_registers[accel_grp4_regs_count] = {ACCEL_GRP4_RES_12BIT|ACCEL_GRP4_G_2G, 0, 0};
static u8 kionix_accel_registers[accel_grp4_regs_count] = {ACCEL_GRP4_RES_12BIT, 0, 0};


struct kionix_accel_data
{
    struct early_suspend early_suspend;
};

static const struct i2c_device_id kionix_accel_id[] =
{
    { KIONIX_ACCEL_I2C_NAME, 0 },
    { }
};

static int kionix_accel_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int kionix_accel_remove(struct i2c_client *client);
static int kionix_accel_suspend(struct i2c_client *client, pm_message_t state);
static int kionix_accel_resume(struct i2c_client *client);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void kionix_accel_early_suspend(struct early_suspend *h);
static void kionix_accel_late_resume(struct early_suspend *h);
#endif

static struct i2c_client 	*kionix_accel_client;

static struct i2c_driver kionix_accel_driver =
{
    .probe 		= kionix_accel_probe,
    .remove 	= kionix_accel_remove,
    .id_table	= kionix_accel_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend 	= kionix_accel_suspend,
    .resume 	= kionix_accel_resume,
#endif
    .driver 	= {
        .owner	= THIS_MODULE,
        .name	= KIONIX_ACCEL_I2C_NAME,
    },
};

static int kionix_i2c_read(struct i2c_client *client, u8 addr, u8 *data, int len)
{
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	return i2c_transfer(client->adapter, msgs, 2);
}

/* read sensor data from kionix_accel */
static int kionix_accel_read_data(short *x, short *y, short *z)
{
	u8 accel_data[7];

    //s16 accel_data_s16[8];
    //int accel_data[8];

	s16 x1, y1, z1;
	int err;   

	//u8 shift = 4;
    //unsigned int direction = 1;
	//u8 axis_map_x = ((direction-1)%2);
	//u8 axis_map_y =  (direction%2);
	//u8 axis_map_z =  2;
	//bool negate_z = ((direction-1)/4);
    //bool negate_x = (((direction+2)/2)%2);
	//bool negate_y = (((direction+5)/4)%2);
	
    err = kionix_i2c_read(kionix_accel_client, ACCEL_GRP4_XOUT_L, accel_data, 6);
    if (err)
    {
        pr_err("%s, failed\n", __func__);        
    }
    //printk("KIONIX_ACCEL: [X - %04x : %04x ] [Y - %04x : %04x] [Z - %04x : %04x]\n", 
    //    accel_data[0], accel_data[1], accel_data[2], accel_data[3], accel_data[4], accel_data[5]);

    //x1 = ((s16) le16_to_cpu(accel_data_s16[axis_map_x])) >> shift;
	//y1 = ((s16) le16_to_cpu(accel_data_s16[axis_map_y])) >> shift;
	//z1 = ((s16) le16_to_cpu(accel_data_s16[axis_map_z])) >> shift;

    //accel_data[axis_map_x] = (negate_x ? -x1 : x1) + acceld->accel_cali[axis_map_x];
	//accel_data[axis_map_y] = (negate_y ? -y1 : y1) + acceld->accel_cali[axis_map_y];
	//accel_data[axis_map_z] = (negate_z ? -z1 : z1) + acceld->accel_cali[axis_map_z];

    x1 = accel_data[1];
    x1 = (x1 << 8) + accel_data[0];
    *x = x1 >> 4;
    
    y1 = accel_data[3];
    y1 = (y1 << 8) + accel_data[2];
    *y = y1 >> 4;

    z1 = accel_data[5];
    z1 = (z1 << 8) + accel_data[4];
    *z = z1 >> 4;
    
    return 0;
}

static int kionix_accel_grp4_update_odr(unsigned int poll_interval)
{
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp4_odr_table); i++) {
		odr = kionix_accel_grp4_odr_table[i].mask;
		if (poll_interval < kionix_accel_grp4_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update DATA_CTRL_REG register if the ODR is not changed */
	if(kionix_accel_registers[accel_grp4_data_ctrl] == odr)
		return 0;
	else
		kionix_accel_registers[accel_grp4_data_ctrl] = odr;

	return 0;
}

static int kionix_accel_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int kionix_accel_release(struct inode *inode, struct file *file)
{
    return 0;
}

void kionix_accel_enable(void)
{
    int err;
	err = i2c_smbus_write_byte_data(kionix_accel_client, ACCEL_GRP4_CTRL_REG1, 
			kionix_accel_registers[accel_grp4_ctrl_reg1] | ACCEL_GRP4_PC1_ON);
    if (err)
    {
        pr_err("%s, failed\n", __func__);        
    }
    return;
}

void kionix_accel_disable(void)
{
    int err;

	err = i2c_smbus_write_byte_data(kionix_accel_client, ACCEL_GRP4_CTRL_REG1, 0);
	if (err < 0)
	{
        pr_err("%s, failed\n", __func__);        
    }
    
    return;
}

static long kionix_accel_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *pa = (void __user *)arg;
    short data[3] = {0};

    switch (cmd)
    {
    case KIONIX_ACCEL_IOC_STDBY:
        //printk("kionix_accel_ioctl: KIONIX_ACCEL_IOC_STDBY");
        kionix_accel_disable();
        /* wait STDBY done */
        msleep(KIONIX_ACCEL_IOC_STDBY);
        break;
    case KIONIX_ACCEL_IOC_ACT:
        //printk("kionix_accel_ioctl: KIONIX_ACCEL_IOC_ACT");
        kionix_accel_enable();
        /* wait ACTIVE done */
        msleep(KIONIX_ACCEL_IOC_ACT);
        break;
    case KIONIX_ACCEL_IOC_RESET:
        //printk("kionix_accel_ioctl: KIONIX_ACCEL_IOC_RESET");
        break;
    case KIONIX_ACCEL_IOC_READXYZ:
        //printk("kionix_accel_ioctl: KIONIX_ACCEL_IOC_READXYZ\n");
        kionix_accel_read_data(&data[0], &data[1], &data[2]);
#ifdef CONFIG_SENSORS_KIONIX_ACCEL_TARGET_TAIJI        
        data[1] = data[1] - 40;
#endif        

#if DEBUG
        //printk("to user: KIONIX_ACCEL: [X - %04x] [Y - %04x] [Z - %04x]\n", data[0], data[1], data[2]);
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

static ssize_t kionix_accel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    sprintf(buf, "kionix_accel");
    ret = strlen(buf) + 1;

    return ret;
}

static DEVICE_ATTR(kionix_accel, S_IRUGO, kionix_accel_show, NULL);

static struct file_operations kionix_accel_fops =
{
    .owner		= THIS_MODULE,
    .open		= kionix_accel_open,
    .release	= kionix_accel_release,
    .unlocked_ioctl		= kionix_accel_ioctl,
};

static struct miscdevice kionix_accel_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = KIONIX_ACCEL_DEV_NAME,
    .fops = &kionix_accel_fops,
};

static int kionix_init_chip(void)
{
	int err;

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(kionix_accel_client, ACCEL_GRP4_CTRL_REG1, 0);
	if (err < 0) {
        pr_err("%s: ACCEL_GRP4_CTRL_REG1 error\n", __func__);
		return err;
	}

	err = i2c_smbus_write_byte_data(kionix_accel_client, ACCEL_GRP4_DATA_CTRL, kionix_accel_registers[accel_grp4_data_ctrl]);
	if (err < 0) {
        pr_err("%s: ACCEL_GRP4_DATA_CTRL error\n", __func__);
		return err;
	}

	err = i2c_smbus_write_byte_data(kionix_accel_client, ACCEL_GRP4_CTRL_REG1, kionix_accel_registers[accel_grp4_ctrl_reg1]);
	if (err < 0) {
        pr_err("%s: ACCEL_GRP4_CTRL_REG1 error\n", __func__);
		return err;
	}
    
    return 0;
}

static int __devinit kionix_accel_verify(struct i2c_client *client)
{
	int retval = i2c_smbus_read_byte_data(client, ACCEL_WHO_AM_I);

#if KIONIX_KMSG_INF
	switch (retval) {
		case KIONIX_ACCEL_WHO_AM_I_KXTE9:
			KMSGINF(&client->dev, "this accelerometer is a KXTE9.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTF9:
			KMSGINF(&client->dev, "this accelerometer is a KXTF9.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTI9_1001:
			KMSGINF(&client->dev, "this accelerometer is a KXTI9-1001.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTIK_1004:
			KMSGINF(&client->dev, "this accelerometer is a KXTIK-1004.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005:
			KMSGINF(&client->dev, "this accelerometer is a KXTJ9-1005.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007:
			KMSGINF(&client->dev, "this accelerometer is a KXTJ9-1007.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008:
			KMSGINF(&client->dev, "this accelerometer is a KXCJ9-1008.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009:
			KMSGINF(&client->dev, "this accelerometer is a KXTJ2-1009.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXCJK_1013:
			KMSGINF(&client->dev, "this accelerometer is a KXCJK-1013.\n");
			break;
		default:
			break;
	}
#endif

	return retval;
}

static int kionix_accel_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int res = 0;
    struct kionix_accel_data *pdata = NULL;

    printk("%s, begin\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA )) {
		pr_err("client is not i2c capable. Abort.\n");
		return -ENXIO;
	}

    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (pdata == NULL)
    {
        res = -ENOMEM;
        goto out;
    }
    kionix_accel_client = client;
    
    res = kionix_accel_verify(client);
	if (res < 0) {
		pr_err("%s: kionix_accel_verify returned err = 0x%x. Abort.\n", __func__, res);
		goto out;
	}

    i2c_set_clientdata(client, pdata);

    res = misc_register(&kionix_accel_device);
    if (res)
    {
        pr_err("%s: kionix_accel_device register failed\n", __FUNCTION__);
        goto out;
    }

    res = device_create_file(&client->dev, &dev_attr_kionix_accel);
    if (res)
    {
        pr_err("%s: device_create_file failed\n", __FUNCTION__);
        goto out_deregister;
    }

    kionix_accel_grp4_update_odr(500);
    
    if (kionix_init_chip())
    {
        pr_err("%s: kionix_init_chip error\n", __func__);
        goto out_deregister;
    }

    kionix_accel_enable();

#ifdef CONFIG_HAS_EARLYSUSPEND
    pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    pdata->early_suspend.suspend = kionix_accel_early_suspend;
    pdata->early_suspend.resume = kionix_accel_late_resume;
    register_early_suspend(&pdata->early_suspend);
#endif

    printk("%s: probe OK\n", __func__);
    return 0;

out_deregister:
    misc_deregister(&kionix_accel_device);

out:
    if (pdata)
        kfree(pdata);

    pr_err("%s: probe failed\n", __func__);
    return res;
}


static int kionix_accel_remove(struct i2c_client *client)
{
    device_remove_file(&client->dev, &dev_attr_kionix_accel);
    misc_deregister(&kionix_accel_device);

    return 0;
}

static int kionix_accel_suspend(struct i2c_client *client, pm_message_t state)
{
//    kionix_accel_disable();
    return 0;
}

static int kionix_accel_resume(struct i2c_client *client)
{
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void kionix_accel_early_suspend(struct early_suspend *h)
{
    kionix_accel_suspend(kionix_accel_client, PMSG_SUSPEND);
}

static void kionix_accel_late_resume(struct early_suspend *h)
{
    kionix_accel_resume(kionix_accel_client);
}
#endif

static int __init kionix_accel_init(void)
{
    int res;

    res = i2c_add_driver(&kionix_accel_driver);
    if (res < 0)
    {
        pr_info("add kionix_accel i2c driver failed, abc\n");
        return -ENODEV;
    }
//    pr_info("add kionix_accel i2c driver\n");

    return (res);
}

static void __exit kionix_accel_exit(void)
{
    pr_info("kionix_accel driver: exit\n");
    i2c_del_driver(&kionix_accel_driver);
}

late_initcall(kionix_accel_init);
module_exit(kionix_accel_exit);

MODULE_AUTHOR("KIONIX, Inc.");
MODULE_DESCRIPTION("KIONIX ACCEL sensor driver");
MODULE_LICENSE("GPL");

