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

//#define CM_DEBUG

#ifdef CM_DEBUG
#define CMDBG(x) printk x
#else
#define CMDBG(x)
#endif


#define CM3623_DEV_NAME	"cm3623"

#define CM3623_I2C_NAME	"cm3623"
#define CM3623_I2C_ADDR   0x49
#define CM3623_RETRY_COUNT 3

/* Use 'm' as magic number */
#define CM3623_IOM			'l'

/* IOCTLs for CM3623 device */
#define CM3623_IOC_PROXIMITY_STDBY		_IO (CM3623_IOM, 0x01)
#define CM3623_IOC_PROXIMITY_ACT		_IO (CM3623_IOM, 0x02)
#define CM3623_IOC_RESET		        _IO (CM3623_IOM, 0x03)
#define CM3623_IOC_READXYZ		        _IOR(CM3623_IOM, 0x04, int[1])


#define CM3623_DELAY_STDBY	10	/* ms */
#define CM3623_DELAY_ACT	10	/* ms */

#define PS_MAX  255
#define ALS_MAX 10000

#define CM3623_I2C_GROUP_NUM 2
struct cm3623_i2c_group{
	unsigned char als_wr;
	unsigned char als_rd_msb;
	unsigned char als_rd_lsb;
	unsigned char ps_wr;
	unsigned char ps_rd;
	unsigned char ps_thd_wr;
	unsigned char init;
	unsigned char ara;
};

struct cm3623_i2c_group cm3623_i2c_groups[CM3623_I2C_GROUP_NUM] = {
	//AD
	{
		.als_wr     = 0x90,
		.als_rd_msb = 0x91,
		.als_rd_lsb = 0x93,
		.ps_wr      = 0xF0,
		.ps_rd      = 0xF1,
		.ps_thd_wr  = 0xF2,
		.init       = 0x92,
		.ara        = 0x0C
	},
	
	{
		.als_wr 	= 0x20,
		.als_rd_msb = 0x21,
		.als_rd_lsb = 0x23,
		.ps_wr		= 0xB0,
		.ps_rd		= 0xB1,
		.ps_thd_wr	= 0xB2,
		.init		= 0x22,
		.ara		= 0x18
	}
};

struct cm3623_data
{
	struct i2c_client *cm3623_client;
	atomic_t delay;
	int als_enable;
	int ps_enable;
	unsigned char als_threshold;
	unsigned char ps_threshold;
	struct input_dev *input;
	struct mutex value_mutex;
	struct delayed_work work;
    struct early_suspend early_suspend;
	struct cm3623_i2c_group * i2c;
};

static const struct i2c_device_id cm3623_id[] =
{
    { CM3623_I2C_NAME, 0 },
    { }
};

struct wake_lock cm3623_wake_lock;

static int cm3623_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int cm3623_remove(struct i2c_client *client);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cm3623_early_suspend(struct early_suspend *h);
static void cm3623_late_resume(struct early_suspend *h);
#endif

static int cm3623_suspend(struct i2c_client *client, pm_message_t state);
static int cm3623_resume(struct i2c_client *client);

static struct i2c_client * cm3623_client = NULL;

static struct i2c_driver cm3623_driver =
{
    .probe 		= cm3623_probe,
    .remove 	= cm3623_remove,
    .id_table	= cm3623_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend	= cm3623_suspend,
    .resume		= cm3623_resume,
#endif
    .driver 	= {
        .owner	= THIS_MODULE,
        .name	= CM3623_I2C_NAME,
    },
};

static struct cm3623_data * p_cm3623_data = NULL;

int is_proximity_sensor_working(void)
{
#if 0	
	if (p_cm3623_data)
	{
		return p_cm3623_data->ps_enable;
	}
	return 0;
#else
	return 1;
#endif	
}

EXPORT_SYMBOL(is_proximity_sensor_working);

static int cm3623_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int cm3623_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int cm3623_i2c_read_byte(struct i2c_client *client,
		unsigned char i2c_addr)
{
	int i;
	unsigned char data;
	struct i2c_msg msgs[] = {
		{
			.addr	= i2c_addr/2,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= &data,
		}
	};

	for (i = 0; i < CM3623_RETRY_COUNT; i++) {
		if (i2c_transfer(cm3623_client->adapter, msgs, 1) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= CM3623_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, CM3623_RETRY_COUNT);
		return -EIO;
	}

	return (int)data;
	
}

static int cm3623_i2c_write_byte(struct i2c_client *client,
		unsigned char i2c_addr, unsigned char data)
{
	int i;
	struct i2c_msg msg[] = {
		{
			.addr	= i2c_addr/2,
			.flags	= 0,
			.len	= 1,
			.buf	= &data,
		}
	};

	for (i = 0; i < CM3623_RETRY_COUNT; i++) {
		if (i2c_transfer(cm3623_client->adapter, msg, 1) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= CM3623_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, CM3623_RETRY_COUNT);
		return -EIO;
	}
	return 0;
	
}

static int cm3623_read_ps(struct i2c_client *client)
{
	int ret = 0;
	struct cm3623_data *cm3623 = i2c_get_clientdata(client);
	struct cm3623_i2c_group * p = cm3623->i2c;

	if (cm3623->ps_enable)
		ret = cm3623_i2c_read_byte(client, p->ps_rd);

	return ret;
}

static int cm3623_read_als(struct i2c_client *client)
{
	int ret = 0;
	int val;
	struct cm3623_data *cm3623 = i2c_get_clientdata(client);
	struct cm3623_i2c_group * p = cm3623->i2c;
	val = cm3623_i2c_read_byte(client, p->als_rd_lsb);
	CMDBG(("als low value 0x%x\n", val));
	if(val < 0)
		return 0;
	
	val &= 0xFF;
	ret |= val;
	val = cm3623_i2c_read_byte(client, p->als_rd_msb);
	CMDBG(("als high value 0x%x\n", val));
	
	if(val < 0)
		return val;
	val &= 0xFF;
	ret |= (val << 8);

	CMDBG(("cm3623_read_als, value %d\n", ret));
	return ret;
}

static int cm3623_get_als(struct i2c_client *client)
{
	int ret;
	ret = cm3623_read_als(client);
	ret = ret * 35 / 100;	/* The parameter for conversing lux is 0.35,
				               please contact with vendor for detail info. */

	if(ret > ALS_MAX)
		ret = ALS_MAX;

	return ret;
}

static int cm3623_get_ps(struct i2c_client *client)
{
	int ret;
	ret = cm3623_read_ps(client);
	return ret;
}

static int cm3623_state(struct cm3623_data *cm3623, int als_on, int ps_on)
{
	int rc; 
	u8 val;
	struct cm3623_i2c_group * p = cm3623->i2c;
	val = 0x20;
	rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->init, val);
	if(rc < 0)
		pr_err("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);
	als_on ?(val = 0x06):(val = 1);
	rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->als_wr, val);
	if(rc < 0)
		pr_err("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);
	val = cm3623->ps_threshold; // THD_PS
	rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->ps_thd_wr, val);
	if(rc < 0)
		pr_err("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);
	ps_on?(val = 0x30):(val = 1);
	rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->ps_wr, val);
	if(rc < 0)
		pr_err("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);
	return rc; 
}

static int cm3623_set_light_stdby(void)
{
	if (p_cm3623_data)
	{
		p_cm3623_data->als_enable = 0;
		cm3623_state(p_cm3623_data, p_cm3623_data->als_enable, p_cm3623_data->ps_enable);
	}
    return 0;
}

static int cm3623_set_light_act(void)
{
	if (p_cm3623_data)
	{
		p_cm3623_data->als_enable = 1;
		cm3623_state(p_cm3623_data, p_cm3623_data->als_enable, p_cm3623_data->ps_enable);
	}
    return 0;
}

static int cm3623_set_proximity_stdby(void)
{
	if (p_cm3623_data)
	{
		wake_unlock(&cm3623_wake_lock);
		p_cm3623_data->ps_enable = 0;
		cm3623_state(p_cm3623_data, p_cm3623_data->als_enable, p_cm3623_data->ps_enable);
	}
    return 0;
}

static int cm3623_set_proximity_act(void)
{
	if (p_cm3623_data)
	{
		wake_lock(&cm3623_wake_lock);
		p_cm3623_data->ps_enable = 1;
		cm3623_state(p_cm3623_data, p_cm3623_data->als_enable, p_cm3623_data->ps_enable);
	}
    return 0;
}

static int cm3623_config(struct cm3623_data *cm3623)
{
	int rc = -1; 
	u8 val;
	int i;
	struct cm3623_i2c_group * p;
	
	for (i=0; i<CM3623_I2C_GROUP_NUM; i++)
	{
		cm3623->i2c = cm3623_i2c_groups + i;
		p = cm3623->i2c;
		val = 0x20;
		rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->init, val);
		if (rc < 0)
		{
			pr_err("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);
			continue;
		}

		val = (1 << 0); // SD_ALS
		val |= (1 << 1); // WDM
		val |= (3 << 2); // IT_ALS
		val |= (0 << 4); // THD_ALS
		val |= (0 << 6); // GAIN_ALS
		rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->als_wr, val);
		if(rc < 0)
		{
			pr_err("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);
			continue;
		}

		val = cm3623->ps_threshold; // THD_PS
		rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->ps_thd_wr, val);
		if(rc < 0)
			pr_info("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);

		val = (1 << 0); // SD_PS
		val |= (0 << 2); // INT_PS
		val |= (0 << 3); // INT_ALS
		val |= (3 << 4); // IT_PS
		val |= (0 << 6); // DR_PS
		rc = cm3623_i2c_write_byte(cm3623->cm3623_client,p->ps_wr, val);
		if(rc < 0)
		{
			pr_err("%s cm3623_i2c_write_byte rc=%d\n", __func__, rc);
			continue;
		}

		if (rc == 0)
		{
			if (i==0)
			{
				pr_info("CM3623, chip AD\n");
			}
			else
			{
				pr_info("CM3623, only\n");
			}
			break;
		}
	}
	return rc; 
}


static long cm3623_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *pa = (void __user *)arg;
    int data[2] = {0};
    int vec[2] = {0};

    switch (cmd)
    {
    case CM3623_IOC_PROXIMITY_STDBY:
        cm3623_set_proximity_stdby();
        break;
    case CM3623_IOC_PROXIMITY_ACT:
        cm3623_set_proximity_act();
        break;
    case CM3623_IOC_RESET:
        break;
    case CM3623_IOC_READXYZ:
		data[0] = cm3623_get_als(cm3623_client);
		data[1] = cm3623_get_ps(cm3623_client);

		if (data[0] < 0)
			data[0] = 0;

		if (data[1] < 0)
			data[1] = 0;

		vec[0] = data[0];
        vec[1] = data[1];

        CMDBG(("cm3623_read_data, lux %d, ps data %d\n", data[0], data[1]));
        if (copy_to_user(pa, vec, sizeof(vec)))
        {
            return -EFAULT;
        }
        break;
    default:
        break;
    }

    return 0;
}

static ssize_t cm3623_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    sprintf(buf, "cm3623");
    ret = strlen(buf) + 1;

    return ret;
}


static DEVICE_ATTR(cm3623, S_IRUGO, cm3623_show, NULL);

static struct file_operations cm3623_fops =
{
    .owner		= THIS_MODULE,
    .open		= cm3623_open,
    .release	= cm3623_release,
	.unlocked_ioctl 	= cm3623_ioctl,
};

static struct miscdevice cm3623_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = CM3623_DEV_NAME,
    .fops = &cm3623_fops,
};

static int cm3623_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int res = 0;
    struct cm3623_data *pdata = NULL;
	int err = 0;

    printk("cm3623_probe\n");
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
    {
        pr_err("%s: functionality check failed\n", __FUNCTION__);
        res = -ENODEV;
        goto out;
    }
    CMDBG(("%s: functionality check OK\n", __FUNCTION__));

    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (pdata == NULL)
    {
        res = -ENOMEM;
        goto out;
    }

    cm3623_client = client;
    i2c_set_clientdata(client, pdata);

    res = misc_register(&cm3623_device);
    if (res)
    {
        pr_err("%s: cm3623_device register failed\n", __FUNCTION__);
        goto out;
    }
    CMDBG(("%s: cm3623_device register OK\n", __FUNCTION__));

    res = device_create_file(&client->dev, &dev_attr_cm3623);
    if (res)
    {
        pr_err("%s: device_create_file failed\n", __FUNCTION__);
        goto out_deregister;
    }
    CMDBG(("%s: device_create_file OK\n", __FUNCTION__));
	pdata->ps_enable = 0;
	pdata->als_enable = 0;
	pdata->ps_threshold = 0;
	pdata->als_threshold = 0;
	err = cm3623_config(pdata);
	if(err < 0)
	{
		pr_err("cm3623_config error err=%d\n", err);
		goto out_deregister;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
    pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    pdata->early_suspend.suspend = cm3623_early_suspend;
    pdata->early_suspend.resume = cm3623_late_resume;
    register_early_suspend(&pdata->early_suspend);
#endif

	p_cm3623_data = pdata;
	cm3623_set_light_act();
    printk("cm3623_probe, OK\n");
    return 0;

out_deregister:
    CMDBG(("cm3623_probe, failed\n"));
    misc_deregister(&cm3623_device);
out:
	if (pdata)
		kfree(pdata);
    return res;
}


static int cm3623_remove(struct i2c_client *client)
{
    device_remove_file(&client->dev, &dev_attr_cm3623);
    misc_deregister(&cm3623_device);

    return 0;
}

static int cm3623_suspend(struct i2c_client *client, pm_message_t state)
{
    cm3623_set_light_stdby();
    return 0;
}

static int cm3623_resume(struct i2c_client *client)
{
    cm3623_set_light_act();
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cm3623_early_suspend(struct early_suspend *h)
{
	CMDBG(("cm3623_early_suspend\n"));
    cm3623_suspend(cm3623_client, PMSG_SUSPEND);
}

static void cm3623_late_resume(struct early_suspend *h)
{
	CMDBG(("cm3623_late_resumed\n"));
    cm3623_resume(cm3623_client);
}
#endif

static int __init cm3623_init(void)
{
    int res;

    res = i2c_add_driver(&cm3623_driver);
    if (res < 0)
    {
        pr_err("add cm3623 i2c driver failed\n");
        return -ENODEV;
    }

	wake_lock_init(&cm3623_wake_lock, WAKE_LOCK_SUSPEND, "CM3623");
    return (res);
}

static void __exit cm3623_exit(void)
{
    i2c_del_driver(&cm3623_driver);
	wake_lock_destroy(&cm3623_wake_lock);
}

MODULE_AUTHOR("Thundersoft");
MODULE_DESCRIPTION("cm3623 driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

late_initcall(cm3623_init);
module_exit(cm3623_exit);

