/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * gui.h
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

#ifndef _GUI_H
#define _GUI_H

/* C */
#include <string.h>

/* alpm */
#include <alpm_list.h>

/* gtk */
#include <gtk/gtk.h>

/* notify */
#include <libnotify/notify.h>

#define FREE_NOTIFS_LIST(p)                                                 \
            do                                                              \
            {                                                               \
                alpm_list_free_inner (p, (alpm_list_fn_free) free_notif);   \
                alpm_list_free (p);                                         \
                p = NULL;                                                   \
            } while(0)


void
free_notif (notif_t *notif);

void
show_notif (notif_t *notif);

gboolean
show_error_cmdline (gchar *arg[]);

void
action_upgrade (NotifyNotification *notification, const char *action, gchar *_cmdline);

void
action_watched (NotifyNotification *notification, char *action, notif_t *notif);

void
action_watched_aur (NotifyNotification *notification, char *action, notif_t *notif);

void
action_news (NotifyNotification *notification, char *action, notif_t *notif);

void
notification_closed_cb (NotifyNotification *notification, gpointer data);

void
kalu_check (gboolean is_auto);

void
kalu_auto_check (void);

gboolean
is_pacman_conflicting (alpm_list_t *packages);

void
icon_popup_cb (GtkStatusIcon *_icon, guint button, guint activate_time,
               gpointer data);

gboolean
icon_press_cb (GtkStatusIcon *icon, GdkEventButton *event, gpointer data);

gboolean
icon_query_tooltip_cb (GtkWidget *icon, gint x, gint y, gboolean keyboard_mode,
                       GtkTooltip *tooltip, gpointer data);

#endif /* _GUI_H */
