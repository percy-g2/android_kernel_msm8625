/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "msm.h"
#define SENSOR_NAME "tcm9516md"
#define PLATFORM_DRIVER_NAME "msm_camera_tcm9516md"
#define tcm9516md_obj tcm9516md_##obj

#ifdef CDBG
#undef CDBG
#endif
#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif

#define TCM9516MD_VERBOSE_DGB

#ifdef TCM9516MD_VERBOSE_DGB
#define CDBG(fmt, args...) printk(fmt, ##args)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define CDBG_HIGH(fmt, args...) printk(fmt, ##args)
#endif

static struct msm_sensor_ctrl_t tcm9516md_s_ctrl;
static int is_tcm9516md_ac = 0;

DEFINE_MUTEX(tcm9516md_mut);

static struct msm_camera_i2c_reg_conf tcm9516md_start_settings[] = {
	{0x0100, 0xe0},  /* streaming on */
};

static struct msm_camera_i2c_reg_conf tcm9516md_stop_settings[] = {
	{0x0100, 0xe0},  /* streaming off*/
};

static struct msm_camera_i2c_reg_conf tcm9516md_groupon_settings[] = {
	{0x0100, 0xe8},
};

static struct msm_camera_i2c_reg_conf tcm9516md_groupoff_settings[] = {
	{0x0100, 0xe0},
};

static struct msm_camera_i2c_reg_conf tcm9516md_prev_settings[] = {
//TAB_NAME: Preview-1304x980-28.9fps	
//{0x0100,0x88},//MODE_SEL/VREVON/HREVON/SWRST/GRHOLD/-/-/OUT_FORMAT;
	{0x0300,0x40},//-/PIXCKDIV[2:0]/-/SYSCKDIV[2:0];
	{0x0301,0x01},//-/-/-/-/-/-/PRECKDIV[1:0];
	{0x0302,0x01},//-/-/-/-/-/OPSYSDIV[2:0];
	{0x0303,0x00},//-/-/-/-/-/-/-/PLLMULT[8];
	{0x0304,0x41},//PLLMULT[7:0];
	{0x0305,0x04},//TTLINE[15:8];
	{0x0306,0xC9},//TTLINE[7:0];
	{0x0307,0x0A},//TTLDOT[15:8];
	{0x0308,0xC0},//TTLDOT[7:0];
	{0x030D,0x00},//-/-/-/-/HOUT_STADR[11:8];
	{0x030E,0x0C},//HOUT_STADR[7:0];
	{0x030F,0x00},//-/-/-/-/VOUT_STADR[11:8];
	{0x0310,0x0A},//VOUT_STADR[7:0];
	{0x0311,0x05},//-/-/-/-/HOUTSIZ[11:8];
	{0x0312,0x00},//HOUTSIZ[7:0];
	{0x0313,0x03},//-/-/-/-/VOUTSIZ[11:8];
	{0x0314,0xD4},//VOUTSIZ[7:0];
	{0x0315,0x81},//HANABIN/-/-/-/-/-/VMON[1:0];
	{0x0400,0x00},///-/-/-/-/-/-/-/HSCALE;
	{0x32C6,0x26},//TLPXPRD[3:0]/ CLKPREPRD[3:0];
	{0x32C7,0x63},//CLKTRIPRD[3:0]/ HSPREPRD[3:0];
	{0x32C8,0x55},//HS0PRD[3:0]/ HSTRLPRD[3:0];
// {0xa700, 0x01},
// {0xa701, 0xBF},
// {0xa702, 0x00},//0x14


	//{0x0100,0x80},//MODE_SEL/VREVON/HREVON/SWRST/GRHOLD/-/-/OUT_FORMAT;
};

static struct msm_camera_i2c_reg_conf tcm9516md_snap_settings[] = {
//TAB_NAME: Capture-2608x1960-14fps
//{0x0100,0x88},//MODE_SEL/VREVON/HREVON/SWRST/GRHOLD/-/-/OUT_FORMAT;
	{0x0300,0x40},//-/PIXCKDIV[2:0]/-/SYSCKDIV[2:0];
	{0x0301,0x01},//-/-/-/-/-/-/PRECKDIV[1:0];
	{0x0302,0x01},//-/-/-/-/-/OPSYSDIV[2:0];
	{0x0303,0x00},//-/-/-/-/-/-/-/PLLMULT[8];
	{0x0304,0x41},//PLLMULT[7:0];
	{0x0305,0x07},//TTLINE[15:8];
	{0x0306,0xE8},//TTLINE[7:0];
	{0x0307,0x0D},//TTLDOT[15:8];
	{0x0308,0x70},//TTLDOT[7:0];
	{0x030D,0x00},//-/-/-/-/HOUT_STADR[11:8];
	{0x030E,0x00},//HOUT_STADR[7:0];
	{0x030F,0x00},//-/-/-/-/VOUT_STADR[11:8];
	{0x0310,0x00},//VOUT_STADR[7:0];
	{0x0311,0x0A},//-/-/-/-/HOUTSIZ[11:8];
	{0x0312,0x30},//HOUTSIZ[7:0];
	{0x0313,0x07},//-/-/-/-/VOUTSIZ[11:8];
	{0x0314,0xA8},//VOUTSIZ[7:0];
	{0x0315,0x00},//HANABIN/-/-/-/-/-/VMON[1:0];
	{0x0400,0x00},///-/-/-/-/-/-/-/HSCALE;
	{0x32C6,0x25},//TLPXPRD[3:0]/ CLKPREPRD[3:0];
	{0x32C7,0x63},//CLKTRIPRD[3:0]/ HSPREPRD[3:0];
	{0x32C8,0x55},//HS0PRD[3:0]/ HSTRLPRD[3:0];
	//{0x0100,0x80},//MODE_SEL/VREVON/HREVON/SWRST/GRHOLD/-/-/OUT_FORMAT;
};

static struct msm_camera_i2c_reg_conf tcm9516md_video_60fps_settings[] = {
	
};

static struct msm_camera_i2c_reg_conf tcm9516md_video_90fps_settings[] = {
	
};

static struct msm_camera_i2c_reg_conf tcm9516md_zsl_settings[] = {
	
};

static struct msm_camera_i2c_reg_conf tcm9516md_recommend_settings[] = {
//TAB_NAME: Overall
{0x0000,0x10},//VERNUM[15:8];
{0x0001,0x00},//VERNUM[7:0];
{0x0002,0x00},//-/-/-/-/-/ANA_GA_MIN[11:8];
{0x0003,0x2C},//ANA_GA_MIN[7:0];
{0x0004,0x01},//-/-/-/-/-/ANA_GA_MAX[11:8];
{0x0005,0x60},//ANA_GA_MAX[7:0];
{0x0010,0x60},//PISO[15:8];
{0x0011,0x60},//PISO[7:0];
{0x0100,0xe0},//MODE_SEL/VREVON/HREVON/SWRST/GRHOLD/-/-/OUT_FORMAT;
{0x0200,0x00},//INTGTIM[15:8];
{0x0201,0x40},//INTGTIM[7:0];
{0x0202,0x00},//-/-/-/-/ANAGAIN[11:8];
{0x0203,0x2C},//ANAGAIN[7:0];
{0x0204,0x01},//-/-/-/-/-/-/MWBGAINGR[9:8];
{0x0205,0x00},//MWBGAINGR[7:0];
{0x0206,0x01},//-/-/-/-/-/-/MWBGAINR[9:8];
{0x0207,0x00},//MWBGAINR[7:0];
{0x0208,0x01},//-/-/-/-/-/-/MWBGAINB[9:8];
{0x0209,0x00},//MWBGAINB[7:0];
{0x020A,0x01},//-/-/-/-/-/-/MWBGAINGB[9:8];
{0x020B,0x00},//MWBGAINGB[7:0];
{0x0300,0x40},//-/PIXCKDIV[2:0]/-/SYSCKDIV[2:0];
{0x0301,0x02},//-/-/-/-/-/-/PRECKDIV[1:0];
{0x0302,0x01},//-/-/-/-/-/OPSYSDIV[2:0];
{0x0303,0x00},//-/-/-/-/-/-/-/PLLMULT[8];
{0x0304,0x41},//PLLMULT[7:0];
{0x0305,0x04},//TTLINE[15:8];
{0x0306,0xC9},//TTLINE[7:0];
{0x0307,0x0A},//TTLDOT[15:8];
{0x0308,0xC0},//TTLDOT[7:0];
{0x030D,0x00},//-/-/-/-/HOUT_STADR[11:8];
{0x030E,0x0C},//HOUT_STADR[7:0];
{0x030F,0x00},//-/-/-/-/VOUT_STADR[11:8];
{0x0310,0x0A},//VOUT_STADR[7:0];
{0x0311,0x05},//-/-/-/-/HOUTSIZ[11:8];
{0x0312,0x00},//HOUTSIZ[7:0];
{0x0313,0x03},//-/-/-/-/VOUTSIZ[11:8];
{0x0314,0xD4},//VOUTSIZ[7:0];
{0x0315,0x81},//HANABIN/-/-/-/-/-/VMON[1:0];
{0x0400,0x00},///-/-/-/-/-/-/-/HSCALE;
{0x0600,0x02},//TPAT_SEL[2:0]/(4)/(3)/(2)/ TPAT_R[9:8];
{0x0601,0xC0},//TPAT_R[7:0];
{0x0602,0x02},//(7)/(6)/(5)/(4)/(3)/(2)/ TPAT_GR[9:8];
{0x0603,0xC0},//TPAT_GR[7:0];
{0x0604,0x02},//(7)/(6)/(5)/(4)/(3)/(2)/ TPAT_B[9:8];
{0x0605,0xC0},//TPAT_B[7:0];
{0x0606,0x02},//(7)/(6)/(5)/(4)/(3)/(2)/ TPAT_GB[9:8];
{0x0607,0xC0},//TPAT_GB[7:0];
{0x0608,0x00},//(7)/(6)/(5)/(4)/ CURHW[11:8];
{0x0609,0x00},//CURHW[7:0];
{0x060A,0x00},//(7)/(6)/(5)/(4)/ CURHST[11:8];
{0x060B,0x00},//CURHST[7:0];
{0x060C,0x00},//(7)/(6)/(5)/(4)/(3)/ CORVW[10:8];
{0x060D,0x00},//CORVW[7:0];
{0x060E,0x00},//(7)/(6)/(5)/(4)/(3)/ CURVST[10:8];
{0x060F,0x00},//CURVST[7:0];
{0x3240,0xC2},//POSLFIX/ NEGLFIX/ NEGLEAKCUT/ POSBSTSEL/ NEGBSTCNT[3:0];
{0x3241,0x63},//(7)/ POSBSTCNT[2:0]/ (3)/ POSBSTHG[2:0];
{0x3242,0x35},//GDMOSBGREN/ (6)/ VSIGDRSEL[1:0]/ (3)/ POSBSTGA[2:0];
{0x3243,0x26},//GDMOSCNT[3:0]/ KBIASCNT[3:0];
{0x3244,0x22},//SPARE[1:0]/ DRADRV[1:0]/ DRADRV_PU/ DRADRVL[2:0];
{0x3245,0x3E},//S1CNT[3:0]/ CBIASIA[1:0]/ CBIASIB/ IDLOFFEN;
{0x3246,0x20},//VREFDLYCNT/ VREFSWG[1:0]/ (3)/ SENSEMODE[2:0];
{0x3247,0x0C},//LNOB_ON/ LNOBMODE/ EDGETESTEN[1:0]/ VOUT_SEL[3:0];
{0x3248,0x03},//VLAT_OFF/ (6)/(5)/(4)/ (3)/ (2)/ (1)/ VCO_CONV;
{0x3250,0x04},//BSC_OFF/ (6)/(5)/(4)/ SADR_1W[3:0];
{0x3251,0x84},//ESRST_D[3:0]/ SAME_WD/ ES_MODE/ RODATA_U/ ALLZEROSET;
{0x3252,0x3C},//(7)/ ESREAD_1W[6:0] ;
{0x3253,0x14},//(7)/ ESREAD_2U[6:0];
{0x3254,0x3C},//(7)/ ESREAD_2W[6:0];
{0x3255,0xC2},//(7)/ ZSET_U[3:0]/ ZSET_W[3:0];
{0x3256,0x87},//ZSET_W[4]/ RORST_W[6:0];
{0x3257,0x48},//RORST_U[3:0]/ RSTDRAIN3_U[3:0];
{0x3258,0x00},//STG_SPARE[7:0];
{0x3259,0x3C},//(7)/ ROREAD_W[6:0];
{0x325A,0x66},//RSTDRAIN2_U[3:0]/ RSTDRAIN3_D[3:0];
{0x325B,0x1B},//(7)/(6)/ VSIGPU_W[5:0];
{0x325C,0x80},//VSIGPU_U[3:0]/ VSIGDR_U[1:0]/ VSIGDR_D[1:0];
{0x325D,0x12},//(7)/ DRCUT_U[2:0]/ VSIGPU_EN/ (2)/ VSIGDR1EN/ SIGDR2EN;
{0x325E,0x47},//S1_1D[3:0]/ S1_2U[3:0];
{0x325F,0x68},//S1_2W[3:0]/ S1_2P/ S1_DMPOFF/ (1)/ CDS_STPBST;
{0x3260,0x78},//S3_W[3:0]/ S4_D[3:0];
{0x3261,0x05},//AD9BIT/ (6)/(5)/(4)/ IDLOFF_U[1:0]/ IDLOFF_MG[1:0];
{0x3262,0x40},//HPL2_SEL/ DRKCLIP_D[2:0]/ (3)/ INTEN_SU[2:0];
{0x3263,0xC2},//FBC_ON/ FBC_MODE[1:0]/ CPFBC_MODE[1:0]/ FBC_SUSP[1:0]/ FBC_START;
{0x3264,0x82},//CLAMPSFT_SEL[2:0]/ BLADJ_MODE[1:0]/ BLADJ_COEF[10:8];
{0x3265,0xAA},//BLADJ_COEF[7:0];
{0x3266,0x00},//EXT_CLP_ON/ FFBC/ (5)/(4)/ (3)/ S4IO/ OADJ_MODE[1:0];
{0x3267,0x00},//(7)/ (6)/(5)/(4)/ OADJON_START[3:0];
{0x3268,0x20},//OADJON_DLY[3:0]/ OADJON_WIDTH[3:0];
{0x3269,0x08},//ES_MARGIN[7:0];
{0x326A,0x40},//(7)/ BLEVEL[6:0];
{0x326B,0x34},//(7)/ (6)/(5)/ FBC_ENREST[1:0]/ FBC_RESTLV[3:0]	;
{0x326C,0x20},//(7)/ HOB_WIDTH[6:0];
{0x3270,0x30},//(7)/ LNNARROW/ HLNRSW/ PIXDANSA/ HOBPSTN[3:0];
{0x3271,0x00},//HOBNRON/ VOBNRON/ SEKI_DISP/ HOBLKLVL[4:0];
{0x3272,0x80},//VLNRSW/ DANSASW[1:0]/ VOBNRON/ VOBFIL/ (2)/ VOB_DISP/HOB_DISP;
{0x3273,0xC0},//WBPCMODE/ BBPCMODE/ (5)/(4)/ GAMMAOFF/ DRCDRE[1:0]/ ABCTH;
{0x3274,0x00},//BBPCLV[7:0];
{0x3275,0x18},//WBPCLV[7:0];
{0x3276,0x00},//HCBCGA0[3:0]/ HCBCGA1[3:0];
{0x3277,0x00},//PWBGAINR[7:0];
{0x3278,0x00},//PWBGAINGR[7:0];
{0x3279,0x00},//PWBGAINGB[7:0];
{0x327A,0x00},//PWBGAINB[7:0];
{0x327B,0x00},//HDCBLACK[7:0];
{0x327C,0x5F},//PWBBLNOFF/ HDCBLNOFF (5)/ HDCPOINT[4:0];
{0x327D,0x8F},//LSSCON /LSASIGN /LSQSIGN / (4) / HDSCLOPE[3:0];
{0x327E,0xFF},//LSHGA[3:0] / LSVGA[3:0];
{0x327F,0x25},//LSHOFS[7:0];
{0x3280,0x0C},//LSVOFS[7:0];
{0x3281,0x1E},//LSALGR[7:0];
{0x3282,0x1E},//LSALGB[7:0];
{0x3283,0x1C},//LSALR[7:0];
{0x3284,0x1E},//LSALB[7:0];
{0x3285,0x18},//LSARGR[7:0];
{0x3286,0x18},//LSARGB[7:0];
{0x3287,0x1B},//LSARR[7:0];
{0x3288,0x20},//LSARB[7:0];
{0x3289,0x14},//LSAUGR[7:0];
{0x328A,0x14},//LSAUGB[7:0];
{0x328B,0x19},//LSAUR[7:0];
{0x328C,0x15},//LSAUB[7:0];
{0x328D,0x0E},//LSADGR[7:0];
{0x328E,0x0E},//LSADGB[7:0];
{0x328F,0x12},//LSADR[7:0];
{0x3290,0x12},//LSADB[7:0];
{0x3291,0x4A},//LSBLGR[7:0];
{0x3292,0x4A},//LSBLGB[7:0];
{0x3293,0x4F},//LSBLR[7:0];
{0x3294,0x3B},//LSBLB[7:0];
{0x3295,0x46},//LSBRGR[7:0];
{0x3296,0x46},//LSBRGB[7:0];
{0x3297,0x4B},//LSBRR[7:0];
{0x3298,0x27},//LSBRB[7:0];
{0x3299,0x1E},//LSCUGR[7:0];
{0x329A,0x1E},//LSCUGB[7:0];
{0x329B,0x2F},//LSCUR[7:0];
{0x329C,0x1E},//LSCUB[7:0];
{0x329D,0x28},//LSCDGR[7:0];
{0x329E,0x28},//LSCDGB[7:0];
{0x329F,0x14},//LSCDR[7:0];
{0x32A0,0x1F},//LSCDB[7:0];
{0x32A1,0x14},//LSDLGR[7:0];
{0x32A2,0x14},//LSDLGB[7:0];
{0x32A3,0x5E},//LSDLR[7:0];
{0x32A4,0x14},//LSDLB[7:0];
{0x32A5,0x00},//LSDRGR[7:0];
{0x32A6,0x00},//LSDRGB[7:0];
{0x32A7,0x48},//LSDRR[7:0];
{0x32A8,0x50},//LSDRB[7:0];
{0x32A9,0x00},//LSEUGR[7:0];
{0x32AA,0x00},//LSEUGB[7:0];
{0x32AB,0x32},//LSEUR[7:0];
{0x32AC,0x09},//LSEUB[7:0];
{0x32AD,0x00},//LSEDGR[7:0];
{0x32AE,0x00},//LSEDGB[7:0];
{0x32AF,0x8A},//LSEDR[7:0];
{0x32B0,0x00},//LSEDB[7:0];
{0x32B1,0x00},//HNCAMP[2:0]/ HNCDET /(3)/ (2)/ (1)/ (0);
{0x32B2,0x00},//HNCLIM0[3:0]/ HNCLIM1[3:0];
{0x32B3,0x00},//PPRO_SPARE[7:0];
	{0x32B4,0x77},//{0x32B4,0x55},//NOISEMP0[3:0]/ NOISEMP1[3:0];
	{0x32B5,0x11},//{0x32B5,0x55},//EDMP0[3:0]/ EDMP1[3:0];
	{0x32B6,0x55},//{0x32B6,0xDD},//FLNZMP[3:0]/ GAINMP[3:0];
{0x32B7,0xFF},//ANAGMAX[7:0];
{0x32B8,0x00},//ANAGMIN[7:0];
{0x32C0,0x80},//PHYPWRON/(6)/TXOUT_MODE[1:0]/(3)/CLK_DELAY[2:0];
{0x32C1,0x66},//(7) / DA1_DELAY[2:0] / (3) / DA2_DELAY[2:0];
{0x32C2,0x80},//REGVD_SEL[1:0]/ (5)/ (4)/ DLREN_SEL/ MIPI1L / AUTO_R_SEL/ LPFR_SEL;
{0x32C3,0x44},//HS_SR_CNT/ LP_SR_CNT/ LB_LANE_SEL/ DEBUG_ON/ (3)/ PHASE_ADJUST[2:0;
{0x32C4,0x03},//(7)/(6)/(5)/(4)/(3)/(2)/ PARALLEL_OUT_SW[1:0];
{0x32C5,0x78},//ESCDATA[7:0];
{0x32C6,0x26},//TLPXPRD[3:0]/ CLKPREPRD[3:0];
{0x32C7,0x63},//CLKTRIPRD[3:0]/ HSPREPRD[3:0];
{0x32C8,0x55},//HS0PRD[3:0]/ HSTRLPRD[3:0];
{0x32C9,0xA7},//NUMWAKE[7:0];
{0x32CA,0x00},//HFCORROFF/ CLKUPLS / ESCREQ / FNUMRST / CLKMODE/ PARALLEL_MODE / F;
{0x32CB,0x00},//FIFODLY[7:0];
{0x32CC,0x11},//FS_CODE[7:0];
{0x32CD,0x44},//FE_CODE[7:0];
{0x32CE,0x22},//LS_CODE[7:0];
{0x32CF,0x33},//LE_CODE[7:0];
{0x32D0,0x30},//ESYNC_SW/ ESYNC_SET/ VSYNC_PH/ HSYNC_PH/ (3)/ (2)/ HCTR_STOP/VCTR_;
{0x32D1,0x00},//VPSET[15:8];
{0x32D2,0x01},//VPSET[7:0];
{0x32D3,0x00},//(7)/ (6)/ HPSET[13:8];
{0x32D4,0x00},//HPSET[7:0];
{0x32D5,0x00},//LATCH_TEST[1:0],,DAC_TEST,GAIN_TEST,AG_TEST/ (2)/ (1)/ (0);
{0x32D6,0x00},//DACT_INT[7:0];
{0x32D7,0x10},//DACT_STEP[3:0]/ (3)/ (2)/ DACT_SWD[1:0];
{0x32D8,0xFF},//DACS_MAX[7:0];
{0x32D9,0x00},//DACS_INT[7:0];
{0x32DA,0x10},//DACS_STEP[3:0](3)/ (2)/ DACS_SWD[1:0];
{0x32DB,0xFF},//DACS_MAX[7:0];
{0x32DC,0x81},//OADJON_SW/ OADJ_ON/ OADJ_SET/ EXT_OFS_ON/ EXT_DACPARA[11:8];
{0x32DD,0x00},//EXT_DACPARA[7:0];
{0x32DE,0x00},//SIGIN_ON/ ROREAD_OFF/ RSTDRAIN_HI/ DRCUT_HI/ DRCUT_OFF/ IDLOFF_OFF;
{0x32DF,0x00},//ESREAD_1OFF/ ESREAD_2OFF/ ESRST_1OFF/ ESRST_2OFF/ RORST_1OFF/ RORS;
{0x32E0,0x01},//DRESET_OFF/ FTLSNS_1OFF/ (5)/ SADR_1OFF/ (3)/ RST_1OFF/ (1)/ BOOST;
{0x32E1,0x00},//S1_1OFF/ S1_2OFF/ S3_OFF/ S4_OFF/ INTEN_1OFF/ INTRS_1OFF/ INTEN_2O;
{0x32E2,0x00},//FBC_PARAM[1:0]/ (5)/ (4)/ (3)/ (2)/ (1)/ (0);
{0x32E3,0x00},//(7)/ OBAVE[6:0];
{0x32E4,0x00},//(7)/ (6)/ (5)/ (4)/ OFFSET_ST[11:8],;
{0x32E5,0x00},//OFFSET_ST[7:0];
{0x32E6,0x00},//(7)/ (6)/ PSCLAMP_ST[13:8];
{0x32E7,0x00},//PSCLAMP_ST[7:0];
{0x32E8,0x00},//(7)/ (6)/ CLAMP_ST[13:8];
{0x32E9,0x00},//CLAMP_ST[7:0];
{0x32F0,0x90},//BIAS_SEL/ BSTREADEV/ RSTVDSEL/ READVDSEL/ SYSCLKOFF/ PLLCNTL[2:0];
{0x32F1,0x00},//ANAMON1_SEL[3:0]/ TEST_PHY1/ TEST_PHY2/ ANAMON0_SEL[1:0];
{0x32F2,0x55},//PCMODE/ ICP_PCH/ ICP_NCH/ VCO_STP_X/ TXEV_SEL/ RST_DIV_X/ (1)/ TCO;
{0x32F3,0x00},//VALUE1[7:0;
{0x32F4,0x00},//VALUE2[7:0];
{0x32F5,0x00},//HOB_DUMMY/ V_FULL_MODE/ VREG_TEST[1:0]/ TEST_LVDS[3:0];
{0x32F6,0x80},//I2C_DRVDN/ (6)/ (5)/ T_OUTSEL[4:0];
{0x32F7,0x80},//T_TMOSEL[3:0]/ DCLK_POL/ (2)/ (1)/ VCO_ON;
{0x3400,0x00},//(7)/(6)/(5)/(4)/(3)/GLBRST_POL/MSHUTTER_POL/STROBE_POL;
{0x3401,0x00},//(7)/(6)/(5)/(4)/(3)/GLBRST_SW/GLBRST_H_REG/GLBRST_SEL;
{0x3402,0x00},//GLBRST_WIDTH_A[7:0];
{0x3403,0x02},//(7)/(6)/(5)/(4)/GLBRST_WIDTH_B[3:0];
{0x3404,0x10},//GLBRST_WIDTH_C[7:0];
{0x3405,0x02},//GLBRST_WIDTH_D[7:0];
{0x3406,0x00},//(7)/(6)/(5)/(4)/GLBRST_WIDTH_F[11:8];
{0x3407,0x10},//GLBRST_WIDTH_F[7:0];
{0x3408,0x05},//GLBRST_WIDTH_H[7:0];
{0x3409,0x04},//GLBRST_WIDTH_J[7:0];
{0x340A,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/(1)/MSHUTTER_DLY[8];
{0x340B,0x00},//MSHUTTER_DLY[7:0];
{0x340C,0x04},//STROBE_RDLY[7:0];
{0x340D,0x04},//STROBE_FDLY[7:0];
{0x3410,0x2D},//(7)/GLBREAD_1W[6:0];
{0x3411,0x33},//(7)/GLBREAD_1D[6:0];
{0x3412,0x20},//(7)/GLBREAD_2W[6:0];
{0x3413,0x78},//(7)/GLBREAD_2D[6:0];
{0x3414,0x4E},//(7)/GLBTGRESET_W[6:0];
{0x3415,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/GLBMDCHG_0SET/GLBHREGCK_OFF;
{0x3420,0x70},//B_TPH_OFFSET[7:0];
{0x3421,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/(1)/TPAT_NOISE_EN;
{0x3422,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/DIN_SW[1:0];
{0x3423,0x00},//B_DIN_ADD[7:0];
{0x3424,0x07},//(7)/(6)/(5)/(4)/(3)/DH_OUT_SW/DL_OUT_SW/D_NOISE_SW;
{0x3425,0x00},//(7)/(6)/TPG_VLN2_SW/TPG_VLN_SW/TPBP_SW/TPG_RN_SW/D_DANSA_SW/TPG_HL;
{0x3426,0x00},//(7)/TPBP_PIX[2:0]/(3)/(2)/VLN_MP[1:0];
{0x3427,0x30},//B_RN_MP[7:0];
{0x3428,0x30},//B_HLN_MP[7:0];
{0x3429,0x30},//B_RDIN[7:0];
{0x342A,0x80},//B_B_DANSA[7:0];
{0x342B,0x80},//B_GB_DANSA[7:0];
{0x342C,0x80},//B_GR_DANSA[7:0];
{0x342D,0x80},//B_R_DANSA[7:0];
{0xA700,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/DAC_CODE[9:8];
{0xA701,0x00},//DAC_CODE[7:0];
	//{0xA702,0x00},//(7)/(6)/MODE[1:0]/(3)/STEP[2:0];
{0xA703,0x00},//SRST/(6)/(5)/(4)/(3)/(2)/(1)/(0);
{0xA704,0x00},//T_DAC_FAST/(6)/T_AMP_FAST/(4)/BGR_TST/(2)/DC_TST/(1);
{0xA705,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/(1)/DACOUT_P_TST;
{0xA706,0x00},//--;
{0xA707,0x03},//(7)/(6)/(5)/(4)/(3)/(2)/VCMCK_DIV[1:0];
{0xA708,0x00},//--;
{0xA709,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/(1)/PD_X;
{0xA70A,0x00},//(7)/(6)/TEST_RSW/ADCCNT[4:0];
{0xA70B,0x00},//(7)/(6)/(5)/(4)/(3)/(2)/ADCOUT[9:8];
	{0x3240,0xCA},
	{0x325D,0x11},
	{0x326A,0x40},
	{0x3272,0xF8},
};


static struct msm_camera_i2c_conf_array tcm9516md_init_conf[] = {
	{&tcm9516md_recommend_settings[0],
	ARRAY_SIZE(tcm9516md_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array tcm9516md_confs[] = {
	{&tcm9516md_snap_settings[0],
	ARRAY_SIZE(tcm9516md_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&tcm9516md_prev_settings[0],
	ARRAY_SIZE(tcm9516md_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&tcm9516md_video_60fps_settings[0],
	ARRAY_SIZE(tcm9516md_video_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&tcm9516md_video_90fps_settings[0],
	ARRAY_SIZE(tcm9516md_video_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&tcm9516md_zsl_settings[0],
	ARRAY_SIZE(tcm9516md_zsl_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_csi_params tcm9516md_csi_params = {
	.data_format = CSI_10BIT,
	.lane_cnt    = 2,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 14,
};

static struct v4l2_subdev_info tcm9516md_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t tcm9516md_dimensions[] = {
	{ /* For Snapshot*/
		.x_output = 2608,//0xA30
		.y_output = 1960,//0x7A8
		.line_length_pclk = 3440,//0xD70
		.frame_length_lines = 2024,//0x7E8
		.vt_pixel_clk = 97500000,
		.op_pixel_clk = 48000000,
		.binning_factor = 0,
	},
	{ /* For PREVIEW */
		.x_output = 0x500,//1306
		.y_output = 0x3D4,//980
		.line_length_pclk = 0xAC0,//2752
		.frame_length_lines = 0x4C9,//1225
		.vt_pixel_clk = 97500000,
		.op_pixel_clk = 48000000,
		.binning_factor = 1,
	},
	{ /* For 60fps */
		.x_output = 0x280,  /*640*/
		.y_output = 0x1E0,   /*480*/
		.line_length_pclk = 0x73C,
		.frame_length_lines = 0x1F8,
		.vt_pixel_clk = 56004480,
		.op_pixel_clk = 159408000,
		.binning_factor = 0x0,
	},
	{ /* For 90fps */
		.x_output = 0x280,  /*640*/
		.y_output = 0x1E0,   /*480*/
		.line_length_pclk = 0x73C,
		.frame_length_lines = 0x1F8,
		.vt_pixel_clk = 56004480,
		.op_pixel_clk = 159408000,
		.binning_factor = 0x0,
	},
	{ /* For ZSL */
		.x_output = 0xA30,  /*2608*/  /*for 5Mp*/
		.y_output = 0x7A0,   /*1952*/
		.line_length_pclk = 0xA8C,
		.frame_length_lines = 0x7B0,
		.vt_pixel_clk = 79704000,
		.op_pixel_clk = 159408000,
		.binning_factor = 0x0,
	},

};

static struct msm_sensor_output_reg_addr_t tcm9516md_reg_addr = {
	.x_output = 0x0311,
	.y_output = 0x0313,
	.line_length_pclk = 0x0307,  
	.frame_length_lines = 0x0305,
};

static struct msm_camera_csi_params *tcm9516md_csi_params_array[] = {
	&tcm9516md_csi_params, /* Snapshot */
	&tcm9516md_csi_params, /* Preview */
	&tcm9516md_csi_params, /* 60fps */
	&tcm9516md_csi_params, /* 90fps */
	&tcm9516md_csi_params, /* ZSL */
};

static struct msm_sensor_id_info_t tcm9516md_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x1000,
};

static struct msm_sensor_exp_gain_info_t tcm9516md_exp_gain_info = {
	.coarse_int_time_addr = 0x0200,
	.global_gain_addr = 0x0202,
	.vert_offset = 4,
};

void tcm9516md_sensor_reset_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_camera_i2c_write(
		s_ctrl->sensor_i2c_client,
		0x103, 0x1,
		MSM_CAMERA_I2C_BYTE_DATA);
}

#define MSB 1
#define LSB 0
inline uint8_t tcm9516md_byte(uint16_t word, uint8_t offset)
{
	return word >> (offset * BITS_PER_BYTE);
}

static int32_t tcm9516md_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	uint16_t min_ll_pck = 3440;  // From capture setting TTLDOT
	uint32_t ll_pck, fl_lines;
	uint32_t ll_ratio;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t ll_pck_msb, ll_pck_lsb;
	uint8_t offset;
	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}
	printk("tcm9516md_write_pict_exp_gain: the gain = %d,line = %d\n",gain,line);
	//curr_frame_length_lines 2024
	// fps_divider 1480
	fl_lines = s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider / Q10;
	ll_pck = s_ctrl->curr_line_length_pclk;//3440
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset)) {
		ll_ratio = (line * Q10) / (fl_lines - offset);
		ll_pck = ll_pck * ll_ratio / Q10;
		line = fl_lines - offset;
	}
	
	if (ll_pck < min_ll_pck)
	ll_pck = min_ll_pck;
	
	printk("tcm9516md_write_pict_exp_gain: the ll_pck = %d,line = %d\n",ll_pck,line);
	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);
	
	intg_time_msb = (uint8_t) ((line & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (line & 0x00FF);
	
	
	ll_pck_msb = (uint8_t) ((ll_pck & 0xFF00) >> 8);
	ll_pck_lsb = (uint8_t) (ll_pck & 0x00FF);
	
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->global_gain_addr,
	gain_msb,
	MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->global_gain_addr + 1,
	gain_lsb,
	MSM_CAMERA_I2C_BYTE_DATA);
	
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_output_reg_addr->line_length_pclk,
	ll_pck_msb,
	MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_output_reg_addr->line_length_pclk + 1,
	ll_pck_lsb,
	MSM_CAMERA_I2C_BYTE_DATA);
	
	/* Coarse Integration Time */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
	intg_time_msb,
	MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
	intg_time_lsb,
	MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	{
		uint16_t gain = 0;
		uint16_t line = 0;
		int32_t rc = -1;

	  rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
    0x0202,&gain, MSM_CAMERA_I2C_WORD_DATA);
		if(rc < 0)
			{
				printk("read gain error\n");
			}
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			0x0200,&line,MSM_CAMERA_I2C_WORD_DATA);
		if(rc < 0)
			{
				printk("read line error\n");
			}
		printk("the gain = %d and line = %d\n",gain,line);
	}
	
	return 0;
}

static int32_t tcm9516md_write_prev_exp_gain(struct msm_sensor_ctrl_t *s_ctrl, uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	int32_t rc = 0;
	uint32_t fl_lines, offset;
	fl_lines = s_ctrl->curr_frame_length_lines;
	pr_info("tcm9516md_write_prev_exp_gain :%d %d\n", gain, line);
	printk("the gain = %d,line = %d\n",gain,line);
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

#if 0
	{
		uint16_t temp700, temp701, temp702;
	  
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0xA700, &temp700,MSM_CAMERA_I2C_BYTE_DATA);
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0xA701, &temp701,MSM_CAMERA_I2C_BYTE_DATA); 
  	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0xA702, &temp702,MSM_CAMERA_I2C_BYTE_DATA); 

	  printk("TCM DCA Read: 0xA700: %x, 0xA701: %x, 0xA702: %x\n", temp700, temp701, temp702);
	}
#endif

	/* Analogue Gain */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->global_gain_addr,
	tcm9516md_byte(gain, MSB),
	MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->global_gain_addr + 1,
	tcm9516md_byte(gain, LSB),
	MSM_CAMERA_I2C_BYTE_DATA);
	
	printk("tcm9516md_write_prev_exp_gain :the fl_lines = %d\n",fl_lines);
	if (line > (fl_lines - offset)) {
		fl_lines = line + offset;
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines,
		tcm9516md_byte(fl_lines, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines + 1,
		tcm9516md_byte(fl_lines, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		tcm9516md_byte(line, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
		tcm9516md_byte(line, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	} else if (line < (fl_lines - offset)) {
		if (fl_lines < s_ctrl->curr_frame_length_lines)
		fl_lines = s_ctrl->curr_frame_length_lines;
		
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		tcm9516md_byte(line, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
		tcm9516md_byte(line, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines,
		tcm9516md_byte(fl_lines, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines + 1,
		tcm9516md_byte(fl_lines, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	} else {
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines,
		tcm9516md_byte(fl_lines, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines + 1,
		tcm9516md_byte(fl_lines, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		tcm9516md_byte(line, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
		tcm9516md_byte(line, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	}

	/*
	{
		uint16_t gain = 0;
		uint16_t line = 0;
		int32_t rc = -1;

	  rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
    0x0202,&gain, MSM_CAMERA_I2C_WORD_DATA);
		if(rc < 0)
			{
				printk("read gain error\n");
			}
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			0x0200,&line,MSM_CAMERA_I2C_WORD_DATA);
		if(rc < 0)
			{
				printk("read line error\n");
			}
		printk("the gain = %d and line = %d\n",gain,line);
	}
	*/
	return rc;
}

extern void camera_af_software_powerdown(struct i2c_client *client);
int32_t tcm9516md_sensor_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("%s IN\r\n", __func__);
	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	s_ctrl->sensor_i2c_addr = s_ctrl->sensor_i2c_addr;

	rc = msm_sensor_i2c_probe(client, id);

	if (client->dev.platform_data == NULL) {
		CDBG_HIGH("%s: NULL sensor data\n", __func__);
		return -EFAULT;
	}

	/* send software powerdown cmd to AF motor, avoid current leak */
	if(0 == rc)
	{
		camera_af_software_powerdown(client);
	}
	usleep_range(5000, 5100);

	return rc;
}

static const struct i2c_device_id tcm9516md_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&tcm9516md_s_ctrl},
	{ }
};

static struct i2c_driver tcm9516md_i2c_driver = {
	.id_table = tcm9516md_i2c_id,
	.probe  = tcm9516md_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client tcm9516md_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&tcm9516md_i2c_driver);
}

static struct v4l2_subdev_core_ops tcm9516md_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops tcm9516md_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops tcm9516md_subdev_ops = {
	.core = &tcm9516md_subdev_core_ops,
	.video  = &tcm9516md_subdev_video_ops,
};

int32_t tcm9516md_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;
	int rc = 0;
	printk("##### %s: E\n", __func__);
	
	//Stop stream first
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(10);

	gpio_direction_output(21, 0); // Front Cam PWD
	gpio_direction_output(20, 0); // Back Cam PWD
	
	gpio_direction_output(info->sensor_pwd, 0);
	//printk("%s line%d -- sensor_reset: %d\n",__func__,__LINE__,gpio_get_value_cansleep(info->sensor_reset));
	//gpio_direction_output(info->sensor_reset, 0);
	
	//printk("%s line%d -- sensor_reset: %d\n",__func__,__LINE__,gpio_get_value_cansleep(info->sensor_reset));
	//gpio_direction_output(7, 0);
	
	usleep_range(5000, 5100);
	msm_sensor_power_down(s_ctrl);
	printk("##### %s: X\n",__func__);	
	return rc;
}


int32_t tcm9516md_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;
	printk("##### %s: E\n",__func__);	



	gpio_direction_output(21, 0); // Front Cam PWD
	gpio_direction_output(20, 0); // Back Cam PWD

	usleep_range(5000, 5100);
	
	/* turn on ldo and vreg */
	rc = msm_sensor_power_up(s_ctrl);
	if (rc < 0) {
		CDBG("%s: msm_sensor_power_up failed\n", __func__);
		return rc;
	}
	
	
	gpio_direction_output(info->sensor_pwd, 1);
	msleep(20);
	printk("##### %s: X\n",__func__);
	return rc;
}

static void tcm9516md_sensor_reset_pwd(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	printk("##### %s: E\n",__func__);

	gpio_direction_output(21, 0); // Front Cam PWD
	gpio_direction_output(20, 0); // Back Cam PWD
	
	gpio_direction_output(info->sensor_pwd, 0);
	gpio_direction_output(info->sensor_reset, 0);
	usleep_range(5000, 6000);
	gpio_direction_output(info->sensor_pwd, 1);
	msleep(10);
	gpio_direction_output(info->sensor_reset, 1);
	return ;
}
int32_t tcm9516md_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
  uint16_t id = 0;
  int32_t rc = -1;
  CDBG("tcm9516md sensor read id\n");

  rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
    s_ctrl->sensor_id_info->sensor_id_reg_addr,
    &id, MSM_CAMERA_I2C_WORD_DATA);
  printk("tcm9516md read_id 0x%x : 0x%x\n", s_ctrl->sensor_id_info->sensor_id_reg_addr, id);
  
  if (id != s_ctrl->sensor_id_info->sensor_id) {
    tcm9516md_sensor_reset_pwd(s_ctrl);
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
    s_ctrl->sensor_id_info->sensor_id_reg_addr,
      &id, MSM_CAMERA_I2C_WORD_DATA);
    printk("tcm9516md read_id 0x%x : 0x%x\n", s_ctrl->sensor_id_info->sensor_id_reg_addr, id);
    if (id == s_ctrl->sensor_id_info->sensor_id) is_tcm9516md_ac = 1;
  }

  if (id != s_ctrl->sensor_id_info->sensor_id)
    return -ENODEV;

  CDBG("tcm9516md readid ok, success\n");

/*

 	{

	  uint16_t tempH;
	  
	  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3000,0x0001,MSM_CAMERA_I2C_BYTE_DATA);
	  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3002,0x0000,MSM_CAMERA_I2C_BYTE_DATA); // Page 0

      msleep(100);

	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3014,	&tempH, MSM_CAMERA_I2C_BYTE_DATA);
      printk("tcm9516md OTP Module ID: 0x%x\n", tempH);


      printk("tcm9516md OTP Module Production Information:\n");
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3017,	&tempH, MSM_CAMERA_I2C_BYTE_DATA);
      printk("%x ",tempH);
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3018,	&tempH, MSM_CAMERA_I2C_BYTE_DATA);
      printk("%x ",tempH);
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3019,	&tempH, MSM_CAMERA_I2C_BYTE_DATA);
      printk("%x ",tempH);
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x301A,	&tempH, MSM_CAMERA_I2C_BYTE_DATA);	  
      printk("%x\n",tempH);

	  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3000,0x0000,MSM_CAMERA_I2C_BYTE_DATA);
  
 		}
  


 	{

	  uint16_t tempH,tempL;

      msleep(100);
	  
	  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3000,0x0001,MSM_CAMERA_I2C_BYTE_DATA);
	  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3002,0x0001,MSM_CAMERA_I2C_BYTE_DATA); //Page 1

      msleep(100);

	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3004,	&tempH, MSM_CAMERA_I2C_BYTE_DATA);
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3005,	&tempL, MSM_CAMERA_I2C_BYTE_DATA);	  
      printk("tcm9516md OTP DAC Inf 0x%x\n", tempH*256+tempL);


	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3006,	&tempH, MSM_CAMERA_I2C_BYTE_DATA);
	  msm_camera_i2c_read(s_ctrl->sensor_i2c_client,0x3007,	&tempL, MSM_CAMERA_I2C_BYTE_DATA);	  
      printk("tcm9516md OTP DAC Macro 0x%x\n", tempH*256+tempL);


	  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x3000,0x0000,MSM_CAMERA_I2C_BYTE_DATA);
  
 		}

  */
  return rc;
}

static int32_t vfe_clk = 266667000;

int32_t tcm9516md_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;
	
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	if (csi_config == 0 || res == 0)
		msleep(66);
	else
		msleep(266);

	//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x4800, 0x25,MSM_CAMERA_I2C_BYTE_DATA);
	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x103, 0x1,MSM_CAMERA_I2C_BYTE_DATA);
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		csi_config = 0;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,s_ctrl->msm_sensor_reg->mode_settings, res);
		msleep(30);
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,NOTIFY_CSIC_CFG,s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(30);
			csi_config = 1;
			//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x100, 0x1,MSM_CAMERA_I2C_BYTE_DATA);
		}
		//msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x4800, 0x4,MSM_CAMERA_I2C_BYTE_DATA);
		msleep(266);
		if (res == MSM_SENSOR_RES_4)
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,NOTIFY_PCLK_CHANGE,&vfe_clk);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;
}

static struct msm_sensor_fn_t tcm9516md_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = tcm9516md_write_prev_exp_gain,
	.sensor_write_snapshot_exp_gain = tcm9516md_write_pict_exp_gain,
	.sensor_csi_setting = tcm9516md_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = tcm9516md_sensor_power_up,
	.sensor_power_down = tcm9516md_sensor_power_down,
	.sensor_match_id   = tcm9516md_sensor_match_id,
};

static struct msm_sensor_reg_t tcm9516md_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = tcm9516md_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(tcm9516md_start_settings),
	.stop_stream_conf = tcm9516md_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(tcm9516md_stop_settings),
	.group_hold_on_conf = tcm9516md_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(tcm9516md_groupon_settings),
	.group_hold_off_conf = tcm9516md_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(tcm9516md_groupoff_settings),
	.init_settings = &tcm9516md_init_conf[0],
	.init_size = ARRAY_SIZE(tcm9516md_init_conf),
	.mode_settings = &tcm9516md_confs[0],
	.output_settings = &tcm9516md_dimensions[0],
	.num_conf = ARRAY_SIZE(tcm9516md_confs),
};

static struct msm_sensor_ctrl_t tcm9516md_s_ctrl = {
	.msm_sensor_reg = &tcm9516md_regs,
	.sensor_i2c_client = &tcm9516md_sensor_i2c_client,
	.sensor_i2c_addr = 0x6c, 
	.sensor_output_reg_addr = &tcm9516md_reg_addr,
	.sensor_id_info = &tcm9516md_id_info,
	.sensor_exp_gain_info = &tcm9516md_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &tcm9516md_csi_params_array[0],
	.msm_sensor_mutex = &tcm9516md_mut,
	.sensor_i2c_driver = &tcm9516md_i2c_driver,
	.sensor_v4l2_subdev_info = tcm9516md_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(tcm9516md_subdev_info),
	.sensor_v4l2_subdev_ops = &tcm9516md_subdev_ops,
	.func_tbl = &tcm9516md_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Toshiba 5MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
