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

/* C */
#include <string.h>

/* gtk */
#include <gtk/gtk.h>

/* kalu */
#include "kalu.h"
#include "preferences.h"
#include "util.h"

static GtkWidget *window                = NULL;
/* General */
static GtkWidget *filechooser           = NULL;
static GtkWidget *combo_interval        = NULL;
static GtkWidget *button_skip           = NULL;
static GtkWidget *spin_begin_hour       = NULL;
static GtkWidget *spin_begin_minute     = NULL;
static GtkWidget *spin_end_hour         = NULL;
static GtkWidget *spin_end_minute       = NULL;
static GtkWidget *auto_upgrades         = NULL;
static GtkWidget *auto_watched          = NULL;
static GtkWidget *auto_aur              = NULL;
static GtkWidget *auto_watched_aur      = NULL;
static GtkWidget *auto_news             = NULL;
static GtkWidget *manual_upgrades       = NULL;
static GtkWidget *manual_watched        = NULL;
static GtkWidget *manual_aur            = NULL;
static GtkWidget *manual_watched_aur    = NULL;
static GtkWidget *manual_news           = NULL;
/* Upgrades */
static GtkWidget *button_upg_action     = NULL;
static GtkWidget *upg_action_combo      = NULL;
static GtkWidget *cmdline_label         = NULL;
static GtkWidget *cmdline_entry         = NULL;
static GtkWidget *cmdline_post_hbox     = NULL;
static GtkListStore *cmdline_post_store = NULL;
static GtkWidget *upg_title_entry       = NULL;
static GtkWidget *upg_package_entry     = NULL;
static GtkWidget *upg_sep_entry         = NULL;

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
    gtk_widget_set_sensitive (upg_action_combo, is_active);
    gtk_widget_set_sensitive (cmdline_entry, is_active);
    gtk_widget_set_sensitive (cmdline_post_hbox, is_active);
}

static void
upg_action_changed_cb (GtkComboBox *combo, gpointer data _UNUSED_)
{
    int choice = gtk_combo_box_get_active (combo);
    if (choice == 0)
    {
        gtk_widget_hide (cmdline_label);
        gtk_widget_hide (cmdline_entry);
        gtk_widget_show (cmdline_post_hbox);
    }
    else if (choice == 1)
    {
        gtk_widget_show (cmdline_label);
        gtk_widget_show (cmdline_entry);
        gtk_widget_hide (cmdline_post_hbox);
    }
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
            (gpointer) insert_text_cb, combo);
        gtk_editable_insert_text (editable, text, length, position);
        g_signal_handlers_unblock_by_func (editable,
            (gpointer) insert_text_cb, combo);
    }
    
    /* this we added it already (if valid) stop the original addition */
    g_signal_stop_emission_by_name (editable, "insert_text");
}

static void
selection_changed_cb (GtkTreeSelection *selection, GtkWidget *buttons[2])
{
    gboolean has_selection;
    
    has_selection = (gtk_tree_selection_count_selected_rows (selection) > 0);
    gtk_widget_set_sensitive (buttons[0], has_selection);
    gtk_widget_set_sensitive (buttons[1], has_selection); 
}

static void
btn_postsysupgrade_add_cb (GtkButton *button _UNUSED_, GtkTreeView *tree)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeViewColumn *column;
    
    gtk_list_store_append (cmdline_post_store, &iter);
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (cmdline_post_store), &iter);
    column = gtk_tree_view_get_column (tree, 0);
    gtk_tree_view_set_cursor (tree, path, column, TRUE);
}

static void
btn_postsysupgrade_edit_cb (GtkButton *button _UNUSED_, GtkTreeView *tree)
{
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
btn_postsysupgrade_remove_cb (GtkButton *button _UNUSED_, GtkTreeView *tree)
{
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
renderer_edited_cb (GtkCellRendererText *renderer _UNUSED_, gchar *path,
                    gchar *text, gpointer data _UNUSED_)
{
    GtkTreeModel *model = GTK_TREE_MODEL (cmdline_post_store);
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
        char *s;
        gtk_tree_model_get (model, &iter, 0, &s, -1);
        free (s);
        gtk_list_store_set (cmdline_post_store, &iter, 0, text, -1);
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
    /* icon */
    GdkPixbuf *pixbuf;
    pixbuf = gtk_widget_render_icon_pixbuf (window, "kalu-logo", GTK_ICON_SIZE_DIALOG);
    gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
    g_object_unref (pixbuf);
    
    /* vbox */
    GtkWidget *vbox;
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);
    
    /* notebook */
    GtkWidget *notebook;
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
    char *s;
    
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
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser), config->pacmanconf);
    gtk_widget_show (filechooser);
    
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
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),   "15", "15");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),   "30", "30");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),   "60", "60 (Hour)");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval),  "120", "120 (Two hours)");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_interval), "1440", "1440 (Day)");
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
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_skip), config->has_skip);
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
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_begin_hour), config->skip_begin_hour);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_begin_minute), config->skip_begin_minute);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_end_hour), config->skip_end_hour);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_end_minute), config->skip_end_minute);
    }
    
    ++top;
    /* AutoChecks */
    label = gtk_label_new ("During an automatic check, check for :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);
    
    auto_news = gtk_check_button_new_with_label ("Arch Linux news");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_news),
        config->checks_auto & CHECK_NEWS);
    gtk_grid_attach (GTK_GRID (grid), auto_news, 1, top, 1, 1);
    gtk_widget_show (auto_news);
    ++top;
    auto_upgrades = gtk_check_button_new_with_label ("Package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_upgrades),
        config->checks_auto & CHECK_UPGRADES);
    gtk_grid_attach (GTK_GRID (grid), auto_upgrades, 1, top, 1, 1);
    gtk_widget_show (auto_upgrades);
    ++top;
    auto_watched = gtk_check_button_new_with_label ("Watched package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_watched),
        config->checks_auto & CHECK_WATCHED);
    gtk_grid_attach (GTK_GRID (grid), auto_watched, 1, top, 1, 1);
    gtk_widget_show (auto_watched);
    ++top;
    auto_aur = gtk_check_button_new_with_label ("AUR package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_aur),
        config->checks_auto & CHECK_AUR);
    gtk_grid_attach (GTK_GRID (grid), auto_aur, 1, top, 1, 1);
    gtk_widget_show (auto_aur);
    ++top;
    auto_watched_aur = gtk_check_button_new_with_label ("AUR watched package upgrades");
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
    
    manual_news = gtk_check_button_new_with_label ("Arch Linux news");
    gtk_widget_set_margin_top (manual_news, 10);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_news),
        config->checks_manual & CHECK_NEWS);
    gtk_grid_attach (GTK_GRID (grid), manual_news, 1, top, 1, 1);
    gtk_widget_show (manual_news);
    ++top;
    manual_upgrades = gtk_check_button_new_with_label ("Package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_upgrades),
        config->checks_manual & CHECK_UPGRADES);
    gtk_grid_attach (GTK_GRID (grid), manual_upgrades, 1, top, 1, 1);
    gtk_widget_show (manual_upgrades);
    ++top;
    manual_watched = gtk_check_button_new_with_label ("Watched package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_watched),
        config->checks_manual & CHECK_WATCHED);
    gtk_grid_attach (GTK_GRID (grid), manual_watched, 1, top, 1, 1);
    gtk_widget_show (manual_watched);
    ++top;
    manual_aur = gtk_check_button_new_with_label ("AUR package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_aur),
        config->checks_manual & CHECK_AUR);
    gtk_grid_attach (GTK_GRID (grid), manual_aur, 1, top, 1, 1);
    gtk_widget_show (manual_aur);
    ++top;
    manual_watched_aur = gtk_check_button_new_with_label ("AUR watched package upgrades");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (manual_watched_aur),
        config->checks_manual & CHECK_WATCHED_AUR);
    gtk_grid_attach (GTK_GRID (grid), manual_watched_aur, 1, top, 1, 1);
    gtk_widget_show (manual_watched_aur);
    
    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);
    
    /*******************************************/
    
    /* [ Upgrades ] */
    top = 0;
    grid = gtk_grid_new ();
    lbl_page = gtk_label_new ("Upgrades");
    
    /* UpgradeAction */
    button_upg_action = gtk_check_button_new_with_label ("Show a button \"Upgrade system\" on notifications");
    gtk_widget_set_size_request (button_upg_action, 420, -1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_upg_action),
        config->action != UPGRADE_NO_ACTION);
    gtk_grid_attach (GTK_GRID (grid), button_upg_action, 0, top, 4, 1);
    gtk_widget_show (button_upg_action);
    g_signal_connect (G_OBJECT (button_upg_action), "toggled",
                      G_CALLBACK (upg_action_toggled_cb), NULL);
    
    ++top;
    label = gtk_label_new ("When clicking the button :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 2, 1);
    gtk_widget_show (label);
    
    upg_action_combo = gtk_combo_box_text_new ();
    gtk_widget_set_sensitive (upg_action_combo, config->action != UPGRADE_NO_ACTION);
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (upg_action_combo), "1",
        "Run kalu's system updater");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (upg_action_combo), "2",
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
    
    ++top;
    /* CmdLine */
    cmdline_label = gtk_label_new ("Command-line:");
    gtk_grid_attach (GTK_GRID (grid), cmdline_label, 0, top, 2, 1);
    
    cmdline_entry = gtk_entry_new ();
    gtk_widget_set_sensitive (cmdline_entry, config->action != UPGRADE_NO_ACTION);
    gtk_entry_set_text (GTK_ENTRY (cmdline_entry), config->cmdline);
    gtk_grid_attach (GTK_GRID (grid), cmdline_entry, 2, top, 2, 1);
    
    if (config->action == UPGRADE_ACTION_CMDLINE)
    {
        gtk_widget_show (cmdline_label);
        gtk_widget_show (cmdline_entry);
    }
    
    ++top;
    /* PostSysUpgrade */
    GtkWidget *tree;
    cmdline_post_store = gtk_list_store_new (1, G_TYPE_STRING);
    tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (cmdline_post_store));
    gtk_widget_set_sensitive (tree, config->action != UPGRADE_NO_ACTION);
    g_object_unref (cmdline_post_store);
    
    /* vbox - sort of a toolbar but vertical */
    GtkWidget *vbox_tb;
    GtkWidget *button;
    GtkWidget *image;
    cmdline_post_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_grid_attach (GTK_GRID (grid), cmdline_post_hbox, 0, top, 4, 1);
    if (config->action != UPGRADE_ACTION_CMDLINE)
    {
        gtk_widget_show (cmdline_post_hbox);
    }
    
    vbox_tb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (cmdline_post_hbox), vbox_tb, FALSE, FALSE, 0);
    gtk_widget_show (vbox_tb);
    
    static GtkWidget *buttons[2];
    
    /* button Add */
    image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, "Add a new command-line");
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (btn_postsysupgrade_add_cb), (gpointer) tree);
    gtk_widget_show (button);
    /* button Edit */
    image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    buttons[0] = button;
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, "Edit selected command-line");
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (btn_postsysupgrade_edit_cb), (gpointer) tree);
    gtk_widget_show (button);
    /* button Remove */
    image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    buttons[1] = button;
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, "Remove selected command-line");
    gtk_box_pack_start (GTK_BOX (vbox_tb), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (btn_postsysupgrade_remove_cb), (gpointer) tree);
    gtk_widget_show (button);
    
    /* switch sensitive of buttons based on selection */
    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
    g_signal_connect (selection, "changed",
                      G_CALLBACK (selection_changed_cb), (gpointer) buttons);
    selection_changed_cb (selection, buttons);
    
    /* a scrolledwindow for the tree */
    GtkWidget *scrolled;
    scrolled = gtk_scrolled_window_new (
        gtk_tree_view_get_hadjustment (GTK_TREE_VIEW (tree)),
        gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (tree)));
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
        GTK_SHADOW_OUT);
    gtk_widget_show (scrolled);
    gtk_box_pack_start (GTK_BOX (cmdline_post_hbox), scrolled, TRUE, TRUE, 0);
    
    /* cell renderer & column(s) */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    /* column: Command-line */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
    g_signal_connect (G_OBJECT (renderer), "edited",
                      G_CALLBACK (renderer_edited_cb), NULL);
    column = gtk_tree_view_column_new_with_attributes (
        "After completing a system upgrade, ask whether to start the following :",
       renderer,
       "text", 0,
       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    
    /* eo.columns  */
    
    /* doing this now otherwise it's triggered with non-yet-existing widgets to hide/show */
    g_signal_connect (G_OBJECT (upg_action_combo), "changed",
                      G_CALLBACK (upg_action_changed_cb), NULL);
    
    /* fill data */
    GtkTreeIter iter;
    alpm_list_t *j;
    for (j = config->cmdline_post; j; j = alpm_list_next (j))
    {
        gtk_list_store_append (cmdline_post_store, &iter);
        gtk_list_store_set (cmdline_post_store, &iter,
            0,  strdup (j->data),
            -1);
    }
    
    gtk_container_add (GTK_CONTAINER (scrolled), tree);
    gtk_widget_show (tree);
    
    ++top;
    /* template */
    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), "<b>Notification template</b>");
    gtk_widget_set_margin_top (label, 15);
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 4, 1);
    gtk_widget_show (label);
    
    ++top;
    label = gtk_label_new ("Title :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);
    
    upg_title_entry = gtk_entry_new ();
    gtk_widget_set_tooltip_markup (upg_title_entry,
        "The following variables are available :\n"
        "- <b>$NB</b> : number of packages/news items\n"
        "- <b>$DL</b> : total download size\n"
        "- <b>$INS</b> : total installed size\n"
        "- <b>$NET</b> : total net (post-install difference) size\n"
        );
    s = escape_text (config->tpl_upgrades->title);
    gtk_entry_set_text (GTK_ENTRY (upg_title_entry), s);
    free (s);
    gtk_grid_attach (GTK_GRID (grid), upg_title_entry, 1, top, 3, 1);
    gtk_widget_show (upg_title_entry);
    
    ++top;
    label = gtk_label_new ("Package :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);
    
    upg_package_entry = gtk_entry_new ();
    gtk_widget_set_tooltip_markup (upg_package_entry,
        "The following variables are available :\n"
        "- <b>$PKG</b> : package name\n"
        "- <b>$OLD</b> : old/current version number\n"
        "- <b>$NEW</b> : new version number\n"
        "- <b>$DL</b> : download size\n"
        "- <b>$INS</b> : installed size\n"
        "- <b>$NET</b> : net (post-install difference) size\n"
        );
    s = escape_text (config->tpl_upgrades->package);
    gtk_entry_set_text (GTK_ENTRY (upg_package_entry), s);
    free (s);
    gtk_grid_attach (GTK_GRID (grid), upg_package_entry, 1, top, 3, 1);
    gtk_widget_show (upg_package_entry);
    
    ++top;
    label = gtk_label_new ("Separator :");
    gtk_grid_attach (GTK_GRID (grid), label, 0, top, 1, 1);
    gtk_widget_show (label);
    
    upg_sep_entry = gtk_entry_new ();
    gtk_widget_set_tooltip_text (upg_sep_entry, "No variables available.");
    s = escape_text (config->tpl_upgrades->sep);
    gtk_entry_set_text (GTK_ENTRY (upg_sep_entry), s);
    free (s);
    gtk_grid_attach (GTK_GRID (grid), upg_sep_entry, 1, top, 3, 1);
    gtk_widget_show (upg_sep_entry);
    
    
    /* add page */
    gtk_widget_show (grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, lbl_page);
    
    
    
    
    
    
    
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
