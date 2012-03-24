/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * alpm.h
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

#ifndef _KALU_ALPM_H
#define _KALU_ALPM_H

/* glib */
#include <glib-2.0/glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

typedef struct _kalu_alpm_t {
    char            *dbpath; /* the tmp-path where we copied dbs */
    alpm_handle_t   *handle;
    alpm_transflag_t flags;
} kalu_alpm_t;

/* global variable */
extern unsigned short alpm_verbose;

gboolean
kalu_alpm_load (const gchar *conffile, GError **error);

gboolean
kalu_alpm_syncdbs (gint *nb_dbs_synced, GError **error);

gboolean
kalu_alpm_has_updates (alpm_list_t **packages, GError **error);

gboolean
kalu_alpm_has_updates_watched (alpm_list_t **packages, alpm_list_t *watched, GError **error);

gboolean
kalu_alpm_has_foreign (alpm_list_t **packages, alpm_list_t *ignore, GError **error);

void
kalu_alpm_free (void);

#endif /* _KALU_ALPM_H */
