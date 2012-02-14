/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * util-gtk.c
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
#include <stdlib.h>
#include <string.h>

/* notify */
#include <libnotify/notify.h>

/* alpm list */
#include <alpm_list.h>

/* kalu */
#include "util-gtk.h"
#include "util.h"

static void renderer_toggle_cb (GtkCellRendererToggle *, gchar *, GtkTreeModel *);

void
rend_size (GtkTreeViewColumn *column _UNUSED_, GtkCellRenderer *renderer,
           GtkTreeModel *store, GtkTreeIter *iter, int col, int is_unsigned)
{
    GValue value = G_VALUE_INIT;
    double size;
    const char *unit;
    char buf[23];
    
    gtk_tree_model_get_value (store, iter, col, &value);
    if (is_unsigned)
    {
        size = humanize_size (g_value_get_uint (&value), '\0', &unit);
    }
    else
    {
        size = humanize_size (g_value_get_int (&value), '\0', &unit);
    }
    snprintf (buf, 23, "%.2f %s", size, unit);
    g_object_set (renderer, "text", buf, NULL);
    g_value_unset (&value);
}

GtkWidget *
new_confirm (const gchar *message,
             const gchar *submessage,
             const gchar *btn_yes_label,
             const gchar *btn_yes_image,
             const gchar *btn_no_label,
             const gchar *btn_no_image,
             GtkWidget *window
             )
{
    GtkWidget *dialog;
    GtkWidget *button;
    GtkWidget *image;
    
    if (NULL == submessage)
    {
        dialog = gtk_message_dialog_new_with_markup (
            GTK_WINDOW(window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_NONE,
            NULL);
        gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG(dialog), message);
    }
    else
    {
        dialog = gtk_message_dialog_new (
            GTK_WINDOW(window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_NONE,
            "%s",
            message);
        gtk_message_dialog_format_secondary_markup (
            GTK_MESSAGE_DIALOG(dialog),
            "%s",
            submessage);
    }
    
    gtk_window_set_decorated (GTK_WINDOW(dialog), FALSE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(dialog), TRUE);
    gtk_window_set_skip_pager_hint (GTK_WINDOW(dialog), TRUE);
    
    button = gtk_dialog_add_button(
        GTK_DIALOG(dialog),
        (NULL == btn_no_label) ? GTK_STOCK_NO : btn_no_label,
        GTK_RESPONSE_NO);
    image = gtk_image_new_from_stock (
        (NULL == btn_no_image) ? GTK_STOCK_NO : btn_no_image,
        GTK_ICON_SIZE_MENU);
    gtk_button_set_image( GTK_BUTTON(button), image);

    button = gtk_dialog_add_button(
        GTK_DIALOG(dialog),
        (NULL == btn_yes_label) ? GTK_STOCK_YES : btn_yes_label,
        GTK_RESPONSE_YES);
    image = gtk_image_new_from_stock (
        (NULL == btn_yes_image) ? GTK_STOCK_YES : btn_yes_image,
        GTK_ICON_SIZE_MENU);
    gtk_button_set_image( GTK_BUTTON(button), image);
    
    return dialog;
}

gboolean
confirm (const gchar *message,
         const gchar *submessage,
         const gchar *btn_yes_label,
         const gchar *btn_yes_image,
         const gchar *btn_no_label,
         const gchar *btn_no_image,
         GtkWidget *window
         )
{
    GtkWidget  *dialog;
    gint        rc;
    
    dialog = new_confirm (message, submessage, btn_yes_label, btn_yes_image,
                          btn_no_label, btn_no_image, window);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);
    rc = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return rc == GTK_RESPONSE_YES;
}

void
show_error (const gchar *message, const gchar *submessage, GtkWindow *parent)
{
    GtkWidget *dialog;
    
    if (NULL == submessage)
    {
        dialog = gtk_message_dialog_new_with_markup (
            parent,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            NULL);
        gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG(dialog), message);
    }
    else
    {
        dialog = gtk_message_dialog_new (
            parent,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "%s",
            message);
        gtk_message_dialog_format_secondary_markup (
            GTK_MESSAGE_DIALOG(dialog),
            "%s",
            submessage);
    }
    
    gtk_window_set_decorated (GTK_WINDOW(dialog), FALSE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(dialog), TRUE);
    gtk_window_set_skip_pager_hint (GTK_WINDOW(dialog), TRUE);
    
    g_signal_connect (G_OBJECT (dialog), "response",
                      G_CALLBACK (gtk_widget_destroy), NULL);
    gtk_widget_show (dialog);
}

void
notify_error (const gchar *summary, const gchar *text)
{
    NotifyNotification *notification;
    GtkWidget          *w;
    GdkPixbuf          *pixbuf;
    
    notification = notify_notification_new (summary,
                                            text,
                                            NULL);
    w = gtk_label_new (NULL);
    g_object_ref_sink (w);
    pixbuf = gtk_widget_render_icon_pixbuf (w, "kalu-logo", GTK_ICON_SIZE_BUTTON);
    notify_notification_set_image_from_pixbuf (notification, pixbuf);
    g_object_unref (pixbuf);
    gtk_widget_destroy (w);
    g_object_unref (w);
    notify_notification_show (notification, NULL);
    g_object_unref (notification);
}

static void
renderer_toggle_cb (GtkCellRendererToggle *renderer _UNUSED_, gchar *path, GtkTreeModel *model)
{
    GtkTreeIter iter;
    gboolean run;
    
    gtk_tree_model_get_iter_from_string (model, &iter, path);
    gtk_tree_model_get (model, &iter, 0, &run, -1);
    run = !run;
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, run, -1);
}

alpm_list_t *
confirm_choices (const gchar *message,
                 const gchar *submessage,
                 const gchar *btn_yes_label,
                 const gchar *btn_yes_image,
                 const gchar *btn_no_label,
                 const gchar *btn_no_image,
                 const gchar *chk_label,
                 const gchar *choice_label,
                 alpm_list_t *choices,
                 GtkWidget *window
                 )
{
    GtkWidget *dialog;
    int rc;
    alpm_list_t *choices_selected = NULL;
    
    dialog = new_confirm (message, submessage, btn_yes_label, btn_yes_image,
        btn_no_label, btn_no_image, window);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);
    
    /* content area: where we'll add our list */
    GtkWidget *box;
    box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    
    /* liststore for the list */
    GtkListStore *store;
    store = gtk_list_store_new (2, G_TYPE_BOOLEAN, G_TYPE_STRING);
    
    /* said list */
    GtkWidget *list;
    list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    g_object_unref (store);

    /* a scrolledwindow for the list */
    GtkWidget *scrolled;
    scrolled = gtk_scrolled_window_new (
        gtk_tree_view_get_hadjustment (GTK_TREE_VIEW (list)),
        gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (list)));
    gtk_box_pack_start (GTK_BOX (box), scrolled, TRUE, TRUE, 0);
    gtk_widget_show (scrolled);
    
    /* cell renderer & column(s) */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    /* column: Run */
    renderer = gtk_cell_renderer_toggle_new ();
    g_signal_connect (G_OBJECT (renderer), "toggled",
                      G_CALLBACK (renderer_toggle_cb), (gpointer) store);
    column = gtk_tree_view_column_new_with_attributes (chk_label,
                                                       renderer,
                                                       "active", 0,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    /* column: Package */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (choice_label,
                                                       renderer,
                                                       "text", 1,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
    
    /* eo.columns  */
    
    /* fill data */
    GtkTreeIter iter;
    alpm_list_t *i;
    for (i = choices; i; i = alpm_list_next (i))
    {
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
            0,  TRUE,
            1,  i->data,
            -1);
    }
    
    gtk_container_add (GTK_CONTAINER (scrolled), list);
    gtk_widget_show (list);

    /* show it */
    rc = gtk_dialog_run (GTK_DIALOG (dialog));
    if (rc == GTK_RESPONSE_YES)
    {
        if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
        {
            gboolean is_selected;
            const gchar *choice;
            while (1)
            {
                gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                    0,  &is_selected,
                    1,  &choice,
                    -1);
                if (is_selected)
                {
                    choices_selected = alpm_list_add (choices_selected, strdup (choice));
                }
                if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter))
                {
                    break;
                }
            }
        }
    }
    gtk_widget_destroy (dialog);
    return choices_selected;
}
