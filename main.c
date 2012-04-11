/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * main.c
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
#include <time.h> /* for debug() */

/* gtk */
#include <gtk/gtk.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* notify */
#include <libnotify/notify.h>

/* curl */
#include <curl/curl.h>

/* kalu */
#include "kalu.h"
#include "kalu-alpm.h"
#include "conf.h"
#include "util.h"
#include "util-gtk.h"
#include "watched.h"
#include "arch_linux.h"
#ifndef DISABLE_UPDATER
#include "kalu-updater.h"
#include "updater.h"
#endif
#include "aur.h"
#include "news.h"
#include "preferences.h"


/* global variable */
config_t *config = NULL;

#ifndef DISABLE_UPDATER
#define run_updater()   do {                \
        set_kalpm_busy (TRUE);              \
        updater_run (config->cmdline_post); \
    } while (0)
#endif

static void action_upgrade (NotifyNotification *notification, const char *action, gpointer data);
static void action_watched (NotifyNotification *notification, char *action, alpm_list_t *packages);
static void notify_updates (alpm_list_t *packages, check_t type, gchar *xml_news);
static void kalu_check (gboolean is_auto);
static void kalu_auto_check (void);
static void menu_check_cb (GtkMenuItem *item, gpointer data);
static void menu_quit_cb (GtkMenuItem *item, gpointer data);
static void icon_popup_cb (GtkStatusIcon *icon, guint button, guint activate_time, gpointer data);
static void free_config (void);

static kalpm_state_t kalpm_state = { FALSE, 0, 0, NULL, 0, 0, 0, 0, 0, 0 };

static void
run_cmdline (const char *cmdline)
{
    GError *error = NULL;
    
    if (!g_spawn_command_line_async (cmdline, &error))
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
                cmdline, error->message);
        gtk_window_set_title (GTK_WINDOW (dialog), "kalu: Unable to start process");
        gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
        gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
        gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), FALSE);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
        g_error_free (error);
    }
}

static void
action_upgrade (NotifyNotification *notification, const char *action, gpointer data _UNUSED_)
{
    char *cmdline = NULL;
    
    notify_notification_close (notification, NULL);
    
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
    
    if (cmdline)
    {
        run_cmdline (cmdline);
    }
}

static void
action_watched (NotifyNotification *notification, char *action _UNUSED_,
    alpm_list_t *packages)
{
    notify_notification_close (notification, NULL);
    watched_update (packages, FALSE);
}

static void
action_watched_aur (NotifyNotification *notification, char *action _UNUSED_,
    alpm_list_t *packages)
{
    notify_notification_close (notification, NULL);
    watched_update (packages, TRUE);
}

static void
action_news (NotifyNotification *notification, char *action _UNUSED_,
    gchar *xml_news)
{
    GError *error = NULL;
    
    notify_notification_close (notification, NULL);
    if (!news_show (xml_news, TRUE, &error))
    {
        show_error ("Unable to show the news", error->message, NULL);
        g_clear_error (&error);
    }
}

static void
free_packages (alpm_list_t *packages)
{
    FREE_PACKAGE_LIST (packages);
}

static void
notification_closed_cb (NotifyNotification *notification, gpointer data _UNUSED_)
{
    g_object_unref (notification);
}

static void
notify_updates (alpm_list_t *packages, check_t type, gchar *xml_news)
{
    alpm_list_t *i;
    
    int         nb          = 0;
    int         net_size;
    off_t       dsize       = 0;
    off_t       isize       = 0;
    off_t       nsize       = 0;
    
    gchar      *summary;
    gchar      *text;
    unsigned int alloc;
    unsigned int len;
    char        buf[255];
    char       *s;
    char       *b;
    templates_t template;
    const char *unit;
    double      size_h;
    replacement_t *replacements[6];
    
    templates_t *t, *tt;
    /* tpl_upgrades is the ref/fallback for pretty much everything */
    t = config->tpl_upgrades;
    if (type & CHECK_UPGRADES)
    {
        tt = config->tpl_upgrades;
    }
    else if (type & CHECK_WATCHED)
    {
        tt = config->tpl_watched;
    }
    else if (type & CHECK_AUR)
    {
        tt = config->tpl_aur;
    }
    else if (type & CHECK_WATCHED_AUR)
    {
        tt = config->tpl_watched_aur;
        /* watched-aur uses aur as fallback */
        t = config->tpl_aur;
    }
    else if (type & CHECK_NEWS)
    {
        tt = config->tpl_news;
    }
    /* set the templates to use */
    template.title = (tt->title) ? tt->title : t->title;
    template.package = (tt->package) ? tt->package : t->package;
    template.sep = (tt->sep) ? tt->sep : t->sep;
    /* watched-aur might have fallen back to aur, which itself needs to fallback */
    if (type & CHECK_WATCHED_AUR)
    {
        t = config->tpl_upgrades;
        template.title = (template.title) ? template.title : t->title;
        template.package = (template.package) ? template.package : t->package;
        template.sep = (template.sep) ? template.sep : t->sep;
    }
    
    alloc = 1024;
    text = (gchar *) malloc ((alloc + 1) * sizeof (*text));
    len = 0;
    
    for (i = packages; i; i = alpm_list_next (i))
    {
        ++nb;
        if (type & CHECK_NEWS)
        {
            replacements[0] = (replacement_t *) malloc (sizeof (*replacements));
            replacements[0]->name = "NEWS";
            replacements[0]->value = i->data;
            replacements[1] = NULL;
            
            /* add separator? */
            if (nb > 1)
            {
                s = text + len;
                for (b = template.sep; *b; ++b, ++s, ++len)
                {
                    *s = *b;
                }
            }
            
            parse_tpl (template.package, &text, &len, &alloc, replacements);
            
            free (replacements[0]);
            
            debug ("-> %s", (char *) i->data);
        }
        else
        {
            kalu_package_t *pkg = i->data;
            
            net_size = (int) (pkg->new_size - pkg->old_size);
            dsize += pkg->dl_size;
            isize += pkg->new_size;
            nsize += net_size;
            
            replacements[0] = (replacement_t *) malloc (sizeof (*replacements));
            replacements[0]->name = "PKG";
            replacements[0]->value = pkg->name;
            replacements[1] = (replacement_t *) malloc (sizeof (*replacements));
            replacements[1]->name = "OLD";
            replacements[1]->value = pkg->old_version;
            replacements[2] = (replacement_t *) malloc (sizeof (*replacements));
            replacements[2]->name = "NEW";
            replacements[2]->value = pkg->new_version;
            replacements[3] = (replacement_t *) malloc (sizeof (*replacements));
            replacements[3]->name = "DL";
            size_h = humanize_size (pkg->dl_size, '\0', &unit);
            snprintf (buf, 255, "%.2f %s", size_h, unit);
            replacements[3]->value = strdup (buf);
            replacements[4] = (replacement_t *) malloc (sizeof (*replacements));
            replacements[4]->name = "INS";
            size_h = humanize_size (pkg->new_size, '\0', &unit);
            snprintf (buf, 255, "%.2f %s", size_h, unit);
            replacements[4]->value = strdup (buf);
            replacements[5] = (replacement_t *) malloc (sizeof (*replacements));
            replacements[5]->name = "NET";
            size_h = humanize_size (net_size, '\0', &unit);
            snprintf (buf, 255, "%.2f %s", size_h, unit);
            replacements[5]->value = strdup (buf);
            replacements[6] = NULL;
            
            /* add separator? */
            if (nb > 1)
            {
                s = text + len;
                for (b = template.sep; *b; ++b, ++s, ++len)
                {
                    *s = *b;
                }
            }
            
            parse_tpl (template.package, &text, &len, &alloc, replacements);
            
            free (replacements[3]->value);
            free (replacements[4]->value);
            free (replacements[5]->value);
            for (int j = 0; j < 6; ++j)
                free (replacements[j]);
            
            debug ("-> %s %s -> %s [dl=%d; ins=%d]",
                   pkg->name,
                   pkg->old_version,
                   pkg->new_version,
                   (int) pkg->dl_size,
                   (int) pkg->new_size);
        }
    }
    
    alloc = 128;
    summary = (gchar *) malloc ((alloc + 1) * sizeof (*summary));
    len = 0;
    
    replacements[0] = (replacement_t *) malloc (sizeof (*replacements));
    replacements[0]->name = "NB";
    snprintf (buf, 255, "%d", nb);
    replacements[0]->value = strdup (buf);
    if (type & CHECK_NEWS)
    {
        replacements[1] = NULL;
    }
    else
    {
        replacements[1] = (replacement_t *) malloc (sizeof (*replacements));
        replacements[1]->name = "DL";
        size_h = humanize_size (dsize, '\0', &unit);
        snprintf (buf, 255, "%.2f %s", size_h, unit);
        replacements[1]->value = strdup (buf);
        replacements[2] = (replacement_t *) malloc (sizeof (*replacements));
        replacements[2]->name = "NET";
        size_h = humanize_size (nsize, '\0', &unit);
        snprintf (buf, 255, "%.2f %s", size_h, unit);
        replacements[2]->value = strdup (buf);
        replacements[3] = (replacement_t *) malloc (sizeof (*replacements));
        replacements[3]->name = "INS";
        size_h = humanize_size (isize, '\0', &unit);
        snprintf (buf, 255, "%.2f %s", size_h, unit);
        replacements[3]->value = strdup (buf);
        replacements[4] = NULL;
    }
    
    parse_tpl (template.title, &summary, &len, &alloc, replacements);
    
    free (replacements[0]->value);
    free (replacements[0]);
    if (!(type & CHECK_NEWS))
    {
        free (replacements[1]->value);
        free (replacements[2]->value);
        free (replacements[3]->value);
        free (replacements[1]);
        free (replacements[2]);
        free (replacements[3]);
    }
    
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
    notify_notification_set_timeout (notification, config->timeout);
    if (type & CHECK_UPGRADES)
    {
        if (config->action != UPGRADE_NO_ACTION)
        {
            notify_notification_add_action (notification, "do_updates",
                "Update system...", (NotifyActionCallback) action_upgrade,
                NULL, NULL);
        }
    }
    else if (type & CHECK_AUR)
    {
        if (config->cmdline_aur)
        {
            notify_notification_add_action (notification, "do_updates_aur",
                "Update AUR packages...", (NotifyActionCallback) action_upgrade,
                NULL, NULL);
        }
    }
    else if (type & CHECK_WATCHED)
    {
        notify_notification_add_action (notification,
                                        "mark_watched",
                                        "Mark packages...",
                                        (NotifyActionCallback) action_watched,
                                        packages,
                                        (GFreeFunc) free_packages);
    }
    else if (type & CHECK_WATCHED_AUR)
    {
        notify_notification_add_action (notification,
                                        "mark_watched_aur",
                                        "Mark packages...",
                                        (NotifyActionCallback) action_watched_aur,
                                        packages,
                                        (GFreeFunc) free_packages);
    }
    else if (type & CHECK_NEWS)
    {
        notify_notification_add_action (notification,
                                        "mark_news",
                                        "Show news...",
                                        (NotifyActionCallback) action_news,
                                        xml_news,
                                        (GFreeFunc) free);
    }
    /* we use a callback on "closed" to unref it, because when there's an action
     * we need to keep a ref, otherwise said action won't work */
    g_signal_connect (G_OBJECT (notification), "closed",
                      G_CALLBACK (notification_closed_cb), NULL);
    notify_notification_show (notification, NULL);
    free (summary);
    free (text);
}

static void
kalu_check_work (gboolean is_auto)
{
    GError *error = NULL;
    alpm_list_t *packages, *aur_pkgs;
    gchar *xml_news;
    gboolean got_something =  FALSE;
    gint nb_syncdbs     = -1;
    gint nb_upgrades    = -1;
    gint nb_watched     = -1;
    gint nb_aur         = -1;
    gint nb_watched_aur = -1;
    gint nb_news        = -1;
    unsigned int checks = (is_auto) ? config->checks_auto : config->checks_manual;
    
    if (checks & CHECK_NEWS)
    {
        packages = NULL;
        if (news_has_updates (&packages, &xml_news, &error))
        {
            got_something = TRUE;
            nb_news = (gint) alpm_list_count (packages);
            notify_updates (packages, CHECK_NEWS, xml_news);
            FREELIST (packages);
            /* we dont free xml_news because it might be used by the notification
             * action (to show the news). hence, it'll be done when the notif is over */
        }
        else if (error != NULL)
        {
            notify_error ("Unable to check the news", error->message);
            g_clear_error (&error);
        }
    }
    
    /* ALPM is required even for AUR only, since we get the list of foreign
     * packages from localdb (however we can skip sync-ing dbs then) */
    if (checks & (CHECK_UPGRADES | CHECK_WATCHED | CHECK_AUR))
    {
        if (!kalu_alpm_load (config->pacmanconf, &error))
        {
            notify_error ("Unable to check for updates -- loading alpm library failed",
                error->message);
            g_clear_error (&error);
            set_kalpm_busy (FALSE);
            return;
        }
        
        /* syncdbs only if needed */
        if (checks & (CHECK_UPGRADES | CHECK_WATCHED)
            && !kalu_alpm_syncdbs (&nb_syncdbs, &error))
        {
            notify_error ("Unable to check for updates -- could not synchronize databases",
                error->message);
            g_clear_error (&error);
            kalu_alpm_free ();
            set_kalpm_busy (FALSE);
            return;
        }
        
        if (checks & CHECK_UPGRADES)
        {
            packages = NULL;
            if (kalu_alpm_has_updates (&packages, &error))
            {
                got_something = TRUE;
                nb_upgrades = (gint) alpm_list_count (packages);
                notify_updates (packages, CHECK_UPGRADES, NULL);
                FREE_PACKAGE_LIST (packages);
            }
            else if (error == NULL)
            {
                nb_upgrades = 0;
            }
            else
            {
                got_something = TRUE;
                /* means the error is likely to come from a dependency issue/conflict */
                if (error->code == 2)
                {
                    /* we do the notification (instead of calling notify_error) because
                     * we need to add the "Update system" button/action. */
                    NotifyNotification *notification;
                    GtkWidget          *w;
                    GdkPixbuf          *pixbuf;
                    
                    notification = notify_notification_new (
                        "Unable to compile list of packages",
                        error->message,
                        NULL);
                    w = gtk_label_new (NULL);
                    g_object_ref_sink (w);
                    pixbuf = gtk_widget_render_icon_pixbuf (w, "kalu-logo", GTK_ICON_SIZE_BUTTON);
                    notify_notification_set_image_from_pixbuf (notification, pixbuf);
                    g_object_unref (pixbuf);
                    gtk_widget_destroy (w);
                    g_object_unref (w);
                    notify_notification_set_timeout (notification, config->timeout);
                    if (config->action != UPGRADE_NO_ACTION)
                    {
                        notify_notification_add_action (notification, "do_updates",
                            "Update system...", (NotifyActionCallback) action_upgrade,
                            NULL, NULL);
                    }
                    /* we use a callback on "closed" to unref it, because when there's an action
                     * we need to keep a ref, otherwise said action won't work */
                    g_signal_connect (G_OBJECT (notification), "closed",
                                      G_CALLBACK (notification_closed_cb), NULL);
                    notify_notification_show (notification, NULL);
                }
                else
                {
                    notify_error ("Unable to check for updates", error->message);
                }
                g_clear_error (&error);
            }
        }
        
        if (checks & CHECK_WATCHED && config->watched /* NULL if no watched pkgs */)
        {
            packages = NULL;
            if (kalu_alpm_has_updates_watched (&packages, config->watched, &error))
            {
                got_something = TRUE;
                nb_watched = (gint) alpm_list_count (packages);
                notify_updates (packages, CHECK_WATCHED, NULL);
                /* watched are a special case, because the list of packages must not be
                 * free-d right now, but later when the notification is over. this is
                 * because the notification might use it to show the "mark packages"
                 * window/list */
            }
            else if (error == NULL)
            {
                nb_watched = 0;
            }
            else
            {
                got_something = TRUE;
                notify_error ("Unable to check for updates of watched packages",
                              error->message);
                g_clear_error (&error);
            }
        }
        
        if (checks & CHECK_AUR)
        {
            aur_pkgs = NULL;
            if (kalu_alpm_has_foreign (&aur_pkgs, config->aur_ignore, &error))
            {
                packages = NULL;
                if (aur_has_updates (&packages, aur_pkgs, FALSE, &error))
                {
                    got_something = 1;
                    nb_aur = (gint) alpm_list_count (packages);
                    notify_updates (packages, CHECK_AUR, NULL);
                    FREE_PACKAGE_LIST (packages);
                }
                else if (error == NULL)
                {
                    nb_aur = 0;
                }
                else
                {
                    got_something = TRUE;
                    notify_error ("Unable to check for AUR packages", error->message);
                    g_clear_error (&error);
                }
                alpm_list_free (aur_pkgs);
            }
            else if (error == NULL)
            {
                nb_aur = 0;
            }
            else
            {
                got_something = TRUE;
                notify_error ("Unable to check for AUR packages", error->message);
                g_clear_error (&error);
            }
        }
        
        kalu_alpm_free ();
    }
    
    if (checks & CHECK_WATCHED_AUR && config->watched_aur /* NULL if not watched aur pkgs */)
    {
        packages = NULL;
        if (aur_has_updates (&packages, config->watched_aur, TRUE, &error))
        {
            got_something = TRUE;
            nb_watched_aur = (gint) alpm_list_count (packages);
            notify_updates (packages, CHECK_WATCHED_AUR, NULL);
            /* watched are a special case, because the list of packages must not be
             * free-d right now, but later when the notification is over. this is
             * because the notification might use it to show the "mark packages"
             * window/list */
        }
        else if (error == NULL)
        {
            nb_watched_aur = 0;
        }
        else
        {
            got_something = TRUE;
            notify_error ("Unable to check for updates of watched AUR packages",
                          error->message);
            g_clear_error (&error);
        }
    }
    
    if (!is_auto && !got_something)
    {
        notify_error ("No upgrades available.", NULL);
    }
    
    /* update state */
    if (NULL != kalpm_state.last_check)
    {
        g_date_time_unref (kalpm_state.last_check);
    }
    kalpm_state.last_check = g_date_time_new_now_local ();
    if (nb_syncdbs >= 0)
    {
        set_kalpm_nb_syncdbs (nb_syncdbs);
    }
    if (nb_news >= 0)
    {
        set_kalpm_nb (CHECK_NEWS, nb_news);
    }
    if (nb_upgrades >= 0)
    {
        set_kalpm_nb (CHECK_UPGRADES, nb_upgrades);
    }
    if (nb_watched >= 0)
    {
        set_kalpm_nb (CHECK_WATCHED, nb_watched);
    }
    if (nb_aur >= 0)
    {
        set_kalpm_nb (CHECK_AUR, nb_aur);
    }
    if (nb_watched_aur >= 0)
    {
        set_kalpm_nb (CHECK_WATCHED_AUR, nb_watched_aur);
    }
    set_kalpm_busy (FALSE);
}

static inline void
kalu_check (gboolean is_auto)
{
    /* in case e.g. the menu was shown (sensitive) before an auto-check started */
    if (kalpm_state.is_busy)
    {
        return;
    }
    set_kalpm_busy (TRUE);
    
    /* run in a separate thread, to not block/make GUI unresponsive */
    g_thread_create ((GThreadFunc) kalu_check_work, GINT_TO_POINTER (is_auto), FALSE, NULL);
}

static void
kalu_auto_check (void)
{
    kalu_check (TRUE);
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
        run_cmdline (config->cmdline);
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
        show_error ("Unable to show the recent Arch Linxu news", error->message, NULL);
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

static void
icon_popup_cb (GtkStatusIcon *_icon _UNUSED_, guint button, guint activate_time,
               gpointer data _UNUSED_)
{
    GtkWidget   *menu;
    GtkWidget   *item;
    GtkWidget   *image;
    guint        pos = 0;
    
    menu = gtk_menu_new();
    
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

static GPtrArray *open_windows = NULL;
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
    } while (0)

static gboolean
icon_press_click (gpointer data _UNUSED_)
{
    icon_press_timeout = 0;
    
    process_click_action (config->on_sgl_click);
    
    return FALSE;
}

static gboolean
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
static gboolean
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

static void
free_config (void)
{
    if (config == NULL)
        return;
    
    free (config->pacmanconf);
    
    /* tpl */
    free (config->tpl_upgrades->title);
    free (config->tpl_upgrades->package);
    free (config->tpl_upgrades->sep);
    free (config->tpl_upgrades);
    
    /* tpl watched */
    free (config->tpl_watched->title);
    free (config->tpl_watched->package);
    free (config->tpl_watched->sep);
    free (config->tpl_watched);
    
    /* tpl aur */
    free (config->tpl_aur->title);
    free (config->tpl_aur->package);
    free (config->tpl_aur->sep);
    free (config->tpl_aur);
    
    /* watched */
    FREE_WATCHED_PACKAGE_LIST (config->watched);
    
    /* watched aur */
    FREE_WATCHED_PACKAGE_LIST (config->watched_aur);
    
    /* aur ignore */
    FREELIST (config->aur_ignore);
    
    #ifndef DISABLE_UPDATER
    FREELIST (config->cmdline_post);
    #endif
    
    /* news */
    free (config->news_last);
    FREELIST (config->news_read);
    
    free (config);
}

void
free_package (kalu_package_t *package)
{
    free (package->name);
    free (package->old_version);
    free (package->new_version);
    free (package);
}

void
free_watched_package (watched_package_t *w_pkg)
{
    free (w_pkg->name);
    free (w_pkg->version);
    free (w_pkg);
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

static GtkStatusIcon *icon = NULL;

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
    if (busy == kalpm_state.is_busy)
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
        
        kalpm_state.is_busy = TRUE;
    }
    else
    {
        /* set timeout for next auto-check */
        guint seconds;
        
        if (config->has_skip)
        {
            GDateTime *now, *begin, *end, *next;
            gint year, month, day;
            gboolean is_within_skip;
            
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
        
        kalpm_state.is_busy = FALSE;
    }
}

void
debug (const char *fmt, ...)
{
    va_list    args;
    time_t     now;
    struct tm *ptr;
    char       buf[10];
    
    if (!config->is_debug)
    {
        return;
    }
    
    now = time (NULL);
    ptr = localtime (&now);
    strftime (buf, 10, "%H:%M:%S", ptr);
    fprintf (stdout, "[%s] ", buf);
    
    va_start (args, fmt);
    vfprintf (stdout, fmt, args);
    va_end (args);
    
    fprintf (stdout, "\n");
}

int
main (int argc, char *argv[])
{
    GError          *error = NULL;
    GtkIconFactory  *factory;
    GtkIconSet      *iconset;
    GdkPixbuf       *pixbuf;
    gchar            conffile[MAX_PATH];
    
    gtk_init (&argc, &argv);
    config = calloc (1, sizeof(*config));
    
    /* parse command line -- very basic stuff */
    if (argc > 1)
    {
        if (strcmp (argv[1], "-h") == 0 || strcmp (argv[1], "--help") == 0)
        {
            printf ("kalu - " PACKAGE_TAG " v" PACKAGE_VERSION "\n\n");
            printf (" -h, --help        Show this help screen and exit\n");
            printf (" -V, --version     Show version information and exit\n");
            printf (" -d, --debug       Enable debug mode\n");
            printf ("\nFor more, please refer to the man page: man kalu\n");
            return 0;
        }
        else if (strcmp (argv[1], "-V") == 0 || strcmp (argv[1], "--version") == 0)
        {
            printf ("kalu - " PACKAGE_TAG " v" PACKAGE_VERSION "\n");
            printf ("Copyright (C) 2012 Olivier Brunel\n");
            printf ("License GPLv3+: GNU GPL version 3 or later"
                    " <http://gnu.org/licenses/gpl.html>\n");
            printf ("This is free software: you are free to change and redistribute it.\n");
            printf ("There is NO WARRANTY, to the extent permitted by law.\n");
            return 0;
        }
        else if (strcmp (argv[1], "-d") == 0 || strcmp (argv[1], "--debug") == 0)
        {
            config->is_debug = TRUE;
            debug ("debug mode enabled");
        }
    }
    
    /* defaults -- undefined "sub"templates will use the corresponding main ones */
    /* (e.g. tpl_sep_watched_verbose defaults to tpl_sep_verbose) */
    config->pacmanconf = strdup ("/etc/pacman.conf");
    config->interval = 3600; /* 1 hour */
    config->timeout = NOTIFY_EXPIRES_DEFAULT;
    config->syncdbs_in_tooltip = TRUE;
    config->checks_manual = CHECK_UPGRADES | CHECK_WATCHED | CHECK_AUR
                            | CHECK_WATCHED_AUR | CHECK_NEWS;
    config->checks_auto   = CHECK_UPGRADES | CHECK_WATCHED | CHECK_AUR
                            | CHECK_WATCHED_AUR | CHECK_NEWS;
    #ifndef DISABLE_UPDATER
    config->action = UPGRADE_ACTION_KALU;
    #else
    config->action = UPGRADE_NO_ACTION;
    #endif
    config->on_sgl_click = DO_CHECK;
    config->on_dbl_click = DO_SYSUPGRADE;
    config->sane_sort_order = TRUE;
    
    config->tpl_upgrades = calloc (1, sizeof (templates_t));
    config->tpl_upgrades->title = strdup ("$NB updates available (D: $DL; N: $NET)");
    config->tpl_upgrades->package = strdup ("- <b>$PKG</b> $OLD > <b>$NEW</b> (D: $DL; N: $NET)");
    config->tpl_upgrades->sep = strdup ("\n");
    
    config->tpl_watched = calloc (1, sizeof (templates_t));
    config->tpl_watched->title = strdup ("$NB watched packages updated (D: $DL; N: $NET)");
    
    config->tpl_aur = calloc (1, sizeof (templates_t));
    config->tpl_aur->title = strdup ("AUR: $NB packages updated");
    config->tpl_aur->package = strdup ("- <b>$PKG</b> $OLD > <b>$NEW</b>");
    
    config->tpl_watched_aur = calloc (1, sizeof (templates_t));
    config->tpl_watched_aur->title = strdup ("AUR: $NB watched packages updated");
    
    config->tpl_news = calloc (1, sizeof (templates_t));
    config->tpl_news->title = strdup ("$NB unread news");
    config->tpl_news->package = strdup ("- $NEWS");
    
    /* parse config */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/kalu.conf", g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_KALU, &error))
    {
        show_error ("Unable to parse configuration", error->message, NULL);
        g_clear_error (&error);
    }
    /* parse watched */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/watched.conf", g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_WATCHED, &error))
    {
        show_error ("Unable to parse watched packages", error->message, NULL);
        g_clear_error (&error);
    }
    /* parse watched aur */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/watched-aur.conf", g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_WATCHED_AUR, &error))
    {
        show_error ("Unable to parse watched AUR packages", error->message, NULL);
        g_clear_error (&error);
    }
    /* parse news */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/news.conf", g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_NEWS, &error))
    {
        show_error ("Unable to parse last news data", error->message, NULL);
        g_clear_error (&error);
    }
    
    /* icon stuff: so we'll be able to use "kalu-logo" from stock, and it will
     * handle multiple size/resize as well as graying out (e.g. on unsensitive
     * widgets) automatically */
    factory = gtk_icon_factory_new ();
    /* kalu-logo */
    pixbuf = gdk_pixbuf_new_from_inline (-1, arch, FALSE, NULL);
    iconset = gtk_icon_set_new_from_pixbuf (pixbuf);
    g_object_unref (G_OBJECT (pixbuf));
    gtk_icon_factory_add (factory, "kalu-logo", iconset);
    /* kalu-logo-gray */
    pixbuf = gdk_pixbuf_new_from_inline (-1, arch_gray, FALSE, NULL);
    iconset = gtk_icon_set_new_from_pixbuf (pixbuf);
    g_object_unref (G_OBJECT (pixbuf));
    gtk_icon_factory_add (factory, "kalu-logo-gray", iconset);
    /* add it all */
    gtk_icon_factory_add_default (factory);
    
    icon = gtk_status_icon_new_from_stock ("kalu-logo-gray");
    gtk_status_icon_set_name (icon, "kalu");
    gtk_status_icon_set_title (icon, "kalu -- keeping arch linux updated");
    gtk_status_icon_set_tooltip_text (icon, "kalu");
    
    g_signal_connect (G_OBJECT (icon), "query-tooltip",
                      G_CALLBACK (icon_query_tooltip_cb), NULL);
    g_signal_connect (G_OBJECT (icon), "popup-menu",
                      G_CALLBACK (icon_popup_cb), NULL);
    g_signal_connect (G_OBJECT (icon), "button-press-event",
                      G_CALLBACK (icon_press_cb), NULL);

    gtk_status_icon_set_visible (icon, TRUE);
    
    /* set timer, first check in 2 seconds */
    kalpm_state.timeout = g_timeout_add_seconds (2, (GSourceFunc) kalu_auto_check, NULL);
    
    notify_init ("kalu");
    if (curl_global_init (CURL_GLOBAL_ALL) == 0)
    {
        config->is_curl_init = TRUE;
    }
    else
    {
        show_error ("Unable to initialize cURL",
            "kalu will therefore not be able to check the AUR",
            NULL);
    }
    gtk_main ();
    if (config->is_curl_init)
    {
        curl_global_cleanup ();
    }
    notify_uninit ();
    free_config ();
    if (open_windows)
    {
        g_ptr_array_free (open_windows, TRUE);
    }
    return 0;
}

