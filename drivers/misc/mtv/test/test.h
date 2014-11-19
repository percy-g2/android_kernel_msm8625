
/* Example for applcation */

#ifndef __TEST_H__
#define __TEST_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <linux/types.h>
#if defined(__ANDROID__) || defined(ANDROID)
//#include <sys/fcntl.h>
#include <fcntl.h>
#include <linux/signal.h> /* Android */
#else
#include <fcntl.h> 
#include <signal.h> /* Linux */
#endif

#include "raontv.h"
#include "mtv_ioctl.h"

#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
#include "raontv_ficdec.h"
#endif
/*=============================================================================
 * Configuration fo File dump
 *============================================================================*/ 
#define TS_DUMP_DIR "/data/raontech"

/* Debug definition */
//#define _DEBUG_PERIODIC_SIG_INFO /* Show the signal information by DM timer. */
//#define _DEBUG_PERIODIC_TSP_STAT /* Show the TSP statistics by DM timer. */
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	/* Select TSIF AP if TSIF was enabled. */
	//#define TSIF_SAMSUNG_AP
	#define TSIF_QUALCOMM_AP

	#if defined(TSIF_SAMSUNG_AP) && defined(TSIF_QUALCOMM_AP)
		#error "Must select the one of TSIF AP"
	#endif

	#if !defined(TSIF_SAMSUNG_AP) && !defined(TSIF_QUALCOMM_AP)
		#error "Must select the one"
	#endif

	#if defined(TSIF_SAMSUNG_AP)
		#define TSIF_DEV_NAME	"s3c-tsi"
	#elif defined(TSIF_QUALCOMM_AP)
		#define TSIF_DEV_NAME	"tsif0"
	#else
		#error "Code not present"
	#endif
#endif


#define _MTV_DEBUG_MSG
#ifdef _MTV_DEBUG_MSG
	#define DMSG0(fmt)					printf(fmt)
	#define DMSG1(fmt, arg1)					printf(fmt, arg1)
	#define DMSG2(fmt, arg1, arg2)				printf(fmt, arg1, arg2)
	#define DMSG3(fmt, arg1, arg2, arg3)			printf(fmt, arg1, arg2, arg3)
	#define DMSG4(fmt, arg1, arg2, arg3, arg4)			printf(fmt, arg1, arg2, arg3, arg4)
	#define DMSG5(fmt, arg1, arg2, arg3, arg4, arg5)		printf(fmt, arg1, arg2, arg3, arg4, arg5)
	#define DMSG6(fmt, arg1, arg2, arg3, arg4, arg5, arg6)	printf(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
	#define DMSG7(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)	printf(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)

	#define EMSG0(fmt)			printf(fmt) // LOGE
	#define EMSG1(fmt, arg1)			printf(fmt, arg1)
	#define EMSG2(fmt, arg1, arg2)		printf(fmt, arg1, arg2)
	#define EMSG3(fmt, arg1, arg2, arg3)	printf(fmt, arg1, arg2, arg3)

#else
	#define DMSG0(fmt)					((void)0)
	#define DMSG1(fmt, arg1)					((void)0)
	#define DMSG2(fmt, arg1, arg2)				((void)0)
	#define DMSG3(fmt, arg1, arg2, arg3)			((void)0)
	#define DMSG4(fmt, arg1, arg2, arg3, arg4)			((void)0)
	#define DMSG5(fmt, arg1, arg2, arg3, arg4, arg5)		((void)0)
	#define DMSG6(fmt, arg1, arg2, arg3, arg4, arg5, arg6)	((void)0)
	#define DMSG7(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)	((void)0)

	#define EMSG0(fmt)			((void)0)
	#define EMSG1(fmt, arg1)			((void)0)
	#define EMSG2(fmt, arg1, arg2)		((void)0)
	#define EMSG3(fmt, arg1, arg2, arg3)	((void)0)
#endif



#if defined(RTV_IF_SPI)
	#define MAX_READ_TSP_SIZE	(188*16)
//	#define MAX_READ_TSP_SIZE	(188*8)
	//#define MAX_READ_TSP_SIZE	(188*24)
#else
	#define MAX_NUM_TSIF_TSP_CHUNK  16
	
	#if defined(TSIF_SAMSUNG_AP)
		#define TSIF_TSP_SIZE		188
				
	#elif defined(TSIF_QUALCOMM_AP)
		#define TSIF_TSP_SIZE	192 // 188(HTS pkt) + 3(TTS pkt) + 1(status)

		#define MSM_TSIF_HTS_PKT_SIZE	188
		#define MSM_TSIF_PKT_BUF_SIZE	(MAX_NUM_TSIF_TSP_CHUNK * MSM_TSIF_HTS_PKT_SIZE)

		int tsif_get_msm_hts_pkt(unsigned char *pkt_buf, 
								const unsigned char *ts_buf,
								int ts_size);
	#else
		#error "Code not present"
	#endif

	#define MAX_READ_TSP_SIZE	(MAX_NUM_TSIF_TSP_CHUNK * TSIF_TSP_SIZE)
#endif


#define MTV_DM_TIMER_EXPIRE_MS	1000//500

#if((MTV_DM_TIMER_EXPIRE_MS < 10) || (MTV_DM_TIMER_EXPIRE_MS > 1000/* by Linux */))
	#error "Must 10 from 1000"
#endif


typedef enum
{	
	MTV_MODE_TDMB   = 0,
	MTV_MODE_DAB     = 1,
	MTV_MODE_1SEG   = 3,
	MTV_MODE_FM       = 4
} E_MTV_MODE_TYPE;


typedef struct 
{
	const char *str;
	U32 freq;
} DAB_FREQ_TBL_INFO;

typedef struct
{
	unsigned int ch;
	unsigned int freq;
} ISDBT_FREQ_TBL_INFO;


extern BOOL mtv_is_power_on;

extern int fd_dmb_dev; /* MTV device file descriptor. */
extern volatile E_MTV_MODE_TYPE mtv_tv_mode;
extern unsigned int mtv_prev_channel;


#define CLEAR_STDIN	while(getc(stdin) != '\n')


double time_elapse(void);


struct mrevent {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    volatile bool triggered;
};
static inline void mrevent_init(struct mrevent *ev) 
{
    pthread_mutex_init(&ev->mutex, 0);
    pthread_cond_init(&ev->cond, 0);
    ev->triggered = false;
}
static inline void mrevent_trigger(struct mrevent *ev) 
{
    pthread_mutex_lock(&ev->mutex);
    ev->triggered = true;
    pthread_cond_signal(&ev->cond);
    pthread_mutex_unlock(&ev->mutex);
}
static inline void mrevent_reset(struct mrevent *ev)
{
//	DMSG0("[mrevent_reset] Entered\n");
    pthread_mutex_lock(&ev->mutex);
    ev->triggered = false;
    pthread_mutex_unlock(&ev->mutex);
}
static inline void mrevent_wait(struct mrevent *ev) 
{
     pthread_mutex_lock(&ev->mutex);
     while (!ev->triggered)
         pthread_cond_wait(&ev->cond, &ev->mutex);
     pthread_mutex_unlock(&ev->mutex);
}

#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
void show_fic_information(RTV_FIC_ENSEMBLE_INFO *ensble, U32 dwFreqKHz);
E_RTV_SERVICE_TYPE get_dab_service_type_from_user(void);
unsigned int get_subch_id_from_user(void);

#endif

/* DAB */
void dab_read(int dev);
void test_DAB(void);

void fm_read(int dev);
void test_FM(void);

void isdbt_read(int dev);
void test_ISDBT(void);

void tdmb_read(int dev);
void test_TDMB(void);

void test_RegisterIO(void);

void test_delete_thread(void);
int test_create_thread(void);


void verify_video_tsp(unsigned char* buf, unsigned int size) ;
void show_video_tsp_statistics(void);
void init_tsp_statistics(void);


/* from test_freq_tbl.c */
const DAB_FREQ_TBL_INFO *get_dab_freq_table_from_user(unsigned int *num_freq);
BOOL is_valid_dab_freq(unsigned int ch_freq_khz);

const DAB_FREQ_TBL_INFO *get_tdmb_freq_table_from_user(unsigned int *num_freq);
BOOL is_valid_tdmb_freq(unsigned int ch_freq_khz);

const ISDBT_FREQ_TBL_INFO *get_isdbt_freq_table_from_user(unsigned int *num_freq,
							unsigned int area_idx);


void resume_dm_timer(void);
void suspend_dm_timer(void);
int def_dm_timer(void (*callback)(void));

#if defined(TSIF_SAMSUNG_AP)
void tsif_run(int run);
#endif

int get_read_device_fd(void);
#endif 



