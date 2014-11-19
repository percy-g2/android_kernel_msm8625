#include "test.h"

int fd_dmb_dev; /* MTV device file descriptor. */
volatile E_MTV_MODE_TYPE mtv_tv_mode;

static IOCTL_REG_ACCESS_INFO ioctl_reg_access_info;
static IOCTL_GPIO_ACCESS_INFO gpio_info;

unsigned int mtv_prev_channel; /* TDMB/DAB/FM: KHz, ISDBT: Channel number. */


#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
static int fd_tsif_dev;
#if defined(TSIF_SAMSUNG_AP)
static int tsif_run_flag;
#endif
#endif

static volatile int dmb_thread_run;
static pthread_t tsp_read_thread_cb;
static pthread_t *tsp_read_thread_cb_ptr;


/* To get signal inforamtion.*/
static volatile int dm_timer_thread_run;
static pthread_t dm_timer_thread_cb;
static pthread_t *dm_timer_thread_cb_ptr;


typedef struct
{
    BOOL	is_resumed;
    unsigned int sleep_msec;
    void (*callback)(void);
    struct mrevent event;
} MTV_DM_TIMER_INFO;
static MTV_DM_TIMER_INFO dm_timer;


struct timeval st, sp;
double time_elapse(void)
{

    struct timeval tt;
    gettimeofday( &sp, NULL );

    tt.tv_sec = sp.tv_sec - st.tv_sec;
    tt.tv_usec = sp.tv_usec - st.tv_usec;

    gettimeofday(&st, NULL );

    if( tt.tv_usec < 0 )
    {

        tt.tv_usec += 1000000;
        tt.tv_sec--;

    }

    return (double)tt.tv_usec/1000000 + (double)tt.tv_sec;
}


#define NUM_PID_GROUP	(8192/32/*bits*/)

typedef struct
{
    unsigned long	pid_cnt;
    unsigned int	continuity_counter;
    unsigned long	discontinuity_cnt;
} PID_INFO;

typedef struct
{
    unsigned long	tsp_cnt;

    unsigned long	sync_byte_err_cnt;
    unsigned long	tei_cnt;
    unsigned long	null_cnt;
} TSP_INFO;

static PID_INFO pid_info[8192];
static TSP_INFO tsp_info;
static __u32 pid_grp_bits[NUM_PID_GROUP]; /* 32 Bits for PID pre group*/

void show_video_tsp_statistics(void)
{
    __u32 pid_bits;
    unsigned int  grp_idx, pid;
    float sync_err_rate = 0.0, total_err_rate = 0.0;
    unsigned long num_err_pkt;

    DMSG0("\t################# [Video TSP Statistics] ###############\n");
    for(grp_idx=0; grp_idx<NUM_PID_GROUP; grp_idx++)
    {
        pid = grp_idx * 32;
        pid_bits = pid_grp_bits[grp_idx];
        while( pid_bits )
        {
            /* Check if the pid was countered. */
            if( pid_bits & 0x1 )
            {
                DMSG3("\t# PID: 0x%04X, Pkts: %ld, Discontinuity: %ld\n",
                      pid, pid_info[pid].pid_cnt,
                      pid_info[pid].discontinuity_cnt);

            }
            pid_bits >>= 1;
            pid++;
        }
    }

    if(tsp_info.sync_byte_err_cnt > tsp_info.tei_cnt)
        num_err_pkt = tsp_info.sync_byte_err_cnt;
    else
        num_err_pkt = tsp_info.tei_cnt;

    if(tsp_info.tsp_cnt != 0)
    {
        sync_err_rate = ((float)tsp_info.sync_byte_err_cnt / (float)tsp_info.tsp_cnt) * 100;
        total_err_rate = ((float)num_err_pkt / (float)tsp_info.tsp_cnt) * 100;
    }

    DMSG1("\t#\t\t Total TSP: %ld\n", tsp_info.tsp_cnt);
    DMSG3("\t# [Error Kind] Sync Byte: %ld, TEI: %ld, Null: %ld \n",
          tsp_info.sync_byte_err_cnt,
          tsp_info.tei_cnt, tsp_info.null_cnt);
    DMSG2("\t# [Error Rate] Total: %.3f %%, Sync: %.3f %%\n",
          total_err_rate, sync_err_rate);
    DMSG0("\t######################################################\n");
}


static inline void verify_video_pid(unsigned char *buf, unsigned int pid)
{
    unsigned int prev_conti;
    unsigned int cur_conti = buf[3] & 0x0F;
    unsigned int grp_idx = pid >> 5;
    unsigned int pid_idx = pid & 31;

    if((pid_grp_bits[grp_idx] & (1<<pid_idx)) != 0x0)
    {
        prev_conti = pid_info[pid].continuity_counter;

        if(((prev_conti + 1) & 0x0F) != cur_conti)
            pid_info[pid].discontinuity_cnt++;
    }
    else
    {
        pid_grp_bits[grp_idx] |= (1<<pid_idx);

        pid_info[pid].pid_cnt = 0;
    }

    pid_info[pid].continuity_counter = cur_conti;
    pid_info[pid].pid_cnt++;
}

void verify_video_tsp(unsigned char *buf, unsigned int size)
{
    unsigned int pid;

    do
    {
        tsp_info.tsp_cnt++;

        pid = ((buf[1] & 0x1F) << 8) | buf[2];

        if(buf[0] == 0x47)
        {
            if((buf[1] & 0x80) != 0x80)
            {
                if(pid != 0x1FFF)
                    verify_video_pid(buf, pid);

                else
                    tsp_info.null_cnt++;
            }
            else
                tsp_info.tei_cnt++;
        }
        else
            tsp_info.sync_byte_err_cnt++;

        buf += 188;
        size -= 188;
    }
    while(size != 0);
}

void init_tsp_statistics(void)
{
    memset(&tsp_info, 0, sizeof(TSP_INFO));
    memset(pid_grp_bits, 0, sizeof(__u32) * NUM_PID_GROUP);
}

#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
void show_fic_information(RTV_FIC_ENSEMBLE_INFO *ensble, U32 dwFreqKHz)
{
    unsigned int i;

    DMSG0("\t########################################################\n");
    DMSG1("\t#\t\tFreq: %u Khz\n", dwFreqKHz);

    for(i=0; i<ensble->bTotalSubCh; i++)
    {
        DMSG1("\t# [%-16s] ", ensble->tSubChInfo[i].szSvcLabel);
        DMSG1("SubCh ID: %-2d, ", ensble->tSubChInfo[i].bSubChID);
        DMSG1("Bit Rate: %3d, ", ensble->tSubChInfo[i].wBitRate);
        DMSG1("TMId: %d, ", ensble->tSubChInfo[i].bTMId);
        DMSG1("Svc Type: 0x%02X\n", ensble->tSubChInfo[i].bSvcType);
    }

    DMSG1("\t#\tTotal: %d\n", ensble->bTotalSubCh);
    DMSG0("\t########################################################\n");
}
/* TDMB/DAB both used */
E_RTV_SERVICE_TYPE get_dab_service_type_from_user(void)
{
    E_RTV_SERVICE_TYPE svc_type;
    while(1)
    {
        DMSG0("Input service type(1: Video, 2:Audio, 4: Data):");
        scanf("%u", &svc_type);
        CLEAR_STDIN;
        if((svc_type ==RTV_SERVICE_VIDEO)
                || (svc_type ==RTV_SERVICE_AUDIO)
                || (svc_type ==RTV_SERVICE_DATA))
            break;
    }
    return svc_type;
}
unsigned int get_subch_id_from_user(void)
{
    unsigned int subch_id;

    while(1)
    {
        DMSG0("Input sub channel ID(ex. 0):");
        scanf("%u", &subch_id);
        CLEAR_STDIN;
        if(subch_id < 64)
            break;
    }

    return subch_id;
}
#endif

#if defined(TSIF_SAMSUNG_AP)
void tsif_run(int run)
{
    if(run == 1)
    {
        if(tsif_run_flag == 1)
            return;
        /* Start TSI */
        if(ioctl(fd_tsif_dev, 0xAABB) != 0)
        {
            EMSG0("TSI start error");
        }
    }
    else
    {
        if(tsif_run_flag== 0)
            return;
        /*	Stop TSI */
        if(ioctl(fd_tsif_dev, 0xAACC) != 0)
        {
            EMSG0("TSI stop error");
        }
    }

    tsif_run_flag = run;
}
#endif

#if defined(TSIF_QUALCOMM_AP)
int tsif_get_msm_hts_pkt(unsigned char *pkt_buf, const unsigned char *ts_buf,
                         int ts_size)
{
    unsigned int i, num_pkts = ts_size / TSIF_TSP_SIZE;
    for(i = 0; i < num_pkts; i++)
    {
        memcpy(pkt_buf, ts_buf, MSM_TSIF_HTS_PKT_SIZE);
        pkt_buf += MSM_TSIF_HTS_PKT_SIZE;
        ts_buf += TSIF_TSP_SIZE;
    }
    return num_pkts * MSM_TSIF_HTS_PKT_SIZE;
}
#endif
int get_read_device_fd(void)
{
#if defined(RTV_IF_SPI)
    return fd_dmb_dev;
#elif defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
    return fd_tsif_dev;
#endif
}

void *read_thread(void *arg)
{
#if defined(RTV_IF_SPI)
    int dev = fd_dmb_dev;
#elif defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
    int dev = fd_tsif_dev;
#endif

    DMSG0("[read_thread] Entered\n");

    for(;;)
    {
        if(dmb_thread_run == 0)
        {
            DMSG0("[read_thread] not run\n");
            break;
        }

        switch(mtv_tv_mode)
        {
#ifdef RTV_ISDBT_ENABLE
        case MTV_MODE_1SEG:
//            DMSG0("read_thread, MTV_MODE_1SEG\n");
            isdbt_read(dev);
            break;
#endif

#ifdef RTV_TDMB_ENABLE
        case MTV_MODE_TDMB:
            DMSG0("read_thread, MTV_MODE_TDMB\n");
            tdmb_read(dev);
            break;
#endif

#ifdef RTV_DAB_ENABLE
        case MTV_MODE_DAB:
            DMSG0("read_thread, MTV_MODE_DAB\n");
            dab_read(dev);
            break;
#endif

#ifdef RTV_FM_ENABLE
        case MTV_MODE_FM:
            DMSG0("read_thread, MTV_MODE_FM\n");
            fm_read(dev);
            break;
#endif

        default:
            //DMSG0("Unknown mode\n");
            sleep(1);
            break;
        }

    }

    DMSG0("[read_thread] Exit...\n");

    pthread_exit((void *)NULL);
    return NULL;
}


void resume_dm_timer(void)
{
    if(dm_timer.is_resumed == TRUE)
        return;

    dm_timer.is_resumed = TRUE;

    dm_timer.sleep_msec = MTV_DM_TIMER_EXPIRE_MS;

    /* Send the event to timer thread. */
    mrevent_trigger(&dm_timer.event);
}

void suspend_dm_timer(void)
{
    dm_timer.sleep_msec = 0;
    dm_timer.is_resumed = FALSE;
}

/* Define DM timer. */
int def_dm_timer(void (*callback)(void))
{
    if(callback == NULL)
    {
        EMSG0("[MTV] Invalid exipre handler\n");
        return -2;
    }

    dm_timer.callback = callback;

    return 0;
}

static void init_dm_timer(void)
{
    mrevent_init(&dm_timer.event);

    dm_timer.is_resumed = FALSE;
    dm_timer.sleep_msec = 0;
    dm_timer.callback = NULL;
}

void * test_dm_timer_thread(void *arg)
{
    unsigned int sleep_msec;

    DMSG0("[test_dm_timer_thread] Entered\n");

    init_dm_timer();

    for(;;)
    {
        /* Wait for the event. */
        mrevent_wait(&dm_timer.event);

        /* Clear the event. */
        mrevent_reset(&dm_timer.event);

        if(dm_timer_thread_run == 0)
            break;

        /* To prevent crash , we use the copied address. */
        sleep_msec = dm_timer.sleep_msec;
        if(sleep_msec != 0)
        {
            if(dm_timer.callback != NULL)
                (*(dm_timer.callback))();

            usleep(sleep_msec*1000);
            //sleep(1);

            mrevent_trigger(&dm_timer.event);
        }
    }

    DMSG0("[test_dm_timer_thread] Exit...\n");

    pthread_exit((void *)NULL);
    return NULL;
}


int test_create_thread(void)
{
    int ret = 0;

    if(dm_timer_thread_cb_ptr == NULL)
    {
        dm_timer_thread_run = 1;

        ret = pthread_create(&dm_timer_thread_cb, NULL, test_dm_timer_thread, NULL);
        if(ret < 0)
        {
            EMSG1("DM Timer thread create error: %d\n", ret);
            return ret;
        }

        dm_timer_thread_cb_ptr = &dm_timer_thread_cb;
    }

    sleep(1);

    if(tsp_read_thread_cb_ptr == NULL)
    {
        dmb_thread_run = 1;

        ret = pthread_create(&tsp_read_thread_cb, NULL, read_thread, NULL);
        if(ret < 0)
        {
            EMSG1("thread create error: %d\n", ret);
            return ret;
        }

        tsp_read_thread_cb_ptr = &tsp_read_thread_cb;
    }

    return ret;
}

void test_delete_thread(void)
{
    if(tsp_read_thread_cb_ptr != NULL)
    {
        dmb_thread_run = 0;

        pthread_join(tsp_read_thread_cb, NULL);
        tsp_read_thread_cb_ptr = NULL;
    }

    if(dm_timer_thread_cb_ptr != NULL)
    {
        dm_timer_thread_run = 0;
        mrevent_trigger(&dm_timer.event);

        pthread_join(dm_timer_thread_cb, NULL);
        dm_timer_thread_cb_ptr = NULL;
    }
}


void test_GPIO(void)
{
    int key;
    unsigned int pin = 0;
    unsigned int value;

    while(1)
    {
        DMSG0("================ GPIO Test ===============\n");
        DMSG0("\t1: GPIO Write(Set) Test\n");
        DMSG0("\t2: GPIO Read(Get) Test\n");
        DMSG0("\tq or Q: Quit\n");
        DMSG0("========================================\n");

        //fflush(stdin);
        key = getc(stdin);
        CLEAR_STDIN;

        switch( key )
        {
        case '1':
            DMSG0("[GPIO Write(Set) Test]\n");

            DMSG0("Select Pin Number:");
            scanf("%u" , &gpio_info.pin);
            CLEAR_STDIN;

            while(1)
            {
                DMSG0("Input Pin Level(0 or 1):");
                scanf("%u" , &value);
                CLEAR_STDIN;
                if((value==0) || (value==1))
                    break;
            }
            gpio_info.pin = pin;
            gpio_info.value = value;

            if(ioctl(fd_dmb_dev, IOCTL_TEST_GPIO_SET, &gpio_info) < 0)
            {
                EMSG0("IOCTL_TEST_GPIO_SET failed\n");
            }
            break;

        case '2':
            DMSG0("[GPIO Read(Set) Test]\n");

            DMSG0("Select Pin Number:");
            scanf("%u" , &gpio_info.pin);
            CLEAR_STDIN;

            if(ioctl(fd_dmb_dev, IOCTL_TEST_GPIO_GET, &gpio_info) < 0)
            {
                EMSG0("IOCTL_TEST_GPIO_GET failed\n");
            }

            DMSG2("Pin(%u): %u\n", gpio_info.pin, gpio_info.value);
            DMSG0("\n");
            break;

        case 'q':
        case 'Q':
            goto GPIO_TEST_EXIT;

        default:
            DMSG1("[%c]\n", key);
        }
    }

GPIO_TEST_EXIT:
    return;
}

static void show_register_page(void)
{
    DMSG0("===============================================\n");
    DMSG0("\t0: HOST_PAGE\n");
    DMSG0("\t1: RF_PAGE\n");
    DMSG0("\t2: COMM_PAGE\n");
    DMSG0("\t3: DD_PAGE\n");
    DMSG0("\t4: MSC0_PAGE\n");
    DMSG0("\t5: MSC1_PAGE\n");
    DMSG0("\t6: OFDM PAGE\n");
    DMSG0("\t7: FEC_PAGE\n");
    DMSG0("\t8: FIC_PAGE\n");
    DMSG0("===============================================\n");
}

void test_RegisterIO(void)
{
    int key;
    unsigned int i, page, addr, read_cnt, write_data;
    const char *page_str[] =
    {"HOST", "RF", "COMM", "DD", "MSC0", "MSC1", "OFDM", "FEC", "FIC"};

    while(1)
    {
        DMSG0("================ Register IO Test ===============\n");
        DMSG0("\t1: Single Read\n");
        DMSG0("\t2: Burst Read\n");
        DMSG0("\t3: Write\n");
        DMSG0("\tq or Q: Quit\n");
        DMSG0("================================================\n");

        //fflush(stdin);
        key = getc(stdin);
        CLEAR_STDIN;

        switch( key )
        {
        case '1':
            DMSG0("[Reg IO] Single Read\n");
            show_register_page();
            while(1)
            {
                DMSG0("Select Page:");
                scanf("%x" , &page);
                CLEAR_STDIN;
                if(page <= 8)
                    break;
            }

            DMSG0("Input Address(hex) : ");
            scanf("%x", &addr);
            CLEAR_STDIN;

            ioctl_reg_access_info.page = page;
            ioctl_reg_access_info.addr = addr;

            DMSG2("\t[INPUT] %s[0x%02X]\n", page_str[page], addr);

            if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_SINGLE_READ, &ioctl_reg_access_info) == 0)
                DMSG3("%s[0x%02X]: 0x%02X\n",
                      page_str[page],
                      addr, ioctl_reg_access_info.read_data[0]);
            else
                EMSG0("IOCTL_TEST_REG_SINGLE_READ failed\n");

            break;

        case '2':
            DMSG0("[Reg IO] Burst Read\n");
            show_register_page();
            while(1)
            {
                DMSG0("Select Page:");
                scanf("%x" , &page);
                CLEAR_STDIN;
                if(page <= 8)
                    break;
            }

            DMSG0("Input Address(hex): ");
            scanf("%x", &addr);
            CLEAR_STDIN;

            while(1)
            {
                DMSG1("Input the number of reading (up to %d:",
                      MAX_NUM_MTV_REG_READ_BUF);
                scanf("%d" , &read_cnt);
                CLEAR_STDIN;
                if(read_cnt <= MAX_NUM_MTV_REG_READ_BUF)
                    break;
            }

            ioctl_reg_access_info.page = page;
            ioctl_reg_access_info.addr = addr;
            ioctl_reg_access_info.read_cnt = read_cnt;

            DMSG3("\t[INPUT] %s[0x%02X]: %u\n", page_str[page], addr, read_cnt);

            if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_BURST_READ, &ioctl_reg_access_info) == 0)
            {
                for(i = 0; i < read_cnt; i++, addr++)
                    DMSG3("%s[0x%02X]: 0x%02X\n",
                          page_str[page], addr,
                          ioctl_reg_access_info.read_data[i]);
            }
            else
                EMSG0("IOCTL_TEST_REG_BURST_READ failed\n");
            break;

        case '3':
            DMSG0("[Reg IO] Write\n");

            show_register_page();
            while(1)
            {
                DMSG0("Select Page:");
                scanf("%x" , &page);
                CLEAR_STDIN;
                if(page <= 8)
                    break;
            }

            DMSG0("Input Address(hex) :,  data(hex) : ");
            scanf("%x" , &addr);
            CLEAR_STDIN;

            scanf("%x" , &write_data);
            CLEAR_STDIN;


            ioctl_reg_access_info.page = page;
            ioctl_reg_access_info.addr = addr;
            ioctl_reg_access_info.write_data = write_data;

            DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page], addr, write_data);

            if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_WRITE, &ioctl_reg_access_info) < 0)
                EMSG0("IOCTL_TEST_REG_WRITE failed\n");

            break;

        case 'q':
        case 'Q':
            goto REG_IO_TEST_EXIT;

        default:
            DMSG1("[%c]\n", key);
        }
        DMSG0("\n");
    }

REG_IO_TEST_EXIT:

    return;
}



int main(void)
{
    int key, ret;
    char name[32];

    dmb_thread_run = 0;
    tsp_read_thread_cb_ptr = NULL;

    dm_timer_thread_run = 0;
    dm_timer_thread_cb_ptr = NULL;

#if defined(TSIF_SAMSUNG_AP)
    tsif_run_flag = 0;
#endif

    /* Open driver */
    sprintf(name,"/dev/%s", RAONTV_DEV_NAME);
    fd_dmb_dev = open(name, O_RDWR);
    if(fd_dmb_dev < 0)
    {
        EMSG1("Can't not open %s\n", name);
        return -1;
    }

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
    sprintf(name,"/dev/%s", "tsif0");
    fd_tsif_dev = open(name, O_RDWR);
    if(fd_tsif_dev<0)
    {
        EMSG1("Can't not open %s\n", name);
        return -2;
    }
#endif

    mkdir(TS_DUMP_DIR, 0777);
    /* Create the reading thread. */
    if((ret = test_create_thread()) != 0)
    {
        EMSG1("test_create_thread() failed: %d\n", ret);
        goto APP_MAIN_EXIT;
    }

    while(1)
    {
        DMSG0("===============================================\n");
#ifdef RTV_TDMB_ENABLE
        DMSG0("\t1: TDMB Test\n");
#endif

#ifdef RTV_ISDBT_ENABLE
        DMSG0("\t2: ISDBT Test\n");
#endif

#ifdef RTV_DAB_ENABLE
        DMSG0("\t3: DAB Test\n");
#endif

#ifdef RTV_FM_ENABLE
        DMSG0("\t4: FM Test\n");
#endif
        DMSG0("\t5: GPIO Test\n");
        DMSG0("\t6: Register IO Test\n");
        DMSG0("\tq or Q: Quit\n");
        DMSG0("===============================================\n");

        fflush(stdin);
        key = getc(stdin);
        CLEAR_STDIN;

        switch( key )
        {
#ifdef RTV_TDMB_ENABLE
        case '1':
            test_TDMB();
            break;
#endif

#ifdef RTV_ISDBT_ENABLE
        case '2':
            test_ISDBT();
            break;
#endif

#ifdef RTV_DAB_ENABLE
        case '3':
            test_DAB();
            break;
#endif

#ifdef RTV_FM_ENABLE
        case '4':
            test_FM();
            break;
#endif
        case '5':
            test_GPIO();
            break;

        case '6':
            /* Power-up chip*/
            if(ioctl(fd_dmb_dev, IOCTL_TEST_MTV_POWER_ON) < 0)
                EMSG0("IOCTL_TEST_MTV_POWER_ON failed\n");
            else
                test_RegisterIO();

            if(ioctl(fd_dmb_dev, IOCTL_TEST_MTV_POWER_OFF) < 0)
            {
                EMSG0("IOCTL_TEST_MTV_POWER_OFF failed\n");
            }
            break;
        case 'q':
        case 'Q':
            goto APP_MAIN_EXIT;
        default:
            DMSG1("[%c]\n", key);
        }
    }

APP_MAIN_EXIT:

	test_delete_thread();

    close(fd_dmb_dev);

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
    close(fd_tsif_dev);
#endif

    return 0;
}
