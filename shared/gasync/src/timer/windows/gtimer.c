/*
 Copyright (c) 2016 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include <gtimer.h>
#include <gerror.h>
#include "timerres.h"

#include <windows.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_TIMERS 32

static struct {
    int used;
    int user;
    unsigned int period; // in base timer ticks
    unsigned int nexp; // number of base timer ticks since last event
    int (*fp_read)(int);
    int (*fp_close)(int);
} timers[MAX_TIMERS] = { };

#define CHECK_TIMER(TIMER,RETVALUE) \
  if (TIMER < 0 || TIMER >= MAX_TIMERS || timers[TIMER].used == 0) { \
    PRINT_ERROR_OTHER("invalid timer") \
    return RETVALUE; \
  }

void gtimer_destructor(void) __attribute__((destructor));
void gtimer_destructor(void) {
    unsigned int i;
    for (i = 0; i < sizeof(timers) / sizeof(*timers); ++i) {
        if (timers[i].used) {
            gtimer_close(i);
        }
    }
}

static int get_slot() {
    unsigned int i;
    for (i = 0; i < sizeof(timers) / sizeof(*timers); ++i) {
        if (timers[i].used == 0) {
            return i;
        }
    }
    return -1;
}

static int timer_cb(unsigned int nexp) {

    int ret = 0;

    unsigned int timer;
    for (timer = 0; timer < sizeof(timers) / sizeof(*timers); ++timer) {
        if (timers[timer].used) {
            timers[timer].nexp += nexp;
            unsigned int divisor = timers[timer].nexp / timers[timer].period;
            if (divisor >= 1) {
                int status = timers[timer].fp_read(timers[timer].user);
                if (status < 0) {
                    ret = -1;
                } else if (ret != -1 && status) {
                    ret = 1;
                }
                if (divisor > 1) {
                    LARGE_INTEGER li = timerres_get_time();
                    fprintf (stderr, "%u.%06u timer fired %u times...\n", (unsigned int)(li.QuadPart / 10000000), (unsigned int)(li.QuadPart / 10), divisor);
                }
                timers[timer].nexp = 0;
            }
        }
    }

    return ret;
}

int gtimer_start(int user, unsigned int usec, GPOLL_READ_CALLBACK fp_read, GPOLL_CLOSE_CALLBACK fp_close,
        GPOLL_REGISTER_HANDLE fp_register) {

    if (usec == 0) {
        PRINT_ERROR_OTHER("timer period cannot be 0")
        return -1;
    }

    int slot = get_slot();
    if (slot < 0) {
        PRINT_ERROR_OTHER("no slot available")
        return -1;
    }

    int timer_resolution = timerres_begin(fp_register, gpoll_remove_handle, timer_cb);
    if (timer_resolution < 0) {
        return -1;
    }
    
    unsigned int period = usec * 10 / timer_resolution;
    if (period == 0) {
        fprintf(stderr, "%s:%d %s: timer period should be at least %dus\n", __FILE__, __LINE__, __func__, timer_resolution / 10);
        timerres_end();
        return -1;
    }

    if (period * timer_resolution != usec * 10) {
        fprintf(stderr, "rounding timer period to %u\n", period * timer_resolution / 10);
    }

    LARGE_INTEGER li = timerres_get_time();
    li.QuadPart += period;

    timers[slot].used = 1;
    timers[slot].user = user;
    timers[slot].period = period;
    timers[slot].nexp = 0;
    timers[slot].fp_read = fp_read;
    timers[slot].fp_close = fp_close;

    return slot;
}

int gtimer_close(int timer) {

    CHECK_TIMER(timer, -1)

    memset(timers + timer, 0x00, sizeof(*timers));

    timerres_end();

    return 1;
}
