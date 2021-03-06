/*
 * arch/arm/mach-meson8/time.c
 *
 * Copyright (C) 2013 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <asm/memory.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/sched_clock.h>
#include <plat/io.h>
#include <mach/am_regs.h>
#include <linux/delay.h>




/***********************************************************************
 * System timer
 **********************************************************************/
#define TIMER_E_RESOLUTION_BIT         8
#define TIMER_E_ENABLE_BIT        20
#define TIMER_E_RESOLUTION_MASK       (7UL << TIMER_E_RESOLUTION_BIT)
#define TIMER_E_RESOLUTION_SYS           0
#define TIMER_E_RESOLUTION_1us           1
#define TIMER_E_RESOLUTION_10us          2
#define TIMER_E_RESOLUTION_100us         3
#define TIMER_E_RESOLUTION_1ms           4

#define TIMER_DI_RESOLUTION_BIT         6
#define TIMER_CH_RESOLUTION_BIT         4
#define TIMER_BG_RESOLUTION_BIT         2
#define TIMER_AF_RESOLUTION_BIT         0

#define TIMER_DI_ENABLE_BIT         19
#define TIMER_CH_ENABLE_BIT         18
#define TIMER_BG_ENABLE_BIT         17
#define TIMER_AF_ENABLE_BIT         16

#define TIMER_DI_MODE_BIT         15
#define TIMER_CH_MODE_BIT         14
#define TIMER_BG_MODE_BIT         13
#define TIMER_AF_MODE_BIT         12

#define TIMER_RESOLUTION_1us            0
#define TIMER_RESOLUTION_10us           1
#define TIMER_RESOLUTION_100us          2
#define TIMER_RESOLUTION_1ms            3




/********** Clock Source Device, Timer-E *********/
static cycle_t cycle_read_timerE(struct clocksource *cs)
{
    return (cycles_t) aml_read_reg32(P_ISA_TIMERE);
}

static struct clocksource clocksource_timer_e = {
    .name   = "Timer-E",
    .rating = 300,
    .read   = cycle_read_timerE,
    .mask   = CLOCKSOURCE_MASK(32),
    .flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct delay_timer aml_delay_timer;

static u32 notrace meson8_read_sched_clock(void)
{
    return (u32)aml_read_reg32(P_ISA_TIMERE);
}

static void __init meson_clocksource_init(void)
{
	aml_clr_reg32_mask(P_ISA_TIMER_MUX, TIMER_E_RESOLUTION_MASK);
	aml_set_reg32_mask(P_ISA_TIMER_MUX, TIMER_E_RESOLUTION_1us << TIMER_E_RESOLUTION_BIT);
///    aml_write_reg32(P_ISA_TIMERE, 0);

    /**
     * (counter*mult)>>shift=xxx ns
     */
    /**
     * Constants generated by clocks_calc_mult_shift(m, s, 1MHz, NSEC_PER_SEC, 0).
     * This gives a resolution of about 1us and a wrap period of about 1h11min.
     */
    clocksource_timer_e.shift = 22;
    clocksource_timer_e.mult = 4194304000u;
    clocksource_register_khz(&clocksource_timer_e,1000);

    setup_sched_clock(meson8_read_sched_clock, 32,USEC_PER_SEC);

}

/********** Clock Event Device, Timer-ABCD/FGHI *********/

struct meson_clock {
	struct clock_event_device	clockevent;
	struct irqaction	irq;
	const char * name;	/*A,B,C,D,F,G,H,I*/
	int	bit_enable;	
	int 	bit_mode;
	int	bit_resolution;
	unsigned int	mux_reg;
	unsigned int	reg;
};

static irqreturn_t meson_timer_interrupt(int irq, void *dev_id);
static int meson_set_next_event(unsigned long evt,
                                struct clock_event_device *dev);
static void meson_clkevt_set_mode(enum clock_event_mode mode,
                                  struct clock_event_device *dev);

#ifdef CONFIG_SMP
/*
static void meson_tick_set_mode(enum clock_event_mode mode,
                                  struct clock_event_device *dev);
static int meson_tick_set_next_event(unsigned long evt,
                                struct clock_event_device *dev);
*/
#define meson_tick_rating 450
#else
/*
#define meson_tick_set_mode meson_clkevt_set_mode
#define meson_tick_set_next_event meson_set_next_event
*/
#define meson_tick_rating 300
#endif

#if 0
static struct meson_clock meson_timer_a = {
        .clockevent={
            .name           = "MESON TIMER-A",
            .rating         = 400, /* Reasonably fast and accurate clock event */

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-A",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_A,
        },
        .name = "A",
        .bit_enable = TIMER_AF_ENABLE_BIT,
        .bit_mode = TIMER_AF_MODE_BIT,
        .bit_resolution = TIMER_AF_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX,
        .reg=P_ISA_TIMERA
};
#endif

static struct meson_clock meson_timer_f = {
        .clockevent={
            .name           = "MESON TIMER-F",
            .rating         = 300, 

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-F",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_F,
        },
        .name = "F",
        .bit_enable = TIMER_AF_ENABLE_BIT,
        .bit_mode = TIMER_AF_MODE_BIT,
        .bit_resolution = TIMER_AF_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX1,
        .reg=P_ISA_TIMERF,
};

#if 0
static struct meson_clock meson_timer_b = {
        .clockevent={
            .name           = "MESON TIMER-B",
            .rating         = 300,

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-B",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_B,
        },
        .name = "B",
        .bit_enable = TIMER_BG_ENABLE_BIT,
        .bit_mode = TIMER_BG_MODE_BIT,
        .bit_resolution = TIMER_BG_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX,
        .reg=P_ISA_TIMERB,
};
#endif

static struct meson_clock meson_timer_g = {
        .clockevent={
            .name           = "MESON TIMER-G",
            .rating         = 300, 

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-G",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_G,
        },
        .name = "G",
        .bit_enable = TIMER_BG_ENABLE_BIT,
        .bit_mode = TIMER_BG_MODE_BIT,
        .bit_resolution = TIMER_BG_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX1,
        .reg=P_ISA_TIMERG,
};

#if 0
static struct meson_clock meson_timer_c = {
        .clockevent={
            .name           = "MESON TIMER-C",
            .rating         = 300,

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-C",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_C,
        },
        .name = "C",
        .bit_enable = TIMER_CH_ENABLE_BIT,
        .bit_mode = TIMER_CH_MODE_BIT,
        .bit_resolution = TIMER_CH_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX,
        .reg=P_ISA_TIMERC,
};
#endif

static struct meson_clock meson_timer_h = {
        .clockevent={
            .name           = "MESON TIMER-H",
            .rating         = 300, 

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-H",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_H,
        },
        .name = "H",
        .bit_enable = TIMER_CH_ENABLE_BIT,
        .bit_mode = TIMER_CH_MODE_BIT,
        .bit_resolution = TIMER_CH_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX1,
        .reg=P_ISA_TIMERH,
};

#if 0
static struct meson_clock meson_timer_d = {
        .clockevent={
            .name           = "MESON TIMER-D",
            .rating         = 300,

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-D",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_D,
        },
        .name = "D",
        .bit_enable = TIMER_DI_ENABLE_BIT,
        .bit_mode = TIMER_DI_MODE_BIT,
        .bit_resolution = TIMER_DI_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX,
        .reg=P_ISA_TIMERD,
};
#endif

static struct meson_clock meson_timer_i = {
        .clockevent={
            .name           = "MESON TIMER-I",
            .rating         = 300, 

            .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
            .shift          = 20,
            .set_next_event = meson_set_next_event,
            .set_mode       = meson_clkevt_set_mode,
        },
        .irq={
            .name           = "MESON TIMER-I",
            .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
            .handler        = meson_timer_interrupt,
            .irq            =INT_TIMER_I,
        },
        .name = "I",
        .bit_enable = TIMER_DI_ENABLE_BIT,
        .bit_mode = TIMER_DI_MODE_BIT,
        .bit_resolution = TIMER_DI_RESOLUTION_BIT,
        .mux_reg=P_ISA_TIMER_MUX1,
        .reg=P_ISA_TIMERI,
};

static struct meson_clock *clockevent_to_clock(struct clock_event_device *evt)
{
	//return container_of(evt, struct meson_clock, clockevent);
	return (struct meson_clock*)evt->private;
}

static DEFINE_SPINLOCK(time_lock);

static void meson_clkevt_set_mode(enum clock_event_mode mode,
                                  struct clock_event_device *dev)
{
	struct meson_clock * clk=clockevent_to_clock(dev);

	spin_lock(&time_lock);
	switch (mode) {
		case CLOCK_EVT_MODE_RESUME:
			printk(KERN_INFO"Resume timer%s\n", clk->name);
			aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_enable, 1);
		break;

		case CLOCK_EVT_MODE_PERIODIC:
			aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_mode, 1);
			aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_enable, 1);
			//printk("Periodic timer %s!,mux_reg=%x\n", clk->name,aml_read_reg32(clk->mux_reg));
		break;

		case CLOCK_EVT_MODE_ONESHOT:
			aml_set_reg32_bits(clk->mux_reg, 0, clk->bit_mode, 1);
			aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_enable, 1);
			//printk("One shot timer %s!mux_reg=%x\n", clk->name,aml_read_reg32(clk->mux_reg));
		break;
		case CLOCK_EVT_MODE_SHUTDOWN:
		case CLOCK_EVT_MODE_UNUSED:
			//printk(KERN_INFO"Disable timer %p %s\n",dev, clk->name);
			aml_set_reg32_bits(clk->mux_reg, 0, clk->bit_enable, 1);
		break;
	}
	spin_unlock(&time_lock);
}
static int meson_set_next_event(unsigned long evt,
                                struct clock_event_device *dev)
{
    struct meson_clock * clk=clockevent_to_clock(dev);
    /* use a big number to clear previous trigger cleanly */
    aml_set_reg32_mask(clk->reg, evt & 0xffff);

    /* then set next event */
    aml_set_reg32_bits(clk->reg, evt, 0, 16);
    return 0;
}


/* Clock event timer interrupt handler */
static irqreturn_t meson_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	if(evt == NULL || evt->event_handler == NULL){
		WARN_ONCE(evt == NULL || evt->event_handler == NULL,
			"%p %s %p %d",evt,evt?evt->name:NULL,evt?evt->event_handler:NULL,irq);
		return IRQ_HANDLED;
	}
	evt->event_handler(evt);
	return IRQ_HANDLED;

}
static void __cpuinit meson_timer_init_device(struct clock_event_device *evt)
{
	evt->mult=div_sc(1000000, NSEC_PER_SEC, 20);
	evt->max_delta_ns =
		clockevent_delta2ns(0xfffe, evt);
	evt->min_delta_ns=clockevent_delta2ns(1, evt);
	evt->cpumask = cpumask_of(smp_processor_id());
}
static void __init meson_timer_setup(struct clock_event_device *evt, struct meson_clock * clk )
{
    /**
     * Enable Timer and setting the time base;
     */
    aml_set_reg32_mask(clk->mux_reg,
    		((1 << clk->bit_enable)
    		|(1 << clk->bit_mode)
    		|(TIMER_RESOLUTION_1us << clk->bit_resolution)) );
    aml_write_reg32(clk->reg, 9999);
    meson_timer_init_device(&clk->clockevent);
    printk(KERN_ERR"Global timer: %s (%p) initialized\n",clk->clockevent.name,clk);

    clk->irq.dev_id=&clk->clockevent;
    clk->clockevent.private = (void *)clk;
    clockevents_register_device(&(clk->clockevent));

    /* Set up the IRQ handler */
    setup_irq(clk->irq.irq, &clk->irq);
}

#ifdef CONFIG_SMP
#include <asm/localtimer.h>

struct meson_clock * meson8_smp_local_timer[CONFIG_NR_CPUS]={
	&meson_timer_f,	/* CPU0 f*/
	&meson_timer_g,	/* CPU1 g*/
	&meson_timer_h,	/* CPU2 h*/
	&meson_timer_i	/* CPU3 i*/
};

int  __cpuinit meson_local_timer_setup(struct clock_event_device *evt)
{
	int cpu;
	struct meson_clock * clk;
	struct clock_event_device * meson_evt;
	
	if(!evt){
		printk(KERN_ERR"meson_local_timer_setup: null evt!\n");
		return -EINVAL;
	}

	cpu = smp_processor_id();
	if(cpu == 0)
		return 0;
	
	clk = meson8_smp_local_timer[cpu];
	
	aml_set_reg32_mask(clk->mux_reg,
		((1 << clk->bit_enable)
		|(1 << clk->bit_mode)
		|(TIMER_RESOLUTION_1us << clk->bit_resolution)) );
	aml_write_reg32(clk->reg, 9999);
	
	meson_timer_init_device(&(clk->clockevent));
	//printk(KERN_ERR"Local timer: %s (%p) for CPU%d initialized\n",
	//	clk->clockevent.name,clk,cpu);

	meson_evt = &clk->clockevent;
	evt->name = meson_evt->name;
	evt->rating = meson_evt->rating;
	evt->features = meson_evt->features;
	evt->shift = meson_evt->shift;
	evt->set_next_event = meson_evt->set_next_event;
	evt->set_mode = meson_evt->set_mode;
	evt->private = clk;
	evt->mult=div_sc(1000000, NSEC_PER_SEC, 20);
	evt->max_delta_ns =
		clockevent_delta2ns(0xfffe, evt);
	evt->min_delta_ns=clockevent_delta2ns(1, evt);
	clk->irq.dev_id=evt;

	clockevents_register_device(evt);
		
	if(cpu){
		irq_set_affinity(clk->irq.irq, cpumask_of(cpu));
	}
	/* Set up the IRQ handler */
	//setup_irq(clk->irq.irq, &clk->irq);
	enable_percpu_irq(clk->irq.irq, 0);
	
	return 0;
}
void  __cpuinit meson_local_timer_stop(struct clock_event_device *evt)
{
	struct meson_clock * clk;

	if(!evt){
		printk(KERN_ERR"meson_local_timer_stop: null evt!\n");
		return;//return -EINVAL;
	}

	clk = clockevent_to_clock(evt);
	evt->set_mode(CLOCK_EVT_MODE_UNUSED, evt);
	aml_clr_reg32_mask(clk->mux_reg,(1 << clk->bit_enable));
	//remove_irq(clk->irq.irq, &clk->irq);
	disable_percpu_irq(clk->irq.irq);

	return;
}

static struct local_timer_ops meson_local_timer_ops = {
	.setup = meson_local_timer_setup,
	.stop = meson_local_timer_stop,
};
inline int local_timer_ack(void)
{
    return 1;
}
#endif  /* CONFIG_SMP */

static void __init meson_clockevent_init(void)
{

    /***
     * Disable Timer A~D, and clean the time base
     * Now all of the timer is 1us base
     */
    aml_clr_reg32_mask(P_ISA_TIMER_MUX,~(TIMER_E_RESOLUTION_MASK));
    /***
     * Disable Timer F~I, and clean the time base
     * Now all of the timer is 1us base
     */
    aml_write_reg32(P_ISA_TIMER_MUX1,0);
	
#ifdef CONFIG_SMP
    meson_timer_setup(NULL,meson8_smp_local_timer[0]);
#else
    meson_timer_setup(NULL,&meson_timer_a);
//    meson_timer_setup(NULL,&meson_timer_b);
//    meson_timer_setup(NULL,&meson_timer_c);
//    meson_timer_setup(NULL,&meson_timer_d);
#endif
}


/*
 * This sets up the system timers, clock source and clock event.
 */
void __init meson_timer_init(void)
{
	int i;
	struct meson_clock *clk;
	meson_clocksource_init();
	meson_clockevent_init();
#ifdef CONFIG_SMP
	for(i=1; i<NR_CPUS; i++){
		clk = meson8_smp_local_timer[i];
		/* Set up the IRQ handler */
		setup_irq(clk->irq.irq, &clk->irq);
	}
	local_timer_register(&meson_local_timer_ops);
#endif

	aml_delay_timer.read_current_timer = &cycle_read_timerE;
	aml_delay_timer.freq = 1000*1000;//1us resolution
	register_current_timer_delay(&aml_delay_timer);

}

