/**
 * kalu - Copyright (C) 2012-2014 Olivier Brunel
 *
 * kalu-updater.c
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

#include <config.h>

/* C */
#include <string.h>

/* gio - for dbus */
#include <gio/gio.h>

/* kalu */
#include "kalu.h"
#include "kalu-updater.h"
#include "closures.h"

#include "../kalu-dbus/updater-dbus.h"


struct _KaluUpdater
{
    GDBusProxy parent_instance;
    KaluUpdaterPrivate *priv;
};

struct _KaluUpdaterClass
{
    GDBusProxyClass parent_class;

    /* signals */
    void (*debug)                   (KaluUpdater        *kupdater,
                                     const gchar        *msg);

    void (*downloading)             (KaluUpdater        *kupdater,
                                     const gchar        *filename,
                                     guint               xfered,
                                     guint               total);

    void (*sync_dbs)                (KaluUpdater        *kupdater,
                                     gint                nb);

    void (*sync_db_start)           (KaluUpdater        *kupdater,
                                     const gchar        *name);

    void (*sync_db_end)             (KaluUpdater        *kupdater,
                                     sync_db_results_t   result);

    void (*log)                     (KaluUpdater        *kupdater,
                                     loglevel_t          level,
                                     const gchar        *msg);

    void (*total_download)          (KaluUpdater        *kupdater,
                                     guint               total);

    void (*event)                   (KaluUpdater        *kupdater,
                                     event_t             event);

    void (*event_installed)         (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *version,
                                     alpm_list_t        *optdeps);

    void (*event_reinstalled)       (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *version);

    void (*event_removed)           (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *version);

    void (*event_upgraded)          (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *old_version,
                                     const gchar        *new_version,
                                     alpm_list_t        *newoptdeps);

    void (*event_downgraded)        (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *old_version,
                                     const gchar        *new_version,
                                     alpm_list_t        *newoptdeps);

    void (*event_pkgdownload_start) (KaluUpdater        *kupdater,
                                     const gchar        *file);

    void (*event_pkgdownload_done)  (KaluUpdater        *kupdater,
                                     const gchar        *file);

    void (*event_pkgdownload_failed)(KaluUpdater        *kupdater,
                                     const gchar        *file);

    void (*event_scriptlet)         (KaluUpdater        *kupdater,
                                     const gchar        *msg);

    void (*event_pacnew_created)    (KaluUpdater        *kupdater,
                                     gboolean            from_noupgrade,
                                     const gchar        *pkg,
                                     const gchar        *old_version,
                                     const gchar        *new_version,
                                     const gchar        *file);

    void (*event_pacsave_created)   (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *version,
                                     const gchar        *file);

    void (*event_pacorig_created)   (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *version,
                                     const gchar        *file);

    void (*event_delta_generating)  (KaluUpdater        *kupdater,
                                     const gchar        *delta,
                                     const gchar        *dest);

    void (*event_optdep_removal)    (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     const gchar        *optdep);

    void (*progress)                (KaluUpdater        *kupdater,
                                     event_t             event,
                                     const gchar        *pkg,
                                     gint                percent,
                                     guint               total,
                                     guint               current);

    /* questions */
    gboolean (*install_ignorepkg)   (KaluUpdater        *kupdater,
                                     const gchar        *pkg);

    gboolean (*replace_pkg)         (KaluUpdater        *kupdater,
                                     const gchar        *repo1,
                                     const gchar        *pkg1,
                                     const gchar        *repo2,
                                     const gchar        *pkg2);

    gboolean (*conflict_pkg)        (KaluUpdater        *kupdater,
                                     const gchar        *pkg1,
                                     const gchar        *pkg2,
                                     const gchar        *reason);

    gboolean (*remove_pkgs)         (KaluUpdater        *kupdater,
                                     alpm_list_t        *pkgs);

    gint     (*select_provider)     (KaluUpdater        *kupdater,
                                     const gchar        *pkg,
                                     alpm_list_t        *providers);

    gboolean (*corrupted_pkg)       (KaluUpdater        *kupdater,
                                     const gchar        *file,
                                     const gchar        *error);

    gboolean (*import_key)          (KaluUpdater        *kupdater,
                                     const gchar        *key_fingerprint,
                                     const gchar        *key_uid,
                                     const gchar        *key_created);
};

struct _KaluUpdaterPrivate
{
    method_callback_t *method_callbacks;
};

GType       kalu_updater_get_type   (void) G_GNUC_CONST;

enum
{
  SIGNAL_DEBUG,
  SIGNAL_DOWNLOADING,
  SIGNAL_SYNC_DBS,
  SIGNAL_SYNC_DB_START,
  SIGNAL_SYNC_DB_END,
  SIGNAL_LOG,
  SIGNAL_TOTAL_DOWNLOAD,
  SIGNAL_EVENT,
  SIGNAL_EVENT_INSTALLED,
  SIGNAL_EVENT_REINSTALLED,
  SIGNAL_EVENT_REMOVED,
  SIGNAL_EVENT_UPGRADED,
  SIGNAL_EVENT_DOWNGRADED,
  SIGNAL_EVENT_DELTA_GENERATING,
  SIGNAL_EVENT_RETRIEVING_PKGS,
  SIGNAL_EVENT_RETRIEVING_PKGS_DONE,
  SIGNAL_EVENT_RETRIEVING_PKGS_FAILED,
  SIGNAL_EVENT_PKGDOWNLOAD_START,
  SIGNAL_EVENT_PKGDOWNLOAD_DONE,
  SIGNAL_EVENT_PKGDOWNLOAD_FAILED,
  SIGNAL_EVENT_SCRIPTLET,
  SIGNAL_EVENT_OPTDEP_REMOVAL,
  SIGNAL_EVENT_PACNEW_CREATED,
  SIGNAL_EVENT_PACSAVE_CREATED,
  SIGNAL_EVENT_PACORIG_CREATED,
  SIGNAL_PROGRESS,
  /* questions */
  SIGNAL_INSTALL_IGNOREPKG,
  SIGNAL_REPLACE_PKG,
  SIGNAL_CONFLICT_PKG,
  SIGNAL_REMOVE_PKGS,
  SIGNAL_SELECT_PROVIDER,
  SIGNAL_CORRUPTED_PKG,
  SIGNAL_IMPORT_KEY,
  NB_SIGNALS
};

static guint signals[NB_SIGNALS] = { 0 };

G_DEFINE_TYPE (KaluUpdater, kalu_updater, G_TYPE_DBUS_PROXY)

void
kalu_updater_new (
        GCancellable        *cancellable,
        GAsyncReadyCallback  callback,
        gpointer             data)
{
    g_async_initable_new_async (
            KALU_TYPE_UPDATER,
            G_PRIORITY_DEFAULT,
            cancellable,
            callback,
            data,
            "g-bus-type",       G_BUS_TYPE_SYSTEM,
            "g-flags",          G_DBUS_PROXY_FLAGS_NONE,
            "g-name",           DBUS_NAME,
            "g-object-path",    OBJECT_PATH,
            "g-interface-name", INTERFACE_NAME,
            NULL
            );
}

KaluUpdater *
kalu_updater_new_finish (GAsyncResult *res, GError **error)
{
    GObject *object;
    GObject *source_object;

    source_object = g_async_result_get_source_object (res);
    g_assert (source_object != NULL);

    object = g_async_initable_new_finish (
            G_ASYNC_INITABLE (source_object),
            res,
            error);
    g_object_unref (source_object);

    if (object != NULL)
    {
        return KALU_UPDATER (object);
    }
    else
    {
        return NULL;
    }
}

static void
kalu_updater_finalize (GObject *object)
{
    G_GNUC_UNUSED KaluUpdater *kupdater = KALU_UPDATER (object);

    free (kupdater->priv->method_callbacks);

    if (G_OBJECT_CLASS (kalu_updater_parent_class)->finalize != NULL)
    {
        G_OBJECT_CLASS (kalu_updater_parent_class)->finalize (object);
    }
}

static void
kalu_updater_init (KaluUpdater *kupdater)
{
    /* Sets the expected interface */
    g_dbus_proxy_set_interface_info (G_DBUS_PROXY (kupdater),
            (GDBusInterfaceInfo *) &interface_info);

    kupdater->priv = G_TYPE_INSTANCE_GET_PRIVATE (kupdater,
            KALU_TYPE_UPDATER,
            KaluUpdaterPrivate);
    method_callback_t mc[] = {
        {"Init",        FALSE, NULL, NULL},
        {"InitAlpm",    FALSE, NULL, NULL},
        {"AddDb",       FALSE, NULL, NULL},
        {"SyncDbs",     FALSE, NULL, NULL},
        {"GetPackages", FALSE, NULL, NULL},
        {"SysUpgrade",  FALSE, NULL, NULL},
        {"Abort",       FALSE, NULL, NULL},
        {"NoSysUpgrade",FALSE, NULL, NULL},
        {"FreeAlpm",    FALSE, NULL, NULL},
        {NULL, FALSE, NULL, NULL}
    };
    size_t count = sizeof (mc) / sizeof (mc[0]);
    size_t i;
    kupdater->priv->method_callbacks = new0 (method_callback_t, count);
    for (i = 0; i < count; ++i)
    {
        kupdater->priv->method_callbacks[i] =  mc[i];
    }
}

#define emit_signal_answer(signal, ...)  do {                               \
    GError *error = NULL;                                                   \
    gboolean response = FALSE;                                              \
    g_signal_emit (kupdater, signals[signal], 0, __VA_ARGS__, &response);   \
    g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),                        \
            "Answer",                                                       \
            g_variant_new ("(i)", (gint) response),                         \
            G_DBUS_CALL_FLAGS_NONE,                                         \
            -1,                                                             \
            NULL,                                                           \
            &error);                                                        \
    if (error != NULL)                                                      \
    {                                                                       \
        /* emit signal error or something */                                \
    }                                                                       \
} while (0)
static void
kalu_updater_g_signal (GDBusProxy   *proxy,
                       const gchar  *sender_name _UNUSED_,
                       const gchar  *signal_name,
                       GVariant     *parameters)
{
    KaluUpdater *kupdater = KALU_UPDATER (proxy);
    if (g_strcmp0 (signal_name, "Debug") == 0)
    {
        gchar *msg;

        g_variant_get (parameters, "(s)", &msg);
        g_signal_emit (kupdater, signals[SIGNAL_DEBUG], 0, msg);
        free (msg);
    }
    else if (g_strcmp0 (signal_name, "MethodFailed") == 0)
    {
        gchar *name, *msg;

        g_variant_get (parameters, "(ss)", &name, &msg);
        if (msg == NULL)
        {
            msg = strdup (_("No error message specified in MethodFailed\n"));
        }

        if (g_strcmp0 (name, "Answer") == 0)
        {
            free (name);
            free (msg);
            return;
        }

        method_callback_t *mc;
        for (mc = kupdater->priv->method_callbacks; ; ++mc)
        {
            if (g_strcmp0 (mc->name, name) == 0)
            {
                if (!mc->is_running)
                {
                    debug ("MethodFailed: method %s not registered running\n",
                            name);
                    break;
                }
                debug ("MethodFailed for method %s: %s\n", name, msg);

                mc->is_running = FALSE;

                if (mc->callback == NULL)
                {
                    break;
                }

                KaluMethodCallback cb = mc->callback;
                gpointer data = mc->data;

                mc->callback = NULL;
                mc->data = NULL;

                cb (kupdater, msg, data);
                break;
            }
            else if (mc->name == NULL)
            {
                debug ("MethodFailed: Internal method definition missing for %s\n",
                        name);
            }
        }

        free (name);
        free (msg);
    }
    else if (g_strcmp0 (signal_name, "MethodFinished") == 0)
    {
        gchar *name;

        g_variant_get (parameters, "(s)", &name);

        if (g_strcmp0 (name, "Answer") == 0)
        {
            free (name);
            return;
        }

        method_callback_t *mc;
        for (mc = kupdater->priv->method_callbacks; ; ++mc)
        {
            if (g_strcmp0 (mc->name, name) == 0)
            {
                if (!mc->is_running)
                {
                    debug ("MethodFinished: method %s not registered running\n",
                            name);
                    break;
                }
                debug ("MethodFinished for method %s\n", name);

                mc->is_running = FALSE;

                if (mc->callback == NULL)
                {
                    break;
                }

                KaluMethodCallback cb = mc->callback;
                gpointer data = mc->data;

                mc->callback = NULL;
                mc->data = NULL;

                cb (kupdater, NULL, data);
                break;
            }
            else if (mc->name == NULL)
            {
                debug ("MethodFinished: Internal method definition missing for %s\n",
                        name);
            }
        }

        free (name);
    }
    else if (g_strcmp0 (signal_name, "GetPackagesFinished") == 0)
    {
        GVariantIter *iter;
        alpm_list_t *pkgs= NULL;

        method_callback_t *mc;
        for (mc = kupdater->priv->method_callbacks; ; ++mc)
        {
            if (g_strcmp0 (mc->name, "GetPackages") == 0)
            {
                if (!mc->is_running)
                {
                    debug ("GetPackagesFinished: method not registered running\n");
                    break;
                }

                KaluGetPackagesCallback cb = (KaluGetPackagesCallback) mc->callback;
                gpointer data = mc->data;

                mc->is_running = FALSE;
                mc->callback = NULL;
                mc->data = NULL;

                kalu_package_t *k_pkg;
                g_variant_get (parameters, "(a(sssssuuu))", &iter);
                k_pkg = new0 (kalu_package_t, 1);
                while (g_variant_iter_loop (iter, "(sssssuuu)",
                            &k_pkg->repo,
                            &k_pkg->name,
                            &k_pkg->desc,
                            &k_pkg->old_version,
                            &k_pkg->new_version,
                            &k_pkg->dl_size,
                            &k_pkg->old_size,
                            &k_pkg->new_size))
                {
                    pkgs = alpm_list_add (pkgs, k_pkg);
                    k_pkg = new0 (kalu_package_t, 1);
                }
                g_variant_iter_free (iter);
                free (k_pkg);

                cb (kupdater, NULL, pkgs, data);

                alpm_list_t *i;
                for (i = pkgs; i; i = alpm_list_next (i))
                {
                    k_pkg = i->data;
                    free (k_pkg->repo);
                    free (k_pkg->name);
                    free (k_pkg->desc);
                    free (k_pkg->old_version);
                    free (k_pkg->new_version);
                    free (k_pkg);
                }
                alpm_list_free (pkgs);
                break;
            }
            else if (mc->name == NULL)
            {
                debug ("GetPackagesFinished: Internal method definition missing\n");
            }
        }
    }
    else if (g_strcmp0 (signal_name, "SyncDbs") == 0)
    {
        gint nb;

        g_variant_get (parameters, "(i)", &nb);
        g_signal_emit (kupdater, signals[SIGNAL_SYNC_DBS], 0, nb);
    }
    else if (g_strcmp0 (signal_name, "SyncDbStart") == 0)
    {
        gchar *name;

        g_variant_get (parameters, "(s)", &name);
        g_signal_emit (kupdater, signals[SIGNAL_SYNC_DB_START], 0, name);
        free (name);
    }
    else if (g_strcmp0 (signal_name, "SyncDbEnd") == 0)
    {
        gint result;

        g_variant_get (parameters, "(i)", &result);
        g_signal_emit (kupdater, signals[SIGNAL_SYNC_DB_END], 0, result);
    }
    else if (g_strcmp0 (signal_name, "TotalDownload") == 0)
    {
        guint total;

        g_variant_get (parameters, "(u)", &total);
        g_signal_emit (kupdater, signals[SIGNAL_TOTAL_DOWNLOAD], 0, total);
    }
    else if (g_strcmp0 (signal_name, "Event") == 0)
    {
        gint event;

        g_variant_get (parameters, "(i)", &event);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT], 0, (event_t) event);
    }
    else if (g_strcmp0 (signal_name, "EventInstalled") == 0)
    {
        gchar *pkg, *version, *dep;
        alpm_list_t *optdeps = NULL;
        GVariantIter *iter;

        g_variant_get (parameters, "(ssas)", &pkg, &version, &iter);
        while (g_variant_iter_loop (iter, "s", &dep))
        {
            optdeps = alpm_list_add (optdeps, strdup (dep));
        }
        g_variant_iter_free (iter);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_INSTALLED], 0,
                pkg, version, optdeps);
        free (pkg);
        free (version);
        FREELIST (optdeps);
    }
    else if (g_strcmp0 (signal_name, "EventReinstalled") == 0)
    {
        gchar *pkg, *version;

        g_variant_get (parameters, "(ss)", &pkg, &version);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_REINSTALLED], 0,
                pkg, version);
        free (pkg);
        free (version);
    }
    else if (g_strcmp0 (signal_name, "EventRemoved") == 0)
    {
        gchar *pkg, *version;

        g_variant_get (parameters, "(ss)", &pkg, &version);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_REMOVED], 0,
                pkg, version);
        free (pkg);
        free (version);
    }
    else if (g_strcmp0 (signal_name, "EventUpgraded") == 0)
    {
        gchar *pkg, *old_version, *new_version, *dep;
        alpm_list_t *newoptdeps = NULL;
        GVariantIter *iter;

        g_variant_get (parameters, "(sssas)",
                &pkg,
                &old_version,
                &new_version,
                &iter);
        while (g_variant_iter_loop (iter, "s", &dep))
        {
            newoptdeps = alpm_list_add (newoptdeps, strdup (dep));
        }
        g_variant_iter_free (iter);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_UPGRADED], 0, pkg,
                old_version, new_version, newoptdeps);
        free (pkg);
        free (old_version);
        free (new_version);
        FREELIST (newoptdeps);
    }
    else if (g_strcmp0 (signal_name, "EventDowngraded") == 0)
    {
        gchar *pkg, *old_version, *new_version, *dep;
        alpm_list_t *newoptdeps = NULL;
        GVariantIter *iter;

        g_variant_get (parameters, "(sssas)",
                &pkg,
                &old_version,
                &new_version,
                &iter);
        while (g_variant_iter_loop (iter, "s", &dep))
        {
            newoptdeps = alpm_list_add (newoptdeps, strdup (dep));
        }
        g_variant_iter_free (iter);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_DOWNGRADED], 0, pkg,
                old_version, new_version, newoptdeps);
        free (pkg);
        free (old_version);
        free (new_version);
        FREELIST (newoptdeps);
    }
    else if (g_strcmp0 (signal_name, "EventRetrievingPkgs") == 0)
    {
        gchar *repo;

        g_variant_get (parameters, "(s)", &repo);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_RETRIEVING_PKGS], 0,
                repo);
        free (repo);
    }
    else if (g_strcmp0 (signal_name, "EventPkgdownloadStart") == 0)
    {
        gchar *file;

        g_variant_get (parameters, "(s)", &file);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_PKGDOWNLOAD_START], 0,
                file);
        free (file);
    }
    else if (g_strcmp0 (signal_name, "EventPkgdownloadDone") == 0)
    {
        gchar *file;

        g_variant_get (parameters, "(s)", &file);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_PKGDOWNLOAD_DONE], 0,
                file);
        free (file);
    }
    else if (g_strcmp0 (signal_name, "EventPkgdownloadFailed") == 0)
    {
        gchar *file;

        g_variant_get (parameters, "(s)", &file);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_PKGDOWNLOAD_FAILED], 0,
                file);
        free (file);
    }
    else if (g_strcmp0 (signal_name, "EventScriptlet") == 0)
    {
        gchar *msg;

        g_variant_get (parameters, "(s)", &msg);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_SCRIPTLET], 0, msg);
        free (msg);
    }
    else if (g_strcmp0 (signal_name, "EventPacnewCreated") == 0)
    {
        gboolean from_noupgrade;
        gchar *pkg, *old_version, *new_version, *file;

        g_variant_get (parameters, "(issss)",
                &from_noupgrade,
                &pkg,
                &old_version,
                &new_version,
                &file);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_PACNEW_CREATED], 0,
                from_noupgrade, pkg, old_version, new_version, file);
        free (pkg);
        free (old_version);
        free (new_version);
        free (file);
    }
    else if (g_strcmp0 (signal_name, "EventPacsaveCreated") == 0)
    {
        gchar *pkg, *version, *file;

        g_variant_get (parameters, "(sss)",
                &pkg,
                &version,
                &file);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_PACSAVE_CREATED], 0,
                pkg, version, file);
        free (pkg);
        free (version);
        free (file);
    }
    else if (g_strcmp0 (signal_name, "EventPacorigCreated") == 0)
    {
        gchar *pkg, *version, *file;

        g_variant_get (parameters, "(sss)",
                &pkg,
                &version,
                &file);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_PACORIG_CREATED], 0,
                pkg, version, file);
        free (pkg);
        free (version);
        free (file);
    }
    else if (g_strcmp0 (signal_name, "EventDeltaGenerating") == 0)
    {
        gchar *delta, *dest;

        g_variant_get (parameters, "(ss)", &delta, &dest);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_DELTA_GENERATING], 0,
                delta, dest);
        free (delta);
        free (dest);
    }
    else if (g_strcmp0 (signal_name, "EventOptdepRemoval") == 0)
    {
        gchar *pkg, *optdep;

        g_variant_get (parameters, "(ss)", &pkg, &optdep);
        g_signal_emit (kupdater, signals[SIGNAL_EVENT_OPTDEP_REMOVAL], 0,
                pkg, optdep);
        free (pkg);
        free (optdep);
    }
    else if (g_strcmp0 (signal_name, "Progress") == 0)
    {
        gint event, percent;
        gchar *pkg;
        guint total, current;

        g_variant_get (parameters, "(isiuu)",
                &event,
                &pkg,
                &percent,
                &total,
                &current);
        g_signal_emit (kupdater, signals[SIGNAL_PROGRESS], 0,
                (event_t) event, pkg, percent, total, current);
        free (pkg);
    }
    else if (g_strcmp0 (signal_name, "Downloading") == 0)
    {
        gchar *name;
        guint xfered, total;

        g_variant_get (parameters, "(suu)", &name, &xfered, &total);
        g_signal_emit (kupdater, signals[SIGNAL_DOWNLOADING], 0,
                name, xfered, total);
        free (name);
    }
    else if (g_strcmp0 (signal_name, "Log") == 0)
    {
        gint level;
        gchar *msg;

        g_variant_get (parameters, "(is)", &level, &msg);
        g_signal_emit (kupdater, signals[SIGNAL_LOG], 0, level, msg);
        free (msg);
    }
    else if (g_strcmp0 (signal_name, "AskInstallIgnorePkg") == 0)
    {
        gchar *pkg;
        g_variant_get (parameters, "(s)", &pkg);
        emit_signal_answer (SIGNAL_INSTALL_IGNOREPKG, pkg);
        free (pkg);
    }
    else if (g_strcmp0 (signal_name, "AskReplacePkg") == 0)
    {
        gchar *repo1, *pkg1, *repo2, *pkg2;
        g_variant_get (parameters, "(ssss)", &repo1, &pkg1, &repo2, &pkg2);
        emit_signal_answer (SIGNAL_REPLACE_PKG, repo1, pkg1, repo2, pkg2);
        free (repo1);
        free (pkg1);
        free (repo2);
        free (pkg2);
    }
    else if (g_strcmp0 (signal_name, "AskConflictPkg") == 0)
    {
        gchar *pkg1, *pkg2, *reason;
        g_variant_get (parameters, "(sss)", &pkg1, &pkg2, &reason);
        emit_signal_answer (SIGNAL_CONFLICT_PKG, pkg1, pkg2, reason);
        free (pkg1);
        free (pkg2);
        free (reason);
    }
    else if (g_strcmp0 (signal_name, "AskRemovePkgs") == 0)
    {
        GVariantIter *iter;
        gchar *pkg;
        alpm_list_t *pkgs = NULL;

        g_variant_get (parameters, "(as)", &iter);
        while (g_variant_iter_loop (iter, "s", &pkg))
        {
            pkgs = alpm_list_add (pkgs, strdup (pkg));
        }
        g_variant_iter_free (iter);

        emit_signal_answer (SIGNAL_REMOVE_PKGS, pkgs);
        free (pkg);
        FREELIST (pkgs);
    }
    else if (g_strcmp0 (signal_name, "AskSelectProvider") == 0)
    {
        GVariantIter *iter1, *iter2;
        gchar *pkg;
        alpm_list_t *providers = NULL;
        provider_t *provider;

        g_variant_get (parameters, "(saas)", &pkg, &iter1);
        while (g_variant_iter_loop (iter1, "as", &iter2))
        {
            provider = new0 (provider_t, 1);
            g_variant_iter_loop (iter2, "s", &provider->repo);
            g_variant_iter_loop (iter2, "s", &provider->pkg);
            g_variant_iter_loop (iter2, "s", &provider->version);
            providers = alpm_list_add (providers, provider);
        }
        g_variant_iter_free (iter1);

        emit_signal_answer (SIGNAL_SELECT_PROVIDER, pkg, providers);
        alpm_list_t *i;
        for (i = providers; i; i = alpm_list_next (i))
        {
            provider = i->data;
            free (provider->repo);
            free (provider->pkg);
            free (provider->version);
            free (provider);
        }
        alpm_list_free (providers);
    }
    else if (g_strcmp0 (signal_name, "AskCorruptedPkg") == 0)
    {
        gchar *file, *err;

        g_variant_get (parameters, "(ss)", &file, &err);
        emit_signal_answer (SIGNAL_CORRUPTED_PKG, file, err);
        free (file);
        free (err);
    }
    else if (g_strcmp0 (signal_name, "AskImportKey") == 0)
    {
        gchar *key_fingerprint, *key_uid, *key_created;

        g_variant_get (parameters, "(sss)",
                &key_fingerprint,
                &key_uid,
                &key_created);
        emit_signal_answer (SIGNAL_IMPORT_KEY,
                key_fingerprint, key_uid, key_created);
        free (key_fingerprint);
        free (key_uid);
        free (key_created);
    }
    else
    {
        g_warning ("Unknown signal received from kalu-updater: %s", signal_name);
    }
}
#undef emit_signal_answer

static void
kalu_updater_class_init (KaluUpdaterClass *klass)
{
    GObjectClass    *gobject_class;
    GDBusProxyClass *proxy_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = kalu_updater_finalize;

    proxy_class = G_DBUS_PROXY_CLASS (klass);
    proxy_class->g_signal = kalu_updater_g_signal;

    signals[SIGNAL_DOWNLOADING] = g_signal_new (
            "downloading",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, downloading),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_UINT_UINT,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING,
            G_TYPE_UINT,
            G_TYPE_UINT);

    signals[SIGNAL_SYNC_DBS] = g_signal_new (
            "sync-dbs",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, sync_dbs),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__INT,
            G_TYPE_NONE,
            1,
            G_TYPE_INT);

    signals[SIGNAL_SYNC_DB_START] = g_signal_new (
            "sync-db-start",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, sync_db_start),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    signals[SIGNAL_SYNC_DB_END] = g_signal_new (
            "sync-db-end",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, sync_db_end),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__INT,
            G_TYPE_NONE,
            1,
            G_TYPE_INT);

    signals[SIGNAL_DEBUG] = g_signal_new (
            "debug",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, debug),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    signals[SIGNAL_LOG] = g_signal_new (
            "log",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, log),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__INT_STRING,
            G_TYPE_NONE,
            2,
            G_TYPE_INT,
            G_TYPE_STRING);

    signals[SIGNAL_TOTAL_DOWNLOAD] = g_signal_new (
            "total-download",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, total_download),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__UINT,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);

    signals[SIGNAL_EVENT] = g_signal_new (
            "event",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__INT,
            G_TYPE_NONE,
            1,
            G_TYPE_INT);

    signals[SIGNAL_EVENT_INSTALLED] = g_signal_new (
            "event-installed",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_installed),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING_POINTER,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_POINTER);

    signals[SIGNAL_EVENT_REINSTALLED] = g_signal_new (
            "event-reinstalled",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_reinstalled),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING,
            G_TYPE_NONE,
            2,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_REMOVED] = g_signal_new (
            "event-removed",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_removed),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING,
            G_TYPE_NONE,
            2,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_UPGRADED] = g_signal_new (
            "event-upgraded",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_upgraded),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING_STRING_POINTER,
            G_TYPE_NONE,
            4,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_POINTER);

    signals[SIGNAL_EVENT_DOWNGRADED] = g_signal_new (
            "event-downgraded",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_downgraded),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING_STRING_POINTER,
            G_TYPE_NONE,
            4,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_POINTER);

    signals[SIGNAL_EVENT_PKGDOWNLOAD_START] = g_signal_new (
            "event-pkgdownload-start",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_pkgdownload_start),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_PKGDOWNLOAD_DONE] = g_signal_new (
            "event-pkgdownload-done",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_pkgdownload_done),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_PKGDOWNLOAD_FAILED] = g_signal_new (
            "event-pkgdownload-failed",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_pkgdownload_failed),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_SCRIPTLET] = g_signal_new (
            "event-scriptlet",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_scriptlet),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_OPTDEP_REMOVAL] = g_signal_new (
            "event-optdep-removal",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_optdep_removal),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING,
            G_TYPE_NONE,
            2,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_PACNEW_CREATED] = g_signal_new (
            "event-pacnew-created",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_pacnew_created),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__INT_STRING_STRING_STRING_STRING,
            G_TYPE_NONE,
            5,
            G_TYPE_INT,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_PACSAVE_CREATED] = g_signal_new (
            "event-pacsave-created",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_pacsave_created),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING_STRING,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_PACORIG_CREATED] = g_signal_new (
            "event-pacorig-created",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_pacorig_created),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING_STRING,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_EVENT_DELTA_GENERATING] = g_signal_new (
            "event-delta-generating",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, event_delta_generating),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__STRING_STRING,
            G_TYPE_NONE,
            2,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_PROGRESS] = g_signal_new (
            "progress",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, progress),
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__INT_STRING_INT_UINT_UINT,
            G_TYPE_NONE,
            5,
            G_TYPE_INT,
            G_TYPE_STRING,
            G_TYPE_INT,
            G_TYPE_UINT,
            G_TYPE_UINT);

    signals[SIGNAL_INSTALL_IGNOREPKG] = g_signal_new (
            "install-ignorepkg",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, install_ignorepkg),
            NULL,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__STRING,
            G_TYPE_BOOLEAN,
            1,
            G_TYPE_STRING);

    signals[SIGNAL_REPLACE_PKG] = g_signal_new (
            "replace-pkg",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, replace_pkg),
            NULL,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__STRING_STRING_STRING_STRING,
            G_TYPE_BOOLEAN,
            4,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_CONFLICT_PKG] = g_signal_new (
            "conflict-pkg",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, conflict_pkg),
            NULL,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__STRING_STRING_STRING,
            G_TYPE_BOOLEAN,
            3,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_REMOVE_PKGS] = g_signal_new (
            "remove-pkgs",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, remove_pkgs),
            NULL,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__POINTER,
            G_TYPE_BOOLEAN,
            1,
            G_TYPE_POINTER);

    signals[SIGNAL_SELECT_PROVIDER] = g_signal_new (
            "select-provider",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, select_provider),
            NULL,
            NULL,
            g_cclosure_user_marshal_INT__STRING_POINTER,
            G_TYPE_INT,
            2,
            G_TYPE_STRING,
            G_TYPE_POINTER);

    signals[SIGNAL_CORRUPTED_PKG] = g_signal_new (
            "corrupted-pkg",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, corrupted_pkg),
            NULL,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__STRING_STRING,
            G_TYPE_BOOLEAN,
            2,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[SIGNAL_IMPORT_KEY] = g_signal_new (
            "import-key",
            KALU_TYPE_UPDATER,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (KaluUpdaterClass, import_key),
            NULL,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__STRING_STRING_STRING,
            G_TYPE_BOOLEAN,
            3,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    g_type_class_add_private (gobject_class, sizeof (KaluUpdaterPrivate));
}

static gboolean
check_method (KaluUpdater *kupdater, const gchar *name,
              KaluMethodCallback callback, gpointer data, GError **error)
{
    if (!KALU_IS_UPDATER (kupdater))
    {
        g_set_error (error, KALU_UPDATER_ERROR, 1,
                _("Object is not a KaluUpdater\n"));
        return FALSE;
    }

    method_callback_t *mc;
    for (mc = kupdater->priv->method_callbacks; ; ++mc)
    {
        if (g_strcmp0 (mc->name, name) == 0)
        {
            if (mc->is_running)
            {
                g_set_error (error, KALU_UPDATER_ERROR, 1,
                        _("Cannot call method %s: already running\n"), name);
                return FALSE;
            }
            mc->is_running = TRUE;
            mc->callback = callback;
            mc->data = data;
            break;
        }
        else if (mc->name == NULL)
        {
            g_set_error (error, KALU_UPDATER_ERROR, 1,
                    _("Internal method definition missing for %s\n"), name);
            return FALSE;
        }
    }

    return TRUE;
}

static void
abort_method (KaluUpdater *kupdater, const gchar *name)
{
    method_callback_t *mc;
    for (mc = kupdater->priv->method_callbacks; ; ++mc)
    {
        if (g_strcmp0 (mc->name, name) == 0)
        {
            if (mc->is_running)
            {
                mc->is_running = FALSE;
                mc->callback = NULL;
                mc->data = NULL;
            }
            break;
        }
    }
}

#define check(name)     do {                                    \
    if (!check_method (kupdater, name, callback, data, error))  \
    {                                                           \
        return FALSE;                                           \
    }                                                           \
} while (0)

#define end(name)     do {                  \
    if (!variant)                           \
    {                                       \
        abort_method (kupdater, name);      \
        return FALSE;                       \
    }                                       \
    g_variant_unref (variant);              \
    return TRUE;                            \
} while (0)

/* Init */
gboolean    kalu_updater_init_upd           (KaluUpdater        *kupdater,
                                             gboolean            downloadonly,
                                             GCancellable       *cancellable,
                                             KaluMethodCallback  callback,
                                             gpointer            data,
                                             GError            **error)
{
    GVariant *variant;
    check ("Init");

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "Init",
            g_variant_new ("(b)", downloadonly),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    end ("Init");
}


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
                                             gdouble              usedelta,
                                             alpm_list_t         *ignorepkgs,
                                             alpm_list_t         *ignoregroups,
                                             alpm_list_t         *noupgrades,
                                             alpm_list_t         *noextracts,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("InitAlpm");

    GVariantBuilder *cachedirs_builder;
    GVariantBuilder *ignorepkgs_builder;
    GVariantBuilder *ignoregroups_builder;
    GVariantBuilder *noupgrades_builder;
    GVariantBuilder *noextracts_builder;
    alpm_list_t *i;

    cachedirs_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    for (i = cachedirs; i; i = alpm_list_next (i))
    {
        g_variant_builder_add (cachedirs_builder, "s", i->data);
    }

    ignorepkgs_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    for (i = ignorepkgs; i; i = alpm_list_next (i))
    {
        g_variant_builder_add (ignorepkgs_builder, "s", i->data);
    }

    ignoregroups_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    for (i = ignoregroups; i; i = alpm_list_next (i))
    {
        g_variant_builder_add (ignoregroups_builder, "s", i->data);
    }

    noupgrades_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    for (i = noupgrades; i; i = alpm_list_next (i))
    {
        g_variant_builder_add (noupgrades_builder, "s", i->data);
    }

    noextracts_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    for (i = noextracts; i; i = alpm_list_next (i))
    {
        g_variant_builder_add (noextracts_builder, "s", i->data);
    }

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "InitAlpm",
            g_variant_new ("(ssssasisbbdasasasas)",
                rootdir,
                dbpath,
                logfile,
                gpgdir,
                cachedirs_builder,
                siglevel,
                arch,
                checkspace,
                usesyslog,
                usedelta,
                ignorepkgs_builder,
                ignoregroups_builder,
                noupgrades_builder,
                noextracts_builder),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    g_variant_builder_unref (cachedirs_builder);
    g_variant_builder_unref (ignorepkgs_builder);
    g_variant_builder_unref (ignoregroups_builder);
    g_variant_builder_unref (noupgrades_builder);
    g_variant_builder_unref (noextracts_builder);
    end ("InitAlpm");
}


/* AddDb */

gboolean    kalu_updater_add_db             (KaluUpdater         *kupdater,
                                             gchar               *name,
                                             alpm_siglevel_t      siglevel,
                                             alpm_list_t         *servers,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("AddDb");

    GVariantBuilder *servers_builder;
    alpm_list_t *i;

    servers_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    for (i = servers; i; i = alpm_list_next (i))
    {
        g_variant_builder_add (servers_builder, "s", i->data);
    }

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "AddDb",
            g_variant_new ("(sias)",
                name,
                siglevel,
                servers_builder),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    g_variant_builder_unref (servers_builder);
    end ("AddDb");
}


/* SyncDbs */

gboolean    kalu_updater_sync_dbs           (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("SyncDbs");

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "SyncDbs",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    end ("SyncDbs");
}


/* GetPackages */

gboolean    kalu_updater_get_packages       (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("GetPackages");

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "GetPackages",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    end ("GetPackages");
}


/* SysUpgrade */

gboolean    kalu_updater_sysupgrade         (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("SysUpgrade");

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "SysUpgrade",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    end ("SysUpgrade");
}


/* Abort */

gboolean    kalu_updater_abort              (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("Abort");

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "Abort",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    end ("Abort");
}


/* NoSysUpgrade */

gboolean    kalu_updater_no_sysupgrade      (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("NoSysUpgrade");

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "NoSysUpgrade",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    end ("NoSysUpgrade");
}


/* FreeAlpm */

gboolean    kalu_updater_free_alpm          (KaluUpdater         *kupdater,
                                             GCancellable        *cancellable,
                                             KaluMethodCallback   callback,
                                             gpointer             data,
                                             GError             **error)
{
    GVariant *variant;
    check ("FreeAlpm");

    variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (kupdater),
            "FreeAlpm",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            cancellable,
            error);

    end ("FreeAlpm");
}

