#include "test.h"


static const DAB_FREQ_TBL_INFO dab_korea_metropolitan_tbl[] =
{
	{"8A", 181280}, {"8B", 183008}, {"8C", 184736},
	{"12A", 205280}, {"12B", 207008}, {"12C", 208736}
};

static const DAB_FREQ_TBL_INFO dab_korea_national_tbl[] =
{
	{"7A", 175280}, {"7B", 177008}, {"7A", 178736},
	{"8A", 181280}, {"8B", 183008}, {"8C", 184736},
	{"9A", 187280}, {"9B", 189008}, {"9C", 190736},
	{"10A", 193280}, {"10B", 195008}, {"10C", 196736},
	{"11A", 199280}, {"11B", 201008}, {"11C", 202736},
	{"12A", 205280}, {"12B", 207008}, {"12C", 208736},
	{"13A", 211280}, {"13B", 213008}, {"13C", 214736}
};

static const DAB_FREQ_TBL_INFO dab_band3_tbl[]=
{
	{"5A", 174928}, {"5B", 176640}, {"5C", 178352}, {"5D", 180064},
	{"6A", 181936}, {"6B", 183648}, {"6C", 185360}, {"6D", 187072},
	{"7A", 188928}, {"7B", 190640}, {"7C", 192352}, {"7D", 194064},
	{"8A", 195936}, {"8B", 197648}, {"8C", 199360}, {"8D", 201072},
	{"9A", 202928}, {"9B", 204640}, {"9C", 206352}, {"9D", 208064},
	{"10A", 209936}, {"10N", 210096}, {"10B", 211648}, {"10C", 213360},
	{"10D", 215072}, {"11A", 216928}, {"11N", 217088}, {"11B", 218640},
	{"11C", 220352}, {"11D", 222064}, {"12A", 223936}, {"12N", 224096},
	{"12B", 225648}, {"12C", 227360}, {"12D", 229072}, {"13A", 230784},
	{"13B", 232496}, {"13C", 234208}, {"13D", 235776},{"13E", 237488},
	{"13F", 239200}
};		  

#ifdef RTV_DAB_LBAND_ENABLED
static const DAB_FREQ_TBL_INFO  dab_lband_tbl[]=
{
	{"LA", 1452960}, {"LB", 1454672}, {"LC", 1456384}, {"LD", 1458096},
	{"LE", 1459808}, {"LF", 1461520}, {"LG", 1463232}, {"LH", 1464944},
	{"LI", 1466656}, {"LJ", 1468368}, {"LK", 1470080}, {"LL", 1471792},
	{"LM", 1473504}, {"LN", 1475216}, {"LO", 1476928}, {"LP", 1478640},
	{"LQ", 1480352}, {"LR", 1482064}, {"LS", 1483776}, {"LT", 1485488},
	{"LU", 1487200}, {"LV", 1488912}, {"LW", 1490624}
};
#endif


static const ISDBT_FREQ_TBL_INFO isdbt_freq_tbl[2][69-14+1] =
{
	/* Japan */
	{
		{13, 473143}, {14, 479143}, {15, 485143}, {16, 491143}, {17, 497143}, {18, 503143},
		{19, 509143}, {20, 515143}, {21, 521143}, {22, 527143}, {23, 533143}, {24, 539143},
		{25, 545143}, {26, 551143}, {27, 557143}, {28, 563143}, {29, 569143}, {30, 575143},
		{31, 581143}, {32, 587143}, {33, 593143}, {34, 599143}, {35, 605143}, {36, 611143},
		{37, 617143}, {38, 623143}, {39, 629143}, {40, 635143}, {41, 641143}, {42, 647143},
		{43, 653143}, {44, 659143}, {45, 665143}, {46, 671143}, {47, 677143}, {48, 683143},
		{49, 689143}, {50, 695143}, {51, 701143}, {52, 707143}, {53, 713143}, {54, 719143},
		{55, 725143}, {56, 731143}, {57, 737143}, {58, 743143}, {59, 749143}, {60, 755143},
		{61, 761143}, {62, 767143}
	},

	/* Latin America */
	{
		{14, 473143}, {15, 479143}, {16, 485143}, {17, 491143}, {18, 497143}, {19, 503143},
		{20, 509143}, {21, 515143}, {22, 521143}, {23, 527143}, {24, 533143}, {25, 539143},
		{26, 545143}, {27, 551143}, {28, 557143}, {29, 563143}, {30, 569143}, {31, 575143},
		{32, 581143}, {33, 587143}, {34, 593143}, {35, 599143}, {36, 605143}, {37, 611143},
		{38, 617143}, {39, 623143}, {40, 629143}, {41, 635143}, {42, 641143}, {43, 647143},
		{44, 653143}, {45, 659143}, {46, 665143}, {47, 671143}, {48, 677143}, {49, 683143},
		{50, 689143}, {51, 695143}, {52, 701143}, {53, 707143}, {54, 713143}, {55, 719143},
		{56, 725143}, {57, 731143}, {58, 737143}, {59, 743143}, {60, 749143}, {61, 755143},
		{62, 761143}, {63, 767143}, {64, 773143}, {65, 779143}, {66, 785143}, {67, 791143},
		{68, 797143}, {69, 803143}
	},
};


/*============== DAB ====================================*/
BOOL is_valid_dab_freq(unsigned int ch_freq_khz)
{
	int i, num_ch;
	
	num_ch = sizeof(dab_korea_national_tbl) / sizeof(DAB_FREQ_TBL_INFO);
	for(i=0; i<num_ch; i++)
	{
		if(ch_freq_khz == dab_korea_national_tbl[i].freq)
			return TRUE;
	}

	// Check Band III
	num_ch = sizeof(dab_band3_tbl) / sizeof(DAB_FREQ_TBL_INFO);
	for(i=0; i<num_ch; i++)
	{
		if(ch_freq_khz == dab_band3_tbl[i].freq)
			return TRUE;
	}

#ifdef RTV_DAB_LBAND_ENABLED
	// Check L-Band
	num_ch = sizeof(dab_lband_tbl) / sizeof(DAB_FREQ_TBL_INFO);
	for(i=0; i<num_ch; i++)
	{
		if(ch_freq_khz == dab_lband_tbl[i].freq)
			return TRUE;
	}
#endif

	return FALSE;
}


const DAB_FREQ_TBL_INFO *get_dab_freq_table_from_user(unsigned int *num_freq)
{
	int key;
	const DAB_FREQ_TBL_INFO *freq_tbl_ptr;
		
	while(1)
	{
		DMSG0("=========== Select DAB Frequency ============\n");
		DMSG0("\t0: Korea Metropolitan\n");
		DMSG0("\t1: Korea National\n");
		DMSG0("\t2: DAB Band III\n");
#ifdef RTV_DAB_LBAND_ENABLED		
		DMSG0("\t3: DAB L-Band\n");
#endif
		DMSG0("\tq or Q: return\n");
		DMSG0("======================================\n");

		key = getc(stdin);				
		CLEAR_STDIN;

		switch(key)
		{
			case '0' :
				*num_freq = sizeof(dab_korea_metropolitan_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_korea_metropolitan_tbl;
				return freq_tbl_ptr;

			case '1' :
				*num_freq = sizeof(dab_korea_national_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_korea_national_tbl;
				return freq_tbl_ptr;

			case '2' :
				*num_freq = sizeof(dab_band3_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_band3_tbl;
				return freq_tbl_ptr;

#ifdef RTV_DAB_LBAND_ENABLED
			case '3' :
				*num_freq = sizeof(dab_lband_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_lband_tbl;
				return freq_tbl_ptr;
#endif

			case 'q':
			case 'Q':
				return NULL;
		}
	}

	return NULL;
}
/*============== end DAB ====================================*/

/*============== TDMB ====================================*/
BOOL is_valid_tdmb_freq(unsigned int ch_freq_khz)
{
	int i, num_ch;
	
	num_ch = sizeof(dab_korea_national_tbl) / sizeof(DAB_FREQ_TBL_INFO);
	for(i=0; i<num_ch; i++)
	{
		if(ch_freq_khz == dab_korea_national_tbl[i].freq)
			return TRUE;
	}

	return FALSE;
}


const DAB_FREQ_TBL_INFO *get_tdmb_freq_table_from_user(unsigned int *num_freq)
{
	int key;
	const DAB_FREQ_TBL_INFO *freq_tbl_ptr;
		
	while(1)
	{
		DMSG0("=========== Select DAB Frequency ============\n");
		DMSG0("\t0: Korea Metropolitan\n");
		DMSG0("\t1: Korea National\n");
		DMSG0("\tq or Q: return\n");
		DMSG0("======================================\n");

		key = getc(stdin);				
		CLEAR_STDIN;

		switch(key)
		{
			case '0' :
				*num_freq = sizeof(dab_korea_metropolitan_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_korea_metropolitan_tbl;
				return freq_tbl_ptr;

			case '1' :
				*num_freq = sizeof(dab_korea_national_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_korea_national_tbl;
				return freq_tbl_ptr;

			case 'q':
			case 'Q':
				return NULL;
		}
	}

	return NULL;
}
/*============== end DAB ====================================*/


/*============== ISDBT ====================================*/
const ISDBT_FREQ_TBL_INFO *get_isdbt_freq_table_from_user(unsigned int *num_freq,
							unsigned int area_idx)
{
	const ISDBT_FREQ_TBL_INFO *freq_tbl_ptr;

	//*num_freq = sizeof(isdbt_freq_tbl[area_idx]) / sizeof(ISDBT_FREQ_TBL_INFO);
	if(area_idx == 0)
		*num_freq = 62-13+1;
	else
		*num_freq = 69-14+1;

	freq_tbl_ptr = isdbt_freq_tbl[area_idx];

	return freq_tbl_ptr;
}



/*============== end ISDBT ====================================*/




