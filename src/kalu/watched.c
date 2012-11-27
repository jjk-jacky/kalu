/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * watched.c
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

#include <config.h>

/* C */
#include <string.h>

/* gtk */
#include <gtk/gtk.h>

/* alpm */
#include <alpm_list.h>

/* kalu */
#include "kalu.h"
#include "watched.h"
#include "util.h"
#include "util-gtk.h"
#include "gui.h" /* show_notif() */

enum {
    WCOL_UPD,
    WCOL_NAME,
    WCOL_OLD_VERSION,
    WCOL_NEW_VERSION,
    WCOL_DL_SIZE,
    WCOL_INS_SIZE,
    WCOL_NB
};

struct _replace_t {
    char *name;
    char *old;
    char *new;
};

typedef enum {
    W_NOTIF,
    W_MANAGE,
    W_NOTIF_AUR,
    W_MANAGE_AUR
} w_type_t;

struct _watched_t {
    GtkWidget *window_notif;
    GtkWidget *tree_notif;
    
    GtkWidget *window_manage;
    GtkWidget *tree_manage;
    
    GtkWidget *window_notif_aur;
    GtkWidget *tree_notif_aur;
    
    GtkWidget *window_manage_aur;
    GtkWidget *tree_manage_aur;
};

static struct _watched_t watched = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static GtkWidget *watched_new_window (w_type_t type);
static void manage_load_watched (gboolean is_aur);




static void
renderer_toggle_cb (GtkCellRendererToggle *renderer _UNUSED_, gchar *path,
                    gboolean is_aur)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean upd;
    
    if (is_aur)
    {
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_notif_aur));
    }
    else
    {
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_notif));
    }
    
    gtk_tree_model_get_iter_from_string (model, &iter, path);
    gtk_tree_model_get (model, &iter, WCOL_UPD, &upd, -1);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, WCOL_UPD, !upd, -1);
}

static void
window_destroy_cb (GtkWidget *window, w_type_t type)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *name = NULL;
    char *old  = NULL;
    char *new  = NULL;
    
    switch (type)
    {
        case W_NOTIF:
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_notif));
            break;
        case W_MANAGE:
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage));
            break;
        case W_NOTIF_AUR:
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_notif_aur));
            break;
        case W_MANAGE_AUR:
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage_aur));
            break;
        default:
            return;
    }
    
    /* free memory */
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        while (1)
        {
            gtk_tree_model_get (model, &iter, WCOL_NAME, &name,
                WCOL_OLD_VERSION, &old, WCOL_NEW_VERSION, &new, -1);
            free (name);
            free (old);
            free (new);
            
            /* next */
            if (!gtk_tree_model_iter_next (model, &iter))
            {
                break;
            }
        }
    }
    
    switch (type)
    {
        case W_NOTIF:
            watched.window_notif = NULL;
            watched.tree_notif = NULL;
            break;
        case W_MANAGE:
            watched.window_manage = NULL;
            watched.tree_manage = NULL;
            break;
        case W_NOTIF_AUR:
            watched.window_notif_aur = NULL;
            watched.tree_notif_aur = NULL;
            break;
        case W_MANAGE_AUR:
            watched.window_manage_aur = NULL;
            watched.tree_manage_aur = NULL;
            break;
    }
    
    /* remove from list of open windows */
    remove_open_window (window);
}

static void
btn_close_cb (GtkButton *button _UNUSED_, w_type_t type)
{
    switch (type)
    {
        case W_NOTIF:
            gtk_widget_destroy (watched.window_notif);
            break;
        case W_MANAGE:
            gtk_widget_destroy (watched.window_manage);
            break;
        case W_NOTIF_AUR:
            gtk_widget_destroy (watched.window_notif_aur);
            break;
        case W_MANAGE_AUR:
            gtk_widget_destroy (watched.window_manage_aur);
            break;
    }
}

static gboolean
save_watched (gboolean is_aur, alpm_list_t *new_watched)
{
    FILE *fp;
    char file[MAX_PATH];
    alpm_list_t *i;
    gboolean success = FALSE;
    
    if (is_aur)
    {
        snprintf (file, MAX_PATH - 1, "%s/.config/kalu/watched-aur.conf", g_get_home_dir ());
    }
    else
    {
        snprintf (file, MAX_PATH - 1, "%s/.config/kalu/watched.conf", g_get_home_dir ());
    }
    
    if (ensure_path (file))
    {
        fp = fopen (file, "w");
        if (fp != NULL)
        {
            for (i = new_watched; i; i = alpm_list_next (i))
            {
                watched_package_t *w_pkg = i->data;
                fputs (w_pkg->name, fp);
                fputs ("=", fp);
                fputs (w_pkg->version, fp);
                fputs ("\n", fp);
            }
            
            fclose (fp);
            success = TRUE;
        }
    }
    
    return success;
}

static int
watched_package_name_cmp (watched_package_t *w_pkg1, watched_package_t *w_pkg2)
{
    return strcmp (w_pkg1->name, w_pkg2->name);
}

static void
monitor_response_cb (GtkWidget *dialog, gint response, alpm_list_t *updates)
{
    gtk_widget_destroy (dialog);
    if (response == GTK_RESPONSE_YES)
    {
        GtkTreeModel *model;
        GtkTreeIter iter;
        watched_package_t *w_pkg, w_pkg_tmp;
        gboolean is_aur = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "is-aur"));
        
        if (is_aur)
        {
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage_aur));
        }
        else
        {
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage));
        }
        
        if (gtk_tree_model_get_iter_first (model, &iter))
        {
            while (1)
            {
                /* get packge info from list */
                gtk_tree_model_get (model, &iter,
                    WCOL_NAME,        &(w_pkg_tmp.name),
                    WCOL_OLD_VERSION, &(w_pkg_tmp.version),
                    -1);
                /* was this package updated ? */
                w_pkg = alpm_list_find (updates, &w_pkg_tmp,
                    (alpm_list_fn_cmp) watched_package_name_cmp);
                if (NULL != w_pkg)
                {
                    /* update version */
                    free (w_pkg_tmp.version);
                    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        WCOL_OLD_VERSION, strdup (w_pkg->version), -1);
                }
                
                /* next */
                if (!gtk_tree_model_iter_next (model, &iter))
                {
                    break;
                }
            }
        }
    }
    
    alpm_list_free (updates);
}

static void
btn_mark_cb (GtkButton *button _UNUSED_, gboolean is_aur)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean upd;
    gchar *new;
    watched_package_t *w_pkg, *w_pkg2, w_pkg_tmp;
    alpm_list_t *updates = NULL;
    int nb_watched = 0;
    GtkWidget *window_notif, *window_manage;
    alpm_list_t **cfglist, *i, *new_watched = NULL;
    
    if (is_aur)
    {
        window_notif = watched.window_notif_aur;
        window_manage = watched.window_manage_aur;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_notif_aur));
        cfglist = &(config->watched_aur);
    }
    else
    {
        window_notif = watched.window_notif;
        window_manage = watched.window_manage;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_notif));
        cfglist = &(config->watched);
    }
    
    gtk_widget_hide (window_notif);
    
    /* duplicate the list */
    for (i = *cfglist; i; i = alpm_list_next (i))
    {
        w_pkg = i->data;
        w_pkg2 = calloc (1, sizeof (*w_pkg2));
        w_pkg2->name = strdup (w_pkg->name);
        w_pkg2->version = strdup (w_pkg->version);
        new_watched = alpm_list_add (new_watched, w_pkg2);
    }
    
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        while (1)
        {
            gtk_tree_model_get (model, &iter, WCOL_UPD, &upd, -1);
            if (upd)
            {
                gtk_tree_model_get (model, &iter,
                    WCOL_NAME,        &(w_pkg_tmp.name),
                    WCOL_OLD_VERSION, &(w_pkg_tmp.version),
                    WCOL_NEW_VERSION, &new,
                    -1);
                /* find this package/version */
                w_pkg = alpm_list_find (new_watched, &w_pkg_tmp,
                    (alpm_list_fn_cmp) watched_package_cmp);
                if (NULL != w_pkg)
                {
                    /* update version */
                    free (w_pkg->version);
                    w_pkg->version = strdup (new);
                    /* keeping track of all updates */
                    updates = alpm_list_add (updates, w_pkg);
                }
                else
                {
                    ++nb_watched;
                }
            }
            else
            {
                ++nb_watched;
            }
            
            /* next */
            if (!gtk_tree_model_iter_next (model, &iter))
            {
                break;
            }
        }
    }
    
    /* save to file */
    if (save_watched (is_aur, new_watched))
    {
        /* clear list in emory */
        FREE_WATCHED_PACKAGE_LIST (*cfglist);

        /* apply changes */
        *cfglist = new_watched;

        /* manage window needs an update? */
        if (window_manage != NULL && updates != NULL)
        {
            GtkWidget *dialog;
            dialog = new_confirm ("Do you want to import marked changes into the current list?",
                                  "You have marked one or more packages with updated version number since starting the editing of this list.",
                                  "Yes, import changes.", NULL,
                                  "No, keep the list as is.", NULL,
                                  window_manage);
            g_object_set_data (G_OBJECT (dialog), "is-aur", GINT_TO_POINTER (is_aur));
            g_signal_connect (G_OBJECT (dialog), "response",
                              G_CALLBACK (monitor_response_cb), (gpointer) updates);
            gtk_widget_show (dialog);
        }
        else
        {
            alpm_list_free (updates);
        }
        
        /* done */
        check_t type = (is_aur) ? CHECK_WATCHED_AUR : CHECK_WATCHED;
        /* we go and change the last_notifs. if nb_watched = 0 we can
         * simply remove it, else we change it to ask to run the checks again
         * to be up to date */
        for (i = config->last_notifs; i; i = alpm_list_next (i))
        {
            notif_t *notif = i->data;
            if (notif->type & type)
            {
                if (nb_watched == 0)
                {
                    config->last_notifs = alpm_list_remove_item (config->last_notifs, i);
                    free_notif (notif);
                }
                else
                {
                    FREE_PACKAGE_LIST (notif->data);
                    free (notif->text);
                    notif->text = strdup (
                        (is_aur)
                        ? "Watched AUR packages have changed, "
                            "you need to run the checks again to be up-to-date."
                        : "Watched packages have changed, "
                            "you need to run the checks again to be up-to-date."
                        );
                }
                break;
            }
        }
        set_kalpm_nb (type, nb_watched, TRUE);
        gtk_widget_destroy (window_notif);
    }
    else
    {
        gtk_widget_show (window_notif);
        show_error ("Unable to save changes to disk", NULL, GTK_WINDOW (window_notif));
        /* free duplicate list */
        FREE_WATCHED_PACKAGE_LIST (new_watched);
        alpm_list_free (updates);
    }
}

static void
btn_save_cb (GtkButton *button _UNUSED_, gboolean is_aur)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkWidget *window;
    watched_package_t *w_pkg;
    alpm_list_t **cfglist, *new_watched = NULL;
    
    if (is_aur)
    {
        window = watched.window_manage_aur;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage_aur));
        cfglist = &(config->watched_aur);
    }
    else
    {
        window = watched.window_manage;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage));
        cfglist = &(config->watched);
    }
    
    gtk_widget_hide (window);
    
    /* make up new list */
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        while (1)
        {
            gchar *name = NULL, *version = NULL;
            gtk_tree_model_get (model, &iter, WCOL_NAME, &name,
                WCOL_OLD_VERSION, &version, -1);
            w_pkg = calloc (1, sizeof (*w_pkg));
            w_pkg->name = strdup ((name) ? name : "-");
            w_pkg->version = strdup ((version) ? version : "0");
            new_watched = alpm_list_add (new_watched, w_pkg);
            /* next */
            if (!gtk_tree_model_iter_next (model, &iter))
            {
                break;
            }
        }
    }
    
    /* save it */
    if (save_watched (is_aur, new_watched))
    {
        /* clear watched list in memory */
        FREE_WATCHED_PACKAGE_LIST (*cfglist);
        
        /* apply */
        *cfglist = new_watched;
        
        /* done */
        gtk_widget_destroy (window);
    }
    else
    {
        /* free */
        FREE_WATCHED_PACKAGE_LIST (new_watched);
        
        gtk_widget_show (window);
        show_error ("Unable to save changes to disk", NULL, GTK_WINDOW (window));
    }
}

static void
btn_add_cb (GtkToolButton *tb_item _UNUSED_, gboolean is_aur)
{
    GtkTreeView *tree   = GTK_TREE_VIEW ((is_aur) ? watched.tree_manage_aur
                                                  : watched.tree_manage);
    GtkTreeModel *model = gtk_tree_view_get_model (tree);
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeViewColumn *column;
    
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    path = gtk_tree_model_get_path (model, &iter);
    column = gtk_tree_view_get_column (tree, 0);
    gtk_tree_view_set_cursor (tree, path, column, TRUE);
}

static void
btn_edit_cb (GtkToolButton *tb_item _UNUSED_, gboolean is_aur)
{
    GtkTreeView *tree   = GTK_TREE_VIEW ((is_aur) ? watched.tree_manage_aur
                                                  : watched.tree_manage);
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeViewColumn *column;
    
    selection = gtk_tree_view_get_selection (tree);
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        return;
    }
    path = gtk_tree_model_get_path (model, &iter);
    column = gtk_tree_view_get_column (tree, 0);
    gtk_tree_view_set_cursor (tree, path, column, TRUE);
}

static void
btn_remove_cb (GtkToolButton *tb_item _UNUSED_, gboolean is_aur)
{
    GtkTreeView *tree   = GTK_TREE_VIEW ((is_aur) ? watched.tree_manage_aur
                                                  : watched.tree_manage);
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    selection = gtk_tree_view_get_selection (tree);
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        return;
    }
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
btn_reload_cb (GtkToolButton *tb_item, int from_disk)
{
    gboolean is_aur = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tb_item), "is-aur"));
    if (from_disk)
    {
        GError *error = NULL;
        if (!reload_watched (is_aur, &error))
        {
            show_error ((is_aur) ? "Unable to parse watched AUR packages"
                : "Unable to parse watched packages", error->message, NULL);
            g_clear_error (&error);
        }
    }
    manage_load_watched (is_aur);
}

static void
renderer_edited_cb (GtkCellRendererText *renderer, gchar *path, gchar *text, int col)
{
    gboolean is_aur = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "is-aur"));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (is_aur)
    {
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage_aur));
    }
    else
    {
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage));
    }
    
    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
        char *s;
        gtk_tree_model_get (model, &iter, col, &s, -1);
        free (s);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, text, -1);
    }
}

static void
selection_changed_cb (GtkTreeSelection *selection, GtkToolbar *toolbar)
{
    GtkToolItem *tb_item;
    gboolean has_selection;
    
    has_selection = (gtk_tree_selection_count_selected_rows (selection) > 0);
    tb_item = gtk_toolbar_get_nth_item (toolbar, 1); /* Edit */
    gtk_widget_set_sensitive (GTK_WIDGET (tb_item), has_selection);
    tb_item = gtk_toolbar_get_nth_item (toolbar, 2); /* Remove */
    gtk_widget_set_sensitive (GTK_WIDGET (tb_item), has_selection); 
}

static void
_rend_size (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
            GtkTreeModel *store, GtkTreeIter *iter, int col)
{
    rend_size (column, renderer, store, iter, col, 1);
}

static GtkWidget *
watched_new_window (w_type_t type)
{
    gboolean is_aur    = (type == W_NOTIF_AUR || type == W_MANAGE_AUR);
    gboolean is_update = (type == W_NOTIF || type == W_NOTIF_AUR);
    GtkWidget *button, *image;
    
    /* the window */
    GtkWidget *window;
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    switch (type)
    {
        case W_NOTIF:
            gtk_window_set_title (GTK_WINDOW (window), "Mark updated watched packages - kalu");
            break;
        case W_MANAGE:
            gtk_window_set_title (GTK_WINDOW (window), "Manage watched packages - kalu");
            break;
        case W_NOTIF_AUR:
            gtk_window_set_title (GTK_WINDOW (window), "Mark updated watched AUR packages - kalu");
            break;
        case W_MANAGE_AUR:
            gtk_window_set_title (GTK_WINDOW (window), "Manage watched AUR packages - kalu");
            break;
    }
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
    /* add to list of open windows */
    add_open_window (window);
    /* icon */
    GdkPixbuf *pixbuf;
    pixbuf = gtk_widget_render_icon_pixbuf (window, "kalu-logo", GTK_ICON_SIZE_DIALOG);
    gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
    g_object_unref (pixbuf);
    
    /* ensure minimum size */
    gint w, h;
    gtk_window_get_size (GTK_WINDOW (window), &w, &h);
    w = MAX(420, w);
    h = MAX(230, h);
    gtk_window_set_default_size (GTK_WINDOW (window), w, h);

    /* everything in a vbox */
    GtkWidget *vbox;
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);
    
    GtkWidget *toolbar;
    if (!is_update)
    {
        GtkToolItem *tb_item;
        /* toolbar */
        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
        gtk_widget_show (toolbar);
        
        /* button: Add */
        tb_item = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_widget_set_tooltip_text (GTK_WIDGET (tb_item), "Add a new package");
        g_signal_connect (G_OBJECT (tb_item), "clicked",
                          G_CALLBACK (btn_add_cb), GINT_TO_POINTER (is_aur));
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tb_item, -1);
        gtk_widget_show (GTK_WIDGET (tb_item));
        /* button: Edit */
        tb_item = gtk_tool_button_new_from_stock (GTK_STOCK_EDIT);
        gtk_widget_set_tooltip_text (GTK_WIDGET (tb_item), "Edit selected package");
        gtk_widget_set_sensitive (GTK_WIDGET (tb_item), FALSE);
        g_signal_connect (G_OBJECT (tb_item), "clicked",
                          G_CALLBACK (btn_edit_cb), GINT_TO_POINTER (is_aur));
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tb_item, -1);
        gtk_widget_show (GTK_WIDGET (tb_item));
        /* button: Remove */
        tb_item = gtk_tool_button_new_from_stock (GTK_STOCK_REMOVE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (tb_item), "Remove selected package");
        gtk_widget_set_sensitive (GTK_WIDGET (tb_item), FALSE);
        g_signal_connect (G_OBJECT (tb_item), "clicked",
                          G_CALLBACK (btn_remove_cb), GINT_TO_POINTER (is_aur));
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tb_item, -1);
        gtk_widget_show (GTK_WIDGET (tb_item));
        /* --- */
        tb_item = gtk_separator_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tb_item, -1);
        gtk_widget_show (GTK_WIDGET (tb_item));
        /* button: Reload from memory */
        tb_item = gtk_tool_button_new_from_stock (GTK_STOCK_UNDO);
        g_object_set_data (G_OBJECT (tb_item), "is-aur", GINT_TO_POINTER (is_aur));
        gtk_widget_set_tooltip_text (GTK_WIDGET (tb_item), "Reload list from memory (undo changes)");
        g_signal_connect (G_OBJECT (tb_item), "clicked",
                          G_CALLBACK (btn_reload_cb), (gpointer) 0);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tb_item, -1);
        gtk_widget_show (GTK_WIDGET (tb_item));
        /* button: Reload from disk */
        tb_item = gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH);
        g_object_set_data (G_OBJECT (tb_item), "is-aur", GINT_TO_POINTER (is_aur));
        gtk_widget_set_tooltip_text (GTK_WIDGET (tb_item), "Reload list from file");
        g_signal_connect (G_OBJECT (tb_item), "clicked",
                          G_CALLBACK (btn_reload_cb), (gpointer) 1);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tb_item, -1);
        gtk_widget_show (GTK_WIDGET (tb_item));
    }
    
    /* store for the list */
    GtkListStore *store;
    store = gtk_list_store_new (WCOL_NB,
                G_TYPE_BOOLEAN,     /* upd (mark watched) */
                G_TYPE_STRING,      /* pkg */
                G_TYPE_STRING,      /* old version */
                G_TYPE_STRING,      /* new version */
                G_TYPE_UINT,        /* dl size */
                G_TYPE_UINT         /* inst size */
                );
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                          WCOL_NAME,
                                          GTK_SORT_ASCENDING);
    
    /* said list */
    GtkWidget *list;
    list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    /* hint for alternate row colors */
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list), TRUE);
    if (!is_update)
    {
        /* selection */
        GtkTreeSelection *selection;
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
        g_signal_connect (selection, "changed",
                          G_CALLBACK (selection_changed_cb), (gpointer) toolbar);
    }
    
    /* cell renderer & column(s) */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    if (is_update)
    {
        /* column: Update */
        renderer = gtk_cell_renderer_toggle_new ();
        column = gtk_tree_view_column_new_with_attributes ("Update",
                                                           renderer,
                                                           "active", WCOL_UPD,
                                                           NULL);
        g_object_set (renderer, "activatable", TRUE, NULL);
        g_signal_connect (G_OBJECT (renderer), "toggled",
                          G_CALLBACK (renderer_toggle_cb), GINT_TO_POINTER (is_aur));
        gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    }
    /* column: Package */
    renderer = gtk_cell_renderer_text_new ();
    if (!is_update)
    {
        g_object_set_data (G_OBJECT (renderer), "is-aur", GINT_TO_POINTER (is_aur));
        g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
        g_signal_connect (G_OBJECT (renderer), "edited",
                          G_CALLBACK (renderer_edited_cb), (gpointer) WCOL_NAME);
    }
    column = gtk_tree_view_column_new_with_attributes ("Package",
                                                       renderer,
                                                       "text", WCOL_NAME,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Old version */
    if (!is_update)
    {
        /* we need another renderer so we know which column gets edited */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set_data (G_OBJECT (renderer), "is-aur", GINT_TO_POINTER (is_aur));
        g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
        g_signal_connect (G_OBJECT (renderer), "edited",
                          G_CALLBACK (renderer_edited_cb), (gpointer) WCOL_OLD_VERSION);
    }
    column = gtk_tree_view_column_new_with_attributes ("Version",
                                                       renderer,
                                                       "text", WCOL_OLD_VERSION,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    if (is_update)
    {
        /* column: New version */
        column = gtk_tree_view_column_new_with_attributes ("New",
                                                           renderer,
                                                           "text", WCOL_NEW_VERSION,
                                                           NULL);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
        if (!is_aur)
        {
            /* column: Download size */
            column = gtk_tree_view_column_new_with_attributes ("Download",
                                                               renderer,
                                                               NULL);
            gtk_tree_view_column_set_cell_data_func (column, renderer,
                (GtkTreeCellDataFunc) _rend_size, (gpointer) WCOL_DL_SIZE, NULL);
            gtk_tree_view_column_set_resizable (column, TRUE);
            gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
            /* column: Installed size */
            column = gtk_tree_view_column_new_with_attributes ("Installed",
                                                               renderer,
                                                               NULL);
            gtk_tree_view_column_set_cell_data_func (column, renderer,
                (GtkTreeCellDataFunc) _rend_size, (gpointer) WCOL_INS_SIZE, NULL);
            gtk_tree_view_column_set_resizable (column, TRUE);
            gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
        }
    }
    
    gtk_box_pack_start (GTK_BOX (vbox), list, TRUE, TRUE, 0);
    gtk_widget_show (list);
    
    /* button box */
    GtkWidget *hbox;
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);
    
    if (is_update)
    {
        /* Apply */
        button = gtk_button_new_with_label ("Mark as seen");
        image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
        gtk_widget_set_tooltip_text (button, "Save new version numbers of checked packages");
        g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (btn_mark_cb), GINT_TO_POINTER (is_aur));
        gtk_widget_show (button);
    }
    else
    {
        /* Save */
        button = gtk_button_new_with_label ("Save list");
        image = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
        gtk_widget_set_tooltip_text (button, "Save list of watched packages");
        g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (btn_save_cb), GINT_TO_POINTER (is_aur));
        gtk_widget_show (button);
    }
    /* Close */
    button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (btn_close_cb), (gpointer) type);
    gtk_widget_show (button);
    
    /* signals */
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (window_destroy_cb), (gpointer) type);
    
    switch (type)
    {
        case W_NOTIF:
            watched.window_notif = window;
            watched.tree_notif = list;
            break;
        case W_MANAGE:
            watched.window_manage = window;
            watched.tree_manage = list;
            break;
        case W_NOTIF_AUR:
            watched.window_notif_aur = window;
            watched.tree_notif_aur = list;
            break;
        case W_MANAGE_AUR:
            watched.window_manage_aur = window;
            watched.tree_manage_aur = list;
            break;
    }
    return window;
}

void
watched_update (alpm_list_t *packages, gboolean is_aur)
{
    GtkWidget *window, *tree;
    w_type_t type;
    
    if (is_aur)
    {
        type = W_NOTIF_AUR;
        window = watched.window_notif_aur;
    }
    else
    {
        type = W_NOTIF;
        window = watched.window_notif;
    }
    
    if (NULL != window)
    {
        gtk_window_present (GTK_WINDOW (window));
        return;
    }
    
    window = watched_new_window (type);
    tree = (is_aur) ? watched.tree_notif_aur : watched.tree_notif;
    
    /* fill it up */
    GtkListStore *store;
    GtkTreeIter iter;
    alpm_list_t *i;
    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (tree)));
    for (i = packages; i; i = alpm_list_next (i))
    {
        kalu_package_t *pkg = i->data;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
            WCOL_UPD,           TRUE,
            WCOL_NAME,          strdup (pkg->name),
            WCOL_OLD_VERSION,   strdup (pkg->old_version),
            WCOL_NEW_VERSION,   strdup (pkg->new_version),
            WCOL_DL_SIZE,       pkg->dl_size,
            WCOL_INS_SIZE,      pkg->new_size,
            -1);
    }
    
    /* show */
    gtk_widget_show (window);
}

static void
manage_load_watched (gboolean is_aur)
{
    GtkWidget *window;
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *name, *old;
    alpm_list_t *cfglist;
    
    if (is_aur)
    {
        window = watched.window_manage_aur;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage_aur));
        cfglist = config->watched_aur;
    }
    else
    {
        window = watched.window_manage;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (watched.tree_manage));
        cfglist = config->watched;
    }
    
    if (NULL == window)
    {
        return;
    }
    
    /* clear list */
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        while (1)
        {
            gtk_tree_model_get (model, &iter, WCOL_NAME, &name,
                WCOL_OLD_VERSION, &old, -1);
            free (name);
            free (old);
            
            /* next */
            if (!gtk_tree_model_iter_next (model, &iter))
            {
                break;
            }
        }
    }
    GtkListStore *store = GTK_LIST_STORE (model);
    gtk_list_store_clear (store);
    
    /* fill it up */
    alpm_list_t *i;
    for (i = cfglist; i; i = alpm_list_next (i))
    {
        watched_package_t *w_pkg = i->data;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
            WCOL_NAME,          strdup (w_pkg->name),
            WCOL_OLD_VERSION,   strdup (w_pkg->version),
            -1);
    }
}

void
watched_manage (gboolean is_aur)
{
    GtkWidget *window;
    w_type_t type;
    
    if (is_aur)
    {
        type = W_MANAGE_AUR;
        window = watched.window_manage_aur;
    }
    else
    {
        type = W_MANAGE;
        window = watched.window_manage;
    }
    
    if (NULL != window)
    {
        gtk_window_present (GTK_WINDOW (window));
        return;
    }
    
    window = watched_new_window (type);
    manage_load_watched (is_aur);
    
    /* show */
    gtk_widget_show (window);
}
