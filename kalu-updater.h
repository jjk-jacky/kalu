/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * kalu-updater.h
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

#ifndef _KALU_KALU_UPDATER_H
#define _KALU_KALU_UPDATER_H

/* glib */
#include <glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

typedef struct _provider_t {
    gchar *repo;
    gchar *pkg;
    gchar *version;
} provider_t;

#define KALU_UPDATER_ERROR        g_quark_from_static_string ("kalu-updater error")

#define KALU_TYPE_UPDATER         (kalu_updater_get_type ())
#define KALU_UPDATER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), KALU_TYPE_UPDATER, KaluUpdater))
#define KALU_UPDATER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), KALU_TYPE_UPDATER, KaluUpdaterClass))
#define KALU_UPDATER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), KALU_TYPE_UPDATER, KaluUpdaterClass))
#define KALU_IS_UPDATER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), KALU_TYPE_UPDATER))
#define KALU_IS_UPDATER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), KALU_TYPE_UPDATER))

typedef struct _KaluUpdater        KaluUpdater;
typedef struct _KaluUpdaterClass   KaluUpdaterClass;
typedef struct _KaluUpdaterPrivate KaluUpdaterPrivate;

typedef void (*KaluMethodCallback)      (KaluUpdater *updater, const gchar *errmsg,
                                         gpointer data);
typedef void (*KaluGetPackagesCallback) (KaluUpdater *updater, const gchar *errmsg,
                                         alpm_list_t *pkgs, gpointer data);

typedef struct _method_callback_t {
    const gchar        *name;
    gboolean            is_running;
    KaluMethodCallback  callback;
    gpointer            data;
} method_callback_t;

/* object creation */
void
kalu_updater_new (GCancellable *cancellable, GAsyncReadyCallback callback, gpointer data);

KaluUpdater *
kalu_updater_new_finish (GAsyncResult *res, GError **error);

/* Init */
gboolean    kalu_updater_init_upd           (KaluUpdater        *updater,
                                             GCancellable       *cancellable,
                                             KaluMethodCallback  callback,
                                             gpointer            data,
                                             GError            **error);


/* InitAlpm */
gboolean    kalu_updater_init_alpm          (KaluUpdater         *kupdater,
                                             gchar               *rootdir,
                                             gchar               *dbpath,
                                             gchar               *logfile,
                                             gchar               *gpgdir,
                                             alpm_list_t         *cachedirs,
                                             alpm_siglevel_t     siglevel,
                                             gchar               *arch,
                                             gboolean             checkspace,
                                             gboolean             usesyslog,
                                             gboolean             usedelta,
                                             alpm_list_t         *ignorepkgs,
                                             alpm_list_t         *ignoregroups,
                                             alpm_list_t         *noupgrades,
                                             alpm_list_t         *noextracts,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error);


/* AddDb */
gboolean    kalu_updater_add_db             (KaluUpdater         *kupdater,
                                             gchar               *name,
                                             alpm_siglevel_t      siglevel,
                                             alpm_list_t         *servers,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error);


/* SyncDbs */
gboolean    kalu_updater_sync_dbs           (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error);


/* GetPackages */
gboolean    kalu_updater_get_packages       (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error);


/* SysUpgrade */

gboolean    kalu_updater_sysupgrade         (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error);


/* NoSysUpgrade */

gboolean    kalu_updater_no_sysupgrade      (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error);


/* FreeAlpm */
gboolean    kalu_updater_free_alpm          (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error);

#endif /* _KALU_KALU_UPDATER_H */
