/******************************************************************************** 
* (c) COPYRIGHT 2010 RAONTECH, Inc. ALL RIGHTS RESERVED.
* 
* This software is the property of RAONTECH and is furnished under license by RAONTECH.                
* This software may be used only in accordance with the terms of said license.                         
* This copyright noitce may not be remoced, modified or obliterated without the prior                  
* written permission of RAONTECH, Inc.                                                                 
*                                                                                                      
* This software may not be copied, transmitted, provided to or otherwise made available                
* to any other person, company, corporation or other entity except as specified in the                 
* terms of said license.                                                                               
*                                                                                                      
* No right, title, ownership or other interest in the software is hereby granted or transferred.       
*                                                                                                      
* The information contained herein is subject to change without notice and should 
* not be construed as a commitment by RAONTECH, Inc.                                                                    
* 
* TITLE 	  : RAONTECH TV configuration header file. 
*
* FILENAME    : raontv_port.h
*
* DESCRIPTION : 
*		Configuration for RAONTECH TV Services.
*
********************************************************************************/

/******************************************************************************** 
* REVISION HISTORY
*
*    DATE	  	  NAME				REMARKS
* ----------  -------------    --------------------------------------------------
* 10/12/2010  Ko, Kevin        Added the code of conutry for RTV_CONUTRY_ARGENTINA.
* 10/01/2010  Ko, Kevin        Changed the debug message macro names.
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
* 04/09/2010  Yang, Maverick   REV1 SETTING 
* 01/25/2010  Yang, Maverick   Created.                                                   
********************************************************************************/

#ifndef __RAONTV_PORT_H__
#define __RAONTV_PORT_H__

/*=============================================================================
 * Includes the user header files if neccessry.
 *============================================================================*/ 
#if defined(__KERNEL__) /* Linux kernel */
    #include <linux/io.h>
    #include <linux/kernel.h>
    #include <linux/delay.h>
    #include <linux/mm.h>
    #include <linux/mutex.h>
    #include <linux/uaccess.h>
	
#elif defined(WINCE)
    #include <windows.h>
    #include <drvmsg.h>
    
#else
	#include <stdio.h>   
#endif

#ifdef __cplusplus 
extern "C"{ 
#endif  

/*############################################################################
#
# COMMON configurations
#
############################################################################*/
/*============================================================================
* The slave address for I2C and SPI, the base address for EBI2.
*===========================================================================*/
#define RAONTV_CHIP_ADDR	0x86 

/*============================================================================
* Modifies the basic data types if neccessry.
*===========================================================================*/
typedef int					BOOL;
typedef signed char			S8;
typedef unsigned char		U8;
typedef signed short		S16;
typedef unsigned short		U16;
typedef signed int			S32;
typedef unsigned int		U32;

typedef int                 INT;
typedef unsigned int        UINT;
typedef long                LONG;
typedef unsigned long       ULONG;
 
typedef volatile U8			VU8;
typedef volatile U16		VU16;
typedef volatile U32		VU32;

#define INLINE		__inline


/*============================================================================
* Selects the TV mode(s) to target product.
*===========================================================================*/
#define RTV_ISDBT_ENABLE

/*============================================================================
* Defines the package type of chip to target product.
*===========================================================================*/
//#define RAONTV_CHIP_PKG_WLCSP	// MTV220/318/221
#define RAONTV_CHIP_PKG_QFN		// MTV818
//#define RAONTV_CHIP_PKG_LGA	// MTV250/251/350/351


/*============================================================================
* Defines the external source freqenecy in KHz.
* Ex> #define RTV_SRC_CLK_FREQ_KHz	36000 // 36MHz
*=============================================================================
* MTV250 : #define RTV_SRC_CLK_FREQ_KHz  32000  //must be defined 
* MTV350 : #define RTV_SRC_CLK_FREQ_KHz  24576  //must be defined 
*===========================================================================*/
#define RTV_SRC_CLK_FREQ_KHz			26000
//#define RTV_SRC_CLK_FREQ_KHz			36000
//#define RTV_SRC_CLK_FREQ_KHz			19200
	

/*============================================================================
* Define the power type.
*============================================================================*/  
//#define RTV_PWR_EXTERNAL
#define RTV_PWR_LDO
//#define RTV_PWR_DCDC


/*============================================================================
* Defines the I/O voltage.
*===========================================================================*/
#define RTV_IO_1_8V
//#define RTV_IO_2_5V
//#define RTV_IO_3_3V


#if defined(RTV_IO_2_5V) || defined(RTV_IO_3_3V)
	#error "If VDDIO pin is connected with IO voltage, RTV_IO_1_8V must be defined,please check the HW pin connection"
	#error "If VDDIO pin is connected with GND, IO voltage must be defined same as AP IO voltage"
#endif


/*============================================================================
* Defines the Host interface.
*===========================================================================*/
//#define RTV_IF_MPEG2_SERIAL_TSIF // I2C + TSIF Master Mode. 
//#define RTV_IF_MPEG2_PARALLEL_TSIF // I2C + TSIF Master Mode. Support only 1seg &TDMB Application!
#define RTV_IF_QUALCOMM_TSIF // I2C + TSIF Master Mode
//define RTV_IF_SPI // AP: SPI Master Mode
//#define RTV_IF_SPI_SLAVE // AP: SPI Slave Mode
//#define RTV_IF_EBI2 // External Bus Interface Slave Mode

/*============================================================================
* Defines the clear mode of interrupts.
* 1. RTV_MSC_INTR_ISTAUS_ACC_CLR_MODE
*		In case, the interface is SPI/EBI2 
*			AND the ISR of platform is the nested ISR.
*
* 2. RTV_MSC_INTR_MEM_ACC_CLR_MODE
*		In case, the interface is TSIF.
*		In case, the interface is SPI/EBI2 
*			AND the ISR of platform is NOT nested ISR.
*===========================================================================*/
//#define RTV_MSC_INTR_ISTAUS_ACC_CLR_MODE
#define RTV_MSC_INTR_MEM_ACC_CLR_MODE


/*============================================================================
* Defines the delay macro in milliseconds.
*===========================================================================*/
#if defined(__KERNEL__) /* Linux kernel */
	#define RTV_DELAY_MS(ms)    mdelay(ms) 

#elif defined(WINCE)
	#define RTV_DELAY_MS(ms)    Sleep(ms) 

#else
	extern void mtv818_delay_ms(int ms);
	#define RTV_DELAY_MS(ms) 	mtv818_delay_ms(ms) // TODO
#endif

/*============================================================================
* Defines the debug message macro.
*===========================================================================*/
#if 1
	#define RTV_DBGMSG0(fmt)					printk(fmt)
	#define RTV_DBGMSG1(fmt, arg1)				printk(fmt, arg1) 
	#define RTV_DBGMSG2(fmt, arg1, arg2)		printk(fmt, arg1, arg2) 
	#define RTV_DBGMSG3(fmt, arg1, arg2, arg3)	printk(fmt, arg1, arg2, arg3) 
#else
	/* To eliminates the debug messages. */
	#define RTV_DBGMSG0(fmt)					((void)0) 
	#define RTV_DBGMSG1(fmt, arg1)				((void)0) 
	#define RTV_DBGMSG2(fmt, arg1, arg2)		((void)0) 
	#define RTV_DBGMSG3(fmt, arg1, arg2, arg3)	((void)0) 
#endif
/*#### End of Common ###########*/


/*############################################################################
#
# ISDB-T specific configurations
#
############################################################################*/
/*============================================================================
* Defines the NOTCH FILTER setting Enable.
* In order to reject GSM/CDMA blocker, NOTCH FILTER must be defined.
* This feature used for module company in the JAPAN.
*===========================================================================*/
//#if defined(RTV_ISDBT_ENABLE)  //Do not use
//	#ifdef RAONTV_CHIP_PKG_WLCSP
	//	#define RTV_NOTCH_FILTER_ENABLE  
//	#endif
//#endif

#if defined(RTV_NOTCH_FILTER_ENABLE)
	#error "RTV_NOTCH_FILTER_ENABLE must be confirmed by RAONTECH"
#endif

/*##############################################################################
#
# T-DMB/DAB specific configurations
#
############################################################################*/
#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
	/* Defines the FIC processing mode. */
	//#define RTV_FIC_POLLING_MODE

	/* Defines the number of AV service. (0 ~ 1) */
	#define RTV_MAX_NUM_DAB_AV_SVC	1

	/* Defines the number DATA services. (TSIF: 0 ~ 3, SPI: 0 ~ 4) */
	#define RTV_MAX_NUM_DAB_DATA_SVC	0

	#ifdef RTV_DAB_ENABLE
		/* Select the DAB L-BABD used or not. */
		#define RTV_DAB_LBAND_ENABLED
		
		/* Select the DAB reconfig interrupt or not. */
		#define RTV_DAB_RECONFIG_ENABLED
	#endif
#endif /* #if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)  */

/*############################################################################
#
# FM specific configurations
#
############################################################################*/
#ifdef RTV_FM_ENABLE
	#define RTV_FM_CH_MIN_FREQ_KHz		76000
	#define RTV_FM_CH_MAX_FREQ_KHz		108000
	#define RTV_FM_CH_STEP_FREQ_KHz		100 // in KHz

	//#define RTV_FM_RDS_ENABLED

	#if defined(__KERNEL__) /* Linux kernel */
		//#define RTV_FM_SCAN_LINUX_USER_SPACE_COPY_USED
	#endif
#endif


/*############################################################################
#
# Host Interface specific configurations
#
############################################################################*/
#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_QUALCOMM_TSIF)\
|| defined(RTV_IF_MPEG2_PARALLEL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	/*=================================================================
	* Defines the TSIF interface for MPEG2 or QUALCOMM TSIF.	 
	*================================================================*/
//	#define RTV_TSIF_FORMAT_1
//	#define RTV_TSIF_FORMAT_2
//	#define RTV_TSIF_FORMAT_3
	//#define RTV_TSIF_FORMAT_4
	#define RTV_TSIF_FORMAT_5

//	#define RTV_TSIF_CLK_SPEED_DIV_2 // Host Clk/2
	#define RTV_TSIF_CLK_SPEED_DIV_4 // Host Clk/4
//	#define RTV_TSIF_CLK_SPEED_DIV_6 // Host Clk/6
//	#define RTV_TSIF_CLK_SPEED_DIV_8 // Host Clk/8

	/*=================================================================
	* Defines the register I/O macros.
	*================================================================*/
	unsigned char mtv_i2c_read(U8 reg);
	void mtv_i2c_read_burst(U8 reg, U8 *buf, int size);
	void mtv_i2c_write(U8 reg, U8 val);
	#define	RTV_REG_GET(reg)			mtv_i2c_read((U8)(reg))
	#define	RTV_REG_BURST_GET(reg, buf, size)	mtv_i2c_read_burst((U8)(reg), buf, size)
	#define	RTV_REG_SET(reg, val)			mtv_i2c_write((U8)(reg), (U8)(val))
	#define	RTV_REG_MASK_SET(reg, mask, val)\
		do {					\
			U8 tmp;				\
			tmp = (RTV_REG_GET(reg)|(U8)(mask)) & (U8)((~(mask))|(val));\
			RTV_REG_SET(reg, tmp);		\
		} while(0)

#elif defined(RTV_IF_SPI)
	/*=================================================================
	* Defines the register I/O macros.
	*================================================================*/
	unsigned char mtv_spi_read(unsigned char reg);
	void mtv_spi_read_burst(unsigned char reg, unsigned char *buf, int size);
	void mtv_spi_write(unsigned char reg, unsigned char val);

	#define	RTV_REG_GET(reg)            			(U8)mtv_spi_read((U8)(reg))
	#define	RTV_REG_BURST_GET(reg, buf, size) 		mtv_spi_read_burst((U8)(reg), buf, (size))
	#define	RTV_REG_SET(reg, val)       			mtv_spi_write((U8)(reg), (U8)(val))       
	#define	RTV_REG_MASK_SET(reg, mask, val)\
		do {					\
			U8 tmp;				\
			tmp = (RTV_REG_GET(reg)|(U8)(mask)) & (U8)((~(mask))|(val));\
			RTV_REG_SET(reg, tmp);		\
		} while(0)
    
#elif defined(RTV_IF_EBI2)
	/*=================================================================
	* Defines the register I/O macros.
	*================================================================*/
	#define RTV_EBI2_MEM_WITDH  8 // 
	//#define RTV_EBI2_MEM_WITDH  16 // 
	//#define RTV_EBI2_MEM_WITDH  32 //
		
    #if (RTV_EBI2_MEM_WITDH == 8)
	extern VU8 g_bRtvEbiMapSelData;

	static INLINE U8 RTV_REG_GET(U8 reg)
	{	
		U8 bData;
		if(reg == 0x3)
		{
			bData = g_bRtvEbiMapSelData;
			if(bData ==0x09 || bData == 0x0A || bData == 0x0B || bData == 0x0C )
			{
				*(VU8 *)(RAONTV_CHIP_ADDR | 0x04 ) = 0x03;
				*(VU8 *)(RAONTV_CHIP_ADDR | 0x06 ) = 0x00;
				*(VU8 *)(RAONTV_CHIP_ADDR | 0x00 ) = bData;
			}
		}
		else
		{
			   *(VU8 *)(RAONTV_CHIP_ADDR | 0x04 ) = reg;
			   *(VU8 *)(RAONTV_CHIP_ADDR | 0x06 ) = g_bRtvEbiMapSelData;
			   bData = *(VU8 *)(RAONTV_CHIP_ADDR | 0x00 );
		
		}	

		return bData;
	}

	static INLINE void RTV_REG_SET(U8 reg, U8 val)
	{
		if(reg == 0x3) 
		{
		   	g_bRtvEbiMapSelData = val;
			if(val ==0x09 || val ==0x0A || val ==0x0B || val ==0x0C )
			{
				   *(VU8 *)(RAONTV_CHIP_ADDR | 0x04 ) = 0x03;
				   *(VU8 *)(RAONTV_CHIP_ADDR | 0x06 ) = 0x00;
				   *(VU8 *)(RAONTV_CHIP_ADDR | 0x00 ) = val;
			}
		}
		else
		{
			   *(VU8 *)(RAONTV_CHIP_ADDR | 0x04 ) = reg;
			   *(VU8 *)(RAONTV_CHIP_ADDR | 0x06 ) = g_bRtvEbiMapSelData;
			   *(VU8 *)(RAONTV_CHIP_ADDR | 0x00 ) = val;
		}
	}
	
	#define RTV_REG_MASK_SET(reg, mask, val)								\
		do {																\
		U8 tmp;															\
		tmp = (RTV_REG_GET(reg)|(U8)(mask)) & (U8)((~(mask))|(val));	\
		RTV_REG_SET(reg, tmp);											\
		} while(0)
		    
    #elif (RTV_EBI2_MEM_WITDH == 16)
    
    #elif (RTV_EBI2_MEM_WITDH == 32)
        
    #else
        #error "Can't support to memory witdh!"
    #endif
    
#else
	#error "Must define the interface definition !"
#endif


#if defined(RTV_IF_SPI) || defined(RTV_FIC_INTR_ENABLED)
	#if defined(__KERNEL__)	
		extern struct mutex raontv_guard;
		#define RTV_GUARD_INIT		mutex_init(&raontv_guard)
		#define RTV_GUARD_LOCK		mutex_lock(&raontv_guard)
		#define RTV_GUARD_FREE		mutex_unlock(&raontv_guard)
		#define RTV_GUARD_DEINIT 	((void)0)
		
    #elif defined(WINCE)        
	        extern CRITICAL_SECTION		raontv_guard;
	        #define RTV_GUARD_INIT		InitializeCriticalSection(&raontv_guard)
	        #define RTV_GUARD_LOCK		EnterCriticalSection(&raontv_guard)
	        #define RTV_GUARD_FREE		LeaveCriticalSection(&raontv_guard)
	        #define RTV_GUARD_DEINIT		DeleteCriticalSection(&raontv_guard)
	#else
		// temp: TODO
		#define RTV_GUARD_INIT		((void)0)
		#define RTV_GUARD_LOCK		((void)0)
		#define RTV_GUARD_FREE 	((void)0)
		#define RTV_GUARD_DEINIT 	((void)0)
	#endif
	
#else
	#define RTV_GUARD_INIT		((void)0)
	#define RTV_GUARD_LOCK		((void)0)
	#define RTV_GUARD_FREE 	((void)0)
	#define RTV_GUARD_DEINIT 	((void)0)
#endif



/*############################################################################
#
# Pre-definintion by RAONTECH.
# Assume that FM only project was not exist.
#
############################################################################*/
#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_QUALCOMM_TSIF)\
|| defined(RTV_IF_MPEG2_PARALLEL_TSIF)
	#define RTV_IF_TSIF
#endif
	
#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_QUALCOMM_TSIF)
	#define RTV_IF_SERIAL_TSIF /* Serial TSIF only */
#endif



#if (defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE))\
&& !defined(RTV_FIC_POLLING_MODE)
	#define RTV_FIC_INTR_ENABLED /* FIC Interrupt mode. */
#endif

#if (defined(RTV_TDMB_ENABLE)||defined(RTV_DAB_ENABLE))\
&& !(defined(RTV_ISDBT_ENABLE)||defined(RTV_FM_ENABLE)) 
	/* Only TDMB or DAB enabled. */
	#define RTV_TDMBorDAB_ONLY_ENABLED

#elif !(defined(RTV_TDMB_ENABLE)||defined(RTV_DAB_ENABLE) || defined(RTV_FM_ENABLE))\
&& defined(RTV_ISDBT_ENABLE)
	/* Only 1SEG enabled. */
	#define RTV_ISDBT_ONLY_ENABLED

#elif (defined(RTV_TDMB_ENABLE)||defined(RTV_DAB_ENABLE)) && defined(RTV_FM_ENABLE)\
&& !defined(RTV_ISDBT_ENABLE)
		/* Only TDMB/DAB and FM  enabled. */
	#define RTV_TDMBorDAB_FM_ENABLED
#endif


#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
	/* Defines the number of Sub Channel to be open simultaneously. (AV + DATA) */
	#define RTV_NUM_DAB_AVD_SERVICE	(RTV_MAX_NUM_DAB_AV_SVC+RTV_MAX_NUM_DAB_DATA_SVC)
	
	/* Configured by RAONTECH. */
	#ifdef RTV_FIC_POLLING_MODE
		#define RTV_NUM_FIC_SVC	0
	#else
		#define RTV_NUM_FIC_SVC	1 /* FIC Interrupt mode */
	#endif

	#if ((RTV_NUM_DAB_AVD_SERVICE + RTV_NUM_FIC_SVC) > 1)
		#define RTV_MULTI_SERVICE_MODE
	#endif

	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		/* Defines CIF mode by number of subchannel. */
		#if (RTV_MAX_NUM_DAB_DATA_SVC >= 2) /* more 1 data */
			#define RTV_CIF_MODE_ENABLED
			#define RTV_CIF_HEADER_INSERTED 
		#endif
	#else
		#if ((RTV_NUM_DAB_AVD_SERVICE + RTV_NUM_FIC_SVC) > 1)
			#define RTV_CIF_MODE_ENABLED

			/* Alwayas has a CIF Header in TSIF interface. */
			#define RTV_CIF_HEADER_INSERTED
		#endif
	#endif

	#if ((RTV_NUM_DAB_AVD_SERVICE == 0) && (RTV_NUM_FIC_SVC == 1))\
	&& defined(RTV_FIC_INTR_ENABLED)
		#define RTV_NOSVC_FIC_INTR_ONLY_MODE
	#endif
#endif

#if defined(RTV_FM_ENABLE) && defined(RTV_FM_RDS_ENABLED)
	#ifndef RTV_MULTI_SERVICE_MODE
		#define RTV_MULTI_SERVICE_MODE
	#endif
#endif

/* Define the state of MSC1 memory usage for MTV chip. */
#if defined(RTV_IF_SPI)
	#if defined(RTV_ISDBT_ENABLE)\
	|| (defined(RTV_FM_ENABLE) && !defined(RTV_FM_RDS_ENABLED))\
	|| ((defined(RTV_TDMB_ENABLE)||defined(RTV_DAB_ENABLE))\
		&& (((RTV_MAX_NUM_DAB_AV_SVC == 0) && (RTV_MAX_NUM_DAB_DATA_SVC == 1))\
			|| (RTV_MAX_NUM_DAB_AV_SVC >= 1)))

	/* TDMB/DAB: (Av:0 ea, DATA: 1ea) or (AV >=1) */
	#define RTV_MSC1_ENABLED
	
	#endif
#endif /* #if defined(RTV_IF_SPI) */

/* Define the state of MSC0 memory usage for MTV chip. */
#if defined(RTV_IF_SPI)
	#if (defined(RTV_FM_ENABLE) && defined(RTV_FM_RDS_ENABLED))\
	|| ((defined(RTV_TDMB_ENABLE)||defined(RTV_DAB_ENABLE))\
		&& (((RTV_MAX_NUM_DAB_AV_SVC == 1) && (RTV_MAX_NUM_DAB_DATA_SVC == 1))\
			|| (RTV_MAX_NUM_DAB_DATA_SVC >= 2)))

	/* TDMB/DAB: (Av:1 ea, DATA: 1ea) or (AV: 0, DATA >=2) */
	#define RTV_MSC0_ENABLED

	#endif
#endif /* #if defined(RTV_IF_SPI) */


/*############################################################################
#
# Check erros by user-configurations.
#
############################################################################*/
#if !defined(RAONTV_CHIP_PKG_WLCSP) && !defined(RAONTV_CHIP_PKG_QFN)\
&& !defined(RAONTV_CHIP_PKG_LGA)
	#error "Must define the package type !"
#endif

#if !defined(RTV_PWR_EXTERNAL) && !defined(RTV_PWR_LDO)  && !defined(RTV_PWR_DCDC)
	#error "Must define the power type !"
#endif

#if !defined(RTV_IO_1_8V) && !defined(RTV_IO_2_5V)  && !defined(RTV_IO_3_3V)
	#error "Must define I/O voltage!"
#endif

 
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE) ||defined(RTV_IF_SPI)
    #if (RAONTV_CHIP_ADDR >= 0xFF)
        #error "Invalid chip address"
    #endif
#elif defined(RTV_IF_EBI2)
    #if (RAONTV_CHIP_ADDR <= 0xFF)
        #error "Invalid chip address"
    #endif
    
#else
	#error "Must define the interface definition !"
#endif


#if defined(RTV_DAB_ENABLE) && defined(RTV_TDMB_ENABLE)
	#error "Should select one of RTV_DAB_ENABLE or RTV_TDMB_ENABLE"
#endif

#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
	#if !defined(RTV_MAX_NUM_DAB_AV_SVC) && !defined(RTV_MAX_NUM_DAB_DATA_SVC)
		#error "Should be define both!"
	#endif

	#if (RTV_MAX_NUM_DAB_AV_SVC < 0) || (RTV_MAX_NUM_DAB_AV_SVC > 1)
		#error "Must 0 or 1 for AV service"
	#endif

	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		#if (RTV_MAX_NUM_DAB_DATA_SVC < 0) || (RTV_MAX_NUM_DAB_DATA_SVC > 4)
			#error "Must from 0 to 4 for DATA service"
		#endif

	#else
		#if (RTV_MAX_NUM_DAB_DATA_SVC < 0) || (RTV_MAX_NUM_DAB_DATA_SVC > 3)
			#error "Must from 0 to 3 for DATA service"
		#endif
	#endif
#endif


#if !defined(RTV_TDMB_ENABLE) && !defined(RTV_DAB_ENABLE)
	/* To prevent the compile error. */
	#define RTV_NUM_DAB_AVD_SERVICE	1
	#define RTV_MAX_NUM_DAB_DATA_SVC 	1
#endif


#ifdef RTV_IF_MPEG2_PARALLEL_TSIF
	#if defined(RTV_FM_ENABLE) || defined(RTV_DAB_ENABLE)\
	|| defined(RAONTV_CHIP_PKG_WLCSP)  || defined(RAONTV_CHIP_PKG_LGA)
		#error "Not support parallel TSIF!"
	#endif
	
	#if defined(RTV_TDMB_ENABLE) && (RTV_NUM_DAB_AVD_SERVICE > 1)
		#error "Not support T-DMB multi sub channel mode!"
	#endif

	#if defined(RTV_DAB_ENABLE) && (RTV_NUM_DAB_AVD_SERVICE > 1)
		#error "Not support DAB multi sub channel mode!"
	#endif
#endif



#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2) 
	#if !defined(RTV_MSC_INTR_MEM_ACC_CLR_MODE)\
	&& !defined(RTV_MSC_INTR_ISTAUS_ACC_CLR_MODE)
		#error " Should selects an interrupt clear mode"
	#endif

	#if defined(RTV_MSC_INTR_MEM_ACC_CLR_MODE)\
	&& defined(RTV_MSC_INTR_ISTAUS_ACC_CLR_MODE)
		#error " Should selects an interrupt clear mode"
	#endif
#endif


/* Check the interrupt clear mode of ISTAUS register. */
#if defined(RTV_IF_TSIF) ||defined(RTV_IF_SPI_SLAVE)
	#ifndef RTV_MSC_INTR_MEM_ACC_CLR_MODE
		#error "Should define RTV_MSC_INTR_MEM_ACC_CLR_MODE"
	#endif

#else /* SPI, EBI2 */
	#if defined(__KERNEL__) || defined(WINCE)
		#ifndef RTV_MSC_INTR_ISTAUS_ACC_CLR_MODE
			#error "Should define RTV_MSC_INTR_ISTAUS_ACC_CLR_MODE"
		#endif	
	#endif
#endif

#if !defined(RTV_CIF_MODE_ENABLED) && defined( RTV_CIF_HEADER_INSERTED)
	#error "Should defines RTV_CIF_MODE_ENABLED"
#endif


void rtvOEM_ConfigureInterrupt(void);
void rtvOEM_PowerOn(int on);

#ifdef __cplusplus 
} 
#endif 

#endif /* __RAONTV_PORT_H__ */

