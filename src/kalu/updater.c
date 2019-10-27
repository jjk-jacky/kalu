/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * updater.c
 * Copyright (C) 2012-2016 Olivier Brunel <jjk@jjacky.com>
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
#include <string.h> /* strdup */

/* gtk */
#include <gtk/gtk.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* kalu */
#include "kalu.h"
#include "updater.h"
#include "util-gtk.h"
#include "util.h"
#include "kalu-updater.h"
#include "conf.h"
#include "gui.h" /* show_notif() */
#include "kalu-alpm.h"  /* simulation */

#include "../kalu-dbus/kupdater.h"

/* split of pctg of each step in the global progress of the sysupgrade */
#define PCTG_DOWNLOAD           0.42
#define PCTG_KEYRING            0.01
#define PCTG_PKG_INTEGRITY      0.01
#define PCTG_FILE_CONFLICTS     0.01
#define PCTG_LOAD_PKGFILES      0.01
#define PCTG_CHECK_DISKSPACE    0.01
#define PCTG_SYSUPGRADE         0.53

#define PCTG_NO_DL_KEYRING            0.05
#define PCTG_NO_DL_PKG_INTEGRITY      0.05
#define PCTG_NO_DL_FILE_CONFLICTS     0.05
#define PCTG_NO_DL_LOAD_PKGFILES      0.05
#define PCTG_NO_DL_CHECK_DISKSPACE    0.05
#define PCTG_NO_DL_SYSUPGRADE         0.75

#define PCTG_DL_ONLY_DOWNLOAD           0.90
#define PCTG_DL_ONLY_KEYRING            0.02
#define PCTG_DL_ONLY_PKG_INTEGRITY      0.08
#define PCTG_DL_ONLY_FILE_CONFLICTS     0.00
#define PCTG_DL_ONLY_LOAD_PKGFILES      0.00
#define PCTG_DL_ONLY_CHECK_DISKSPACE    0.00
#define PCTG_DL_ONLY_SYSUPGRADE         0.00

/* GtkListStore columns */
enum {
    UCOL_REPO,
    UCOL_PACKAGE,
    UCOL_DESC,
    UCOL_OLD,
    UCOL_NEW,
    UCOL_CUR_DL_SIZE,
    UCOL_CUR_XFRD_SIZE,
    UCOL_TOT_DL_SIZE,
    UCOL_DL_SIZE,
    UCOL_OLD_SIZE,
    UCOL_NEW_SIZE,
    UCOL_NET_SIZE,
    UCOL_DL_IS_ACTIVE,
    UCOL_INST_IS_ACTIVE,
    UCOL_DL_IS_DONE,
    UCOL_DL_IS_DONE_COLOR,
    UCOL_INST_IS_DONE,
    UCOL_PCTG,
    NB_UCOL
};

/* GtkTreeView columns */
enum {
    COL_REPO,
    COL_NAME,
    COL_OLD,
    COL_NEW,
    COL_DOWNLOAD,
    COL_INSTALLED,
    COL_NET,
    NB_COL
};

typedef struct _add_db_t {
    pacman_config_t *pac_conf;
    double pctg;
    double inc;
    alpm_list_t *i;
} add_db_t;

typedef struct _sync_dbs_t {
    gint total;
    gint processed;
} sync_dbs_t;

typedef struct _pkg_iter_t {
    gchar *filename;
    GtkTreeIter *iter;
} pkg_iter_t;

typedef enum {
    LOGTYPE_UNIMPORTANT = 0,
    LOGTYPE_NORMAL,
    LOGTYPE_INFO,
    LOGTYPE_WARNING,
    LOGTYPE_ERROR
} logtype_t;

typedef enum {
    STEP_NONE,
    STEP_SYNC_DBS,
    STEP_WAITING_USER_CONFIRMATION,
    STEP_USER_CONFIRMED,
    STEP_DOWNLOADING,
    STEP_PKG_INTEGRITY,
    STEP_FILE_CONFLICTS,
    STEP_LOAD_PKGFILES,
    STEP_CHECKING_DISKSPACE,
    STEP_UPGRADING,
    STEP_KEYRING,
} steps_t;

struct pacfile
{
    gchar *pkg;
    gchar *oldver;
    gchar *file;
};

typedef struct _updater_t {
    GtkWidget *window;
    GtkWidget *lbl_main;
    GtkWidget *pbar_main;
    GtkWidget *lbl_action;
    GtkWidget *pbar_action;
    GtkListStore *store;
    GtkWidget *list;
    GtkWidget *paned;
    GtkWidget *expander;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *btn_sysupgrade;
    GtkWidget *btn_close;
    GtkWidget *btn_rerun;
    GtkWidget *btn_abort;

    gint pos_expanded;
    gint pos_collapsed;

    steps_t step;
    void *step_data;

    double pctg_download;
    double pctg_pkg_integrity;
    double pctg_file_conflicts;
    double pctg_load_pkgfiles;
    double pctg_check_diskspace;
    double pctg_sysupgrade;
    double pctg_keyring;

    guint step_done;
    double pctg_done;

    guint total_inst;
    guint total_dl;

    KaluUpdater *kupdater;
    gboolean downloadonly;

    alpm_list_t *cmdline_post;

    GArray *pacfile;
} updater_t;

static updater_t *updater = NULL;

static void
add_log (logtype_t type, const gchar *fmt, ...)
{
    GtkTextMark *mark;
    GtkTextIter  iter;
    gchar        buf[1024];
    gchar       *buffer = buf;
    const char  *tag;
    va_list      args;
    int          len;

    va_start (args, fmt);
    len = vsnprintf (buffer, 1024, fmt, args);
    va_end (args);
    if (len >= 1024)
    {
        buffer = new (gchar, ++len);
        va_start (args, fmt);
        vsprintf (buffer, fmt, args);
        va_end (args);
    }

    switch (type)
    {
        case LOGTYPE_ERROR:
            tag = "error";
            break;
        case LOGTYPE_WARNING:
            tag = "warning";
            break;
        case LOGTYPE_INFO:
            tag = "info";
            break;
        case LOGTYPE_UNIMPORTANT:
            tag = "unimportant";
            break;
        case LOGTYPE_NORMAL:
        default:
            tag = "normal";
            break;
    }

    if (type == LOGTYPE_ERROR || type == LOGTYPE_WARNING
            || type == LOGTYPE_INFO)
    {
        gtk_expander_set_expanded (GTK_EXPANDER (updater->expander), TRUE);
    }

    mark = gtk_text_buffer_get_mark (updater->buffer, "end-mark");
    gtk_text_buffer_get_iter_at_mark (updater->buffer, &iter, mark);
    gtk_text_buffer_insert_with_tags_by_name (updater->buffer, &iter,
            buffer, -1, tag, NULL);
    /* scrolling to the end using gtk_text_view_scroll_to_iter doesn't work;
     * using a mark like so seems to work better... */
    gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (updater->text_view),
            mark);
    if (buffer != buf)
    {
        free (buffer);
    }
}

static void
_show_error (const gchar *msg, const gchar *fmt, ...)
{
    gchar *buffer, bufmsg[1024], buf[255];
    va_list args;
    int len;

    snprintf (buf, 255, "<big><b><span color=\"%s\">%s</span></b></big>",
            config->color_error, msg);

    buffer = bufmsg;
    va_start (args, fmt);
    len = vsnprintf (buffer, 1024, fmt, args);
    va_end (args);
    if (len >= 1024)
    {
        /* this is one long error message... */
        buffer = new (gchar, ++len);
        va_start (args, fmt);
        vsprintf (buffer, fmt, args);
        va_end (args);
    }

    add_log (LOGTYPE_ERROR, _("Error: %s: %s\n"), msg, buffer);

    gtk_label_set_markup (GTK_LABEL (updater->lbl_main), buf);
    gtk_widget_hide (updater->pbar_main);
    gtk_label_set_text (GTK_LABEL (updater->lbl_action), buffer);
    gtk_widget_show (updater->lbl_action);
    gtk_widget_hide (updater->pbar_action);
    gtk_widget_set_sensitive (updater->btn_close, TRUE);
    if (updater->downloadonly)
    {
        gtk_widget_set_sensitive (updater->btn_sysupgrade, TRUE);
        gtk_widget_set_sensitive (updater->btn_rerun, TRUE);
    }
    if (buffer != bufmsg)
    {
        free (buffer);
    }
}

static gboolean
text_view_draw_cb (GtkWidget *widget _UNUSED_, gpointer *cr _UNUSED_,
                   gpointer data _UNUSED_)
{
    static gboolean is_first = TRUE;

    /* this is so that the first time the text view is shown, we make sure
     * to scroll to the bottom of the log.
     * add_log() does that, but as long as it's not visible, it doesn't actually
     * matter (and using map, map-event, etc seemed to always fail, i guess
     * because they run too soon/before whatever needs to be done for this
     * to work was done. so, here we go, in draw... */

    if (is_first)
    {
        GtkTextMark *mark;
        mark = gtk_text_buffer_get_mark (updater->buffer, "end-mark");
        gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (updater->text_view),
                mark);
        is_first = FALSE;
    }

    return FALSE;
}

static void
rend_net_size (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
           GtkTreeModel *store, GtkTreeIter *iter)
{
    rend_size (column, renderer, store, iter, UCOL_NET_SIZE, 0);
}

static void
rend_size_pb (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
           GtkTreeModel *store, GtkTreeIter *iter, gboolean is_dl)
{
    gint col_is_active, col_data;
    gboolean is_active;

    if (is_dl)
    {
        col_is_active = UCOL_DL_IS_ACTIVE;
        col_data = UCOL_DL_SIZE;
    }
    else
    {
        col_is_active = UCOL_INST_IS_ACTIVE;
        col_data = UCOL_NEW_SIZE;
    }

    gtk_tree_model_get (store, iter, col_is_active, &is_active, -1);
    gtk_cell_renderer_set_visible (renderer, !is_active);
    if (!is_active)
    {
        rend_size (column, renderer, store, iter, col_data, 1);
    }
}

static void
rend_pbar_pb (GtkTreeViewColumn *column _UNUSED_, GtkCellRenderer *renderer,
           GtkTreeModel *store, GtkTreeIter *iter, gboolean is_dl)
{
    gint col_is_active, val;
    gboolean is_active;
    double pctg;

    if (is_dl)
    {
        col_is_active = UCOL_DL_IS_ACTIVE;
    }
    else
    {
        col_is_active = UCOL_INST_IS_ACTIVE;
    }

    gtk_tree_model_get (store, iter,
            col_is_active,  &is_active,
            UCOL_PCTG,      &pctg,
            -1);
    gtk_cell_renderer_set_visible (renderer, is_active);
    val = (gint) (pctg * 100);
    g_object_set (renderer, "value", val, NULL);

    if (is_dl)
    {
        guint dl_size;
        char buf[255], buf_tot[23];
        double size;
        const char *unit;

        gtk_tree_model_get (store, iter, UCOL_CUR_DL_SIZE, &dl_size, -1);
        size = humanize_size (dl_size, '\0', &unit);
        snprint_size (buf_tot, 23, size, unit);
        snprintf (buf, 255, _("%d%% of %s"), val, buf_tot);
        g_object_set (renderer, "text", buf, NULL);
    }
    else if (val == 100)
    {
        g_object_set (renderer, "text", _("Post-processing..."), NULL);
    }
    else
    {
        g_object_set (renderer, "text", NULL, NULL);
    }
}

static GtkTreeIter *
get_iter_for_pkg (const gchar *pkg)
{
    GtkTreeIter  *iter;
    GtkTreeModel *model;
    const gchar  *name;

    iter = new0 (GtkTreeIter, 1);
    model = GTK_TREE_MODEL (updater->store);

    gtk_tree_model_get_iter_first (model, iter);
    do
    {
        gtk_tree_model_get (model, iter,
                UCOL_PACKAGE,   &name,
                -1);

        if (g_strcmp0 (pkg, name) == 0)
        {
            return iter;
        }

    } while (gtk_tree_model_iter_next (model, iter));

    free (iter);
    return NULL;
}

static void
on_debug (KaluUpdater *kupdater _UNUSED_, const gchar *msg, gpointer data _UNUSED_)
{
    debug ("[kalu-updater] %s", msg);
}

static void
on_log (KaluUpdater *kupdater _UNUSED_, loglevel_t level, const gchar *msg)
{
    if (level & LOG_ERROR)
    {
        add_log (LOGTYPE_ERROR, _("Error: %s"), msg);
    }
    else if (level & LOG_WARNING)
    {
        add_log (LOGTYPE_WARNING, _("Warning: %s"), msg);
    }
    else if (level & LOG_DEBUG)
    {
        debug ("ALPM: %s", msg);
    }
    else if (level & LOG_FUNCTION)
    {
        add_log (LOGTYPE_UNIMPORTANT, _("ALPM: %s"), msg);
    }
}

static void
on_total_download (KaluUpdater *kupdater _UNUSED_, guint total)
{
    updater->total_dl = total;
}

static void
on_event (KaluUpdater *kupdater _UNUSED_, event_t event)
{
    const gchar *msg = NULL;
    int upd_action = updater->step == STEP_NONE;

    switch (event)
    {
        case EVENT_RETRIEVING_PKGS:
            msg = _("Downloading packages...");
            break;
        case EVENT_RETRIEVING_PKGS_DONE:
        case EVENT_RETRIEVING_PKGS_FAILED:
            if (updater->step == STEP_DOWNLOADING)
            {
                updater->pctg_done += updater->pctg_download;
            }
            break;
        case EVENT_CHECKING_DEPS:
            msg = _("Checking dependencies...");
            break;
        case EVENT_RESOLVING_DEPS:
            msg = _("Resolving dependencies...");
            break;
        case EVENT_INTERCONFLICTS:
            msg = _("Checking inter-conflict...");
            break;
        case EVENT_TRANSACTION:
            msg = _("Processing package changes...");
            break;
        case EVENT_HOOKS_PRE:
            msg = _("Running pre-transaction hooks...");
            upd_action = 1;
            break;
        case EVENT_HOOKS_POST:
            msg = _("Running post-transaction hooks...");
            upd_action = 1;
            break;
        case EVENT_KEY_DOWNLOAD:
            msg = _("Importing required keys...");
            break;
        default:
            return;
    }
    if (msg)
    {
        add_log (LOGTYPE_NORMAL, "%s\n", msg);
    }
    if (upd_action)
    {
        gtk_label_set_text (GTK_LABEL (updater->lbl_action), msg);
        gtk_widget_show (updater->lbl_action);
        gtk_widget_hide (updater->pbar_action);
    }
}

static void
on_event_hookrun (KaluUpdater *kupdater _UNUSED_, event_type_t type, int total,
                  int position, const gchar *hook, const gchar *desc)
{
    double pctg;

    if (type == EVENT_TYPE_START)
    {
        pctg = ((double) position - 1.0) / total;
        if (*desc != '\0')
            add_log (LOGTYPE_NORMAL, _("Running '%s' (%s)...\n"), hook, desc);
        else
            add_log (LOGTYPE_NORMAL, _("Running '%s'...\n"), hook);
    }
    else
        pctg = (double) position / total;

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action), pctg);
    gtk_widget_show (updater->pbar_action);
}

static void
finish_pkg_install (const char *pkg, const char *version _UNUSED_)
{
    pkg_iter_t *pkg_iter = updater->step_data;
    guint size = 0;

    if (g_strcmp0 (pkg, pkg_iter->filename) != 0)
    {
        /* locate pkg in tree */
        pkg_iter->iter = get_iter_for_pkg (pkg);
        if (pkg_iter->iter == NULL)
        {
            debug ("finish_pkg_install: unable to find iter for %s", pkg);
            return;
        }
        if (pkg_iter->filename != NULL)
        {
            free (pkg_iter->filename);
        }
        pkg_iter->filename = strdup (pkg);

        /* make sure it's visible */
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (updater->list),
                gtk_tree_model_get_path (GTK_TREE_MODEL (updater->store),
                    pkg_iter->iter),
                NULL,
                FALSE,
                0,
                0);
    }

    /* pkg progress */
    gtk_list_store_set (updater->store, pkg_iter->iter,
            UCOL_INST_IS_ACTIVE, FALSE,
            UCOL_INST_IS_DONE,   TRUE,
            -1);

    /* get pkg size */
    gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
            UCOL_NEW_SIZE,  &size,
            -1);
    if (size == 0)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                UCOL_OLD_SIZE,  &size,
                -1);
    }

    updater->step_done += size;
    /* free */
    free (pkg_iter->filename);
    pkg_iter->filename = NULL;
    free (pkg_iter->iter);
    pkg_iter->iter = NULL;
}

static void
on_event_installed (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                    const gchar *version, alpm_list_t *optdeps)
{
    alpm_list_t *i;

    add_log (LOGTYPE_NORMAL, _("Package installed: %s (%s)\n"), pkg, version);
    finish_pkg_install (pkg, version);

    if (optdeps != NULL)
    {
        add_log (LOGTYPE_INFO, _("Optional dependencies for %s:\n"), pkg);
        FOR_LIST (i, optdeps)
        {
            add_log (LOGTYPE_INFO, "- %s\n", (const char *) i->data);
        }
    }
}

static void
on_event_reinstalled (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                      const gchar *version)
{
    add_log (LOGTYPE_NORMAL, _("Package reinstalled: %s (%s)\n"), pkg, version);
    finish_pkg_install (pkg, version);
}

static void
on_event_removed (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                    const gchar *version)
{
    add_log (LOGTYPE_NORMAL, _("Package removed: %s (%s)\n"), pkg, version);
    finish_pkg_install (pkg, version);
}

static void
on_event_upgraded (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                   const gchar *old_version, const gchar *new_version,
                   alpm_list_t *newoptdeps)
{
    alpm_list_t *i;

    add_log (LOGTYPE_NORMAL, _("Package upgraded: %s (%s -> %s)\n"),
            pkg, old_version, new_version);
    finish_pkg_install (pkg, new_version);

    if (newoptdeps != NULL)
    {
        add_log (LOGTYPE_INFO, _("New optional dependencies for %s-%s:\n"),
                pkg, new_version);
        FOR_LIST (i, newoptdeps)
        {
            add_log (LOGTYPE_INFO, "- %s\n", (const char *) i->data);
        }
    }
}

static void
on_event_downgraded (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                     const gchar *old_version, const gchar *new_version,
                     alpm_list_t *newoptdeps)
{
    alpm_list_t *i;

    add_log (LOGTYPE_NORMAL, _("Package downgraded: %s (%s -> %s)\n"),
            pkg, old_version, new_version);
    finish_pkg_install (pkg, new_version);

    if (newoptdeps != NULL)
    {
        add_log (LOGTYPE_INFO, _("New optional dependencies for %s-%s:\n"),
                pkg, new_version);
        FOR_LIST (i, newoptdeps)
        {
            add_log (LOGTYPE_INFO, "- %s\n", (const char *) i->data);
        }
    }
}

static void
on_event_pkgdownload_start (KaluUpdater *kupdater _UNUSED_, const gchar *filename)

{
    if (updater->step == STEP_USER_CONFIRMED)
    {
        /* adding "Downloading packages" to the log is done on corresponding event */
        gtk_label_set_text (GTK_LABEL (updater->lbl_action),
                _("Downloading packages..."));
        gtk_progress_bar_set_fraction (
                GTK_PROGRESS_BAR (updater->pbar_action), 0.0);
        gtk_widget_show (updater->lbl_action);
        gtk_widget_show (updater->pbar_action);
        updater->step = STEP_DOWNLOADING;
        updater->step_done = 0;
        updater->step_data = new0 (pkg_iter_t, 1);
    }
    else if (G_UNLIKELY (updater->step != STEP_DOWNLOADING))
    {
        debug ("on_pkgdownload_start: invalid step: %d", updater->step);
        return;
    }

    /* first, we need to find the package we're dealing with */
    pkg_iter_t *pkg_iter = updater->step_data;
    gchar *pkg, *s;
    int i;

    free (pkg_iter->filename);
    pkg_iter->filename = NULL;

    /* we want the package name only: the third dash from the last is the one
     * between pkgname and pkgver, since filenames follow:
     * PACKAGE-[EPOCH:]PKGVER-PKGREL-ARCH.pkg.EXT (and no dash is allowed in
     * PKGVER) */

    s = pkg = strdup (filename);
    for (i = 0; i < 3; ++i)
    {
        if (NULL == (s = strrchr (pkg, '-')))
        {
            free (pkg);
            debug ("on_download: invalid filename: %s", filename);
            return;
        }
        *s = '\0';
    }

    /* locate pkg in tree */
    pkg_iter->iter = get_iter_for_pkg (pkg);
    if (pkg_iter->iter == NULL)
    {
        free (pkg);
        debug ("on_download: unable to find iter for %s", pkg);
        return;
    }
    free (pkg);
    pkg_iter->filename = strdup (filename);

    /* make sure it's visible */
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (updater->list),
            gtk_tree_model_get_path (
                GTK_TREE_MODEL (updater->store),
                pkg_iter->iter),
            NULL,
            FALSE,
            0,
            0);

    {
        guint size;

        /* get download size as default/initial value. Most of the time, it'll
         * stay the same, but in case of multiple deltas it might change once
         * the download is initiated */
        gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
            UCOL_DL_SIZE,       &size,
            -1);
        /* pkg progress */
        gtk_list_store_set (updater->store, pkg_iter->iter,
                UCOL_PCTG,          0.0,
                UCOL_CUR_DL_SIZE,   size,
                UCOL_CUR_XFRD_SIZE, 0,
                UCOL_DL_IS_ACTIVE,  TRUE,
                UCOL_DL_IS_DONE,    FALSE,
                -1);
    }
}

static void
on_event_pkgdownload_done (KaluUpdater *kupdater _UNUSED_,
                           const gchar *file _UNUSED_)

{
    if (G_UNLIKELY (updater->step != STEP_DOWNLOADING))
    {
        debug ("on_pkgdownload_done: invalid step: %d", updater->step);
        return;
    }

    pkg_iter_t *pkg_iter = updater->step_data;
    guint dl_size, cur_size, tot_size;
    gboolean dl_done;

    gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
            UCOL_DL_SIZE,       &dl_size,
            UCOL_CUR_XFRD_SIZE, &cur_size,
            UCOL_TOT_DL_SIZE,   &tot_size,
            -1);
    dl_done = (cur_size + tot_size == dl_size);

    /* pkg progress */
    gtk_list_store_set (updater->store, pkg_iter->iter,
            UCOL_DL_IS_ACTIVE,      FALSE,
            UCOL_DL_IS_DONE,        TRUE,
            UCOL_CUR_DL_SIZE,       0,
            UCOL_CUR_XFRD_SIZE,     0,
            UCOL_TOT_DL_SIZE,       cur_size + tot_size,
            (dl_done) ? UCOL_DL_IS_DONE_COLOR : -1,  config->color_info,
            -1);

    /* free */
    free (pkg_iter->filename);
    pkg_iter->filename = NULL;
    free (pkg_iter->iter);
    pkg_iter->iter = NULL;
}

static void
on_event_pkgdownload_failed (KaluUpdater *kupdater _UNUSED_,
                             const gchar *file _UNUSED_)

{
    if (G_UNLIKELY (updater->step != STEP_DOWNLOADING))
    {
        debug ("on_pkgdownload_failed: invalid step: %d", updater->step);
        return;
    }

    pkg_iter_t *pkg_iter = updater->step_data;
    guint cur_size, tot_size;

    gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
            UCOL_CUR_XFRD_SIZE, &cur_size,
            UCOL_TOT_DL_SIZE,   &tot_size,
            -1);

    /* pkg progress */
    gtk_list_store_set (updater->store, pkg_iter->iter,
            UCOL_DL_IS_ACTIVE,      FALSE,
            UCOL_DL_IS_DONE,        TRUE,
            UCOL_CUR_DL_SIZE,       0,
            UCOL_CUR_XFRD_SIZE,     0,
            UCOL_TOT_DL_SIZE,       cur_size + tot_size,
            UCOL_DL_IS_DONE_COLOR,  config->color_error,
            -1);

    /* free */
    free (pkg_iter->filename);
    pkg_iter->filename = NULL;
    free (pkg_iter->iter);
    pkg_iter->iter = NULL;
}

static void
on_event_scriptlet (KaluUpdater *kupdater _UNUSED_, const gchar *msg)
{
    add_log (LOGTYPE_INFO, "%s", msg);
}

static void
add_pacnew (const gchar *pkg, const gchar *old_version, const gchar *oldfile)
{
    struct pacfile pf = { 0, };
    gchar buf[255], *b = buf;
    gint len;

    if (!updater->pacfile)
        updater->pacfile = g_array_new (FALSE, FALSE, sizeof (struct pacfile));

    pf.pkg = g_strdup (pkg);
    pf.oldver = g_strdup (old_version);
    len = snprintf (buf, 255, "%s.pacnew", oldfile);
    if (G_UNLIKELY (len >= 255))
    {
        b = g_strdup_printf ("%s.pacnew", oldfile);
    }
    pf.file = g_shell_quote (b);
    if (G_UNLIKELY (b != buf))
    {
        g_free (b);
    }
    g_array_append_val (updater->pacfile, pf);
}

static void
on_event_pacnew_created (KaluUpdater *kupdater _UNUSED_,
                         gboolean from_noupgrade _UNUSED_, const gchar *pkg,
                         const gchar *old_version, const gchar *new_version _UNUSED_,
                         const gchar *file)
{
    add_pacnew (pkg, old_version, file);
    add_log (LOGTYPE_WARNING, _("Warning: %s installed as %s.pacnew\n"),
            file, file);
}

static void
on_event_pacsave_created (KaluUpdater *kupdater _UNUSED_, const gchar *pkg _UNUSED_,
                          const gchar *version _UNUSED_, const gchar *file)
{
    add_log (LOGTYPE_WARNING, _("Warning: %s saved as %s.pacsave\n"),
            file, file);
}

static void
on_event_optdep_removal (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                          const gchar *optdep)
{
    add_log (LOGTYPE_INFO, _("%s optionally requires %s\n"), pkg, optdep);
}

static void
on_progress (KaluUpdater *kupdater _UNUSED_, event_t event, const gchar *pkg,
             int percent, guint total _UNUSED_, guint current _UNUSED_)
{
    steps_t      step;
    const gchar *msg;
    double       pctg_step;

    switch (event)
    {
        case EVENT_PKG_INTEGRITY:
            step = STEP_PKG_INTEGRITY;
            msg = _("Checking packages integrity...");
            pctg_step = updater->pctg_pkg_integrity;
            break;
        case EVENT_FILE_CONFLICTS:
            step = STEP_FILE_CONFLICTS;
            msg = _("Checking for file conflicts...");
            pctg_step = updater->pctg_file_conflicts;
            break;
        case EVENT_LOAD_PKGFILES:
            step = STEP_LOAD_PKGFILES;
            msg = _("Loading package files...");
            pctg_step = updater->pctg_load_pkgfiles;
            break;
        case EVENT_CHECKING_DISKSPACE:
            step = STEP_CHECKING_DISKSPACE;
            msg = _("Checking disk space...");
            pctg_step = updater->pctg_check_diskspace;
            break;
        case EVENT_KEYRING:
            step = STEP_KEYRING;
            msg = _("Checking keyring...");
            pctg_step = updater->pctg_keyring;
            break;
        case EVENT_INSTALLING:
        case EVENT_REINSTALLING:
        case EVENT_UPGRADING:
        case EVENT_DOWNGRADING:
        case EVENT_REMOVING:
            step = STEP_UPGRADING;
            msg = _("Upgrading system...");
            break;
        default:
            return;
    }

    if (updater->step != step)
    {
        if (NULL != updater->step_data)
        {
            free (updater->step_data);
            updater->step_data = NULL;
        }
        updater->step = step;
        gtk_label_set_text (GTK_LABEL (updater->lbl_action), msg);
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action),
                0.0);
        gtk_widget_show (updater->lbl_action);
        gtk_widget_show (updater->pbar_action);
        if (updater->step == STEP_UPGRADING)
        {
            updater->step_done = 0;
            updater->step_data = new0 (pkg_iter_t, 1);
        }
    }

    if (step == STEP_UPGRADING)
    {
        /* special case, since we need to update single progress in the Treeview */

        double      pctg        = (double) percent / 100;
        pkg_iter_t *pkg_iter    = updater->step_data;
        guint       size = 0;

        if (g_strcmp0 (pkg, pkg_iter->filename) != 0)
        {
            /* locate pkg in tree */
            pkg_iter->iter = get_iter_for_pkg (pkg);
            if (pkg_iter->iter == NULL)
            {
                debug ("on_progress: unable to find iter for %s", pkg);
                return;
            }
            if (pkg_iter->filename != NULL)
            {
                free (pkg_iter->filename);
            }
            pkg_iter->filename = strdup (pkg);

            /* make sure it's visible */
            gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (updater->list),
                    gtk_tree_model_get_path (GTK_TREE_MODEL (updater->store),
                        pkg_iter->iter),
                    NULL,
                    FALSE,
                    0,
                    0);
        }

        /* pkg progress */
        gtk_list_store_set (updater->store, pkg_iter->iter,
                UCOL_PCTG,           pctg,
                UCOL_INST_IS_ACTIVE, TRUE,
                -1);

        /* get pkg size */
        gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                UCOL_NEW_SIZE,  &size,
                -1);
        if (size == 0)
        {
            gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                    UCOL_OLD_SIZE,  &size,
                    -1);
        }

        if (updater->total_inst > 0)
        {
            /* upgrading progress */
            guint done;

            done = (guint) (size * pctg);
            pctg = (double) (updater->step_done + done) / updater->total_inst;

            gtk_progress_bar_set_fraction (
                    GTK_PROGRESS_BAR (updater->pbar_action), pctg);

            /* global progress */
            pctg = updater->pctg_done + (pctg * updater->pctg_sysupgrade);
            gtk_progress_bar_set_fraction (
                    GTK_PROGRESS_BAR (updater->pbar_main), pctg);
        }
    }
    else
    {
        /* simpler stuff: progress in the pbar_action */
        double pctg = (double) percent / 100;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action),
                pctg);
        /* global progress */
        pctg *= pctg_step;
        pctg += updater->pctg_done;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main),
                pctg);
        if (percent == 100)
        {
            updater->pctg_done += pctg_step;
        }
    }
}

static gboolean
on_install_ignorepkg (KaluUpdater *kupdater _UNUSED_, const gchar *pkg)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;

    snprintf (question, 255,
            _("%s is in IgnorePkg/IgnoreGroup. Install anyway ?"),
            pkg);
    snprintf (lbl_yes, 42,   _("Install %s anyway"), pkg);
    snprintf (lbl_no,  42,   _("Do not install %s"), pkg);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, NULL, lbl_yes, NULL, lbl_no, NULL,
            updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? _("Yes") : _("No"));
    return answer;
}

static gboolean
on_replace_pkg (KaluUpdater *kupdater _UNUSED_, const gchar *repo1,
        const gchar *pkg1, const gchar *repo2, const gchar *pkg2)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;

    snprintf (question, 255,
            _("Do you want to replace %s (%s) with %s (%s) ?"),
            pkg1, repo1, pkg2, repo2);
    snprintf (lbl_yes, 42,   _("Replace with %s"), pkg2);
    snprintf (lbl_no,  42,   _("Keep %s"), pkg1);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, NULL, lbl_yes, NULL, lbl_no, NULL,
            updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? _("Yes"): _("No"));
    return answer;
}

static gboolean
on_conflict_pkg (KaluUpdater *kupdater _UNUSED_, const gchar *pkg1, const gchar *pkg2,
                 const gchar *reason)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;

    snprintf (question, 255,
            _("%s and %s and in conflict. Remove %s ?"),
            pkg1, pkg2, pkg2);
    snprintf (lbl_yes, 42,   _("Remove %s"), pkg2);
    snprintf (lbl_no,  42,   _("Keep %s"), pkg2);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, reason, lbl_yes, NULL, lbl_no, NULL,
            updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? _("Yes") : _("No"));
    return answer;
}

static gboolean
on_remove_pkgs (KaluUpdater *kupdater _UNUSED_, alpm_list_t *pkgs)
{
    gchar question[255], lbl_yes[42], buf[255];
    gboolean answer;

    if (alpm_list_count (pkgs) > 1)
    {
        alpm_list_t *i;
        gchar *s, *ss;
        const gchar *q = _("Do you want to skip the packages listed below ?");

        s = g_strdup (_("Those packages cannot be upgraded due to "
                    "unresolvable dependencies :\n"));
        add_log (LOGTYPE_INFO, "%s\n", q);
        add_log (LOGTYPE_INFO, "%s", s);
        FOR_LIST (i, pkgs)
        {
            snprintf (buf, 255, "- %s\n", (const char *) i->data);
            ss = g_strconcat (s, buf, NULL);
            g_free (s);
            s = ss;
            add_log (LOGTYPE_INFO, "%s", buf);
        }

        answer = confirm (q, s,
                _("Yes, skip those packages"), NULL,
                _("No, abort upgrade"), NULL,
                updater->window);
        g_free (s);
    }
    else
    {
        snprintf (question, 255,
                _("Do you want to skip package %s ?"),
                (const char *) pkgs->data);
        snprintf (buf, 255,
                _("Package %s cannot be upgraded due to unresolvable dependencies."),
                (const char *) pkgs->data);
        snprintf (lbl_yes, 42,
                _("Yes, skip %s"),
                (const char *) pkgs->data);
        add_log (LOGTYPE_INFO, "%s\n", question);
        add_log (LOGTYPE_INFO, "%s\n", buf);
        answer = confirm (question, buf,
                lbl_yes,                NULL,
                _("No, abort upgrade"), NULL,
                updater->window);
    }
    add_log (LOGTYPE_INFO, "\t%s\n", (answer) ? _("Yes") : _("No"));
    return answer;
}

static void
provider_tree_selection_changed_cb (GtkTreeSelection *selection, gpointer button)
{
    GtkTreeModel *model;
    gtk_widget_set_sensitive (button,
            gtk_tree_selection_get_selected (selection, &model, NULL));
}

static void
provider_button_ok_clicked_cb (GtkButton *button _UNUSED_, gpointer ptr[2])
{
    GtkDialog *dialog = ptr[0];
    GtkWidget *list   = ptr[1];
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gint rc;
        gtk_tree_model_get (model, &iter, 0, &rc, -1);
        gtk_dialog_response (dialog, rc);
    }
}

static gboolean
provider_tree_clicked_cb (GtkWidget *tree, GdkEvent *event, gpointer ptr[2])
{
    GtkButton *button = ptr[1];

    if (event->type == GDK_2BUTTON_PRESS)
    {
        ptr[1] = tree;
        provider_button_ok_clicked_cb (button, ptr);
        ptr[1] = button;
        return TRUE;
    }
    return FALSE;
}

static gboolean
provider_tree_key_pressed_cb (GtkWidget *tree, GdkEventKey *event, gpointer ptr[2])
{
    GtkDialog *dialog = ptr[0];
    GtkButton *button = ptr[1];

    if (event->keyval == GDK_KEY_Return)
    {
        ptr[1] = tree;
        provider_button_ok_clicked_cb (button, ptr);
        ptr[1] = button;
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Escape)
    {
        gtk_dialog_response (dialog, 0);
        return TRUE;
    }
    return FALSE;
}

static int
on_select_provider (KaluUpdater *kupdater _UNUSED_, const gchar *pkg, alpm_list_t *providers)
{
    gchar question[255];
    snprintf (question, 255,
            _("There are %d providers available for %s.\nPlease select the one to install :"),
            (int) alpm_list_count (providers),
            pkg);
    add_log (LOGTYPE_INFO, "%s\n", question);

    /* dialog */
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new_with_markup (
            GTK_WINDOW (updater->window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_NONE,
            NULL);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), question);
    gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), TRUE);

    GtkWidget *box;
    box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    /* store for the list */
    GtkListStore *store;
    store = gtk_list_store_new (4,
            G_TYPE_INT,     /* id */
            G_TYPE_STRING,  /* repo */
            G_TYPE_STRING,  /* pkg */
            G_TYPE_STRING  /* version */
            );

    /* said list */
    GtkWidget *list;
    list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    g_object_unref (store);
    /* hint for alternate row colors */
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list), TRUE);
    G_GNUC_END_IGNORE_DEPRECATIONS

    /* scrolledwindow for list */
    GtkWidget *scrolled_window;
    scrolled_window = gtk_scrolled_window_new (
            gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (list)),
            gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (list)));
    gtk_box_pack_start (GTK_BOX (box), scrolled_window, TRUE, FALSE, 0);
    gtk_widget_show (scrolled_window);

    /* cell renderer & column(s) */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    renderer = gtk_cell_renderer_text_new ();
    /* column: Repo */
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Repo"),
            renderer,
            "text", 1,
            NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Package */
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Package"),
            renderer,
            "text", 2,
            NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Version */
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Version"),
            renderer,
            "text", 3,
            NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);


    gtk_container_add (GTK_CONTAINER (scrolled_window), list);
    gtk_widget_show (list);

    /* content: providers */
    GtkTreeIter iter;
    gint id = 0;
    alpm_list_t *i;

    FOR_LIST (i, providers)
    {
        provider_t *provider = i->data;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                0,  ++id,
                1,  provider->repo,
                2,  provider->pkg,
                3,  provider->version,
                -1);
        add_log (LOGTYPE_INFO, "- %d. %s/%s-%s\n", id, provider->repo,
                provider->pkg, provider->version);
    }

    /* button Cancel */
    GtkWidget *button;
    button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("Abort"), 0);
    /* button Use selected provider */
    GtkWidget *image;
    image = gtk_image_new_from_icon_name ("gtk-apply", GTK_ICON_SIZE_MENU);
    button = gtk_button_new_with_label (_("Use selected provider"));
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_widget_show (button);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    box = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
    G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

    gpointer ptr1[2] = {dialog, list};
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (provider_button_ok_clicked_cb), ptr1);
    gpointer ptr2[2] = {dialog, button};
    g_signal_connect (G_OBJECT (list), "button-press-event",
            G_CALLBACK (provider_tree_clicked_cb), ptr2);
    g_signal_connect (G_OBJECT (list), "key-press-event",
            G_CALLBACK (provider_tree_key_pressed_cb), ptr2);

    /* selection */
    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
    g_signal_connect (G_OBJECT (selection), "changed",
            G_CALLBACK (provider_tree_selection_changed_cb), button);

    gtk_widget_show (dialog);
    gtk_tree_selection_unselect_all (selection);

    /* show it */
    gint rc = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    add_log (LOGTYPE_INFO, "\t%d\n", rc);
    return rc;
}

static gboolean
on_corrupted_pkg (KaluUpdater *kupdater _UNUSED_, const gchar *file, const gchar *error)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;

    snprintf (question, 255,
            _("File %s is corrupted. Do you want to delete it ?"),
            file);
    snprintf (lbl_yes, 42,   _("Delete %s"), file);
    snprintf (lbl_no,  42,   _("Do not delete %s"), file);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, error, lbl_yes, NULL, lbl_no, NULL,
            updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? _("Yes") : _("No"));
    return answer;
}

static gboolean
on_import_key (KaluUpdater *kupdater _UNUSED_, const gchar *key_fingerprint,
               const gchar *key_uid, const gchar *key_created)
{
    gchar question[255], details[255];
    gchar *det;
    gboolean answer;

    snprintf (question, 255, _("Do you want to import key %s ?"),
            key_fingerprint);
    snprintf (details,  255, _("Key %s: %s (created %s)"),
            key_fingerprint,
            key_uid,
            key_created);
    det = g_markup_escape_text (details, -1);
    add_log (LOGTYPE_INFO, "%s\n%s\n", question, det);
    answer = confirm (question, det, _("Yes, import key"), NULL, _("No"), NULL,
            updater->window);
    g_free (det);
    add_log (LOGTYPE_INFO, "\t%s\n", (answer) ? _("Yes") : _("No"));
    return answer;
}

static void
on_download (KaluUpdater *kupdater _UNUSED_, const gchar *filename,
        guint xfered, guint total)
{
    switch (updater->step)
    {
        case STEP_SYNC_DBS:
            {
                sync_dbs_t *sync_dbs = updater->step_data;
                double pctg;

                /* are we done? */
                if (xfered > 0 && xfered == total)
                {
                    gtk_widget_hide (updater->lbl_action);
                    gtk_widget_hide (updater->pbar_action);
                }
                else if (total > 0)
                {
                    /* pctg of this download */
                    pctg = (double) xfered / total;
                    gtk_progress_bar_set_fraction (
                            GTK_PROGRESS_BAR (updater->pbar_action), pctg);
                    /* reduce pctg to value of one db */
                    pctg *= 1.0 / sync_dbs->total;
                    /* add already processed dbs */
                    pctg += (double) sync_dbs->processed / sync_dbs->total;
                    /* full sync-ing pctg */
                    gtk_progress_bar_set_fraction (
                            GTK_PROGRESS_BAR (updater->pbar_main), pctg);

                    if (!gtk_widget_get_visible (updater->lbl_action))
                    {
                        gchar buf[255];
                        snprintf (buf, 255, _("Downloading %s ..."), filename);
                        gtk_label_set_text (GTK_LABEL (updater->lbl_action),
                                buf);
                        gtk_widget_show (updater->lbl_action);
                        gtk_widget_show (updater->pbar_action);
                    }
                }
            }
            break;

        case STEP_DOWNLOADING:
            {
                /* first, we need to find the package we're dealing with */
                double      pctg     = (total > 0) ? (double) xfered / total : 0;
                pkg_iter_t *pkg_iter = updater->step_data;

                if (G_UNLIKELY (g_strcmp0 (filename, pkg_iter->filename) != 0))
                {
                    debug ("on_download: invalid iter, expected '%s', got '%s'",
                            filename, pkg_iter->filename);
                    return;
                }

                /* pkg progress */

                /* story goes like this: in UCOL_DL_SIZE we have the download
                 * size as reported by ALPM. This might be the size of the file
                 * to download, or the combined sizes of all deltas.
                 * UCOL_TOT_DL_SIZE is the size of previous downloads, i.e.
                 * previous deltas for that package, or when resuming from
                 * another mirror (see below).
                 *
                 * So, we use xfered & total as UCOL_CUR_{XFRD,DL}_SIZE to keep
                 * track of the sizes for the current download (file). However,
                 * in case of a download failure, ALPM doesn't trigger the
                 * pkgdownload_failed event, but instead moves on to (resumes
                 * from) the next mirror. (Only when out of mirrors does the
                 * pkgdownload_failed occurs.)
                 *
                 * So, here the xfered size might be reset (and the total size
                 * shrunken) as we're (silectly) resuming the download (from
                 * another mirror), and because we try to keep track of how much
                 * was downloaded so we can tell when the download is "fully"
                 * done (as in, all deltas), we need to keep track.
                 */
                {
                    guint old_xfered, old_total, tot_size;

                    gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                            UCOL_CUR_XFRD_SIZE, &old_xfered,
                            UCOL_CUR_DL_SIZE,   &old_total,
                            UCOL_TOT_DL_SIZE,   &tot_size,
                            -1);
                    if (xfered < old_xfered && total < old_total)
                    {
                        gtk_list_store_set (updater->store, pkg_iter->iter,
                                UCOL_PCTG,          pctg,
                                UCOL_CUR_XFRD_SIZE, xfered,
                                UCOL_CUR_DL_SIZE,   total,
                                UCOL_TOT_DL_SIZE,   tot_size + old_xfered,
                                -1);
                        updater->step_done += old_xfered;
                    }
                    else
                    {
                        gtk_list_store_set (updater->store, pkg_iter->iter,
                                UCOL_PCTG,          pctg,
                                UCOL_CUR_XFRD_SIZE, xfered,
                                UCOL_CUR_DL_SIZE,   total,
                                -1);
                    }
                }

                if (updater->total_dl > 0)
                {
                    /* download progress */
                    pctg = (double) (updater->step_done + xfered) / updater->total_dl;
                    gtk_progress_bar_set_fraction (
                            GTK_PROGRESS_BAR (updater->pbar_action), pctg);

                    /* global progress */
                    pctg = updater->pctg_done + (pctg * updater->pctg_download);
                    gtk_progress_bar_set_fraction (
                            GTK_PROGRESS_BAR (updater->pbar_main), pctg);
                }

                /* are we done? */
                if (xfered == total)
                {
                    updater->step_done += total;
                }
            }
            break;

        default:
            debug ("got download activity outside of a valid step: %s %d/%d",
                    filename, xfered, total);
            break;
    }
}

static void
on_sync_dbs (KaluUpdater *kupdater _UNUSED_, gint nb)
{
    if (updater->step != STEP_SYNC_DBS)
    {
        add_log (LOGTYPE_ERROR,
                _("Error: got a SYNC_DBS event while not synchronizing databases\n"));
        return;
    }

    sync_dbs_t *sync_db = updater->step_data;
    sync_db->total = nb;
    add_log (LOGTYPE_UNIMPORTANT,
            _n("Synchronizing a database\n", "Synchronizing %d databases\n",
                (long unsigned int) nb),
            nb);
    gtk_label_set_text (GTK_LABEL (updater->lbl_main), (!updater->downloadonly)
            ? _("Synchronizing databases...") : _("Synchronizing simulation databases..."));
}

static void
on_sync_db_start (KaluUpdater *kupdater _UNUSED_, const gchar *name)
{
    if (updater->step != STEP_SYNC_DBS)
    {
        add_log (LOGTYPE_ERROR,
                _("Error: got a SYNC_DB_START event while not synchronizing databases\n"));
        return;
    }

    sync_dbs_t *sync_db = updater->step_data;
    double pctg;

    add_log (LOGTYPE_NORMAL, _("Synchronizing database %s... "), name);
    pctg = (double) sync_db->processed / sync_db->total;
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), pctg);
}

static void
on_sync_db_end (KaluUpdater *kupdater _UNUSED_, sync_db_results_t result)
{
    if (updater->step != STEP_SYNC_DBS)
    {
        add_log (LOGTYPE_ERROR,
                _("Error: got a SYNC_DB_END event while not synchronizing databases\n"));
        return;
    }

    sync_dbs_t *sync_db = updater->step_data;
    double pctg;

    if (result == SYNC_FAILURE)
    {
        add_log (LOGTYPE_ERROR, _("failed\n"));
    }
    else
    {
        add_log (LOGTYPE_NORMAL, _("ok\n"));
    }

    if (sync_db->total > 0)
    {
        pctg = (double) ++(sync_db->processed) / sync_db->total;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main),
                pctg);
    }
}

static void
free_kupdater (gboolean unref)
{
    if (updater->kupdater == NULL)
        return;

    if (updater->step_data != NULL)
    {
        free (updater->step_data);
        updater->step_data = NULL;
    }
    if (updater->step == STEP_WAITING_USER_CONFIRMATION)
    {
        kalu_updater_no_sysupgrade (updater->kupdater, NULL, NULL, NULL, NULL);
        updater->step = STEP_NONE;
    }
    kalu_updater_free_alpm (updater->kupdater, NULL, NULL, NULL, NULL);

    if (unref)
    {
        g_object_unref (updater->kupdater);
        updater->kupdater = NULL;
    }
}

static void
btn_close_cb (GtkButton *button _UNUSED_, gpointer data _UNUSED_)
{
    if (updater->kupdater != NULL)
    {
        free_kupdater (FALSE);
        /*FIXME*/
    }
    else
        kalu_alpm_free ();
    gtk_widget_destroy (updater->window);
}

static void
btn_abort_cb (GtkButton *button _UNUSED_, gpointer date _UNUSED_)
{
    if (confirm (_("Are you sure you want to abort the current operation ?"),
                (updater->downloadonly || updater->step != STEP_UPGRADING) ? NULL
                : _("CAUTION: This might leave your system in an unstable/non-working state."),
                _("Yes, abort operation"), NULL,
                _("No, keep going"), NULL,
                updater->window))
    {
        gtk_widget_set_sensitive (updater->btn_abort, FALSE);
        kalu_updater_abort (updater->kupdater, NULL, NULL, NULL, NULL);
    }
}

static void
dl_pkgs_done (void)
{
    gchar buf[128], *b = buf;

#define fmt "<big><b><span color=\"%s\">%s</span></b></big>"
    add_log (LOGTYPE_NORMAL, _("Downloading packages complete."));
    add_log (LOGTYPE_NORMAL, "\n");
    if (G_UNLIKELY (g_snprintf (buf, 128, fmt, config->color_info,
                    _("Downloading packages complete.")) >= 128))
        b = g_strdup_printf (fmt, config->color_info, _("Downloading packages complete."));
    gtk_label_set_markup (GTK_LABEL (updater->lbl_main), b);
    if (G_UNLIKELY (b != buf))
    {
        g_free (b);
        b = buf;
    }
#undef fmt
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 1.0);
    gtk_widget_show (updater->pbar_main);
    gtk_widget_hide (updater->lbl_action);
    gtk_widget_hide (updater->pbar_action);
    gtk_widget_set_sensitive (updater->btn_close, TRUE);
    updater->step = STEP_NONE;
    free_kupdater (TRUE);
}

static void
updater_sysupgrade_cb (KaluUpdater *kupdater _UNUSED_, const gchar *errmsg)
{
    alpm_list_t *i;
    gchar buf[128], *b = buf;

    gtk_widget_set_sensitive (updater->btn_abort, FALSE);
    gtk_widget_set_sensitive (updater->btn_close, TRUE);

    if (errmsg != NULL)
    {
        if (updater->downloadonly)
        {
            _show_error (_("Downloading packages failed"), "%s", errmsg);
            free_kupdater (TRUE);
        }
        else
        {
            _show_error (_("System update failed"), "%s", errmsg);
        }
        return;
    }

    if (updater->downloadonly)
    {
        dl_pkgs_done ();
        return;
    }

#define fmt "<big><b><span color=\"%s\">%s</span></b></big>"
    add_log (LOGTYPE_NORMAL, _("System upgrade complete."));
    add_log (LOGTYPE_NORMAL, "\n");
    if (G_UNLIKELY (g_snprintf (buf, 128, fmt, config->color_info,
                    _("System upgrade complete.")) >= 128))
        b = g_strdup_printf (fmt, config->color_info, _("System upgrade complete."));
    gtk_label_set_markup (GTK_LABEL (updater->lbl_main), b);
    if (G_UNLIKELY (b != buf))
        g_free (b);
#undef fmt
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 1.0);
    gtk_widget_hide (updater->lbl_action);
    gtk_widget_hide (updater->pbar_action);

    /* update nb of upgrades in state (used in kalu's systray icon tooltip) */
    set_kalpm_nb (CHECK_UPGRADES, 0, FALSE);
    /* we go and change the last_notifs, to remove the notif about packages */
    FOR_LIST (i, config->last_notifs)
    {
        notif_t *notif = i->data;
        if (notif->type & CHECK_UPGRADES)
        {
            config->last_notifs = alpm_list_remove_item (config->last_notifs, i);
            free_notif (notif);
            break;
        }
    }

    if (NULL != updater->cmdline_post)
    {
        GError      *error = NULL;
        size_t       count = alpm_list_count (updater->cmdline_post);
        alpm_list_t *cmdlines = NULL;

        if (count == 1)
        {
            if (config->confirm_post)
            {
                add_log (LOGTYPE_INFO,
                        _("Do you want to run the post-sysupgrade process ?"));
                if (confirm (_("Do you want to run the post-sysupgrade process ?"),
                            updater->cmdline_post->data,
                            _("Yes, Start it now"), NULL,
                            _("No"), NULL,
                            updater->window))
                {
                    add_log (LOGTYPE_INFO, _(" Yes\n"));
                    cmdlines = alpm_list_add (cmdlines,
                            updater->cmdline_post->data);
                }
                else
                {
                    add_log (LOGTYPE_INFO, _(" No\n"));
                }
            }
            else
            {
                cmdlines = alpm_list_add (cmdlines,
                        updater->cmdline_post->data);
            }
        }
        else
        {
            if (config->confirm_post)
            {
                add_log (LOGTYPE_INFO,
                        _("Do you want to run the post-sysupgrade processes ?"));
                cmdlines = confirm_choices (
                        _("Do you want to run the following post-sysupgrade processes ?"),
                        _("Only checked processes will be launch."),
                        _("Yes, run checked processes now"), NULL,
                        _("No"), NULL,
                        /* TRANSLATORS: as in, run selected process */
                        _c("column", "Run"),
                        _c("column", "Command-line"),
                        updater->cmdline_post,
                        updater->window);
                if (NULL != cmdlines)
                {
                    add_log (LOGTYPE_INFO, _(" Yes\n"));
                }
                else
                {
                    add_log (LOGTYPE_INFO, _(" No\n"));
                }
            }
            else
            {
                cmdlines = alpm_list_copy (updater->cmdline_post);
            }
        }

        if (cmdlines)
        {
            /* construct list of "upgraded" packages */
            GtkTreeIter   iter;
            GtkTreeModel *model = GTK_TREE_MODEL (updater->store);
            GString      *str_packages;
            GString      *str_pacfiles;
            char         *s;

            str_packages = g_string_sized_new (255);
            gtk_tree_model_get_iter_first (model, &iter);
            do
            {
                gtk_tree_model_get (model, &iter, UCOL_PACKAGE, &s, -1);
                str_packages = g_string_append (str_packages, s);
                str_packages = g_string_append_c (str_packages, ' ');
            } while (gtk_tree_model_iter_next (model, &iter));
            if (G_LIKELY (str_packages->len > 0))
                g_string_truncate (str_packages, str_packages->len - 1);

            if (!updater->pacfile || updater->pacfile->len == 0)
                str_pacfiles = NULL;
            else
            {
                guint n;

                str_pacfiles = g_string_new (NULL);
                for (n = 0; n < updater->pacfile->len; ++n)
                {
                    struct pacfile *pf;

                    pf = &g_array_index (updater->pacfile, struct pacfile, n);
                    g_string_append (str_pacfiles, pf->file);
                    g_string_append_c (str_pacfiles, ' ');
                    g_string_append (str_pacfiles, pf->pkg);
                    g_string_append_c (str_pacfiles, '-');
                    g_string_append (str_pacfiles, pf->oldver);
                    g_string_append_c (str_pacfiles, ' ');
                }
                g_string_truncate (str_pacfiles, str_pacfiles->len - 1);
            }

            /* start */
            FOR_LIST (i, cmdlines)
            {
                char *ss;
                s  = strreplace (i->data, "$PACKAGES", str_packages->str);
                ss = strreplace (s, "$PACFILES",
                        (str_pacfiles) ? str_pacfiles->str : "");
                g_free (s);
                s = ss;

                add_log (LOGTYPE_NORMAL, _("Starting: '%s' .."), s);
                if (!g_spawn_command_line_async (s, &error))
                {
                    add_log (LOGTYPE_NORMAL, _(" failed\n"));
                    _show_error (_("Unable to start post-sysupgrade process."),
                            _("Command-line: %s\nError: %s"),
                            s, error->message);
                    g_clear_error (&error);
                }
                else
                {
                    add_log (LOGTYPE_NORMAL, _(" ok\n"));
                }
                free (s);
            }

            alpm_list_free (cmdlines);
            g_string_free (str_packages, TRUE);
            if (str_pacfiles)
                g_string_free (str_pacfiles, TRUE);
        }
    }
}

static void
btn_sysupgrade_cb (GtkButton *button _UNUSED_, gpointer data _UNUSED_)
{
    GError *error = NULL;

    if (updater->step != STEP_WAITING_USER_CONFIRMATION)
    {
        _show_error (_("Unable to start system upgrade"),
                _("Invalid internal state"));
        return;
    }
    gtk_widget_set_sensitive (updater->btn_close, FALSE);
    gtk_widget_set_sensitive (updater->btn_sysupgrade, FALSE);
    updater->step = STEP_USER_CONFIRMED;
    updater->pctg_done = 0.0;
    gtk_label_set_text (GTK_LABEL (updater->lbl_main),
            _("Performing system upgrade..."));
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.0);
    gtk_widget_show (updater->lbl_main);
    gtk_widget_show (updater->pbar_main);
    gtk_widget_hide (updater->lbl_action);
    gtk_widget_hide (updater->pbar_action);

    if (!kalu_updater_sysupgrade (updater->kupdater, NULL,
                (KaluMethodCallback) updater_sysupgrade_cb, NULL, &error))
    {
        _show_error (_("Unable to start system upgrade"), error->message);
        g_clear_error (&error);
        return;
    }
    gtk_widget_set_sensitive (updater->btn_abort, TRUE);
}

static void
updater_get_packages_cb (KaluUpdater *kupdater _UNUSED_, const gchar *errmsg,
                         alpm_list_t *pkgs, gpointer data _UNUSED_)
{
    alpm_list_t     *i;
    kalu_package_t  *k_pkg;
    GtkTreeIter      iter;

    if (errmsg != NULL)
    {
        _show_error (_("Failed to get packages list"), "%s", errmsg);
        if (updater->downloadonly)
            free_kupdater (TRUE);
        return;
    }
    else if (pkgs == NULL)
    {
        _show_error (_("No packages to upgrade"),
                _("Your system is already up-to-date."));
        if (updater->downloadonly)
        {
            free_kupdater (TRUE);
            /* i.e. this is a simulation, so there's no point for those buttons
             * to be sensitive (_show_error() switched them automatically) */
            gtk_widget_set_sensitive (updater->btn_sysupgrade, FALSE);
            gtk_widget_set_sensitive (updater->btn_rerun, FALSE);
        }
        return;
    }

    guint dl_size = 0;
    guint inst_size = 0;
    gint net_size = 0;
    updater->total_dl = 0; /* will be set through on_total_download */
    updater->total_inst = 0;
    FOR_LIST (i, pkgs)
    {
        k_pkg = i->data;
        gtk_list_store_append (updater->store, &iter);
        gtk_list_store_set (updater->store, &iter,
                UCOL_REPO,              k_pkg->repo,
                UCOL_PACKAGE,           k_pkg->name,
                UCOL_DESC,              k_pkg->desc,
                UCOL_OLD,               k_pkg->old_version,
                UCOL_NEW,               k_pkg->new_version,
                UCOL_CUR_DL_SIZE,       0,
                UCOL_CUR_XFRD_SIZE,     0,
                UCOL_TOT_DL_SIZE,       0,
                UCOL_DL_SIZE,           k_pkg->dl_size,
                UCOL_OLD_SIZE,          k_pkg->old_size,
                UCOL_NEW_SIZE,          k_pkg->new_size,
                UCOL_NET_SIZE,          k_pkg->new_size - k_pkg->old_size,
                UCOL_DL_IS_ACTIVE,      FALSE,
                UCOL_INST_IS_ACTIVE,    FALSE,
                UCOL_DL_IS_DONE,        k_pkg->dl_size == 0,
                UCOL_DL_IS_DONE_COLOR,  (k_pkg->dl_size == 0) ? config->color_info : NULL,
                UCOL_INST_IS_DONE,      FALSE,
                UCOL_PCTG,              0.0,
                -1);
        dl_size += k_pkg->dl_size;
        inst_size += k_pkg->new_size;
        net_size += (gint) (k_pkg->new_size - k_pkg->old_size);
        /* total_inst will be used to make up the total install pbar, where
         * each pkg represent its installed size. except we use the old_size
         * for pkgs that have none, i.e. that are being removed */
        updater->total_inst += (k_pkg->new_size > 0) ? k_pkg->new_size : k_pkg->old_size;
    }

    gchar buf[255], *b = buf;
    double size;
    const char *unit;
    char dl_buf[23], inst_buf[23], net_buf[23];

    size = humanize_size (inst_size, '\0', &unit);
    snprint_size (inst_buf, 23, size, unit);
    size = humanize_size (net_size, '\0', &unit);
    snprint_size (net_buf, 23, size, unit);

    gtk_widget_hide (updater->pbar_main);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0);

    if (updater->downloadonly && updater->kupdater)
    {
        GError *err = NULL;

        /* i.e. this is from "downloading packages" not simulation */

        if (dl_size == 0)
        {
            dl_pkgs_done ();
            return;
        }

#define fmt "<big><b>%s</b></big>"
        if (G_UNLIKELY (g_snprintf (buf, 255, fmt,
                        _("Do you want to download packages ?")) >= 255))
            b = g_strdup_printf (fmt, _("Do you want to download packages ?"));
#undef fmt
        gtk_label_set_markup (GTK_LABEL (updater->lbl_main), b);
        if (G_UNLIKELY (b != buf))
        {
            g_free (b);
            b = buf;
        }

        updater->pctg_download          = PCTG_DL_ONLY_DOWNLOAD;
        updater->pctg_pkg_integrity     = PCTG_DL_ONLY_PKG_INTEGRITY;
        updater->pctg_file_conflicts    = PCTG_DL_ONLY_FILE_CONFLICTS;
        updater->pctg_load_pkgfiles     = PCTG_DL_ONLY_LOAD_PKGFILES;
        updater->pctg_check_diskspace   = PCTG_DL_ONLY_CHECK_DISKSPACE;
        updater->pctg_keyring           = PCTG_DL_ONLY_KEYRING;
        updater->pctg_sysupgrade        = PCTG_DL_ONLY_SYSUPGRADE;

        size = humanize_size (dl_size, '\0', &unit);
        snprint_size (dl_buf, 255, size, unit);
#define fmt _("Download:\t%s")
        if (G_UNLIKELY (g_snprintf (buf, 255, fmt, dl_buf) >= 255))
            b = g_strdup_printf (fmt, dl_buf);
#undef fmt
        gtk_label_set_markup (GTK_LABEL (updater->lbl_action), b);
        if (G_UNLIKELY (b != buf))
        {
            g_free (b);
            b = buf;
        }
        gtk_widget_show (updater->lbl_action);
        add_log (LOGTYPE_NORMAL,
                _("Download: %s\n"),
                dl_buf);

        if (!confirm (_("Do you want to download packages ?"), NULL,
                    _("Download packages"), "document-save",
                    _("No, cancel download"), "gtk-cancel",
                    updater->window))
        {
            _show_error (_("Downloading packages failed"),
                    _("Download operation cancelled."));
            free_kupdater (TRUE);
            return;
        }

        updater->step = STEP_USER_CONFIRMED;
        updater->pctg_done = 0.0;
        gtk_label_set_text (GTK_LABEL (updater->lbl_main),
                _("Downloading packages..."));
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.0);
        gtk_widget_show (updater->pbar_main);

        if (!kalu_updater_sysupgrade (updater->kupdater, NULL,
                    (KaluMethodCallback) updater_sysupgrade_cb, NULL, &err))
        {
            _show_error (_("Downloading packages failed"), err->message);
            g_clear_error (&err);
            free_kupdater (TRUE);
            return;
        }

        gtk_widget_set_sensitive (updater->btn_abort, TRUE);
        return;
    }

    gtk_label_set_markup (GTK_LABEL (updater->lbl_main), (!updater->downloadonly)
            ? _("<big><b>Do you want to upgrade your system ?</b></big>")
            : _("<big><b>Simulation complete</b></big>"));
    if (dl_size > 0)
    {
        updater->pctg_download          = PCTG_DOWNLOAD;
        updater->pctg_pkg_integrity     = PCTG_PKG_INTEGRITY;
        updater->pctg_file_conflicts    = PCTG_FILE_CONFLICTS;
        updater->pctg_load_pkgfiles     = PCTG_LOAD_PKGFILES;
        updater->pctg_check_diskspace   = PCTG_CHECK_DISKSPACE;
        updater->pctg_keyring           = PCTG_KEYRING;
        updater->pctg_sysupgrade        = PCTG_SYSUPGRADE;

        size = humanize_size (dl_size, '\0', &unit);
        snprint_size (dl_buf, 255, size, unit);
#define fmt "Download:\t%s\nInstall:\t\t%s\nNet:\t\t\t%s"
        if (G_UNLIKELY (g_snprintf (buf, 255, fmt, dl_buf, inst_buf, net_buf) >= 255))
            b = g_strdup_printf (fmt, dl_buf, inst_buf, net_buf);
#undef fmt
        add_log (LOGTYPE_NORMAL,
                _("Download: %s - Install: %s - Net: %s\n"),
                dl_buf, inst_buf, net_buf);
    }
    else
    {
        updater->pctg_download          = 0;
        updater->pctg_pkg_integrity     = PCTG_NO_DL_PKG_INTEGRITY;
        updater->pctg_file_conflicts    = PCTG_NO_DL_FILE_CONFLICTS;
        updater->pctg_load_pkgfiles     = PCTG_NO_DL_LOAD_PKGFILES;
        updater->pctg_check_diskspace   = PCTG_NO_DL_CHECK_DISKSPACE;
        updater->pctg_keyring           = PCTG_NO_DL_KEYRING;
        updater->pctg_sysupgrade        = PCTG_NO_DL_SYSUPGRADE;

#define fmt "Install:\t\t%s\nNet:\t\t\t%s"
        if (G_UNLIKELY (g_snprintf (buf, 255, fmt, inst_buf, net_buf) >= 255))
            b = g_strdup_printf (fmt, inst_buf, net_buf);
#undef fmt
        add_log (LOGTYPE_NORMAL,
                _("Install: %s - Net: %s\n"),
                inst_buf, net_buf);
    }
    gtk_label_set_markup (GTK_LABEL (updater->lbl_action), b);
    if (G_UNLIKELY (b != buf))
    {
        g_free (b);
        b = buf;
    }
    gtk_widget_show (updater->lbl_action);
    if (!updater->downloadonly || dl_size > 0)
        gtk_widget_set_sensitive (updater->btn_sysupgrade, TRUE);
    gtk_widget_set_sensitive (updater->btn_close, TRUE);
    if (updater->downloadonly)
        gtk_widget_set_sensitive (updater->btn_rerun, TRUE);
    updater->step = STEP_WAITING_USER_CONFIRMATION;
    add_log (LOGTYPE_UNIMPORTANT, (!updater->downloadonly)
            ? _("Got package list; waiting for user confirmation.\n")
            : _("Simulation complete.\n"));
}

static void
updater_sync_dbs_cb (KaluUpdater *kupdater, const gchar *errmsg, gpointer data _UNUSED_)
{
    GError *error = NULL;

    if (errmsg != NULL)
    {
        _show_error (_("Failed to synchronize databases"), "%s", errmsg);
        free (updater->step_data);
        updater->step_data = NULL;
        updater->step = STEP_NONE;
        return;
    }

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 1);
    add_log (LOGTYPE_NORMAL, _("Databases synchronized\n"));
    reset_kalpm_synced_dbs ();

    add_log (LOGTYPE_UNIMPORTANT, _("Getting packages list\n"));
    gtk_label_set_text (GTK_LABEL (updater->lbl_main),
            _("Getting packages list..."));
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.0);
    if (!kalu_updater_get_packages (kupdater,
                NULL,
                (KaluMethodCallback) updater_get_packages_cb,
                NULL,
                &error))
    {
        _show_error (_("Failed to get packages list"), "%s", error->message);
        g_clear_error (&error);
        return;
    }
}

static void
updater_add_db_cb (KaluUpdater *kupdater, const gchar *errmsg, add_db_t *add_db)
{
    GError *error = NULL;

    /* did we just add a db? */
    if (add_db->i != NULL)
    {
        if (errmsg != NULL)
        {
            add_log (LOGTYPE_ERROR, _(" failed\n"));
            _show_error (_("Failed to register databases"), "%s", errmsg);
            if (updater->downloadonly)
                free_kupdater (TRUE);
            else
                free_pacman_config (add_db->pac_conf);
            free (add_db);
            return;
        }
        add_log (LOGTYPE_NORMAL, _(" ok\n"));
        add_db->pctg += add_db->inc;
        gtk_progress_bar_set_fraction (
                GTK_PROGRESS_BAR (updater->pbar_main), add_db->pctg);
        /* next */
        add_db->i = alpm_list_next (add_db->i);
    }
    /* or is this the start? */
    else
    {
        add_db->i = add_db->pac_conf->databases;
    }

    if (add_db->i)
    {
        database_t *db_conf = add_db->i->data;
        add_log (LOGTYPE_NORMAL, _("Register database %s... "), db_conf->name);
        if (!kalu_updater_add_db (kupdater,
                    db_conf->name,
                    db_conf->siglevel,
                    db_conf->servers,
                    NULL,
                    (KaluMethodCallback) updater_add_db_cb,
                    (gpointer) add_db,
                    &error))
        {
            add_log (LOGTYPE_ERROR, _(" failed\n"));
            _show_error (_("Failed to register databases"), "%s",
                    error->message);
            g_clear_error (&error);
            if (updater->downloadonly)
                free_kupdater (TRUE);
            else
                free_pacman_config (add_db->pac_conf);
            free (add_db);
            return;
        }
    }
    else
    {
        if (!updater->downloadonly)
            free_pacman_config (add_db->pac_conf);
        free (add_db);
        gtk_progress_bar_set_fraction (
                GTK_PROGRESS_BAR (updater->pbar_main), 1);
        if (!updater->downloadonly)
        {
            /* we're good, so let's sync dbs */
            updater->step_data = new0 (sync_dbs_t, 1);
            updater->step = STEP_SYNC_DBS;
            if (!kalu_updater_sync_dbs (kupdater,
                        NULL,
                        (KaluMethodCallback) updater_sync_dbs_cb,
                        NULL,
                        &error))
            {
                _show_error (_("Failed to synchronize databases"), "%s",
                        error->message);
                g_clear_error (&error);
                free (updater->step_data);
                updater->step_data = NULL;
                updater->step = STEP_NONE;
            }
        }
        else
        {
            /* downloadonly: we assume dbs to be synced already (from
             * simulation), so we just ahead */
            add_log (LOGTYPE_UNIMPORTANT, _("Getting packages list\n"));
            gtk_label_set_text (GTK_LABEL (updater->lbl_main),
                    _("Getting packages list..."));
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.0);
            if (!kalu_updater_get_packages (kupdater,
                        NULL,
                        (KaluMethodCallback) updater_get_packages_cb,
                        NULL,
                        &error))
            {
                _show_error (_("Failed to get packages list"), "%s", error->message);
                g_clear_error (&error);
                free_kupdater (TRUE);
                return;
            }
            gtk_list_store_clear (updater->store);
        }
    }
}

static void
updater_init_alpm_cb (KaluUpdater *kupdater, const gchar *errmsg,
        pacman_config_t *pac_conf)
{
    if (errmsg != NULL)
    {
        add_log (LOGTYPE_UNIMPORTANT, _(" failed\n"));
        _show_error (_("Failed to initialize ALPM library"), "%s", errmsg);
        if (updater->downloadonly)
            free_kupdater (TRUE);
        else
            free_pacman_config (pac_conf);
        return;
    }
    add_log (LOGTYPE_UNIMPORTANT, _(" ok\n"));
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.42);

    add_log (LOGTYPE_UNIMPORTANT, _("Registering databases\n"));
    add_db_t *add_db;
    add_db = new0 (add_db_t, 1);
    add_db->pac_conf = pac_conf;
    add_db->pctg = 0.42;
    add_db->inc = 0.58 / (int) alpm_list_count (pac_conf->databases);
    add_db->i = NULL;

    updater_add_db_cb (kupdater, NULL, add_db);
}

static void
updater_method_cb (KaluUpdater *kupdater, const gchar *errmsg,
        pacman_config_t *pac_conf)
{
    GError *error = NULL;

    if (errmsg != NULL)
    {
        add_log (LOGTYPE_UNIMPORTANT, _(" failed\n"));
        _show_error (_("Failed to initialize"), "%s", errmsg);
        if (updater->downloadonly)
            free_kupdater (TRUE);
        else
            free_pacman_config (pac_conf);
        return;
    }
    add_log (LOGTYPE_UNIMPORTANT, _(" ok\n"));
    gtk_progress_bar_set_fraction (
            GTK_PROGRESS_BAR (updater->pbar_main), 0.23);

    add_log (LOGTYPE_UNIMPORTANT, _("Initializing ALPM library..."));
    gtk_label_set_text (GTK_LABEL (updater->lbl_main),
            _("Initializing ALPM library..."));
    if (!kalu_updater_init_alpm (kupdater,
                pac_conf->rootdir,
                (!updater->downloadonly)
                ? pac_conf->dbpath : (gchar *) kalu_alpm_get_dbpath (),
                (!updater->downloadonly) ? pac_conf->logfile : (gchar *) "/dev/null",
                pac_conf->gpgdir,
                pac_conf->hookdirs,
                pac_conf->cachedirs,
                pac_conf->siglevel,
                pac_conf->arch,
                pac_conf->checkspace,
                pac_conf->usesyslog,
                pac_conf->ignorepkgs,
                pac_conf->ignoregroups,
                pac_conf->noupgrades,
                pac_conf->noextracts,
                NULL,
                (KaluMethodCallback) updater_init_alpm_cb,
                (gpointer) pac_conf,
                &error))
    {
        add_log (LOGTYPE_UNIMPORTANT, _(" failed\n"));
        _show_error (_("Failed to initialize ALPM library"), "%s",
                error->message);
        g_clear_error (&error);
        if (updater->downloadonly)
            free_kupdater (TRUE);
        else
            free_pacman_config (pac_conf);
    }
}

static void
updater_new_cb (GObject *source _UNUSED_, GAsyncResult *res,
        pacman_config_t *pac_conf)
{
    GError *error = NULL;
    KaluUpdater *kalu_updater;

    kalu_updater = kalu_updater_new_finish (res, &error);
    if (kalu_updater == NULL)
    {
        add_log (LOGTYPE_UNIMPORTANT, _(" failed\n"));
        _show_error (_("Could not initiate kalu_updater"), "%s",
                error->message);
        g_clear_error (&error);
        if (!updater->downloadonly)
            free_pacman_config (pac_conf);
        return;
    }
    add_log (LOGTYPE_UNIMPORTANT, _(" ok \n"));
    updater->kupdater = kalu_updater;
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.15);

    g_signal_connect (kalu_updater,
            "debug",
            G_CALLBACK (on_debug),
            NULL);
    g_signal_connect (kalu_updater,
            "downloading",
            G_CALLBACK (on_download),
            NULL);
    g_signal_connect (kalu_updater,
            "sync-dbs",
            G_CALLBACK (on_sync_dbs),
            NULL);
    g_signal_connect (kalu_updater,
            "sync-db-start",
            G_CALLBACK (on_sync_db_start),
            NULL);
    g_signal_connect (kalu_updater,
            "sync-db-end",
            G_CALLBACK (on_sync_db_end),
            NULL);
    g_signal_connect (kalu_updater,
            "log",
            G_CALLBACK (on_log),
            NULL);
    g_signal_connect (kalu_updater,
            "total-download",
            G_CALLBACK (on_total_download),
            NULL);
    g_signal_connect (kalu_updater,
            "event",
            G_CALLBACK (on_event),
            NULL);
    g_signal_connect (kalu_updater,
            "event-hookrun",
            G_CALLBACK (on_event_hookrun),
            NULL);
    g_signal_connect (kalu_updater,
            "event-installed",
            G_CALLBACK (on_event_installed),
            NULL);
    g_signal_connect (kalu_updater,
            "event-reinstalled",
            G_CALLBACK (on_event_reinstalled),
            NULL);
    g_signal_connect (kalu_updater,
            "event-removed",
            G_CALLBACK (on_event_removed),
            NULL);
    g_signal_connect (kalu_updater,
            "event-upgraded",
            G_CALLBACK (on_event_upgraded),
            NULL);
    g_signal_connect (kalu_updater,
            "event-downgraded",
            G_CALLBACK (on_event_downgraded),
            NULL);
    g_signal_connect (kalu_updater,
            "event-pkgdownload-start",
            G_CALLBACK (on_event_pkgdownload_start),
            NULL);
    g_signal_connect (kalu_updater,
            "event-pkgdownload-done",
            G_CALLBACK (on_event_pkgdownload_done),
            NULL);
    g_signal_connect (kalu_updater,
            "event-pkgdownload-failed",
            G_CALLBACK (on_event_pkgdownload_failed),
            NULL);
    g_signal_connect (kalu_updater,
            "event-scriptlet",
            G_CALLBACK (on_event_scriptlet),
            NULL);
    g_signal_connect (kalu_updater,
            "event-pacnew-created",
            G_CALLBACK (on_event_pacnew_created),
            NULL);
    g_signal_connect (kalu_updater,
            "event-pacsave-created",
            G_CALLBACK (on_event_pacsave_created),
            NULL);
    g_signal_connect (kalu_updater,
            "event-optdep-removal",
            G_CALLBACK (on_event_optdep_removal),
            NULL);
    g_signal_connect (kalu_updater,
            "progress",
            G_CALLBACK (on_progress),
            NULL);
    g_signal_connect (kalu_updater,
            "install-ignorepkg",
            G_CALLBACK (on_install_ignorepkg),
            NULL);
    g_signal_connect (kalu_updater,
            "replace-pkg",
            G_CALLBACK (on_replace_pkg),
            NULL);
    g_signal_connect (kalu_updater,
            "conflict-pkg",
            G_CALLBACK (on_conflict_pkg),
            NULL);
    g_signal_connect (kalu_updater,
            "remove_pkgs",
            G_CALLBACK (on_remove_pkgs),
            NULL);
    g_signal_connect (kalu_updater,
            "select-provider",
            G_CALLBACK (on_select_provider),
            NULL);
    g_signal_connect (kalu_updater,
            "corrupted-pkg",
            G_CALLBACK (on_corrupted_pkg),
            NULL);
    g_signal_connect (kalu_updater,
            "import-key",
            G_CALLBACK (on_import_key),
            NULL);

    add_log (LOGTYPE_UNIMPORTANT, _("Initializing kalu_updater..."));
    if (!kalu_updater_init_upd (kalu_updater, updater->downloadonly, NULL,
                (KaluMethodCallback) updater_method_cb,
                (gpointer) pac_conf,
                &error))
    {
        add_log (LOGTYPE_UNIMPORTANT, _(" failed\n"));
        _show_error (_("Could not initialize kalu_updater"), "%s", error->message);
        g_clear_error (&error);
        if (updater->downloadonly)
            free_kupdater (TRUE);
        else
            free_pacman_config (pac_conf);
    }
}

static void
format_size (guint size, gchar *buf, gboolean is_signed)
{
    gchar *s;
    size_t l, i;

    if (is_signed)
    {
        gint ssize = (gint) size;
        snprintf (buf, 128, "%i", ssize);
    }
    else
    {
        snprintf (buf, 128, "%u", size);
    }
    /* add thousand sep */
    l = strlen (buf);
    /* point to the future end of the string */
    s = buf + l + (unsigned int) (l / 3);
    /* when a multiple of 3, that's one less space (6 char. == 1 space) */
    if (l % 3 == 0)
    {
        --s;
    }
    *s-- = '\0';
    for (i = l - 1; i > 0; --i)
    {
        *s-- = buf[i];
        if ((l - i) % 3 == 0)
        {
            *s-- = ' ';
        }
    }
    strcat (buf, " B");
}

static gboolean
list_query_tooltip_cb (GtkWidget    *widget,
                       gint          x,
                       gint          y,
                       gboolean      keyboard _UNUSED_,
                       GtkTooltip   *tooltip,
                       gpointer      data _UNUSED_)
{
    GtkTreeView         *treeview   = GTK_TREE_VIEW (widget);
    GtkTreeModel        *model      = GTK_TREE_MODEL (updater->store);
    GtkTreePath         *path;
    GtkTreeViewColumn   *column;
    GtkTreeIter          iter;
    gboolean             ret        = FALSE;
    gint                 bx;
    gint                 by;

    gtk_tree_view_convert_widget_to_bin_window_coords (treeview, x, y,
            &bx, &by);
    if (gtk_tree_view_get_path_at_pos (treeview, bx, by, &path, &column,
                NULL, NULL))
    {
        if (gtk_tree_model_get_iter (model, &iter, path))
        {
            gint col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (column),
                        "col-id"));
            gchar *s, buf[128];
            guint size;

            switch (col)
            {
                case COL_NAME:
                    gtk_tree_model_get (model, &iter, UCOL_DESC, &s, -1);
                    gtk_tooltip_set_text (tooltip, s);
                    ret = TRUE;
                    break;

                case COL_DOWNLOAD:
                    gtk_tree_model_get (model, &iter, UCOL_DL_SIZE, &size, -1);
                    format_size (size, buf, FALSE);
                    gtk_tooltip_set_text (tooltip, buf);
                    ret = TRUE;
                    break;

                case COL_INSTALLED:
                    gtk_tree_model_get (model, &iter, UCOL_NEW_SIZE, &size, -1);
                    format_size (size, buf, FALSE);
                    gtk_tooltip_set_text (tooltip, buf);
                    ret = TRUE;
                    break;

                case COL_NET:
                    gtk_tree_model_get (model, &iter, UCOL_NET_SIZE, &size, -1);
                    format_size (size, buf, TRUE);
                    gtk_tooltip_set_text (tooltip, buf);
                    ret = TRUE;
                    break;
            }
        }
        gtk_tree_path_free (path);
    }

    return ret;
}

static gboolean
window_delete_event_cb (GtkWidget *window _UNUSED_, GdkEvent *event _UNUSED_,
                        gpointer data _UNUSED_)
{
    /* cannot close window */
    return TRUE;
}

static void
free_pacfile (struct pacfile *pf)
{
    if (!pf)
        return;
    g_free (pf->file);
    g_free (pf->pkg);
    g_free (pf->oldver);
}

static void
window_destroy_cb (GtkWidget *window _UNUSED_, gpointer data _UNUSED_)
{
    if (NULL != updater->step_data)
    {
        free (updater->step_data);
        updater->step_data = NULL;
    }

    if (updater->kupdater)
    {
        g_object_unref (updater->kupdater);
        updater->kupdater = NULL;
    }

    if (updater->pacfile)
    {
        g_array_set_clear_func (updater->pacfile, (GDestroyNotify) free_pacfile);
        g_array_free (updater->pacfile, TRUE);
    }

    free (updater);
    updater = NULL;

    set_kalpm_busy (FALSE);
}

static void
paned_position_cb (GObject *o, GParamSpec *pspec _UNUSED_, gpointer data _UNUSED_)
{
    if (gtk_expander_get_expanded (GTK_EXPANDER (updater->expander)))
        updater->pos_expanded  = gtk_paned_get_position (GTK_PANED (o));
    else
        updater->pos_collapsed = gtk_paned_get_position (GTK_PANED (o));
}

static void
expander_expanded_cb (GObject *o, GParamSpec *pspec _UNUSED_, gpointer sw)
{
    if (gtk_expander_get_expanded (GTK_EXPANDER (o)))
    {
        gtk_widget_show (GTK_WIDGET (sw));
        gtk_paned_set_position (GTK_PANED (updater->paned), updater->pos_expanded);
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (sw));
        gtk_paned_set_position (GTK_PANED (updater->paned), updater->pos_collapsed);
    }
}

static void
btn_download_cb (GtkButton *button _UNUSED_, gpointer data)
{
    gtk_widget_set_sensitive (updater->btn_sysupgrade, FALSE);
    gtk_widget_set_sensitive (updater->btn_close, FALSE);
    gtk_widget_set_sensitive (updater->btn_rerun, FALSE);
    gtk_label_set_text (GTK_LABEL (updater->lbl_main),
            _("Initializing... Please wait..."));
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.0);
    gtk_widget_show (updater->pbar_main);
    gtk_widget_hide (updater->lbl_action);
    /* create kalu_updater */
    add_log (LOGTYPE_UNIMPORTANT, _("Creating kalu_updater..."));
    kalu_updater_new (NULL,
            (GAsyncReadyCallback) updater_new_cb,
            data);
}

static void
btn_rerun_cb (GtkButton *button _UNUSED_, gpointer data _UNUSED_)
{
    GError *err = NULL;
    alpm_list_t *packages = NULL;

    add_log (LOGTYPE_NORMAL, _("\nRerun simulation...\n"));
    gtk_list_store_clear (updater->store);
    kalu_alpm_has_updates (&packages, &err);
    updater_get_packages_cb (NULL, (err) ? err->message : NULL, packages, NULL);
    g_clear_error (&err);
    FREE_PACKAGE_LIST (packages);
}

static void
dl_progress_cb (const gchar *filename, off_t xfered, off_t total)
{
    on_download (NULL, filename, (guint) xfered, (guint) total);
    /* because in simulation the sync-ing dbs is actually done in the main
     * thread */
    while (gtk_events_pending ())
        gtk_main_iteration ();
}

static void
question_cb (alpm_question_t *question)
{
    alpm_list_t *i, *l;

    switch (question->type)
    {
        case ALPM_QUESTION_INSTALL_IGNOREPKG:
            {
                alpm_question_install_ignorepkg_t *q = (alpm_question_install_ignorepkg_t *) question;
                q->install = on_install_ignorepkg (NULL, alpm_pkg_get_name (q->pkg));
                break;
            }

        case ALPM_QUESTION_REPLACE_PKG:
            {
                alpm_question_replace_t *q = (alpm_question_replace_t *) question;
                q->replace = on_replace_pkg (NULL,
                        alpm_db_get_name (alpm_pkg_get_db (q->oldpkg)),
                        alpm_pkg_get_name (q->oldpkg),
                        alpm_db_get_name (q->newdb),
                        alpm_pkg_get_name (q->newpkg));
                break;
            }

        case ALPM_QUESTION_CONFLICT_PKG:
            {
                alpm_question_conflict_t *q = (alpm_question_conflict_t *) question;
                const char *reason;
                if (strcmp (q->conflict->reason->name, q->conflict->package1) == 0
                        || strcmp (q->conflict->reason->name, q->conflict->package2) == 0)
                {
                    reason = "";
                }
                else
                {
                    reason = q->conflict->reason->name;
                }
                q->remove = on_conflict_pkg (NULL,
                        q->conflict->package1,
                        q->conflict->package2,
                        reason);
                break;
            }

        case ALPM_QUESTION_REMOVE_PKGS:
            {
                alpm_question_remove_pkgs_t *q = (alpm_question_remove_pkgs_t *) question;
                l = NULL;
                FOR_LIST (i, q->packages)
                {
                    l = alpm_list_add (l, (gpointer) alpm_pkg_get_name (i->data));
                }

                q->skip = on_remove_pkgs (NULL, l);
                alpm_list_free (l);
                break;
            }

        case ALPM_QUESTION_SELECT_PROVIDER:
            {
                alpm_question_select_provider_t *q = (alpm_question_select_provider_t *) question;
                /* creates a string like "foobar>=4.2" */
                char *pkg = alpm_dep_compute_string (q->depend);

                l = NULL;
                FOR_LIST (i, q->providers)
                {
                    provider_t *provider;

                    provider = g_slice_new (provider_t);
                    provider->repo = (gchar *) alpm_db_get_name (alpm_pkg_get_db (i->data));
                    provider->pkg = (gchar *) alpm_pkg_get_name (i->data);
                    provider->version = (gchar *) alpm_pkg_get_version (i->data);
                    l = alpm_list_add (l, provider);
                }

                q->use_index = on_select_provider (NULL, pkg, l);
                free (pkg);
                FOR_LIST (i, l)
                {
                    g_slice_free (provider_t, l->data);
                }
                alpm_list_free (l);
                break;
            }

        case ALPM_QUESTION_CORRUPTED_PKG:
            {
                alpm_question_corrupted_t *q = (alpm_question_corrupted_t *) question;
                q->remove = on_corrupted_pkg (NULL, q->filepath, alpm_strerror (q->reason));
                break;
            }

        case ALPM_QUESTION_IMPORT_KEY:
            {
                alpm_question_import_key_t *q = (alpm_question_import_key_t *) question;
                gchar created[12];
                strftime (created, 12, "%Y-%m-%d", localtime (&q->key->created));

                q->import = on_import_key (NULL, q->key->fingerprint, q->key->uid, created);
                break;
            }

        default:
            debug ("Received unknown question: %d", question->type);
            return;
    }
}

static void
log_cb (alpm_loglevel_t level, const char *fmt, va_list args)
{
    gchar *s;

    if (level & (ALPM_LOG_DEBUG | ALPM_LOG_FUNCTION))
        return;

    if (!fmt || *fmt == '\0')
        return;

    s = g_strdup_vprintf (fmt, args);
    on_log (NULL, level, s);
    g_free (s);
}

void
updater_run (const gchar *conffile, alpm_list_t *cmdline_post)
{
    /* conffile == NULL means run simulation (and possibly download only) */
    gboolean run_simulation = conffile == NULL;

    updater = new0 (updater_t, 1);
    updater->cmdline_post = cmdline_post;
    updater->pos_expanded = -1;
    updater->pos_collapsed = -1;
    updater->downloadonly = run_simulation;

    /* the window */
    GtkWidget *window;
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    updater->window = window;
    gtk_window_set_title (GTK_WINDOW (window), (!run_simulation)
            ? _("System upgrade - kalu") : _("Upgrade simulation - kalu"));
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    /* icon */
    gtk_window_set_icon_name (GTK_WINDOW (window), "kalu");

    /* ensure minimum size */
    gint w, h;
    gtk_window_get_size (GTK_WINDOW (window), &w, &h);
    w = MAX(650, w);
    h = MAX(420, h);
    gtk_window_set_default_size (GTK_WINDOW (window), w, h);

    /* everything in a vbox */
    GtkWidget *vbox;
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top (vbox, 4);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);

    /* label */
    GtkWidget *label;
    label = gtk_label_new (_("Initializing... Please wait..."));
    updater->lbl_main = label;
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    /* progress bar */
    GtkWidget *pbar;
    pbar = gtk_progress_bar_new ();
    updater->pbar_main = pbar;
    gtk_box_pack_start (GTK_BOX (vbox), pbar, FALSE, FALSE, 2);
    gtk_widget_show (pbar);

    /* action progress in a vbox */
    GtkWidget *vbox2;
    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start (vbox2, 10);
    gtk_widget_set_margin_end (vbox2, 10);
    gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 5);
    gtk_widget_show (vbox2);

    /* label */
    label = gtk_label_new (NULL);
    updater->lbl_action = label;
    gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    /* progress bar */
    pbar = gtk_progress_bar_new ();
    updater->pbar_action = pbar;
    gtk_box_pack_start (GTK_BOX (vbox2), pbar, FALSE, FALSE, 0);
    gtk_widget_show (pbar);

    /* get vbox2 to minumum size, then hide its content (for now) */
    GtkRequisition minimum;
    gtk_widget_get_preferred_size (vbox2, &minimum, NULL);
    gtk_widget_set_size_request (vbox2, minimum.width, minimum.height);
    gtk_widget_hide (label);
    gtk_widget_hide (pbar);

    /* paned for list & log */
    GtkWidget *paned;
    paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
    updater->paned = paned;
    gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);
    gtk_widget_show (paned);

    /* store for the list */
    GtkListStore *store;
    store = gtk_list_store_new (NB_UCOL,
            G_TYPE_STRING,  /* UCOL_REPO */
            G_TYPE_STRING,  /* UCOL_PACKAGE */
            G_TYPE_STRING,  /* UCOL_DESC */
            G_TYPE_STRING,  /* UCOL_OLD */
            G_TYPE_STRING,  /* UCOL_NEW */
            G_TYPE_UINT,    /* UCOL_CUR_DL_SIZE */
            G_TYPE_UINT,    /* UCOL_CUR_XFRD_SIZE */
            G_TYPE_UINT,    /* UCOL_TOT_DL_SIZE */
            G_TYPE_UINT,    /* UCOL_DL_SIZE */
            G_TYPE_UINT,    /* UCOL_OLD_SIZE */
            G_TYPE_UINT,    /* UCOL_NEW_SIZE */
            G_TYPE_INT,     /* UCOL_NET_SIZE */
            G_TYPE_BOOLEAN, /* UCOL_DL_IS_ACTIVE */
            G_TYPE_BOOLEAN, /* UCOL_INST_IS_ACTIVE */
            G_TYPE_BOOLEAN, /* UCOL_DL_IS_DONE */
            G_TYPE_STRING,  /* UCOL_DL_IS_DONE_COLOR */
            G_TYPE_BOOLEAN, /* UCOL_INST_IS_DONE */
            G_TYPE_DOUBLE   /* UCOL_PCTG */
            );
    updater->store = store;

    /* said list */
    GtkWidget *list;
    list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    updater->list = list;
    g_object_unref (store);
    /* hint for alternate row colors */
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list), TRUE);
    G_GNUC_END_IGNORE_DEPRECATIONS
    /* tooltip */
    gtk_widget_set_has_tooltip (list, TRUE);
    g_signal_connect (G_OBJECT(list), "query-tooltip",
            G_CALLBACK (list_query_tooltip_cb),
            NULL);

    /* scrolledwindow for list */
    GtkWidget *scrolled_window;
    scrolled_window = gtk_scrolled_window_new (
            gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (list)),
            gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (list)));
    gtk_paned_pack1 (GTK_PANED (paned), scrolled_window, TRUE, FALSE);
    gtk_widget_show (scrolled_window);

    /* cell renderer & column(s) */
    GtkCellRenderer *renderer;
    GtkCellRenderer *renderer_lbl;
    GtkCellRenderer *renderer_pbar;
    GtkTreeViewColumn *column;
    /* column: Repo */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Repo"),
            renderer,
            "text", UCOL_REPO,
            NULL);
    g_object_set_data (G_OBJECT (column), "col-id", (gpointer) COL_REPO);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, UCOL_REPO);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Package */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Package"),
            renderer,
            "text", UCOL_PACKAGE,
            NULL);
    g_object_set_data (G_OBJECT (column), "col-id", (gpointer) COL_NAME);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, UCOL_PACKAGE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Old version */
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Old/Current"),
            renderer,
            "text", UCOL_OLD,
            NULL);
    g_object_set_data (G_OBJECT (column), "col-id", (gpointer) COL_OLD);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, UCOL_OLD);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: New version */
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "New"),
            renderer,
            "text", UCOL_NEW,
            NULL);
    g_object_set_data (G_OBJECT (column), "col-id", (gpointer) COL_NEW);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, UCOL_NEW);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Download size */
    renderer_lbl = gtk_cell_renderer_text_new ();
    renderer_pbar = gtk_cell_renderer_progress_new ();
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Download"),
            renderer_lbl,
            "foreground",
            UCOL_DL_IS_DONE_COLOR,
            "foreground-set",
            UCOL_DL_IS_DONE,
            NULL);
    g_object_set_data (G_OBJECT (column), "col-id", (gpointer) COL_DOWNLOAD);
    gtk_tree_view_column_set_cell_data_func (column, renderer_lbl,
            (GtkTreeCellDataFunc) rend_size_pb, (gpointer) TRUE, NULL);
    gtk_cell_renderer_set_visible (renderer_pbar, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pbar,
            (GtkTreeCellDataFunc) rend_pbar_pb, (gpointer) TRUE, NULL);
    gtk_tree_view_column_pack_start (column, renderer_pbar, TRUE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, UCOL_DL_SIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Installed size */
    renderer_lbl = gtk_cell_renderer_text_new ();
    g_object_set (renderer_lbl, "foreground", config->color_info, NULL);
    renderer_pbar = gtk_cell_renderer_progress_new ();
    column = gtk_tree_view_column_new_with_attributes (
            _c("column", "Installed"),
            renderer_lbl,
            "foreground-set",
            UCOL_INST_IS_DONE,
            NULL);
    g_object_set_data (G_OBJECT (column), "col-id", (gpointer) COL_INSTALLED);
    gtk_tree_view_column_set_cell_data_func (column, renderer_lbl,
            (GtkTreeCellDataFunc) rend_size_pb, (gpointer) FALSE, NULL);
    gtk_cell_renderer_set_visible (renderer_pbar, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pbar,
            (GtkTreeCellDataFunc) rend_pbar_pb, (gpointer) FALSE, NULL);
    gtk_tree_view_column_pack_start (column, renderer_pbar, TRUE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, UCOL_NEW_SIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Net size */
    column = gtk_tree_view_column_new_with_attributes ("Net",
            renderer,
            NULL);
    g_object_set_data (G_OBJECT (column), "col-id", (gpointer) COL_NET);
    gtk_tree_view_column_set_cell_data_func (column, renderer,
            (GtkTreeCellDataFunc) rend_net_size, NULL, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, UCOL_NET_SIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);

    /* set the tree ordered by package ASC */
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
            UCOL_PACKAGE,
            GTK_SORT_ASCENDING);

    gtk_container_add (GTK_CONTAINER (scrolled_window), list);
    gtk_widget_show (list);

    /* box for expander & log (paned2) */
    GtkWidget *box;
    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_pack2 (GTK_PANED (paned), box, FALSE, FALSE);
    gtk_widget_show (box);

    /* expander */
    GtkWidget *expander;
    expander = gtk_expander_new (_("Log"));
    updater->expander = expander;
    gtk_box_pack_start (GTK_BOX (box), expander, FALSE, FALSE, 0);
    gtk_widget_show (expander);

    /* text view */
    GtkWidget *text_view;
    text_view = gtk_text_view_new ();
    updater->text_view = text_view;
    gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
    g_signal_connect (G_OBJECT (text_view), "draw",
            G_CALLBACK (text_view_draw_cb), NULL);

    /* buffer */
    updater->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
    gtk_text_buffer_create_tag (updater->buffer, "normal", NULL);
    gtk_text_buffer_create_tag (updater->buffer, "unimportant",
            "foreground",   config->color_unimportant,
            NULL);
    gtk_text_buffer_create_tag (updater->buffer, "info",
            "foreground",   config->color_info,
            NULL);
    gtk_text_buffer_create_tag (updater->buffer, "warning",
            "foreground",   config->color_warning,
            NULL);
    gtk_text_buffer_create_tag (updater->buffer, "error",
            "foreground",   config->color_error,
            NULL);
    /* create mark at the end, where we'll insert stuff/scroll to */
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter (updater->buffer, &iter);
    gtk_text_buffer_create_mark (updater->buffer, "end-mark", &iter, FALSE);

    /* scrolledwindow for text view */
    scrolled_window = gtk_scrolled_window_new (
            gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (text_view)),
            gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (text_view)));
    gtk_box_pack_start (GTK_BOX (box), scrolled_window, TRUE, TRUE, 0);

    gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
    gtk_widget_show (text_view);

    /* button box */
    GtkWidget *hbox;
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    GtkWidget *button, *image;

    /* Abort */
    button = gtk_button_new_with_mnemonic (_("_Abort"));
    image = gtk_image_new_from_icon_name ("gtk-cancel", GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    updater->btn_abort = button;
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_abort_cb), NULL);
    gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
    gtk_widget_show (button);

    if (!run_simulation)
    {
        /* Upgrade system */
        button = gtk_button_new_with_mnemonic (_("_Upgrade system..."));
        updater->btn_sysupgrade = button;
        image = gtk_image_new_from_icon_name ("kalu", GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_widget_set_sensitive (button, FALSE);
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
        g_signal_connect (G_OBJECT (button), "clicked",
                G_CALLBACK (btn_sysupgrade_cb), NULL);
        gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
        gtk_widget_show (button);
    }
    else
    {
        /* Download packages only */
        button = gtk_button_new_with_mnemonic (_("_Download packages..."));
        updater->btn_sysupgrade = button;
        image = gtk_image_new_from_icon_name ("document-save", GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_widget_set_sensitive (button, FALSE);
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
        gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
        gtk_widget_show (button);
    }

    /* Close */
    button = gtk_button_new_with_mnemonic (_("_Close"));
    image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    updater->btn_close = button;
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_close_cb), NULL);
    gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
    gtk_widget_show (button);

    if (run_simulation)
    {
        /* Re-run simulation (to ask questions again) */
        button = gtk_button_new_with_mnemonic (_("_Rerun simulation..."));
        updater->btn_rerun = button;
        image = gtk_image_new_from_icon_name ("view-refresh", GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_widget_set_sensitive (button, FALSE);
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
        g_signal_connect (G_OBJECT (button), "clicked",
                G_CALLBACK (btn_rerun_cb), NULL);
        gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
        gtk_widget_show (button);
    }

    /* signals */
    g_signal_connect (G_OBJECT (window), "delete-event",
            G_CALLBACK (window_delete_event_cb), NULL);
    g_signal_connect (G_OBJECT (window), "destroy",
            G_CALLBACK (window_destroy_cb), NULL);
    g_signal_connect (G_OBJECT (paned), "notify::position",
            G_CALLBACK (paned_position_cb), NULL);
    g_signal_connect (G_OBJECT (expander), "notify::expanded",
            G_CALLBACK (expander_expanded_cb), scrolled_window);

    if (config->auto_show_log)
        gtk_expander_set_expanded (GTK_EXPANDER (updater->expander), TRUE);

    gtk_widget_show (window);

    if (!run_simulation)
    {
        /* parse pacman.conf */
        GError *error = NULL;
        gchar *section = NULL;

        pacman_config_t *pac_conf = NULL;
        add_log (LOGTYPE_UNIMPORTANT, _("Parsing %s ..."), conffile);
        if (!parse_pacman_conf (conffile, &section, 0, 0, &pac_conf, &error))
        {
            add_log (LOGTYPE_UNIMPORTANT, _(" failed\n"));
            _show_error (_("Unable to parse pacman.conf"), "%s: %s",
                    conffile, error->message);
            g_clear_error (&error);
            free_pacman_config (pac_conf);
            return;
        }
        add_log (LOGTYPE_UNIMPORTANT, _(" ok\n"));
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.05);

        /* we must have databases */
        if (alpm_list_count (pac_conf->databases) == 0)
        {
            _show_error (_("No databases defined"), NULL);
            free_pacman_config (pac_conf);
            return;
        }

        /* create kalu_updater */
        add_log (LOGTYPE_UNIMPORTANT, _("Creating kalu_updater..."));
        kalu_updater_new (NULL,
                (GAsyncReadyCallback) updater_new_cb,
                (gpointer) pac_conf);
    }
    else
    {
        GError *err = NULL;
        kalu_simul_t simulation = {
            .dl_progress_cb = dl_progress_cb,
            .question_cb = question_cb,
            .log_cb = log_cb,
            .on_sync_dbs = (void (*) (gpointer unused, gint nb)) on_sync_dbs,
            .on_sync_db_start =
                (void (*) (gpointer unused, const gchar *name)) on_sync_db_start,
            .on_sync_db_end =
                (void (*) (gpointer unused, guint result)) on_sync_db_end,
            .pac_conf = NULL
        };
        sync_dbs_t sync_dbs = { 0, 0 };
        alpm_list_t *packages = NULL;

        /* make sure the window is visible, etc */
        while (gtk_events_pending ())
            gtk_main_iteration ();

        updater->kupdater = NULL;
        if (!kalu_alpm_load (&simulation, config->pacmanconf, get_kalpm_synced_dbs (), &err))
        {
            _show_error (_("Failed to initialize simulation"),
                    err->message);
            /* _show_error() switched them on, but they should stay off */
            gtk_widget_set_sensitive (updater->btn_sysupgrade, FALSE);
            gtk_widget_set_sensitive (updater->btn_rerun, FALSE);
            g_clear_error (&err);
            return;
        }
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.42);
        /* connect now w/ the pac_conf to be used; will be free-d when handler
         * is disconnected, i.e. on window close */
        g_signal_connect_data (updater->btn_sysupgrade, "clicked",
                G_CALLBACK (btn_download_cb),
                simulation.pac_conf,
                (GClosureNotify) free_pacman_config,
                0);

        while (gtk_events_pending ())
            gtk_main_iteration ();

        updater->step = STEP_SYNC_DBS;
        updater->step_data = &sync_dbs;
        if (!kalu_alpm_syncdbs (get_kalpm_synced_dbs (), &err))
        {
            _show_error (_("Failed to synchronize databases"),
                    err->message);
            /* _show_error() switched them on, but they should stay off */
            gtk_widget_set_sensitive (updater->btn_sysupgrade, FALSE);
            gtk_widget_set_sensitive (updater->btn_rerun, FALSE);
            g_clear_error (&err);
            /* to not try to free it */
            updater->step_data = NULL;
            return;
        }
        updater->step_data = NULL;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 1);
        add_log (LOGTYPE_NORMAL, _("Databases synchronized\n"));

        kalu_alpm_has_updates (&packages, &err);
        updater_get_packages_cb (NULL, (err) ? err->message : NULL, packages, NULL);
        g_clear_error (&err);
        FREE_PACKAGE_LIST (packages);
    }
}
