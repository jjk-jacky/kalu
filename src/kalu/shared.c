/**
 * kalu - Copyright (C) 2012-2013 Olivier Brunel
 *
 * shared.c
 * Copyright (C) 2012 Olivier Brunel <i.am.jack.mail@gmail.com>
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

/* kalu */
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
