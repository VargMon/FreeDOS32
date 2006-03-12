/* FreeDOS-32 PIT-driven Event Management version 0.1
 * Copyright (C) 2006  Salvatore ISAJA
 *
 * This file "process.c" is part of the
 * FreeDOS-32 PIT-driven Event Management (the Program).
 *
 * The Program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Program; see the file GPL.txt; if not, write to
 * the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <dr-env.h>
#include <list.h>
#include <kernel.h>
#include <ll/i386/error.h>
#include "pit.h"
extern void pit_isr(void);
extern void pit_isr2(void);

#define PIT_DEBUG 0
#if PIT_DEBUG
 #define LOG_PRINTF(x) fd32_log_printf x
#else
 #define LOG_PRINTF(x)
#endif


/**** THE "ticks" VARIABLE HAS A DIFFERENT SEMANTICS WHEN THIS IS DEFINED ****/
/* FIXME: This should affect the assembly file too */
#define FULL_RESOLUTION

#define TSC_DELAY 1
#define TSC_TIME  2

/**
 * \defgroup pit PIT-driven event management
 *
 * These facilities add timer support to FreeDOS-32, using the Programmable Interval Timer.
 * The PIT is programmed at the default frequency of about 18.2 Hz, and is used to
 * schedule events at specified times. When an event is triggered a custom callback
 * function is called. All callback functions are called from inside the interrupt service
 * routine of the PIT.
 * \par Copyright:
 *      FreeDOS-32 PIT-driver event management version 0.1 \n
 *      Copyright &copy; 2006 Salvatore Isaja \n
 *      GNU General Public License version 2 or later
 * \todo The list of events could be ordered to optimize how the timer ISR loops across
 *       events to check which one to trigger.
 * @{
 */

typedef struct Event
{
	struct Event *prev;
	struct Event *next;
	uint64_t when;
	void (*callback)(void *p);
	void *param;
}
Event;

#define NUM_EVENTS 64 /* Max number of events allowed to be registered at the same time */
#define PIT_CLOCK 1193181 /* Hz, according to Mark Feldman's PC-GPE article on PIT */
static Event events_table[NUM_EVENTS];
static List events_used;
static List events_free;
volatile uint64_t ticks;
/* Use the programmed PIT tick and convert when using, avoid losing precision */
unsigned pit_ticks;
static volatile int calibration_finished;
static uint64_t iterations_per_sec;
static int use_rdtsc;
static int pit_mode;
static uint64_t tick_per_sec_p;

INLINE_OP uint64_t rdtsc(void)
{
	uint64_t r;
	__asm__ __volatile__ ("rdtsc" : "=A" (r));
	return r;
}



INLINE_OP uint16_t read_pit0()
{
	uint8_t lsb, msb;

	fd32_outb(0x43, 0x00); /* Latch channel 0 */
	lsb = fd32_inb(0x40); /* Read channel 0 lach register */
	msb = fd32_inb(0x40);

	return ((uint16_t)msb) << 8 | lsb;
}

/* This part of the interface needs more work. */
uint64_t pit_gettime( int mode )
{
	uint64_t tmp;
	uint16_t adjust;

	switch(mode)
	{
		case PIT_TICKS_EXACT:
			if(pit_mode == PIT_NATIVE_MODE)
			{
				fd32_cli();
				tmp = ticks;
				adjust = read_pit0();
				fd32_sti();
				tmp += pit_ticks - adjust; /* CHECKME */
				return tmp;
			}
		case PIT_TICKS_FAST:
			fd32_cli();
			tmp = ticks;
			fd32_sti();
			return tmp;
		case PIT_USECS_EXACT:
			if(pit_mode == PIT_NATIVE_MODE)
			{
				fd32_cli();
				tmp = ticks;
				adjust = read_pit0();
				fd32_sti();
				tmp += pit_ticks - adjust; /* CHECKME */
				tmp *= 1000*1000;
				tmp /= PIT_CLOCK;
				return tmp;
			}
		case PIT_USECS_FAST:
			fd32_cli();
			tmp = ticks;
			fd32_sti();
			tmp *= 1000*1000;
			tmp /= PIT_CLOCK;
			return tmp;
		case PIT_TICK_FREQUENCY:
#ifdef FULL_RESOLUTION
			return PIT_CLOCK;
#else
			return PIT_CLOCK / pit_ticks;
#endif
		case PIT_TSC_FREQUENCY:
			return tick_per_sec_p;
		case PIT_TSC:
			return rdtsc();
		case PIT_GET_MODE:
			return mode;
		default:
			return -1;
	}
}


static unsigned usec_to_ticks(unsigned usec)
{
	uint64_t tmp_usec = usec;
	if(use_rdtsc & TSC_TIME)
		return (tmp_usec * tick_per_sec_p) / 1000000;
#ifdef FULL_RESOLUTION
	return (tmp_usec * PIT_CLOCK) / 1000000;
#else
	uint64_t tmp_pit_ticks = pit_ticks;
	return (tmp_usec * PIT_CLOCK) / (tmp_pit_ticks * 1000000);
#endif
}

void pit_process(void)
{
	//LOG_PRINTF(("[PIT] start pit_process\n"));
	Event *e, *enext;
	if(use_rdtsc & TSC_TIME)
	{
		for (e = (Event *) events_used.begin; e; e = enext)
		{
			enext = e->next;
			fd32_cli();
			if (rdtsc() >= e->when)
			{
				LOG_PRINTF(("[PIT] @%u event %08xh (scheduled @%u)\n", (unsigned) ticks, (unsigned) e, (unsigned) e->when));
				fd32_sti();
				list_erase(&events_used, (ListItem *) e);
				list_push_front(&events_free, (ListItem *) e);
				e->callback(e->param);
			}
			fd32_sti();
		}
	}
	else
	{
		for (e = (Event *) events_used.begin; e; e = enext)
		{
			enext = e->next;
			fd32_cli();
			if (ticks >= e->when)
			{
				LOG_PRINTF(("[PIT] @%u event %08xh (scheduled @%u)\n", (unsigned) ticks, (unsigned) e, (unsigned) e->when));
				fd32_sti();
				list_erase(&events_used, (ListItem *) e);
				list_push_front(&events_free, (ListItem *) e);
				e->callback(e->param);
			}
			fd32_sti();
		}
	}
	//LOG_PRINTF(("[PIT] end pit_process\n"));
}


/**
 * \brief Replacement for interrupt service routine.
 *
 * Another module or native application taking control of the PIT may call this
 * function every 55ms to enable time events registered with this module to be
 * dispatched. INT 1Ch and other actions only required for DOS compatibility
 * will not be performed by this function.
 */
void pit_external_process(void)
{
	fd32_cli();
#ifdef FULL_RESOLUTION
	ticks += pit_ticks;
#else
	ticks++;
#endif
	fd32_sti();
	pit_process();
}


/**
 * \brief Registers a timer-driven event.
 *
 * The callback function specified by \a callback will be called by the timer handler
 * after the amount of microseconds specified by \a usec has elapsed. The parameter
 * specified by \a param will be passed to the callback function.
 * \return A handle to identify the registered event, or NULL on failure.
 * \remarks The actual time elapsed may be longer than \a usec according to the rounding
 *          due to the frequency the PIT is programmed.
 */
void *pit_event_register(unsigned usec, void (*callback)(void *p), void *param)
{
	LOG_PRINTF(("[PIT] start pit_event_register (%u us)\n", usec));
	uint32_t f = ll_fsave();
	Event *e = (Event *) events_free.begin;
	if (e)
	{
		list_erase(&events_free, (ListItem *) e);
		list_push_back(&events_used, (ListItem *) e);
		e->when = usec_to_ticks(usec);
		if(use_rdtsc & TSC_TIME)
			e->when += rdtsc();
		else
		{
			fd32_cli();
			e->when += ticks;
			fd32_sti();
		}
		e->callback = callback;
		e->param = param;
	}
	ll_frestore(f);
	/* We should disable interrupts here too. */
	LOG_PRINTF(("[PIT] end pit_event_register %08xh (ticks=%u,when=%u,diff=%u)\n",
		(unsigned) e, (unsigned) ticks, (unsigned) e->when, (unsigned) (e->when - ticks)));
	return e;
}


/**
 * \brief Cancels a registered timer-driven event.
 * \param event_handle handle of the event to cancel as returned by pit_event_register.
 * \return 0 on success, or a negative value on failure (event not found).
 * \remarks It is not necessary to cancel an event that already occurred, although it
 *          does not harm to (and a negative value will be returned).
 */
int pit_event_cancel(void *event_handle)
{
	Event *e;
	int res = -1;
	uint32_t f = ll_fsave();
	LOG_PRINTF(("[PIT] @%u pit_event_cancel %08xh\n", (unsigned) ticks, (unsigned) event_handle));
	for (e = (Event *) events_used.begin; e; e = e->next)
		if (e == event_handle)
		{
			list_erase(&events_used, (ListItem *) e);
			list_push_front(&events_free, (ListItem *) e);
			res = 0;
			break;
		}
	ll_frestore(f);
	return res;
}


/**
 * \brief Blocks the execution for the specified amount of time.
 * \param usec number of microseconds to block.
 * \remarks The function actively loops until the specified amount of time has passed.
 * \remarks The actual time elapsed may be longer than \a usec according to the rounding
 *          due to the frequency the PIT is programmed.
 */
void pit_delay(unsigned usec)
{
	uint64_t when;
	if(use_rdtsc & TSC_TIME)
	{
		when = rdtsc() + usec_to_ticks(usec);
		while (rdtsc() < when)
			fd32_cpu_idle();
	}
	else
	{
		when = ticks + usec_to_ticks(usec);
		while (1)
		{
			fd32_cli();
			if(ticks >= when)
			{
				fd32_sti();
				break;
			}
			fd32_sti();
			fd32_cpu_idle();
		}
	}
}

static uint64_t delay_loop(uint64_t rounds)
{
	while(rounds)
	{
		if(calibration_finished)
			break;
		rounds--;
	}
	return rounds;
}

/**
 * \brief Blocks the execution for the specified amount of more precise time.
 * \param nsec number of nanoseconds to block.
 * \remarks The function actively loops until the specified amount of time has passed.
 */
static void nano_delay_l(unsigned nsec)
{
	uint64_t iterations = iterations_per_sec * nsec / 1000000000;
	delay_loop(iterations);
}



/* A implementation of nano_delay Pentium+ processor (by Hanzac Chen) */
static void nano_delay_p(unsigned nsec)
{
	uint64_t cpu_tick = rdtsc();
	/* NOTE:
	 *	Use tick_per_usec instead of tick_per_nsec is for increasing integer
	 *	calculation precision
	 */
	uint64_t when_tick = cpu_tick + nsec * tick_per_sec_p / 1000000000; /* using libgcc to allow 64-bit division */

	while (cpu_tick < when_tick) {
		cpu_tick = rdtsc();
	}
}

static void callback(void* parm)
{
	calibration_finished = 1;
}

static void measure_tsc_frequency()
{
	uint64_t start, end;
	uint16_t t1, t2;
	int t;

	t = 0;
	fd32_cli();
	t2 = read_pit0();
	start = rdtsc();
	/* delay one second */
	while(t < PIT_CLOCK)
	{
		t1 = read_pit0();
		t += (t2 - t1) % pit_ticks;
		t2 = t1;
	}
	end = rdtsc();
	fd32_sti();

	tick_per_sec_p = end - start;
	fd32_message("[PIT] TSC frequency has been measured to %lluHz.\n", tick_per_sec_p);

	LOG_PRINTF(("[PIT] PIT Tick per second is %u\n", PIT_CLOCK / pit_ticks));
	LOG_PRINTF(("[PIT] CPU Tick per second is %llu\n", tick_per_sec_p));
}

static void nano_delay_init(void)
{
	if (use_rdtsc & TSC_DELAY) {
		if (add_call("nano_delay", (uint32_t) nano_delay_p, ADD) == -1)
			message("[PIT] Cannot add \"nano_delay\" to the symbol table. Aborted.\n");
	} else {
		calibration_finished = 0;
		if(pit_event_register( 1000*1000, &callback, NULL) < 0)
		{
			message("[PIT] Failed to init nano_delay.\n");
			return;
		}
		iterations_per_sec = 0xFFFFFFFFFFFFFFFFLL - delay_loop(0xFFFFFFFFFFFFFFFFLL);
		calibration_finished = 0;
		
		if (add_call("nano_delay", (uint32_t) nano_delay_l, ADD) == -1)
			message("[PIT] Cannot add \"nano_delay\" to the symbol table. Aborted.\n");
	}
}

static struct { char *name; uint32_t address; } symbols[] =
{
	{ "pit_event_register", 	(uint32_t) pit_event_register 	},
	{ "pit_event_cancel",   	(uint32_t) pit_event_cancel   	},
	{ "pit_delay",          	(uint32_t) pit_delay          	},
	{ "pit_external_process", 	(uint32_t) pit_external_process	},
	{ "pit_gettime", 			(uint32_t) pit_gettime			},
	{ "pit_set_mode", 			(uint32_t) pit_set_mode			},
	{ 0, 0 }
};


static struct option pit_options[] =
{
  /* These options set a flag. */
  {"tsc-delay", no_argument, &use_rdtsc, 'D'},
  {"tsc-time", no_argument, &use_rdtsc, 'T'},
  /* These options don't set a flag.
     We distinguish them by their indices. */
  {"max-count", required_argument, 0, 'C'},
  {"tick-period", required_argument, 0, 'P'},
  {0, 0, 0, 0}
};

static void counter_init(int counter, int mode, uint16_t max, void (*isr)(void))
{
	uint32_t f;
	
	f = ll_fsave();
	fd32_outb(0x43, counter << 6 | 3 << 4 | mode << 1 /* 0x34 */);
	fd32_outb(0x40, max & 0x00FF);
	fd32_outb(0x40, max >> 8);
	if(isr != NULL)
		idt_place(PIC1_BASE + 0, isr);
	ll_frestore(f);
}

/**
 * \brief Set the operating mode of this module.
 * \param mode The new operating mode, either PIT_COMPATIBLE_MODE or PIT_NATIVE_MODE.
 * \param maxcount Maximum value of the counter. Interrupts will be generated with a
 *         frequency of 1193181/\a maxcount Hz.
 * \return 0 on success, or a negative value on failure.
 * \remarks For compatibility with DOS/DPMI programs, \a mode should be
 *          PIT_COMPATIBLE_MODE and \a maxcount should be 0x10000.
 * \remarks No assumption is made about the state of the PIT in compatibility mode,
 *          and no attempt is made to access the PIT.
 */
int pit_set_mode( int mode, uint16_t maxcount )
{
	switch (mode)
	{
		case PIT_COMPATIBLE_MODE:
		case PIT_NATIVE_MODE:
			pit_mode = mode;
			fd32_cli();
			ticks += maxcount - pit_ticks; /* Adjusment for the next PIT interrupt. */
			pit_ticks = maxcount;
			if(mode == PIT_NATIVE_MODE)
				counter_init(0, 2, pit_ticks, &pit_isr2);
			else
				counter_init(0, 2, pit_ticks, &pit_isr);
			fd32_sti();
			return 0;
		default:
			return -1;
	}
}

			
void pit_init(process_info_t *pi)
{
	unsigned k;
	uint64_t tmp;

	/* Parsing options ... */
	char **argv;
	int argc = fd32_get_argv(pi->filename, pi->args, &argv);

	use_rdtsc = 0; /* Disable rdtsc by default for i386 compatible */
		//k = PIT_CLOCK / frequency;
	pit_mode = PIT_COMPATIBLE_MODE;
	pit_ticks = 0x10000;
	if (argc > 1) {
		int c, option_index = 0;
		/* Parse the command line */
		for ( ; (c = getopt_long (argc, argv, "", pit_options, &option_index)) != -1; ) {
			switch (c) {
				case 'D':
					use_rdtsc |= TSC_DELAY;
					message("Pentium+ time-stamp enabled\n");
					break;
				case 'T':
					use_rdtsc |= TSC_TIME;
					message("Pentium+ time-stamp enabled\n");
					break;
				case 'C':
					pit_ticks = strtoi(optarg, 10, NULL);
					if(pit_ticks > 0x10000 || pit_ticks < 2)
					{
						message("Invalid max-count argument %s(%u), using default.\n", optarg, pit_ticks);
						pit_ticks = 0x10000;
					}
					pit_mode = PIT_NATIVE_MODE;
					break;
				case 'P':
					tmp = strtoi(optarg, 10, NULL); /* Fix this when we get a better strtoi. */
					tmp *= PIT_CLOCK;
					tmp /= 1000*1000;
					if( tmp > 0x10000 || tmp < 2)
						pit_ticks = 0x10000;
					else
						pit_ticks = (uint16_t)tmp;
					pit_mode = PIT_NATIVE_MODE;
					break;
				default:
					break;
			}
		}
	}
	/* TODO: Use CPUID to override TSC options */
	message("Max timer value is %u.\n", pit_ticks);
	fd32_unget_argv(argc, argv);
	
	message("Going to install PIT-driven event management... ");
	for (k = 0; symbols[k].name; k++)
		if (add_call(symbols[k].name, symbols[k].address, ADD) == -1)
		{
			message("\n[PIT] Cannot add \"%s\" to the symbol table. Aborted.\n", symbols[k].name);
			return;
		}
	list_init(&events_used);
	list_init(&events_free);
	for (k = 0; k < NUM_EVENTS; k++) list_push_back(&events_free, (ListItem *) &events_table[k]);
	ticks = 0;
	if(pit_ticks == 0x10000)
		counter_init(0, 2, pit_ticks, &pit_isr);
	else
		counter_init(0, 2, pit_ticks, &pit_isr2);
		
	if(use_rdtsc != 0)
		measure_tsc_frequency();
	irq_unmask(0);
	/* Initialize the nano_delay */
	nano_delay_init();
	message("done\n");
}

/* @} */
