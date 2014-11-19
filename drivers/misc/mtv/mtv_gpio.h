#ifndef __MTV_GPIO_H__
#define __MTV_GPIO_H__


#ifdef __cplusplus 
extern "C"{ 
#endif  

#include <linux/gpio.h>
#include <mach/gpio.h>

#if defined(CONFIG_ARCH_S5PV310)// for Hardkernel
#define PORT_CFG_OUTPUT                 1
#define MTV_PWR_EN                      S5PV310_GPD0(3)         
#define MTV_PWR_EN_CFG_VAL              S3C_GPIO_SFN(PORT_CFG_OUTPUT)
#define RAONTV_IRQ_INT                  S5PV310_GPX0(2)

static inline int mtv_configure_gpio(void)
{
	if(gpio_request(MTV_PWR_EN, "MTV_PWR_EN"))		
		DMBMSG("MTV_PWR_EN Port request error!!!\n");
	else	
	{
		// MTV_EN
		s3c_gpio_cfgpin(MTV_PWR_EN, MTV_PWR_EN_CFG_VAL);
		s3c_gpio_setpull(MTV_PWR_EN, S3C_GPIO_PULL_NONE);
		gpio_direction_output(MTV_PWR_EN, 0); // power down
	}

	return 0;
}

#elif defined(CONFIG_ARCH_S5PV210)//for MV210
#define S5PV210_GPD0_PWM_TOUT_0         (0x1 << 0)
#define MTV_PWR_EN                      S5PV210_GPD0(0) 
#define MTV_PWR_EN_CFG_VAL              S5PV210_GPD0_PWM_TOUT_0 
#define RAONTV_IRQ_INT                  IRQ_EINT6

static inline int mtv_configure_gpio(void)
{
	if(gpio_request(MTV_PWR_EN, "MTV_PWR_EN"))		
		DMBMSG("MTV_PWR_EN Port request error!!!\n");
	else	
	{
		// MTV_EN
		s3c_gpio_cfgpin(MTV_PWR_EN, MTV_PWR_EN_CFG_VAL);
		s3c_gpio_setpull(MTV_PWR_EN, S3C_GPIO_PULL_NONE);
		gpio_direction_output(MTV_PWR_EN, 0); // power down
	}

	return 0;
}

#elif defined(CONFIG_ARCH_MSM) //for msm
#define MTV_PWR_EN                      85
#define MTV_INT_NUM                     40
#define RAONTV_IRQ_INT                  gpio_to_irq(MTV_INT_NUM)
#define QRD8225Q_MTV_INT_NUM			28
#define QRD8225Q_RAONTV_IRQ_INT         gpio_to_irq(QRD8225Q_MTV_INT_NUM)

#define S5PV210_GPD0_PWM_TOUT_0       (0x1 << 0)
#define MTV_PWR_EN_CFG_VAL              S5PV210_GPD0_PWM_TOUT_0 

static inline int mtv_configure_gpio(void)
{
#if 0
    int rc = 0;
    rc = gpio_request(MTV_PWR_EN, "MTV_PWR_EN");
    if (rc < 0) {
        pr_err("%s: gpio_request---GPIO_SKU1_CAM_5MP_SHDN failed!", __func__);
		return rc;
    }

    rc = gpio_tlmm_config(GPIO_CFG(MTV_PWR_EN, 0,
            GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
            GPIO_CFG_2MA), GPIO_CFG_ENABLE);

    if (rc < 0) {
        pr_err("%s: unable to config MTV_PWR_EN!\n", __func__);
        gpio_free(MTV_PWR_EN);
		return rc;
    }

	gpio_direction_output(MTV_PWR_EN, 0); // power down
#endif

	return 0;
}

#else
	#error "code not present"
#endif

#ifdef __cplusplus 
} 
#endif 

#endif /* __MTV_GPIO_H__*/
