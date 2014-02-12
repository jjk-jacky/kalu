/**
 * kalu - Copyright (C) 2012-2014 Olivier Brunel
 *
 * shared.c
 * Copyright (C) 2012-2014 Olivier Brunel <jjk@jjacky.com>
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

/* C */
#include <stdio.h>
#include <sys/utsname.h>

/* alpm */
#include <alpm.h>

/* kalu */
#include "config.h"
#include "shared.h"

void *
_alloc (size_t len, int zero)
{
    void *ptr;

    ptr = malloc (len);
    if (!ptr)
    {
        exit (255);
    }
    if (zero)
    {
        memzero (ptr, len);
    }
    return ptr;
}

void *
_realloc (void *ptr, size_t len)
{
    ptr = realloc (ptr, len);
    if (!ptr)
    {
        exit (255);
    }
    return ptr;
}

void
set_user_agent (void)
{
    char ua[128];
    struct utsname un;

    uname (&un);
    snprintf (ua, 128, "kalu/%s (%s %s) libalpm/%s",
            PACKAGE_VERSION, un.sysname, un.machine, alpm_version ());
    setenv ("HTTP_USER_AGENT", ua, 0);
}
