/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * preferences.c
 * Copyright (C) 2012-2017 Olivier Brunel <jjk@jjacky.com>
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
static GtkWidget *notif_icon_scale          = NULL;
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
/* Watched */
/* AUR */
static GtkWidget *aur_cmdline_entry         = NULL;
static GtkListStore *aur_ignore_store       = NULL;
/* Watched */
/* Misc */
static GtkWidget *syncdbs_in_tooltip        = NULL;
static GtkWidget *on_sgl_click              = NULL;
static GtkWidget *on_dbl_click              = NULL;
static GtkWidget *on_mdl_click              = NULL;
static GtkWidget *on_sgl_click_paused       = NULL;
static GtkWidget *on_dbl_click_paused       = NULL;
static GtkWidget *on_mdl_click_paused       = NULL;
/* templates */
struct _tpl_widgets {
    GtkWidget *source;
    GtkWidget *entry;
};
static struct _tpl_widgets tpl_widgets[_NB_TPL * _NB_FLD] = { NULL, };
static templates_t tpl_config[_NB_TPL];
static int tpl_pg[_NB_TPL] = { 2, 3, 4, 4, 5, 1 };



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
free_tpl_if_differ_from (templates_t tpl_to_free[],
                         templates_t tpl_ref[])
{
    int tpl, fld;

    for (tpl = 0; tpl < _NB_TPL; ++tpl)
        for (fld = 0; fld < _NB_FLD; ++fld)
            if (tpl_to_free[tpl].fields[fld].custom
                    != tpl_ref[tpl].fields[fld].custom)
                free (tpl_to_free[tpl].fields[fld].custom);
}

static void
destroy_cb (GtkWidget *widget _UNUSED_)
{
    /* free our own (unsaved) custom values */
    free_tpl_if_differ_from (tpl_config, config->templates);

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
        return g_strdup (_c("expiration-delay", "Default"));
    }
    else if (value == 40)
    {
        return g_strdup (_c("expiration-delay", "Never"));
    }
    else
    {
        /* TRANSLATORS: context "expiration-delay" Also note this can only be
         * from 4 to 42 */
        return g_strdup_printf (_n("%d seconds", "%d seconds",
                    (long unsigned int) (3 + value)),
                3 + value);
    }
}

static void
set_combo_tooltip (GtkWidget *w, int tpl, int fld)
{
    GString *str;

    str = g_string_new (_("Select the source for this field:\n"));
    if (tpl_config[tpl].fields[fld].def)
        g_string_append (str, _("- Default: use default value\n"));
    if (fld != FLD_TITLE && tpl_config[tpl].fallback != NO_TPL)
        g_string_append_printf (str, _("- Fall back: use value from template '%s'\n"),
                gtk_notebook_get_tab_label_text ((GtkNotebook *) notebook,
                    gtk_notebook_get_nth_page ((GtkNotebook *) notebook,
                        tpl_pg[tpl_config[tpl].fallback])));
    g_string_append (str, _("- Custom: specify a custom value\n"));
    if (fld != FLD_TITLE)
        g_string_append (str, _("- None: no value\n"));

    g_string_truncate (str, str->len - 1);
    gtk_widget_set_tooltip_markup (w, str->str);
    g_string_free (str, TRUE);
}

static void
fill_combo (GtkWidget *w, int tpl, int fld)
{
    if (tpl_config[tpl].fields[fld].def)
        gtk_combo_box_text_append ((GtkComboBoxText *) w, "DEFAULT", _("Default"));
    if (fld != FLD_TITLE && tpl_config[tpl].fallback != NO_TPL)
        gtk_combo_box_text_append ((GtkComboBoxText *) w, "FALLBACK", _("Fall back"));
    gtk_combo_box_text_append ((GtkComboBoxText *) w, "CUSTOM", _("Custom"));
    if (fld != FLD_TITLE)
        gtk_combo_box_text_append ((GtkComboBoxText *) w, "NONE", _("None"));
}

static const char *
get_fld_value (tpl_t tpl, int fld)
{
    struct field *f = &tpl_config[tpl].fields[fld];

    switch (f->source)
    {
        case TPL_SCE_DEFAULT:
            return f->def;

        case TPL_SCE_FALLBACK:
            return get_fld_value (tpl_config[tpl].fallback, fld);

        case TPL_SCE_CUSTOM:
            return f->custom;

        case TPL_SCE_NONE:
        case TPL_SCE_UNDEFINED: /* silence warning */
        default: /* silence warning */
            return NULL;
    }
}

static void
update_custom_for_id (int id)
{
    int tpl, fld;
    gchar *cur;
    const gchar *new;

    tpl = id / _NB_FLD;
    fld = id - tpl * 3;

    cur = tpl_config[tpl].fields[fld].custom;
    new = gtk_entry_get_text ((GtkEntry *) tpl_widgets[id].entry);

    if (!streq (cur, new))
    {
        if (config->templates[tpl].fields[fld].custom != cur)
            free (cur);
        tpl_config[tpl].fields[fld].custom = strdup (new);
    }
}

static void
set_entry_text_for_id (int id, const gchar *s)
{
    GtkEntryBuffer *buffer;

    buffer = gtk_entry_get_buffer ((GtkEntry *) tpl_widgets[id].entry);
    gtk_entry_buffer_delete_text (buffer, 0, -1);
    if (s)
    {
        s = escape_text (s);
        gtk_entry_buffer_insert_text (buffer, 0, s, -1);
        free ((char *) s);
    }
}

static void
sce_changed_cb (GtkComboBox *combo, gpointer data)
{
    int id, tpl, fld;
    const gchar *sce;
    const gchar *s;

    id = GPOINTER_TO_INT (g_object_get_data ((GObject *) combo, "id"));
    tpl = id / _NB_FLD;
    fld = id - tpl * 3;

    /* if it was CUSTOM before the change, we need to update the custom value.
     * This could happen w/out focus_out_cb() called when e.g. using mouse wheel
     * over the combobox whilst editing the entry.
     * data is set when called on window creation, i.e. we shall skip the
     * update_custom_for_id() call since it obviously is empty for now. */
    if (!data && tpl_config[tpl].fields[fld].source == TPL_SCE_CUSTOM)
        update_custom_for_id (id);

    sce = gtk_combo_box_get_active_id (combo);
    if (streq (sce, "DEFAULT"))
    {
        tpl_config[tpl].fields[fld].source = TPL_SCE_DEFAULT;
        s = tpl_config[tpl].fields[fld].def;
    }
    else if (streq (sce, "FALLBACK"))
    {
        tpl_config[tpl].fields[fld].source = TPL_SCE_FALLBACK;
        s = get_fld_value (tpl_config[tpl].fallback, fld);
    }
    else if (streq (sce, "CUSTOM"))
    {
        tpl_config[tpl].fields[fld].source = TPL_SCE_CUSTOM;
        s = tpl_config[tpl].fields[fld].custom;
    }
    else /* NONE */
    {
        tpl_config[tpl].fields[fld].source = TPL_SCE_NONE;
        s = NULL;
    }

    set_entry_text_for_id (id, s);

    if (tpl_config[tpl].fields[fld].source == TPL_SCE_CUSTOM)
    {
        gtk_widget_set_sensitive (tpl_widgets[id].entry, TRUE);
        gtk_editable_set_editable ((GtkEditable *) tpl_widgets[id].entry, TRUE);
        gtk_entry_set_icon_from_icon_name ((GtkEntry *) tpl_widgets[id].entry,
                GTK_ENTRY_ICON_PRIMARY, NULL);
    }
    else if (tpl_config[tpl].fields[fld].source == TPL_SCE_NONE)
    {
        gtk_widget_set_sensitive (tpl_widgets[id].entry, FALSE);
        gtk_entry_set_icon_from_icon_name ((GtkEntry *) tpl_widgets[id].entry,
                GTK_ENTRY_ICON_PRIMARY, NULL);
    }
    else
    {
        gtk_widget_set_sensitive (tpl_widgets[id].entry, TRUE);
        gtk_editable_set_editable ((GtkEditable *) tpl_widgets[id].entry, FALSE);
        gtk_entry_set_icon_from_icon_name ((GtkEntry *) tpl_widgets[id].entry,
                GTK_ENTRY_ICON_PRIMARY, "gtk-edit");
        gtk_entry_set_icon_tooltip_text ((GtkEntry *) tpl_widgets[id].entry,
                GTK_ENTRY_ICON_PRIMARY, _("Edit as custom value"));
    }
}

static gboolean
focus_out_cb (GtkWidget *entry, GdkEvent *event _UNUSED_, gpointer data _UNUSED_)
{
    int id, tpl, fld;

    id = GPOINTER_TO_INT (g_object_get_data ((GObject *) entry, "id"));
    tpl = id / _NB_FLD;
    fld = id - tpl * 3;

    if (tpl_config[tpl].fields[fld].source == TPL_SCE_CUSTOM)
        update_custom_for_id (id);

    /* keep processing */
    return FALSE;
}

static void
entry_icon_press_cb (GtkEntry               *entry,
                     GtkEntryIconPosition    pos _UNUSED_,
                     GdkEvent               *event _UNUSED_,
                     gpointer                data _UNUSED_)
{
    int id, tpl, fld;
    gchar *s;

    id = GPOINTER_TO_INT (g_object_get_data ((GObject *) entry, "id"));
    tpl = id / _NB_FLD;
    fld = id - tpl * 3;

    s = escape_text (get_fld_value (tpl, fld));
    gtk_combo_box_set_active_id ((GtkComboBox *) tpl_widgets[id].source, "CUSTOM");
    gtk_entry_set_text (entry, s);
    free (s);
    gtk_widget_grab_focus ((GtkWidget *) entry);
}

#define add_label(text)    do {                                     \
    w = gtk_label_new (text);                                       \
    gtk_grid_attach (GTK_GRID (grid), w, 0, top, 1, 1);             \
    gtk_widget_show (w);                                            \
} while (0)
#define add_combo(fld)     do {                                     \
    w = gtk_combo_box_text_new ();                                  \
    fill_combo (w, tpl, fld);                                       \
    g_object_set_data (G_OBJECT (w), "id",                          \
            GINT_TO_POINTER (tpl * 3 + fld));                       \
    gtk_combo_box_set_active_id ((GtkComboBox *) w,                 \
            tpl_sce_names[tpl_config[tpl].fields[fld].source]);     \
    g_signal_connect (G_OBJECT (w), "changed",                      \
            G_CALLBACK (sce_changed_cb), NULL);                     \
    gtk_grid_attach (GTK_GRID (grid), w, 1, top, 1, 1);             \
    gtk_widget_show (w);                                            \
    tpl_widgets[tpl * 3 + fld].source = w;                          \
} while (0)
#define set_entry(fld)   do {                                       \
    gtk_entry_set_width_chars ((GtkEntry *) w, 42);                 \
    g_object_set_data (G_OBJECT (w), "type",                        \
            GINT_TO_POINTER (type));                                \
    g_object_set_data (G_OBJECT (w), "id",                          \
            GINT_TO_POINTER (tpl * 3 + fld));                       \
    gtk_widget_add_events (w, GDK_FOCUS_CHANGE_MASK);               \
    g_signal_connect (G_OBJECT (w), "focus-out-event",              \
            G_CALLBACK (focus_out_cb), NULL);                       \
    g_signal_connect (G_OBJECT (w), "icon-press",                   \
            G_CALLBACK (entry_icon_press_cb), NULL);                \
    tpl_widgets[tpl * 3 + fld].entry = w;                           \
    sce_changed_cb ((GtkComboBox *) tpl_widgets[tpl * 3 + fld].source,  \
            GINT_TO_POINTER (1));                                   \
} while (0)
#define add_to_grid(entry)   do {                           \
    gtk_grid_attach (GTK_GRID (grid), entry, 2, top, 2, 1); \
    gtk_widget_show (entry);                                \
} while (0)
static void
add_template (GtkWidget    *grid,
              int           top,
              int           tpl,
              check_t       type)
{
    char *tooltip;
    GtkWidget *w;

    /* notification template */
    w = gtk_label_new (NULL);
    gtk_widget_set_size_request (w, 500, -1);
    if (type & _CHECK_AUR_NOT_FOUND)
        gtk_label_set_markup (GTK_LABEL (w), _("<b>When packages are not found:</b>"));
    else
        gtk_label_set_markup (GTK_LABEL (w), _("<b>Notification template</b>"));
    gtk_widget_set_margin_top (w, 15);
    gtk_grid_attach (GTK_GRID (grid), w, 0, top, 4, 1);
    gtk_widget_show (w);

    /* Title */
    ++top;
    add_label (_("Title :"));
    add_combo (FLD_TITLE);

    w = gtk_entry_new ();
    /* set tooltip */
    tooltip = g_strconcat (
            _(  "The following variables are available :"
                "\n- <b>$NB</b> : number of packages/news items"),
            (type & (CHECK_UPGRADES | CHECK_WATCHED))
            ? _("\n- <b>$DL</b> : total download size"
                "\n- <b>$INS</b> : total installed size"
                "\n- <b>$NET</b> : total net (post-install difference) size")
            : NULL,
            NULL);
    gtk_widget_set_tooltip_markup (w, tooltip);
    g_free (tooltip);
    /* set value if any, else unsensitive */
    set_entry (FLD_TITLE);
    /* add to grid */
    add_to_grid (w);


    /* Package */
    ++top;
    add_label ((type & CHECK_NEWS) ? _("News item :") : _("Package :"));
    add_combo (FLD_PACKAGE);

    w = gtk_entry_new ();
    /* set tooltip */
    if (type & CHECK_NEWS)
        tooltip = g_strconcat (
            _("The following variables are available :"),
            _("\n- <b>$NEWS</b> : the news title"),
            NULL);
    else
    {
        tooltip = g_strconcat (
            _("The following variables are available :"),
            (type & (CHECK_UPGRADES | CHECK_WATCHED))
            ? _("\n- <b>$REPO</b> : repository name") : "",
            _("\n- <b>$PKG</b> : package name"),
            _("\n- <b>$OLD</b> : old/current version number"),
            (type & _CHECK_AUR_NOT_FOUND) ? NULL
            : _("\n- <b>$NEW</b> : new version number"),
            (!(type & (CHECK_UPGRADES | CHECK_WATCHED)))
            ? NULL : _("\n- <b>$DL</b> : download size"),
            _("\n- <b>$INS</b> : installed size"),
            _("\n- <b>$NET</b> : net (post-install difference) size"),
            NULL);
    }
    gtk_widget_set_tooltip_markup (w, tooltip);
    g_free (tooltip);
    /* set value if any, else unsensitive */
    set_entry (FLD_PACKAGE);
    /* add to grid */
    add_to_grid (w);


    /* Separator */
    ++top;
    add_label (_("Separator :"));
    add_combo (FLD_SEP);

    w = gtk_entry_new ();
    /* set tooltip */
    gtk_widget_set_tooltip_text (w, _("No variables available."));
    /* set value if any, else unsensitive */
    set_entry (FLD_SEP);
    /* add to grid */
    add_to_grid (w);
}
#undef add_to_grid
#undef set_entry
#undef add_combo
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
    show_error (_("Unable to apply/save preferences"), errmsg,          \
            GTK_WINDOW (window));                                       \
    goto clean_on_error;                                                \
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
    else if (strcmp (s, "SIMULATION") == 0)                                 \
    {                                                                       \
        new_config.name = DO_SIMULATION;                                    \
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
    else if (strcmp (s, "EXIT") == 0)                                       \
    {                                                                       \
        new_config.name = DO_EXIT;                                          \
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
#define add_color(val_name, cfg_name, def)              \
    if (!streq (new_config.color_##val_name, def))      \
    {                                                   \
        if (*new_config.color_##val_name == '#')        \
            *new_config.color_##val_name = '.';         \
        add_to_conf ("Color" cfg_name " = %s\n",        \
                new_config.color_##val_name);           \
        if (*new_config.color_##val_name == '.')        \
            *new_config.color_##val_name = '#';         \
    }
static void
btn_save_cb (GtkButton *button _UNUSED_, gpointer data _UNUSED_)
{
    gchar *conf, tmp[1024];
    int alloc = 1024, len = 0, need;
    gchar *s;
    gint nb, begin_hour, begin_minute, end_hour, end_minute;
    check_t type;
    GtkTreeModel *model;
    GtkTreeIter iter;
    config_t new_config;

    /* init the new kalu.conf we'll be writing */
    conf = new0 (gchar, alloc + 1);
    add_to_conf ("[options]\n");
    /* also init the new config_t: copy the actual one */
    memcpy (&new_config, config, sizeof (config_t));
    /* "import" current templates */
    memcpy (&new_config.templates, &tpl_config, sizeof (templates_t) * _NB_TPL);
    /* and set to NULL all that matters (strings/lists/templates we'll re-set) */
    new_config.pacmanconf       = NULL;
    new_config.notif_icon_user  = NULL;
    new_config.cmdline          = NULL;
    new_config.cmdline_aur      = NULL;
    new_config.cmdline_link     = NULL;
#ifndef DISABLE_UPDATER
    new_config.cmdline_post     = NULL;
#endif
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

    /* disabling buttons on notifications (no GUI) */
    if (!new_config.notif_buttons)
    {
        add_to_conf ("NotifButtons = 0\n");
    }

#ifndef DISABLE_UPDATER
    /* colors (no GUI) */
    add_color (unimportant, "Unimportant", "gray");
    add_color (info, "Info", "blue");
    add_color (warning, "Warning", "green");
    add_color (error, "Error", "red");

    /* auto show log (no GUI) */
    if (new_config.auto_show_log)
    {
        add_to_conf ("AutoShowLog = 1\n");
    }
#endif

    /* General */
    s = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
    if (NULL == s)
    {
        error_on_page (0,
                _("You need to select the configuration file to use for libalpm (pacman.conf)"));
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
                    _("You need to select the file to use as icon on notifications"));
        }
        add_to_conf ("NotificationIcon = %s\n", s);
        new_config.notif_icon = ICON_USER;
        new_config.notif_icon_user = strdup (s);
    }

    nb = (gint) gtk_range_get_value (GTK_RANGE (notif_icon_scale));
    add_to_conf ("NotificationIconSize = %d\n", nb);
    new_config.notif_icon_size = nb;

    s = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_interval));
    if (*s == '\0')
    {
        g_free (s);
        error_on_page (0, _("You need to specify an interval."));
    }
    nb = atoi (s);
    if (nb < 0)
    {
        g_free (s);
        error_on_page (0, _("Invalid value for the auto-check interval."));
        return;
    }
    add_to_conf ("Interval = %d\n", nb);
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
            error_on_page (0, _("Invalid value for the auto-check interval."));
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
        error_on_page (0, _("Nothing selected for automatic checks."));
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
        error_on_page (0, _("Nothing selected for manual checks."));
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
                _("You need to specify the command-line to open links."));
    }
    else if (!strstr (s, "$URL"))
    {
        error_on_page (1,
                _("You need to use $URL on the command line to open links."));
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
                error_on_page (2, _("You need to specify the command-line."));
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
            error_on_page (4, _("You need to specify the command-line."));
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
    new_config.syncdbs_in_tooltip = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (syncdbs_in_tooltip));
    add_to_conf ("SyncDbsInTooltip = %d\n", new_config.syncdbs_in_tooltip);

    do_click (on_sgl_click, "OnSglClick", 0);
    do_click (on_dbl_click, "OnDblClick", 0);
    do_click (on_mdl_click, "OnMdlClick", 0);
    do_click (on_sgl_click_paused, "OnSglClickPaused", 1);
    do_click (on_dbl_click_paused, "OnDblClickPaused", 1);
    do_click (on_mdl_click_paused, "OnMdlClickPaused", 1);

    /* ** TEMPLATES ** */

    int tpl, fld;
    for (tpl = 0; tpl < _NB_TPL; ++tpl)
    {
        add_to_conf ("[template-%s]\n", tpl_names[tpl]);
        for (fld = 0; fld < _NB_FLD; ++fld)
        {
            add_to_conf ("%sSce = %s\n",
                    fld_names[fld],
                    tpl_sce_names[new_config.templates[tpl].fields[fld].source]);
            if (new_config.templates[tpl].fields[fld].custom
                    && *new_config.templates[tpl].fields[fld].custom != '\0')
                add_to_conf ("%s = \"%s\"\n",
                        fld_names[fld],
                        /* so we get it w/ LF-s escaped */
                        gtk_entry_get_text ((GtkEntry *) tpl_widgets[tpl * 3 + fld].entry));
            else if (new_config.templates[tpl].fields[fld].source == TPL_SCE_CUSTOM)
                error_on_page (tpl_pg[tpl], _("Custom value is required."));
        }
    }

    /* save file */
    char conffile[PATH_MAX];
    GError *error = NULL;
    snprintf (conffile, PATH_MAX - 1, "%s/kalu/kalu.conf",
            g_get_user_config_dir ());
    if (!ensure_path (conffile))
    {
        s = strrchr (conffile, '/');
        *s = '\0';
        s = g_strdup_printf (_("%s cannot be created or is not a folder"),
                conffile);
        show_error (_("Unable to write configuration file"), s,
                GTK_WINDOW (window));
        g_free (s);
        goto clean_on_error;
    }
    if (!g_file_set_contents (conffile, conf, -1, &error))
    {
        show_error (_("Unable to write configuration file"), error->message,
                GTK_WINDOW (window));
        g_clear_error (&error);
        goto clean_on_error;
    }
    debug ("preferences saved to %s:\n%s", conffile, conf);
    g_free (conf);

    /* free the now unneeded strings/lists */
    free_tpl_if_differ_from (config->templates, new_config.templates);
    free (config->pacmanconf);
    free (config->notif_icon_user);
    free (config->cmdline);
    free (config->cmdline_aur);
    free (config->cmdline_link);
#ifndef DISABLE_UPDATER
    FREELIST (config->cmdline_post);
#endif
    FREELIST (config->aur_ignore);
    /* copy new ones over */
    memcpy (config, &new_config, sizeof (config_t));

    /* reset timeout for next auto-checks */
    reset_timeout ();

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
    image = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, tooltip_add);
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_add_cb), (gpointer) tree);
    gtk_widget_show (button);
    /* button Edit */
    image = gtk_image_new_from_icon_name ("gtk-edit", GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    g_object_set_data (G_OBJECT (tree), "btn-edit", (gpointer) button);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, tooltip_edit);
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_edit_cb), (gpointer) tree);
    gtk_widget_show (button);
    /* button Remove */
    image = gtk_image_new_from_icon_name ("list-remove", GTK_ICON_SIZE_MENU);
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

enum click
{
    CLICK_SGL,
    CLICK_DBL,
    CLICK_MDL
};

static void
add_on_click_actions (
        int *top,
        GtkWidget *grid,
        enum click click,
        gboolean is_paused
        )
{
    GtkWidget    *label     = NULL;
    GtkWidget   **combo     = NULL;
    on_click_t    on_click  = DO_NOTHING;

    ++*top;
    switch (click)
    {
        case CLICK_SGL:
            label = gtk_label_new (_("When clicking the systray icon :"));
            combo = (is_paused) ? &on_sgl_click_paused : &on_sgl_click;
            on_click = (is_paused) ? config->on_sgl_click_paused : config->on_sgl_click;
            break;
        case CLICK_DBL:
            label = gtk_label_new (_("When double clicking the systray icon :"));
            combo = (is_paused) ? &on_dbl_click_paused : &on_dbl_click;
            on_click = (is_paused) ? config->on_dbl_click_paused : config->on_dbl_click;
            break;
        case CLICK_MDL:
            label = gtk_label_new (_("When middle clicking the systray icon :"));
            combo = (is_paused) ? &on_mdl_click_paused : &on_mdl_click;
            on_click = (is_paused) ? config->on_mdl_click_paused : config->on_mdl_click;
            break;
    }

    gtk_grid_attach (GTK_GRID (grid), label, 0, *top, 1, 1);
    gtk_widget_show (label);

    *combo = gtk_combo_box_text_new ();
    if (is_paused)
    {
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "SAME_AS_ACTIVE",
                _("Same as when active/not paused"));
    }
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "NOTHING",
            _("Do nothing"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "CHECK",
            _("Check for Upgrades..."));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "SYSUPGRADE",
            _("System upgrade..."));
#ifndef DISABLE_UPDATER
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "SIMULATION",
            _("Upgrade simulation..."));
#endif
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "TOGGLE_WINDOWS",
#ifndef DISABLE_UPDATER
            _("Hide/show opened windows (except kalu's updater)")
#else
            _("Hide/show opened windows")
#endif
            );
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "LAST_NOTIFS",
            _("Re-show last notifications..."));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "TOGGLE_PAUSE",
            _("Toggle pause/resume automatic checks"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (*combo), "EXIT",
            _("Exit kalu"));
    gtk_grid_attach (GTK_GRID (grid), *combo, 1, *top, 1, 1);
    gtk_widget_show (*combo);

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
#ifndef DISABLE_UPDATER
    else if (on_click == DO_SIMULATION)
    {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (*combo), "SIMULATION");
    }
#endif
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

static void
refresh_tpl_fallbacks (int tpl)
{
    int fld;

    for (fld = 0; fld < _NB_FLD; ++fld)
        if (tpl_config[tpl].fields[fld].source == TPL_SCE_FALLBACK)
            set_entry_text_for_id (tpl * 3 + fld,
                    get_fld_value (tpl_config[tpl].fallback, fld));
}

static void
switch_page_cb (GtkNotebook *nb     _UNUSED_,
                GtkWidget   *w      _UNUSED_,
                guint        page,
                gpointer     data   _UNUSED_)
{
    switch (page)
    {
        case 1:
            refresh_tpl_fallbacks (TPL_NEWS);
            break;
        case 2:
            refresh_tpl_fallbacks (TPL_UPGRADES);
            break;
        case 3:
            refresh_tpl_fallbacks (TPL_WATCHED);
            break;
        case 4:
            refresh_tpl_fallbacks (TPL_AUR);
            refresh_tpl_fallbacks (TPL_AUR_NOT_FOUND);
            break;
        case 5:
            refresh_tpl_fallbacks (TPL_WATCHED_AUR);
            break;
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

    /* "import" templates from config */
    memcpy (&tpl_config, &config->templates, sizeof (templates_t) * _NB_TPL);

    /* the window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), _("Preferences - kalu"));
    gtk_container_set_border_width (GTK_CONTAINER (window), 2);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    /* add to list of open windows */
    add_open_window (window);
    /* icon */
    gtk_window_set_icon_name (GTK_WINDOW (window), "kalu");

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
    lbl_page = gtk_label_new (_c("prefs-tab", "General"));

    /* PacmanConf */
    label = gtk_label_new (_("Configuration file (pacman.conf) to use:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    filechooser = gtk_file_chooser_button_new (_("Select your pacman.conf"),
            GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_grid_attach (GTK_GRID (grid), filechooser, 1, top, 1, 1);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser),
            config->pacmanconf);
    gtk_widget_show (filechooser);

    ++top;
    /* NotificationIcon */
    label = gtk_label_new (_("Icon used on notifications :"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    notif_icon_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (notif_icon_combo), "1",
            _("No icon"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (notif_icon_combo), "2",
            _("kalu's icon"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (notif_icon_combo), "3",
            _("Select file:"));
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
            _("Select notification icon"),
            GTK_FILE_CHOOSER_ACTION_OPEN);
    file_filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (file_filter, _("Supported Images"));
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
    /* NotificationIcon */
    label = gtk_label_new (_("Size of the icon used on notifications :"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    notif_icon_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
            8, 48, 1);
    gtk_range_set_value (GTK_RANGE (notif_icon_scale), config->notif_icon_size);
    gtk_grid_attach (GTK_GRID (grid), notif_icon_scale, 1, top, 1, 1);
    gtk_widget_show (notif_icon_scale);

    ++top;
    /* Timeout */
    label = gtk_label_new (_("Notifications expire after (seconds) :"));
    gtk_widget_set_tooltip_text (label,
            _("Delay after which the notification should expire/be automatically closed by the daemon."));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    /* 40 = 42 (MAX) - 4 (MIN) + 2 (DEFAULT + NEVER) */
    timeout_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
            0, 40, 1);
    gtk_widget_set_tooltip_text (timeout_scale,
            _("Delay after which the notification should expire/be automatically closed by the daemon."));
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
    label = gtk_label_new (_("Check for upgrades every (minutes) :"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    combo_interval = gtk_combo_box_text_new_with_entry ();
    entry = gtk_bin_get_child (GTK_BIN (combo_interval));
    /* make sure only digits can be typed in */
    g_signal_connect (G_OBJECT (entry), "insert-text",
            G_CALLBACK (insert_text_cb), (gpointer) combo_interval);
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "0", _c("delay-in-minutes", "0 (Disabled)"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "15", "15");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "30", "30");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "60", _c("delay-in-minutes", "60 (Hour)"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "120", _c("delay-in-minutes", "120 (Two hours)"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),
            "1440", _c("delay-in-minutes", "1440 (Day)"));
    /* Note: config->interval actually is in seconds, not minutes */
    if (config->interval == 0)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 0);
    }
    else if (config->interval == 900 /* 15m */)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 1);
    }
    else if (config->interval == 1800 /* 30m */)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 2);
    }
    else if (config->interval == 3600 /* 60m/1h */)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 3);
    }
    else if (config->interval == 7200 /* 120m/2h */)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 4);
    }
    else if (config->interval == 86400 /* 1440m/1d */)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_interval), 5);
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

    button_skip = gtk_check_button_new_with_label (
            _c("skip-between", "Do not check between"));
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

    label = gtk_label_new (_c("skip-between", "and"));
    gtk_widget_set_margin_start (label, 5);
    gtk_widget_set_margin_end (label, 5);
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

    const gchar *chk_news        = _c("checks", "Arch Linux news");
    const gchar *chk_upgrades    = _c("checks", "Package upgrades");
    const gchar *chk_watched     = _c("checks", "Watched package upgrades");
    const gchar *chk_aur         = _c("checks", "AUR package upgrades");
    const gchar *chk_watched_aur = _c("checks", "AUR watched package upgrades");

    ++top;
    /* AutoChecks */
    label = gtk_label_new (_("During an automatic check, check for :"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    auto_news = gtk_check_button_new_with_label (chk_news);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_news),
            config->checks_auto & CHECK_NEWS);
    gtk_grid_attach (GTK_GRID (grid), auto_news, 1, top, 1, 1);
    gtk_widget_show (auto_news);
    ++top;
    auto_upgrades = gtk_check_button_new_with_label (chk_upgrades);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_upgrades),
            config->checks_auto & CHECK_UPGRADES);
    gtk_grid_attach (GTK_GRID (grid), auto_upgrades, 1, top, 1, 1);
    gtk_widget_show (auto_upgrades);
    ++top;
    auto_watched = gtk_check_button_new_with_label (chk_watched);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_watched),
            config->checks_auto & CHECK_WATCHED);
    gtk_grid_attach (GTK_GRID (grid), auto_watched, 1, top, 1, 1);
    gtk_widget_show (auto_watched);
    ++top;
    auto_aur = gtk_check_button_new_with_label (chk_aur);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_aur),
            config->checks_auto & CHECK_AUR);
    gtk_grid_attach (GTK_GRID (grid), auto_aur, 1, top, 1, 1);
    gtk_widget_show (auto_aur);
    ++top;
    auto_watched_aur = gtk_check_button_new_with_label (chk_watched_aur);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_watched_aur),
            config->checks_auto & CHECK_WATCHED_AUR);
    gtk_grid_attach (GTK_GRID (grid), auto_watched_aur, 1, top, 1, 1);
    gtk_widget_show (auto_watched_aur);

    ++top;
    /* ManualChecks */
    label = gtk_label_new (_("During a manual check, check for :"));
    gtk_widget_set_margin_top (label, 10);
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    manual_news = gtk_check_button_new_with_label (chk_news);
    gtk_widget_set_margin_top (manual_news, 10);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_news),
            config->checks_manual & CHECK_NEWS);
    gtk_grid_attach (GTK_GRID (grid), manual_news, 1, top, 1, 1);
    gtk_widget_show (manual_news);
    ++top;
    manual_upgrades = gtk_check_button_new_with_label (chk_upgrades);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_upgrades),
            config->checks_manual & CHECK_UPGRADES);
    gtk_grid_attach (GTK_GRID (grid), manual_upgrades, 1, top, 1, 1);
    gtk_widget_show (manual_upgrades);
    ++top;
    manual_watched = gtk_check_button_new_with_label (chk_watched);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_watched),
            config->checks_manual & CHECK_WATCHED);
    gtk_grid_attach (GTK_GRID (grid), manual_watched, 1, top, 1, 1);
    gtk_widget_show (manual_watched);
    ++top;
    manual_aur = gtk_check_button_new_with_label (chk_aur);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_aur),
            config->checks_manual & CHECK_AUR);
    gtk_grid_attach (GTK_GRID (grid), manual_aur, 1, top, 1, 1);
    gtk_widget_show (manual_aur);
    ++top;
    manual_watched_aur = gtk_check_button_new_with_label (chk_watched_aur);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_watched_aur),
            config->checks_manual & CHECK_WATCHED_AUR);
    gtk_grid_attach (GTK_GRID (grid), manual_watched_aur, 1, top, 1, 1);
    gtk_widget_show (manual_watched_aur);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ News ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new (_c("prefs-tab", "News"));

    label = gtk_label_new (_("Command line to open links :"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);

    cmdline_link_entry = gtk_entry_new ();
    gtk_widget_set_tooltip_markup (cmdline_link_entry,
            _("Use variable <b>$URL</b> for the URL to open"));
    if (config->cmdline_link != NULL)
    {
        gtk_entry_set_text (GTK_ENTRY (cmdline_link_entry), config->cmdline_link);
    }
    gtk_grid_attach (GTK_GRID (grid), cmdline_link_entry, 1, top, 3, 1);
    gtk_widget_show (cmdline_link_entry);

    ++top;
    add_template (grid, top, TPL_NEWS, CHECK_NEWS);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Upgrades ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new (_c("prefs-tab", "Upgrades"));

    /* CheckPacmanConflict */
    check_pacman_conflict = gtk_check_button_new_with_label (
            _("Check for pacman/kalu conflict"));
    gtk_widget_set_tooltip_text (check_pacman_conflict,
            _("Check whether an upgrade of pacman is likely to fail due to "
                "kalu's dependency, and if so adds a button on to notification "
                "to show a message about why and how to upgrade."));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_pacman_conflict),
            config->check_pacman_conflict);
    gtk_grid_attach (GTK_GRID (grid), check_pacman_conflict, 0, top, 4, 1);
    gtk_widget_show (check_pacman_conflict);

    ++top;
    /* UpgradeAction */
    button_upg_action = gtk_check_button_new_with_label (
            _("Show a button \"Upgrade system\" on notifications (and on kalu's menu)"));
    gtk_widget_set_tooltip_text (button_upg_action,
            _("Whether or not to show a button \"Upgrade system\" on notifications, "
                "as well as an item \"System upgrade\" on kalu's menu"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_upg_action),
            config->action != UPGRADE_NO_ACTION);
    gtk_grid_attach (GTK_GRID (grid), button_upg_action, 0, top, 4, 1);
    gtk_widget_show (button_upg_action);
    g_signal_connect (G_OBJECT (button_upg_action), "toggled",
            G_CALLBACK (upg_action_toggled_cb), NULL);

#ifndef DISABLE_UPDATER
    ++top;
    label = gtk_label_new (_("When clicking the button/menu :"));
    gtk_widget_set_tooltip_text (label,
            _("When clicking the button \"Upgrade system\" on notifications, "
                "or the menu \"System upgrade\""));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    upg_action_combo = gtk_combo_box_text_new ();
    gtk_widget_set_tooltip_text (upg_action_combo,
            _("When clicking the button \"Upgrade system\" on notifications, "
                "or the menu \"System upgrade\""));
    gtk_widget_set_sensitive (upg_action_combo,
            config->action != UPGRADE_NO_ACTION);
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (upg_action_combo),
            "1",
            _("Run kalu's system updater"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (upg_action_combo),
            "2",
            _("Run the specified command-line"));
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
    cmdline_label = gtk_label_new (_("Command-line:"));
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
            _("After completing a system upgrade, start the following :"),
            _("You can use <b>$PACKAGES</b> to be replaced by the list of upgraded packages; "
                "And <b>$PACFILES</b> to be replaced by the list of pacnew files "
                "(See manual for more information)"),
            _("Add a new command-line"),
            _("Edit selected command-line"),
            _("Remove selected command-line"),
            config->cmdline_post);
    gtk_widget_set_sensitive (cmdline_post_hbox,
            config->action != UPGRADE_NO_ACTION);
    ++top;
    /* ConfirmPostSysUpgrade */
    confirm_post = gtk_check_button_new_with_label (
            _("Ask confirmation before starting anything"));
    gtk_widget_set_tooltip_text (confirm_post,
            _("Confirmation will be asked before starting those processes. "
                "With multiple ones, you'll be able to select which one(s) to start."));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (confirm_post),
            config->confirm_post);
    gtk_grid_attach (GTK_GRID (grid), confirm_post, 0, top, 2, 1);
    gtk_widget_show (confirm_post);

    /* doing this now otherwise it's triggered with non-yet-existing widgets
     * to hide/show */
    g_signal_connect (G_OBJECT (upg_action_combo), "changed",
            G_CALLBACK (upg_action_changed_cb), NULL);
#endif

    ++top;
    add_template (grid, top, TPL_UPGRADES, CHECK_UPGRADES);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Watched ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new (_c("prefs-tab", "Watched"));

    button = gtk_button_new_with_label (_("Manage watched packages..."));
    gtk_widget_set_margin_top (button, 10);
    gtk_grid_attach (GTK_GRID (grid), button, 1, top, 2, 1);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_manage_watched_cb), GINT_TO_POINTER (FALSE));

    ++top;
    add_template (grid, top, TPL_WATCHED, CHECK_WATCHED);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ AUR ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new (_c("prefs-tab", "AUR"));

    /* CmdLine */
    button = gtk_check_button_new_with_label (
            _("Show a button \"Update AUR packages\" on notifications"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
            config->cmdline_aur != NULL);
    gtk_grid_attach (GTK_GRID (grid), button, 0, top, 4, 1);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT (button), "toggled",
            G_CALLBACK (aur_action_toggled_cb), NULL);

    ++top;
    label = gtk_label_new (_("When clicking the button, run the following :"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    aur_cmdline_entry = gtk_entry_new ();
    gtk_widget_set_tooltip_markup (aur_cmdline_entry,
            _("You can use <b>$PACKAGES</b> to be replaced by the list of AUR "
                "packages with upgrades available"));
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
    label = gtk_label_new (
            _("Do not check the AUR for the following packages :"));
    gtk_widget_set_margin_top (label, 10);
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 4, 1);
    gtk_widget_show (label);

    ++top;
    add_list (grid, top, &aur_ignore_store, &hbox,
            _("Package name"),
            NULL,
            _("Add a new package"),
            _("Edit selected package"),
            _("Remove selected package"),
            config->aur_ignore);

    /* template */
    ++top;
    add_template (grid, top, TPL_AUR, CHECK_AUR);
    top += 3; /* for template above */
    ++top;
    add_template (grid, top, TPL_AUR_NOT_FOUND, _CHECK_AUR_NOT_FOUND);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Watched AUR ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new (_c("prefs-tab", "Watched AUR"));

    button = gtk_button_new_with_label (_("Manage watched AUR packages..."));
    gtk_widget_set_margin_top (button, 10);
    gtk_grid_attach (GTK_GRID (grid), button, 1, top, 2, 1);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_manage_watched_cb), GINT_TO_POINTER (TRUE));

    ++top;
    add_template (grid, top, TPL_WATCHED_AUR, CHECK_WATCHED_AUR);

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* [ Misc ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new (_c("prefs-tab", "Misc"));

    syncdbs_in_tooltip = gtk_check_button_new_with_label (
            _("Show if databases can be synchronized in tooltip"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (syncdbs_in_tooltip),
            config->syncdbs_in_tooltip);
    gtk_grid_attach (GTK_GRID (grid), syncdbs_in_tooltip, 0, top, 2, 1);
    gtk_widget_show (syncdbs_in_tooltip);

    ++top;
    label = gtk_label_new (NULL);
    gtk_widget_set_margin_top (label, 10);
    gtk_label_set_markup (GTK_LABEL (label),
            _("When kalu is <b>active</b> :"));
    gtk_widget_set_tooltip_markup (label,
            _("Actions to be done when kalu is <b>not</b> paused."));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    add_on_click_actions (&top, grid, CLICK_SGL, FALSE);
    add_on_click_actions (&top, grid, CLICK_DBL, FALSE);
    add_on_click_actions (&top, grid, CLICK_MDL, FALSE);

    ++top;
    label = gtk_label_new (NULL);
    gtk_widget_set_margin_top (label, 23);
    gtk_label_set_markup (GTK_LABEL (label),
            _("When kalu is <b>paused</b> :"));
    gtk_widget_set_tooltip_markup (label,
            _("Actions to be done when kalu is <b>paused</b>."));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);

    add_on_click_actions (&top, grid, CLICK_SGL, TRUE);
    add_on_click_actions (&top, grid, CLICK_DBL, TRUE);
    add_on_click_actions (&top, grid, CLICK_MDL, TRUE);

#ifdef ENABLE_STATUS_NOTIFIER
    ++top;
    label = gtk_label_new (NULL);
    gtk_widget_set_margin_top (label, 42);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 80);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_markup (GTK_LABEL (label),
            _("Using KDE's StatusNotifierItem interface, the action on click "
                "will be mapped to Activate, the action on middle click mapped "
                "to SecondaryActivate, the action on double click is unused."));
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);
#endif

    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);

    /*******************************************/

    /* templates combo tooltip -- needs to be done after all pages have been
     * added */
    int tpl, fld;
    for (tpl = 0; tpl < _NB_TPL; ++tpl)
        for (fld = 0; fld < _NB_FLD; ++fld)
            set_combo_tooltip (tpl_widgets[tpl * 3 + fld].source, tpl, fld);

    /* buttons */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    GtkWidget *image;
    button = gtk_button_new_with_mnemonic (_("_Save preferences"));
    image = gtk_image_new_from_icon_name ("document-save", GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
    gtk_widget_set_tooltip_text (button, _("Apply and save preferences"));
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_save_cb), NULL);
    gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
    gtk_widget_show (button);

    button = gtk_button_new_with_mnemonic (_("_Close"));
    image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    g_signal_connect_swapped (G_OBJECT (button), "clicked",
            G_CALLBACK (gtk_widget_destroy), (gpointer) window);
    gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
    gtk_widget_show (button);

    /* signals */
    g_signal_connect (G_OBJECT (notebook), "switch-page",
            G_CALLBACK (switch_page_cb), NULL);
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
