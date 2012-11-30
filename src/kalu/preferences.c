/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * preferences.c
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

/* kalu */
#include "kalu.h"
#include "preferences.h"
#include "gui.h"
#include "util.h"
#include "watched.h"
#include "util-gtk.h"

static gboolean
focus_out_cb (GtkWidget *entry, GdkEvent *event _UNUSED_, gpointer data _UNUSED_);

static GtkWidget *window                    = NULL;
static GtkWidget *notebook                  = NULL;
/* General */
static GtkWidget *filechooser               = NULL;
static GtkWidget *notif_icon_combo          = NULL;
static GtkWidget *notif_icon_filechooser    = NULL;
static GtkWidget *combo_interval            = NULL;
static GtkWidget *timeout_scale             = NULL;
static GtkWidget *button_skip               = NULL;
static GtkWidget *spin_begin_hour           = NULL;
static GtkWidget *spin_begin_minute         = NULL;
static GtkWidget *spin_end_hour             = NULL;
static GtkWidget *spin_end_minute           = NULL;
static GtkWidget *auto_upgrades             = NULL;
static GtkWidget *auto_watched              = NULL;
static GtkWidget *auto_aur                  = NULL;
static GtkWidget *auto_watched_aur          = NULL;
static GtkWidget *auto_news                 = NULL;
static GtkWidget *manual_upgrades           = NULL;
static GtkWidget *manual_watched            = NULL;
static GtkWidget *manual_aur                = NULL;
static GtkWidget *manual_watched_aur        = NULL;
static GtkWidget *manual_news               = NULL;
/* News */
static GtkWidget *cmdline_link_entry        = NULL;
static GtkWidget *news_title_entry          = NULL;
static GtkWidget *news_package_entry        = NULL;
static GtkWidget *news_sep_entry            = NULL;
/* Upgrades */
static GtkWidget *check_pacman_conflict     = NULL;
static GtkWidget *button_upg_action         = NULL;
#ifndef DISABLE_UPDATER
static GtkWidget *upg_action_combo          = NULL;
#endif
static GtkWidget *cmdline_label             = NULL;
static GtkWidget *cmdline_entry             = NULL;
#ifndef DISABLE_UPDATER
static GtkWidget *cmdline_post_hbox         = NULL;
static GtkListStore *cmdline_post_store     = NULL;
static GtkWidget *confirm_post              = NULL;
#endif
static GtkWidget *upg_title_entry           = NULL;
static GtkWidget *upg_package_entry         = NULL;
static GtkWidget *upg_sep_entry             = NULL;
/* Watched */
static GtkWidget *watched_title_entry       = NULL;
static GtkWidget *watched_package_entry     = NULL;
static GtkWidget *watched_sep_entry         = NULL;
/* AUR */
static GtkWidget *aur_cmdline_entry         = NULL;
static GtkListStore *aur_ignore_store       = NULL;
static GtkWidget *aur_title_entry           = NULL;
static GtkWidget *aur_package_entry         = NULL;
static GtkWidget *aur_sep_entry             = NULL;
/* Watched */
static GtkWidget *watched_aur_title_entry   = NULL;
static GtkWidget *watched_aur_package_entry = NULL;
static GtkWidget *watched_aur_sep_entry     = NULL;
/* Misc */
static GtkWidget *sane_sort_order           = NULL;
static GtkWidget *syncdbs_in_tooltip        = NULL;
static GtkWidget *on_sgl_click              = NULL;
static GtkWidget *on_dbl_click              = NULL;
static GtkWidget *on_sgl_click_paused       = NULL;
static GtkWidget *on_dbl_click_paused       = NULL;

/* we keep a copy of templates like so, so that we can use it when refreshing
 * the different templates. that is, values shown when a template is not set
 * and therefore fallsback to another one.
 * And because we only ever fallback on aur (for watched-aur) and upgrades, we
 * only need to keep those two. */
static struct _fallback_templates {
    templates_t *tpl_upgrades;
    templates_t *tpl_aur;
} fallback_templates;


static char *
escape_text (const char *text)
{
    char *escaped, *s;
    s = strreplace (text, "\n", "\\n");
    escaped = strreplace (s, "\t", "\\t");
    free (s);
    return escaped;
}

static void
destroy_cb (GtkWidget *widget _UNUSED_)
{
    free (fallback_templates.tpl_upgrades->title);
    free (fallback_templates.tpl_upgrades->package);
    free (fallback_templates.tpl_upgrades->sep);
    free (fallback_templates.tpl_upgrades);
    free (fallback_templates.tpl_aur->title);
    free (fallback_templates.tpl_aur->package);
    free (fallback_templates.tpl_aur->sep);
    free (fallback_templates.tpl_aur);

    /* remove from list of open windows */
    remove_open_window (window);

    window = NULL;
}

static void
skip_toggled_cb (GtkToggleButton *button, gpointer data _UNUSED_)
{
    gboolean is_active = gtk_toggle_button_get_active (button);
    gtk_widget_set_sensitive (spin_begin_hour, is_active);
    gtk_widget_set_sensitive (spin_begin_minute, is_active);
    gtk_widget_set_sensitive (spin_end_hour, is_active);
    gtk_widget_set_sensitive (spin_end_minute, is_active);
}

static void
upg_action_toggled_cb (GtkToggleButton *button, gpointer data _UNUSED_)
{
    gboolean is_active = gtk_toggle_button_get_active (button);
#ifndef DISABLE_UPDATER
    gtk_widget_set_sensitive (upg_action_combo, is_active);
#endif
    gtk_widget_set_sensitive (cmdline_entry, is_active);
#ifndef DISABLE_UPDATER
    gtk_widget_set_sensitive (cmdline_post_hbox, is_active);
    gtk_widget_set_sensitive (confirm_post, is_active);
#endif
}

#ifndef DISABLE_UPDATER
static void
upg_action_changed_cb (GtkComboBox *combo, gpointer data _UNUSED_)
{
    int choice = gtk_combo_box_get_active (combo);
    if (choice == 0)
    {
        gtk_widget_hide (cmdline_label);
        gtk_widget_hide (cmdline_entry);
        gtk_widget_show (cmdline_post_hbox);
        gtk_widget_show (confirm_post);
    }
    else if (choice == 1)
    {
        gtk_widget_show (cmdline_label);
        gtk_widget_show (cmdline_entry);
        gtk_widget_hide (cmdline_post_hbox);
        gtk_widget_hide (confirm_post);
    }
}
#endif

static void
notif_icon_combo_changed_cb (GtkComboBox *combo, gpointer data _UNUSED_)
{
    int choice = gtk_combo_box_get_active (combo);
    if (choice == 2)
    {
        gtk_widget_show (notif_icon_filechooser);
    }
    else if (choice == 1)
    {
        gtk_widget_hide (notif_icon_filechooser);
    }
}

static void
aur_action_toggled_cb (GtkToggleButton *button, gpointer data _UNUSED_)
{
    gtk_widget_set_sensitive (aur_cmdline_entry,
            gtk_toggle_button_get_active (button));
}

static void
insert_text_cb (GtkEditable *editable,
                const gchar *text,
                gint         length,
                gint        *position,
                GtkComboBox *combo)
{
    gchar *c = (gchar *) text;
    gint l = length;
    gchar *s;

    /* check if it's an item from the list, if which case we allow */
    GtkTreeModel *model = gtk_combo_box_get_model (combo);
    GtkTreeIter iter;
    gtk_tree_model_get_iter_first (model, &iter);
    do
    {
        gtk_tree_model_get (model, &iter, 0, &s, -1);
        if (strncmp (s, text, (size_t) length) == 0)
        {
            return;
        }
    } while (gtk_tree_model_iter_next (model, &iter));


    /* make sure it's digit only */
    for (; l; --l, ++c)
    {
        if (!(*c >= '0' && *c <= '9'))
        {
            break;
        }
    }

    /* if so, add it */
    if (l == 0)
    {
        g_signal_handlers_block_by_func (editable,
                (gpointer) insert_text_cb,
                combo);
        gtk_editable_insert_text (editable, text, length, position);
        g_signal_handlers_unblock_by_func (editable,
                (gpointer) insert_text_cb,
                combo);
    }

    /* this we added it already (if valid) stop the original addition */
    g_signal_stop_emission_by_name (editable, "insert_text");
}

static void
selection_changed_cb (GtkTreeSelection *selection, GtkWidget *tree)
{
    gboolean has_selection;
    GtkWidget *btn_edit = g_object_get_data (G_OBJECT (tree), "btn-edit");
    GtkWidget *btn_remove = g_object_get_data (G_OBJECT (tree), "btn-remove");

    has_selection = (gtk_tree_selection_count_selected_rows (selection) > 0);
    gtk_widget_set_sensitive (btn_edit, has_selection);
    gtk_widget_set_sensitive (btn_remove, has_selection); 
}

static void
btn_add_cb (GtkButton *button _UNUSED_, GtkTreeView *tree)
{
    GtkListStore        *store;
    GtkTreeIter          iter;
    GtkTreePath         *path;
    GtkTreeViewColumn   *column;

    store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (tree), "store"));

    gtk_list_store_append (store, &iter);
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
    column = gtk_tree_view_get_column (tree, 0);
    gtk_tree_view_set_cursor (tree, path, column, TRUE);
}

static void
btn_edit_cb (GtkButton *button _UNUSED_, GtkTreeView *tree)
{
    GtkTreeSelection    *selection;
    GtkTreeModel        *model;
    GtkTreeIter          iter;
    GtkTreePath         *path;
    GtkTreeViewColumn   *column;

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
btn_remove_cb (GtkButton *button _UNUSED_, GtkTreeView *tree)
{
    GtkTreeSelection    *selection;
    GtkTreeModel        *model;
    GtkTreeIter          iter;

    selection = gtk_tree_view_get_selection (tree);
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        return;
    }
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
renderer_edited_cb (GtkCellRendererText *renderer, gchar *path,
                    gchar *text, gpointer data _UNUSED_)
{
    GtkListStore    *store;
    GtkTreeModel    *model;
    GtkTreeIter      iter;

    store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (renderer), "store"));
    model = GTK_TREE_MODEL (store);

    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
        char *s;
        gtk_tree_model_get (model, &iter, 0, &s, -1);
        free (s);
        gtk_list_store_set (store, &iter, 0, text, -1);
    }
}

static gchar *
timeout_format_value (GtkScale *scale _UNUSED_, gdouble val, gpointer data _UNUSED_)
{
    int value = (int) val;
    if (value == 0)
    {
        return g_strdup ("Default");
    }
    else if (value == 40)
    {
        return g_strdup ("Never");
    }
    else
    {
        return g_strdup_printf ("%d seconds", 3 + value);
    }
}

#define refresh_entry_text(entry, tpl, tpl_item)     do {   \
    if (!gtk_widget_get_sensitive (entry))                  \
    {                                                       \
        s = escape_text (fallback_templates.tpl->tpl_item); \
        gtk_entry_set_text (GTK_ENTRY (entry), s);          \
        free (s);                                           \
    }                                                       \
} while (0)
#define refresh_entry_text_watched_aur(entry, tpl_item) do {    \
    if (!gtk_widget_get_sensitive (entry))                      \
    {                                                           \
        s = NULL;                                               \
        if (NULL != fallback_templates.tpl_aur->tpl_item)       \
        {                                                       \
            s = fallback_templates.tpl_aur->tpl_item;           \
        }                                                       \
        if (NULL == s)                                          \
        {                                                       \
            s = fallback_templates.tpl_upgrades->tpl_item;      \
        }                                                       \
        s = escape_text (s);                                    \
        gtk_entry_set_text (GTK_ENTRY (entry), s);              \
        free (s);                                               \
    }                                                           \
} while (0)
static void
refresh_tpl_widgets (void)
{
    char *s;

    refresh_entry_text (news_title_entry, tpl_upgrades, title);
    refresh_entry_text (news_package_entry, tpl_upgrades, package);
    refresh_entry_text (news_sep_entry, tpl_upgrades, sep);

    refresh_entry_text (watched_title_entry, tpl_upgrades, title);
    refresh_entry_text (watched_package_entry, tpl_upgrades, package);
    refresh_entry_text (watched_sep_entry, tpl_upgrades, sep);

    refresh_entry_text (aur_title_entry, tpl_upgrades, title);
    refresh_entry_text (aur_package_entry, tpl_upgrades, package);
    refresh_entry_text (aur_sep_entry, tpl_upgrades, sep);

    /* watched-aur: falls back to aur first, then upgrades */
    refresh_entry_text_watched_aur (watched_aur_title_entry, title);
    refresh_entry_text_watched_aur (watched_aur_package_entry, package);
    refresh_entry_text_watched_aur (watched_aur_sep_entry, sep);
}
#undef refresh_entry_text_watched_aur
#undef refresh_entry_text

static void
tpl_toggled_cb (GtkToggleButton *button, GtkWidget *entry)
{
    gboolean is_active = gtk_toggle_button_get_active (button);
    char **tpl_item = g_object_get_data (G_OBJECT (entry), "tpl-item");
    char *old, **fallback, *s;

    /* switch entry's sensitive state */
    gtk_widget_set_sensitive (entry, is_active);

    if (is_active)
    {
        /* try to restore old-value if there's one */
        old = g_object_get_data (G_OBJECT (entry), "old-value");
        if (NULL != old)
        {
            s = old;
        }
        else
        {
            s = (char *) "";
        }
        gtk_entry_set_text (GTK_ENTRY (entry), s);

        /* if this entry has a tpl-item we need to update it as well */
        if (NULL != tpl_item)
        {
            free (*tpl_item);
            *tpl_item = strdup (s);
        }
    }
    else
    {
        /* store old value */
        old = (char *) gtk_entry_get_text (GTK_ENTRY (entry));
        g_object_set_data_full (G_OBJECT (entry), "old-value",
                strdup (old), (GDestroyNotify) free);
        /* now, replace the text with whatever we fall back on.
         * watched-aur falls back to aur first, then, as others, to upgrades */
        check_t type = (check_t) g_object_get_data (G_OBJECT (entry), "type");
        fallback = NULL;
        if (type & CHECK_WATCHED_AUR)
        {
            fallback = g_object_get_data (G_OBJECT (entry), "fallback-aur");
        }
        if (NULL == fallback || NULL == *fallback)
        {
            fallback = g_object_get_data (G_OBJECT (entry), "fallback");
        }
        s = escape_text (*fallback);
        gtk_entry_set_text (GTK_ENTRY (entry), s);
        free (s);

        /* if this entry has a tpl-item we need to reset it as well */
        if (NULL != tpl_item)
        {
            free (*tpl_item);
            *tpl_item = NULL;
        }
    }
    /* if this entry has a tpl-item, we need te refresh every unsensitive
     * widget (which might be falling back to it) */
    if (NULL != tpl_item)
    {
        refresh_tpl_widgets ();
    }
}

static gboolean
focus_out_cb (GtkWidget *entry, GdkEvent *event _UNUSED_, gpointer data _UNUSED_)
{
    char **tpl_item = (char **) g_object_get_data (G_OBJECT (entry), "tpl-item");
    if (NULL == tpl_item)
    {
        return FALSE;
    }

    /* update fallback_templates item value */
    free (*tpl_item);
    *tpl_item = strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

    /* now update every unsensitive widget, so they're up-to-date when user
     * switches pages */
    refresh_tpl_widgets ();

    /* keep processing */
    return FALSE;
}

#define add_label(text, tpl_item)    do {                           \
    if (type & CHECK_UPGRADES)                                      \
    {                                                               \
        label = gtk_label_new (text);                               \
    }                                                               \
    else                                                            \
    {                                                               \
        label = gtk_check_button_new_with_label (text);             \
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label),    \
                NULL != template->tpl_item);                        \
    }                                                               \
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);         \
    gtk_widget_show (label);                                        \
} while (0)
#define set_entry(entry, tpl_item)   do {                               \
    g_object_set_data (G_OBJECT (entry), "type",                        \
            (gpointer) type);                                           \
    g_object_set_data (G_OBJECT (entry), "fallback",                    \
            (gpointer) &(fallback_templates.tpl_upgrades->tpl_item));   \
    if (type & CHECK_WATCHED_AUR)                                       \
    {                                                                   \
        g_object_set_data (G_OBJECT (entry), "fallback-aur",            \
                (gpointer) &(fallback_templates.tpl_aur->tpl_item));    \
    }                                                                   \
    if (type & (CHECK_UPGRADES | CHECK_AUR))                            \
    {                                                                   \
        gpointer ptr;                                                   \
        if (type & CHECK_UPGRADES)                                      \
        {                                                               \
            ptr = &(fallback_templates.tpl_upgrades->tpl_item);         \
        }                                                               \
        else                                                            \
        {                                                               \
            ptr = &(fallback_templates.tpl_aur->tpl_item);              \
        }                                                               \
        g_object_set_data (G_OBJECT (entry), "tpl-item", ptr);          \
        gtk_widget_add_events (entry, GDK_FOCUS_CHANGE_MASK);           \
        g_signal_connect (G_OBJECT (entry), "focus-out-event",          \
                G_CALLBACK (focus_out_cb), NULL);                       \
    }                                                                   \
    if (NULL != template->tpl_item)                                     \
    {                                                                   \
        s = template->tpl_item;                                         \
    }                                                                   \
    else                                                                \
    {                                                                   \
        gtk_widget_set_sensitive (entry, FALSE);                        \
        s = NULL;                                                       \
        if (type & CHECK_WATCHED_AUR                                    \
                && NULL != config->tpl_aur->tpl_item)                   \
        {                                                               \
            s = config->tpl_aur->tpl_item;                              \
        }                                                               \
        if (s == NULL)                                                  \
        {                                                               \
            s = config->tpl_upgrades->tpl_item;                         \
        }                                                               \
    }                                                                   \
    s = escape_text (s);                                                \
    gtk_entry_set_text (GTK_ENTRY (entry), s);                          \
    free (s);                                                           \
} while (0)
#define connect_signal(entry)   do {                    \
    if (!(type & CHECK_UPGRADES))                       \
    {                                                   \
        g_signal_connect (G_OBJECT (label), "toggled",  \
                G_CALLBACK (tpl_toggled_cb),            \
                (gpointer) entry);                      \
    }                                                   \
} while (0)
#define add_to_grid(entry)   do {                           \
    gtk_grid_attach (GTK_GRID (grid), entry, 1, top, 3, 1); \
    gtk_widget_show (entry);                                \
} while (0)
static void
add_template (GtkWidget    *grid,
              int           top,
              GtkWidget   **title_entry,
              GtkWidget   **package_entry,
              GtkWidget   **sep_entry,
              templates_t  *template,
              check_t       type)
{
    char *s, *tooltip;
    GtkWidget *label;

    /* notification template */
    label = gtk_label_new (NULL);
    gtk_widget_set_size_request (label, 420, -1);
    gtk_label_set_markup (GTK_LABEL (label), "<b>Notification template</b>");
    gtk_widget_set_margin_top (label, 15);
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 4, 1);
    gtk_widget_show (label);

    /* Title */
    ++top;
    add_label ("Title :", title);

    *title_entry = gtk_entry_new ();
    /* set tooltip */
    tooltip = g_strconcat (
            "The following variables are available :"
            "\n- <b>$NB</b> : number of packages/news items",
            (type & (CHECK_UPGRADES | CHECK_WATCHED))
            ? "\n- <b>$DL</b> : total download size"
              "\n- <b>$INS</b> : total installed size"
              "\n- <b>$NET</b> : total net (post-install difference) size"
            : NULL,
            NULL);
    gtk_widget_set_tooltip_markup (*title_entry, tooltip);
    g_free (tooltip);
    /* set value if any, else unsensitive */
    set_entry (*title_entry, title);
    /* if it's a check-button, connect to toggled (now that we have the related entry) */
    connect_signal (*title_entry);
    /* add to grid */
    add_to_grid (*title_entry);


    /* Package */
    ++top;
    add_label ((type & CHECK_NEWS) ? "News item :" : "Package :", package);

    *package_entry = gtk_entry_new ();
    /* set tooltip */
    tooltip = g_strconcat (
            "The following variables are available :",
            (type & CHECK_NEWS)
            ? "\n- <b>$NEWS</b> : the news title"
            : "\n- <b>$PKG</b> : package name"
              "\n- <b>$OLD</b> : old/current version number"
              "\n- <b>$NEW</b> : new version number",
            (type & (CHECK_UPGRADES | CHECK_WATCHED))
            ? "\n- <b>$DL</b> : download size"
              "\n- <b>$INS</b> : installed size"
              "\n- <b>$NET</b> : net (post-install difference) size"
            : NULL,
            NULL);
    gtk_widget_set_tooltip_markup (*package_entry, tooltip);
    g_free (tooltip);
    /* set value if any, else unsensitive */
    set_entry (*package_entry, package);
    /* if it's a check-button, connect to toggled (now that we have the related
     * entry) */
    connect_signal (*package_entry);
    /* add to grid */
    add_to_grid (*package_entry);


    /* Separator */
    ++top;
    add_label ("Separator :", sep);

    *sep_entry = gtk_entry_new ();
    /* set tooltip */
    gtk_widget_set_tooltip_text (*sep_entry, "No variables available.");
    /* set value if any, else unsensitive */
    set_entry (*sep_entry, sep);
    /* if it's a check-button, connect to toggled (now that we have the related
     * entry) */
    connect_signal (*sep_entry);
    /* add to grid */
    add_to_grid (*sep_entry);
}
#undef add_to_grid
#undef connect_signal
#undef set_entry
#undef add_label

static void
btn_manage_watched_cb (GtkButton *button _UNUSED_, gboolean is_aur)
{
    watched_manage (is_aur);
}

#define add_to_conf(...)    do {                                        \
    need = snprintf (tmp, 1024, __VA_ARGS__);                           \
    if (len + need >= alloc)                                            \
    {                                                                   \
        alloc += need + 1024;                                           \
        conf = renew (gchar, alloc + 1, conf);                          \
    }                                                                   \
    if (need < 1024)                                                    \
    {                                                                   \
        strcat (conf, tmp);                                             \
    }                                                                   \
    else                                                                \
    {                                                                   \
        sprintf (conf + len, __VA_ARGS__);                              \
    }                                                                   \
    len += need;                                                        \
} while (0)
#define error_on_page(page, errmsg)    do {                             \
    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);      \
    show_error ("Unable to apply/save preferences", errmsg,             \
            GTK_WINDOW (window));                                       \
    goto clean_on_error;                                                \
} while (0)
#define tpl_field(tpl_name, tpl_key, name, key, entry, field, page) do {    \
    if (gtk_widget_get_sensitive (entry))                                   \
    {                                                                       \
        s = (char *) gtk_entry_get_text (GTK_ENTRY (entry));                \
        if (*s == '\0')                                                     \
        {                                                                   \
            error_on_page (page, "Template missing field " field ".");      \
        }                                                                   \
        if (!has_tpl)                                                       \
        {                                                                   \
            add_to_conf ("[template-" name "]\n");                          \
        }                                                                   \
        add_to_conf (key " = \"%s\"\n", s);                                 \
        new_config.tpl_name->tpl_key = strreplace (s, "\\n", "\n");         \
        has_tpl = TRUE;                                                     \
    }                                                                       \
} while (0)
#define free_tpl(tpl)  do {     \
    if (tpl->title)             \
    {                           \
        free (tpl->title);      \
    }                           \
    if (tpl->package)           \
    {                           \
        free (tpl->package);    \
    }                           \
    if (tpl->sep)               \
    {                           \
        free (tpl->sep);        \
    }                           \
    free (tpl);                 \
} while (0)
#define do_click(name, confname, is_paused)    do {                         \
    s = (gchar *) gtk_combo_box_get_active_id (GTK_COMBO_BOX (name));       \
    if (!s)                                                                 \
    {                                                                       \
        s = (gchar *) "NOTHING";                                            \
    }                                                                       \
    add_to_conf ("%s = %s\n", confname, s);                                 \
    if (strcmp (s, "SYSUPGRADE") == 0)                                      \
    {                                                                       \
        new_config.name = DO_SYSUPGRADE;                                    \
    }                                                                       \
    else if (strcmp (s, "CHECK") == 0)                                      \
    {                                                                       \
        new_config.name = DO_CHECK;                                         \
    }                                                                       \
    else if (strcmp (s, "TOGGLE_WINDOWS") == 0)                             \
    {                                                                       \
        new_config.name = DO_TOGGLE_WINDOWS;                                \
    }                                                                       \
    else if (strcmp (s, "LAST_NOTIFS") == 0)                                \
    {                                                                       \
        new_config.name = DO_LAST_NOTIFS;                                   \
    }                                                                       \
    else if (strcmp (s, "TOGGLE_PAUSE") == 0)                               \
    {                                                                       \
        new_config.name = DO_TOGGLE_PAUSE;                                  \
    }                                                                       \
    else if (is_paused && strcmp (s, "SAME_AS_ACTIVE") == 0)                \
    {                                                                       \
        new_config.name = DO_SAME_AS_ACTIVE;                                \
    }                                                                       \
    else /* if (strcmp (s, "NOTHING") == 0) */                              \
    {                                                                       \
        new_config.name = DO_NOTHING;                                       \
    }                                                                       \
} while (0)
static void
btn_save_cb (GtkButton *button _UNUSED_, gpointer data _UNUSED_)
{
    gchar *conf, tmp[1024];
    int alloc = 1024, len = 0, need;
    gchar *s;
    gint nb, begin_hour, begin_minute, end_hour, end_minute;
    check_t type;
    gboolean has_tpl;
    GtkTreeModel *model;
    GtkTreeIter iter;
    config_t new_config;

    /* init the new kalu.conf we'll be writing */
    conf = new0 (gchar, alloc + 1);
    add_to_conf ("[options]\n");
    /* also init the new config_t: copy the actual one */
    memcpy (&new_config, config, sizeof (config_t));
    /* and set to NULL all that matters (strings/lists/templates we'll re-set) */
    new_config.pacmanconf       = NULL;
    new_config.notif_icon_user  = NULL;
    new_config.cmdline          = NULL;
    new_config.cmdline_aur      = NULL;
    new_config.cmdline_link     = NULL;
#ifndef DISABLE_UPDATER
    new_config.cmdline_post     = NULL;
#endif
    new_config.tpl_upgrades     = new0 (templates_t, 1);
    new_config.tpl_watched      = new0 (templates_t, 1);
    new_config.tpl_aur          = new0 (templates_t, 1);
    new_config.tpl_watched_aur  = new0 (templates_t, 1);
    new_config.tpl_news         = new0 (templates_t, 1);
    new_config.aur_ignore       = NULL;

    /* re-use the UseIP value (cannot be set via GUI) */
    if (new_config.use_ip == IPv4)
    {
        add_to_conf ("UseIP = 4\n");
    }
    else if (new_config.use_ip == IPv6)
    {
        add_to_conf ("UseIP = 6\n");
    }

    /* disabling showing notifs for auto-checks (no GUI) */
    if (!new_config.auto_notifs)
    {
        add_to_conf ("AutoNotifs = 0\n");
    }

    /* General */
    s = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
    if (NULL == s)
    {
        error_on_page (0,
                "You need to select the configuration file to use for libalpm (pacman.conf)");
    }
    add_to_conf ("PacmanConf = %s\n", s);
    new_config.pacmanconf = strdup (s);

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (notif_icon_combo)) == 0)
    {
        add_to_conf ("NotificationIcon = NONE\n");
        new_config.notif_icon = ICON_NONE;
    }
    else if (gtk_combo_box_get_active (GTK_COMBO_BOX (notif_icon_combo)) == 1)
    {
        add_to_conf ("NotificationIcon = KALU\n");
        new_config.notif_icon = ICON_KALU;
    }
    else /* if (gtk_combo_box_get_active (GTK_COMBO_BOX (notif_icon_combo_changed_cb)) == 2) */
    {
        s = gtk_file_chooser_get_filename (
                GTK_FILE_CHOOSER (notif_icon_filechooser));
        if (NULL == s)
        {
            error_on_page (0,
                    "You need to select the file to use as icon on notifications");
        }
        add_to_conf ("NotificationIcon = %s\n", s);
        new_config.notif_icon = ICON_USER;
        new_config.notif_icon_user = strdup (s);
    }

    s = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_interval));
    if (*s == '\0')
    {
        g_free (s);
        error_on_page (0, "You need to specify an interval.");
    }
    nb = atoi (s);
    if (nb <= 0)
    {
        g_free (s);
        error_on_page (0, "Invalid value for the auto-check interval.");
        return;
    }
    add_to_conf ("Interval = %s\n", s);
    g_free (s);
    new_config.interval = nb * 60; /* we store seconds, not minutes */

    nb = (gint) gtk_range_get_value (GTK_RANGE (timeout_scale));
    if (nb == 0)
    {
        add_to_conf ("Timeout = DEFAULT\n");
        nb = NOTIFY_EXPIRES_DEFAULT;
    }
    else if (nb == 40)
    {
        add_to_conf ("Timeout = NEVER\n");
        nb = NOTIFY_EXPIRES_NEVER;
    }
    else
    {
        nb += 3;
        add_to_conf ("Timeout = %d\n", nb);
        nb *= 1000; /* we store ms, not seconds */
    }
    new_config.timeout = nb;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_skip)))
    {
        begin_hour = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (spin_begin_hour));
        begin_minute = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (spin_begin_minute));
        end_hour = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (spin_end_hour));
        end_minute = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (spin_end_minute));

        if (begin_hour < 0 || begin_hour > 23
                || begin_minute < 0 || begin_minute > 59
                || end_hour < 0 || end_hour > 23
                || end_minute < 0 || end_minute > 59)
        {
            error_on_page (0, "Invalid value for the auto-check interval.");
        }

        add_to_conf ("SkipPeriod = %02d:%02d-%02d:%02d\n",
                begin_hour, begin_minute, end_hour, end_minute);
        new_config.has_skip = TRUE;
        new_config.skip_begin_hour = begin_hour;
        new_config.skip_begin_minute = begin_minute;
        new_config.skip_end_hour = end_hour;
        new_config.skip_end_minute = end_minute;
    }
    else
    {
        new_config.has_skip = FALSE;
    }

    type = 0;
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (auto_news)))
    {
        type |= CHECK_NEWS;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (auto_upgrades)))
    {
        type |= CHECK_UPGRADES;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (auto_watched)))
    {
        type |= CHECK_WATCHED;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (auto_aur)))
    {
        type |= CHECK_AUR;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (auto_watched_aur)))
    {
        type |= CHECK_WATCHED_AUR;
    }
    if (type == 0)
    {
        error_on_page (0, "Nothing selected for automatic checks.");
    }
    add_to_conf ("AutoChecks =%s%s%s%s%s\n",
            (type & CHECK_NEWS)        ? " NEWS"        : "",
            (type & CHECK_UPGRADES)    ? " UPGRADES"    : "",
            (type & CHECK_WATCHED)     ? " WATCHED"     : "",
            (type & CHECK_AUR)         ? " AUR"         : "",
            (type & CHECK_WATCHED_AUR) ? " WATCHED_AUR" : ""
            );
    new_config.checks_auto = type;

    type = 0;
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (manual_news)))
    {
        type |= CHECK_NEWS;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (manual_upgrades)))
    {
        type |= CHECK_UPGRADES;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (manual_watched)))
    {
        type |= CHECK_WATCHED;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (manual_aur)))
    {
        type |= CHECK_AUR;
    }
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (manual_watched_aur)))
    {
        type |= CHECK_WATCHED_AUR;
    }
    if (type == 0)
    {
        error_on_page (0, "Nothing selected for manual checks.");
    }
    add_to_conf ("ManualChecks =%s%s%s%s%s\n",
            (type & CHECK_NEWS)        ? " NEWS"        : "",
            (type & CHECK_UPGRADES)    ? " UPGRADES"    : "",
            (type & CHECK_WATCHED)     ? " WATCHED"     : "",
            (type & CHECK_AUR)         ? " AUR"         : "",
            (type & CHECK_WATCHED_AUR) ? " WATCHED_AUR" : ""
            );
    new_config.checks_manual = type;

    /* News */
    s = (char *) gtk_entry_get_text (GTK_ENTRY (cmdline_link_entry));
    if (s == NULL || *s == '\0')
    {
        error_on_page (1,
                "You need to specify the command-line to open links.");
    }
    else if (!strstr (s, "$URL"))
    {
        error_on_page (1,
                "You need to use $URL on the command line to open links.");
    }
    add_to_conf ("CmdLineLink = %s\n", s);
    new_config.cmdline_link = strdup (s);

    /* Upgrades */
    new_config.check_pacman_conflict = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (check_pacman_conflict));
    add_to_conf ("CheckPacmanConflict = %d\n",
            new_config.check_pacman_conflict);

    s = (char *) gtk_entry_get_text (GTK_ENTRY (cmdline_entry));
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_upg_action)))
    {
#ifndef DISABLE_UPDATER
        if (gtk_combo_box_get_active (GTK_COMBO_BOX (upg_action_combo)) == 0)
        {
            add_to_conf ("UpgradeAction = KALU\n");
            new_config.action = UPGRADE_ACTION_KALU;
        }
        else
        {
#endif
            add_to_conf ("UpgradeAction = CMDLINE\n");
            new_config.action = UPGRADE_ACTION_CMDLINE;
            if (s == NULL || *s == '\0')
            {
                error_on_page (2, "You need to specify the command-line.");
            }
#ifndef DISABLE_UPDATER
        }
#endif
    }
    else
    {
        add_to_conf ("UpgradeAction = NONE\n");
        new_config.action = UPGRADE_NO_ACTION;
    }
    if (s != NULL && *s != '\0')
    {
        add_to_conf ("CmdLine = %s\n", s);
        new_config.cmdline = strdup (s);
    }

#ifndef DISABLE_UPDATER
    model = GTK_TREE_MODEL (cmdline_post_store);
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gtk_tree_model_get (model, &iter, 0, &s, -1);
            if (s != NULL && *s != '\0')
            {
                add_to_conf ("PostSysUpgrade = %s\n", s);
                new_config.cmdline_post = alpm_list_add (
                        new_config.cmdline_post,
                        strdup (s));
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }
    new_config.confirm_post = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (confirm_post));
    add_to_conf ("ConfirmPostSysUpgrade = %d\n", new_config.confirm_post);
#endif

    /* AUR */

    if (gtk_widget_get_sensitive (aur_cmdline_entry))
    {
        s = (char *) gtk_entry_get_text (GTK_ENTRY (aur_cmdline_entry));
        if (s == NULL || *s == '\0')
        {
            error_on_page (4, "You need to specify the command-line.");
        }
        add_to_conf ("CmdLineAur = %s\n", s);
        new_config.cmdline_aur = strdup (s);
    }

    model = GTK_TREE_MODEL (aur_ignore_store);
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        add_to_conf ("AurIgnore =");
        do
        {
            gtk_tree_model_get (model, &iter, 0, &s, -1);
            if (s != NULL && *s != '\0')
            {
                add_to_conf (" %s", s);
                new_config.aur_ignore = alpm_list_add (new_config.aur_ignore,
                        strdup (s));
            }
        } while (gtk_tree_model_iter_next (model, &iter));
        add_to_conf ("\n");
    }

    /* Misc */
    new_config.sane_sort_order = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (sane_sort_order));
    add_to_conf ("SaneSortOrder = %d\n", new_config.sane_sort_order);

    new_config.syncdbs_in_tooltip = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (syncdbs_in_tooltip));
    add_to_conf ("SyncDbsInTooltip = %d\n", new_config.syncdbs_in_tooltip);

    do_click (on_sgl_click, "OnSglClick", 0);
    do_click (on_dbl_click, "OnDblClick", 0);
    do_click (on_sgl_click_paused, "OnSglClickPaused", 1);
    do_click (on_dbl_click_paused, "OnDblClickPaused", 1);

    /* ** TEMPLATES ** */

    /* Upgrades */
    has_tpl = FALSE;
    tpl_field (tpl_upgrades,
            title,
            "upgrades",
            "Title",
            upg_title_entry,
            "Title",
            2);
    tpl_field (tpl_upgrades,
            package,
            "upgrades",
            "Package",
            upg_package_entry,
            "Package",
            2);
    tpl_field (tpl_upgrades,
            sep,
            "upgrades",
            "Sep",
            upg_sep_entry,
            "Separator",
            2);

    /* Watched */
    has_tpl = FALSE;
    tpl_field (tpl_watched,
            title,
            "watched",
            "Title",
            watched_title_entry,
            "Title",
            3);
    tpl_field (tpl_watched,
            package,
            "watched",
            "Package",
            watched_package_entry,
            "Package",
            3);
    tpl_field (tpl_watched,
            sep,
            "watched",
            "Sep",
            watched_sep_entry,
            "Separator",
            3);

    /* AUR */
    has_tpl = FALSE;
    tpl_field (tpl_aur,
            title,
            "aur",
            "Title",
            aur_title_entry,
            "Title",
            4);
    tpl_field (tpl_aur,
            package,
            "aur",
            "Package",
            aur_package_entry,
            "Package",
            4);
    tpl_field (tpl_aur,
            sep,
            "aur",
            "Sep",
            aur_sep_entry,
            "Separator",
            4);

    /* Watched AUR */
    has_tpl = FALSE;
    tpl_field (tpl_watched_aur,
            title,
            "watched-aur",
            "Title",
            watched_aur_title_entry,
            "Title",
            5);
    tpl_field (tpl_watched_aur,
            package,
            "watched-aur",
            "Package",
            watched_aur_package_entry,
            "Package",
            5);
    tpl_field (tpl_watched_aur,
            sep,
            "watched-aur",
            "Sep",
            watched_aur_sep_entry,
            "Separator",
            5);

    /* News */
    has_tpl = FALSE;
    tpl_field (tpl_news,
            title,
            "news",
            "Title",
            news_title_entry,
            "Title",
            1);
    tpl_field (tpl_news,
            package,
            "news",
            "Package",
            news_package_entry,
            "News item",
            1);
    tpl_field (tpl_news,
            sep,
            "news",
            "Sep",
            news_sep_entry,
            "Separator",
            1);

    /* save file */
    char conffile[MAX_PATH];
    GError *error = NULL;
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/kalu.conf",
            g_get_home_dir ());
    if (!ensure_path (conffile))
    {
        s = strrchr (conffile, '/');
        *s = '\0';
        s = g_strdup_printf ("%s cannot be created or is not a folder",
                conffile);
        show_error ("Unable to write configuration file", s,
                GTK_WINDOW (window));
        g_free (s);
        goto clean_on_error;
    }
    if (!g_file_set_contents (conffile, conf, -1, &error))
    {
        show_error ("Unable to write configuration file", error->message,
                GTK_WINDOW (window));
        g_clear_error (&error);
        goto clean_on_error;
    }
    debug ("preferences saved to %s:\n%s", conffile, conf);
    g_free (conf);

    /* free the now unneeded strings/lists */
    free (config->pacmanconf);
    free (config->notif_icon_user);
    free (config->cmdline);
    free (config->cmdline_aur);
    free (config->cmdline_link);
#ifndef DISABLE_UPDATER
    FREELIST (config->cmdline_post);
#endif
    free_tpl (config->tpl_upgrades);
    free_tpl (config->tpl_watched);
    free_tpl (config->tpl_aur);
    free_tpl (config->tpl_watched_aur);
    free_tpl (config->tpl_news);
    FREELIST (config->aur_ignore);
    /* copy new ones over */
    memcpy (config, &new_config, sizeof (config_t));

    /* done */
    gtk_widget_destroy (window);
    return;

clean_on_error:
    free (new_config.pacmanconf);
    free (new_config.notif_icon_user);
    free (new_config.cmdline);
    free (new_config.cmdline_aur);
    free (new_config.cmdline_link);
#ifndef DISABLE_UPDATER
    if (new_config.cmdline_post)
    {
        FREELIST (new_config.cmdline_post);
    }
#endif
    free_tpl (new_config.tpl_upgrades);
    free_tpl (new_config.tpl_watched);
    free_tpl (new_config.tpl_aur);
    free_tpl (new_config.tpl_watched_aur);
    free_tpl (new_config.tpl_news);
    if (new_config.aur_ignore)
    {
        FREELIST (new_config.aur_ignore);
    }
    g_free (conf);
}
#undef free_tpl
#undef tpl_field
#undef error_on_page
#undef add_to_conf
#undef do_click

static void
add_list (GtkWidget     *grid,
          int            top,
          GtkListStore **store,
          GtkWidget    **hbox,
          const char    *column_title,
          const char    *tooltip_list,
          const char    *tooltip_add,
          const char    *tooltip_edit,
          const char    *tooltip_remove,
          alpm_list_t   *list_data)
{
    GtkWidget *tree;
    *store = gtk_list_store_new (1, G_TYPE_STRING);
    tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (*store));
    g_object_set_data (G_OBJECT (tree), "store", (gpointer) *store);
    g_object_unref (*store);
    if (tooltip_list)
    {
        gtk_widget_set_tooltip_markup (tree, tooltip_list);
    }

    /* vbox - sort of a toolbar but vertical */
    GtkWidget *vbox_tb;
    GtkWidget *button;
    GtkWidget *image;
    *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_grid_attach (GTK_GRID (grid), *hbox, 0, top, 4, 1);
    gtk_widget_show (*hbox);

    vbox_tb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (*hbox), vbox_tb, FALSE, FALSE, 0);
    gtk_widget_show (vbox_tb);

    /* button Add */
    image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, tooltip_add);
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_add_cb), (gpointer) tree);
    gtk_widget_show (button);
    /* button Edit */
    image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    g_object_set_data (G_OBJECT (tree), "btn-edit", (gpointer) button);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, tooltip_edit);
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_edit_cb), (gpointer) tree);
    gtk_widget_show (button);
    /* button Remove */
    image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    g_object_set_data (G_OBJECT (tree), "btn-remove", (gpointer) button);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, tooltip_remove);
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_remove_cb), (gpointer) tree);
    gtk_widget_show (button);

    /* switch sensitive of buttons based on selection */
    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
    g_signal_connect (selection, "changed",
            G_CALLBACK (selection_changed_cb), (gpointer) tree);
    selection_changed_cb (selection, tree);

    /* a scrolledwindow for the tree */
    GtkWidget *scrolled;
    scrolled = gtk_scrolled_window_new (
            gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (tree)),
            gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (tree)));
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
            GTK_SHADOW_OUT);
    gtk_widget_show (scrolled);
    gtk_box_pack_start (GTK_BOX (*hbox), scrolled, TRUE, TRUE, 0);

    /* cell renderer & column(s) */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    /* column: Command-line */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set_data (G_OBJECT (renderer), "store", (gpointer) *store);
    g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
    g_signal_connect (G_OBJECT (renderer), "edited",
            G_CALLBACK (renderer_edited_cb), NULL);
    column = gtk_tree_view_column_new_with_attributes (column_title, renderer,
            "text", 0, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

    /* eo.columns  */

    /* fill data */
    GtkTreeIter iter;
    alpm_list_t *j;
    FOR_LIST (j, list_data)
    {
        gtk_list_store_append (*store, &iter);
        gtk_list_store_set (*store, &iter,
                0,  strdup (j->data),
                -1);
    }

    gtk_container_add (GTK_CONTAINER (scrolled), tree);
    gtk_widget_show (tree);
}

static void
add_on_click_actions (
        int *top,
        GtkWidget *grid,
        gboolean is_sgl_click,
        gboolean is_paused
        )
{
    GtkWidget    *label;
    GtkWidget   **combo;
    on_click_t    on_click;

    ++*top;
    label = gtk_label_new ((is_sgl_click)
            ? "When clicking the systray icon :"
            : "When double clicking the systray icon :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, *top, 1, 1);
    gtk_widget_show (label);

    combo = (is_sgl_click)
        ? (is_paused) ? &on_sgl_click_paused : &on_sgl_click
        : (is_paused) ? &on_dbl_click_paused : &on_dbl_click;

    *combo = gtk_combo_box_text_new ();
    if (is_paused)
    {
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "SAME_AS_ACTIVE",
                "Same as when active/not paused");
    }
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "NOTHING",
            "Do nothing");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "CHECK",
            "Check for Upgrades...");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "SYSUPGRADE",
            "System upgrade...");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "TOGGLE_WINDOWS",
#ifndef DISABLE_UPDATER
            "Hide/show opened windows (except kalu's updater)"
#else
            "Hide/show opened windows"
#endif
            );
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "LAST_NOTIFS",
            "Re-show last notifications...");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "TOGGLE_PAUSE",
            "Toggle pause/resume automatic checks");
    gtk_grid_attach (GTK_GRID (grid), *combo, 1, *top, 1, 1);
    gtk_widget_show (*combo);

    on_click = (is_sgl_click)
        ? (is_paused) ? config->on_sgl_click_paused : config->on_sgl_click
        : (is_paused) ? config->on_dbl_click_paused : config->on_dbl_click;

    if (is_paused && on_click == DO_SAME_AS_ACTIVE)
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "SAME_AS_ACTIVE");
    }
    else if (on_click == DO_CHECK)
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "CHECK");
    }
    else if (on_click == DO_SYSUPGRADE)
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "SYSUPGRADE");
    }
    else if (on_click == DO_TOGGLE_WINDOWS)
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "TOGGLE_WINDOWS");
    }
    else if (on_click == DO_LAST_NOTIFS)
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "LAST_NOTIFS");
    }
    else if (on_click == DO_TOGGLE_PAUSE)
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "TOGGLE_PAUSE");
    }
    else /* DO_NOTHING */
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "NOTHING");
    }
}

void
show_prefs (void)
{
    if (NULL != window)
    {
        gtk_window_present (GTK_WINDOW (window));
        return;
    }

    /* the window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Preferences - kalu");
    gtk_container_set_border_width (GTK_CONTAINER (window), 2);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    /* add to list of open windows */
    add_open_window (window);
    /* icon */
    GdkPixbuf *pixbuf;
    pixbuf = gtk_widget_render_icon_pixbuf (window, "kalu-logo",
            GTK_ICON_SIZE_DIALOG);
    gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
    g_object_unref (pixbuf);

    /* vbox */
    GtkWidget *vbox;
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);

    /* notebook */
    notebook = gtk_notebook_new ();
    gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
    gtk_widget_show (notebook);

    gchar buf[255];
    GtkWidget *lbl_page;
    GtkWidget *grid;
    int top;
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *box;
    GtkWidget *button;
    GtkWidget *hbox;
    GtkFileFilter *file_filter;

    /* [ General ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("General");

    /* PacmanConf */
    label = gtk_label_new ("Configuration file (pacman.conf) to use:");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    filechooser = gtk_file_chooser_button_new ("Select your pacman.conf",
            GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_grid_attach (GTK_GRID (grid), filechooser, 1, top, 1, 1);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser),
            config->pacmanconf);
    gtk_widget_show (filechooser);

    ++top;
    /* NotificationIcon */
    label = gtk_label_new ("Icon used on notifications :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    notif_icon_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (notif_icon_combo), "1",
            "No icon");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (notif_icon_combo), "2",
            "kalu's icon (small)");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (notif_icon_combo), "3",
            "Select file:");
    gtk_grid_attach (GTK_GRID (grid), notif_icon_combo, 1, top, 1, 1);
    gtk_widget_show (notif_icon_combo);
    if (config->notif_icon == ICON_KALU)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (notif_icon_combo), 1);
    }
    else if (config->notif_icon == ICON_USER)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (notif_icon_combo), 2);
    }
    else /* if (config->notif_icon == ICON_NONE) */
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (notif_icon_combo), 0);
    }

    ++top;
    notif_icon_filechooser = gtk_file_chooser_button_new (
            "Select notification icon",
            GTK_FILE_CHOOSER_ACTION_OPEN);
    file_filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (file_filter, "Supported Images");
    gtk_file_filter_add_pixbuf_formats (file_filter);
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (notif_icon_filechooser),
            file_filter);
    gtk_grid_attach (GTK_GRID (grid), notif_icon_filechooser, 0, top, 2, 1);
    if (config->notif_icon_user)
    {
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (notif_icon_filechooser),
                config->notif_icon_user);
    }
    if (config->notif_icon == ICON_USER)
    {
        gtk_widget_show (notif_icon_filechooser);
    }
    /* doing this now otherwise it's triggered with non-yet-existing widgets to hide/show */
    g_signal_connect (G_OBJECT (notif_icon_combo), "changed",
            G_CALLBACK (notif_icon_combo_changed_cb), NULL);

    ++top;
    /* Timeout */
    label = gtk_label_new ("Notifications expire after (seconds) :");
    gtk_widget_set_tooltip_text (label,
            "Delay after which the notification should expire/be automatically closed by the daemon.");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    /* 40 = 42 (MAX) - 4 (MIN) + 2 (DEFAULT + NEVER) */
    timeout_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
            0, 40, 1);
    gtk_widget_set_tooltip_text (timeout_scale,
            "Delay after which the notification should expire/be automatically closed by the daemon.");
    if (config->timeout == NOTIFY_EXPIRES_DEFAULT)
    {
        gtk_range_set_value (GTK_RANGE (timeout_scale), 0);
    }
    else if (config->timeout == NOTIFY_EXPIRES_NEVER)
    {
        gtk_range_set_value (GTK_RANGE (timeout_scale), 40);
    }
    else
    {
        gtk_range_set_value (GTK_RANGE (timeout_scale),
                (config->timeout / 1000) - 3);
    }
    gtk_grid_attach (GTK_GRID (grid), timeout_scale, 1, top, 1, 1);
    gtk_widget_show (timeout_scale);
    g_signal_connect (G_OBJECT (timeout_scale), "format-value",
            G_CALLBACK (timeout_format_value), NULL);

    ++top;
    /* Interval */
    label = gtk_label_new ("Check for upgrades every (minutes) :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    combo_interval = gtk_combo_box_text_new_with_entry ();
    entry = gtk_bin_get_child (GTK_BIN (combo_interval));
    /* make sure only digits can be typed in */
    g_signal_connect (G_OBJECT (entry), "insert-text",
            G_CALLBACK (insert_text_cb), (gpointer) combo_interval);
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "15", "15");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "30", "30");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "60", "60 (Hour)");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "120", "120 (Two hours)");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "1440", "1440 (Day)");
    /* Note: config->interval actually is in seconds, not minutes */
    if (config->interval == 900 /* 15m */)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 0);
    }
    else if (config->interval == 1800 /* 30m */)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 1);
    }
    else
    {
        snprintf (buf, 255, "%d", config->interval / 60);
        gtk_entry_set_text (GTK_ENTRY (entry), buf);
    }
    gtk_grid_attach (GTK_GRID (grid), combo_interval, 1, top, 1, 1);
    gtk_widget_show (combo_interval);

    ++top;
    /* SkipPeriod */
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_grid_attach (GTK_GRID (grid), box, 0, top, 2, 1);
    gtk_widget_show (box);

    button_skip = gtk_check_button_new_with_label ("Do not check between");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_skip),
            config->has_skip);
    gtk_box_pack_start (GTK_BOX (box), button_skip, FALSE, FALSE, 0);
    gtk_widget_show (button_skip);
    g_signal_connect (G_OBJECT (button_skip), "toggled",
            G_CALLBACK (skip_toggled_cb), NULL);

    spin_begin_hour = gtk_spin_button_new_with_range (0, 23, 1);
    gtk_widget_set_sensitive (spin_begin_hour, config->has_skip);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_begin_hour), 0);
    gtk_box_pack_start (GTK_BOX (box), spin_begin_hour, FALSE, FALSE, 0);
    gtk_widget_show (spin_begin_hour);

    label = gtk_label_new (":");
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    spin_begin_minute = gtk_spin_button_new_with_range (0, 59, 1);
    gtk_widget_set_sensitive (spin_begin_minute, config->has_skip);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_begin_minute), 0);
    gtk_box_pack_start (GTK_BOX (box), spin_begin_minute, FALSE, FALSE, 0);
    gtk_widget_show (spin_begin_minute);

    label = gtk_label_new ("and");
    gtk_widget_set_margin_left (label, 5);
    gtk_widget_set_margin_right (label, 5);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    spin_end_hour = gtk_spin_button_new_with_range (0, 23, 1);
    gtk_widget_set_sensitive (spin_end_hour, config->has_skip);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_end_hour), 0);
    gtk_box_pack_start (GTK_BOX (box), spin_end_hour, FALSE, FALSE, 0);
    gtk_widget_show (spin_end_hour);

    label = gtk_label_new (":");
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    spin_end_minute = gtk_spin_button_new_with_range (0, 59, 1);
    gtk_widget_set_sensitive (spin_end_minute, config->has_skip);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_end_minute), 0);
    gtk_box_pack_start (GTK_BOX (box), spin_end_minute, FALSE, FALSE, 0);
    gtk_widget_show (spin_end_minute);

    if (config->has_skip)
    {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_begin_hour),
                config->skip_begin_hour);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_begin_minute),
                config->skip_begin_minute);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_end_hour),
                config->skip_end_hour);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_end_minute),
                config->skip_end_minute);
    }

    ++top;
    /* AutoChecks */
    label = gtk_label_new ("During an automatic check, check for :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    auto_news = gtk_check_button_new_with_label (
            "Arch Linux news");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_news),
            config->checks_auto & CHECK_NEWS);
    gtk_grid_attach (GTK_GRID (grid), auto_news, 1, top, 1, 1);
    gtk_widget_show (auto_news);
    ++top;
    auto_upgrades = gtk_check_button_new_with_label (
            "Package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_upgrades),
            config->checks_auto & CHECK_UPGRADES);
    gtk_grid_attach (GTK_GRID (grid), auto_upgrades, 1, top, 1, 1);
    gtk_widget_show (auto_upgrades);
    ++top;
    auto_watched = gtk_check_button_new_with_label (
            "Watched package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_watched),
            config->checks_auto & CHECK_WATCHED);
    gtk_grid_attach (GTK_GRID (grid), auto_watched, 1, top, 1, 1);
    gtk_widget_show (auto_watched);
    ++top;
    auto_aur = gtk_check_button_new_with_label (
            "AUR package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_aur),
            config->checks_auto & CHECK_AUR);
    gtk_grid_attach (GTK_GRID (grid), auto_aur, 1, top, 1, 1);
    gtk_widget_show (auto_aur);
    ++top;
    auto_watched_aur = gtk_check_button_new_with_label (
            "AUR watched package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_watched_aur),
            config->checks_auto & CHECK_WATCHED_AUR);
    gtk_grid_attach (GTK_GRID (grid), auto_watched_aur, 1, top, 1, 1);
    gtk_widget_show (auto_watched_aur);

    ++top;
    /* ManualChecks */
    label = gtk_label_new ("During a manual check, check for :");
    gtk_widget_set_margin_top (label, 10);
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    manual_news = gtk_check_button_new_with_label (
            "Arch Linux news");
    gtk_widget_set_margin_top (manual_news, 10);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_news),
            config->checks_manual & CHECK_NEWS);
    gtk_grid_attach (GTK_GRID (grid), manual_news, 1, top, 1, 1);
    gtk_widget_show (manual_news);
    ++top;
    manual_upgrades = gtk_check_button_new_with_label (
            "Package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_upgrades),
            config->checks_manual & CHECK_UPGRADES);
    gtk_grid_attach (GTK_GRID (grid), manual_upgrades, 1, top, 1, 1);
    gtk_widget_show (manual_upgrades);
    ++top;
    manual_watched = gtk_check_button_new_with_label (
            "Watched package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_watched),
            config->checks_manual & CHECK_WATCHED);
    gtk_grid_attach (GTK_GRID (grid), manual_watched, 1, top, 1, 1);
    gtk_widget_show (manual_watched);
    ++top;
    manual_aur = gtk_check_button_new_with_label (
            "AUR package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_aur),
            config->checks_manual & CHECK_AUR);
    gtk_grid_attach (GTK_GRID (grid), manual_aur, 1, top, 1, 1);
    gtk_widget_show (manual_aur);
    ++top;
    manual_watched_aur = gtk_check_button_new_with_label (
            "AUR watched package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_watched_aur),
            config->checks_manual & CHECK_WATCHED_AUR);
    gtk_grid_attach (GTK_GRID (grid), manual_watched_aur, 1, top, 1, 1);
    gtk_widget_show (manual_watched_aur);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* set the copy of those templates to be used for falling back. that is,
     * they'll be updated whenever they're changed on prefs window, whereaas
     * config if only updated upon (and if) saving changes */
    fallback_templates.tpl_upgrades = new0 (templates_t, 1);
    fallback_templates.tpl_upgrades->title = strdup (config->tpl_upgrades->title);
    fallback_templates.tpl_upgrades->package = strdup (config->tpl_upgrades->package);
    fallback_templates.tpl_upgrades->sep = strdup (config->tpl_upgrades->sep);
    fallback_templates.tpl_aur = new0 (templates_t, 1);
    if (NULL != config->tpl_aur->title)
    {
        fallback_templates.tpl_aur->title = strdup (config->tpl_aur->title);
    }
    if (NULL != config->tpl_aur->package)
    {
        fallback_templates.tpl_aur->package = strdup (config->tpl_aur->package);
    }
    if (NULL != config->tpl_aur->sep)
    {
        fallback_templates.tpl_aur->sep = strdup (config->tpl_aur->sep);
    }

    /*******************************************/

    /* [ News ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("News");

    label = gtk_label_new ("Command line to open links :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    cmdline_link_entry = gtk_entry_new ();
    gtk_widget_set_tooltip_markup (cmdline_link_entry,
            "Use variable <b>$URL</b> for the URL to open");
    if (config->cmdline_link != NULL)
    {
        gtk_entry_set_text (GTK_ENTRY (cmdline_link_entry), config->cmdline_link);
    }
    gtk_grid_attach (GTK_GRID (grid), cmdline_link_entry, 1, top, 1, 1);
    gtk_widget_show (cmdline_link_entry);

    ++top;
    add_template (grid, top,
            &news_title_entry,
            &news_package_entry,
            &news_sep_entry,
            config->tpl_news,
            CHECK_NEWS);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Upgrades ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("Upgrades");

    /* CheckPacmanConflict */
    check_pacman_conflict = gtk_check_button_new_with_label (
            "Check for pacman/kalu conflict");
    gtk_widget_set_tooltip_text (check_pacman_conflict, 
            "Check whether an upgrade of pacman is likely to fail due to kalu's dependency, "
            "and if so adds a button on to notification to show a message about why "
            "and how to upgrade.");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_pacman_conflict),
            config->check_pacman_conflict);
    gtk_grid_attach (GTK_GRID (grid), check_pacman_conflict, 0, top, 4, 1);
    gtk_widget_show (check_pacman_conflict);

    ++top;
    /* UpgradeAction */
    button_upg_action = gtk_check_button_new_with_label (
            "Show a button \"Upgrade system\" on notifications (and on kalu's menu)");
    gtk_widget_set_tooltip_text (button_upg_action,
            "Whether or not to show a button \"Upgrade system\" on notifications, "
            "as well as an item \"System upgrade\" on kalu's menu");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_upg_action),
            config->action != UPGRADE_NO_ACTION);
    gtk_grid_attach (GTK_GRID (grid), button_upg_action, 0, top, 4, 1);
    gtk_widget_show (button_upg_action);
    g_signal_connect (G_OBJECT (button_upg_action), "toggled",
            G_CALLBACK (upg_action_toggled_cb), NULL);

#ifndef DISABLE_UPDATER
    ++top;
    label = gtk_label_new ("When clicking the button/menu :");
    gtk_widget_set_tooltip_text (label,
            "When clicking the button \"Upgrade system\" on notifications, "
            "or the menu \"System upgrade\"");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    upg_action_combo = gtk_combo_box_text_new ();
    gtk_widget_set_tooltip_text (upg_action_combo,
            "When clicking the button \"Upgrade system\" on notifications, "
            "or the menu \"System upgrade\"");
    gtk_widget_set_sensitive (upg_action_combo,
            config->action != UPGRADE_NO_ACTION);
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (upg_action_combo),
            "1",
            "Run kalu's system updater");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (upg_action_combo),
            "2",
            "Run the specified command-line");
    gtk_grid_attach (GTK_GRID (grid), upg_action_combo, 2, top, 2, 1);
    gtk_widget_show (upg_action_combo);
    if (config->action == UPGRADE_ACTION_CMDLINE)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (upg_action_combo), 1);
    }
    else /* UPGRADE_ACTION_KALU || UPGRADE_NO_ACTION */
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (upg_action_combo), 0);
    }
#endif

    ++top;
    /* CmdLine */
    cmdline_label = gtk_label_new ("Command-line:");
    gtk_grid_attach (GTK_GRID (grid), cmdline_label, 0, top, 2, 1);

    cmdline_entry = gtk_entry_new ();
    gtk_widget_set_sensitive (cmdline_entry,
            config->action != UPGRADE_NO_ACTION);
    if (config->cmdline)
    {
        gtk_entry_set_text (GTK_ENTRY (cmdline_entry), config->cmdline);
    }
    gtk_grid_attach (GTK_GRID (grid), cmdline_entry, 2, top, 2, 1);

#ifndef DISABLE_UPDATER
    if (config->action == UPGRADE_ACTION_CMDLINE)
    {
#endif
        gtk_widget_show (cmdline_label);
        gtk_widget_show (cmdline_entry);
#ifndef DISABLE_UPDATER
    }
#endif

#ifndef DISABLE_UPDATER
    ++top;
    /* PostSysUpgrade */
    add_list (grid, top, &cmdline_post_store, &cmdline_post_hbox,
            "After completing a system upgrade, start the following :",
            "You can use <b>$PACKAGES</b> to be replaced by the list of upgraded packages",
            "Add a new command-line",
            "Edit selected command-line",
            "Remove selected command-line",
            config->cmdline_post);
    gtk_widget_set_sensitive (cmdline_post_hbox,
            config->action != UPGRADE_NO_ACTION);
    ++top;
    /* ConfirmPostSysUpgrade */
    confirm_post = gtk_check_button_new_with_label (
            "Ask confirmation before starting anything");
    gtk_widget_set_tooltip_text (confirm_post,
            "Confirmation will be asked before starting those processes. "
            "With multiple ones, you'll be able to select which one(s) to start.");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (confirm_post),
            config->confirm_post);
    gtk_grid_attach (GTK_GRID (grid), confirm_post, 0, top, 2, 1);
    gtk_widget_show (confirm_post);

    /* doing this now otherwise it's triggered with non-yet-existing widgets to hide/show */
    g_signal_connect (G_OBJECT (upg_action_combo), "changed",
            G_CALLBACK (upg_action_changed_cb), NULL);
#endif

    ++top;
    add_template (grid, top,
            &upg_title_entry,
            &upg_package_entry,
            &upg_sep_entry,
            config->tpl_upgrades,
            CHECK_UPGRADES);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Watched ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("Watched");

    button = gtk_button_new_with_label ("Manage watched packages...");
    gtk_widget_set_margin_top (button, 10);
    gtk_grid_attach (GTK_GRID (grid), button, 1, top, 2, 1);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_manage_watched_cb), GINT_TO_POINTER (FALSE));

    ++top;
    add_template (grid, top,
            &watched_title_entry,
            &watched_package_entry,
            &watched_sep_entry,
            config->tpl_watched,
            CHECK_WATCHED);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ AUR ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("AUR");

    /* CmdLine */
    button = gtk_check_button_new_with_label (
            "Show a button \"Update AUR packages\" on notifications");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
            config->cmdline_aur != NULL);
    gtk_grid_attach (GTK_GRID (grid), button, 0, top, 4, 1);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT (button), "toggled",
            G_CALLBACK (aur_action_toggled_cb), NULL);

    ++top;
    label = gtk_label_new ("When clicking the button, run the following :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    aur_cmdline_entry = gtk_entry_new ();
    gtk_widget_set_tooltip_markup (aur_cmdline_entry,
            "You can use <b>$PACKAGES</b> to be replaced by the list of AUR packages "
            "with upgrades available");
    if (config->cmdline_aur != NULL)
    {
        gtk_entry_set_text (GTK_ENTRY (aur_cmdline_entry), config->cmdline_aur);
    }
    else
    {
        gtk_widget_set_sensitive (aur_cmdline_entry, FALSE);
    }
    gtk_grid_attach (GTK_GRID (grid), aur_cmdline_entry, 2, top, 2, 1);
    gtk_widget_show (aur_cmdline_entry);

    ++top;
    /* AurIgnore */
    label = gtk_label_new ("Do not check the AUR for the following packages :");
    gtk_widget_set_margin_top (label, 10);
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 4, 1);
    gtk_widget_show (label);

    ++top;
    add_list (grid, top, &aur_ignore_store, &hbox,
            "Package name",
            NULL,
            "Add a new package",
            "Edit selected package",
            "Remove selected package",
            config->aur_ignore);

    /* template */
    ++top;
    add_template (grid, top,
            &aur_title_entry,
            &aur_package_entry,
            &aur_sep_entry,
            config->tpl_aur,
            CHECK_AUR);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Watched AUR ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("Watched AUR");

    button = gtk_button_new_with_label ("Manage watched AUR packages...");
    gtk_widget_set_margin_top (button, 10);
    gtk_grid_attach (GTK_GRID (grid), button, 1, top, 2, 1);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_manage_watched_cb), GINT_TO_POINTER (TRUE));

    ++top;
    add_template (grid, top,
            &watched_aur_title_entry,
            &watched_aur_package_entry,
            &watched_aur_sep_entry,
            config->tpl_watched_aur,
            CHECK_WATCHED_AUR);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Misc ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("Misc");

    sane_sort_order = gtk_check_button_new_with_label ("Use sane sort indicator");
    gtk_widget_set_tooltip_text (sane_sort_order,
            "So when sorted descendingly, the arrow points down...\n"
            "This is used for the packages list in kalu's updater");
    gtk_widget_set_margin_top (sane_sort_order, 10);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sane_sort_order),
            config->sane_sort_order);
    gtk_grid_attach (GTK_GRID (grid), sane_sort_order, 0, top, 2, 1);
    gtk_widget_show (sane_sort_order);

    ++top;
    syncdbs_in_tooltip = gtk_check_button_new_with_label (
            "Show if databases can be synchronized in tooltip");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (syncdbs_in_tooltip),
            config->syncdbs_in_tooltip);
    gtk_grid_attach (GTK_GRID (grid), syncdbs_in_tooltip, 0, top, 2, 1);
    gtk_widget_show (syncdbs_in_tooltip);

    ++top;
    label = gtk_label_new (NULL);
    gtk_widget_set_margin_top (label, 10);
    gtk_label_set_markup (GTK_LABEL (label),
            "When kalu is <b>active</b> :");
    gtk_widget_set_tooltip_markup (label,
            "Actions to be done when kalu is <b>not</b> paused.");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    add_on_click_actions (&top, grid, TRUE, FALSE);
    add_on_click_actions (&top, grid, FALSE, FALSE);

    ++top;
    label = gtk_label_new (NULL);
    gtk_widget_set_margin_top (label, 23);
    gtk_label_set_markup (GTK_LABEL (label),
            "When kalu is <b>paused</b> :");
    gtk_widget_set_tooltip_markup (label,
            "Actions to be done when kalu is <b>paused</b>.");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    add_on_click_actions (&top, grid, TRUE, TRUE);
    add_on_click_actions (&top, grid, FALSE, TRUE);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* buttons */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    GtkWidget *image;
    button = gtk_button_new_with_label ("Save preferences");
    image = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
    gtk_widget_set_tooltip_text (button, "Apply and save preferences");
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_save_cb), NULL);
    gtk_widget_show (button);

    button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    g_signal_connect_swapped (G_OBJECT (button), "clicked",
            G_CALLBACK (gtk_widget_destroy), (gpointer) window);

    /* signals */
    g_signal_connect (G_OBJECT (window), "destroy",
            G_CALLBACK (destroy_cb), NULL);

    /* enforce (minimum) size */
    gint w, h;
    gtk_window_get_size (GTK_WINDOW (window), &w, &h);
    w = MAX(420, w);
    h = MAX(420, h);
    gtk_widget_set_size_request (window, w, h);

    /* show */
    gtk_widget_show (window);
}
