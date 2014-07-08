/**
 * kalu - Copyright (C) 2012-2014 Olivier Brunel
 *
 * kalu-alpm.h
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

#ifndef _KALU_ALPM_H
#define _KALU_ALPM_H

/* glib */
#include <glib-2.0/glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

#ifndef DISABLE_UPDATER
typedef struct {
    void (*dl_progress_cb) (const gchar *filename, off_t xfered, off_t total);
    void (*question_cb) (alpm_question_t event, void *data1, void *data2,
            void *data3, int *response);
    void (*log_cb) (alpm_loglevel_t level, const char *fmt, va_list args);
    void (*on_sync_dbs) (gpointer unused, gint nb);
    void (*on_sync_db_start) (gpointer unused, const gchar *name);
    void (*on_sync_db_end) (gpointer unused, guint result);
} kalu_simul_t;
#else
typedef struct _kalu_simul_t kalu_simul_t;
#endif

typedef struct _kalu_alpm_t {
    char            *dbpath; /* the tmp-path where we copied dbs */
    alpm_handle_t   *handle;
    alpm_transflag_t flags;
#ifndef DISABLE_UPDATER
    kalu_simul_t    *simulation;
#endif
} kalu_alpm_t;

/* global variable */
extern unsigned short alpm_verbose;

gboolean
kalu_alpm_load (kalu_simul_t *simulation, const gchar *conffile, GError **error);

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
