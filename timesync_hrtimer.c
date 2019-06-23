/*
 * TimeSync API driver.
 *
 * Copyright 2016 Google Inc.
 * Copyright 2016 Linaro Ltd.
 * Copyright 2019 The BeagleBoard.org Foundation
 *
 * Released under the GPLv2 only.
 *
 * This code simply uses hrtimer and CLOCK_MONOTONIC to provide a timesync
 * source.
 *
 * Based on timesync_platform.c .
 *
 * Modified 20190622 by Christopher Friedt, chrisfriedt@gmail.com
 */
#include <asm-generic/errno.h>
#include <linux/types.h>
#include <linux/hrtimer.h>

#include "greybus.h"
#include "arche_platform.h"

static struct hrtimer timer;
static u32 gb_timesync_clock_frequency;
int (*arche_platform_change_state_cb)(enum arche_platform_state state,
				      struct gb_timesync_svc *pdata);
EXPORT_SYMBOL_GPL(arche_platform_change_state_cb);

u64 gb_timesync_platform_get_counter(void)
{
	ktime_t t = hrtimer_cb_get_time(&timer);
	return (u64) t;
}

u32 gb_timesync_platform_get_clock_rate(void)
{
	return gb_timesync_clock_frequency;
}

int gb_timesync_platform_lock_bus(struct gb_timesync_svc *pdata)
{
	return arche_platform_change_state_cb(ARCHE_PLATFORM_STATE_TIME_SYNC,
					      pdata);
}

void gb_timesync_platform_unlock_bus(void)
{
	arche_platform_change_state_cb(ARCHE_PLATFORM_STATE_ACTIVE, NULL);
}

int __init gb_timesync_platform_init(void)
{
	hrtimer_init( &timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS );

	gb_timesync_clock_frequency = 1000000000 / hrtimer_resolution;

	return 0;
}

void gb_timesync_platform_exit(void) {}
