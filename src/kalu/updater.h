/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * updater.h
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

#ifndef _KALU_UPDATER_H
#define _KALU_UPDATER_H

/* glib */
#include <glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

void
updater_run (const gchar *conffile, alpm_list_t *cmdline_post);

#endif /* _KALU_UPDATER_H */
