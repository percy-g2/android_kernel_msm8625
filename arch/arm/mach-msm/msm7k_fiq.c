/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/nmi.h>
#include <asm/fiq.h>
#include <asm/hardware/gic.h>
#include <asm/cacheflush.h>
#include <mach/irqs.h>
#include <mach/socinfo.h>
#include <asm/unwind.h>
#include <mach/fiq.h>

#include "msm_watchdog.h"

#define MODULE_NAME "MSM7K_FIQ"

struct msm_watchdog_dump msm_dump_cpu_ctx[NR_CPUS];
static int fiq_counter;
static int msm_fiq_no;
void *msm7k_fiq_stack[NR_CPUS];
static spinlock_t msm_fiq_lock;

/* Called from the FIQ asm handler */
void msm7k_fiq_handler(void)
{
	struct pt_regs ctx_regs;
	static cpumask_t fiq_cpu_mask;
	int this_cpu;
	unsigned long msm_fiq_flags;

	spin_lock_irqsave(&msm_fiq_lock, msm_fiq_flags);
	this_cpu = smp_processor_id();

	pr_info("%s: Fiq is received on CPU%d\n", __func__, this_cpu);
	fiq_counter += 1;

	pr_err("%s msm_dump_cpu_ctx[this_cpu] usr_r0:0x%x", __func__, msm_dump_cpu_ctx[this_cpu].usr_r0);
	pr_err("%s msm_dump_cpu_ctx[this_cpu] usr_r0:0x%x usr_r1:0x%x usr_r2:0x%x usr_r3:0x%x usr_r4:0x%x usr_r5:0x%x usr_r6:0x%x usr_r7:0x%x usr_r8:0x%x usr_r9:0x%x usr_r10:0x%x usr_r11:0x%x usr_r12:0x%x usr_r13:0x%x usr_r14:0x%x irq_spsr:0x%x irq_r13:0x%x irq_r14:0x%x svc_spsr:0x%x svc_r13:0x%x svc_r14:0x%x abt_spsr:0x%x abt_r13:0x%x abt_r14:0x%x und_spsr:0x%x und_r13:0x%x und_r14:0x%x fiq_spsr:0x%x fiq_r8:0x%x fiq_r9:0x%x fiq_r10:0x%x fiq_r11:0x%x fiq_r12:0x%x fiq_r13:0x%x fiq_r14:0x%x\n",__func__, msm_dump_cpu_ctx[this_cpu].usr_r0,msm_dump_cpu_ctx[this_cpu].usr_r1,msm_dump_cpu_ctx[this_cpu].usr_r2,msm_dump_cpu_ctx[this_cpu].usr_r3, msm_dump_cpu_ctx[this_cpu].usr_r4, msm_dump_cpu_ctx[this_cpu].usr_r5, msm_dump_cpu_ctx[this_cpu].usr_r6, msm_dump_cpu_ctx[this_cpu].usr_r7, msm_dump_cpu_ctx[this_cpu].usr_r8, msm_dump_cpu_ctx[this_cpu].usr_r9, msm_dump_cpu_ctx[this_cpu].usr_r10, msm_dump_cpu_ctx[this_cpu].usr_r11, msm_dump_cpu_ctx[this_cpu].usr_r12, msm_dump_cpu_ctx[this_cpu].usr_r13, msm_dump_cpu_ctx[this_cpu].usr_r14, msm_dump_cpu_ctx[this_cpu].irq_spsr, msm_dump_cpu_ctx[this_cpu].irq_r13, msm_dump_cpu_ctx[this_cpu].irq_r14, msm_dump_cpu_ctx[this_cpu].svc_spsr, msm_dump_cpu_ctx[this_cpu].svc_r13, msm_dump_cpu_ctx[this_cpu].svc_r14, msm_dump_cpu_ctx[this_cpu].abt_spsr,msm_dump_cpu_ctx[this_cpu].abt_r13, msm_dump_cpu_ctx[this_cpu].abt_r14, msm_dump_cpu_ctx[this_cpu].und_spsr,msm_dump_cpu_ctx[this_cpu].und_r13, msm_dump_cpu_ctx[this_cpu].und_r14, msm_dump_cpu_ctx[this_cpu].fiq_spsr,msm_dump_cpu_ctx[this_cpu].fiq_r8, msm_dump_cpu_ctx[this_cpu].fiq_r9, msm_dump_cpu_ctx[this_cpu].fiq_r10, msm_dump_cpu_ctx[this_cpu].fiq_r11, msm_dump_cpu_ctx[this_cpu].fiq_r12, msm_dump_cpu_ctx[this_cpu].fiq_r13, msm_dump_cpu_ctx[this_cpu].fiq_r14);
	ctx_regs.ARM_pc = msm_dump_cpu_ctx[this_cpu].fiq_r14;
	ctx_regs.ARM_lr = msm_dump_cpu_ctx[this_cpu].svc_r14;
	ctx_regs.ARM_sp = msm_dump_cpu_ctx[this_cpu].svc_r13;
	ctx_regs.ARM_fp = msm_dump_cpu_ctx[this_cpu].usr_r11;
	unwind_backtrace(&ctx_regs, current);

	if (fiq_counter == 1 && (cpu_is_msm8625() || cpu_is_msm8625q())) {
		cpumask_copy(&fiq_cpu_mask, cpu_online_mask);
		cpu_clear(this_cpu, fiq_cpu_mask);
		gic_raise_secure_softirq(&fiq_cpu_mask, GIC_SECURE_SOFT_IRQ);
	}

	flush_cache_all();
	outer_flush_all();
	spin_unlock_irqrestore(&msm_fiq_lock, msm_fiq_flags);
	return;
}

struct fiq_handler msm7k_fh = {
	.name = MODULE_NAME,
};

static int __init msm_setup_fiq_handler(void)
{
	int i, ret = 0;

	spin_lock_init(&msm_fiq_lock);
	claim_fiq(&msm7k_fh);
	set_fiq_handler(&msm7k_fiq_start, msm7k_fiq_length);

	for_each_possible_cpu(i) {
		msm7k_fiq_stack[i] = (void *)__get_free_pages(GFP_KERNEL,
			THREAD_SIZE_ORDER);
		if (msm7k_fiq_stack[i] == NULL)
			break;
	}

	if (i != nr_cpumask_bits) {
		pr_err("FIQ STACK SETUP IS NOT SUCCESSFUL\n");
		for (i = 0; i < nr_cpumask_bits && msm7k_fiq_stack[i] != NULL;
					i++)
			free_pages((unsigned long)msm7k_fiq_stack[i],
					THREAD_SIZE_ORDER);
		return -ENOMEM;
	}

	fiq_set_type(msm_fiq_no, IRQF_TRIGGER_RISING);
	if (cpu_is_msm8625() || cpu_is_msm8625q())
		gic_set_irq_secure(msm_fiq_no);
	else
		msm_fiq_select(msm_fiq_no);

	enable_irq(msm_fiq_no);
	pr_info("%s : MSM FIQ handler setup--done\n", __func__);
	return ret;
}

static int __init init7k_fiq(void)
{
	if (cpu_is_msm8625() || cpu_is_msm8625q())
		msm_fiq_no = MSM8625_INT_A9_M2A_2;
	else
		msm_fiq_no = INT_A9_M2A_2;

	if (msm_setup_fiq_handler())
		pr_err("MSM FIQ INIT FAILED\n");

	return 0;
}
late_initcall(init7k_fiq);
