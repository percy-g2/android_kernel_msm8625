/*
 * mtv.c
 *
 * RAONTECH MTV main driver.
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

#include <linux/version.h>

#include "mtv.h"
#include "mtv_gpio.h"

#include "raontv_internal.h"
#include "mtv_ioctl_func.h"
#include <mach/socinfo.h>

extern irqreturn_t mtv_isr(int irq, void *param);
extern int mtv_isr_thread(void *data);

struct mtv_cb *mtv_cb_ptr = NULL;


#if defined(RTV_IF_SPI)  || defined(RTV_FIC_INTR_ENABLED)
static MTV_TSP_QUEUE_INFO mtv_tsp_pool;
static MTV_TSP_QUEUE_INFO mtv_tsp_queue;

static void mtv_reset_tsp(void)
{
    MTV_TS_PKT_INFO *tsp;

#ifdef DEBUG_MTV_IF_MEMORY
    DMBMSG("[mtv] # Max alloc tsp from pool: %d\n",
           mtv_cb_ptr->max_alloc_tsp_pool_cnt);
#endif

    if(mtv_tsp_pool.cnt == MAX_NUM_TS_PKT_BUF)
        goto MTV_RESET_EXIT;

    if(mtv_cb_ptr->prev_tsp != NULL)
    {
        mtv_free_tsp(mtv_cb_ptr->prev_tsp);
        mtv_cb_ptr->prev_tsp = NULL;
    }
    while ((tsp=mtv_get_tsp()) != NULL)
    {
        mtv_free_tsp(tsp);
    }

#ifdef DEBUG_MTV_IF_MEMORY
    mtv_cb_ptr->max_alloc_tsp_pool_cnt = 0;
#endif

MTV_RESET_EXIT:
    //DMBMSG("[mtv] Pool TSP count: %d\n", mtv_tsp_pool.cnt);

    RESET_MSC_IF_DEBUG;

    return;
}

/* Dequeue a ts packet from ts data queue. */
MTV_TS_PKT_INFO * mtv_get_tsp(void)
{
    MTV_TS_PKT_INFO *tsp = NULL;
    struct list_head *head_ptr = &mtv_tsp_queue.head;

    spin_lock(&mtv_tsp_queue.lock);

    if(!list_empty(head_ptr))
    {
        tsp = list_first_entry(head_ptr, MTV_TS_PKT_INFO, link);
        list_del(&tsp->link);
        mtv_tsp_queue.cnt--;
    }

    spin_unlock(&mtv_tsp_queue.lock);

    return tsp;
}

/* Enqueue a ts packet into ts data queue. */
void mtv_put_tsp(MTV_TS_PKT_INFO *tsp)
{
    spin_lock(&mtv_tsp_queue.lock);

    list_add_tail(&tsp->link, &mtv_tsp_queue.head);
    mtv_tsp_queue.cnt++;

    spin_unlock(&mtv_tsp_queue.lock);
}

#ifdef RTV_FIC_INTR_ENABLED
void mtv_clear_fic_tsp_contents(void)
{
    MTV_TS_PKT_INFO *tsp = NULL;
    struct list_head *item;

    spin_lock(&mtv_tsp_queue.lock);

    list_for_each(item, &mtv_tsp_queue.head)
    {
        tsp = list_entry(item, MTV_TS_PKT_INFO, link);
        tsp->fic_size = 0;
    }

    spin_unlock(&mtv_tsp_queue.lock);
}
#endif


void mtv_init_tsp_queue(void)
{
    mtv_cb_ptr->prev_tsp = NULL;

    spin_lock_init(&mtv_tsp_queue.lock);
    INIT_LIST_HEAD(&mtv_tsp_queue.head);
    mtv_tsp_queue.cnt = 0;
}


void mtv_free_tsp(MTV_TS_PKT_INFO *tsp)
{
    spin_lock(&mtv_tsp_pool.lock);

#ifdef RTV_MSC1_ENABLED
    tsp->msc1_size = 0;
#endif

#ifdef RTV_MSC0_ENABLED
    tsp->msc0_size = 0;
#endif

#ifdef RTV_FIC_INTR_ENABLED
    tsp->fic_size = 0;
#endif

    list_add_tail(&tsp->link, &mtv_tsp_pool.head);
    mtv_tsp_pool.cnt++;

    spin_unlock(&mtv_tsp_pool.lock);
}


MTV_TS_PKT_INFO *mtv_alloc_tsp(void)
{
    MTV_TS_PKT_INFO *tsp = NULL;
    struct list_head *head_ptr = &mtv_tsp_pool.head;

    spin_lock(&mtv_tsp_pool.lock);

    if(!list_empty(head_ptr))
    {
        tsp = list_first_entry(head_ptr, MTV_TS_PKT_INFO, link);
        list_del(&tsp->link);
        mtv_tsp_pool.cnt--;

#ifdef DEBUG_MTV_IF_MEMORY
        mtv_cb_ptr->max_alloc_tsp_pool_cnt
            = MAX(mtv_cb_ptr->max_alloc_tsp_pool_cnt,
                  MAX_NUM_TS_PKT_BUF - mtv_tsp_pool.cnt);
#endif
    }

    spin_unlock(&mtv_tsp_pool.lock);

    return tsp;

}


static int mtv_delete_tsp_pool(void)
{
    struct list_head *head_ptr = &mtv_tsp_pool.head;
    MTV_TS_PKT_INFO *tsp;

    if(mtv_cb_ptr->prev_tsp != NULL)
    {
        kfree(mtv_cb_ptr->prev_tsp);
        mtv_cb_ptr->prev_tsp = NULL;
    }

    while ((tsp=mtv_get_tsp()) != NULL)
    {
        kfree(tsp);
    }

    while (!list_empty(head_ptr))
    {
        tsp = list_entry(head_ptr->next, MTV_TS_PKT_INFO, link);
        list_del(&tsp->link);
        kfree(tsp);
    }

    return 0;
}

static int mtv_create_tsp_pool(void)
{
    unsigned int i;
    MTV_TS_PKT_INFO *tsp;

    spin_lock_init(&mtv_tsp_pool.lock);
    INIT_LIST_HEAD(&mtv_tsp_pool.head);

    mtv_tsp_pool.cnt = 0;
#ifdef DEBUG_MTV_IF_MEMORY
    mtv_cb_ptr->max_alloc_tsp_pool_cnt = 0;
#endif

    for(i=0; i<MAX_NUM_TS_PKT_BUF; i++)
    {
        tsp = (MTV_TS_PKT_INFO *)kmalloc(sizeof(MTV_TS_PKT_INFO),
                                         GFP_DMA);
        if(tsp == NULL)
        {
            mtv_delete_tsp_pool();
            DMBERR("[mtv] %d TSP allocation failed!\n", i);
            return -ENOMEM;
        }

#ifdef RTV_MSC1_ENABLED
        tsp->msc1_size = 0;
#endif

#ifdef RTV_MSC0_ENABLED
        tsp->msc0_size = 0;
#endif

#ifdef RTV_FIC_INTR_ENABLED
        tsp->fic_size = 0;
#endif

        list_add_tail(&tsp->link, &mtv_tsp_pool.head);
        mtv_tsp_pool.cnt++;
    }

    return 0;
}

static int mtv_create_thread(void)
{
    if(mtv_cb_ptr->isr_thread_cb == NULL)
    {
        init_waitqueue_head(&mtv_cb_ptr->isr_wq);

        mtv_cb_ptr->isr_thread_cb
            = kthread_run(mtv_isr_thread, NULL, "mtv_isr_thread");
        if (IS_ERR(mtv_cb_ptr->isr_thread_cb))
        {
            mtv_cb_ptr->isr_thread_cb = NULL;
            return PTR_ERR(mtv_cb_ptr->isr_thread_cb);
        }
    }

    return 0;
}

static void mtv_delete_thread(void)
{
    if(mtv_cb_ptr->isr_thread_cb != NULL)
    {
        kthread_stop(mtv_cb_ptr->isr_thread_cb);
        mtv_cb_ptr->isr_thread_cb = NULL;
    }
}
#endif /* #if defined(RTV_IF_SPI) || defined(RTV_FIC_INTR_ENABLED) */

static int mtv_power_off(void)
{
    DMBMSG("[mtv] Power OFF START\n");

    if(mtv_cb_ptr->is_power_on == FALSE)
        return 0;

    mtv_cb_ptr->is_power_on = FALSE;

#if defined(RTV_IF_SPI) ||defined(RTV_FIC_INTR_ENABLED)
    switch( mtv_cb_ptr->tv_mode )
    {
#ifdef RTV_TDMB_ENABLE
    case DMB_TV_MODE_TDMB :
        rtvTDMB_DisableStreamOut();
        rtvTDMB_CloseFIC();
        break;
#endif

#ifdef RTV_DAB_ENABLE
    case DMB_TV_MODE_DAB :
        rtvDAB_DisableStreamOut();

        /* Before FIC disable for RECONFIG INT enable FIC INT. */
        rtvDAB_DisableReconfigInterrupt();
        rtvDAB_CloseFIC();
        break;
#endif

#ifdef RTV_ISDBT_ENABLE
    case DMB_TV_MODE_1SEG :
        rtvISDBT_DisableStreamOut();
        break;
#endif

#ifdef RTV_FM_ENABLE
    case DMB_TV_MODE_FM :
        rtvFM_DisableStreamOut();
        break;
#endif
    default:
        break;
    }

    mtv_reset_tsp();
#endif

    rtvOEM_PowerOn(0);

    DMBMSG("[mtv] Power OFF END\n");

    return 0;
}


static int mtv_power_on(void)
{
    int ret = 0;

    if(mtv_cb_ptr->is_power_on == TRUE)
        return 0;

    DMBMSG("[mtv] Power ON Start\n");

    rtvOEM_PowerOn(1);

    RESET_MSC_IF_DEBUG;

    switch( mtv_cb_ptr->tv_mode )
    {
#ifdef RTV_TDMB_ENABLE
    case DMB_TV_MODE_TDMB :
        ret = rtvTDMB_Initialize(mtv_cb_ptr->country_band_type);
        break;
#endif

#ifdef RTV_DAB_ENABLE
    case DMB_TV_MODE_DAB :
        ret = rtvDAB_Initialize();
        break;
#endif

#ifdef RTV_ISDBT_ENABLE
    case DMB_TV_MODE_1SEG :
        mtv_cb_ptr->msc1_threshold_size = MTV_TS_THRESHOLD_SIZE;
        ret = rtvISDBT_Initialize(mtv_cb_ptr->country_band_type,
                                  MTV_TS_THRESHOLD_SIZE);
        break;
#endif

#ifdef RTV_FM_ENABLE
    case DMB_TV_MODE_FM :
        mtv_cb_ptr->msc1_threshold_size = MTV_TS_THRESHOLD_SIZE;
        /* RDS: MSC0 threshold ? */
        /* mtv_cb_ptr->msc0_threshold_size = ? */
        ret = rtvFM_Initialize(mtv_cb_ptr->adc_clk_type,
                               mtv_cb_ptr->msc1_threshold_size);
        break;
#endif
    default:
        goto err_return;
    }

    if(ret != RTV_SUCCESS)
    {
        DMBERR("[mtv] Tuner ON failed: %d\n", ret);
        ret = -EFAULT;
        goto err_return;
    }

#if defined(RTV_IF_SPI) || defined(RTV_FIC_INTR_ENABLED)
    mtv_cb_ptr->isr_cnt = 0;
#endif

    mtv_cb_ptr->is_power_on = TRUE;

    DMBMSG("[mtv] Power ON End\n");

    return 0;

err_return:

    mtv_power_off();

    return ret;
}

#ifdef RTV_NOSVC_FIC_INTR_ONLY_MODE
static ssize_t read_fic_only(char *buf, size_t cnt)
{
    int ret = -ENODEV;
    int copy_bytes;
    unsigned int copy_offset;
    MTV_TS_PKT_INFO *tsp;
    ssize_t read_len = 0;

    if(cnt == 0)
    {
        DMBERR("[read_fic_only] Invalid length: %d.\n", cnt);
        return -EAGAIN;
    }

    do
    {
        if(mtv_cb_ptr->prev_tsp == NULL)
        {
            /* Get a new tsp from tsp_queue. */
            tsp = mtv_get_tsp();
            if(tsp == NULL)
                break; /* Stop */

            copy_offset = 0;

            /* Save to use in next time if not finishded. */
            mtv_cb_ptr->prev_tsp = tsp;
            mtv_cb_ptr->prev_org_tsp_size = tsp->fic_size;
        }
        else
        {
            tsp = mtv_cb_ptr->prev_tsp;
            copy_offset = mtv_cb_ptr->prev_org_tsp_size - tsp->fic_size;
        }

        copy_bytes = MIN(tsp->fic_size, cnt);

        ret = CopyToUser(buf, (&tsp->fic_buf[1] + copy_offset), copy_bytes, 0);
        if (ret < 0)
            goto read_fail;
        else
        {
            read_len += copy_bytes;
            buf += copy_bytes;
            cnt -= copy_bytes;
            tsp->fic_size -= copy_bytes;
            if(tsp->fic_size == 0)
            {
                mtv_free_tsp(tsp);

                if(mtv_cb_ptr->prev_tsp == tsp) /* All used. */
                    mtv_cb_ptr->prev_tsp = NULL;
            }
        }
    }
    while(cnt != 0);

    return	read_len;

read_fail:
    if(mtv_cb_ptr->prev_tsp != NULL)
    {
        mtv_free_tsp(mtv_cb_ptr->prev_tsp);
        mtv_cb_ptr->prev_tsp = NULL;
    }

    return ret;
}
#endif /* #ifdef RTV_NOSVC_FIC_INTR_ONLY_MODE */

#ifdef RTV_MULTI_SERVICE_MODE
static inline ssize_t read_multiple_service(char *buf)
{
    int ret = 0;
    MTV_TS_PKT_INFO *tsp;
    ssize_t total_ts_len = 0;
    unsigned int max_num_item = 0;
    IOCTL_MULTI_SERVICE_BUF *m = (IOCTL_MULTI_SERVICE_BUF *)buf;
#ifdef RTV_MSC1_ENABLED
    unsigned int av_idx = 0;
#endif
#ifdef RTV_MSC0_ENABLED
    unsigned int data_idx = 0;
#endif
#ifdef RTV_FIC_INTR_ENABLED /* FIC interrupt Mode. */
    unsigned int fic_idx = 0;
#endif

    do
    {
#ifdef RTV_MSC1_ENABLED
        if(av_idx > MAX_NUM_MTV_MULTI_SVC_BUF)
            break; /* Stop */
#endif

#ifdef RTV_MSC0_ENABLED
        if(data_idx > MAX_NUM_MTV_MULTI_SVC_BUF)
            break;
#endif

#ifdef RTV_FIC_INTR_ENABLED
        if (fic_idx > MAX_NUM_MTV_MULTI_SVC_BUF)
            break;
#endif

        /* Get a new tsp from tsp_queue. */
        tsp = mtv_get_tsp();
        if(tsp == NULL)
            break; /* Stop. */

#ifdef RTV_MSC1_ENABLED
        if(tsp->msc1_size != 0)
        {
            ret = CopyToUser(m->av_ts[av_idx], &tsp->msc1_buf[1], tsp->msc1_size, 0);
            if (ret < 0)
                goto read_fail;

            ret = CopyToUser(&m->av_size[av_idx], &tsp->msc1_size, sizeof(uint), 0);
            if (ret < 0)
                goto read_fail;

            total_ts_len += tsp->msc1_size;
            av_idx++;
        }
#endif

#ifdef RTV_MSC0_ENABLED
        if(tsp->msc0_size != 0)
        {
            ret = CopyToUser(m->data_ts[data_idx], &tsp->msc0_buf[1], tsp->msc0_size, 0);
            if (ret < 0)
                goto read_fail;

            ret = CopyToUser(&m->data_size[data_idx], &tsp->msc0_size, sizeof(uint), 0);
            if (ret < 0)
                goto read_fail;

            total_ts_len += tsp->msc0_size;
            data_idx++;
        }
#endif

#ifdef RTV_FIC_INTR_ENABLED
        if(tsp->fic_size != 0)
        {
            ret = CopyToUser(m->fic_buf[fic_idx], &tsp->fic_buf[1], tsp->fic_size, 0);
            if (ret < 0)
                goto read_fail;

            ret = CopyToUser(&m->fic_size[fic_idx], &tsp->fic_size, sizeof(uint), 0);
            if (ret < 0)
                goto read_fail;

            ret = CopyToUser(&m->freq_khz, &mtv_cb_ptr->freq_khz, sizeof(uint), 0);
            if (ret < 0)
                goto read_fail;

            total_ts_len += tsp->fic_size;
            fic_idx++;
        }
#endif

        /* Free a tsp buffer */
        mtv_free_tsp(tsp);
    }
    while(1);

    if(total_ts_len != 0)
    {
#ifdef RTV_MSC1_ENABLED
        max_num_item = MAX(max_num_item, av_idx);
#endif
#ifdef RTV_MSC0_ENABLED
        max_num_item = MAX(max_num_item, data_idx);
#endif
#ifdef RTV_FIC_INTR_ENABLED /* FIC interrupt Mode. */
        max_num_item = MAX(max_num_item, fic_idx);
#endif
        ret = CopyToUser(&m->max_num_item, &max_num_item, sizeof(uint), 0);
        if (ret < 0)
            goto read_fail;
    }

#ifdef DEBUG_MTV_IF_MEMORY
    mtv_cb_ptr->max_remaining_tsp_cnt
        = MAX(mtv_cb_ptr->max_remaining_tsp_cnt, mtv_tsp_queue.cnt);
#endif

    return total_ts_len;

read_fail:
    if(tsp != NULL)
        mtv_free_tsp(tsp);

    return ret;
}
#endif /* #ifdef RTV_MULTI_SERVICE_MODE */


#ifdef RTV_MSC1_ENABLED
/* In case of 1 av(TDMB/DAB), 1 data(TDMB/DAB), 1 video(ISDBT) or 1 pcm(FM)
Use msc1 memory. */
static inline ssize_t read_single_service(char *buf, size_t cnt)
{
    int ret = -ENODEV;
    int copy_bytes;/* # of bytes to be copied. */
    unsigned int copy_offset;
    MTV_TS_PKT_INFO *tsp;
    ssize_t read_len = 0;

    if(cnt == 0)
    {
        DMBERR("[read_single_service] Invalid length: %d.\n", cnt);
        return -EAGAIN;
    }

    do
    {
        if(mtv_cb_ptr->prev_tsp == NULL)
        {
            /* Get a new tsp from tsp_queue. */
            tsp = mtv_get_tsp();
            if(tsp == NULL)
                break; /* Stop */

            copy_offset = 0;

            /* Save to use in next time if not finishded. */
            mtv_cb_ptr->prev_tsp = tsp;
            mtv_cb_ptr->prev_org_tsp_size = tsp->msc1_size;
        }
        else
        {
            tsp = mtv_cb_ptr->prev_tsp;
            copy_offset =
                mtv_cb_ptr->prev_org_tsp_size - tsp->msc1_size;
        }

        copy_bytes = MIN(tsp->msc1_size, cnt);

        ret = CopyToUser(buf, (&tsp->msc1_buf[1] + copy_offset), copy_bytes, 0);
        if (ret < 0)
            goto read_fail;
        else
        {
            read_len += copy_bytes;
            buf += copy_bytes;
            cnt -= copy_bytes;
            tsp->msc1_size -= copy_bytes;
            if(tsp->msc1_size == 0)
            {
                mtv_free_tsp(tsp);

                if(mtv_cb_ptr->prev_tsp == tsp) /* All used. */
                    mtv_cb_ptr->prev_tsp = NULL;
            }
        }
    }
    while(cnt != 0);

#ifdef DEBUG_MTV_IF_MEMORY
    mtv_cb_ptr->max_remaining_tsp_cnt
        = MAX(mtv_cb_ptr->max_remaining_tsp_cnt, mtv_tsp_queue.cnt);
#endif

    return	read_len;

read_fail:
    if(mtv_cb_ptr->prev_tsp != NULL)
    {
        mtv_free_tsp(mtv_cb_ptr->prev_tsp);
        mtv_cb_ptr->prev_tsp = NULL;
    }

    return ret;
}
#endif /* #ifdef RTV_MSC1_ENABLED */


#if defined(RTV_IF_SPI)  || defined(RTV_FIC_INTR_ENABLED)
static ssize_t mtv_read(struct file *filp, char *buf, size_t count, loff_t *pos)
{
#if defined(RTV_ISDBT_ONLY_ENABLED)
    return read_single_service(buf, count); /* ISDBT only */

#elif defined(RTV_TDMBorDAB_ONLY_ENABLED)
#ifdef RTV_MULTI_SERVICE_MODE
    return read_multiple_service(buf);

#elif defined(RTV_NOSVC_FIC_INTR_ONLY_MODE)
    return read_fic_only(buf, count);

#else
    return read_single_service(buf, count);
#endif
#else
    /* Assume that FM only project was not exist. */
    switch (mtv_cb_ptr->tv_mode)
    {
#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
    case DMB_TV_MODE_TDMB:
    case DMB_TV_MODE_DAB:
#ifdef RTV_MULTI_SERVICE_MODE
        return read_multiple_service(buf);

#elif defined(RTV_NOSVC_FIC_INTR_ONLY_MODE)
        return read_fic_only(buf, count);

#else
        return read_single_service(buf, count);
#endif
#endif

#if defined(RTV_ISDBT_ENABLE)
    case DMB_TV_MODE_1SEG:
        return read_single_service(buf, count);
#endif

#ifdef RTV_FM_ENABLE
    case DMB_TV_MODE_FM:
#ifdef RTV_FM_RDS_ENABLED
        return read_multiple_service(buf);

#else
        return read_single_service(buf, count);
#endif
#endif /* #ifdef RTV_FM_ENABLE */

    default:
        break;
    }
#endif

    return -ENODEV;
}
#endif /* #if defined(RTV_IF_SPI) ||defined(RTV_FIC_INTR_ENABLED) */


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static long mtv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int mtv_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,  unsigned long arg)
#endif
{
    int ret = 0;

    switch( cmd )
    {
#ifdef RTV_TDMB_ENABLE
    case IOCTL_TDMB_POWER_ON:
        ret = tdmb_power_on(arg);
        break;

    case IOCTL_TDMB_POWER_OFF:
        mtv_power_off();
        break;

    case IOCTL_TDMB_SCAN_FREQ:
        ret = tdmb_scan_freq(arg);
        break;

    case IOCTL_TDMB_SCAN_STOP:
        ret = tdmb_scan_stop(arg);
        break;

    case IOCTL_TDMB_READ_FIC:
        ret = tdmb_read_fic(arg);
        break;

    case IOCTL_TDMB_OPEN_SUBCHANNEL:
        ret = tdmb_open_subchannel(arg);
        break;

    case IOCTL_TDMB_CLOSE_SUBCHANNEL:
        ret = tdmb_close_subchannel(arg);
        break;

    case IOCTL_TDMB_CLOSE_ALL_SUBCHANNELS:
        tdmb_close_all_subchannels(arg);
        break;

    case IOCTL_TDMB_GET_LOCK_STATUS:
        ret = tdmb_get_lock_status(arg);
        break;

    case IOCTL_TDMB_GET_SIGNAL_INFO:
        ret = tdmb_get_signal_info(arg);
        break;
#endif /* #ifdef RTV_TDMB_ENABLE */

#ifdef RTV_DAB_ENABLE
    case IOCTL_DAB_POWER_ON:
        ret = dab_power_on();
        break;

    case IOCTL_DAB_POWER_OFF:
        mtv_power_off();
        break;

    case IOCTL_DAB_SCAN_FREQ: /* Full scan by user. */
        ret = dab_scan_freq(arg);
        break;

    case IOCTL_DAB_SCAN_STOP: /* Called when Full scan or Reconfig scan. */
        dab_scan_stop(arg);
        break;

    case IOCTL_DAB_READ_FIC: /* FIC polling Mode. */
        ret = dab_read_fic(arg);
        break;

    case IOCTL_DAB_OPEN_FIC:
        ret = dab_open_fic(arg);
        break;

    case IOCTL_DAB_CLOSE_FIC:
        ret = dab_close_fic(arg);
        break;

    case IOCTL_DAB_OPEN_SUBCHANNEL:
        ret = dab_open_subchannel(arg);
        break;

    case IOCTL_DAB_CLOSE_SUBCHANNEL:
        ret = dab_close_subchannel(arg);
        break;

    case IOCTL_DAB_CLOSE_ALL_SUBCHANNELS:
        dab_close_all_subchannels(arg);
        break;

    case IOCTL_DAB_GET_LOCK_STATUS:
        ret = dab_get_lock_status(arg);
        break;

    case IOCTL_DAB_GET_SIGNAL_INFO:
        ret = dab_get_signal_info(arg);
        break;
#endif /* #ifdef RTV_DAB_ENABLE */

#ifdef RTV_ISDBT_ENABLE
    case IOCTL_ISDBT_POWER_ON:
        ret = isdbt_power_on(arg);
        break;

    case IOCTL_ISDBT_POWER_OFF:
        mtv_power_off();
        break;

    case IOCTL_ISDBT_SCAN_FREQ:
        ret = isdbt_scan_freq(arg);
        break;

    case IOCTL_ISDBT_SET_FREQ:
        ret = isdbt_set_freq(arg);
        break;

    case IOCTL_ISDBT_START_TS:
        isdbt_start_ts();
        break;

    case IOCTL_ISDBT_STOP_TS:
        isdbt_stop_ts();
        break;

    case IOCTL_ISDBT_GET_LOCK_STATUS:
        ret = isdbt_get_lock_status(arg);
        break;

    case IOCTL_ISDBT_GET_TMCC:
        ret = isdbt_get_tmcc(arg);
        break;

    case IOCTL_ISDBT_GET_SIGNAL_INFO:
        ret = isdbt_get_signal_info(arg);
        break;
#endif /* #ifdef RTV_ISDBT_ENABLE */

#ifdef RTV_FM_ENABLE
    case IOCTL_FM_POWER_ON: /* with adc clk type */
        ret = fm_power_on(arg);
        break;

    case IOCTL_FM_POWER_OFF:
        mtv_power_off();
        break;

    case IOCTL_FM_SCAN_FREQ:
        ret = fm_scan_freq(arg);
        break;

    case IOCTL_FM_SRCH_FREQ:
        ret = fm_search_freq(arg);
        break;

    case IOCTL_FM_SET_FREQ:
        ret = fm_set_freq(arg);
        break;

    case IOCTL_FM_START_TS:
        fm_start_ts();
        break;

    case IOCTL_FM_STOP_TS:
        fm_stop_ts();
        break;

    case IOCTL_FM_GET_LOCK_STATUS :
        ret = fm_get_lock_status(arg);
        break;

    case IOCTL_FM_GET_RSSI:
        ret = fm_get_rssi(arg);
        break;
#endif /* #ifdef RTV_FM_ENABLE */

    case IOCTL_TEST_GPIO_SET:
    case IOCTL_TEST_GPIO_GET:
        ret = test_gpio(arg, cmd);
        break;

    case IOCTL_TEST_MTV_POWER_ON:
    case IOCTL_TEST_MTV_POWER_OFF:
        test_power_on_off( cmd);
        break;

    case IOCTL_TEST_REG_SINGLE_READ:
    case IOCTL_TEST_REG_BURST_READ:
    case IOCTL_TEST_REG_WRITE:
        ret = test_register_io(arg, cmd);
        break;



		
    default:
        DMBERR("[mtv] Invalid ioctl command: %d\n", cmd);
        ret = -ENOTTY;
        break;
    }

    return ret;
}


#ifdef RTV_DAB_RECONFIG_ENABLED
static int mtv_sigio_fasync(int fd, struct file *filp, int mode)
{
    return fasync_helper(fd, filp, mode, &mtv_cb_ptr->sigio_async_queue);
}
#endif

static int mtv_close(struct inode *inode, struct file *filp)
{
    DMBMSG("\n[mtv_close] called\n");

    mtv_power_off();

#ifdef RTV_DAB_RECONFIG_ENABLED
    fasync_helper(-1, filp, 0, &mtv_cb_ptr->sigio_async_queue);
#endif

    DMBMSG("[mtv_close] END\n");

    return 0;
}

static int mtv_open(struct inode *inode, struct file *filp)
{
    DMBMSG("[mtv_open] called\n");

    return 0;
}


static struct file_operations mtv_fops =
{
    .owner = THIS_MODULE,

#if defined(RTV_IF_SPI) ||defined(RTV_FIC_INTR_ENABLED)
    .read = mtv_read,
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    .unlocked_ioctl = mtv_ioctl,
#else
    .ioctl = mtv_ioctl,
#endif

#ifdef RTV_DAB_RECONFIG_ENABLED
    .fasync = mtv_sigio_fasync,
#endif
    .release = mtv_close,
    .open = mtv_open
};

static struct miscdevice mtv_misc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = RAONTV_DEV_NAME,
    .fops = &mtv_fops,
};


static void mtv_deinit_device(void)
{
#if defined(RTV_IF_SPI) || defined(RTV_FIC_INTR_ENABLED)
	if (cpu_is_msm8625()){
		free_irq(RAONTV_IRQ_INT, NULL);
	}
	else{
		free_irq(QRD8225Q_RAONTV_IRQ_INT, NULL);
	}

    mtv_delete_thread();
#endif

    mtv_power_off();

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
    i2c_del_driver(&mtv_i2c_driver);

#elif defined(RTV_IF_SPI)
    /* Wait for read() compeltion. memory crasy... */
    mtv_delete_tsp_pool();

    spi_unregister_driver(&mtv_spi_driver);
#endif

    if(mtv_cb_ptr != NULL)
    {
        kfree(mtv_cb_ptr);
        mtv_cb_ptr = NULL;
    }


    DMBMSG("[mtv_deinit_device] END\n");
}


static int mtv_init_device(void)
{
    int ret = 0;

    mtv_cb_ptr = kzalloc(sizeof(struct mtv_cb), GFP_KERNEL);
    if (!mtv_cb_ptr)
        return -ENOMEM;

    mtv_configure_gpio();

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
    if((ret=i2c_add_driver(&mtv_i2c_driver)) < 0)
    {
        DMBERR("RAONTV I2C driver registe failed\n");
        goto err_free_mem;
    }

#elif defined(RTV_IF_SPI)
    if((ret=spi_register_driver(&mtv_spi_driver)) < 0)
    {
        DMBERR("RAONTV SPI driver registe failed\n");
        goto err_free_mem;
    }
#endif

#if defined(RTV_IF_SPI) || defined(RTV_FIC_INTR_ENABLED)
    /* Init tsp queue.*/
    mtv_init_tsp_queue();

    ret = mtv_create_tsp_pool();
    if(ret < 0)
    {
        DMBERR("RAONTV SPI TS buffer creation failed\n");
        goto err_free_mem;
    }

    if ((ret=mtv_create_thread()) != 0)
    {
        DMBERR("[mtv_power_on] mtv_create_thread() error\n");
        goto err_free_mem;
    }

	if (cpu_is_msm8625()){
		ret = request_irq(RAONTV_IRQ_INT,
						  mtv_isr,
						  IRQ_TYPE_EDGE_FALLING,
						  RAONTV_DEV_NAME, NULL);
	}
	else{
		ret = request_irq(QRD8225Q_RAONTV_IRQ_INT,
						  mtv_isr,
						  IRQ_TYPE_EDGE_FALLING,
						  RAONTV_DEV_NAME, NULL);
	}
    if (ret != 0)
    {
        mtv_delete_thread();
        DMBERR("[mtv_init_device] Failed to install irq (%d)\n", ret);
        goto err_free_mem;
    }
#endif

    return 0;

err_free_mem:
    mtv_deinit_device();

    return ret;
}

static int __init mtv_module_init(void)
{
    int ret;
    DMBMSG("mtv_module_init\n");

    ret = mtv_init_device();
    if(ret<0)
        return ret;

    /* misc device registration */
    ret = misc_register(&mtv_misc_device);
    if( ret )
    {
        DMBERR("[mtv_module_init] misc_register() failed! : %d", ret);
        return ret;
    }

    return 0;
}


static void __exit mtv_module_exit(void)
{
    mtv_deinit_device();

    misc_deregister(&mtv_misc_device);
}

late_initcall(mtv_module_init);
module_exit(mtv_module_exit);
MODULE_DESCRIPTION("MTV driver");
MODULE_LICENSE("GPL");

