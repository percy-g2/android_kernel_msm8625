/*
 * mtv_i2c.c
 *
 * RAONTECH MTV I2C driver.
 *
 * Copyright (C) (2011, RAONTECH)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "raontv.h"
#include "raontv_internal.h"

#include "mtv.h"


#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)

extern void dump_stack(void);
#define I2C_DEV_NAME	"mtvi2c"


//struct i2c_client *dmb_i2c_client_ptr;
//struct i2c_adapter *dmb_i2c_adapter_ptr;


static const struct i2c_device_id mtv_i2c_device_id[] = {
	{I2C_DEV_NAME, 0},
	{},
};


MODULE_DEVICE_TABLE(i2c, mtv_i2c_device_id);


static int mtv_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
//	int i;
	DMBMSG("mtv_i2c_probe\n");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        DMBMSG("mtv_i2c_probe: need I2C_FUNC_I2C\n");
        return -ENODEV;
    }
    else
    {
        DMBMSG("mtv_i2c_probe: i2c_client name: %s\n", client->name);
    }

	mtv_cb_ptr->i2c_client_ptr = client;
	mtv_cb_ptr->i2c_adapter_ptr = to_i2c_adapter(client->dev.parent); 
#if 0
    for (i=0; i<5; i++)
    {
	    unsigned char test_data = 0;
        mtv_i2c_write(test_data, 0);
    }
#endif	
	return 0;
}


static int mtv_i2c_remove(struct i2c_client *client)
{
    int ret = 0;

    return ret;
}

void mtv_i2c_read_burst(unsigned char reg, unsigned char *buf, int size)
{
	int ret;	
	u8 out_buf[2];

	struct i2c_msg msg[] = {  
		 {.addr = mtv_cb_ptr->i2c_client_ptr->addr, .flags = 0, .buf = out_buf, .len = 1},	
		 {.addr = mtv_cb_ptr->i2c_client_ptr->addr, .flags = I2C_M_RD, .buf = buf, .len = size} 
	};	

	out_buf[0] = reg;
	out_buf[1] = 0;

//	printk("mtv_i2c_read_burst reg = %d, size %d\n", reg, size);
	ret = i2c_transfer(mtv_cb_ptr->i2c_client_ptr->adapter, msg, 2);
	if (ret != 2) {  
//		 dump_stack();
		 DMBMSG("[mtv_i2c_read_burst] error: %d\n", ret);	
	}  
}

unsigned char mtv_i2c_read(unsigned char reg)
{
	int ret;	
	u8 out_buf[2];
	u8 in_buf[2]; 

	struct i2c_msg msg[] = {  
	     {.addr = mtv_cb_ptr->i2c_client_ptr->addr, .flags = 0, .buf = out_buf, .len = 1},  
	     {.addr = mtv_cb_ptr->i2c_client_ptr->addr, .flags = I2C_M_RD, .buf = in_buf, .len = 1}  
	};  

	out_buf[0] = reg;
	out_buf[1] = 0;

//	ret = i2c_transfer(mtv_cb_ptr->i2c_adapter_ptr, msg, 2);
//	printk("mtv_i2c_read adapter %p\n", mtv_cb_ptr->i2c_client_ptr->adapter);
	ret = i2c_transfer(mtv_cb_ptr->i2c_client_ptr->adapter, msg, 2); 
	if (ret != 2) {  
//		 dump_stack();
	     DMBMSG("[mtv_i2c_read] error: %d\n", ret);  
	     return 0x00;
	}  

	return in_buf[0];
}



void mtv_i2c_write(unsigned char reg, unsigned char val)
{
	int ret;
	u8 out_buf[2];
	struct i2c_msg msg = {.addr = mtv_cb_ptr->i2c_client_ptr->addr, .flags = 0, .buf = out_buf, .len = 2}; 
	
	out_buf[0] = reg;
	out_buf[1] = val;

	ret = i2c_transfer(mtv_cb_ptr->i2c_client_ptr->adapter, &msg, 1);
//	printk("mtv_i2c_write adapter %p\n", mtv_cb_ptr->i2c_client_ptr->adapter);
	if (ret != 1) {  
//		 dump_stack();
	     DMBMSG("[mtv_i2c_write] error: %d\n", ret);  
	}
}


static int mtv_i2c_resume(struct i2c_client *client)
{	
	return 0;
}


static int mtv_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}


struct i2c_driver mtv_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name	= I2C_DEV_NAME,
	},
	.probe = mtv_i2c_probe,
	.remove = __devexit_p(mtv_i2c_remove),
	.suspend = mtv_i2c_suspend,
	.resume  = mtv_i2c_resume,
	.id_table  = mtv_i2c_device_id,
};
#endif /* #if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE) */

