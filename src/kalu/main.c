/**
 * kalu - Copyright (C) 2012-2013 Olivier Brunel
 *
 * main.c
 * Copyright (C) 2012-2013 Olivier Brunel <i.am.jack.mail@gmail.com>
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
#include <locale.h>
#include <string.h>
#include <time.h> /* for debug() */

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* curl */
#include <curl/curl.h>

/* kalu */
#include "kalu.h"
#ifndef DISABLE_GUI
#include "gui.h"
#include "util-gtk.h"
#endif
#include "kalu-alpm.h"
#include "conf.h"
#include "util.h"
#include "aur.h"
#include "news.h"


/* global variable */
config_t *config = NULL;

static void notify_updates (alpm_list_t *packages, check_t type,
        gchar *xml_news, gboolean show_it);
static void free_config (void);

#ifdef DISABLE_GUI

static inline void
do_notify_error (const gchar *summary, const gchar *text)
{
    fprintf (stderr, "%s\n", summary);
    if (text)
    {
        fprintf (stderr, "%s\n", text);
    }
}

static void
do_show_error (
        const gchar *message,
        const gchar *submessage,
        void *parent _UNUSED_
        )
{
    fprintf (stderr, "%s\n", message);
    if (submessage)
    {
        fprintf (stderr, "%s\n", submessage);
    }
}

#else

kalpm_state_t kalpm_state;
static gboolean is_cli = FALSE;
extern const void *_binary_kalu_logo_start;
extern const void *_binary_kalu_logo_size;

static inline void
do_notify_error (const gchar *summary, const gchar *text)
{
    if (!is_cli)
    {
        notify_error (summary, text);
    }
    else
    {
        fprintf (stderr, "%s\n", summary);
        if (text)
        {
            fprintf (stderr, "%s\n", text);
        }
    }
}

static void
do_show_error (const gchar *message, const gchar *submessage, GtkWindow *parent)
{
    if (!is_cli)
    {
        show_error (message, submessage, parent);
    }
    else
    {
        fprintf (stderr, "%s\n", message);
        if (submessage)
        {
            fprintf (stderr, "%s\n", submessage);
        }
    }
}

#endif /* DISABLE_GUI*/

static void
notify_updates (
        alpm_list_t *packages,
        check_t      type,
        gchar       *xml_news,
        gboolean     show_it
        )
{
    alpm_list_t     *i;

    int              nb          = 0;
    int              net_size;
    off_t            dsize       = 0;
    off_t            isize       = 0;
    off_t            nsize       = 0;

    gchar           *summary;
    gchar           *text;
    unsigned int     alloc;
    unsigned int     len;
    char             buf[255];
    char            *s;
    char            *b;
    templates_t      template;
    const char      *unit;
    double           size_h;
    replacement_t   *replacements[8];
    gboolean         escaping = FALSE;
    GString         *string_pkgs = NULL;     /* list of AUR packages */

#ifdef DISABLE_GUI
    (void) xml_news;
    (void) show_it;
#else
    escaping = !is_cli;
#endif

    templates_t *t, *tt;
    /* tpl_upgrades is the ref/fallback for pretty much everything */
    t = config->tpl_upgrades;
    tt = NULL;
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
        /* if needed, init the string that will hold the list of packages */
        if (config->cmdline_aur)
        {
            string_pkgs = g_string_sized_new (255);
        }
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
    template.title = (tt && tt->title) ? tt->title : t->title;
    template.package = (tt && tt->package) ? tt->package : t->package;
    template.sep = (tt && tt->sep) ? tt->sep : t->sep;
    /* watched-aur might have fallen back to aur, which itself needs to fallback */
    if (type & CHECK_WATCHED_AUR)
    {
        t = config->tpl_upgrades;
        template.title = (template.title) ? template.title : t->title;
        template.package = (template.package) ? template.package : t->package;
        template.sep = (template.sep) ? template.sep : t->sep;
    }

    alloc = 1024;
    text = new (gchar, alloc + 1);
    len = 0;

    FOR_LIST (i, packages)
    {
        ++nb;
        if (type & CHECK_NEWS)
        {
            replacements[0] = new0 (replacement_t, 1);
            replacements[0]->name = "NEWS";
            replacements[0]->value = i->data;
            replacements[0]->need_escaping = TRUE;
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

            parse_tpl (template.package, &text, &len, &alloc,
                    replacements, escaping);

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

            replacements[0] = new0 (replacement_t, 1);
            replacements[0]->name = "PKG";
            replacements[0]->value = pkg->name;
            replacements[0]->need_escaping = TRUE;
            replacements[1] = new0 (replacement_t, 1);
            replacements[1]->name = "OLD";
            replacements[1]->value = pkg->old_version;
            replacements[2] = new0 (replacement_t, 1);
            replacements[2]->name = "NEW";
            replacements[2]->value = pkg->new_version;
            replacements[3] = new0 (replacement_t, 1);
            replacements[3]->name = "DL";
            size_h = humanize_size (pkg->dl_size, '\0', &unit);
            snprint_size (buf, 255, size_h, unit);
            replacements[3]->value = strdup (buf);
            replacements[4] = new0 (replacement_t, 1);
            replacements[4]->name = "INS";
            size_h = humanize_size (pkg->new_size, '\0', &unit);
            snprint_size (buf, 255, size_h, unit);
            replacements[4]->value = strdup (buf);
            replacements[5] = new0 (replacement_t, 1);
            replacements[5]->name = "NET";
            size_h = humanize_size (net_size, '\0', &unit);
            snprint_size (buf, 255, size_h, unit);
            replacements[5]->value = strdup (buf);
            replacements[6] = new0 (replacement_t, 1);
            replacements[6]->name = "DESC";
            replacements[6]->value = pkg->desc;
            replacements[6]->need_escaping = TRUE;
            replacements[7] = NULL;

            /* add separator? */
            if (nb > 1)
            {
                s = text + len;
                for (b = template.sep; *b; ++b, ++s, ++len)
                {
                    *s = *b;
                }
            }

            parse_tpl (template.package, &text, &len, &alloc,
                    replacements, escaping);

            free (replacements[3]->value);
            free (replacements[4]->value);
            free (replacements[5]->value);
            for (int j = 0; j < 7; ++j)
                free (replacements[j]);

            debug ("-> %s %s -> %s [dl=%d; ins=%d]",
                    pkg->name,
                    pkg->old_version,
                    pkg->new_version,
                    (int) pkg->dl_size,
                    (int) pkg->new_size);

            /* construct list of packages, for use in cmdline */
            if (string_pkgs)
            {
                string_pkgs = g_string_append (string_pkgs, pkg->name);
                string_pkgs = g_string_append_c (string_pkgs, ' ');
            }
        }
    }

    alloc = 255;
    summary = new (gchar, alloc + 1);
    len = 0;

    replacements[0] = new0 (replacement_t, 1);
    replacements[0]->name = "NB";
    snprintf (buf, 255, "%d", nb);
    replacements[0]->value = strdup (buf);
    if (type & CHECK_NEWS)
    {
        replacements[1] = NULL;
    }
    else
    {
        replacements[1] = new0 (replacement_t, 1);
        replacements[1]->name = "DL";
        size_h = humanize_size (dsize, '\0', &unit);
        snprint_size (buf, 255, size_h, unit);
        replacements[1]->value = strdup (buf);
        replacements[2] = new0 (replacement_t, 1);
        replacements[2]->name = "NET";
        size_h = humanize_size (nsize, '\0', &unit);
        snprint_size (buf, 255, size_h, unit);
        replacements[2]->value = strdup (buf);
        replacements[3] = new0 (replacement_t, 1);
        replacements[3]->name = "INS";
        size_h = humanize_size (isize, '\0', &unit);
        snprint_size (buf, 255, size_h, unit);
        replacements[3]->value = strdup (buf);
        replacements[4] = NULL;
    }

    parse_tpl (template.title, &summary, &len, &alloc,
            replacements, escaping);

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

#ifndef DISABLE_GUI
    if (is_cli)
    {
#endif
        puts (summary);
        puts (text);
        free (summary);
        free (text);
        return;
#ifndef DISABLE_GUI
    }

    notif_t *notif;
    notif = new (notif_t, 1);
    notif->type = type;
    notif->summary = summary;
    notif->text = text;
    notif->data = NULL;

    if (type & CHECK_AUR)
    {
        if (config->cmdline_aur && string_pkgs)
        {
            /* if we have a list of pkgs, update the cmdline */
            notif->data = strreplace (config->cmdline_aur, "$PACKAGES",
                    string_pkgs->str);
            g_string_free (string_pkgs, TRUE);
        }
        /* else no user data, so it'll default to config->cmdline_aur
         * So we'll always be able to call free (cmdline) in action_upgrade */
    }
    else if (type & CHECK_NEWS)
    {
        notif->data = xml_news;
    }
    else
    {
        /* CHECK_UPGRADES, CHECK_WATCHED & CHECK_WATCHED_AUR all use packages */
        notif->data = packages;
    }

    /* add the notif to the last of last notifications, so we can re-show it later */
    debug ("adding new notif (%s) to last_notifs", notif->summary);
    config->last_notifs = alpm_list_add (config->last_notifs, notif);
    /* show it */
    if (show_it)
    {
        show_notif (notif);
    }

#endif /* DISABLE_GUI */
}

void
kalu_check_work (gboolean is_auto)
{
    GError      *error = NULL;
    alpm_list_t *packages;
    alpm_list_t *aur_pkgs;
    gchar       *xml_news;
    gboolean     got_something  = FALSE;
    gint         nb_syncdbs     = -1;
#ifndef DISABLE_GUI
    gint         nb_upgrades    = -1;
    gint         nb_watched     = -1;
    gint         nb_aur         = -1;
    gint         nb_watched_aur = -1;
    gint         nb_news        = -1;
#endif /* DISABLE_GUI */
    unsigned int checks         = (is_auto)
        ? config->checks_auto
        : config->checks_manual;
    gboolean     show_it        = (is_auto) ? config->auto_notifs : TRUE;

#ifndef DISABLE_GUI
    /* drop the list of last notifs, since we'll be making up a new one */
    debug ("drop last_notifs");
    FREE_NOTIFS_LIST (config->last_notifs);
#endif

    /* we will not free packages nor xml_news, because they'll be stored in
     * notif_t (inside config->last_notifs) so we can re-show notifications.
     * Everything gets free-d through the FREE_NOTIFS_LIST above */

    if (checks & CHECK_NEWS)
    {
        packages = NULL;
        if (news_has_updates (&packages, &xml_news, &error))
        {
            got_something = TRUE;
#ifndef DISABLE_GUI
            nb_news = (gint) alpm_list_count (packages);
#endif /* DISABLE_GUI */
            notify_updates (packages, CHECK_NEWS, xml_news, show_it);
            FREELIST (packages);
        }
        else if (error != NULL)
        {
            do_notify_error (_("Unable to check the news"), error->message);
            g_clear_error (&error);
        }
#ifndef DISABLE_GUI
        else
        {
            nb_news = 0;
        }
        if (nb_news >= 0)
        {
            set_kalpm_nb (CHECK_NEWS, nb_news, FALSE);
        }
#endif /* DISABLE_GUI */
    }

    /* ALPM is required even for AUR only, since we get the list of foreign
     * packages from localdb (however we can skip sync-ing dbs then) */
    if (checks & (CHECK_UPGRADES | CHECK_WATCHED | CHECK_AUR))
    {
        if (!kalu_alpm_load (config->pacmanconf, &error))
        {
            do_notify_error (
                    _("Unable to check for updates -- loading alpm library failed"),
                    error->message);
            g_clear_error (&error);
#ifndef DISABLE_GUI
            if (!is_cli)
            {
                set_kalpm_busy (FALSE);
            }
#endif
            return;
        }

        /* syncdbs only if needed */
        if (checks & (CHECK_UPGRADES | CHECK_WATCHED)
                && !kalu_alpm_syncdbs (&nb_syncdbs, &error))
        {
            do_notify_error (
                    _("Unable to check for updates -- could not synchronize databases"),
                    error->message);
            g_clear_error (&error);
            kalu_alpm_free ();
#ifndef DISABLE_GUI
            if (!is_cli)
            {
                set_kalpm_busy (FALSE);
            }
#endif
            return;
        }
#ifndef DISABLE_GUI
        if (nb_syncdbs >= 0)
        {
            set_kalpm_nb_syncdbs (nb_syncdbs);
        }
#endif

        if (checks & CHECK_UPGRADES)
        {
            packages = NULL;
            if (kalu_alpm_has_updates (&packages, &error))
            {
                got_something = TRUE;
#ifndef DISABLE_GUI
                nb_upgrades = (gint) alpm_list_count (packages);
#endif /* DISABLE_GUI */
                notify_updates (packages, CHECK_UPGRADES, NULL, show_it);
            }
#ifndef DISABLE_GUI
            else if (error == NULL)
            {
                nb_upgrades = 0;
            }
            else
#else
                else if (error != NULL)
#endif /* DISABLE_GUI */
                {
                    got_something = TRUE;
                    /* means the error is likely to come from a dependency issue/conflict */
                    if (error->code == 2)
                    {
#ifndef DISABLE_GUI
                        if (!is_cli)
                        {
                            /* we do the notification (instead of calling
                             * notify_error) because we need to add the
                             * "Update system" button/action. */
                            NotifyNotification *notification;
                            notif_t *notif;

                            notif = new (notif_t, 1);
                            notif->type = CHECK_UPGRADES;
                            notif->summary = strdup (_("Unable to compile list of packages"));
                            notif->text = strdup (error->message);
                            notif->data = NULL;

                            notification = new_notification (notif->summary,
                                    notif->text);
                            if (config->action != UPGRADE_NO_ACTION)
                            {
                                notify_notification_add_action (notification,
                                        "do_updates",
                                        _c("notif-button", "Update system..."),
                                        (NotifyActionCallback) action_upgrade,
                                        NULL,
                                        NULL);
                            }
                            /* we use a callback on "closed" to unref it,
                             * because when there's an action we need to keep
                             * a ref, otherwise said action won't work */
                            g_signal_connect (G_OBJECT (notification),
                                    "closed",
                                    G_CALLBACK (notification_closed_cb),
                                    NULL);
                            /* add the notif to the last of last notifications,
                             * so we can re-show it later */
                            debug ("adding new notif (%s) to last_notifs",
                                    notif->summary);
                            config->last_notifs = alpm_list_add (
                                    config->last_notifs,
                                    notif);
                            /* show notif */
                            notify_notification_show (notification, NULL);
                            /* mark icon blue, upgrades are available, we just
                             * don't know which/how many (due to the conflict) */
                            nb_upgrades = UPGRADES_NB_CONFLICT;
                        }
                        else
                        {
#endif
                            do_notify_error (
                                    _("Unable to compile list of packages"),
                                    error->message);
#ifndef DISABLE_GUI
                        }
#endif
                    }
                    else
                    {
                        do_notify_error (
                                _("Unable to check for updates"),
                                error->message);
                    }
                    g_clear_error (&error);
                }
#ifndef DISABLE_GUI
                if (nb_upgrades >= 0 || nb_upgrades == UPGRADES_NB_CONFLICT)
                {
                    set_kalpm_nb (CHECK_UPGRADES, nb_upgrades, FALSE);
                }
#endif
        }

        if (checks & CHECK_WATCHED && config->watched /* NULL if no watched pkgs */)
        {
            packages = NULL;
            if (kalu_alpm_has_updates_watched (&packages,
                        config->watched,
                        &error))
            {
                got_something = TRUE;
#ifndef DISABLE_GUI
                nb_watched = (gint) alpm_list_count (packages);
#endif
                notify_updates (packages, CHECK_WATCHED, NULL, show_it);
            }
#ifndef DISABLE_GUI
            else if (error == NULL)
            {
                nb_watched = 0;
            }
            else
#else
                else if (error != NULL)
#endif
                {
                    got_something = TRUE;
                    do_notify_error (
                            _("Unable to check for updates of watched packages"),
                            error->message);
                    g_clear_error (&error);
                }
#ifndef DISABLE_GUI
                if (nb_watched >= 0)
                {
                    set_kalpm_nb (CHECK_WATCHED, nb_watched, FALSE);
                }
#endif
        }

        if (checks & CHECK_AUR)
        {
            aur_pkgs = NULL;
            if (kalu_alpm_has_foreign (&aur_pkgs, config->aur_ignore, &error))
            {
                packages = NULL;
                if (aur_has_updates (&packages, aur_pkgs, FALSE, &error))
                {
                    got_something = TRUE;
#ifndef DISABLE_GUI
                    nb_aur = (gint) alpm_list_count (packages);
#endif
                    notify_updates (packages, CHECK_AUR, NULL, show_it);
                    FREE_PACKAGE_LIST (packages);
                }
#ifndef DISABLE_GUI
                else if (error == NULL)
                {
                    nb_aur = 0;
                }
                else
#else
                    else if (error != NULL)
#endif
                    {
                        got_something = TRUE;
                        do_notify_error (
                                _("Unable to check for AUR packages"),
                                error->message);
                        g_clear_error (&error);
                    }
                    alpm_list_free (aur_pkgs);
            }
#ifndef DISABLE_GUI
            else if (error == NULL)
            {
                nb_aur = 0;
            }
            else
#else
                else if (error != NULL)
#endif
                {
                    got_something = TRUE;
                    do_notify_error (
                            _("Unable to check for AUR packages"),
                            error->message);
                    g_clear_error (&error);
                }
#ifndef DISABLE_GUI
                if (nb_aur >= 0)
                {
                    set_kalpm_nb (CHECK_AUR, nb_aur, FALSE);
                }
#endif
        }

        kalu_alpm_free ();
    }

    if (checks & CHECK_WATCHED_AUR && config->watched_aur /* NULL if not watched aur pkgs */)
    {
        packages = NULL;
        if (aur_has_updates (&packages, config->watched_aur, TRUE, &error))
        {
            got_something = TRUE;
#ifndef DISABLE_GUI
            nb_watched_aur = (gint) alpm_list_count (packages);
#endif
            notify_updates (packages, CHECK_WATCHED_AUR, NULL, show_it);
        }
#ifndef DISABLE_GUI
        else if (error == NULL)
        {
            nb_watched_aur = 0;
        }
        else
#else
            else if (error != NULL)
#endif
            {
                got_something = TRUE;
                do_notify_error (
                        _("Unable to check for updates of watched AUR packages"),
                        error->message);
                g_clear_error (&error);
            }
#ifndef DISABLE_GUI
            if (nb_watched_aur >= 0)
            {
                set_kalpm_nb (CHECK_WATCHED_AUR, nb_watched_aur, FALSE);
            }
#endif
    }

    if (!is_auto && !got_something)
    {
        do_notify_error (_("No upgrades available."), NULL);
    }

#ifndef DISABLE_GUI
    if (is_cli)
    {
        return;
    }

    /* update state */
    if (NULL != kalpm_state.last_check)
    {
        g_date_time_unref (kalpm_state.last_check);
    }
    kalpm_state.last_check = g_date_time_new_now_local ();
    set_kalpm_busy (FALSE);
#endif
}

static void
free_config (void)
{
    if (config == NULL)
    {
        return;
    }

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
    free (package->desc);
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

#ifndef DISABLE_GUI
extern GtkStatusIcon *icon;
extern GPtrArray     *open_windows;

static GtkIconSet *
get_paused_iconset (GdkPixbuf *pixbuf)
{
    cairo_surface_t *s;
    cairo_t         *cr;
    GdkPixbuf       *pb;
    GtkIconSet      *iconset;

    s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 48, 48);
    cr = cairo_create (s);

    /* put the icon from pixbuf */
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_rectangle (cr, 0, 0, 48, 48);
    cairo_fill (cr);

    /* draw black borders */
    cairo_rectangle (cr, 13, 12, 10, 2);
    cairo_rectangle (cr, 21, 14, 2, 24);
    cairo_rectangle (cr, 13, 36, 10, 2);
    cairo_rectangle (cr, 13, 14, 2, 24);
    cairo_rectangle (cr, 25, 12, 10, 2);
    cairo_rectangle (cr, 33, 14, 2, 24);
    cairo_rectangle (cr, 25, 36, 10, 2);
    cairo_rectangle (cr, 25, 14, 2, 24);
    cairo_set_source_rgba (cr, 0, 0, 0, 0.6);
    cairo_fill (cr);

    /* draw white rectangles */
    cairo_rectangle (cr, 15, 14, 6, 22);
    cairo_rectangle (cr, 27, 14, 6, 22);
    cairo_set_source_rgba (cr, 1, 1, 1, 0.8);
    cairo_fill (cr);

    cairo_destroy (cr);
    pb = gdk_pixbuf_get_from_surface (s, 0, 0, 48, 48);
    cairo_surface_destroy (s);

    iconset = gtk_icon_set_new_from_pixbuf (pb);
    g_object_unref (G_OBJECT (pb));
    return iconset;
}

static GtkIconSet *
get_gray_iconset (GdkPixbuf *pixbuf, GdkPixbuf **pixbuf_gray)
{
    cairo_surface_t *s;
    cairo_pattern_t *pattern;
    cairo_t         *cr;
    GdkPixbuf       *pb;
    GtkIconSet      *iconset;

    s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 48, 48);
    cr = cairo_create (s);

    /* put the icon from pixbuf */
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_rectangle (cr, 0, 0, 48, 48);
    cairo_fill (cr);

    /* use saturation to turn it gray */
    pattern = cairo_pattern_create_for_surface (s);
    cairo_rectangle (cr, 0, 0, 48, 48);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_HSL_SATURATION);
    cairo_mask (cr, pattern);

    cairo_pattern_destroy (pattern);
    cairo_destroy (cr);
    pb = gdk_pixbuf_get_from_surface (s, 0, 0, 48, 48);
    cairo_surface_destroy (s);

    iconset = gtk_icon_set_new_from_pixbuf (pb);
    *pixbuf_gray = pb;
    return iconset;
}
#endif

static gboolean
opt_debug (const gchar  *option _UNUSED_,
           const gchar  *value  _UNUSED_,
           gpointer      data   _UNUSED_,
           GError      **error  _UNUSED_)
{
    ++config->is_debug;
    return TRUE;
}

int
main (int argc, char *argv[])
{
    GError          *error = NULL;
#ifndef DISABLE_GUI
    GInputStream    *stream;
    GtkIconFactory  *factory;
    GtkIconSet      *iconset;
    GdkPixbuf       *pixbuf;
    GdkPixbuf       *pixbuf_gray;
#endif
    gchar            conffile[MAX_PATH];

    config = new0 (config_t, 1);
#ifndef DISABLE_GUI
    zero (kalpm_state);
#endif

    setlocale (LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, LOCALEDIR);
    textdomain (PACKAGE);
#endif

    set_user_agent ();

    /* parse command line */
    gboolean         show_version       = FALSE;
    gboolean         run_manual_checks  = FALSE;
    gboolean         run_auto_checks    = FALSE;
    GOptionEntry     options[] = {
        { "auto-checks",    'a', 0, G_OPTION_ARG_NONE, &run_auto_checks,
            N_("Run automatic checks"), NULL },
        { "manual-checks",  'm', 0, G_OPTION_ARG_NONE, &run_manual_checks,
            N_("Run manual checks"), NULL },
        { "debug",          'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
            opt_debug, N_("Enable debug mode"), NULL },
        { "version",        'V', 0, G_OPTION_ARG_NONE, &show_version,
            N_("Show version information"), NULL },
        { NULL }
    };

    if (argc > 1)
    {
        GOptionContext *context;

        context = g_option_context_new (NULL);
        g_option_context_add_main_entries (context, options, NULL);
#ifdef ENABLE_NLS
        g_option_context_set_translation_domain (context, PACKAGE);
#endif

        if (!g_option_context_parse (context, &argc, &argv, &error))
        {
            fprintf (stderr, _("option parsing failed: %s\n"), error->message);
            g_option_context_free (context);
            return 1;
        }
        if (show_version)
        {
            puts ("kalu - " PACKAGE_TAG " v" PACKAGE_VERSION "\n"
                  "Copyright (C) 2012-2013 Olivier Brunel\n"
                  "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
                  "This is free software: you are free to change and redistribute it.\n"
                  "There is NO WARRANTY, to the extent permitted by law."
                 );
            g_option_context_free (context);
            return 0;
        }
        if (config->is_debug)
        {
            debug ("kalu v" PACKAGE_VERSION " -- debug mode enabled (level %d)",
                    config->is_debug);
        }
#ifndef DISABLE_GUI
        if (run_manual_checks || run_auto_checks)
        {
            is_cli = TRUE;
        }
#endif
        g_option_context_free (context);
    }

    config->pacmanconf = strdup ("/etc/pacman.conf");
    config->interval = 3600; /* 1 hour */
    config->timeout = NOTIFY_EXPIRES_DEFAULT;
    config->notif_icon = ICON_KALU;
    config->syncdbs_in_tooltip = TRUE;
    config->checks_manual = CHECK_UPGRADES | CHECK_WATCHED | CHECK_AUR
        | CHECK_WATCHED_AUR | CHECK_NEWS;
    config->checks_auto   = CHECK_UPGRADES | CHECK_WATCHED | CHECK_AUR
        | CHECK_WATCHED_AUR | CHECK_NEWS;
    config->auto_notifs = TRUE;
    config->notif_buttons = TRUE;
#ifndef DISABLE_UPDATER
    config->action = UPGRADE_ACTION_KALU;
    config->confirm_post = TRUE;
#else
    config->action = UPGRADE_NO_ACTION;
#endif
    config->on_sgl_click = DO_LAST_NOTIFS;
    config->on_dbl_click = DO_CHECK;
    config->on_mdl_click = DO_TOGGLE_PAUSE;
    config->on_sgl_click_paused = DO_SAME_AS_ACTIVE;
    config->on_dbl_click_paused = DO_SAME_AS_ACTIVE;
    config->on_mdl_click_paused = DO_SAME_AS_ACTIVE;
    config->check_pacman_conflict = TRUE;
#ifndef DISABLE_GUI
    config->cmdline_link = strdup ("xdg-open '$URL'");
#endif

    config->tpl_upgrades = new0 (templates_t, 1);
    config->tpl_upgrades->title = strdup (_("$NB updates available (D: $DL; N: $NET)"));
    config->tpl_upgrades->package = strdup (_("- <b>$PKG</b> $OLD > <b>$NEW</b> (D: $DL; N: $NET)"));
    config->tpl_upgrades->sep = strdup ("\n");

    config->tpl_watched = new0 (templates_t, 1);
    config->tpl_watched->title = strdup (_("$NB watched packages updated (D: $DL; N: $NET)"));

    config->tpl_aur = new0 (templates_t, 1);
    config->tpl_aur->title = strdup (_("AUR: $NB packages updated"));
    config->tpl_aur->package = strdup ("- <b>$PKG</b> $OLD > <b>$NEW</b>");

    config->tpl_watched_aur = new0 (templates_t, 1);
    config->tpl_watched_aur->title = strdup (_("AUR: $NB watched packages updated"));

    config->tpl_news = new0 (templates_t, 1);
    config->tpl_news->title = strdup (_("$NB unread news"));
    config->tpl_news->package = strdup ("- $NEWS");

#ifndef DISABLE_GUI
    if (!is_cli)
    {
        if (!gtk_init_check (&argc, &argv))
        {
            fputs (_("GTK+ initialization failed\n"), stderr);
            puts (_("To run kalu on CLI only mode, use --auto-checks or --manual-checks"));
            free_config ();
            return 1;
        }
    }
#endif

    /* parse config */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/kalu.conf",
            g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_KALU, &error))
    {
        do_show_error (_("Errors while parsing configuration"),
                error->message, NULL);
        g_clear_error (&error);
    }
    /* parse watched */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/watched.conf",
            g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_WATCHED, &error))
    {
        do_show_error (_("Unable to parse watched packages"),
                error->message, NULL);
        g_clear_error (&error);
    }
    /* parse watched aur */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/watched-aur.conf",
            g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_WATCHED_AUR, &error))
    {
        do_show_error (_("Unable to parse watched AUR packages"),
                error->message, NULL);
        g_clear_error (&error);
    }
    /* parse news */
    snprintf (conffile, MAX_PATH - 1, "%s/.config/kalu/news.conf",
            g_get_home_dir ());
    if (!parse_config_file (conffile, CONF_FILE_NEWS, &error))
    {
        do_show_error (_("Unable to parse last news data"),
                error->message, NULL);
        g_clear_error (&error);
    }

    if (curl_global_init (CURL_GLOBAL_ALL) == 0)
    {
        config->is_curl_init = TRUE;
    }
    else
    {
        do_show_error (
                _("Unable to initialize cURL"),
                _("kalu will therefore not be able to check the AUR"),
                NULL);
    }

#ifndef DISABLE_GUI
    if (run_manual_checks || run_auto_checks)
    {
#endif
        kalu_check_work (run_auto_checks);
#ifndef DISABLE_GUI
        goto eop;
    }

    /* icon stuff: so we'll be able to use "kalu-logo" from stock, and it will
     * handle multiple size/resize as well as graying out (e.g. on unsensitive
     * widgets) automatically */
    factory = gtk_icon_factory_new ();
    /* kalu-logo */
    pixbuf = gdk_pixbuf_new_from_file (KALU_LOGO, NULL);
    if (!pixbuf)
    {
        /* fallback to inline logo */
        stream = g_memory_input_stream_new_from_data (
                &_binary_kalu_logo_start,
                (gssize) &_binary_kalu_logo_size,
                NULL);
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (G_OBJECT (stream));
    }
    iconset = gtk_icon_set_new_from_pixbuf (pixbuf);
    gtk_icon_factory_add (factory, "kalu-logo", iconset);
    /* add paused version */
    iconset = get_paused_iconset (pixbuf);
    gtk_icon_factory_add (factory, "kalu-logo-paused", iconset);
    /* kalu-logo-gray */
    iconset = get_gray_iconset (pixbuf, &pixbuf_gray);
    gtk_icon_factory_add (factory, "kalu-logo-gray", iconset);
    /* add paused version */
    iconset = get_paused_iconset (pixbuf_gray);
    gtk_icon_factory_add (factory, "kalu-logo-gray-paused", iconset);
    /* free pixbufs */
    g_object_unref (G_OBJECT (pixbuf));
    g_object_unref (G_OBJECT (pixbuf_gray));
    /* add it all */
    gtk_icon_factory_add_default (factory);

    icon = gtk_status_icon_new_from_stock ("kalu-logo-gray");
    gtk_status_icon_set_name (icon, "kalu");
    gtk_status_icon_set_title (icon, "kalu");
    gtk_status_icon_set_tooltip_text (icon, "kalu");

    g_signal_connect (G_OBJECT (icon), "query-tooltip",
            G_CALLBACK (icon_query_tooltip_cb), NULL);
    g_signal_connect (G_OBJECT (icon), "popup-menu",
            G_CALLBACK (icon_popup_cb), NULL);
    g_signal_connect (G_OBJECT (icon), "button-press-event",
            G_CALLBACK (icon_press_cb), NULL);

    gtk_status_icon_set_visible (icon, TRUE);

    /* takes care of setting timeout_skip (if needed) and also triggers the
     * auto-checks (unless within skip period) */
#if 1
    skip_next_timeout ();
#endif

    notify_init ("kalu");
    gtk_main ();
eop:
    if (!is_cli)
    {
        notify_uninit ();
    }
#endif /* DISABLE_GUI */
    if (config->is_curl_init)
    {
        curl_global_cleanup ();
    }
    free_config ();
#ifndef DISABLE_GUI
    if (open_windows)
    {
        g_ptr_array_free (open_windows, TRUE);
    }
#endif
    return 0;
}
