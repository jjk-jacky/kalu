/**
 * kalu - Copyright (C) 2012-2014 Olivier Brunel
 *
 * gui.h
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


void free_notif (notif_t *notif);
void show_notif (notif_t *notif);

gboolean show_error_cmdline (gchar *arg[]);

void action_upgrade (NotifyNotification *notification,
        const char *action,
        gchar *_cmdline);
void action_watched (NotifyNotification *notification,
        char *action,
        notif_t *notif);
void action_watched_aur (NotifyNotification *notification,
        char *action,
        notif_t *notif);
void action_news (NotifyNotification *notification,
        char *action,
        notif_t *notif);

void notification_closed_cb (NotifyNotification *notification, gpointer data);

gboolean is_pacman_conflicting (alpm_list_t *packages);

void kalu_check (gboolean is_auto);
void kalu_auto_check (void);

void icon_popup_cb (GtkStatusIcon *_icon, guint button, guint activate_time,
               gpointer data);

void add_open_window (gpointer window);
void remove_open_window (gpointer window);

gboolean icon_press_cb (GtkStatusIcon *icon, GdkEventButton *event,
        gpointer data);

gboolean icon_query_tooltip_cb (GtkWidget *icon, gint x, gint y,
        gboolean keyboard_mode, GtkTooltip *tooltip, gpointer data);

void process_fifo_command (const gchar *command);

void set_kalpm_nb (check_t type, gint nb, gboolean update_icon);
void set_kalpm_nb_syncdbs (gint nb);
void set_kalpm_busy (gboolean busy);
void reset_timeout (void);
void skip_next_timeout (void);
gboolean reload_watched (gboolean is_aur, GError **error);

#endif /* _GUI_H */
