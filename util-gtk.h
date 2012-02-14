/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * util-gtk.h
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

#ifndef _KALU_UTIL_GTK_H
#define _KALU_UTIL_GTK_H

/* gtk */
#include <gtk/gtk.h>

void
rend_size (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
           GtkTreeModel *store, GtkTreeIter *iter, int col, int is_unsigned);

GtkWidget *
new_confirm (const gchar *message,
             const gchar *submessage,
             const gchar *btn_yes_label,
             const gchar *btn_yes_image,
             const gchar *btn_no_label,
             const gchar *btn_no_image,
             GtkWidget *window
             );

gboolean
confirm (const gchar *message,
         const gchar *submessage,
         const gchar *btn_yes_label,
         const gchar *btn_yes_image,
         const gchar *btn_no_label,
         const gchar *btn_no_image,
         GtkWidget *window
         );

void
show_error (const gchar *summary, const gchar *text, GtkWindow *parent);

void
notify_error (const gchar *summary, const gchar *text);

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
                 );

#endif /* _KALU_UTIL_GTK_H */
