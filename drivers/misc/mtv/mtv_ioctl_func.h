/*
 * File name: mtv_ioctl_func.h
 *
 * Description: MTV IO control functions header file.
 *                   Only used in the mtv.c
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
 */

#ifndef __MTV_IOCTL_FUNC_H__
#define __MTV_IOCTL_FUNC_H__


/*==============================================================================
 * Wrapper functions for the accessing of user memory.
 *============================================================================*/ 
#if defined(__KERNEL__) /* Linux kernel */
#define CopyToUser(to, from, size, arg)	copy_to_user(to, from, size)
#define CopyFromUser(to, from, size, arg)	copy_from_user(to, from, size)
#define GetUser(k_ptr, u_ptr, size, arg)	get_user(*(k_ptr), u_ptr)
#define PutUser(k_ptr, u_ptr, size, arg)	put_user(*(k_ptr), u_ptr)
	
#elif defined(WINCE)
#ifndef __user
	#define __user
#endif

struct WINCE_IOCTL_PARAM
{
	PBYTE pbInBuf,
	DWORD dwInBufSize,
	PBYTE pbOutBuf,
	DWORD dwOutBufSize,
};

static INLINE long CopyToUser(void *to, void *from, unsigned int size, unsigned long arg)
{
	if(arg == 0) /* read() system call */
		memcpy(to, from, size);
	else
	{	/* ioctl() system call */
		struct WINCE_IOCTL_PARAM *param = (struct WINCE_IOCTL_PARAM *)arg;
		
		memcpy(param->pbOutBuf, from, size);
	}

	return 0;
}

/* Only for IOControl() */
static INLINE long CopyFromUser(void *to, void *from, unsigned int size, unsigned long arg)
{
	struct WINCE_IOCTL_PARAM *param = (struct WINCE_IOCTL_PARAM *)arg;

	memcpy(to, param->pbInBuf, param->dwInBufSize);
	
	return 0;
}


static INLINE long GetUser(void *k_ptr, void *u_ptr, unsigned int size, unsigned long arg)
{
	struct WINCE_IOCTL_PARAM *param = (struct WINCE_IOCTL_PARAM *)arg;

	switch(size)
	{
	case 1: *(unsigned char *)k_ptr = *(unsigned char *)param->pbInBuf; break;
	case 2: *(unsigned short *)k_ptr = *(unsigned short *)param->pbInBuf; break;
	case 4: *(unsigned int *)k_ptr = *(unsigned int *)param->pbInBuf; break;
	default: break;
	}
	
	return 0;
}

static INLINE long PutUser(void *k_ptr, void *u_ptr, unsigned int size, unsigned long arg)
{
	struct WINCE_IOCTL_PARAM *param = (struct WINCE_IOCTL_PARAM *)arg;

	switch(size)
	{
	case 1: *(unsigned char *)param->pbOutBuf = *(unsigned char *)k_ptr; break;
	case 2: *(unsigned short *)param->pbOutBuf = *(unsigned short *)k_ptr; break;
	case 4: *(unsigned int *)param->pbOutBuf = *(unsigned int *)k_ptr; break;
	default: break;
	}
	
	return 0;
}
#else
	#error "Code not present"
#endif



/* Forward functions. */
static int mtv_power_on(void);
#if defined(RTV_IF_SPI)  || defined(RTV_FIC_INTR_ENABLED)
static void mtv_reset_tsp(void);
#endif


#ifdef DEBUG_MTV_IF_MEMORY
static INLINE void reset_msc_if_debug(void)
{
#ifdef RTV_MSC0_ENABLED
	mtv_cb_ptr->msc0_ts_intr_cnt = 0;
	mtv_cb_ptr->msc0_ovf_intr_cnt = 0;

	#ifdef RTV_CIF_MODE_ENABLED
	mtv_cb_ptr->msc0_cife_cnt = 0;
	#endif
#endif	

#ifdef RTV_MSC1_ENABLED
	mtv_cb_ptr->msc1_ts_intr_cnt = 0;
	mtv_cb_ptr->msc1_ovf_intr_cnt = 0;
#endif

	mtv_cb_ptr->max_remaining_tsp_cnt = 0;
}

static INLINE void show_msc_if_statistics(void)
{
#if defined(RTV_MSC1_ENABLED) && defined(RTV_MSC0_ENABLED)
	#ifndef RTV_CIF_MODE_ENABLED
	DMBMSG("[mtv] MSC1[ovf: %ld/%ld], MSC0[ovf: %ld/%ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc1_ovf_intr_cnt, mtv_cb_ptr->msc1_ts_intr_cnt,
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->max_remaining_tsp_cnt);
	#else
	DMBMSG("[mtv] MSC1[ovf: %ld/%ld], MSC0[ovf: %ld/%ld, cife: %ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc1_ovf_intr_cnt, mtv_cb_ptr->msc1_ts_intr_cnt,
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->msc0_cife_cnt, mtv_cb_ptr->max_remaining_tsp_cnt);
	#endif
	
#elif defined(RTV_MSC1_ENABLED) && !defined(RTV_MSC0_ENABLED)
	DMBMSG("[mtv] MSC1[ovf: %ld/%ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc1_ovf_intr_cnt, mtv_cb_ptr->msc1_ts_intr_cnt,
		mtv_cb_ptr->max_remaining_tsp_cnt);

#elif !defined(RTV_MSC1_ENABLED) && defined(RTV_MSC0_ENABLED)
	#ifndef RTV_CIF_MODE_ENABLED
	DMBMSG("[mtv] MSC0[ovf: %ld/%ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->max_remaining_tsp_cnt);
	#else
	DMBMSG("[mtv] MSC0[ovf: %ld/%ld, cife: %ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->msc0_cife_cnt, mtv_cb_ptr->max_remaining_tsp_cnt);
	#endif
#endif
}

#define RESET_MSC_IF_DEBUG		reset_msc_if_debug()
#define SHOW_MSC_IF_STATISTICS	show_msc_if_statistics()

#else
#define RESET_MSC_IF_DEBUG		((void)0)
#define SHOW_MSC_IF_STATISTICS	((void)0)
#endif /* #ifdef DEBUG_MTV_IF_MEMORY*/

/*============================================================================
 * Test IO control commands(0~10)
 *==========================================================================*/
static unsigned char get_reg_page_value(unsigned int page_idx)
{
	unsigned char page_val = 0x0;
	static const unsigned char mtv_reg_page_addr[] = {
		0x07/*HOST*/, 0x0F/*RF*/, 0x04/*COMM*/, 0x09/*DD*/,
		0x0B/*MSC0*/, 0x0C/*MSC1*/
	};
	
	switch( mtv_cb_ptr->tv_mode )
	{
	case DMB_TV_MODE_TDMB:
	case DMB_TV_MODE_DAB:
	case DMB_TV_MODE_FM:
		switch( page_idx )
		{
			case 6: page_val = 0x06; break; /* OFDM */
			case 7: page_val = 0x09; break; /* FEC */
			case 8: page_val = 0x0A; break; /* FEC */
			default: page_val = mtv_reg_page_addr[page_idx];					
		}
		break;

	case DMB_TV_MODE_1SEG:
		switch( page_idx )
		{
			case 6: page_val = 0x02; break; /* OFDM */
			case 7: page_val = 0x03; break; /* FEC */
			default: page_val = mtv_reg_page_addr[page_idx];					
		}
		break;
	default:
		break;
	}

	return page_val;
}

static int test_register_io(unsigned long arg, unsigned int cmd)
{
	unsigned int page, addr, write_data, read_cnt, i;
	unsigned char value;	
	U8 reg_page_val = 0;
#if defined(RTV_IF_SPI)	
	unsigned char reg_read_buf[MAX_NUM_MTV_REG_READ_BUF+1];
#else
	unsigned char reg_read_buf[MAX_NUM_MTV_REG_READ_BUF];
#endif
	IOCTL_REG_ACCESS_INFO __user *reg_acc_ptr
			= (IOCTL_REG_ACCESS_INFO __user *)arg;
		
	if(mtv_cb_ptr->is_power_on == FALSE)
	{			
		DMBMSG("[mtv] Power Down state!Must Power ON\n");
		return -EFAULT;
	}

	if(GetUser(&page, &reg_acc_ptr->page, sizeof(page), arg))
		return -EFAULT;
	
	if(GetUser(&addr, &reg_acc_ptr->addr, sizeof(addr), arg))
		return -EFAULT;

	reg_page_val = get_reg_page_value(page);				
	RTV_REG_MAP_SEL(reg_page_val); 

	switch (cmd)
	{
	case IOCTL_TEST_REG_SINGLE_READ:
		value = RTV_REG_GET(addr);
		if(PutUser(&value, &reg_acc_ptr->read_data[0], sizeof(value), arg))
			return -EFAULT;
		break;

	case IOCTL_TEST_REG_BURST_READ:
		if(GetUser(&read_cnt, &reg_acc_ptr->read_cnt, sizeof(read_cnt), arg))
			return -EFAULT;
		
	#if defined(RTV_IF_SPI)
		RTV_REG_BURST_GET(addr, reg_read_buf, read_cnt+1);	
		for(i=1; i< (read_cnt+1); i++) /* Except the first byte. (0xFF) */
		{
			if(PutUser(&reg_read_buf[i], &reg_acc_ptr->read_data[i-1], sizeof(char), arg))
				return -EFAULT;
		}
	#else
		RTV_REG_BURST_GET(addr, reg_read_buf, read_cnt);
		for(i=0; i<read_cnt; i++)
		{
			if(PutUser(&reg_read_buf[i], &reg_acc_ptr->read_data[i], sizeof(char), arg))
				return -EFAULT;
		}
	#endif
		break;

	case IOCTL_TEST_REG_WRITE:
		if(GetUser(&write_data, &reg_acc_ptr->write_data, sizeof(write_data), arg))
			return -EFAULT;
		
		RTV_REG_SET(addr, write_data);
		break;
	}

	return 0;
}

static int test_gpio(unsigned long arg, unsigned int cmd)
{
	unsigned int pin, value;
	IOCTL_GPIO_ACCESS_INFO __user *gpio_ptr
			= (IOCTL_GPIO_ACCESS_INFO __user *)arg;

	if(GetUser(&pin, &gpio_ptr->pin, sizeof(pin), arg))
		return -EFAULT;
	
	switch (cmd)
	{
	case IOCTL_TEST_GPIO_SET:
		if(GetUser(&value, &gpio_ptr->value, sizeof(value), arg))
			return -EFAULT;
		
		gpio_set_value(pin, value);
		break;

	case IOCTL_TEST_GPIO_GET:
		value = gpio_get_value(pin);
		if(PutUser(&value, &gpio_ptr->value, sizeof(value), arg))
			return -EFAULT;
	}

	return 0;
}

static void test_power_on_off(unsigned int cmd)
{
	switch (cmd)
	{
	case IOCTL_TEST_MTV_POWER_ON:	
		DMBMSG("[mtv_ioctl] IOCTL_TEST_MTV_POWER_ON\n");	

		if(mtv_cb_ptr->is_power_on == FALSE)
		{
			rtvOEM_PowerOn(1);

			/* Only for test command to confirm. */
			RTV_DELAY_MS(100);

			/* To read the page of RF. */
			RTV_REG_MAP_SEL(HOST_PAGE);
			RTV_REG_SET(0x7D, 0x06);
		
			mtv_cb_ptr->is_power_on = TRUE;
		}
		break;

	case IOCTL_TEST_MTV_POWER_OFF:	
		if(mtv_cb_ptr->is_power_on == TRUE)
		{
			rtvOEM_PowerOn(0);
			mtv_cb_ptr->is_power_on = FALSE;
		}
		break;	
	}
}


#ifdef RTV_DAB_ENABLE
/*============================================================================
 * DAB IO control commands(70 ~ 89)
 *==========================================================================*/
static INLINE int dab_power_on(void)
{
	int ret;
	
	mtv_cb_ptr->tv_mode = DMB_TV_MODE_DAB;

	mtv_cb_ptr->num_opened_subch = 0;
	
	ret = mtv_power_on();

	return ret;
}

/* Used by full scan mode */
static INLINE int dab_scan_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_freq_khz;

	//DMBMSG("[dab_scan_freq] IOCTL_DAB_SCAN_FREQ\n");	

	if(GetUser(&ch_freq_khz, (unsigned int *)arg, sizeof(ch_freq_khz), arg))
		return -EFAULT;

#ifdef RTV_DAB_RECONFIG_ENABLED
	rtvDAB_DisableReconfigInterrupt();
#endif
	
#ifdef RTV_FIC_INTR_ENABLED
	/* Disable FIC interrupt. Do NOT enable Reconfig INT in this time. 
	   This is full scan by user. */
	rtvDAB_CloseFIC(); 

	mtv_reset_tsp(); /* FIC buffer reset. */

	mtv_cb_ptr->freq_khz = ch_freq_khz;
#endif				
	ret = rtvDAB_ScanFrequency(ch_freq_khz);
	if(ret != RTV_SUCCESS)
	{
		mtv_cb_ptr->fic_size = 0;
		
		if(ret == RTV_CHANNEL_NOT_DETECTED) /* Not device error */
			return IOTCL_SCAN_NOT_DETECTED_RET;
		else
			return ret;
	}

	/* Update the size of FIC and enable FIC. */
	mtv_cb_ptr->fic_size = rtvDAB_OpenFIC();

	return 0;
}

/* Used by full scan mode */
static INLINE void dab_scan_stop(unsigned long arg)
{
	//DMBMSG("[dab_scan_stop] IOCTL_DAB_SCAN_STOP\n");
	
	rtvDAB_CloseFIC();
	
	mtv_cb_ptr->fic_size = 0; /* Reset. */

#ifdef RTV_FIC_INTR_ENABLED
	mtv_reset_tsp(); /* Reset FIC buffer. */
#endif

#ifdef RTV_DAB_RECONFIG_ENABLED
	mtv_cb_ptr->reconfig_occurs_flag = FALSE; // ???????
#endif
	//rtvDAB_EnableReconfigInterrupt();
}

static INLINE int dab_read_fic(unsigned long arg) /* FIC polling Mode. */
{
	int ret;
	unsigned int fic_size;
#if defined(RTV_IF_SPI)
	U8 fic_buf[384+1];
#else
	U8 fic_buf[384];
#endif

	if((fic_size = rtvDAB_ReadFIC(fic_buf, mtv_cb_ptr->fic_size)) == 0)
	{
		DMBERR("[mtv] rtvDAB_ReadFIC() error\n");
		return -EFAULT;
	}

#if defined(RTV_IF_SPI)		
	ret = CopyToUser((void __user*)arg, &fic_buf[1], fic_size, arg);
#else
	ret = CopyToUser((void __user*)arg, fic_buf, fic_size, arg);
#endif
	if(ret >= 0)
		return fic_size;
	else
		return ret;
}

/* Used by Reconfig scan mode only. */
static INLINE int dab_open_fic(unsigned long arg)
{
	/* Update the size of FIC and enable FIC. */
	mtv_cb_ptr->fic_size = rtvDAB_OpenFIC();

	return 0;
}


/* Used by Reconfig scan mode only. */
static INLINE int dab_close_fic(unsigned long arg)
{
	rtvDAB_CloseFIC();

#ifdef RTV_FIC_INTR_ENABLED
	mtv_clear_fic_tsp_contents();
#endif

	mtv_cb_ptr->fic_size = 0; /* Reset. */

	return 0;
}


static INLINE int dab_open_subchannel(unsigned long arg)
{
	int ret = 0;
	unsigned int threshold_size;
	IOCTL_DAB_SUB_CH_INFO sub_ch_info;
	
	ret = CopyFromUser(&sub_ch_info, (const void *)arg, sizeof(IOCTL_DAB_SUB_CH_INFO), arg);
	if(ret < 0)
		return ret;

#if 1
	DMBMSG("[mtv] IOCTL_DAB_OPEN_SUBCHANNEL: ch_freq_khz: %d, dab_sub_ch_id:%u, svc_type: %d\n", 
		sub_ch_info.ch_freq_khz, sub_ch_info.subch_id, sub_ch_info.svc_type);
#endif

	switch( sub_ch_info.svc_type )
	{
	case RTV_SERVICE_VIDEO: 
	case RTV_SERVICE_AUDIO: 
		if(sub_ch_info.svc_type == RTV_SERVICE_VIDEO)
			threshold_size = MTV_TS_THRESHOLD_SIZE; 
		else
			threshold_size = MTV_TS_AUDIO_THRESHOLD_SIZE;

		mtv_cb_ptr->msc1_threshold_size = threshold_size;
		break;
		
	case RTV_SERVICE_DATA:   
		threshold_size = MTV_TS_DATA_THRESHOLD_SIZE;
#if (RTV_NUM_DAB_AVD_SERVICE == 1)  /* Single Sub Channel Mode */
		mtv_cb_ptr->msc1_threshold_size = threshold_size;
#else
		mtv_cb_ptr->msc0_threshold_size = threshold_size;
#endif
		break;
		
	default: 
		DMBERR("[mtv] Invaild Open Sub Channel service type: %d\n",
				sub_ch_info.svc_type);
			return -EFAULT;
	}

#ifdef RTV_FIC_INTR_ENABLED
	mtv_cb_ptr->freq_khz = sub_ch_info.ch_freq_khz;
#endif

	ret = rtvDAB_OpenSubChannel(sub_ch_info.ch_freq_khz,
				sub_ch_info.subch_id,
				sub_ch_info.svc_type,
				threshold_size);
	if(ret == RTV_SUCCESS)
		mtv_cb_ptr->num_opened_subch++;

	if(ret == RTV_ALREADY_OPENED_SUB_CHANNEL)
		ret = RTV_SUCCESS;

#ifdef RTV_DAB_RECONFIG_ENABLED
	mtv_cb_ptr->reconfig_occurs_flag = FALSE;

	if(rtvDAB_GetPreviousFrequency() != sub_ch_info.ch_freq_khz)
		rtvDAB_EnableReconfigInterrupt();
#endif

	return ret;
}

static INLINE int dab_close_subchannel(unsigned long arg)
{
	int ret;
	unsigned int subch_id;
#if defined(RTV_IF_SPI)
	BOOL do_reset = FALSE;
#endif

	if(GetUser(&subch_id, (unsigned int *)arg, sizeof(subch_id), arg))
		return -EFAULT;

	DMBMSG("[mtv] Close subch_id(%d)\n", subch_id);	
	
	ret = rtvDAB_CloseSubChannel(subch_id);

	if(mtv_cb_ptr->num_opened_subch != 0)
		mtv_cb_ptr->num_opened_subch--;

#if defined(RTV_IF_SPI) /* MSC read() for SPI only */
	#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	do_reset = TRUE; /* Anyway, reset */
	#else
	if(mtv_cb_ptr->num_opened_subch == 0)
		do_reset = TRUE;
	#endif

	#ifdef RTV_FIC_INTR_ENABLED /* FIC interrupt mode */
	if(do_reset == TRUE)
	{
		if(mtv_cb_ptr->fic_size != 0) /* FIC was opened. */
			do_reset = FALSE;
	}
	#endif

	if(do_reset == TRUE)
		mtv_reset_tsp();
#endif

	return ret;
}

static INLINE void dab_close_all_subchannels(unsigned long arg)
{
#if defined(RTV_IF_SPI)
	BOOL do_reset = TRUE;
#endif
	rtvDAB_CloseAllSubChannels();

#if defined(RTV_IF_SPI)
	#ifdef RTV_FIC_INTR_ENABLED /* FIC interrupt mode */
	if(mtv_cb_ptr->fic_size != 0) /* FIC was opened. */
		do_reset = FALSE;
	#endif

	if(do_reset == TRUE)
		mtv_reset_tsp();
#endif
}


static INLINE int dab_get_lock_status(unsigned long arg)
{
	unsigned int lock_mask;
	
	lock_mask = rtvDAB_GetLockStatus();

	if(PutUser(&lock_mask, (unsigned int *)arg, sizeof(lock_mask), arg))
		return -EFAULT;

	return 0;
}

static INLINE int dab_get_signal_info(unsigned long arg)
{
	int ret;	
	IOCTL_DAB_SIGNAL_INFO sig_info;

	sig_info.lock_mask = rtvDAB_GetLockStatus();
	sig_info.ber = rtvDAB_GetBER(); 	
	sig_info.cnr = rtvDAB_GetCNR(); 
	sig_info.per = rtvDAB_GetPER(); 
	sig_info.rssi = rtvDAB_GetRSSI();
	sig_info.cer = rtvDAB_GetCER(); 
	sig_info.ant_level = rtvDAB_GetAntennaLevel(sig_info.cer);

	ret = CopyToUser((void __user*)arg, &sig_info,
			sizeof(IOCTL_DAB_SIGNAL_INFO), arg);
	
	SHOW_MSC_IF_STATISTICS;

	return ret;
}
#endif /* #ifdef RTV_DAB_ENABLE */


#ifdef RTV_TDMB_ENABLE
/*==============================================================================
 * TDMB IO control commands(30 ~ 49)
 *============================================================================*/ 
static INLINE int tdmb_power_on(unsigned long arg)
{
	int ret;

	mtv_cb_ptr->tv_mode = DMB_TV_MODE_TDMB;

	mtv_cb_ptr->num_opened_subch = 0;

	if(GetUser(&mtv_cb_ptr->country_band_type, (E_RTV_COUNTRY_BAND_TYPE *)arg, sizeof(E_RTV_COUNTRY_BAND_TYPE), arg))
		return -EFAULT;
	
	ret = mtv_power_on();

	return ret;
}

static INLINE int tdmb_scan_freq(unsigned long arg)
{
	int ret;

	unsigned int ch_freq_khz;
		
	if(GetUser(&ch_freq_khz, (unsigned int *)arg, sizeof(ch_freq_khz), arg))
		return -EFAULT;

#ifdef RTV_FIC_INTR_ENABLED /* SPI or I2C. */
	/* Disable FIC interrupt. This is full scan by user. */
	rtvTDMB_CloseFIC(); 

	mtv_reset_tsp(); /* FIC buffer reset. */

	mtv_cb_ptr->freq_khz = ch_freq_khz;
#endif

	ret = rtvTDMB_ScanFrequency(ch_freq_khz);
	if(ret != RTV_SUCCESS)
	{
		mtv_cb_ptr->fic_size = 0;
		
		if(ret == RTV_CHANNEL_NOT_DETECTED) /* Not device error */
			return IOTCL_SCAN_NOT_DETECTED_RET;
		else
			return ret;
	}	

	rtvTDMB_OpenFIC();

	mtv_cb_ptr->fic_size = 384;

	return 0;
}

static INLINE int tdmb_scan_stop(unsigned long arg)
{
	rtvTDMB_CloseFIC();

	mtv_cb_ptr->fic_size = 0;

#ifdef RTV_FIC_INTR_ENABLED /* SPI or I2C. */
	mtv_reset_tsp(); /* Reset FIC buffer. */
#endif

	return 0;
}

static INLINE int tdmb_read_fic(unsigned long arg) /* FIC polling Mode. */
{
	int ret;
	unsigned int fic_size;
#if defined(RTV_IF_SPI)
	U8 fic_buf[384+1];
#else
	U8 fic_buf[384];
#endif

	if((fic_size = rtvTDMB_ReadFIC(fic_buf)) == 0)
	{
		DMBERR("[mtv] rtvTDMB_ReadFIC() error\n");
		return -EFAULT;
	}

#if defined(RTV_IF_SPI)		
	ret = CopyToUser((void __user*)arg, &fic_buf[1], fic_size, arg);
#else
	ret = CopyToUser((void __user*)arg, fic_buf, fic_size, arg);
#endif

	if(ret >= 0)
		return fic_size;
	else
		return ret;
}

static INLINE int tdmb_open_subchannel(unsigned long arg)
{
	int ret = 0;
	unsigned int threshold_size;
	IOCTL_TDMB_SUB_CH_INFO sub_ch_info;
	
	ret = CopyFromUser(&sub_ch_info, (const void *)arg, sizeof(IOCTL_TDMB_SUB_CH_INFO), arg);
	if(ret < 0)
		return ret;
	/*
	DMBMSG("[mtv] tdmb_open_subchannel: ch_freq_khz: %d, subch_id:%u, svc_type: %d\n", 
		sub_ch_info.ch_freq_khz, sub_ch_info.subch_id, sub_ch_info.svc_type);
	*/

	/* Most of TDMB application use set channel instead of open/close method. 
	So, we reset buffer before open sub channel for the case of 1 service app. */
#if defined(RTV_IF_SPI) && (RTV_NUM_DAB_AVD_SERVICE == 1)\
&& !defined(RTV_FIC_INTR_ENABLED)
	rtvTDMB_CloseSubChannel(0/* don't care for 1 AV subch application */);
	mtv_reset_tsp();
#endif

	switch( sub_ch_info.svc_type )
	{
	case RTV_SERVICE_VIDEO:
	case RTV_SERVICE_AUDIO:
		if(sub_ch_info.svc_type == RTV_SERVICE_VIDEO)
			threshold_size = MTV_TS_THRESHOLD_SIZE;
		else
			threshold_size = MTV_TS_AUDIO_THRESHOLD_SIZE;

		mtv_cb_ptr->msc1_threshold_size = threshold_size;
		break;
		
	case RTV_SERVICE_DATA:
		threshold_size = MTV_TS_DATA_THRESHOLD_SIZE;
		
#if (RTV_NUM_DAB_AVD_SERVICE == 1)  /* Single Sub Channel Mode */
		mtv_cb_ptr->msc1_threshold_size = threshold_size;
#else
		mtv_cb_ptr->msc0_threshold_size = threshold_size;
#endif
		break;

	default: 
		DMBERR("[mtv] Invaild Open Sub Channel service type: %d\n",
					sub_ch_info.svc_type);
		return -EFAULT;
	}

	ret = rtvTDMB_OpenSubChannel(sub_ch_info.ch_freq_khz,
				sub_ch_info.subch_id,
				sub_ch_info.svc_type,
				threshold_size);
	if(ret == RTV_SUCCESS)
		mtv_cb_ptr->num_opened_subch++;

	if(ret == RTV_ALREADY_OPENED_SUB_CHANNEL)
		ret = RTV_SUCCESS;

	return ret;
}

static INLINE int tdmb_close_subchannel(unsigned long arg)
{
	int ret;
	unsigned int subch_id;
#if defined(RTV_IF_SPI)
	BOOL do_reset = FALSE;
#endif

	if(GetUser(&subch_id, (unsigned int *)arg, sizeof(subch_id), arg))
		return -EFAULT;

	DMBMSG("[mtv] Close tdmb_sub_ch_id(%d)\n", subch_id);

	ret = rtvTDMB_CloseSubChannel(subch_id);

	if(mtv_cb_ptr->num_opened_subch != 0)
		mtv_cb_ptr->num_opened_subch--;

#if defined(RTV_IF_SPI) /* MSC read() for SPI only */
	#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	do_reset = TRUE; /* Anyway, reset */
	#else
	if(mtv_cb_ptr->num_opened_subch == 0)
		do_reset = TRUE;
	#endif

	#ifdef RTV_FIC_INTR_ENABLED /* FIC interrupt mode */
	if(do_reset == TRUE)
	{
		if(mtv_cb_ptr->fic_size != 0) /* FIC was opened. */
			do_reset = FALSE;
	}
	#endif

	if(do_reset == TRUE)
		mtv_reset_tsp();
#endif

	return ret;
}

static INLINE void tdmb_close_all_subchannels(unsigned long arg)
{
#if defined(RTV_IF_SPI)
	BOOL do_reset = TRUE;
#endif

	rtvTDMB_CloseAllSubChannels();

#if defined(RTV_IF_SPI)
	#ifdef RTV_FIC_INTR_ENABLED /* FIC interrupt mode */
	if(mtv_cb_ptr->fic_size != 0) /* FIC was opened. */
		do_reset = FALSE;
	#endif

	if(do_reset == TRUE)
		mtv_reset_tsp();
#endif
}


static INLINE int tdmb_get_lock_status(unsigned long arg)
{
	unsigned int lock_mask;
	
	lock_mask = rtvTDMB_GetLockStatus();

	if(PutUser(&lock_mask, (unsigned int *)arg, sizeof(lock_mask), arg))
		return -EFAULT;

	return 0;
}

static INLINE int tdmb_get_signal_info(unsigned long arg)
{
	int ret;	
	IOCTL_TDMB_SIGNAL_INFO sig_info;

	sig_info.lock_mask = rtvTDMB_GetLockStatus();	
	sig_info.ber = rtvTDMB_GetBER(); 	 
	sig_info.cnr = rtvTDMB_GetCNR(); 
	sig_info.per = rtvTDMB_GetPER(); 
	sig_info.rssi = rtvTDMB_GetRSSI();
	sig_info.cer = rtvTDMB_GetCER();
	sig_info.ant_level = rtvTDMB_GetAntennaLevel(sig_info.cer);

	ret = CopyToUser((void __user*)arg, &sig_info,
			sizeof(IOCTL_TDMB_SIGNAL_INFO), arg);

	SHOW_MSC_IF_STATISTICS;
	
	return ret;
}
#endif /* #ifdef RTV_TDMB_ENABLE */


#ifdef RTV_ISDBT_ENABLE
/*============================================================================
* ISDB-T IO control commands(10~29)
*===========================================================================*/
static INLINE int isdbt_power_on(unsigned long arg)
{
	int ret;

	mtv_cb_ptr->tv_mode = DMB_TV_MODE_1SEG;

	if(GetUser(&mtv_cb_ptr->country_band_type, (E_RTV_COUNTRY_BAND_TYPE *)arg, sizeof(E_RTV_COUNTRY_BAND_TYPE), arg))
		return -EFAULT;
	
	ret = mtv_power_on();

	return ret;
}

static INLINE int isdbt_scan_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_num;
		
	if(GetUser(&ch_num, (unsigned int *)arg, sizeof(ch_num), arg))
		return -EFAULT;

	ret = rtvISDBT_ScanFrequency(ch_num);
	if(ret != RTV_SUCCESS)
	{
		if(ret == RTV_CHANNEL_NOT_DETECTED) /* Not device error */
			return IOTCL_SCAN_NOT_DETECTED_RET;
		else
			return ret;
	}

	return 0;
}

static INLINE int isdbt_set_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_num;
		
	if(GetUser(&ch_num, (unsigned int *)arg, sizeof(ch_num), arg))
		return -EFAULT;

	ret = rtvISDBT_SetFrequency(ch_num);

	return ret;
}

static INLINE void isdbt_start_ts(void)
{
	RESET_MSC_IF_DEBUG;

	rtvISDBT_EnableStreamOut(); 
}

static INLINE void isdbt_stop_ts(void)
{
	rtvISDBT_DisableStreamOut();

#if defined(RTV_IF_SPI)	
	mtv_reset_tsp();
#endif
}

static INLINE int isdbt_get_lock_status(unsigned long arg)
{
	unsigned int lock_mask;
	
	lock_mask = rtvISDBT_GetLockStatus();

	if(PutUser(&lock_mask, (unsigned int *)arg, sizeof(lock_mask), arg))
		return -EFAULT;

	return 0;
}

static INLINE int isdbt_get_tmcc(unsigned long arg)
{
	int ret;
	RTV_ISDBT_TMCC_INFO tmcc_info;

	rtvISDBT_GetTMCC(&tmcc_info);

	ret = CopyToUser((void __user*)arg, &tmcc_info,
			sizeof(RTV_ISDBT_TMCC_INFO), arg);

	return ret;
}

static INLINE int isdbt_get_signal_info(unsigned long arg)
{
	int ret;	
	IOCTL_ISDBT_SIGNAL_INFO sig_info;

	sig_info.lock_mask = rtvISDBT_GetLockStatus();
	sig_info.ber = rtvISDBT_GetBER(); 
	sig_info.cnr = rtvISDBT_GetCNR();
	sig_info.ant_level = rtvISDBT_GetAntennaLevel(sig_info.cnr);
	sig_info.per = rtvISDBT_GetPER(); 
//	sig_info.rssi = rtvISDBT_GetRSSI(); 

	ret = CopyToUser((void __user*)arg, &sig_info,
			sizeof(IOCTL_ISDBT_SIGNAL_INFO), arg);

	SHOW_MSC_IF_STATISTICS;
		
	return ret;
}
#endif /* #ifdef RTV_ISDBT_ENABLE */


#ifdef RTV_FM_ENABLE
/*============================================================================
* FM IO control commands(50 ~ 69)
*===========================================================================*/
static INLINE int fm_power_on(unsigned long arg)
{
	int ret;

	mtv_cb_ptr->tv_mode = DMB_TV_MODE_FM;

	if(GetUser(&mtv_cb_ptr->adc_clk_type, (E_RTV_ADC_CLK_FREQ_TYPE *)arg, sizeof(E_RTV_ADC_CLK_FREQ_TYPE), arg))
		return -EFAULT;
	
	ret = mtv_power_on();

	return ret;
}

static INLINE int fm_scan_freq(unsigned long arg)
{
	unsigned int start_freq, end_freq, num_ch_buf;
	int num_detected_ch;
	IOCTL_FM_SCAN_INFO __user *scan_info_ptr
				= (IOCTL_FM_SCAN_INFO __user *)arg;
	
	if(GetUser(&start_freq, &scan_info_ptr->start_freq, sizeof(start_freq), arg))
		return -EFAULT;
	
	if(GetUser(&end_freq, &scan_info_ptr->end_freq, sizeof(end_freq), arg))
		return -EFAULT;
	
	if(GetUser(&num_ch_buf, &scan_info_ptr->num_ch_buf, sizeof(num_ch_buf), arg))
		return -EFAULT;
	
	DMBMSG("[mtv] IOCTL_FM_SCAN_FREQ: start_freq(%d) ~ end_freq(%d) \n", start_freq, end_freq);	
	
	num_detected_ch = rtvFM_ScanFrequency(scan_info_ptr->ch_buf, num_ch_buf, start_freq, end_freq);
	if(num_detected_ch < 0)
		return num_detected_ch;

	if(PutUser(&num_detected_ch, &scan_info_ptr->num_detected_ch, sizeof(num_detected_ch), arg))
		return -EFAULT;
	
	return 0;
}

static INLINE int fm_search_freq(unsigned long arg)
{
	int ret;
	unsigned int start_freq;
	unsigned int end_freq;
	int detected_freq_khz;
	IOCTL_FM_SRCH_INFO __user *srch_info_ptr
			= (IOCTL_FM_SRCH_INFO __user *)arg;

	if(GetUser(&start_freq, &srch_info_ptr->start_freq, sizeof(start_freq), arg))
		return -EFAULT;
	
	if(GetUser(&end_freq, &srch_info_ptr->end_freq, sizeof(end_freq), arg))
		return -EFAULT;

	ret = rtvFM_SearchFrequency(&detected_freq_khz, start_freq, end_freq);
	if(ret < 0)
		return ret;

	if(PutUser(&detected_freq_khz, &srch_info_ptr->detected_freq_khz, sizeof(detected_freq_khz), arg))
		return -EFAULT;

	return 0;
}

static INLINE int fm_set_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_freq_khz;
		
	if(GetUser(&ch_freq_khz, (unsigned int *)arg, sizeof(ch_freq_khz), arg))
		return -EFAULT;

	ret = rtvFM_SetFrequency(ch_freq_khz);

	return ret;
}

static INLINE void fm_start_ts(void)
{
	RESET_MSC_IF_DEBUG;

	rtvFM_EnableStreamOut(); 
}

static INLINE void fm_stop_ts(void)
{
	rtvFM_DisableStreamOut();

#if defined(RTV_IF_SPI)	
	mtv_reset_tsp();
#endif
}

static INLINE int fm_get_lock_status(unsigned long arg)
{
	int ret;
	IOCTL_FM_LOCK_STATUS_INFO lock_status;
	
	rtvFM_GetLockStatus(&lock_status.val, &lock_status.cnt);

	SHOW_MSC_IF_STATISTICS;

	ret = CopyToUser((void __user*)arg, &lock_status,
			sizeof(IOCTL_FM_LOCK_STATUS_INFO), arg);

	return ret;
}

static INLINE int fm_get_rssi(unsigned long arg)
{
	int rssi;

	rssi = rtvFM_GetRSSI();
	
	if(PutUser(&rssi, (int *)arg, sizeof(rssi), arg))
		return -EFAULT;

	return 0;
}
#endif /* #ifdef RTV_FM_ENABLE */


#endif /* __MTV_IOCTL_FUNC_H__ */

