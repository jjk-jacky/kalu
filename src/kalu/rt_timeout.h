/**
 * kalu - Copyright (C) 2012-2013 Olivier Brunel
 *
 * rt_timeout.h
 * Copyright (C) 2013 Olivier Brunel <i.am.jack.mail@gmail.com>
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

#ifndef _KALU_RT_TIMEOUT_H
#define _KALU_RT_TIMEOUT_H

/* glib */
#include <glib-2.0/glib.h>

typedef struct _RtTimeoutSource RtTimeoutSource;

guint rt_timeout_add_seconds (guint interval, GSourceFunc function, gpointer data);

#endif /* _KALU_RT_TIMEOUT_H */
