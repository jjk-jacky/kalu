/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * gui.c
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

/* kalu */
#include "kalu.h"
#include "gui.h"
#include "util-gtk.h"
#include "watched.h"
#include "preferences.h"
#include "conf.h"
#include "news.h"
#ifndef DISABLE_UPDATER
#include "kalu-updater.h"
#include "updater.h"
#endif

#ifndef DISABLE_UPDATER
#define run_updater()   do {                \
        set_kalpm_busy (TRUE);              \
        updater_run (config->pacmanconf,    \
                     config->cmdline_post); \
    } while (0)
#endif

static void menu_check_cb (GtkMenuItem *item, gpointer data);
static void menu_quit_cb (GtkMenuItem *item, gpointer data);

extern kalpm_state_t kalpm_state;


void
free_notif (notif_t *notif)
{
    if (!notif)
    {
        return;
    }
    
    free (notif->summary);
    free (notif->text);
    if (notif->type == CHECK_NEWS || notif->type == CHECK_AUR)
    {
        /* CHECK_NEWS has xml_news; CHECK_AUR has cmdline w/ $PACKAGES replaced */
        free (notif->data);
    }
    else
    {
        /* CHECK_UPGRADES, CHECK_WATCHED & CHECK_WATCHED_AUR all use packages */
        FREE_PACKAGE_LIST (notif->data);
    }
    free (notif);
}

void
show_notif (notif_t *notif)
{
    NotifyNotification *notification;
    
    debug ("showing notif: %s", notif->summary);
    notification = new_notification (notif->summary, notif->text);
    if (!notif->data)
    {
        /* no data means the notification was modified afterwards, as news/packages
         * have been marked read. No more data/action button, just a simple
         * notification (where text explains to re-do checks to be up to date) */
    }
    else if (notif->type & CHECK_UPGRADES)
    {
        if (   config->check_pacman_conflict
            && is_pacman_conflicting ((alpm_list_t *) notif->data))
        {
            notify_notification_add_action (notification, "do_conflict_warn",
                "Possible pacman/kalu conflict...",
                (NotifyActionCallback) show_pacman_conflict,
                NULL, NULL);
        }
        if (config->action != UPGRADE_NO_ACTION)
        {
            notify_notification_add_action (notification, "do_updates",
                "Update system...", (NotifyActionCallback) action_upgrade,
                NULL, NULL);
        }
    }
    else if (notif->type & CHECK_AUR)
    {
        notify_notification_add_action (notification,
                                        "do_updates_aur",
                                        "Update AUR packages...",
                                        (NotifyActionCallback) action_upgrade,
                                        notif->data,
                                        NULL);
    }
    else if (notif->type & CHECK_WATCHED)
    {
        notify_notification_add_action (notification,
                                        "mark_watched",
                                        "Mark packages...",
                                        (NotifyActionCallback) action_watched,
                                        notif,
                                        NULL);
    }
    else if (notif->type & CHECK_WATCHED_AUR)
    {
        notify_notification_add_action (notification,
                                        "mark_watched_aur",
                                        "Mark packages...",
                                        (NotifyActionCallback) action_watched_aur,
                                        notif,
                                        NULL);
    }
    else if (notif->type & CHECK_NEWS)
    {
        notify_notification_add_action (notification,
                                        "mark_news",
                                        "Show news...",
                                        (NotifyActionCallback) action_news,
                                        notif,
                                        NULL);
    }
    /* we use a callback on "closed" to unref it, because when there's an action
     * we need to keep a ref, otherwise said action won't work */
    g_signal_connect (G_OBJECT (notification), "closed",
                      G_CALLBACK (notification_closed_cb), NULL);
    notify_notification_show (notification, NULL);
}

gboolean
show_error_cmdline (gchar *arg[])
{
    GtkWidget *dialog;
    
    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_OK,
                                     "%s",
                                     "Unable to start process");
    gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
            "Error while trying to run command line: %s\n\nThe error was: <b>%s</b>",
            arg[0], arg[1]);
    gtk_window_set_title (GTK_WINDOW (dialog), "kalu: Unable to start process");
    gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), FALSE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    free (arg[0]);
    free (arg[1]);
    free (arg);
    return FALSE;
}

static void
run_cmdline (char *cmdline)
{
    GError *error = NULL;
    
    set_kalpm_busy (TRUE);
    debug ("run cmdline: %s", cmdline);
    if (!g_spawn_command_line_sync (cmdline, NULL, NULL, NULL, &error))
    {
        /* we can't just show the error message from here, because this is ran
         * from another thread, hence no gtk_* functions can be used. */
        char **arg;
        arg = malloc (2 * sizeof (char *));
        arg[0] = strdup (cmdline);
        arg[1] = strdup (error->message);
        g_main_context_invoke (NULL, (GSourceFunc) show_error_cmdline, (gpointer) arg);
    }
    set_kalpm_busy (FALSE);
    if (G_UNLIKELY (error))
    {
        g_clear_error (&error);
    }
    else
    {
        /* check again, to refresh the state (since an upgrade was probably just done) */
        kalu_check (TRUE);
    }
    
    if (cmdline != config->cmdline && cmdline != config->cmdline_aur)
    {
        free (cmdline);
    }
}

void
action_upgrade (NotifyNotification *notification, const char *action, gchar *_cmdline)
{
    /* we need to strdup cmdline because it will be free-d when notification
     * is destroyed, which won't happen until this function is done, however it
     * could happen before the other thread (run_cmdline) starts ! */
    char *cmdline = (_cmdline) ? strdup (_cmdline) : NULL;
    
    notify_notification_close (notification, NULL);
    
    if (!cmdline)
    {
        if (strcmp ("do_updates", action) == 0)
        {
            #ifndef DISABLE_UPDATER
            if (config->action == UPGRADE_ACTION_KALU)
            {
                run_updater ();
            }
            else /* if (config->action == UPGRADE_ACTION_CMDLINE) */
            {
            #endif
                cmdline = config->cmdline;
            #ifndef DISABLE_UPDATER
            }
            #endif
        }
        else /* if (strcmp ("do_updates_aur", action) == 0) */
        {
            cmdline = config->cmdline_aur;
        }
    }
    
    if (cmdline)
    {
        /* run in a separate thread, to not block/make GUI unresponsive */
        g_thread_unref (g_thread_try_new ("action_upgrade",
                                          (GThreadFunc) run_cmdline,
                                          (gpointer) cmdline,
                                          NULL));
    }
}

void
action_watched (NotifyNotification *notification, char *action _UNUSED_,
    notif_t *notif)
{
    notify_notification_close (notification, NULL);
    if (notif->data)
    {
        watched_update ((alpm_list_t *) notif->data, FALSE);
    }
    else
    {
        show_error ("Unable to mark watched packages",
            "Watched packages have changed, "
                "you need to run the checks again to be up-to-date.",
            NULL);
    }
}

void
action_watched_aur (NotifyNotification *notification, char *action _UNUSED_,
    notif_t *notif)
{
    notify_notification_close (notification, NULL);
    if (notif->data)
    {
        watched_update ((alpm_list_t *) notif->data, TRUE);
    }
    else
    {
        show_error ("Unable to mark watched AUR packages",
            "Watched AUR packages have changed, "
                "you need to run the checks again to be up-to-date.",
            NULL);
    }
}

void
action_news (NotifyNotification *notification, char *action _UNUSED_,
    notif_t *notif)
{
    GError *error = NULL;
    
    notify_notification_close (notification, NULL);
    if (notif->data)
    {
        set_kalpm_busy (TRUE);
        if (!news_show ((gchar *) notif->data, TRUE, &error))
        {
            show_error ("Unable to show the news", error->message, NULL);
            g_clear_error (&error);
        }
    }
    else
    {
        show_error ("Unable to show unread news",
            "Read news have changed, you need to run the checks again to be up-to-date.",
            NULL);
    }
}

void
notification_closed_cb (NotifyNotification *notification, gpointer data _UNUSED_)
{
    g_object_unref (notification);
}

gboolean
is_pacman_conflicting (alpm_list_t *packages)
{
    gboolean ret = FALSE;
    alpm_list_t *i;
    kalu_package_t *pkg;
    char *s, *ss, *old, *new, *so, *sn;
    
    for (i = packages; i; i = alpm_list_next (i))
    {
        pkg = i->data;
        if (strcmp ("pacman", pkg->name) == 0)
        {
            /* because we'll mess with it */
            old = strdup (pkg->old_version);
            /* locate begining of (major) version number (might have epoch: before) */
            s = strchr (old, ':');
            if (s)
            {
                so = s + 1;
            }
            else
            {
                so = old;
            }
            
            s = strrchr (so, '-');
            if (!s)
            {
                /* should not be possible */
                free (old);
                break;
            }
            *s = '.';
            
            /* because we'll mess with it */
            new = strdup (pkg->new_version);
            /* locate begining of (major) version number (might have epoch: before) */
            s = strchr (new, ':');
            if (s)
            {
                sn = s + 1;
            }
            else
            {
                sn = new;
            }
            
            s = strrchr (sn, '-');
            if (!s)
            {
                /* should not be possible */
                free (old);
                free (new);
                break;
            }
            *s = '.';
            
            int nb = 0; /* to know which part (major/minor) we're dealing with */
            while ((s = strchr (so, '.')) && (ss = strchr (sn, '.')))
            {
                *s = '\0';
                *ss = '\0';
                ++nb;
                
                /* if major or minor goes up, API changes is likely and kalu's
                 * dependency will kick in */
                if (atoi (sn) > atoi (so))
                {
                    ret = TRUE;
                    break;
                }
                
                /* if nb is 2 this was the minor number, past this we don't care */
                if (nb == 2)
                {
                    break;
                }
                so = s + 1;
                sn = ss + 1;
            }
            
            free (old);
            free (new);
            break;
        }
    }
    
    return ret;
}

inline void
kalu_check (gboolean is_auto)
{
    /* in case e.g. the menu was shown (sensitive) before an auto-check started */
    if (kalpm_state.is_busy)
    {
        return;
    }
    set_kalpm_busy (TRUE);
    
    /* run in a separate thread, to not block/make GUI unresponsive */
    g_thread_unref (g_thread_try_new ("kalu_check_work",
                                      (GThreadFunc) kalu_check_work,
                                      GINT_TO_POINTER (is_auto),
                                      NULL));
}

void
kalu_auto_check (void)
{
    kalu_check (TRUE);
}

static void
show_last_notifs (void)
{
    alpm_list_t *i;
    
    /* in case e.g. the menu was shown (sensitive) before an auto-check started */
    if (kalpm_state.is_busy)
    {
        return;
    }
    
    if (!config->last_notifs)
    {
        notif_t notif;
        
        notif.type = 0;
        notif.summary = (gchar *) "No notifications to show.";
        notif.text = NULL;
        notif.data = NULL;
        
        show_notif (&notif);
        return;
    }
    
    for (i = config->last_notifs; i; i = alpm_list_next (i))
    {
        show_notif ((notif_t *) i->data);
    }
}

static void
menu_check_cb (GtkMenuItem *item _UNUSED_, gpointer data _UNUSED_)
{
    kalu_check (FALSE);
}

static void
menu_quit_cb (GtkMenuItem *item _UNUSED_, gpointer data _UNUSED_)
{
    /* in case e.g. the menu was shown (sensitive) before an auto-check started */
    if (kalpm_state.is_busy)
    {
        return;
    }
    gtk_main_quit ();
}

static void
menu_manage_cb (GtkMenuItem *item _UNUSED_, gboolean is_aur)
{
    watched_manage (is_aur);
}

static inline void
kalu_sysupgrade (void)
{
    /* in case e.g. the menu was shown (sensitive) before an auto-check started */
    if (kalpm_state.is_busy || config->action == UPGRADE_NO_ACTION)
    {
        return;
    }
    
    #ifndef DISABLE_UPDATER
    if (config->action == UPGRADE_ACTION_KALU)
    {
        run_updater ();
    }
    else /* if (config->action == UPGRADE_ACTION_CMDLINE) */
    {
    #endif
        /* run in a separate thread, to not block/make GUI unresponsive */
        g_thread_unref (g_thread_try_new ("cmd_sysupgrade",
                                          (GThreadFunc) run_cmdline,
                                          (gpointer) config->cmdline,
                                          NULL));
    #ifndef DISABLE_UPDATER
    }
    #endif
}

static void
menu_news_cb (GtkMenuItem *item _UNUSED_, gpointer data _UNUSED_)
{
    GError *error = NULL;
    /* in case e.g. the menu was shown (sensitive) before an auto-check started */
    if (kalpm_state.is_busy)
    {
        return;
    }
    set_kalpm_busy (TRUE);
    
    if (!news_show (NULL, FALSE, &error))
    {
        show_error ("Unable to show the recent Arch Linux news", error->message, NULL);
        g_clear_error (&error);
    }
}

static void
menu_help_cb (GtkMenuItem *item _UNUSED_, gpointer data _UNUSED_)
{
    GError *error = NULL;
    
    if (!show_help (&error))
    {
        show_error ("Unable to show help", error->message, NULL);
        g_clear_error (&error);
    }
}

static void
menu_history_cb (GtkMenuItem *item _UNUSED_, gpointer data _UNUSED_)
{
    GError *error = NULL;
    
    if (!show_history (&error))
    {
        show_error ("Unable to show change log", error->message, NULL);
        g_clear_error (&error);
    }
}

static void
menu_prefs_cb (GtkMenuItem *item _UNUSED_, gpointer data _UNUSED_)
{
    show_prefs ();
}

static void
menu_about_cb (GtkMenuItem *item _UNUSED_, gpointer data _UNUSED_)
{
    GtkAboutDialog *about;
    GdkPixbuf *pixbuf;
    const char *authors[] = {"Olivier Brunel", "Dave Gamble", "Pacman Development Team", NULL};
    const char *artists[] = {"Painless Rob", NULL};
    
    about = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());
    pixbuf = gtk_widget_render_icon_pixbuf (GTK_WIDGET (about), "kalu-logo",
                                            GTK_ICON_SIZE_DIALOG);
    gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
    gtk_about_dialog_set_logo (about, pixbuf);
    g_object_unref (G_OBJECT (pixbuf));
    
    gtk_about_dialog_set_program_name (about, PACKAGE_NAME);
    gtk_about_dialog_set_version (about, PACKAGE_VERSION);
    gtk_about_dialog_set_comments (about, PACKAGE_TAG);
    gtk_about_dialog_set_website (about, "https://bitbucket.org/jjacky/kalu");
    gtk_about_dialog_set_website_label (about, "https://bitbucket.org/jjacky/kalu");
    gtk_about_dialog_set_copyright (about, "Copyright (C) 2012 Olivier Brunel");
    gtk_about_dialog_set_license_type (about, GTK_LICENSE_GPL_3_0);
    gtk_about_dialog_set_authors (about, authors);
    gtk_about_dialog_set_artists (about, artists);
    
    gtk_dialog_run (GTK_DIALOG (about));
    gtk_widget_destroy (GTK_WIDGET (about));
}

static gboolean
menu_unmap_cb (GtkWidget *menu, GdkEvent *event _UNUSED_, gpointer data _UNUSED_)
{
    gtk_widget_destroy (menu);
    g_object_unref (menu);
    return TRUE;
}

void
icon_popup_cb (GtkStatusIcon *_icon _UNUSED_, guint button, guint activate_time,
               gpointer data _UNUSED_)
{
    GtkWidget   *menu;
    GtkWidget   *item;
    GtkWidget   *image;
    guint        pos = 0;
    
    menu = gtk_menu_new ();
    
    item = gtk_image_menu_item_new_with_label ("Re-show last notifications...");
    gtk_widget_set_sensitive (item, !kalpm_state.is_busy && config->last_notifs);
    image = gtk_image_new_from_stock (GTK_STOCK_REDO, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_widget_set_tooltip_text (item, "Show notifications from last ran checks");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (show_last_notifs), NULL);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_with_label ("Check for Upgrades...");
    gtk_widget_set_sensitive (item, !kalpm_state.is_busy);
    image = gtk_image_new_from_stock ("kalu-logo", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_widget_set_tooltip_text (item, "Check if there are any upgrades available");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_check_cb), NULL);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    if (config->action != UPGRADE_NO_ACTION)
    {
        item = gtk_image_menu_item_new_with_label ("System upgrade...");
        gtk_widget_set_sensitive (item, !kalpm_state.is_busy);
        image = gtk_image_new_from_stock ("kalu-logo", GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
        gtk_widget_set_tooltip_text (item, "Perform a system upgrade");
        g_signal_connect (G_OBJECT (item), "activate",
                          G_CALLBACK (kalu_sysupgrade), NULL);
        gtk_widget_show (item);
        gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    }
    
    item = gtk_image_menu_item_new_with_label ("Show recent Arch Linux news...");
    gtk_widget_set_sensitive (item, !kalpm_state.is_busy);
    image = gtk_image_new_from_stock ("kalu-logo", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_widget_set_tooltip_text (item, "Show 10 most recent Arch Linux news");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_news_cb), NULL);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_with_label ("Manage watched packages...");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_manage_cb), (gpointer) FALSE);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_with_label ("Manage watched AUR packages...");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_manage_cb), (gpointer) TRUE);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES, NULL);
    gtk_widget_set_tooltip_text (item, "Edit preferences");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_prefs_cb), NULL);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_HELP, NULL);
    gtk_widget_set_tooltip_text (item, "Show help (man page)");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_help_cb), NULL);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_with_label ("Change log");
    gtk_widget_set_tooltip_text (item, "Show change log");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_history_cb), (gpointer) TRUE);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
    gtk_widget_set_tooltip_text (item, "Show Copyright & version information");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_about_cb), NULL);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
    gtk_widget_set_sensitive (item, !kalpm_state.is_busy);
    gtk_widget_set_tooltip_text (item, "Exit kalu");
    g_signal_connect (G_OBJECT (item), "activate",
                      G_CALLBACK (menu_quit_cb), NULL);
    gtk_widget_show (item);
    gtk_menu_attach (GTK_MENU (menu), item, 0, 1, pos, pos + 1); ++pos;
    
    /* since we don't pack the menu anywhere, we need to "take ownership" of it,
     * and we'll destroy it when done, i.e. when it's unmapped */
    g_object_ref_sink (menu);
    gtk_widget_add_events (menu, GDK_STRUCTURE_MASK);
    g_signal_connect (G_OBJECT (menu), "unmap-event",
                      G_CALLBACK (menu_unmap_cb), NULL);
    
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, activate_time);
}

GPtrArray *open_windows = NULL;
static gboolean has_hidden_windows = FALSE;

void
add_open_window (gpointer window)
{
    if (!open_windows)
    {
        open_windows = g_ptr_array_new ();
    }
    g_ptr_array_add (open_windows, window);
}

void
remove_open_window (gpointer window)
{
    g_ptr_array_remove (open_windows, window);
}

static inline void
toggle_open_windows (void)
{
    if (!open_windows || open_windows->len == 0)
    {
        return;
    }
    
    g_ptr_array_foreach (open_windows,
                         (GFunc) ((has_hidden_windows) ? gtk_widget_show : gtk_widget_hide),
                         NULL);
    has_hidden_windows = !has_hidden_windows;
}

static guint icon_press_timeout = 0;

#define process_click_action(on_click)  do {        \
        if (on_click == DO_SYSUPGRADE)              \
        {                                           \
            kalu_sysupgrade ();                     \
        }                                           \
        else if (on_click == DO_CHECK)              \
        {                                           \
            kalu_check (FALSE);                     \
        }                                           \
        else if (on_click == DO_TOGGLE_WINDOWS)     \
        {                                           \
            toggle_open_windows ();                 \
        }                                           \
        else if (on_click == DO_LAST_NOTIFS)        \
        {                                           \
            show_last_notifs ();                    \
        }                                           \
    } while (0)

static gboolean
icon_press_click (gpointer data _UNUSED_)
{
    icon_press_timeout = 0;
    
    process_click_action (config->on_sgl_click);
    
    return FALSE;
}

gboolean
icon_press_cb (GtkStatusIcon *icon _UNUSED_, GdkEventButton *event, gpointer data _UNUSED_)
{
    /* left button? */
    if (event->button == 1)
    {
        if (event->type == GDK_2BUTTON_PRESS)
        {
            /* we probably had a timeout set for the click, remove it */
            if (icon_press_timeout > 0)
            {
                g_source_remove (icon_press_timeout);
                icon_press_timeout = 0;
            }
            
            process_click_action (config->on_dbl_click);
        }
        else if (event->type == GDK_BUTTON_PRESS)
        {
            /* As per GTK manual: on a dbl-click, we get GDK_BUTTON_PRESS twice
             * and then a GDK_2BUTTON_PRESS. Obviously, we want then to ignore
             * the two GDK_BUTTON_PRESS.
             * Also per manual, for a double click to occur, the second button
             * press must occur within 1/4 of a second of the first; so:
             * - on GDK_BUTTON_PRESS we set a timeout, in 250ms
             *  - if it expires/gets triggered, it was a click
             *  - if another click happens within that time, the timeout will be
             *    removed (see GDK_2BUTTON_PRESS above) and the clicks ignored
             * - if a GDK_BUTTON_PRESS occurs while a timeout is set, it's a
             * second click and ca be ignored, GDK_2BUTTON_PRESS will handle it */
            if (icon_press_timeout == 0)
            {
                icon_press_timeout = g_timeout_add (250, icon_press_click, NULL);
            }
        }
    }
    
    return FALSE;
}

#undef process_click_action

#define addstr(...)     do {                            \
        len = snprintf (s, (size_t) max, __VA_ARGS__);  \
        max -= len;                                     \
        s += len;                                       \
    } while (0)
gboolean
icon_query_tooltip_cb (GtkWidget *icon _UNUSED_, gint x _UNUSED_, gint y _UNUSED_,
                       gboolean keyboard_mode _UNUSED_, GtkTooltip *tooltip,
                       gpointer data _UNUSED_)
{
    GDateTime *current;
    GTimeSpan timespan;
    gint nb;
    gchar buf[420], *s = buf;
    gint max = 420, len;
    
    addstr ("[kalu%s]", (has_hidden_windows) ? " +" : "");
    
    if (kalpm_state.is_busy)
    {
        addstr (" Checking/updating in progress...");
        gtk_tooltip_set_text (tooltip, buf);
        return TRUE;
    }
    else if (kalpm_state.last_check == NULL)
    {
        gtk_tooltip_set_text (tooltip, buf);
        return TRUE;
    }
    
    addstr (" Last checked ");
    
    current = g_date_time_new_now_local ();
    timespan = g_date_time_difference (current, kalpm_state.last_check);
    g_date_time_unref (current);
    
    if (timespan < G_TIME_SPAN_MINUTE)
    {
        addstr ("just now");
    }
    else
    {
        if (timespan >= G_TIME_SPAN_DAY)
        {
            nb = (gint) (timespan / G_TIME_SPAN_DAY);
            timespan -= (nb * G_TIME_SPAN_DAY);
            if (nb > 1)
            {
                addstr ("%d days ", nb);
            }
            else
            {
                addstr ("1 day ");
            }
        }
        if (timespan >= G_TIME_SPAN_HOUR)
        {
            nb = (gint) (timespan / G_TIME_SPAN_HOUR);
            timespan -= (nb * G_TIME_SPAN_HOUR);
            if (nb > 1)
            {
                addstr ("%d hours ", nb);
            }
            else
            {
                addstr ("1 hour ");
            }
        }
        if (timespan >= G_TIME_SPAN_MINUTE)
        {
            nb = (gint) (timespan / G_TIME_SPAN_MINUTE);
            timespan -= (nb * G_TIME_SPAN_MINUTE);
            if (nb > 1)
            {
                addstr ("%d minutes ", nb);
            }
            else
            {
                addstr ("1 minute ");
            }
        }
        
        addstr ("ago");
    }
    
    if (config->syncdbs_in_tooltip && kalpm_state.nb_syncdbs > 0)
    {
        addstr ("\nsync possible for %d dbs", kalpm_state.nb_syncdbs);
    }
    
    if (kalpm_state.nb_news > 0)
    {
        addstr ("\n%d unread news", kalpm_state.nb_news);
    }
    if (kalpm_state.nb_upgrades > 0)
    {
        addstr ("\n%d upgrades available", kalpm_state.nb_upgrades);
    }
    if (kalpm_state.nb_watched > 0)
    {
        addstr ("\n%d watched packages updated", kalpm_state.nb_watched);
    }
    if (kalpm_state.nb_aur > 0)
    {
        addstr ("\n%d AUR packages updated", kalpm_state.nb_aur);
    }
    if (kalpm_state.nb_watched_aur > 0)
    {
        addstr ("\n%d watched AUR packages updated", kalpm_state.nb_watched_aur);
    }
    
    if (max <= 0)
    {
        sprintf (buf, "kalu: error setting tooltip");
    }
    gtk_tooltip_set_text (tooltip, buf);
    return TRUE;
}
#undef addstr

GtkStatusIcon *icon = NULL;

static gboolean
set_status_icon (gboolean active)
{
    if (active)
    {
        gtk_status_icon_set_from_stock (icon, "kalu-logo");
    }
    else
    {
        gtk_status_icon_set_from_stock (icon, "kalu-logo-gray");
    }
    /* do NOT get called back */
    return FALSE;
}

void
set_kalpm_nb (check_t type, gint nb)
{
    if (type & CHECK_UPGRADES)
    {
        kalpm_state.nb_upgrades = nb;
    }
    
    if (type & CHECK_WATCHED)
    {
        kalpm_state.nb_watched = nb;
    }
    
    if (type & CHECK_AUR)
    {
        kalpm_state.nb_aur = nb;
    }
    
    if (type & CHECK_WATCHED_AUR)
    {
        kalpm_state.nb_watched_aur = nb;
    }
    
    if (type & CHECK_NEWS)
    {
        kalpm_state.nb_news = nb;
    }
    
    /* thing is, this function can be called from another thread (e.g. from
     * kalu_check_work, which runs in a separate thread not to block GUI...)
     * but when that happens, we can't use gtk_* functions, i.e. we can't change
     * the status icon. so, this will make sure the call to set_status_icon
     * happens in the main thread */
    gboolean active = (kalpm_state.nb_upgrades + kalpm_state.nb_watched
                       + kalpm_state.nb_aur + kalpm_state.nb_watched_aur
                       + kalpm_state.nb_news > 0);
    g_main_context_invoke (NULL, (GSourceFunc) set_status_icon, GINT_TO_POINTER (active));
}

inline void
set_kalpm_nb_syncdbs (gint nb)
{
    kalpm_state.nb_syncdbs = nb;
}

static gboolean
switch_status_icon (void)
{
    static gboolean active = FALSE;
    active = !active;
    set_status_icon (active);
    /* keep timeout alive */
    return TRUE;
}

void
set_kalpm_busy (gboolean busy)
{
    gint old = kalpm_state.is_busy;
    
    /* we use an counter because when using a cmdline for both upgrades & AUR,
     * and both are triggered at the same time (from notifications) then we
     * should only bo back to not busy when *both* are done; fixes #8 */
    if (busy)
    {
        ++kalpm_state.is_busy;
    }
    else if (kalpm_state.is_busy > 0)
    {
        --kalpm_state.is_busy;
    }
    
    /* make sure the state changed/there's something to do */
    if ((old > 0  && kalpm_state.is_busy > 0)
     || (old == 0 && kalpm_state.is_busy == 0))
    {
        return;
    }
    
    if (busy)
    {
        /* remove auto-check timeout */
        if (kalpm_state.timeout > 0)
        {
            g_source_remove (kalpm_state.timeout);
            kalpm_state.timeout = 0;
        }
        
        /* set timeout for status icon */
        kalpm_state.timeout_icon = g_timeout_add (420,
            (GSourceFunc) switch_status_icon, NULL);
    }
    else
    {
        /* set timeout for next auto-check */
        guint seconds;
        
        if (config->has_skip)
        {
            GDateTime *now, *begin, *end, *next;
            gint year, month, day;
            gboolean is_within_skip = FALSE;
            
            now = g_date_time_new_now_local ();
            /* create GDateTime for begin & end of skip period */
            /* Note: begin & end are both for the current day, which means we can
             * have begin > end, with e.g. 18:00-09:00 */
            g_date_time_get_ymd (now, &year, &month, &day);
            begin = g_date_time_new_local (year, month, day,
                config->skip_begin_hour, config->skip_begin_minute, 0);
            end = g_date_time_new_local (year, month, day,
                config->skip_end_hour, config->skip_end_minute, 0);
            
            /* determine when the next check would take place */
            next = g_date_time_add_seconds (now, (gdouble) config->interval);
            
            /* determine if next within skip period */
            /* is begin > end ? */
            if (g_date_time_compare (begin, end) == 1)
            {
                /* e.g. 18:00 -> 09:00 */
                if (g_date_time_compare (next, end) == -1)
                {
                    /* before 09:00 */
                    is_within_skip = TRUE;
                }
                else if (g_date_time_compare (next, begin) == 1)
                {
                    /* after 18:00 */
                    is_within_skip = TRUE;
                    /* we need to switch end to the end for the next day */
                    g_date_time_unref (end);
                    end = g_date_time_new_local (year, month, day + 1,
                        config->skip_end_hour, config->skip_end_minute, 0);
                }
            }
            else
            {
                /* e.g. 09:00 -> 18:00 */
                is_within_skip = (g_date_time_compare (next, begin) == 1
                    && g_date_time_compare (next, end) == -1);
            }
            
            if (is_within_skip)
            {
                /* we'll do the next check at the end of skip period */
                GTimeSpan timespan;
                timespan = g_date_time_difference (end, now);
                seconds = (guint) (timespan / G_TIME_SPAN_SECOND);
            }
            else
            {
                seconds = (guint) config->interval;
            }
            
            g_date_time_unref (now);
            g_date_time_unref (begin);
            g_date_time_unref (end);
            g_date_time_unref (next);
        }
        else
        {
            seconds = (guint) config->interval;
        }
        kalpm_state.timeout = g_timeout_add_seconds (seconds,
            (GSourceFunc) kalu_auto_check, NULL);
        
        /* remove status icon timeout */
        if (kalpm_state.timeout_icon > 0)
        {
            g_source_remove (kalpm_state.timeout_icon);
            kalpm_state.timeout_icon = 0;
            /* ensure icon is right */
            set_kalpm_nb (0, 0);
        }
    }
}

gboolean
reload_watched (gboolean is_aur, GError **error)
{
    GError *local_err = NULL;
    gchar file[MAX_PATH];
    conf_file_t conffile;
    
    if (is_aur)
    {
        /* clear */
        FREE_WATCHED_PACKAGE_LIST (config->watched_aur);
        /* load */
        snprintf (file, MAX_PATH - 1, "%s/.config/kalu/watched-aur.conf", g_get_home_dir ());
        conffile = CONF_FILE_WATCHED_AUR;
    }
    else
    {
        /* clear */
        FREE_WATCHED_PACKAGE_LIST (config->watched);
        /* load */
        snprintf (file, MAX_PATH - 1, "%s/.config/kalu/watched.conf", g_get_home_dir ());
        conffile = CONF_FILE_WATCHED;
    }
    
    if (!parse_config_file (file, conffile, &local_err))
    {
        g_propagate_error (error, local_err);
        return FALSE;
    }
    
    return TRUE;
}
