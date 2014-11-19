#ifndef __MTV_H__
#define __MTV_H__

#ifdef __cplusplus 
extern "C"{
#endif

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/stat.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>  
#include <linux/list.h> 
#include <linux/freezer.h>
#include <linux/completion.h>
/*#include <linux/smp_lock.h>*/
#include <linux/jiffies.h>

#include "raontv.h" /* Place before mtv_ioctl.h ! */
#include "mtv_ioctl.h"


#define DMB_DEBUG


#ifdef RTV_IF_SPI
	#define DEBUG_MTV_IF_MEMORY
#endif

#define DEBUG_SCAN_TIME


#define DMBERR(args...)		do { printk(KERN_ERR  args); } while (0)
#ifdef DMB_DEBUG
	#define DMBMSG(args...)	do { printk(KERN_INFO args); } while (0)
#else 
	#define DMBMSG(x...)  /* null */
#endif 


#define MAX_NUM_TS_PKT_BUF 	40


#define MTV_TS_THRESHOLD_SIZE			(10*188) /* Video */
#define MTV_TS_AUDIO_THRESHOLD_SIZE  		(384*4) /* Audio */
#define MTV_TS_DATA_THRESHOLD_SIZE  		(96*4) /* Data */

typedef enum
{	
	DMB_TV_MODE_TDMB   = 0,
	DMB_TV_MODE_DAB     = 1,
	DMB_TV_MODE_1SEG   = 3,
	DMB_TV_MODE_FM       = 4
} E_DMB_TV_MODE_TYPE;


#if defined(RTV_IF_SPI)
	#define MTV_MAX_FIC_BUF_SIZE	(384 + 1)
#else
	#define MTV_MAX_FIC_BUF_SIZE	384
#endif

typedef struct
{
	struct list_head link; /* to queuing */

#ifdef RTV_MSC1_ENABLED
	UINT msc1_size;
	U8   msc1_buf[MTV_TS_THRESHOLD_SIZE+1]; /* Max Video buffering size. */
#endif

#ifdef RTV_MSC0_ENABLED
	/* Used in the case of TDMB/DAB(Multi subch) and FM(RDS) */
	UINT msc0_size; /* msc0 ts size. */
	U8   msc0_buf[3*1024];
#endif

#ifdef RTV_FIC_INTR_ENABLED
	/* FIC interrupt Mode */
	UINT fic_size;
	U8 fic_buf[MTV_MAX_FIC_BUF_SIZE];
#endif
} MTV_TS_PKT_INFO; 


/* Control Block */
struct mtv_cb
{
	E_DMB_TV_MODE_TYPE tv_mode;
	E_RTV_COUNTRY_BAND_TYPE country_band_type;
	BOOL is_power_on;

	struct device *dev;

#ifdef RTV_FIC_INTR_ENABLED
	unsigned int freq_khz;
#endif

#if defined(RTV_IF_SPI) || defined(RTV_FIC_INTR_ENABLED)
	struct task_struct *isr_thread_cb;
	wait_queue_head_t isr_wq;
#endif

	unsigned int isr_cnt;

#ifdef DEBUG_MTV_IF_MEMORY
	#ifdef RTV_MSC0_ENABLED
	unsigned long msc0_ts_intr_cnt; /* up to DM get */
	unsigned long msc0_ovf_intr_cnt; /* up to DM get */

	#ifdef RTV_CIF_MODE_ENABLED
	unsigned long msc0_cife_cnt; /* up to DM get */
	#endif
	#endif /* RTV_MSC0_ENABLED */

	#ifdef RTV_MSC1_ENABLED
	unsigned long msc1_ts_intr_cnt; /* up to DM get */
	unsigned long msc1_ovf_intr_cnt; /* up to DM get */
	#endif
	
	unsigned int max_alloc_tsp_pool_cnt;
	unsigned int max_remaining_tsp_cnt;
#endif

#ifdef RTV_FM_ENABLE
	E_RTV_ADC_CLK_FREQ_TYPE adc_clk_type;
#endif

#if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE)
	/* Number of opened sub chaneel. To user reset tsp pool. */
	unsigned int num_opened_subch;

	UINT fic_size;
#endif
	unsigned int msc1_threshold_size;
	unsigned int msc0_threshold_size;

#ifdef RTV_DAB_RECONFIG_ENABLED
	bool reconfig_occurs_flag;
	struct fasync_struct *sigio_async_queue;
#endif

	MTV_TS_PKT_INFO *prev_tsp; /* previous tsp */
	unsigned int prev_org_tsp_size;

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	struct i2c_client *i2c_client_ptr;
	struct i2c_adapter *i2c_adapter_ptr;
#elif defined(RTV_IF_SPI)
	struct spi_device *spi_ptr;	
#endif

#ifdef DEBUG_SCAN_TIME
	u64 scan_start_time_jiffies;
	u64 scan_end_time_jiffies;
	unsigned int total_scan_time_ms;

	unsigned int min_scan_rf_lock_time_ms;
	unsigned int max_scan_rf_lock_time_ms;
#endif
};


typedef struct
{
	struct list_head head;
	unsigned int cnt; /* queue count */
	unsigned int total_bytes;
	spinlock_t lock;
} MTV_TSP_QUEUE_INFO;


extern  struct mtv_cb *mtv_cb_ptr;
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	extern struct i2c_driver mtv_i2c_driver;
#elif defined(RTV_IF_SPI)
	extern struct spi_driver mtv_spi_driver;
#endif


void mtv_clear_fic_tsp_contents(void);
MTV_TS_PKT_INFO *mtv_get_tsp(void);
void mtv_put_tsp(MTV_TS_PKT_INFO *pkt);
void mtv_free_tsp(MTV_TS_PKT_INFO *pkt);
MTV_TS_PKT_INFO *mtv_alloc_tsp(void);

#ifdef __cplusplus 
} 
#endif 

#endif /* __MTV_H__*/

