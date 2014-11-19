/* 
 * drivers/input/touchscreen/ft5x06_ts.c
 *
 * FocalTech ft5x06 TouchScreen driver. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
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
 * VERSION      	DATE			AUTHOR
 *    1.0		  2010-01-05			WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */


/**
   This driver is for touchscreen provided by WW Fortune Ship 
   Chip, FocalTech
   I2C addr, 0x3D
   Multitouch, 5 finger
   Touch Key, Menu, Home, Back Search
   Resolution, 480 * 800
*/

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
//#define TS_DEBUG

#ifdef TS_DEBUG
#define TSDBG(x) printk x
#else
#define TSDBG(x)
#endif
#define SCREEN_MAX_X    480
#define SCREEN_MAX_Y    800
#define PRESS_MAX        255

#define FT5X06_NAME	"ft5x06_ts"

struct ft5x06_ts_platform_data{
	u16	intr;		/* irq number	*/
};

enum ft5x06_ts_regs {
	FT5X06_REG_THGROUP					= 0x80,
	FT5X06_REG_THPEAK						= 0x81,
	FT5X06_REG_TIMEENTERMONITOR			= 0x87,
	FT5X06_REG_PERIODACTIVE				= 0x88,
	FT5X06_REG_PERIODMONITOR			= 0x89,
	FT5X06_REG_AUTO_CLB_MODE			= 0xa0,
	FT5X06_REG_PMODE						= 0xa5,	/* Power Consume Mode		*/	
	FT5X06_REG_FIRMID						= 0xa6,
	FT5X06_REG_ERR						= 0xa9,
	FT5X06_REG_CLB						= 0xaa,
};


//FT5X06_REG_PMODE
#define PMODE_ACTIVE        0x00
#define PMODE_MONITOR       0x01
#define PMODE_STANDBY       0x02
#define PMODE_HIBERNATE     0x03

static struct i2c_client * ft5x06_client;

#define FT5X06_FINGER_MAX_NUM      5
#define FT5X06_KEY_MAX_NUM         4
#define FT5X06_KEY_IDLE            0
#define FT5X06_KEY_PRESSED         1

#define FT5X06_TS_SWAP(a, b) do {unsigned int temp; temp = a; a = b; b = temp;} while (0)

struct ts_event {
	u16 x[FT5X06_FINGER_MAX_NUM];
	u16 y[FT5X06_FINGER_MAX_NUM];
	u16	pressure;
    u8  touch_point;
};

struct ft5x06_ts_data {
	struct input_dev	*input_dev;
	struct ts_event		event;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;
	struct early_suspend	early_suspend;
};

struct ft5x06_ts_key_def{
	int key_value;
	char * key_name;
	u16 x0_min;
	u16 x0_max;
	u16 y0_min;
	u16 y0_max;
	
	u16 x1_min;
	u16 x1_max;
	u16 y1_min;
	u16 y1_max;
	
	int status;
};

/*
K1 (Xmin375,Xmax381) (Y845)
K2 (Xmin405,Xmax405) (Y845)
K3 (Xmin435,Xmax435) (Y845)
K4 (Xmin479,Xmax479) (Y845)
*/

static struct ft5x06_ts_key_def key_data[FT5X06_KEY_MAX_NUM] = {
	{KEY_MENU, "Menu",     371, 381, 833, 853, 102,   121,   840,   859,  FT5X06_KEY_IDLE},
	{KEY_HOME, "Home",     401, 411, 833, 853, 230,   249,   840,   859,  FT5X06_KEY_IDLE},
	{KEY_BACK, "Back",     431, 441, 833, 853, 300,   319,   840,   859,  FT5X06_KEY_IDLE},
	{KEY_SEARCH, "Search", 470, 480, 833, 853, 30000, 30000, 30000, 30000,  FT5X06_KEY_IDLE},
};


/***********************************************************************************************
Name	:	ft5x06_i2c_rxdata 

Input	:	*rxdata
                     *length

Output	:	ret

function	:	

***********************************************************************************************/
static int ft5x06_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= ft5x06_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= ft5x06_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(ft5x06_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	
	return ret;
}


/***********************************************************************************************
Name	:	ft5x06_read_reg 

Input	:	addr
                     pdata

Output	:	

function	:	read register of ft5x06

***********************************************************************************************/
static int ft5x06_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= ft5x06_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= ft5x06_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf,
		},
	};

	buf[0] = addr;
	ret = i2c_transfer(ft5x06_client->adapter, msgs, 2);
	if (ret < 0)
	{
		pr_err("%s i2c read error: %d\n", __func__, ret);
	}
	else
	{
		*pdata = buf[0];
	}
	return ret;
  
}


/***********************************************************************************************
Name	:	 ft5x06_read_fw_ver

Input	:	 void
                     

Output	:	 firmware version 	

function	:	 read TP firmware version

***********************************************************************************************/
static unsigned char ft5x06_read_fw_ver(void)
{
	unsigned char ver;
	int ret;
	ret = ft5x06_read_reg(FT5X06_REG_FIRMID, &ver);
	if (ret < 0)
		ver = 0xAB;
	return(ver);
}


//#define CONFIG_SUPPORT_FTS_CTP_UPG


#ifdef CONFIG_SUPPORT_FTS_CTP_UPG

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

#define FTS_NULL                0x0
#define FTS_TRUE                0x01
#define FTS_FALSE              0x0

#define I2C_CTPM_ADDRESS       0xFF //no reference!
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int ft5x06_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= ft5x06_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	ret = i2c_transfer(ft5x06_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}

/***********************************************************************************************
Name	:	 ft5x06_write_reg

Input	:	addr -- address
                     para -- parameter

Output	:	

function	:	write register of ft5x06

***********************************************************************************************/
static int ft5x06_write_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x06_i2c_txdata(buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }
    
    return 0;
}

void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}


/*
[function]: 
    callback: read data from ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[out]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    
    ret=i2c_master_recv(ft5x06_client, pbt_buf, dw_lenth);

    if(ret<=0)
    {
        TSDBG(("[TSP]i2c_read_interface error\n"));
        return FTS_FALSE;
    }
  
    return FTS_TRUE;
}

/*
[function]: 
    callback: write data to ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[in]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    ret=i2c_master_send(ft5x06_client, pbt_buf, dw_lenth);
    if(ret<=0)
    {
        TSDBG(("[TSP]i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret));
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

/*
[function]: 
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;    
    btPara2[in]    :parameter 2;    
    btPara3[in]    :parameter 3;    
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL cmd_write(FTS_BYTE btcmd,FTS_BYTE btPara1,FTS_BYTE btPara2,FTS_BYTE btPara3,FTS_BYTE num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
}

/*
[function]: 
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_write(FTS_BYTE* pbt_buf, FTS_DWRD dw_len)
{
    
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
[function]: 
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_read(FTS_BYTE* pbt_buf, FTS_BYTE bt_len)
{
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


/*
[function]: 
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);    
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/


#define    FTS_PACKET_LENGTH        128

static unsigned char CTPM_FW[]=
{
	#include "ft_app.i"
};

E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    FTS_BYTE reg_val[2] = {0};
    FTS_DWRD i = 0;

    FTS_DWRD  packet_number;
    FTS_DWRD  j;
    FTS_DWRD  temp;
    FTS_DWRD  lenght;
    FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
    FTS_BYTE  auc_i2c_write_buf[10];
    FTS_BYTE bt_ecc;
    int      i_ret;

    /*********Step 1:Reset  CTPM *****/
    /*write 0xaa to register 0xfc*/
    ft5x06_write_reg(0xfc,0xaa);
    delay_qt_ms(50);
     /*write 0x55 to register 0xfc*/
    ft5x06_write_reg(0xfc,0x55);
    TSDBG(("[TSP] Step 1: Reset CTPM test\n"));
   
    delay_qt_ms(30);   


    /*********Step 2:Enter upgrade mode *****/
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do
    {
        i ++;
        i_ret = ft5x06_i2c_txdata(auc_i2c_write_buf, 2);
        delay_qt_ms(5);
    }while(i_ret <= 0 && i < 5 );

    /*********Step 3:check READ-ID***********************/        
    cmd_write(0x90,0x00,0x00,0x00,4);
    byte_read(reg_val,2);
    if (reg_val[0] == 0x79 && reg_val[1] == 0x3)
    {
        TSDBG(("[TSP] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]));
    }
    else
    {
        return ERR_READID;
        //i_is_new_protocol = 1;
    }

     /*********Step 4:erase app*******************************/
    cmd_write(0x61,0x00,0x00,0x00,1);
   
    delay_qt_ms(1500);
    TSDBG(("[TSP] Step 4: erase. \n"));

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    TSDBG(("[TSP] Step 5: start upgrade. \n"));
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0;j<packet_number;j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        delay_qt_ms(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              TSDBG(("[TSP] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH));
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],temp+6);    
        delay_qt_ms(20);
    }

    //send the last six byte
    for (i = 0; i<6; i++)
    {
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
        bt_ecc ^= packet_buf[6];

        byte_write(&packet_buf[0],7);  
        delay_qt_ms(20);
    }

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    cmd_write(0xcc,0x00,0x00,0x00,1);
    byte_read(reg_val,1);
    TSDBG(("[TSP] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc));
    if(reg_val[0] != bt_ecc)
    {
        return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
    cmd_write(0x07,0x00,0x00,0x00,1);

    return ERR_OK;
}


int fts_ctpm_fw_upgrade_with_i_file(void)
{
   FTS_BYTE*     pbt_buf = FTS_NULL;
   int i_ret;
    
    //=========FW upgrade========================*/
   pbt_buf = CTPM_FW;
   /*call the upgrade function*/
   i_ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));
   if (i_ret != 0)
   {
       //error handling ...
       //TBD
   }

   return i_ret;
}

unsigned char fts_ctpm_get_upg_ver(void)
{
    unsigned int ui_sz;
    ui_sz = sizeof(CTPM_FW);
    if (ui_sz > 2)
    {
        return CTPM_FW[ui_sz - 2];
    }
    else
    {
        //TBD, error handling?
        return 0xff; //default value
    }
}

#endif

static void ft5x06_release_key(void)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(ft5x06_client);
	int i;
	for (i = 0; i<FT5X06_KEY_MAX_NUM; i++)
	{
		if (key_data[i].status == FT5X06_KEY_PRESSED)
		{
			TSDBG(("Key %s Released\n", key_data[i].key_name));
			key_data[i].status = FT5X06_KEY_IDLE;
			input_report_key(data->input_dev, key_data[i].key_value, 0);
		}
	}
	
}

/***********************************************************************************************
Name:	  ft5x06_ts_release
Input:    void
Output:	  void
function: no touch handle function
***********************************************************************************************/
static void ft5x06_ts_release(void)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(ft5x06_client);
	ft5x06_release_key();
	input_report_key(data->input_dev, BTN_TOUCH, 0);
	input_sync(data->input_dev);
}

static int ft5x06_read_data(void)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(ft5x06_client);
	struct ts_event *event = &data->event;
	u8 buf[32] = {0};
	int ret = -1;
//	int i;
	if (data->input_dev == NULL)
		return -1;

	ret = ft5x06_i2c_rxdata(buf, 31);

    if (ret < 0) {
		TSDBG(("%s read_data i2c_rxdata failed: %d\n", __func__, ret));
		return ret;
	}

	memset(event, 0, sizeof(struct ts_event));
	event->touch_point = buf[2] & 0x07;// 000 0111

    if (event->touch_point == 0) {
		TSDBG(("ft5x06_read_data, touch_point = 0, ft5x06_ts_release\n"));
        ft5x06_ts_release();
        return 1; 
    }
	else
	{
		TSDBG(("ft5x06_read_data, touch_point = %d\n", event->touch_point));
	}

    switch (event->touch_point) {
		case 5:
			event->x[4] = (s16)(buf[0x1b] & 0x0F)<<8 | (s16)buf[0x1c];
			event->y[4] = (s16)(buf[0x1d] & 0x0F)<<8 | (s16)buf[0x1e];
		case 4:
			event->x[3] = (s16)(buf[0x15] & 0x0F)<<8 | (s16)buf[0x16];
			event->y[3] = (s16)(buf[0x17] & 0x0F)<<8 | (s16)buf[0x18];
		case 3:
			event->x[2] = (s16)(buf[0x0f] & 0x0F)<<8 | (s16)buf[0x10];
			event->y[2] = (s16)(buf[0x11] & 0x0F)<<8 | (s16)buf[0x12];
		case 2:
			event->x[1] = (s16)(buf[9] & 0x0F)<<8 | (s16)buf[10];
			event->y[1] = (s16)(buf[11] & 0x0F)<<8 | (s16)buf[12];
		case 1:
			event->x[0] = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
			event->y[0] = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
            break;
		default:
		    return -1;
	}

#if 0
	for (i=0; i<event->touch_point; i++)
	{
		event->y[i] = event->y[i] * 4 / 5;

		FT5X06_TS_SWAP(event->y[i], event->x[i]);
		event->x[i] = SCREEN_MAX_X - event->x[i];
		
		TSDBG(("Point %d, x = %d, y = %d\n", i + 1, event->x[i], event->y[i]));
	}
#endif

    event->pressure = 200;

    return 0;
}

static struct ft5x06_ts_key_def * ft5x06_get_key_p(int x, int y)
{
	int i;
	for (i = 0; i<FT5X06_KEY_MAX_NUM; i++)
	{
		if (   (x >= key_data[i].x0_min) 
			&& (x <= key_data[i].x0_max) 
			&& (y >= key_data[i].y0_min)
			&& (y <= key_data[i].y0_max))
			return key_data + i;

		if (   (x >= key_data[i].x1_min) 
			&& (x <= key_data[i].x1_max) 
			&& (y >= key_data[i].y1_min)
			&& (y <= key_data[i].y1_max))
			return key_data + i;
			
	}
	
	return NULL;
}

/***********************************************************************************************
Name: ft5x06_report_value
Input: void	
Output:	void
function: report the point information to upper layer
***********************************************************************************************/
static void ft5x06_report_value(void)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(ft5x06_client);
	struct ts_event *event = &data->event;
	struct ft5x06_ts_key_def * p = NULL;
	int i;
	int tracking_id = 0;
	int key_pressed = 0;

	if (data->input_dev == NULL)
		return;
	
	for (i=0; i<event->touch_point; i++)
	{
		if ((event->x[i] < SCREEN_MAX_X) && (event->y[i] < SCREEN_MAX_Y))
		{
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, tracking_id++);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y[i]);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_mt_sync(data->input_dev);
		}
		else
		{
			p = ft5x06_get_key_p(event->x[i], event->y[i]);
			
			if (p)
			{
				if (p->status == FT5X06_KEY_IDLE)
				{
					p->status = FT5X06_KEY_PRESSED;
					TSDBG(("key %s pressed\n", p->key_name));
					input_report_key(data->input_dev, p->key_value, 1);
				}
				key_pressed = 1;
			}
		}
	}

	if (!key_pressed)
	{
		ft5x06_release_key();
	}

	input_report_key(data->input_dev, BTN_TOUCH, !!event->touch_point);
	input_sync(data->input_dev);
}	/*end ft5x06_report_value*/
/***********************************************************************************************
Name: ft5x06_ts_pen_irq_work
Input: irq source
Output:	void
function: handle later half irq
***********************************************************************************************/
static void ft5x06_ts_pen_irq_work(struct work_struct *work)
{
	int ret = -1;
	ret = ft5x06_read_data();	
	if (ret == 0) {	
		TSDBG(("call ft5x06_report_value\n"));
		ft5x06_report_value();
	}
	if (ft5x06_client)
		enable_irq(ft5x06_client->irq);
}
/***********************************************************************************************
Name: ft5x06_ts_interrupt
Input: irq, dev_id
Output: IRQ_HANDLED
function: 
***********************************************************************************************/
static irqreturn_t ft5x06_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x06_ts_data *ft5x06_ts = dev_id;
	disable_irq_nosync(ft5x06_client->irq);
//	TSDBG("==int=\n");
	if (!work_pending(&ft5x06_ts->pen_event_work)) {
		queue_work(ft5x06_ts->ts_workqueue, &ft5x06_ts->pen_event_work);
	}

	return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x06_ts_suspend
Input: handler
Output: void
function: suspend function for power management
***********************************************************************************************/
static void ft5x06_ts_suspend(struct early_suspend *handler)
{
//	struct ft5x06_ts_data *ts;
//	ts =  container_of(handler, struct ft5x06_ts_data, early_suspend);

	TSDBG(("ft5x06_ts_suspend\n"));
	disable_irq(ft5x06_client->irq);

//	disable_irq(ft5x06_client->irq);
//	disable_irq(ft5x06_client->irq);
//	cancel_work_sync(&ts->pen_event_work);
//	flush_workqueue(ts->ts_workqueue);
	// ==set mode ==, 
//    	ft5x06_set_reg(FT5X06_REG_PMODE, PMODE_HIBERNATE);
}
/***********************************************************************************************
Name: ft5x06_ts_resume
Input:	handler
Output:	void
function: resume function for powermanagement
***********************************************************************************************/
static void ft5x06_ts_resume(struct early_suspend *handler)
{
	TSDBG(("ft5x06_ts_resume\n"));
	enable_irq(ft5x06_client->irq);
}
#endif  //CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x06_ts_probe
Input: client, id
Output: 0 if OK, other value indicate error
function: probe
***********************************************************************************************/
static int 
ft5x06_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x06_ts_data *ft5x06_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value; 
	int i;
	
	printk("ft5x06_ts_probe\n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x06_ts = kzalloc(sizeof(*ft5x06_ts), GFP_KERNEL);
	if (!ft5x06_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	ft5x06_ts->input_dev = NULL;

	ft5x06_client = client;
	
	client->irq = TS_INT;
	i2c_set_clientdata(client, ft5x06_ts);

    uc_reg_value = ft5x06_read_fw_ver();
	if (0xAB == uc_reg_value)
	{
		printk("ft5x06_ts_probe get fw version error.\n");
		goto exit_get_fwver_failed;
	}
	else
	{
		printk("ft5x06_ts_probe Firmware version = 0x%x\n", uc_reg_value);
	}

	INIT_WORK(&ft5x06_ts->pen_event_work, ft5x06_ts_pen_irq_work);

	ft5x06_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x06_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	err = request_irq(client->irq, ft5x06_ts_interrupt, IRQF_TRIGGER_FALLING, "ft5x06_ts", ft5x06_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x06_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}

	disable_irq(client->irq);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	ft5x06_ts->input_dev = input_dev;

	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X,  input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y,  input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
	set_bit(EV_KEY,             input_dev->evbit);
	set_bit(EV_ABS,             input_dev->evbit);
	set_bit(BTN_TOUCH,          input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT,  input_dev->propbit);

	/* Enable all supported keys */
	for (i=0; i<FT5X06_KEY_MAX_NUM; i++)
		__set_bit(key_data[i].key_value, input_dev->keybit);

	__clear_bit(KEY_RESERVED, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,  0, FT5X06_FINGER_MAX_NUM, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,   0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,   0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,  0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,  0, 200, 0, 0);

	input_dev->name		= FT5X06_NAME;		//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"ft5x06_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	TSDBG(("ft5x06_ts_probe register_early_suspend\n"));
	ft5x06_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	ft5x06_ts->early_suspend.suspend = ft5x06_ts_suspend;
	ft5x06_ts->early_suspend.resume	= ft5x06_ts_resume;
	register_early_suspend(&ft5x06_ts->early_suspend);
#endif

    msleep(50);

    enable_irq(ft5x06_client->irq);

    msm_tp_set_found_flag(1);
	printk("ft5x06_ts_probe probe OK\n");
    return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
//	free_irq(client->irq, ft5x06_ts);
	free_irq(ft5x06_client->irq, ft5x06_ts);
exit_irq_request_failed:
//exit_platform_data_null:
	cancel_work_sync(&ft5x06_ts->pen_event_work);
	destroy_workqueue(ft5x06_ts->ts_workqueue);
exit_create_singlethread:
exit_get_fwver_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ft5x06_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}
/***********************************************************************************************
Name: ft5x06_ts_remove
Input: client
Output: always 0
function: remove the driver
***********************************************************************************************/
static int __devexit ft5x06_ts_remove(struct i2c_client *client)
{
	struct ft5x06_ts_data *ft5x06_ts = i2c_get_clientdata(client);
	TSDBG(("ft5x06_ts_remove\n"));
	unregister_early_suspend(&ft5x06_ts->early_suspend);
	free_irq(ft5x06_client->irq, ft5x06_ts);
	input_unregister_device(ft5x06_ts->input_dev);
	kfree(ft5x06_ts);
	cancel_work_sync(&ft5x06_ts->pen_event_work);
	destroy_workqueue(ft5x06_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id ft5x06_ts_id[] = {
	{ FT5X06_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x06_ts_id);

static struct i2c_driver ft5x06_ts_driver = {
	.probe		= ft5x06_ts_probe,
	.remove		= __devexit_p(ft5x06_ts_remove),
	.id_table	= ft5x06_ts_id,
	.driver	= {
		.name	= FT5X06_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init ft5x06_ts_init(void)
{
	int ret;
	TSDBG(("ft5x06_ts_init\n"));
    if (msm_tp_get_found_flag())
    {
        return -1;
    }
	ret = i2c_add_driver(&ft5x06_ts_driver);
	TSDBG(("ft5x06_ts_init ret=%d\n", ret));
	return ret;
}

static void __exit ft5x06_ts_exit(void)
{
	TSDBG(("ft5x06_ts_exit\n"));
	i2c_del_driver(&ft5x06_ts_driver);
}

late_initcall(ft5x06_ts_init);
module_exit(ft5x06_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x06 TouchScreen driver");
MODULE_LICENSE("GPL");

