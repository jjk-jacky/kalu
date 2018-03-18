/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * rt_timeout.c
 * Copyright (C) 2013-2018 Olivier Brunel <jjk@jjacky.com>
 *
 * This file is part of kalu.
 *
 * kalu is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * kalu is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * kalu. If not, see http://www.gnu.org/licenses/
 */

#include <sys/timerfd.h>
#include <unistd.h>         /* close() */
#include <time.h>
#include "rt_timeout.h"

struct _RtTimeoutSource
{
    GSource source;
    GPollFD pollfd;
    guint   interval;
};

static gboolean rt_timeout_prepare  (GSource *source, gint *timeout);
static gboolean rt_timeout_check    (GSource *source);
static gboolean rt_timeout_dispatch (GSource *source, GSourceFunc callback, gpointer data);
static void     rt_timeout_finalize (GSource *source);

static GSourceFuncs rt_timeout_funcs =
{
    .prepare    = rt_timeout_prepare,
    .check      = rt_timeout_check,
    .dispatch   = rt_timeout_dispatch,
    .finalize   = rt_timeout_finalize
};

static gboolean
rt_timeout_prepare (GSource *source __attribute__ ((unused)), gint *timeout)
{
    *timeout = -1;
    return FALSE;
}

static gboolean
rt_timeout_check (GSource *source)
{
    RtTimeoutSource *sce = (RtTimeoutSource *) source;
    return (sce->pollfd.revents & G_IO_IN);
}

#define SECOND      1000000000
static void set_value (struct timespec *tp, guint ms)
{
    time_t sec;
    long nsec;

    sec = ms / 1000;
    nsec = (ms - 1000 * sec) * 1000000;

    clock_gettime (CLOCK_REALTIME, tp);
    tp->tv_sec += sec;
    if (tp->tv_nsec + nsec > SECOND)
    {
        ++tp->tv_sec;
        nsec -= SECOND;
    }
    tp->tv_nsec += nsec;
}

static gboolean
rt_timeout_dispatch (GSource *source, GSourceFunc callback, gpointer data)
{
    gboolean again;

    again = callback (data);
    if (again)
    {
        RtTimeoutSource *sce = (RtTimeoutSource *) source;
        struct itimerspec ts;

        set_value (&ts.it_value, sce->interval);
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        if (timerfd_settime (sce->pollfd.fd, TFD_TIMER_ABSTIME, &ts, NULL) == -1)
            again = FALSE;
    }
    return again;
}

static void
rt_timeout_finalize (GSource *source)
{
    RtTimeoutSource *sce = (RtTimeoutSource *) source;
    close (sce->pollfd.fd);
}

guint
rt_timeout_add (guint interval, GSourceFunc function, gpointer data)
{
    GSource          *source;
    RtTimeoutSource  *sce;
    guint             id;
    struct itimerspec ts;

    if (interval == 0)
        return 0;

    source = g_source_new (&rt_timeout_funcs, sizeof (RtTimeoutSource));
    sce = (RtTimeoutSource *) source;

    sce->pollfd.fd = timerfd_create (CLOCK_REALTIME, TFD_NONBLOCK);
    if (sce->pollfd.fd == -1)
    {
        g_source_unref (source);
        return 0;
    }
    set_value (&ts.it_value, interval);
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    if (timerfd_settime (sce->pollfd.fd, TFD_TIMER_ABSTIME, &ts, NULL) == -1)
    {
        g_source_unref (source);
        return 0;
    }

    sce->pollfd.events = G_IO_IN | G_IO_ERR;
    g_source_add_poll (source, &sce->pollfd);

    sce->interval = interval;
    g_source_set_callback (source, function, data, NULL);

    id = g_source_attach (source, NULL);
    g_source_unref (source);
    return id;
}
