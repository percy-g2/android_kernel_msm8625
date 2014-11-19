/*
 * File name: mtv_isr.c
 *
 * Description: MTV ISR driver.
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

#include "raontv_internal.h"
#include "mtv.h"


#ifndef RTV_DAB_RECONFIG_ENABLED
	/* RTV_REG_GET(reg) */
	#define NUM_ISTATUS_BUF	1
	#define MSC_FIC_INTR_BUF_IDX	0	
#else

	#if defined(RTV_IF_SPI)
		#define NUM_ISTATUS_BUF		3
		#define MSC_FIC_INTR_BUF_IDX		1 /* Index 0: 0xFF */
		#define RECONFIG_INTR_BUF_IDX		2
	#else
		/* I2C */
		#define NUM_ISTATUS_BUF		2
		#define MSC_FIC_INTR_BUF_IDX		0
		#define RECONFIG_INTR_BUF_IDX		1
	#endif
#endif


#ifdef RTV_DAB_RECONFIG_ENABLED
static inline void reconfig_fec(void)
{
	U8 Reconfig_CIF_Cnt1=0, cur_CIF_Cnt1=0;
	U8 sync_mode1 = 0, reconfig_stat1 =0;
	

	RTV_REG_MAP_SEL(COMM_PAGE);
	cur_CIF_Cnt1 = RTV_REG_GET(0x8A);
	Reconfig_CIF_Cnt1 = RTV_REG_GET(0x8B);
	
	if(Reconfig_CIF_Cnt1>=10)
	{
		if((cur_CIF_Cnt1 > (Reconfig_CIF_Cnt1-10)) && (cur_CIF_Cnt1 < Reconfig_CIF_Cnt1))
		{
			RTV_REG_MAP_SEL(FEC_PAGE);
			RTV_REG_SET(0xE9, 0x0A);
			mtv_cb_ptr->reconfig_occurs_flag = TRUE;
		}
	}
	else
	{
		if((cur_CIF_Cnt1 > (239 +Reconfig_CIF_Cnt1))||(cur_CIF_Cnt1 < Reconfig_CIF_Cnt1))
		{
			RTV_REG_MAP_SEL(FEC_PAGE);
			RTV_REG_SET(0xE9, 0x0A);
			mtv_cb_ptr->reconfig_occurs_flag = TRUE;
		}
	}
	
	if(mtv_cb_ptr->reconfig_occurs_flag == TRUE)
	{
		if(cur_CIF_Cnt1 > Reconfig_CIF_Cnt1)
		{
			RTV_REG_MAP_SEL(FEC_PAGE);
			sync_mode1 = RTV_REG_GET(0xE9);
			RTV_REG_SET(0xE9, (sync_mode1 & 0xF7));
			mtv_cb_ptr->reconfig_occurs_flag = FALSE;
		}
	}

	
	else if(mtv_cb_ptr->reconfig_occurs_flag ==TRUE)
	{
		RTV_REG_MAP_SEL(FEC_PAGE);
		reconfig_stat1 = RTV_REG_GET(0xFB);
		if((reconfig_stat1 & 0x04)==0)
		{
			RTV_REG_MAP_SEL(FEC_PAGE);
			sync_mode1 = RTV_REG_GET(0xE9);
			RTV_REG_SET(0xE9, (sync_mode1 & 0xF7));
			mtv_cb_ptr->reconfig_occurs_flag = FALSE;
		}
		else
		{
			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(OFDM_E_CON, 0x49); 
			RTV_REG_SET(OFDM_E_CON, 0xC9); // FEC Soft reset
		}
	}	
}

static inline void proc_reconfig(U8 istatus)
{
#if 0 // Test for SIGIO
	if((mtv_cb_ptr->msc1_ts_intr_cnt % 20) == 0)
		kill_fasync(&mtv_cb_ptr->sigio_async_queue, SIGIO, POLL_IN);
#endif

	if( istatus & RE_CONFIG_E_INT )
	//if(mtv_cb_ptr->msc1_ts_intr_cnt > 300) // to test
	{	
		//DMBMSG("RE_CONFIG_E_INT occured!\n");

		reconfig_fec();

		/* Enable FIC interrupt. */
		//rtv_OpenFIC();
		
		/* Singaling to application. */
		kill_fasync(&mtv_cb_ptr->sigio_async_queue, SIGIO, POLL_IN);

		RTV_REG_MAP_SEL(DD_PAGE);
		RTV_REG_SET(INT_E_UCLRH, 0x04);
	}
}
#endif /* #ifdef RTV_DAB_RECONFIG_ENABLED */

#ifdef RTV_FIC_INTR_ENABLED
static inline void read_fic(MTV_TS_PKT_INFO *tsp)
{
#if defined(RTV_TDMB_ENABLE)
	unsigned int size = 384;
#elif defined(RTV_DAB_ENABLE)
	unsigned int size = mtv_cb_ptr->fic_size;
	if(size == 0)
	{
		DMBERR("[mtv] FIC: Invalid FIC SIZE.\n");
		tsp->fic_size = 0;
		return;
	}
#endif

	RTV_REG_MAP_SEL(FIC_PAGE);

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	RTV_REG_BURST_GET(0x10, tsp->fic_buf, size/2);
	RTV_REG_BURST_GET(0x10, tsp->fic_buf+size/2, size/2);
	
#elif defined(RTV_IF_SPI)
	RTV_REG_BURST_GET(0x10, tsp->fic_buf, size+1);
#endif

	tsp->fic_size = size;
}

 static inline MTV_TS_PKT_INFO *proc_fic(MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if( istatus & FIC_E_INT )
	{
		/*DMBMSG("FIC_E_INT occured!\n");*/

		/* Allocate a TS packet from TSP pool 
		if MSC1 and MSC0 interrupts are not occured. */
		if(tsp == NULL)
			tsp = mtv_alloc_tsp();
		
		if(tsp != NULL)
			read_fic(tsp);
		else
			DMBERR("[mtv] FIC: No more TSP buffer\n");
		
		RTV_REG_MAP_SEL(DD_PAGE);
		RTV_REG_SET(INT_E_UCLRL, 0x01);
	}

	return tsp;
}
#endif /* #ifdef RTV_FIC_INTR_ENABLED */


#ifdef RTV_MSC0_ENABLED
/* Only for DAB multi DATA sub channel. */
static inline unsigned int verify_cif_msc0(const unsigned char *buf, unsigned int size)
{     
	unsigned int ch_length, subch_size, normal_data_size = 0;
	const unsigned char *cif_header_ptr = buf;

	if(size == 0)
		return 0;

	do
	{
		ch_length = (cif_header_ptr[2]<<8) | cif_header_ptr[3];
		subch_size = ch_length - 4;

		if((ch_length < 4) || ((subch_size > 3*1024)))
			break;

		normal_data_size += ch_length;		
		cif_header_ptr += ch_length;                   
		size         -= ch_length;
	} while(size > 0);

	return normal_data_size;
}


#define FORCE_DELAY_MS		30
static inline void read_cif_msc0(MTV_TS_PKT_INFO *tsp)
{
	unsigned int size, ret_size;
	U8 msc_tsize[2+1];
	
	RTV_REG_MAP_SEL(DD_PAGE);
	RTV_REG_BURST_GET(MSC0_E_TSIZE_L, msc_tsize, 2+1);
	size = (msc_tsize[1] << 8) | msc_tsize[2];


	RTV_REG_MAP_SEL(MSC0_PAGE);
	RTV_REG_BURST_GET(0x10, tsp->msc0_buf, size+1);

	ret_size = verify_cif_msc0(&tsp->msc0_buf[1], size);
	if(ret_size != size)
	{
		RTV_REG_MAP_SEL(DD_PAGE);
		rtv_ClearAndSetupMemory_MSC0();

		DMBERR("[read_cif_msc0] size: %u, ret_size: %d\n\n", size, ret_size);
		
#if defined(DEBUG_MTV_IF_MEMORY) && defined(RTV_CIF_MODE_ENABLED)
		mtv_cb_ptr->msc0_cife_cnt++;
#endif
	}
	
	tsp->msc0_size = ret_size;
}

/* TDMB/DAB threshold or FM RDS multi service. */
static inline void read_threshold_msc0(MTV_TS_PKT_INFO *tsp)
{
	unsigned int size = mtv_cb_ptr->msc0_threshold_size;

	RTV_REG_MAP_SEL(MSC0_PAGE);
	RTV_REG_BURST_GET(0x10, tsp->msc0_buf, size+1); 
	tsp->msc0_size = size;
}

static inline void  read_msc0(MTV_TS_PKT_INFO *tsp)
{
#if defined(RTV_TDMBorDAB_ONLY_ENABLED)
	#ifdef RTV_CIF_MODE_ENABLED
	read_cif_msc0(tsp);
	#else
	read_threshold_msc0(tsp);
	#endif
	
#else
	/* TDMB/DAB/FM. */
	switch (mtv_cb_ptr->tv_mode)
	{
	#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
	case DMB_TV_MODE_TDMB:
	case DMB_TV_MODE_DAB:
		#ifdef RTV_CIF_MODE_ENABLED
		read_cif_msc0(tsp);
		#else
		read_threshold_msc0(tsp);
		#endif
		break;
	#endif

	#ifdef RTV_FM_ENABLE
	case DMB_TV_MODE_FM:
		read_threshold_msc0(tsp);
		break;	
	#endif

	default: /* Do nothing */
		break;
	}
#endif /* #ifdef RTV_TDMBorDAB_ONLY_ENABLED */


#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->msc0_ts_intr_cnt++;
#endif

#if 0
	DMBMSG("[MSC0: %u] [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n",
		tsp->msc0_size,
		tsp->msc0_buf[1], tsp->msc0_buf[2],
		tsp->msc0_buf[3],tsp->msc0_buf[4]);	
#endif
}

/*===================================================================
* Processing MSC0 interrupt.
* TDMB/DAB: Max 4 DATA data
* FM: 1 RDS data. defined(RTV_FM_RDS_ENABLE). NOT implemeted.
*==================================================================*/
static INLINE MTV_TS_PKT_INFO *proc_msc0(MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if( istatus & MSC0_INTR_BITS )
	{
		if( istatus & (MSC0_E_UNDER_FLOW|MSC0_E_OVER_FLOW) )
		{
			RTV_REG_MAP_SEL(DD_PAGE);						
			rtv_ClearAndSetupMemory_MSC0();
			RTV_REG_SET(INT_E_UCLRL, 0x02);

			DMBMSG("[mtv] MSC0 OF/UF: 0x%02X\n", istatus);

		#ifdef DEBUG_MTV_IF_MEMORY
			mtv_cb_ptr->msc0_ovf_intr_cnt++;
		#endif	
		}
		else
		{
			/* Allocate a TS packet from TSP pool if MSC1 not occured interrupt. */
			if(tsp == NULL)
				tsp = mtv_alloc_tsp();
			
			if(tsp != NULL)
				read_msc0(tsp);
			else
				DMBERR("[mtv] MSC0: No more TSP buffer.\n");
	
			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL, 0x02); /* MSC0 Interrupt clear. */
		}
	}

	return tsp;
}
#endif /* #ifdef RTV_MSC0_ENABLED */  


#ifdef RTV_MSC1_ENABLED
static inline void  read_msc1(MTV_TS_PKT_INFO *tsp)
{
	unsigned int size = mtv_cb_ptr->msc1_threshold_size;
	
	RTV_REG_MAP_SEL(MSC1_PAGE);					
	RTV_REG_BURST_GET(0x10, tsp->msc1_buf, size+1/*0xFF*/); 
	tsp->msc1_size = size; 

#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->msc1_ts_intr_cnt++;
#endif

#if 0
	DMBMSG("[MSC1: %u] [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n",
		tsp->msc1_size, 
		tsp->msc1_buf[1], tsp->msc1_buf[2],
		tsp->msc1_buf[3],tsp->msc1_buf[4]);
#endif	
}

/*===================================================================
* Processing MSC1 interrupt. The first processing in ISR.
* TDMB/DAB: 1 VIDEO or 1 AUDIO or 1 DATA.
* 1 seg: 1 VIDEO data
* FM: 1 PCM data
*==================================================================*/
static INLINE MTV_TS_PKT_INFO *proc_msc1(MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if( istatus & MSC1_INTR_BITS )
	{
		if( istatus & (MSC1_E_UNDER_FLOW|MSC1_E_OVER_FLOW) )
		{
			rtv_ClearAndSetupMemory_MSC1(mtv_cb_ptr->tv_mode);
			/* Clear MSC1 Interrupt. */
			RTV_REG_SET(INT_E_UCLRL, 0x04);
		
			DMBMSG("[mtv] MSC1 OF/UF: 0x%02X\n", istatus);
		
#ifdef DEBUG_MTV_IF_MEMORY
			mtv_cb_ptr->msc1_ovf_intr_cnt++;
#endif
		}
		else
		{
			/* Allocate a TS packet from TSP pool. */
			tsp = mtv_alloc_tsp();
			if(tsp != NULL)
				read_msc1(tsp);
			else
				DMBERR("[mtv] MSC1: No more TSP buffer.\n");

			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL, 0x04);
		}
	}

	return tsp;
}
#endif /* #ifdef RTV_MSC1_ENABLED */


#if defined(RTV_IF_SPI) ||defined(RTV_FIC_INTR_ENABLED) || defined(RTV_DAB_RECONFIG_ENABLED)
static void mtv_isr_handler(void)
{	
	MTV_TS_PKT_INFO *tsp = NULL; /* reset */
	U8 intr_status[NUM_ISTATUS_BUF];
		
	RTV_GUARD_LOCK;

	/* Read the register of interrupt status. */
	RTV_REG_MAP_SEL(DD_PAGE);
#ifndef RTV_DAB_RECONFIG_ENABLED
	intr_status[0] = RTV_REG_GET(INT_E_STATL);
	//DMBMSG("[mtv isr] 0x%02X\n", intr_status[0]);
#else
	RTV_REG_BURST_GET(INT_E_STATL, intr_status, NUM_ISTATUS_BUF);
	//DMBMSG("[mtv isr] 0x%02X\n", intr_status[MSC_FIC_INTR_BUF_IDX]);
#endif
	
#ifdef RTV_MSC1_ENABLED
	/* TDMB/DAB: 1 VIDEO or 1 AUDIO or 1 DATA. 1 seg: 1 VIDEO. FM: PCM. */
	tsp = proc_msc1(tsp, intr_status[MSC_FIC_INTR_BUF_IDX]);
#endif

#ifdef RTV_MSC0_ENABLED
	/* TDMB/DAB: Max 4 DATA data. FM: 1 RDS data( NOT implemeted). */
	tsp = proc_msc0(tsp, intr_status[MSC_FIC_INTR_BUF_IDX]); 
#endif

#ifdef RTV_FIC_INTR_ENABLED
	/* Processing FIC interrupt. */
	tsp = proc_fic(tsp, intr_status[MSC_FIC_INTR_BUF_IDX]);
#endif

#ifdef RTV_DAB_RECONFIG_ENABLED
	/* Processing FEC RE_CONFIG interrupt. */
	proc_reconfig(intr_status[RECONFIG_INTR_BUF_IDX]);
#endif

	RTV_GUARD_FREE;

	/* Enqueue a ts packet into ts data queue if a packet was exist. */
	if(tsp != NULL)		
		mtv_put_tsp(tsp);	
}


int mtv_isr_thread(void *data)
{
	DMBMSG("[mtv] ISR thread Start...\n");

	set_user_nice(current, -1);
	
	while(!kthread_should_stop()) 
	{
		wait_event_interruptible(mtv_cb_ptr->isr_wq,
				kthread_should_stop() || mtv_cb_ptr->isr_cnt);

		if(kthread_should_stop())
			break;

		if(mtv_cb_ptr->is_power_on == TRUE)
		{
			mtv_isr_handler();
			mtv_cb_ptr->isr_cnt--;
		}
	}

	DMBMSG("[mtv] ISR thread Exit.\n");

	return 0;
}


irqreturn_t mtv_isr(int irq, void *param)
{
	if(mtv_cb_ptr->is_power_on == TRUE)
	{
		mtv_cb_ptr->isr_cnt++;

		wake_up(&mtv_cb_ptr->isr_wq);
	}

	return IRQ_HANDLED;
}
#endif /* #if defined(RTV_IF_SPI) ||defined(RTV_FIC_INTR_ENABLED) || defined(RTV_DAB_RECONFIG_ENABLED) */

