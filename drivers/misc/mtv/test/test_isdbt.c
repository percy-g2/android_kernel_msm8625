#include "test.h"

#ifdef RTV_ISDBT_ENABLE

/*============================================================================
 * Configuration for File dump
 *===========================================================================*/
#define _ISDBT_MSC_FILE_DUMP_ENABLE /* for MSC data*/

/* MSC filename: /data/raontech/isdbt_msc_CH.ts */
#define ISDBT_DUMP_MSC_FILENAME_PREFIX	"isdbt_msc"


#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
static FILE *fd_isdbt_msc;
static char isdbt_msc_fname[64];
#endif


static BOOL isdbt_is_power_on;
static BOOL isdbt_is_start_ts;
static volatile BOOL is_isdbt_play_state;  // TRUE : Play State, FALSE: Scan State

static unsigned char isdbt_ts_buf[MAX_READ_TSP_SIZE];
#if defined(TSIF_QUALCOMM_AP)
static unsigned char isdbt_msm_pkt_buf[MSM_TSIF_PKT_BUF_SIZE];
#endif


static unsigned int isdbt_area_idx;
static const char *isdbt_area_str[] = {"Japan", "Latin America"};


//============ Start of file dump =============================
#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
static int isdbt_open_msc_dump_file(unsigned int ch_num)
{
    if(fd_isdbt_msc != NULL)
    {
        EMSG0("[MTV] Must close dump file before open the new file\n");
        return -1;
    }

    sprintf(isdbt_msc_fname, "%s/%s_%u.ts",
            TS_DUMP_DIR, ISDBT_DUMP_MSC_FILENAME_PREFIX, ch_num);

    if((fd_isdbt_msc=fopen(isdbt_msc_fname, "wb")) == NULL)
    {
        EMSG1("[MTV] Fail to open error: %s\n", isdbt_msc_fname);
        return -2;
    }

    DMSG1("[MTV] Opend MSC dump file: %s\n", isdbt_msc_fname);

    return 0;
}

static int isdbt_close_msc_dump_file(void)
{
    if(fd_isdbt_msc != NULL)
    {
        fclose(fd_isdbt_msc);
        fd_isdbt_msc = NULL;

        DMSG1("[MTV] Closed MSC dump file: %s\n", isdbt_msc_fname);
    }

    return 0;
}
#endif /* _ISDBT_MSC_FILE_DUMP_ENABLE */
//============ END of file dump =============================

static void isdbt_processing_av_ts(unsigned char *av_ts, unsigned int av_size)
{
    //DMSG5("\t AV Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", av_size, av_ts[0], av_ts[1], av_ts[2], av_ts[3]);

#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
    if(fd_isdbt_msc != NULL)
        fwrite(av_ts, sizeof(char), av_size, fd_isdbt_msc);
#endif

#ifdef _DEBUG_PERIODIC_TSP_STAT
    verify_video_tsp(av_ts, av_size);
#endif
}

#if 0
void isdbt_read(int dev)
{
    int len = 0;

    if(is_isdbt_play_state == TRUE)

        len = read(dev, isdbt_ts_buf, MAX_READ_TSP_SIZE);


    if(len > 0)
    {
//    	DMSG1("isdbt_read read len is %d\n", len);
#if defined(TSIF_QUALCOMM_AP)
        len = tsif_get_msm_hts_pkt(isdbt_msm_pkt_buf, isdbt_ts_buf, len);
        isdbt_processing_av_ts(isdbt_msm_pkt_buf, len);
#else
        isdbt_processing_av_ts(isdbt_ts_buf, len);
#endif
    }
    else
    {
        usleep(50 * 1000);
    }
}
#endif

void isdbt_read(int dev)
{
 int len = 0;

 if(is_isdbt_play_state == TRUE)
 {
//  DMSG1("isdbt_read(): is_isdbt_play_state= %d\n", is_isdbt_play_state);
  len = read(dev, isdbt_ts_buf, MAX_READ_TSP_SIZE);
//  DMSG1("isdbt_read(): len= %d\n", len);
 }

 if(len > 0)
 {
#if defined(TSIF_QUALCOMM_AP)
  len = tsif_get_msm_hts_pkt(isdbt_msm_pkt_buf, isdbt_ts_buf, len); 
  isdbt_processing_av_ts(isdbt_msm_pkt_buf, len);
#else 
  isdbt_processing_av_ts(isdbt_ts_buf, len);
#endif
 }
 else
 {
  usleep(50 * 1000);
 }
}

static int isdbt_get_tmcc_info(void)
{
    int ret;
    RTV_ISDBT_TMCC_INFO tmcc_info;

    ret = ioctl(fd_dmb_dev, IOCTL_ISDBT_GET_TMCC, &tmcc_info);
    if(ret < 0)
    {
        EMSG0("[ISDBT] IOCTL_ISDBT_GET_TMCC failed\n");
        return ret;
    }

    DMSG1("tmcc_info.eCodeRate = %d\n", tmcc_info.eCodeRate);
    DMSG1("tmcc_info.eGuard = %d\n", tmcc_info.eGuard);
    DMSG1("tmcc_info.eInterlv = %d\n", tmcc_info.eInterlv);
    DMSG1("tmcc_info.eModulation = %d\n", tmcc_info.eModulation);
    DMSG1("tmcc_info.eSeg = %d\n", tmcc_info.eSeg);
    DMSG1("tmcc_info.eTvMode = %d\n", tmcc_info.eTvMode);
    DMSG1("tmcc_info.fEWS = %d\n", tmcc_info.fEWS);

    return 0;
}


static int isdbt_check_signal_info(void)
{
    int ret;
    IOCTL_ISDBT_SIGNAL_INFO sig_info;
    unsigned int lock;

    ret = ioctl(fd_dmb_dev, IOCTL_ISDBT_GET_SIGNAL_INFO, &sig_info);
    if(ret < 0)
    {
        EMSG0("[ISDBT] IOCTL_ISDBT_GET_SIGNAL_INFO failed\n");
        return ret;
    }

    lock = (sig_info.lock_mask == RTV_ISDBT_CHANNEL_LOCK_OK) ? 1 : 0;

    DMSG0("\t########## [ISDBTSignal Inforamtions] ##############\n");
    DMSG1("\t# LOCK: %u (1:LOCK, 0: UNLOCK)\n", lock);
    DMSG1("\t# Antenna Level: %u\n", sig_info.ant_level);
    DMSG1("\t# ber: %f\n", (float)sig_info.ber/RTV_ISDBT_BER_DIVIDER);
    DMSG1("\t# cnr: %f\n", (float)sig_info.cnr/RTV_ISDBT_CNR_DIVIDER);
//    DMSG1("\t# rssi: %f\n", (float)sig_info.rssi/RTV_ISDBT_RSSI_DIVIDER);
    DMSG1("\t# per: %u\n", sig_info.per);
    DMSG0("\t###################################################\n");

    return 0;
}

static int isdbt_check_lock_status(void)
{
    unsigned int lock_mask;

    if(ioctl(fd_dmb_dev, IOCTL_ISDBT_GET_LOCK_STATUS, &lock_mask) == 0)
        DMSG1("lock_mask = %d\n", lock_mask);
    else
        EMSG0("[ISDBT] IOCTL_ISDBT_GET_LOCK_STATUS failed\n");

    DMSG0("\n");

    return 0;
}

static int isdbt_disable_ts(void)
{
    if(isdbt_is_start_ts == FALSE)
        return 0;

    if(ioctl(fd_dmb_dev, IOCTL_ISDBT_STOP_TS) != 0)
    {
        EMSG0("[ISDBT] IOCTL_ISDBT_STOP_TS failed\n");
        return -1;
    }

#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
    isdbt_close_msc_dump_file();
#endif

#if defined(TSIF_SAMSUNG_AP)
    tsif_run(0);
#endif

    isdbt_is_start_ts = FALSE;

    return 0;
}


static int isdbt_enable_ts(void)
{
    if(isdbt_is_start_ts == TRUE)
        return 0;

#if defined(TSIF_SAMSUNG_AP)
    tsif_run(1);
#endif

    if(ioctl(fd_dmb_dev, IOCTL_ISDBT_START_TS) != 0)
    {
        EMSG0("[ISDBT] IOCTL_ISDBT_START_TS failed\n");
        return -1;
    }

    isdbt_is_start_ts = TRUE;

    return 0;
}


static int isdbt_set_channel(unsigned int ch_num)
{
    if(ch_num == mtv_prev_channel)
    {
        DMSG1("[ISDBT] Already opened channed ID(%d)\n", ch_num);
        return 0;
    }

    isdbt_disable_ts();

#ifdef _DEBUG_PERIODIC_TSP_STAT
    init_tsp_statistics();
#endif

#if defined(_DEBUG_PERIODIC_SIG_INFO) || defined(_DEBUG_PERIODIC_TSP_STAT)
    resume_dm_timer();
#endif

    if(ioctl(fd_dmb_dev, IOCTL_ISDBT_SET_FREQ, &ch_num) < 0)
    {
        EMSG0("[ISDBT] IOCTL_ISDBT_SET_FREQ failed\n");
        return -2;
    }

    mtv_prev_channel = ch_num;
    is_isdbt_play_state = TRUE;

    EMSG0("[ISDBT] is set to TRUE\n");


#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
    isdbt_open_msc_dump_file(ch_num);
#endif

    DMSG0("\n");

    return 0;
}

static void isdbt_decode_channel_info(unsigned char *ts, unsigned int size)
{
    //DMSG5("\t Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", size, ts[0], ts[1], ts[2], ts[3]);

#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
    if(fd_isdbt_msc != NULL)
        fwrite(ts, sizeof(char), size, fd_isdbt_msc);
#endif
}

static int isdbt_full_scan_freq(void)

{

    int ret;

    unsigned int num_ch, i, len;

    const ISDBT_FREQ_TBL_INFO *freq_tbl_ptr;

    double scan_end_time;

    is_isdbt_play_state = FALSE;

    freq_tbl_ptr = get_isdbt_freq_table_from_user(&num_ch, isdbt_area_idx);

    time_elapse();

    do
    {
        DMSG3("[ISDBT %s] Scan(%u: %u)\n",
              isdbt_area_str[isdbt_area_idx], freq_tbl_ptr->ch, freq_tbl_ptr->freq);
//        mtv_prev_channel = freq_tbl_ptr->ch;

#if defined(TSIF_SAMSUNG_AP)
        tsif_run(1);
#endif

        ret = ioctl(fd_dmb_dev, IOCTL_ISDBT_SCAN_FREQ, &freq_tbl_ptr->ch);
        if(ret == 0)
        {
            DMSG1("[ISDBT] Scan Detected (%u)\n", freq_tbl_ptr->ch);
            /* If scanning channel is detected, enable TS stream-out */
            isdbt_enable_ts();

            /* Read and decode TS data until acquiring the information of channel. */
            for(i = 0; i < 20; i++) // TEMP!!!!!
            {
                len = read(get_read_device_fd(), isdbt_ts_buf, MAX_READ_TSP_SIZE);
                if(len > 0)
                {

#if defined(TSIF_QUALCOMM_AP)
                    len = tsif_get_msm_hts_pkt(isdbt_msm_pkt_buf, isdbt_ts_buf, len);
                    isdbt_decode_channel_info(isdbt_msm_pkt_buf, len);
#else
                    isdbt_decode_channel_info(isdbt_ts_buf, len);
#endif
                }
                else
                    usleep(50 * 1000);
            }

            /* Disable TS stream-out */
            isdbt_disable_ts();
        }
        else
        {
            if(ret == IOTCL_SCAN_NOT_DETECTED_RET)
                DMSG1("[ISDBT] Scan NOT dectected (%u)\n",
                      freq_tbl_ptr->ch);
            else
                DMSG2("[ISDBT] Scan Devcie Error (%u): %d\n",
                      freq_tbl_ptr->ch, ret);
        }

#if defined(TSIF_SAMSUNG_AP)
        tsif_run(0);
#endif
        DMSG0("\n");
        freq_tbl_ptr++;

    }
    while(--num_ch != 0);
    scan_end_time = time_elapse();
    DMSG1("Total scan time: %f\n\n", scan_end_time);
    return 0;

}

static void isdbt_dm_timer_handler(void)
{
#ifdef _DEBUG_PERIODIC_SIG_INFO
    isdbt_check_signal_info();
#endif

#ifdef _DEBUG_PERIODIC_TSP_STAT
    show_video_tsp_statistics();
#endif
    DMSG0("\n");
}


static int isdbt_power_up(void)
{
    int ret;
    E_RTV_COUNTRY_BAND_TYPE country_band_type;

    if(isdbt_is_power_on == TRUE)
        return 0;

    if(isdbt_area_idx == 0)
        country_band_type = RTV_COUNTRY_BAND_JAPAN;
    else
        country_band_type = RTV_COUNTRY_BAND_BRAZIL;

    if((ret = ioctl(fd_dmb_dev, IOCTL_ISDBT_POWER_ON, &country_band_type)) < 0)
    {
        EMSG1("[ISDBT] IOCTL_ISDBT_POWER_ON failed: %d\n", ret);
        return ret;
    }

    isdbt_is_power_on = TRUE;
    isdbt_is_start_ts = FALSE;
    is_isdbt_play_state = FALSE;

    mtv_prev_channel = 0;

#if defined(_DEBUG_PERIODIC_SIG_INFO) || defined(_DEBUG_PERIODIC_TSP_STAT)
    def_dm_timer(isdbt_dm_timer_handler);
#endif

#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
    fd_isdbt_msc = NULL;
#endif

    /* Test */
#if 0
    {
        unsigned int ch_num;

        ch_num = 13;
        isdbt_set_channel(ch_num);
    }
#endif

#if 0
    {
        int i;
        IOCTL_ISDBT_SIGNAL_INFO sig_info;

        for (i=0; i<50; i++)
        {
            /*
            		unsigned char value;
            		EMSG0("test read i2c\n");

            		ret = ioctl(fd_dmb_dev, IOCTL_ISDBT_GET_SIGNAL_INFO, &sig_info);
            		if(ret < 0)
            		{
            			EMSG0("[ISDBT] IOCTL_ISDBT_GET_SIGNAL_INFO failed\n");
            			return ret;
            		}

            #define IOCTL_TEST_I2C_READ		_IOW(RAONTV_IOC_MAGIC, 6, IOCTL_REG_ACCESS_INFO)
            #define IOCTL_TEST_I2C_WRITE		_IOW(RAONTV_IOC_MAGIC, 7, IOCTL_REG_ACCESS_INFO)
            */
            if(ioctl(fd_dmb_dev, IOCTL_TEST_I2C_READ) < 0)
                EMSG0("IOCTL_TEST_REG_WRITE failed\n");

//		usleep(100 * 1000);
            sleep(1);
        }
    }
#endif

    return 0;
}

static int isdbt_power_down(void)
{
    if(isdbt_is_power_on == FALSE)
        return 0;

#if defined(_DEBUG_PERIODIC_SIG_INFO) || defined(_DEBUG_PERIODIC_TSP_STAT)
    suspend_dm_timer();
#endif

    if(ioctl(fd_dmb_dev, IOCTL_ISDBT_POWER_OFF) < 0)
    {
        EMSG0("[ISDBT] IOCTL_ISDBT_POWER_OFF failed\n");
    }


#ifdef _ISDBT_MSC_FILE_DUMP_ENABLE
    isdbt_close_msc_dump_file();
#endif

#if defined(TSIF_SAMSUNG_AP)
    tsif_run(0);
#endif

    isdbt_is_power_on = FALSE;
    isdbt_is_start_ts = FALSE;

    return 0;
}

static unsigned int get_isdbt_channel_from_user(void)
{
    unsigned int ch_num;

    while(1)
    {
        if(isdbt_area_idx == 0)
            DMSG1("Input %s Channel Number (ex: 13 ~ 62):", isdbt_area_str[isdbt_area_idx]);
        else
            DMSG1("Input %s Channel number (ex: 14 ~ 69):", isdbt_area_str[isdbt_area_idx]);

        scanf("%u", &ch_num);
        CLEAR_STDIN;

        if(isdbt_area_idx == 0)
        {
            if((ch_num >= 13) && (ch_num <= 62))
                break;
        }
        else
        {
            if((ch_num >= 14) && (ch_num <= 69))
                break;
        }

        EMSG0("[ISDBT] Invalid Channel Number\n");
    }

    return ch_num;
}


static void isdbt_sub_func(void)
{
    int key;
    unsigned int ch_num;

    isdbt_is_power_on = FALSE;

    while(1)
    {
        DMSG0("===============================================\n");
        DMSG1("\t0: %s ISDBT Power ON\n", isdbt_area_str[isdbt_area_idx]);
        DMSG0("\t1: ISDBT Power OFF\n");

        DMSG0("\t2: ISDBT Scan freq\n");
        DMSG0("\t3: ISDBT Set Channel\n");
        DMSG0("\t4: ISDBT Start TS\n");
        DMSG0("\t5: ISDBT Stop TS\n");
        DMSG0("\t6: ISDBT Get Lockstatus\n");
        DMSG0("\t7: ISDBT Get Signal Info\n");
        DMSG0("\t8: 1seg Get TMCC\n");
        DMSG0("\t9: [TEST] Register IO Test\n");

        DMSG0("\tq or Q: Quit\n");
        DMSG0("===============================================\n");

        key = getc(stdin);
        CLEAR_STDIN;

        if((key >= '2') && (key <= '9'))
        {
            if(isdbt_is_power_on == FALSE)
            {
                DMSG0("Power Down state!Must Power ON\n");
                continue;
            }
        }

        switch( key )
        {
        case '0':
            DMSG0("[ISDBT Power ON]\n");
            isdbt_power_up();

            /// Test
#if 0
            {
                IOCTL_REG_ACCESS_INFO ioctl_reg_access_info;

                ioctl_reg_access_info.page = 1;
                ioctl_reg_access_info.addr = 0x50;
                ioctl_reg_access_info.write_data = 0x88;


                //DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page], addr, write_data);
                if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_WRITE, &ioctl_reg_access_info) < 0)
                    EMSG0("IOCTL_TEST_REG_BURST_READ failed\n");


                ioctl_reg_access_info.page = 1;
                ioctl_reg_access_info.addr = 0x27;
                ioctl_reg_access_info.write_data = 0x56;

                //DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page], addr, write_data);
                if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_WRITE, &ioctl_reg_access_info) < 0)
                    EMSG0("IOCTL_TEST_REG_BURST_READ failed\n");

            }
#endif
            break;

        case '1':
            DMSG0("[ISDBT Power OFF]\n");
            isdbt_power_down();
            break;

        case '2':
            DMSG0("[ISDBT Scan freq]\n");
            isdbt_full_scan_freq();
            break;

        case '3':
            DMSG0("[ISDBT Set Channel]\n");
            ch_num = get_isdbt_channel_from_user();
            isdbt_set_channel(ch_num);
            break;

        case '4':
            DMSG0("[ISDBT Start TS]\n");
            isdbt_enable_ts();
            break;

        case '5':
            DMSG0("[ISDBT Stop TS]\n");
            isdbt_disable_ts();
            break;

        case '6':
            DMSG0("[ISDBT Get Lockstatus]\n");
            isdbt_check_lock_status();
            break;

        case '7':
            DMSG0("[ISDBT Get Singal Info]\n");
            isdbt_check_signal_info();
            break;

        case '8':
            DMSG0("[ISDBT Get TMCC Info]\n");
            isdbt_get_tmcc_info();
            break;

        case '9':
            test_RegisterIO();
            break;

        case 'q':
        case 'Q':
            goto ISDBT_SUB_EXIT;

        default:
            DMSG1("[%c]\n", key);
        }

        DMSG0("\n");
    }

ISDBT_SUB_EXIT:

    isdbt_power_down();

    DMSG0("ISDBT SUB EXIT\n");

    return;
}



void test_ISDBT()
{
    int key;

    mtv_tv_mode = MTV_MODE_1SEG;

    while(1)
    {
        DMSG0("============== [Select ISDBT Area] ==================\n");
        DMSG0("\t0: ISDBT Japan\n");
        DMSG0("\t1: ISDBT Latin America\n");
        DMSG0("\tq or Q: Quit\n");
        DMSG0("===============================================\n");

        key = getc(stdin);
        CLEAR_STDIN;

        switch( key )
        {
        case '0':
            DMSG0("[ISDBT] Japan Selected.\n");
            isdbt_area_idx = 0;
            isdbt_sub_func();
            break;

        case '1':
            DMSG0("[ISDBT] Latin America Selected.\n");
            isdbt_area_idx = 1;
            isdbt_sub_func();
            break;
        case 'q':
        case 'Q':
            goto ISDBT_EXIT;

        default:
            DMSG1("[%c]\n", key);
        }
    }

ISDBT_EXIT:

    DMSG0("ISDBT EXIT\n");

    return;
}

#endif /* #ifdef RTV_ISDBT_ENABLE */

