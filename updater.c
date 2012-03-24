/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * updater.c
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
#include "kupdater.h"
#include "config.h"

/* split of pctg of each step in the global progress of the sysupgrade */
#define PCTG_DOWNLOAD           0.42
#define PCTG_PKG_INTEGRITY      0.01
#define PCTG_FILE_CONFLICTS     0.01
#define PCTG_LOAD_PKGFILES      0.01
#define PCTG_CHECK_DISKSPACE    0.01
#define PCTG_SYSUPGRADE         0.54

#define PCTG_NO_DL_PKG_INTEGRITY      0.05
#define PCTG_NO_DL_FILE_CONFLICTS     0.05
#define PCTG_NO_DL_LOAD_PKGFILES      0.05
#define PCTG_NO_DL_CHECK_DISKSPACE    0.05
#define PCTG_NO_DL_SYSUPGRADE         0.80

enum {
    UCOL_PACKAGE,
    UCOL_OLD,
    UCOL_NEW,
    UCOL_DL_SIZE,
    UCOL_OLD_SIZE,
    UCOL_NEW_SIZE,
    UCOL_NET_SIZE,
    UCOL_DL_IS_ACTIVE,
    UCOL_INST_IS_ACTIVE,
    UCOL_DL_IS_DONE,
    UCOL_INST_IS_DONE,
    UCOL_PCTG,
    NB_UCOL
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
} steps_t;

typedef struct _updater_t {
    GtkWidget *window;
    GtkWidget *lbl_main;
    GtkWidget *pbar_main;
    GtkWidget *lbl_action;
    GtkWidget *pbar_action;
    GtkListStore *store;
    GtkWidget *list;
    GtkWidget *expander;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *btn_sysupgrade;
    GtkWidget *btn_close;
    
    steps_t step;
    void *step_data;
    
    double pctg_download;
    double pctg_pkg_integrity;
    double pctg_file_conflicts;
    double pctg_load_pkgfiles;
    double pctg_check_diskspace;
    double pctg_sysupgrade;
    
    guint step_done;
    double pctg_done;
    
    guint total_inst;
    guint total_dl;
    
    KaluUpdater *kupdater;
    
    alpm_list_t *cmdline_post;
} updater_t;

static updater_t *updater = NULL;

static void
add_log (logtype_t type, const gchar *fmt, ...)
{
    GtkTextMark *mark;
    GtkTextIter iter;
    gchar buffer[1024];
    const char *tag;
    va_list args;
    
    va_start (args, fmt);
    vsnprintf (buffer, 1024, fmt, args);
    va_end (args);
    
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
    
    if (type == LOGTYPE_ERROR || type == LOGTYPE_WARNING || type == LOGTYPE_INFO)
    {
        gtk_expander_set_expanded (GTK_EXPANDER (updater->expander), TRUE);
    }
    
    mark = gtk_text_buffer_get_mark (updater->buffer, "end-mark");
    gtk_text_buffer_get_iter_at_mark (updater->buffer, &iter, mark);
    gtk_text_buffer_insert_with_tags_by_name (updater->buffer, &iter,
        buffer, -1, tag, NULL);
    /* scrolling to the end using gtk_text_view_scroll_to_iter doesn't work;
     * using a mark like so seems to work better... */
    gtk_text_view_scroll_mark_onscreen ( GTK_TEXT_VIEW (updater->text_view), mark);
}

static void
_show_error (const gchar *msg, const gchar *fmt, ...)
{
    gchar buffer[1024], buf[255];
    va_list args;
    
    snprintf (buf, 255, "<big><b><span color=\"red\">%s</span></b></big>", msg);
    
    va_start (args, fmt);
    vsnprintf (buffer, 1024, fmt, args);
    va_end (args);
    
    add_log (LOGTYPE_ERROR, "Error: %s: %s", msg, buffer);
    
    gtk_label_set_markup (GTK_LABEL (updater->lbl_main), buf);
    gtk_widget_hide (updater->pbar_main);
    gtk_label_set_text (GTK_LABEL (updater->lbl_action), buffer);
    gtk_widget_show (updater->lbl_action);
    gtk_widget_hide (updater->pbar_action);
    gtk_widget_set_sensitive (updater->btn_close, TRUE);
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
        gtk_text_view_scroll_mark_onscreen ( GTK_TEXT_VIEW (updater->text_view), mark);
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
    
    gtk_tree_model_get (store, iter, col_is_active, &is_active, UCOL_PCTG, &pctg, -1);
    gtk_cell_renderer_set_visible (renderer, is_active);
    val = (gint) (pctg * 100);
    g_object_set (renderer, "value", val, NULL);
    
    if (is_dl)
    {
        guint dl_size;
        char buf[255], buf_tot[23];
        double size;
        const char *unit;
        
        gtk_tree_model_get (store, iter, UCOL_DL_SIZE, &dl_size, -1);
        size = humanize_size (dl_size, '\0', &unit);
        snprintf (buf_tot, 23, "%.2f %s", size, unit);
        snprintf (buf, 255, "%d%% of %s", val, buf_tot);
        g_object_set (renderer, "text", buf, NULL);
    }
    else if (val == 100)
    {
        g_object_set (renderer, "text", "Post-processing...", NULL);
    }
    else
    {
        g_object_set (renderer, "text", NULL, NULL);
    }
}

static GtkTreeIter *
get_iter_for_pkg (const gchar *pkg, gboolean has_version)
{
    GtkTreeIter *iter = calloc (1, sizeof (*iter));
    GtkTreeModel *model = GTK_TREE_MODEL (updater->store);
    const gchar *name, *version, *s;
    gchar buf[255];
    
    gtk_tree_model_get_iter_first (model, iter);
    do
    {
        gtk_tree_model_get (model, iter, UCOL_PACKAGE, &name,
                            UCOL_NEW, &version, -1);
        if (has_version)
        {
            /* because from on_download we have package-version */
            snprintf (buf, 255, "%s-%s", name, version);
            s = buf;
        }
        else
        {
            s = name;
        }
        
        if (g_strcmp0 (pkg, s) == 0)
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
        add_log (LOGTYPE_ERROR, "Error: %s", msg);
    }
    else if (level & LOG_WARNING)
    {
        add_log (LOGTYPE_WARNING, "Warning: %s", msg);
    }
    else if (level & LOG_DEBUG)
    {
        debug ("ALPM: %s", msg);
    }
    else if (level & LOG_FUNCTION)
    {
        add_log (LOGTYPE_UNIMPORTANT, "ALPM: %s", msg);
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
    const gchar *msg;
    switch (event)
    {
        case EVENT_CHECKING_DEPS:
            msg = "Checking dependencies...";
            break;
        case EVENT_RESOLVING_DEPS:
            msg = "Resolving dependencies...";
            break;
        case EVENT_INTERCONFLICTS:
            msg = "Checking inter-conflict...";
            break;
        case EVENT_DELTA_INTEGRITY:
            msg = "Checking delta integrity...";
            break;
        case EVENT_DELTA_PATCHES:
            msg = "Applying delta patches...";
            break;
        case EVENT_DELTA_PATCH_DONE:
            msg = "Delta patching done.";
            break;
        case EVENT_DELTA_PATCH_FAILED:
            msg = "Delta patching failed.";
            break;
        default:
            return;
    }
    add_log (LOGTYPE_NORMAL, "%s\n", msg);
    if (updater->step == STEP_NONE)
    {
        gtk_label_set_text (GTK_LABEL (updater->lbl_action), msg);
        gtk_widget_show (updater->lbl_action);
        gtk_widget_hide (updater->pbar_action);
    }
}

static void
finish_pkg_install (const char *pkg, const char *version)
{
    pkg_iter_t *pkg_iter = updater->step_data;
    guint size = 0;
    
    if (g_strcmp0 (pkg, pkg_iter->filename) != 0)
    {
        char buf[255];
        snprintf (buf, 255, "%s-%s", pkg, version);
        
        /* locate pkg in tree */
        pkg_iter->iter = get_iter_for_pkg (buf, TRUE);
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
            gtk_tree_model_get_path (GTK_TREE_MODEL (updater->store), pkg_iter->iter),
            NULL, FALSE, 0, 0);
    }
    
    /* pkg progress */
    gtk_list_store_set (updater->store, pkg_iter->iter,
        UCOL_INST_IS_ACTIVE, FALSE,
        UCOL_INST_IS_DONE,   TRUE,
        -1);
    
    /* get pkg size */
    gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                        UCOL_NEW_SIZE, &size, -1);
    if (size == 0)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                            UCOL_OLD_SIZE, &size, -1);
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
    
    add_log (LOGTYPE_NORMAL, "Package installed: %s (%s)\n", pkg, version);
    finish_pkg_install (pkg, version);
    
    if (optdeps != NULL)
    {
        add_log (LOGTYPE_INFO, "Optional dependencies for %s:\n", pkg);
        for (i = optdeps; i; i = alpm_list_next (i))
        {
            add_log (LOGTYPE_INFO, "- %s\n", (const char *) i->data);
        }
    }
}

static void
on_event_removed (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                    const gchar *version)
{
    add_log (LOGTYPE_NORMAL, "Package removed: %s (%s)\n", pkg, version);
    finish_pkg_install (pkg, version);
}

static void
on_event_upgraded (KaluUpdater *kupdater _UNUSED_, const gchar *pkg,
                   const gchar *old_version, const gchar *new_version,
                   alpm_list_t *newoptdeps)
{
    alpm_list_t *i;
    
    add_log (LOGTYPE_NORMAL, "Package upgraded: %s (%s -> %s)\n",
             pkg, old_version, new_version);
    finish_pkg_install (pkg, new_version);
    
    if (newoptdeps != NULL)
    {
        add_log (LOGTYPE_INFO, "New optional dependencies for %s-%s:\n", pkg, new_version);
        for (i = newoptdeps; i; i = alpm_list_next (i))
        {
            add_log (LOGTYPE_INFO, "- %s\n", (const char *) i->data);
        }
    }
}

static void
on_event_retrieving_pkgs (KaluUpdater *kupdater _UNUSED_, const gchar *repo)
{
    add_log (LOGTYPE_NORMAL, "Downloading packages from %s\n", repo);
    if (updater->step == STEP_NONE)
    {
        gtk_label_set_text (GTK_LABEL (updater->lbl_action), "Downloading packages...");
        gtk_widget_show (updater->lbl_action);
        gtk_widget_hide (updater->pbar_action);
    }
}

static void
on_event_scriptlet (KaluUpdater *kupdater _UNUSED_, const gchar *msg)
{
    add_log (LOGTYPE_INFO, "%s", msg);
}

static void
on_event_delta_generating (KaluUpdater *kupdater _UNUSED_, const gchar *delta,
                           const gchar *dest)
{
    add_log (LOGTYPE_NORMAL, "Using delta %s to generate %s...", delta, dest);
}

static void
on_progress (KaluUpdater *kupdater _UNUSED_, event_t event, const gchar *pkg,
             int percent, guint total _UNUSED_, guint current _UNUSED_)
{
    steps_t step;
    const gchar *msg;
    double pctg_step;
    switch (event)
    {
        case EVENT_PKG_INTEGRITY:
            step = STEP_PKG_INTEGRITY;
            msg = "Checking packages integrity...";
            pctg_step = updater->pctg_pkg_integrity;
            break;
        case EVENT_FILE_CONFLICTS:
            step = STEP_FILE_CONFLICTS;
            msg = "Checking for file conflicts...";
            pctg_step = updater->pctg_file_conflicts;
            break;
        case EVENT_LOAD_PKGFILES:
            step = STEP_LOAD_PKGFILES;
            msg = "Loading package files...";
            pctg_step = updater->pctg_load_pkgfiles;
            break;
        case EVENT_CHECKING_DISKSPACE:
            step = STEP_CHECKING_DISKSPACE;
            msg = "Checking disk space...";
            pctg_step = updater->pctg_check_diskspace;
            break;
        case EVENT_INSTALLING:
        case EVENT_UPGRADING:
        case EVENT_REMOVING:
            step = STEP_UPGRADING;
            msg = "Upgrading system...";
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
        /* because there's no clear event when all downloads are done */
        if (updater->step == STEP_DOWNLOADING)
        {
            updater->pctg_done += updater->pctg_download;
        }
        updater->step = step;
        gtk_label_set_text (GTK_LABEL (updater->lbl_action), msg);
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action), 0.0);
        gtk_widget_show (updater->lbl_action);
        gtk_widget_show (updater->pbar_action);
        if (updater->step == STEP_UPGRADING)
        {
            updater->step_done = 0;
            updater->step_data = calloc (1, sizeof (pkg_iter_t));
        }
    }
    
    if (step == STEP_UPGRADING)
    {
        /* special case, since we need to update single progress in the Treeview */
        
        double pctg = (double) percent / 100;
        pkg_iter_t *pkg_iter = updater->step_data;
        guint size = 0;
        
        if (g_strcmp0 (pkg, pkg_iter->filename) != 0)
        {
            /* locate pkg in tree */
            pkg_iter->iter = get_iter_for_pkg (pkg, FALSE);
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
                gtk_tree_model_get_path (GTK_TREE_MODEL (updater->store), pkg_iter->iter),
                NULL, FALSE, 0, 0);
        }
        
        /* pkg progress */
        gtk_list_store_set (updater->store, pkg_iter->iter,
            UCOL_PCTG,           pctg,
            UCOL_INST_IS_ACTIVE, TRUE,
            -1);
        
        /* get pkg size */
        gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                            UCOL_NEW_SIZE, &size, -1);
        if (size == 0)
        {
            gtk_tree_model_get (GTK_TREE_MODEL (updater->store), pkg_iter->iter,
                                UCOL_OLD_SIZE, &size, -1);
        }
        
        if (updater->total_inst > 0)
        {
            /* upgrading progress */
            guint done = (guint) (size * pctg);
            pctg = (double) (updater->step_done + done) / updater->total_inst;
            if (pctg > 1.0)
            {
                debug("#1: pctg=%.2f", pctg);
            }
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action), pctg);
            
            /* global progress */
            pctg = updater->pctg_done + (pctg * updater->pctg_sysupgrade);
            if (pctg > 1.0)
            {
                debug("#2: pctg=%.2f", pctg);
            }
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), pctg);
        }
    }
    else
    {
        /* simpler stuff: progress in the pbar_action */
        double pctg = (double) percent / 100;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action), pctg);
        /* global progress */
        pctg *= pctg_step;
        pctg += updater->pctg_done;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), pctg);
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
    
    snprintf (question, 255, "%s is in IngorePkg/IgnoreGroup. Install anyway ?",
              pkg);
    snprintf (lbl_yes, 42,   "Install %s anyway", pkg);
    snprintf (lbl_no,  42,   "Do not install %s", pkg);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, NULL, lbl_yes, NULL, lbl_no, NULL, updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? "Yes" : "No");
    return answer;
}

static gboolean
on_replace_pkg (KaluUpdater *kupdater _UNUSED_, const gchar *repo1, const gchar *pkg1,
                const gchar *repo2, const gchar *pkg2)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;
    
    snprintf (question, 255, "Do you want to replace %s (%s) with %s (%s) ?",
              pkg1, repo1, pkg2, repo2);
    snprintf (lbl_yes, 42,   "Replace with %s", pkg2);
    snprintf (lbl_no,  42,   "Keep %s", pkg1);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, NULL, lbl_yes, NULL, lbl_no, NULL, updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? "Yes" : "No");
    return answer;
}

static gboolean
on_conflict_pkg (KaluUpdater *kupdater _UNUSED_, const gchar *pkg1, const gchar *pkg2,
                 const gchar *reason)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;
    
    snprintf (question, 255, "%s and %s and in conflict. Remove %s ?",
              pkg1, pkg2, pkg2);
    snprintf (lbl_yes, 42,   "Remove %s", pkg2);
    snprintf (lbl_no,  42,   "Keep %s", pkg1);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, reason, lbl_yes, NULL, lbl_no, NULL, updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? "Yes" : "No");
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
        const gchar *q = "Do you want to skip the packages listed below ?";
        
        s = g_strdup ("Those packages cannot be upgraded due to unresolvable dependencies :\n");
        add_log (LOGTYPE_INFO, "%s\n", q);
        add_log (LOGTYPE_INFO, "%s", s);
        for (i = pkgs; i; i = alpm_list_next (i))
        {
            snprintf (buf, 255, "- %s\n", (const char *) i->data);
            ss = g_strconcat (s, buf, NULL);
            g_free (s);
            s = ss;
            add_log (LOGTYPE_INFO, "%s", buf);
        }
        
        answer = confirm (q, s,
            "Yes, skip those packages", NULL,
            "No, abort upgrade", NULL,
            updater->window);
        g_free (s);
    }
    else
    {
        snprintf (question, 255, "Do you want to skip package %s ?", (const char *) pkgs->data);
        snprintf (buf, 255, "Package %s cannot be upgraded due to unresolvable dependencies.",
                  (const char *) pkgs->data);
        snprintf (lbl_yes, 42, "Yes, skip %s", (const char *) pkgs->data);
        add_log (LOGTYPE_INFO, "%s\n", question);
        add_log (LOGTYPE_INFO, "%s\n", buf);
        answer = confirm (question, buf,
            lbl_yes, NULL,
            "No, abort upgrade", NULL,
            updater->window);
    }
    add_log (LOGTYPE_INFO, "\t%s\n", (answer) ? "Yes" : "No");
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
        "There are %d providers available for %s.\nPlease select the one to install :",
        (int) alpm_list_count (providers), pkg);
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
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list), TRUE);
    
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
    column = gtk_tree_view_column_new_with_attributes ("Repo",
                                                       renderer,
                                                       "text", 1,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Package */
    column = gtk_tree_view_column_new_with_attributes ("Package",
                                                       renderer,
                                                       "text", 2,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Version */
    column = gtk_tree_view_column_new_with_attributes ("Version",
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
    for (i = providers; i; i = alpm_list_next (i))
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
    button = gtk_dialog_add_button (GTK_DIALOG (dialog), "Abort", 0);
    /* button Use selected provider */
    GtkWidget *image;
    image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
    button = gtk_button_new_with_label ("Use selected provider");
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_widget_show (button);
    box = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
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
on_local_newer (KaluUpdater *kupdater _UNUSED_, const gchar *pkg, const gchar *pkg_version,
                const gchar *repo, const gchar *repo_version)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;
    
    snprintf (question, 255, "Local version of %s is newer (%s) than in %s (%s)."
              "Install anyway ?", pkg, pkg_version, repo, repo_version);
    snprintf (lbl_yes, 42,   "Install %s", pkg);
    snprintf (lbl_no,  42,   "Skip %s", pkg);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, NULL, lbl_yes, NULL, lbl_no, NULL, updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? "Yes" : "No");
    return answer;
}

static gboolean
on_corrupted_pkg (KaluUpdater *kupdater _UNUSED_, const gchar *file, const gchar *error)
{
    gchar question[255], lbl_yes[42], lbl_no[42];
    gboolean answer;
    
    snprintf (question, 255, "File %s is corrupted. Do you want to delete it ?", file);
    snprintf (lbl_yes, 42,   "Delete %s", file);
    snprintf (lbl_no,  42,   "Do not delete %s", file);
    add_log (LOGTYPE_INFO, "%s", question);
    answer = confirm (question, error, lbl_yes, NULL, lbl_no, NULL, updater->window);
    add_log (LOGTYPE_INFO, " %s\n", (answer) ? "Yes" : "No");
    return answer;
}

static gboolean
on_import_key (KaluUpdater *kupdater _UNUSED_, const gchar *key_fingerprint,
               const gchar *key_uid, const gchar *key_created)
{
    gchar question[255], details[255];
    gchar *det;
    gboolean answer;
    
    snprintf (question, 255, "Do you want to import key %s ?", key_fingerprint);
    snprintf (details,  255, "Key %s: %s (created %s)", key_fingerprint, key_uid, key_created);
    det = g_markup_escape_text (details, -1);
    add_log (LOGTYPE_INFO, "%s\n%s\n", question, det);
    answer = confirm (question, det, "Yes, import key", NULL, "No", NULL, updater->window);
    g_free (det);
    add_log (LOGTYPE_INFO, "\t%s\n", (answer) ? "Yes" : "No");
    return answer;
}

static void
on_download (KaluUpdater *kupdater _UNUSED_, const gchar *filename, guint xfered, guint total)
{
    switch (updater->step)
    {
        case STEP_SYNC_DBS:
        {
            sync_dbs_t *sync_dbs = updater->step_data;
            double pctg;
            
            /* are we done? */
            if (xfered == total)
            {
                gtk_widget_hide (updater->lbl_action);
                gtk_widget_hide (updater->pbar_action);
            }
            else if (total > 0)
            {
                /* pctg of this download */
                pctg = (double) xfered / total;
                gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action), pctg);
                /* reduce pctg to value of one db */
                pctg *= 1.0 / sync_dbs->total;
                /* add already processed dbs */
                pctg += (double) sync_dbs->processed / sync_dbs->total;
                /* full sync-ing pctg */
                gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), pctg);
                
                if (!gtk_widget_get_visible (updater->lbl_action))
                {
                    gchar buf[255];
                    snprintf (buf, 255, "Downloading %s ...", filename);
                    gtk_label_set_text (GTK_LABEL (updater->lbl_action), buf);
                    gtk_widget_show (updater->lbl_action);
                    gtk_widget_show (updater->pbar_action);
                }
            }
        }
            break;

        case STEP_USER_CONFIRMED:
            /* adding "Downloading packages" to the log is done on corresponding event */
            gtk_label_set_text (GTK_LABEL (updater->lbl_action), "Downloading packages...");
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action), 0.0);
            gtk_widget_show (updater->lbl_action);
            gtk_widget_show (updater->pbar_action);
            updater->step = STEP_DOWNLOADING;
            updater->step_done = 0;
            updater->step_data = calloc (1, sizeof (pkg_iter_t));
            /* fall through */
        case STEP_DOWNLOADING:
        {
            /* first, we need to find the package we're dealing with */
            double pctg = (double) xfered / total;
            pkg_iter_t *pkg_iter = updater->step_data;
            
            if (g_strcmp0 (filename, pkg_iter->filename) != 0)
            {
                gchar *pkg, *s;
                
                /* remove -arch.pkg.tar.xz from filename, to only have package-version */
                pkg = strdup (filename);
                if (NULL == (s = strrchr (pkg, '-')))
                {
                    free (pkg);
                    debug ("on_download: invalid filename: %s", filename);
                    return;
                }
                *s = '\0';
                
                /* locate pkg in tree */
                pkg_iter->iter = get_iter_for_pkg (pkg, TRUE);
                if (pkg_iter->iter == NULL)
                {
                    free (pkg);
                    debug ("on_download: unable to find iter for %s", pkg);
                    return;
                }
                free (pkg);
                if (pkg_iter->filename != NULL)
                {
                    free (pkg_iter->filename);
                }
                pkg_iter->filename = strdup (filename);
                
                /* make sure it's visible */
                gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (updater->list),
                    gtk_tree_model_get_path (GTK_TREE_MODEL (updater->store), pkg_iter->iter),
                    NULL, FALSE, 0, 0);
            }
            
            /* pkg progress */
            gtk_list_store_set (updater->store, pkg_iter->iter,
                UCOL_PCTG,          pctg,
                UCOL_DL_IS_ACTIVE,  (xfered < total),
                UCOL_DL_IS_DONE,    xfered == total,
                -1);
            
            if (updater->total_dl > 0)
            {
                /* download progress */
                pctg = (double) (updater->step_done + xfered) / updater->total_dl;
                gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_action), pctg);
                
                /* global progress */
                pctg = updater->pctg_done + (pctg * updater->pctg_download);
                gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), pctg);
            }
            
            /* are we done? */
            if (xfered == total)
            {
                updater->step_done += total;
                /* free */
                free (pkg_iter->filename);
                pkg_iter->filename = NULL;
                free (pkg_iter->iter);
                pkg_iter->iter = NULL;
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
                 "Error: got a SYNC_DBS event while not synchronizing databases\n");
        return;
    }
    
    sync_dbs_t *sync_db = updater->step_data;
    sync_db->total = nb;
    add_log (LOGTYPE_UNIMPORTANT, "Synchronizing %d databases\n", nb);
    gtk_label_set_text (GTK_LABEL (updater->lbl_main), "Synchronizing databases...");
}

static void
on_sync_db_start (KaluUpdater *kupdater _UNUSED_, const gchar *name)
{
    if (updater->step != STEP_SYNC_DBS)
    {
        add_log (LOGTYPE_ERROR,
                 "Error: got a SYNC_DB_START event while not synchronizing databases\n");
        return;
    }
    
    sync_dbs_t *sync_db = updater->step_data;
    double pctg;
    
    add_log (LOGTYPE_NORMAL, "Synchronizing database %s... ", name);
    pctg = (double) sync_db->processed / sync_db->total;
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), pctg);
}

static void
on_sync_db_end (KaluUpdater *kupdater _UNUSED_, sync_db_results_t result)
{
    if (updater->step != STEP_SYNC_DBS)
    {
        add_log (LOGTYPE_ERROR, "Error: got a SYNC_DB_END event while not synchronizing databases\n");
        return;
    }
    
    sync_dbs_t *sync_db = updater->step_data;
    double pctg;
    
    if (result == SYNC_FAILURE)
    {
        add_log (LOGTYPE_ERROR, "failed\n");
    }
    else
    {
        add_log (LOGTYPE_NORMAL, "ok\n");
    }
    
    if (sync_db->total > 0)
    {
        pctg = (double) ++(sync_db->processed) / sync_db->total;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), pctg);
    }
}

static void
btn_close_cb (GtkButton *button _UNUSED_, gpointer data _UNUSED_)
{
    if (updater->kupdater != NULL)
    {
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
    }
    gtk_widget_destroy (updater->window);
}

static void
updater_sysupgrade_cb (KaluUpdater *kupdater _UNUSED_, const gchar *errmsg)
{
    gtk_widget_set_sensitive (updater->btn_close, TRUE);
    
    if (errmsg != NULL)
    {
        _show_error ("System update failed", "%s", errmsg);
        return;
    }
    
    add_log (LOGTYPE_NORMAL, "System upgrade complete.\n");
    gtk_label_set_markup (GTK_LABEL (updater->lbl_main),
        "<big><b><span color=\"blue\">System upgrade complete.</span></b></big>");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 1.0);
    gtk_widget_hide (updater->lbl_action);
    gtk_widget_hide (updater->pbar_action);
    
    /* update nb of upgrades in state (used in kalu's systray icon tooltip) */
    set_kalpm_nb (CHECK_UPGRADES, 0);
    
    if (NULL != updater->cmdline_post)
    {
        GError *error = NULL;
        size_t count = alpm_list_count (updater->cmdline_post);
        
        if (count == 1)
        {
            add_log (LOGTYPE_INFO, "Do you want to run the post-sysupgrade process ?");
            if (confirm ("Do you want to run the post-sysupgrade process ?",
                    updater->cmdline_post->data,
                    "Yes, Start it now", NULL,
                    "No", NULL,
                    updater->window))
            {
                add_log (LOGTYPE_INFO, " Yes\n");
                add_log (LOGTYPE_NORMAL, "Starting: '%s' ..", updater->cmdline_post->data);
                if (!g_spawn_command_line_async (updater->cmdline_post->data, &error))
                {
                    add_log (LOGTYPE_NORMAL, " failed\n");
                    _show_error ("Unable to start post-sysupgrade process.",
                        "Command-line: %s\nError: %s",
                        updater->cmdline_post->data, error->message);
                    g_clear_error (&error);
                }
                else
                {
                    add_log (LOGTYPE_NORMAL, " ok\n");
                }
            }
            else
            {
                add_log (LOGTYPE_INFO, " No\n");
            }
        }
        else
        {
            alpm_list_t *cmdlines, *i;
            add_log (LOGTYPE_INFO, "Do you want to run the post-sysupgrade processes ?");
            cmdlines = confirm_choices (
            "Do you want to run the following post-sysupgrade processes ?",
                "Only checked processes will be launch.",
                "Yes, run checked processes now", NULL,
                "No", NULL,
                "Run",
                "Command-line",
                updater->cmdline_post,
                updater->window);
            if (NULL != cmdlines)
            {
                add_log (LOGTYPE_INFO, " Yes\n");
                for (i = cmdlines; i; i = alpm_list_next (i))
                {
                    add_log (LOGTYPE_NORMAL, "Starting: '%s' ..", i->data);
                    if (!g_spawn_command_line_async (i->data, &error))
                    {
                        add_log (LOGTYPE_NORMAL, " failed\n");
                        _show_error ("Unable to start post-sysupgrade process.",
                            "Command-line: %s\nError: %s",
                            i->data, error->message);
                        g_clear_error (&error);
                    }
                    else
                    {
                        add_log (LOGTYPE_NORMAL, " ok\n");
                    }
                }
                FREELIST (cmdlines);
            }
            else
            {
                add_log (LOGTYPE_INFO, " No\n");
            }
        }
    }
}

static void
btn_sysupgrade_cb (GtkButton *button _UNUSED_, gpointer data _UNUSED_)
{
    GError *error = NULL;
    
    if (updater->step != STEP_WAITING_USER_CONFIRMATION)
    {
        _show_error ("Unable to start system upgrade", "Invalid internal state");
        return;
    }
    gtk_widget_set_sensitive (updater->btn_close, FALSE);
    gtk_widget_set_sensitive (updater->btn_sysupgrade, FALSE);
    updater->step = STEP_USER_CONFIRMED;
    gtk_label_set_text (GTK_LABEL (updater->lbl_main), "Performing system upgrade...");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.0);
    gtk_widget_show (updater->lbl_main);
    gtk_widget_show (updater->pbar_main);
    gtk_widget_hide (updater->lbl_action);
    gtk_widget_hide (updater->pbar_action);
    
    if (!kalu_updater_sysupgrade (updater->kupdater, NULL,
             (KaluMethodCallback) updater_sysupgrade_cb, NULL, &error))
    {
        _show_error ("Unable to start system upgrade", error->message);
        g_clear_error (&error);
        return;
    }
}

static void
updater_get_packages_cb (KaluUpdater *kupdater _UNUSED_, const gchar *errmsg,
                         alpm_list_t *pkgs, gpointer data _UNUSED_)
{
    alpm_list_t *i;
    kalu_package_t *k_pkg;
    GtkTreeIter iter;
    
    if (errmsg != NULL)
    {
        _show_error ("Failed to get packages list", "%s", errmsg);
        return;
    }
    else if (pkgs == NULL)
    {
        _show_error ("No packages to upgrade", "Your system is already up-to-date.");
        return;
    }
    
    guint dl_size = 0;
    guint inst_size = 0;
    gint net_size = 0;
    updater->total_dl = 0; /* will be set through on_total_download */
    updater->total_inst = 0;
    for (i = pkgs; i; i = alpm_list_next (i))
    {
        k_pkg = i->data;
        gtk_list_store_append (updater->store, &iter);
        gtk_list_store_set (updater->store, &iter,
            UCOL_PACKAGE,           k_pkg->name,
            UCOL_OLD,               k_pkg->old_version,
            UCOL_NEW,               k_pkg->new_version,
            UCOL_DL_SIZE,           k_pkg->dl_size,
            UCOL_OLD_SIZE,          k_pkg->old_size,
            UCOL_NEW_SIZE,          k_pkg->new_size,
            UCOL_NET_SIZE,          k_pkg->new_size - k_pkg->old_size,
            UCOL_DL_IS_ACTIVE,      FALSE,
            UCOL_INST_IS_ACTIVE,    FALSE,
            UCOL_DL_IS_DONE,        k_pkg->dl_size == 0,
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
    
    gchar buffer[255];
    double size;
    const char *unit;
    char dl_buf[23], inst_buf[23], net_buf[23];
    
    size = humanize_size (inst_size, '\0', &unit);
    snprintf (inst_buf, 23, "%.2f %s", size, unit);
    size = humanize_size (net_size, '\0', &unit);
    snprintf (net_buf, 23, "%.2f %s", size, unit);
    
    gtk_widget_hide (updater->pbar_main);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0);
    
    gtk_label_set_markup (GTK_LABEL (updater->lbl_main),
        "<big><b>Do you want to upgrade your system ?</b></big>");
    if (dl_size > 0)
    {
        updater->pctg_download          = PCTG_DOWNLOAD;
        updater->pctg_pkg_integrity     = PCTG_PKG_INTEGRITY;
        updater->pctg_file_conflicts    = PCTG_FILE_CONFLICTS;
        updater->pctg_load_pkgfiles     = PCTG_LOAD_PKGFILES;
        updater->pctg_check_diskspace   = PCTG_CHECK_DISKSPACE;
        updater->pctg_sysupgrade        = PCTG_SYSUPGRADE;
        
        size = humanize_size (dl_size, '\0', &unit);
        snprintf (dl_buf, 23, "%.2f %s", size, unit);
        snprintf (buffer, 255, "Download:\t%s\nInstall:\t\t%s\nNet:\t\t\t%s",
                  dl_buf, inst_buf, net_buf);
    }
    else
    {
        updater->pctg_download          = 0;
        updater->pctg_pkg_integrity     = PCTG_NO_DL_PKG_INTEGRITY;
        updater->pctg_file_conflicts    = PCTG_NO_DL_FILE_CONFLICTS;
        updater->pctg_load_pkgfiles     = PCTG_NO_DL_LOAD_PKGFILES;
        updater->pctg_check_diskspace   = PCTG_NO_DL_CHECK_DISKSPACE;
        updater->pctg_sysupgrade        = PCTG_NO_DL_SYSUPGRADE;
        
        snprintf (buffer, 255, "Install:\t\t%s\nNet:\t\t\t%s", inst_buf, net_buf);
    }
    gtk_label_set_markup (GTK_LABEL (updater->lbl_action), buffer);
    gtk_widget_show (updater->lbl_action);
    gtk_widget_set_sensitive (updater->btn_sysupgrade, TRUE);
    gtk_widget_set_sensitive (updater->btn_close, TRUE);
    updater->step = STEP_WAITING_USER_CONFIRMATION;
    add_log (LOGTYPE_UNIMPORTANT, "Got package list; waiting for user confirmation.\n");
}

static void
updater_sync_dbs_cb (KaluUpdater *kupdater, const gchar *errmsg, gpointer data _UNUSED_)
{
    GError *error = NULL;
    
    if (errmsg != NULL)
    {
        _show_error ("Failed to synchronize databases", "%s", errmsg);
        free (updater->step_data);
        updater->step_data = NULL;
        updater->step = STEP_NONE;
        return;
    }
    
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 1);
    add_log (LOGTYPE_NORMAL, "Databases synchronized\n");
    set_kalpm_nb_syncdbs (0);
    
    add_log (LOGTYPE_UNIMPORTANT, "Getting packages list\n");
    gtk_label_set_text (GTK_LABEL (updater->lbl_main), "Getting packages list...");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.0);
    if (!kalu_updater_get_packages (kupdater,
                NULL,
                (KaluMethodCallback) updater_get_packages_cb,
                NULL,
                &error))
    {
        _show_error ("Failed to get packages list", "%s", error->message);
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
            add_log (LOGTYPE_ERROR, " failed\n");
            _show_error ("Failed to register databases", "%s", errmsg);
            free_pacman_config (add_db->pac_conf);
            free (add_db);
            return;
        }
        add_log (LOGTYPE_NORMAL, " ok\n");
        add_db->pctg += add_db->inc;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), add_db->pctg);
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
        add_log (LOGTYPE_NORMAL, "Register database %s... ", db_conf->name);
        if (!kalu_updater_add_db (kupdater,
                             db_conf->name,
                             db_conf->siglevel,
                             db_conf->servers,
                             NULL,
                             (KaluMethodCallback) updater_add_db_cb,
                             (gpointer) add_db,
                             &error))
        {
            add_log (LOGTYPE_ERROR, " failed\n");
            _show_error ("Failed to register databases", "%s", error->message);
            g_clear_error (&error);
            free_pacman_config (add_db->pac_conf);
            free (add_db);
            return;
        }
     }
     else
     {
         free_pacman_config (add_db->pac_conf);
         free (add_db);
         gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 1);
         /* we're good, so let's sync dbs */
         updater->step_data = calloc(1, sizeof (sync_dbs_t));
         updater->step = STEP_SYNC_DBS;
         if (!kalu_updater_sync_dbs (kupdater,
                                NULL,
                                (KaluMethodCallback) updater_sync_dbs_cb,
                                NULL,
                                &error))
        {
            _show_error ("Failed to synchronize databases", "%s", error->message);
            g_clear_error (&error);
            free (updater->step_data);
            updater->step_data = NULL;
            updater->step = STEP_NONE;
        }
     }
}

static void
updater_init_alpm_cb (KaluUpdater *kupdater, const gchar *errmsg, pacman_config_t *pac_conf)
{
    if (errmsg != NULL)
    {
        add_log (LOGTYPE_UNIMPORTANT, " failed\n");
        _show_error ("Failed to initialize ALPM library", "%s", errmsg);
        free_pacman_config (pac_conf);
        return;
    }
    add_log (LOGTYPE_UNIMPORTANT, " ok\n");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.42);
    
    add_log (LOGTYPE_UNIMPORTANT, "Registering databases\n");
    add_db_t *add_db = calloc (1, sizeof (*add_db));
    add_db->pac_conf = pac_conf;
    add_db->pctg = 0.42;
    add_db->inc = 0.58 / (int) alpm_list_count (pac_conf->databases);
    add_db->i = NULL;
    
    updater_add_db_cb (kupdater, NULL, add_db);
}

static void
updater_method_cb (KaluUpdater *kupdater, const gchar *errmsg, pacman_config_t *pac_conf)
{
    GError *error = NULL;
    
    if (errmsg != NULL)
    {
        add_log (LOGTYPE_UNIMPORTANT, " failed\n");
        _show_error ("Failed to initialize", "%s", errmsg);
        free_pacman_config (pac_conf);
        return;
    }
    add_log (LOGTYPE_UNIMPORTANT, " ok\n");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.23);
    
    add_log (LOGTYPE_UNIMPORTANT, "Initializing ALPM library...");
    gtk_label_set_text (GTK_LABEL (updater->lbl_main), "Initializing ALPM library...");
    if (!kalu_updater_init_alpm (kupdater,
                            pac_conf->rootdir,
                            pac_conf->dbpath,
                            pac_conf->logfile,
                            pac_conf->gpgdir,
                            pac_conf->cachedirs,
                            pac_conf->siglevel,
                            pac_conf->arch,
                            pac_conf->checkspace,
                            pac_conf->usesyslog,
                            pac_conf->usedelta,
                            pac_conf->ignorepkgs,
                            pac_conf->ignoregroups,
                            pac_conf->noupgrades,
                            pac_conf->noextracts,
                            NULL,
                            (KaluMethodCallback) updater_init_alpm_cb,
                            (gpointer) pac_conf,
                            &error))
    {
        add_log (LOGTYPE_UNIMPORTANT, " failed\n");
        _show_error ("Failed to initialize ALPM library", "%s", error->message);
        g_clear_error (&error);
        free_pacman_config (pac_conf);
    }
}

static void
updater_new_cb (GObject *source _UNUSED_, GAsyncResult *res, pacman_config_t *pac_conf)
{
    GError *error = NULL;
    KaluUpdater *kalu_updater;

    kalu_updater = kalu_updater_new_finish (res, &error);
    if (kalu_updater == NULL)
    {
        add_log (LOGTYPE_UNIMPORTANT, " failed\n");
        _show_error ("Could not initiate kalu_updater", "%s", error->message);
        g_clear_error (&error);
        free_pacman_config (pac_conf);
        gtk_widget_destroy (updater->window);
        return;
    }
    add_log (LOGTYPE_UNIMPORTANT, " ok \n");
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
                      "event-installed",
                      G_CALLBACK (on_event_installed),
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
                      "event-retrieving-pkgs",
                      G_CALLBACK (on_event_retrieving_pkgs),
                      NULL);
    g_signal_connect (kalu_updater,
                      "event-scriptlet",
                      G_CALLBACK (on_event_scriptlet),
                      NULL);
    g_signal_connect (kalu_updater,
                      "event-delta-generating",
                      G_CALLBACK (on_event_delta_generating),
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
                      "local-newer",
                      G_CALLBACK (on_local_newer),
                      NULL);
    g_signal_connect (kalu_updater,
                      "corrupted-pkg",
                      G_CALLBACK (on_corrupted_pkg),
                      NULL);
    g_signal_connect (kalu_updater,
                      "import-key",
                      G_CALLBACK (on_import_key),
                      NULL);
    
    add_log (LOGTYPE_UNIMPORTANT, "Initializing kalu_updater...");
    if (!kalu_updater_init_upd (kalu_updater, NULL,
        (KaluMethodCallback) updater_method_cb, (gpointer) pac_conf, &error))
    {
        add_log (LOGTYPE_UNIMPORTANT, " failed\n");
        _show_error ("Could not initialize kalu_updater", "%s", error->message);
        g_clear_error (&error);
        free_pacman_config (pac_conf);
    }
}

static gboolean
window_delete_event_cb (GtkWidget *window _UNUSED_, GdkEvent *event _UNUSED_,
                        gpointer data _UNUSED_)
{
    /* cannot close window */
    return TRUE;
}

static void
window_destroy_cb (GtkWidget *window _UNUSED_, gpointer data _UNUSED_)
{
    if (NULL != updater->step_data)
    {
        free (updater->step_data);
        updater->step_data = NULL;
    }
    
    g_object_unref (updater->kupdater);
    updater->kupdater = NULL;
    
    free (updater);
    updater = NULL;
    
    set_kalpm_busy (FALSE);
}

void
updater_run (alpm_list_t *cmdline_post)
{
    updater = calloc (1, sizeof (*updater));
    updater->cmdline_post = cmdline_post;
    
    /* the window */
    GtkWidget *window;
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    updater->window = window;
    gtk_window_set_title (GTK_WINDOW (window), "System upgrade - kalu");
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
    /* icon */
    GtkWidget *image;
    GdkPixbuf *pixbuf;
    pixbuf = gtk_widget_render_icon_pixbuf (window, "kalu-logo", GTK_ICON_SIZE_DIALOG);
    gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
    g_object_unref (pixbuf);
    
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
    label = gtk_label_new ("Initializing... Please wait...");
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
    gtk_widget_set_margin_left (vbox2, 10);
    gtk_widget_set_margin_right (vbox2, 10);
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
    gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);
    gtk_widget_show (paned);
    
    /* store for the list */
    GtkListStore *store;
    store = gtk_list_store_new (NB_UCOL,
                G_TYPE_STRING,  /* pkg */
                G_TYPE_STRING,  /* old version */
                G_TYPE_STRING,  /* new version */
                G_TYPE_UINT,    /* dl size */
                G_TYPE_UINT,    /* old (inst) size */
                G_TYPE_UINT,    /* new (inst) size */
                G_TYPE_INT,     /* net size */
                G_TYPE_BOOLEAN, /* dl is active */
                G_TYPE_BOOLEAN, /* inst in active */
                G_TYPE_BOOLEAN, /* dl is done */
                G_TYPE_BOOLEAN, /* inst in done */
                G_TYPE_DOUBLE   /* pctg done */
                );
    updater->store = store;
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                          UCOL_PACKAGE,
                                          GTK_SORT_ASCENDING);
    
    /* said list */
    GtkWidget *list;
    list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    updater->list = list;
    g_object_unref (store);
    /* hint for alternate row colors */
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list), TRUE);
    
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
    /* column: Package */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Package",
                                                       renderer,
                                                       "text", UCOL_PACKAGE,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Old version */
    column = gtk_tree_view_column_new_with_attributes ("Old/Current",
                                                       renderer,
                                                       "text", UCOL_OLD,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: New version */
    column = gtk_tree_view_column_new_with_attributes ("New",
                                                       renderer,
                                                       "text", UCOL_NEW,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Download size */
    renderer_lbl = gtk_cell_renderer_text_new ();
    g_object_set (renderer_lbl, "foreground", "blue", NULL);
    renderer_pbar = gtk_cell_renderer_progress_new ();
    column = gtk_tree_view_column_new_with_attributes ("Download",
                                                       renderer_lbl,
                                                       "foreground-set",
                                                       UCOL_DL_IS_DONE,
                                                       NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer_lbl,
        (GtkTreeCellDataFunc) rend_size_pb, (gpointer) TRUE, NULL);
    gtk_cell_renderer_set_visible (renderer_pbar, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pbar,
        (GtkTreeCellDataFunc) rend_pbar_pb, (gpointer) TRUE, NULL);
    gtk_tree_view_column_pack_start (column, renderer_pbar, TRUE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Installed size */
    renderer_lbl = gtk_cell_renderer_text_new ();
    g_object_set (renderer_lbl, "foreground", "blue", NULL);
    renderer_pbar = gtk_cell_renderer_progress_new ();
    column = gtk_tree_view_column_new_with_attributes ("Installed",
                                                       renderer_lbl,
                                                       "foreground-set",
                                                       UCOL_INST_IS_DONE,
                                                       NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer_lbl,
        (GtkTreeCellDataFunc) rend_size_pb, (gpointer) FALSE, NULL);
    gtk_cell_renderer_set_visible (renderer_pbar, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pbar,
        (GtkTreeCellDataFunc) rend_pbar_pb, (gpointer) FALSE, NULL);
    gtk_tree_view_column_pack_start (column, renderer_pbar, TRUE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Net size */
    column = gtk_tree_view_column_new_with_attributes ("Net",
                                                       renderer,
                                                       NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer,
        (GtkTreeCellDataFunc) rend_net_size, NULL, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    
    gtk_container_add (GTK_CONTAINER (scrolled_window), list);
    gtk_widget_show (list);
    
    /* expander */
    GtkWidget *expander;
    expander = gtk_expander_new ("Log");
    updater->expander = expander;
    gtk_expander_set_resize_toplevel (GTK_EXPANDER (expander), TRUE);
    gtk_paned_pack2 (GTK_PANED (paned), expander, FALSE, FALSE);
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
    gtk_text_buffer_create_tag (updater->buffer, "unimportant", "foreground", "gray", NULL);
    gtk_text_buffer_create_tag (updater->buffer, "normal", NULL);
    gtk_text_buffer_create_tag (updater->buffer, "info", "foreground", "blue", NULL);
    gtk_text_buffer_create_tag (updater->buffer, "warning", "foreground", "green", NULL);
    gtk_text_buffer_create_tag (updater->buffer, "error", "foreground", "red", NULL);
    /* create mark at the end, where we'll insert stuff/scroll to */
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter (updater->buffer, &iter);
    gtk_text_buffer_create_mark (updater->buffer, "end-mark", &iter, FALSE);

    /* scrolledwindow for text view */
    scrolled_window = gtk_scrolled_window_new (
        gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (text_view)),
        gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (text_view)));
    gtk_container_add (GTK_CONTAINER (expander), scrolled_window);
    gtk_widget_show (scrolled_window);
    
    gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
    gtk_widget_show (text_view);
    
    /* button box */
    GtkWidget *hbox;
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);
    
    GtkWidget *button;
    /* Upgrade system */
    button = gtk_button_new_with_label ("Upgrade system...");
    updater->btn_sysupgrade = button;
    image = gtk_image_new_from_stock ("kalu-logo", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (btn_sysupgrade_cb), NULL);
    gtk_widget_show (button);
    
    /* Close */
    button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    updater->btn_close = button;
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (btn_close_cb), NULL);
    gtk_widget_show (button);
    
    /* signals */
    g_signal_connect (G_OBJECT (window), "delete-event",
                      G_CALLBACK (window_delete_event_cb), NULL);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (window_destroy_cb), NULL);
    
    gtk_widget_show (window);
    
    /* parse pacman.conf */
    GError *error = NULL;
    pacman_config_t *pac_conf = NULL;
    add_log (LOGTYPE_UNIMPORTANT, "Parsing pacman.conf...");
    if (!parse_pacman_conf ("/etc/pacman.conf", NULL, 0, 0, &pac_conf, &error))
    {
        add_log (LOGTYPE_UNIMPORTANT, " failed\n");
        _show_error ("Unable to parse pacman.conf", "%s", error->message);
        g_clear_error (&error);
        free_pacman_config (pac_conf);
        gtk_widget_destroy (window);
        return;
    }
    add_log (LOGTYPE_UNIMPORTANT, " ok\n");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (updater->pbar_main), 0.05);
    
    /* we must have databases */
    if (alpm_list_count (pac_conf->databases) == 0)
    {
        _show_error ("No databases defined", NULL);
        free_pacman_config (pac_conf);
        gtk_widget_destroy (window);
        return;
    }
    
    /* create kalu_updater */
    add_log (LOGTYPE_UNIMPORTANT, "Creating kalu_updater...");
    kalu_updater_new (NULL, (GAsyncReadyCallback) updater_new_cb, (gpointer) pac_conf);
}
