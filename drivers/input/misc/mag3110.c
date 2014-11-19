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

#include <linux/ioctl.h>

#define MAG3110_I2C_NAME	"mag3110"
#define MAG3110_DEV_NAME	"mag3110"

#define MAG3110_I2C_ADDR		0x0E

/* MAG3110 register address */
#define MAG3110_REG_CTRL		0x07
#define MAG3110_REG_DATA		0x00
#define MAG3110_REG_DS			0x06

/* MAG3110 control bit */
#define MAG3110_CTRL_TM			0x01
#define MAG3110_CTRL_RM			0x20

/* Use 'm' as magic number */
#define MAG3110_IOM			'm'

/* IOCTLs for MAG3110 device */
#define MAG3110_IOC_TM			_IO (MAG3110_IOM, 0x00)
#define MAG3110_IOC_RM			_IO (MAG3110_IOM, 0x01)
#define MAG3110_IOC_READ		_IOR(MAG3110_IOM, 0x02, int[3])
#define MAG3110_IOC_READXYZ		_IOR(MAG3110_IOM, 0x03, int[3])

#define DEBUG			0
#define MAX_FAILURE_COUNT	3

#define MAG3110_DELAY_TM	10	/* ms */
#define MAG3110_DELAY_RM	10	/* ms */
#define MAG3110_DELAY_STDN	1	/* ms */

#define MAG3110_RETRY_COUNT	3
#define MAG3110_RESET_INTV	10


#define MAG3110_ID		0xC4
#define MAG3110_XYZ_DATA_LEN	6
#define MAG3110_STATUS_ZYXDR	0x08

#define MAG3110_AC_MASK         (0x01)
#define MAG3110_AC_OFFSET       0
#define MAG3110_DR_MODE_MASK    (0x7 << 5)
#define MAG3110_DR_MODE_OFFSET  5
#define MAG3110_IRQ_USED   0

#define POLL_INTERVAL_MAX	500
#define POLL_INTERVAL		100
#define INT_TIMEOUT   1000
/* register enum for mag3110 registers */
enum {
	MAG3110_DR_STATUS = 0x00,
	MAG3110_OUT_X_MSB,
	MAG3110_OUT_X_LSB,
	MAG3110_OUT_Y_MSB,
	MAG3110_OUT_Y_LSB,
	MAG3110_OUT_Z_MSB,
	MAG3110_OUT_Z_LSB,
	MAG3110_WHO_AM_I,

	MAG3110_OFF_X_MSB,
	MAG3110_OFF_X_LSB,
	MAG3110_OFF_Y_MSB,
	MAG3110_OFF_Y_LSB,
	MAG3110_OFF_Z_MSB,
	MAG3110_OFF_Z_LSB,

	MAG3110_DIE_TEMP,

	MAG3110_CTRL_REG1 = 0x10,
	MAG3110_CTRL_REG2,
};
enum {
	MAG_STANDBY,
	MAG_ACTIVED
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mag3110_early_suspend(struct early_suspend *h);
static void mag3110_late_resume(struct early_suspend *h);
#endif

struct mag3110_data
{
	u8 ctl_reg1;
	int active;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif    
};

static struct i2c_client *mag3110_client;

static int mag3110_read_reg(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

/*!
 * This function do one mag3110 register write.
 */
static int mag3110_write_reg(struct i2c_client *client, u8 reg, char value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "i2c write failed\n");
	return ret;
}

/*!
 * This function do multiple mag3110 registers read.
 */
static int mag3110_read_block_data(struct i2c_client *client, u8 reg,
				   int count, u8 *addr)
{
	if (i2c_smbus_read_i2c_block_data
	     (client, reg, count, addr) < count) {
		dev_err(&client->dev, "i2c block read failed\n");
		return -1;
	}

	return count;
}

/*
 * Initialization function
 */
static int mag3110_init_client(struct i2c_client *client)
{
	int val, ret;

	/* enable automatic resets */
	val = 0x80;
	ret = mag3110_write_reg(client, MAG3110_CTRL_REG2, val);

	/* set default data rate to 10HZ */
	val = mag3110_read_reg(client, MAG3110_CTRL_REG1);
	val |= (0x0 << MAG3110_DR_MODE_OFFSET);
	val |= MAG3110_AC_MASK;
	ret = mag3110_write_reg(client, MAG3110_CTRL_REG1, val);

	return ret;
}

/***************************************************************
*
* read sensor data from mag3110
*
***************************************************************/
static int mag3110_read_data(short *x, short *y, short *z)
{
	int retry = 3;
	u8 tmp_data[MAG3110_XYZ_DATA_LEN];
    int result;

    do {
		result = i2c_smbus_read_byte_data(mag3110_client,
					     MAG3110_DR_STATUS);
		retry --; 

		if (!(result & MAG3110_STATUS_ZYXDR))
		{
			msleep(1);
		}
	} while (!(result & MAG3110_STATUS_ZYXDR) && retry >0);

	if(retry == 0){
#if DEBUG		
		printk("magd wait data ready timeout....\n");
#endif
		return -EINVAL;
	}


	if (mag3110_read_block_data(mag3110_client,
			MAG3110_OUT_X_MSB, MAG3110_XYZ_DATA_LEN, tmp_data) < 0)
		return -1;

	*x = ((tmp_data[0] << 8) & 0xff00) | tmp_data[1];
	*y = ((tmp_data[2] << 8) & 0xff00) | tmp_data[3];
	*z = ((tmp_data[4] << 8) & 0xff00) | tmp_data[5];

	return 0;
}


static int mag3110_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int mag3110_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long mag3110_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	short data[3] = {0};
	int vec[3] = {0};

	switch (cmd) {
	case MAG3110_IOC_TM:
		break;
	case MAG3110_IOC_RM:
		break;
	case MAG3110_IOC_READ:
		break;
	case MAG3110_IOC_READXYZ:
		if (mag3110_read_data(&data[0], &data[1], &data[2]) < 0)
		{
			return -EFAULT;
		}
		vec[0] = data[0];
		vec[1] = data[1];
		vec[2] = data[2];

#if DEBUG
        printk("MAG3110: [X - %04x] [Y - %04x] [Z - %04x]\n",
               data[0], data[1], data[2]);
#endif

		if (copy_to_user(pa, vec, sizeof(vec))) {
			return -EFAULT;
		}
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t mag3110_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "MAG3110");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(mag3110, S_IRUGO, mag3110_show, NULL);

static struct file_operations mag3110_fops = {
	.owner		= THIS_MODULE,
	.open		= mag3110_open,
	.release	= mag3110_release,
	.unlocked_ioctl		= mag3110_ioctl,
};

static struct miscdevice mag3110_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MAG3110_DEV_NAME,
	.fops = &mag3110_fops,
};

static int mag3110_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
    struct mag3110_data *pdata = NULL;

	printk("mag3110 probe\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		ret = -ENODEV;
		goto out;
	}
	ret = mag3110_read_reg(client, MAG3110_WHO_AM_I);

	if (MAG3110_ID != ret) {
		dev_err(&client->dev,
			"read chip ID 0x%x is not equal to 0x%x!\n", ret,
			MAG3110_ID);
		return -EINVAL;
	}

    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (pdata == NULL)
    {
        ret = -ENOMEM;
        goto out;
    }

    i2c_set_clientdata(client, pdata);
	mag3110_client = client;

	ret = misc_register(&mag3110_device);
	if (ret) {
		pr_err("%s: mag3110_device register failed\n", __FUNCTION__);
		goto out;
	}

	ret = device_create_file(&client->dev, &dev_attr_mag3110);
	if (ret) {
		pr_err("%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
    pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    pdata->early_suspend.suspend = mag3110_early_suspend;
    pdata->early_suspend.resume = mag3110_late_resume;
    register_early_suspend(&pdata->early_suspend);
#endif

	/* Initialize mag3110 chip */
	mag3110_init_client(client);
	pdata->active = MAG_ACTIVED;

	printk("mag3110 probe OK\n");
	return 0;

out_deregister:
	misc_deregister(&mag3110_device);
out:
	return ret;
}

static int mag3110_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_mag3110);
	misc_deregister(&mag3110_device);

	return 0;
}

static int mag3110_suspend(struct i2c_client *client, pm_message_t state)
{
	int ret = 0;
	struct mag3110_data *data = i2c_get_clientdata(client);
    if(data->active == MAG_ACTIVED){
		data->ctl_reg1 = mag3110_read_reg(client, MAG3110_CTRL_REG1);
		ret = mag3110_write_reg(client, MAG3110_CTRL_REG1,
					   data->ctl_reg1 & ~MAG3110_AC_MASK);
    }
	return ret;
}

static int mag3110_resume(struct i2c_client *client)
{
	int ret = 0;
	struct mag3110_data *data = i2c_get_clientdata(client);
    if(data->active == MAG_ACTIVED){
		ret = mag3110_write_reg(client, MAG3110_CTRL_REG1,
					   data->ctl_reg1);
    }
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mag3110_early_suspend(struct early_suspend *h)
{
    mag3110_suspend(mag3110_client, PMSG_SUSPEND);
}

static void mag3110_late_resume(struct early_suspend *h)
{
    mag3110_resume(mag3110_client);
}
#endif


static const struct i2c_device_id mag3110_id[] = {
	{ MAG3110_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver mag3110_driver = {
	.probe 		= mag3110_probe,
	.remove 	= mag3110_remove,
	.id_table	= mag3110_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend 	= mag3110_suspend,
    .resume 	= mag3110_resume,
#endif
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= MAG3110_I2C_NAME,
	},
};


static int __init mag3110_init(void)
{
	pr_info("mag3110 driver: init\n");
	return i2c_add_driver(&mag3110_driver);
}

static void __exit mag3110_exit(void)
{
	pr_info("mag3110 driver: exit\n");
	i2c_del_driver(&mag3110_driver);
}

late_initcall(mag3110_init);
module_exit(mag3110_exit);

MODULE_AUTHOR("mag3110");
MODULE_DESCRIPTION("mag3110 Magnetic Sensor Driver");
MODULE_LICENSE("GPL");
