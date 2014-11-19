/*
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
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

#define FT5X0X_NAME	"ft5x0x_ts"//"synaptics_i2c_rmi"//"synaptics-rmi-ts"// 

struct ft5x0x_ts_platform_data
{
    u16	intr;		/* irq number	*/
};

enum ft5x0x_ts_regs
{
    FT5X0X_REG_THGROUP					= 0x80,
    FT5X0X_REG_THPEAK						= 0x81,
//	FT5X0X_REG_THCAL						= 0x82,
//	FT5X0X_REG_THWATER					= 0x83,
//	FT5X0X_REG_THTEMP					= 0x84,
//	FT5X0X_REG_THDIFF						= 0x85,
//	FT5X0X_REG_CTRL						= 0x86,
    FT5X0X_REG_TIMEENTERMONITOR			= 0x87,
    FT5X0X_REG_PERIODACTIVE				= 0x88,
    FT5X0X_REG_PERIODMONITOR			= 0x89,
//	FT5X0X_REG_HEIGHT_B					= 0x8a,
//	FT5X0X_REG_MAX_FRAME					= 0x8b,
//	FT5X0X_REG_DIST_MOVE					= 0x8c,
//	FT5X0X_REG_DIST_POINT				= 0x8d,
//	FT5X0X_REG_FEG_FRAME					= 0x8e,
//	FT5X0X_REG_SINGLE_CLICK_OFFSET		= 0x8f,
//	FT5X0X_REG_DOUBLE_CLICK_TIME_MIN	= 0x90,
//	FT5X0X_REG_SINGLE_CLICK_TIME			= 0x91,
//	FT5X0X_REG_LEFT_RIGHT_OFFSET		= 0x92,
//	FT5X0X_REG_UP_DOWN_OFFSET			= 0x93,
//	FT5X0X_REG_DISTANCE_LEFT_RIGHT		= 0x94,
//	FT5X0X_REG_DISTANCE_UP_DOWN		= 0x95,
//	FT5X0X_REG_ZOOM_DIS_SQR				= 0x96,
//	FT5X0X_REG_RADIAN_VALUE				=0x97,
//	FT5X0X_REG_MAX_X_HIGH                       	= 0x98,
//	FT5X0X_REG_MAX_X_LOW             			= 0x99,
//	FT5X0X_REG_MAX_Y_HIGH            			= 0x9a,
//	FT5X0X_REG_MAX_Y_LOW             			= 0x9b,
//	FT5X0X_REG_K_X_HIGH            			= 0x9c,
//	FT5X0X_REG_K_X_LOW             			= 0x9d,
//	FT5X0X_REG_K_Y_HIGH            			= 0x9e,
//	FT5X0X_REG_K_Y_LOW             			= 0x9f,
    FT5X0X_REG_AUTO_CLB_MODE			= 0xa0,
//	FT5X0X_REG_LIB_VERSION_H 				= 0xa1,
//	FT5X0X_REG_LIB_VERSION_L 				= 0xa2,
//	FT5X0X_REG_CIPHER						= 0xa3,
//	FT5X0X_REG_MODE						= 0xa4,
    FT5X0X_REG_PMODE						= 0xa5,	/* Power Consume Mode		*/
    FT5X0X_REG_FIRMID						= 0xa6,
//	FT5X0X_REG_STATE						= 0xa7,
//	FT5X0X_REG_FT5201ID					= 0xa8,
    FT5X0X_REG_ERR						= 0xa9,
    FT5X0X_REG_CLB						= 0xaa,
};


//FT5X0X_REG_PMODE
#define PMODE_ACTIVE        0x00
#define PMODE_MONITOR       0x01
#define PMODE_STANDBY       0x02
#define PMODE_HIBERNATE     0x03

static struct i2c_client * this_client;

#define FT5X0X_FINGER_MAX_NUM      5
#define FT5X0X_KEY_MAX_NUM         3
#define FT5X0X_KEY_IDLE            0
#define FT5X0X_KEY_PRESSED         1
#define FT5X0X_PRESSURE_MAX        255

typedef struct _FT5X0X_FINGER_INFO
{
    u8 last_press;
    u8 curr_press;
    s16 x;
    s16 y;
} FT5X0X_FINGER_INFO;

struct ft5x0x_ts_data
{
    struct input_dev	*input_dev;
    struct work_struct 	pen_event_work;
    struct workqueue_struct *ts_workqueue;
    struct early_suspend	early_suspend;
    u8  touch_point;
    FT5X0X_FINGER_INFO finger_info[FT5X0X_FINGER_MAX_NUM];
};

struct ft5x0x_ts_key_def
{
    int key_value;
    char * key_name;
    s16 x_min;
    s16 x_max;
    s16 y_min;
    s16 y_max;
    int status;
};

#if 0
/* Old touch screen */
static struct ft5x0x_ts_key_def key_data[FT5X0X_KEY_MAX_NUM] = {
	{KEY_MENU, "Menu", 437, 457, 819, 839, FT5X0X_KEY_IDLE},
	{KEY_HOME, "Home", 469, 489, 819, 839, FT5X0X_KEY_IDLE},
	{KEY_BACK, "Back", 501, 521, 819, 839, FT5X0X_KEY_IDLE},
};
#else
/* New touch screen */
static struct ft5x0x_ts_key_def key_data[FT5X0X_KEY_MAX_NUM] =
{
    {KEY_MENU, "Menu", 395, 405, 833, 853, FT5X0X_KEY_IDLE},
    {KEY_HOME, "Home", 427, 437, 833, 853, FT5X0X_KEY_IDLE},
    {KEY_BACK, "Back", 474, 484, 833, 853, FT5X0X_KEY_IDLE},
};
#endif


/***********************************************************************************************
Name	:	ft5x0x_i2c_rxdata

Input	:	*rxdata
                     *length

Output	:	ret

function	:

***********************************************************************************************/
static int ft5x0x_i2c_rxdata(char *rxdata, int length)
{
    int ret;

    struct i2c_msg msgs[] =
    {
        {
            .addr	= this_client->addr,
            .flags	= 0,
            .len	= 1,
            .buf	= rxdata,
        },
        {
            .addr	= this_client->addr,
            .flags	= I2C_M_RD,
            .len	= length,
            .buf	= rxdata,
        },
    };

    ret = i2c_transfer(this_client->adapter, msgs, 2);
    if (ret < 0)
        pr_err("msg %s i2c read error: %d\n", __func__, ret);

    return ret;
}


/***********************************************************************************************
Name	:	ft5x0x_read_reg

Input	:	addr
                     pdata

Output	:

function	:	read register of ft5x0x

***********************************************************************************************/
static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
    int ret;
    u8 buf[2] = {0};

    struct i2c_msg msgs[] =
    {
        {
            .addr	= this_client->addr,
            .flags	= 0,
            .len	= 1,
            .buf	= buf,
        },
        {
            .addr	= this_client->addr,
            .flags	= I2C_M_RD,
            .len	= 1,
            .buf	= buf,
        },
    };

    buf[0] = addr;
    ret = i2c_transfer(this_client->adapter, msgs, 2);
    if (ret < 0)
    {
        pr_err("msg %s i2c read error: %d\n", __func__, ret);
    }
    else
    {
        *pdata = buf[0];
    }
    return ret;

}


/***********************************************************************************************
Name	:	 ft5x0x_read_fw_ver

Input	:	 void


Output	:	 firmware version

function	:	 read TP firmware version

***********************************************************************************************/
static unsigned char ft5x0x_read_fw_ver(void)
{
    unsigned char ver;
    int ret;
    ret = ft5x0x_read_reg(FT5X0X_REG_FIRMID, &ver);
    if (ret < 0)
        ver = 0xAB;
    return(ver);
}


//#define CONFIG_SUPPORT_FTS_CTP_UPG


#ifdef CONFIG_SUPPORT_FTS_CTP_UPG
/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static int ft5x0x_i2c_txdata(char *txdata, int length)
{
    int ret;

    struct i2c_msg msg[] =
    {
        {
            .addr	= this_client->addr,
            .flags	= 0,
            .len	= length,
            .buf	= txdata,
        },
    };

    ret = i2c_transfer(this_client->adapter, msg, 1);
    if (ret < 0)
        pr_err("%s i2c write error: %d\n", __func__, ret);

    return ret;
}

/***********************************************************************************************
Name	:	 ft5x0x_write_reg

Input	:	addr -- address
                     para -- parameter

Output	:

function	:	write register of ft5x0x

***********************************************************************************************/
static int ft5x0x_write_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x0x_i2c_txdata(buf, 2);
    if (ret < 0)
    {
        pr_err("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }

    return 0;
}

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
} E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

#define FTS_NULL                0x0
#define FTS_TRUE                0x01
#define FTS_FALSE              0x0

#define I2C_CTPM_ADDRESS       0xFF //no reference!


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

    ret=i2c_master_recv(this_client, pbt_buf, dw_lenth);

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
    ret=i2c_master_send(this_client, pbt_buf, dw_lenth);
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
    ft5x0x_write_reg(0xfc,0xaa);
    delay_qt_ms(50);
    /*write 0x55 to register 0xfc*/
    ft5x0x_write_reg(0xfc,0x55);
    TSDBG(("[TSP] Step 1: Reset CTPM test\n"));

    delay_qt_ms(30);


    /*********Step 2:Enter upgrade mode *****/
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do
    {
        i ++;
        i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
        delay_qt_ms(5);
    }
    while(i_ret <= 0 && i < 5 );

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
    for (j=0; j<packet_number; j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0; i<FTS_PACKET_LENGTH; i++)
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

        for (i=0; i<temp; i++)
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

static void ft5x0x_release_key(void)
{
    struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
    int i;
    for (i = 0; i<FT5X0X_KEY_MAX_NUM; i++)
    {
        if (key_data[i].status == FT5X0X_KEY_PRESSED)
        {
            TSDBG(("Key %s Released\n", key_data[i].key_name));
            key_data[i].status = FT5X0X_KEY_IDLE;
            input_report_key(data->input_dev,	 key_data[i].key_value, 0);
        }
    }

}

/***********************************************************************************************
Name:	  ft5x0x_ts_release
Input:    void
Output:	  void
function: no touch handle function
***********************************************************************************************/
static void ft5x0x_ts_release(void)
{
    struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
    ft5x0x_release_key();
	input_report_key(data->input_dev, BTN_TOUCH, 0);
    input_sync(data->input_dev);
}

static int ft5x0x_read_data(void)
{
    struct ft5x0x_ts_data *ts = i2c_get_clientdata(this_client);
    FT5X0X_FINGER_INFO * finger_info = ts->finger_info;
    u8 buf[32] = {0};
    int ret = -1;
    int i;

	if (ts->input_dev == NULL)
		return -1;
    ret = ft5x0x_i2c_rxdata(buf, 31);
#if 0
    for (i=0; i<31; i++)
    {
        printk("[%02x] = %02x ", i, buf[i]);
        if ((i==7) || (i==15) || (i==23))
            printk("\n");
    }
    printk("\n");
#endif

    if (ret < 0)
    {
        TSDBG(("%s read_data i2c_rxdata failed: %d\n", __func__, ret));
        return ret;
    }

    ts->touch_point = buf[2] & 0x07;// 000 0111

    if (ts->touch_point == 0)
    {
        TSDBG(("ft5x0x_read_data, touch_point = 0, ft5x0x_ts_release\n"));
        ft5x0x_ts_release();
        return 1;
    }
    else
    {
        TSDBG(("ft5x0x_read_data, touch_point = %d\n", ts->touch_point));
    }

    switch (ts->touch_point)
    {
    case 5:
        finger_info[4].x = (s16)(buf[0x1b] & 0x0F)<<8 | (s16)buf[0x1c];
        finger_info[4].y = (s16)(buf[0x1d] & 0x0F)<<8 | (s16)buf[0x1e];
    case 4:
        finger_info[3].x = (s16)(buf[0x15] & 0x0F)<<8 | (s16)buf[0x16];
        finger_info[3].y = (s16)(buf[0x17] & 0x0F)<<8 | (s16)buf[0x18];
    case 3:
        finger_info[2].x = (s16)(buf[0x0f] & 0x0F)<<8 | (s16)buf[0x10];
        finger_info[2].y = (s16)(buf[0x11] & 0x0F)<<8 | (s16)buf[0x12];
    case 2:
        finger_info[1].x = (s16)(buf[9] & 0x0F)<<8 | (s16)buf[10];
        finger_info[1].y = (s16)(buf[11] & 0x0F)<<8 | (s16)buf[12];
    case 1:
        finger_info[0].x = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
        finger_info[0].y = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
        break;
    default:
        ts->touch_point = 0;
        return -1;
    }

    for (i=0; i<ts->touch_point; i++)
    {
//if (event->x[i] < SCREEN_MAX_X)
//	event->x[i] = SCREEN_MAX_X - event->x[i] - 1;
        TSDBG(("Point %d, x = %d, y = %d\n", i + 1, finger_info[i].x, finger_info[i].y));
    }


    return 0;
}

static struct ft5x0x_ts_key_def * ft5x0x_get_key_p(int x, int y)
{
    int i;
    for (i = 0; i<FT5X0X_KEY_MAX_NUM; i++)
    {
        if (   (x >= key_data[i].x_min)
                && (x <= key_data[i].x_max)
                && (y >= key_data[i].y_min)
                && (y <= key_data[i].y_max))
            return key_data + i;
    }

    return NULL;
}

/***********************************************************************************************
Name: ft5x0x_report_value
Input: void
Output:	void
function: report the point information to upper layer
***********************************************************************************************/
static void ft5x0x_report_value(void)
{
    struct ft5x0x_ts_data *ts = i2c_get_clientdata(this_client);
    FT5X0X_FINGER_INFO *finger_info = ts->finger_info;
    struct ft5x0x_ts_key_def * p = NULL;
    int i;
    int key_pressed = 0;
    int finger_num = 0;
    int point_up = 0;
    int first_touch = 1;

	if (ts->input_dev == NULL)
		return;

    for (i=0; i<FT5X0X_FINGER_MAX_NUM; i++)
    {
        ts->finger_info[i].curr_press = 0;

        if (i < ts->touch_point)
        {
            if ((finger_info[i].x < SCREEN_MAX_X) && (finger_info[i].y < SCREEN_MAX_Y))
            {
                ts->finger_info[i].curr_press = 1;
                finger_num++;
            }
        }
    }

    if (finger_num != 0)
    {
        for (i=0; i<FT5X0X_FINGER_MAX_NUM; i++)
        {
            if (finger_info[i].last_press)
            {
                first_touch = 0; //not first touch
                break;
            }
        }
    }

    for (i=0; i<FT5X0X_FINGER_MAX_NUM; i++)
    {
        if (finger_info[i].curr_press)
        {
            input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,1);
            if (first_touch)
            {
                first_touch = 0;
                input_report_abs(ts->input_dev, ABS_PRESSURE, FT5X0X_PRESSURE_MAX);
                input_report_key(ts->input_dev, BTN_TOUCH, 1);
            }

            input_report_abs(ts->input_dev, ABS_MT_POSITION_X, finger_info[i].x);
            input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, finger_info[i].y);
            input_mt_sync(ts->input_dev);
        }
        else
        {
            if (finger_info[i].last_press != 0)
            {
                point_up = 1;
            }
        }

        finger_info[i].last_press = finger_info[i].curr_press;
    }

    /* Last finger up */
    if (!finger_num && point_up)
    {
        input_report_abs(ts->input_dev, ABS_PRESSURE, 0);
        input_report_key(ts->input_dev, BTN_TOUCH, 0);
        input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
        input_mt_sync(ts->input_dev);
    }

    /* Handle Key */
    key_pressed = 0;

    /* if no point pressed, handle key */
    if (!finger_num)
    {
        for (i=0; i<ts->touch_point; i++)
        {
            p = ft5x0x_get_key_p(finger_info[i].x, finger_info[i].y);

            if (p)
            {
                if (p->status == FT5X0X_KEY_IDLE)
                {
                    p->status = FT5X0X_KEY_PRESSED;
                    TSDBG(("key %s pressed\n", p->key_name));
                    input_report_key(ts->input_dev,	 p->key_value, 1);
                }
                key_pressed = 1;
            }
        }
    }

    if (!key_pressed)
    {
        ft5x0x_release_key();
    }

    input_sync(ts->input_dev);
}	/*end ft5x0x_report_value*/
/***********************************************************************************************
Name: ft5x0x_ts_pen_irq_work
Input: irq source
Output:	void
function: handle later half irq
***********************************************************************************************/
static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
{
    int ret = -1;
    ret = ft5x0x_read_data();
    if (ret == 0)
    {
        TSDBG(("call ft5x0x_report_value\n"));
        ft5x0x_report_value();
    }
    if (this_client)
        enable_irq(this_client->irq);
}
/***********************************************************************************************
Name: ft5x0x_ts_interrupt
Input: irq, dev_id
Output: IRQ_HANDLED
function:
***********************************************************************************************/
static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{
    struct ft5x0x_ts_data *ft5x0x_ts = dev_id;
    disable_irq_nosync(this_client->irq);
//	TSDBG("==int=\n");
    if (!work_pending(&ft5x0x_ts->pen_event_work))
    {
        queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
    }

    return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x0x_ts_suspend
Input: handler
Output: void
function: suspend function for power management
***********************************************************************************************/
static void ft5x0x_ts_suspend(struct early_suspend *handler)
{
//	struct ft5x0x_ts_data *ts;
//	ts =  container_of(handler, struct ft5x0x_ts_data, early_suspend);

    TSDBG(("==ft5x0x_ts_suspend=\n"));
    disable_irq(this_client->irq);

//	disable_irq(this_client->irq);
//	disable_irq(this_client->irq);
//	cancel_work_sync(&ts->pen_event_work);
//	flush_workqueue(ts->ts_workqueue);
    // ==set mode ==,
//    	ft5x0x_set_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);
}
/***********************************************************************************************
Name: ft5x0x_ts_resume
Input:	handler
Output:	void
function: resume function for powermanagement
***********************************************************************************************/
static void ft5x0x_ts_resume(struct early_suspend *handler)
{
    TSDBG(("==ft5x0x_ts_resume=\n"));
    enable_irq(this_client->irq);
}
#endif  //CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name: ft5x0x_ts_probe
Input: client, id
Output: 0 if OK, other value indicate error
function: probe
***********************************************************************************************/
static int
ft5x0x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ft5x0x_ts_data *ts;
    struct input_dev *input_dev;
    int err = 0;
    unsigned char uc_reg_value;
    int i;

    printk("ft5x0x_ts_probe\n");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        err = -ENODEV;
        goto exit_check_functionality_failed;
    }

    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if (!ts)
    {
        err = -ENOMEM;
        goto exit_alloc_data_failed;
    }
	ts->input_dev = NULL;

    for (i=0; i<FT5X0X_FINGER_MAX_NUM; i++)
    {
        ts->finger_info[i].last_press = 0;
    }

    this_client = client;

	client->irq = FTS_TS_INT;
    i2c_set_clientdata(client, ts);

    uc_reg_value = ft5x0x_read_fw_ver();
    if (0xAB == uc_reg_value)
    {
        printk("ft5x0x_ts_probe get fw version error.\n");
        goto exit_get_fwver_failed;
    }
    else
    {
        printk("[FST] Firmware version = 0x%x\n", uc_reg_value);
    }

    INIT_WORK(&ts->pen_event_work, ft5x0x_ts_pen_irq_work);

    ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!ts->ts_workqueue)
    {
        err = -ESRCH;
        goto exit_create_singlethread;
    }

    err = request_irq(client->irq, ft5x0x_ts_interrupt, IRQF_TRIGGER_FALLING, "ft5x0x_ts", ts);
    if (err < 0)
    {
        dev_err(&client->dev, "ft5x0x_probe: request irq failed\n");
        goto exit_irq_request_failed;
    }

    disable_irq(client->irq);

    input_dev = input_allocate_device();
    if (!input_dev)
    {
        err = -ENOMEM;
        dev_err(&client->dev, "failed to allocate input device\n");
        goto exit_input_dev_alloc_failed;
    }

    ts->input_dev = input_dev;

    set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
    set_bit(ABS_MT_POSITION_X,  input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y,  input_dev->absbit);
    set_bit(EV_KEY,			    input_dev->evbit);
    set_bit(EV_ABS,			    input_dev->evbit);
    set_bit(BTN_TOUCH,          input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT,  input_dev->propbit);

    /* Enable all supported keys */
    for (i=0; i<FT5X0X_KEY_MAX_NUM; i++)
        __set_bit(key_data[i].key_value, input_dev->keybit);

    __clear_bit(KEY_RESERVED, input_dev->keybit);

    input_set_abs_params(input_dev, ABS_MT_POSITION_X,   0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y,   0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,  0, PRESS_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_PRESSURE, 0, FT5X0X_PRESSURE_MAX, 0, 0);

    input_dev->name		= FT5X0X_NAME;		//dev_name(&client->dev)
    err = input_register_device(input_dev);
    if (err)
    {
        dev_err(&client->dev,
                "ft5x0x_ts_probe: failed to register input device: %s\n",
                dev_name(&client->dev));
        goto exit_input_register_device_failed;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    TSDBG(("==register_early_suspend =\n"));
    ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    ts->early_suspend.suspend = ft5x0x_ts_suspend;
    ts->early_suspend.resume	= ft5x0x_ts_resume;
    register_early_suspend(&ts->early_suspend);
#endif

    msleep(50);

    enable_irq(this_client->irq);

    msm_tp_set_found_flag(1);
    printk("probe over\n");
    return 0;

exit_input_register_device_failed:
    input_free_device(input_dev);
exit_input_dev_alloc_failed:
    free_irq(this_client->irq, ts);
exit_irq_request_failed:
//exit_platform_data_null:
    cancel_work_sync(&ts->pen_event_work);
    destroy_workqueue(ts->ts_workqueue);
exit_create_singlethread:
exit_get_fwver_failed:
    i2c_set_clientdata(client, NULL);
    kfree(ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
    return err;
}
/***********************************************************************************************
Name: ft5x0x_ts_remove
Input: client
Output: always 0
function: remove the driver
***********************************************************************************************/
static int __devexit ft5x0x_ts_remove(struct i2c_client *client)
{
    struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(client);
    TSDBG(("==ft5x0x_ts_remove=\n"));
    unregister_early_suspend(&ft5x0x_ts->early_suspend);
    free_irq(this_client->irq, ft5x0x_ts);
    input_unregister_device(ft5x0x_ts->input_dev);
    kfree(ft5x0x_ts);
    cancel_work_sync(&ft5x0x_ts->pen_event_work);
    destroy_workqueue(ft5x0x_ts->ts_workqueue);
    i2c_set_clientdata(client, NULL);
    return 0;
}

static const struct i2c_device_id ft5x0x_ts_id[] =
{
    { FT5X0X_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver =
{
    .probe		= ft5x0x_ts_probe,
    .remove		= __devexit_p(ft5x0x_ts_remove),
    .id_table	= ft5x0x_ts_id,
    .driver	= {
        .name	= FT5X0X_NAME,
        .owner	= THIS_MODULE,
    },
};

static int __init ft5x0x_ts_init(void)
{
    int ret;
    TSDBG(("==ft5x0x_ts_init==\n"));
    if (msm_tp_get_found_flag())
    {
        return -1;
    }
    ret = i2c_add_driver(&ft5x0x_ts_driver);
    TSDBG(("ret=%d\n",ret));
    return ret;
}

static void __exit ft5x0x_ts_exit(void)
{
    TSDBG(("==ft5x0x_ts_exit==\n"));
    i2c_del_driver(&ft5x0x_ts_driver);
}

late_initcall(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");

