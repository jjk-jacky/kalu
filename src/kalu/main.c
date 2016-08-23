/**
 * kalu - Copyright (C) 2012-2016 Olivier Brunel
 *
 * main.c
 * Copyright (C) 2012-2016 Olivier Brunel <jjk@jjacky.com>
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
#include <signal.h>
#ifndef DISABLE_GUI
/* FIFO */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* curl */
#include <curl/curl.h>

/* statusnotifier */
#ifdef ENABLE_STATUS_NOTIFIER
#include <statusnotifier.h>
#endif

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

const gchar *tpl_names[_NB_TPL] = {
    "upgrades",
    "watched",
    "aur",
    "aur-not-found",
    "watched-aur",
    "news"
};
const gchar *fld_names[_NB_FLD] = {
    "Title",
    "Package",
    "Sep"
};
const gchar *tpl_sce_names[_NB_TPL_SCE] = {
    "", /* TPL_SCE_UNDEFINED */
    "DEFAULT",
    "FALLBACK",
    "CUSTOM",
    "NONE"
};


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

static const char *
get_fld_value (tpl_t tpl, unsigned int fld)
{
    templates_t *t = &config->templates[tpl];

    switch (t->fields[fld].source)
    {
        case TPL_SCE_DEFAULT:
            return t->fields[fld].def;

        case TPL_SCE_FALLBACK:
            return get_fld_value (t->fallback, fld);

        case TPL_SCE_CUSTOM:
            return t->fields[fld].custom;

        case TPL_SCE_NONE:
        case TPL_SCE_UNDEFINED: /* silence warning */
        default: /* silence warning */
            return NULL;
    }
}

static void
notify_updates (
        alpm_list_t *packages,
        check_t      type,
        gchar       *xml_news,
        gboolean     show_it
        )
{
    alpm_list_t     *i;

    unsigned int     nb          = 0;
    int              net_size;
    off_t            dsize       = 0;
    off_t            isize       = 0;
    off_t            nsize       = 0;

    gchar           *summary;
    gchar           *text = NULL;
    unsigned int     alloc;
    unsigned int     len;
    char             buf[255];
    char            *s;
    const char      *b;
    const char      *fields[_NB_FLD];
    tpl_t            tpl;
    const char      *unit;
    double           size_h;
    replacement_t   *replacements[9];
    gboolean         escaping = FALSE;
    GString         *string_pkgs = NULL;     /* list of AUR packages */

#ifdef DISABLE_GUI
    (void) xml_news;
    (void) show_it;
#else
    escaping = !is_cli;
#endif

    if (type & CHECK_UPGRADES)
        tpl = TPL_UPGRADES;
    else if (type & CHECK_WATCHED)
        tpl = TPL_WATCHED;
    else if (type & CHECK_AUR)
    {
        tpl = TPL_AUR;
        /* if needed, init the string that will hold the list of packages */
        if (config->cmdline_aur)
            string_pkgs = g_string_sized_new (255);
    }
    else if (type & CHECK_WATCHED_AUR)
        tpl = TPL_WATCHED_AUR;
    else if (type & CHECK_NEWS)
        tpl = TPL_NEWS;
    else /* _CHECK_AUR_NOT_FOUND */
        tpl = TPL_AUR_NOT_FOUND;

    fields[FLD_TITLE]   = get_fld_value (tpl, FLD_TITLE);
    fields[FLD_PACKAGE] = get_fld_value (tpl, FLD_PACKAGE);
    fields[FLD_SEP]     = get_fld_value (tpl, FLD_SEP);

    if (fields[FLD_PACKAGE] || string_pkgs)
    {
        if (fields[FLD_PACKAGE])
        {
            alloc = 1024;
            text = new (gchar, alloc + 1);
            len = 0;
        }

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
                if (nb > 1 && fields[FLD_SEP])
                {
                    s = text + len;
                    for (b = fields[FLD_SEP]; *b; ++b, ++s, ++len)
                    {
                        *s = *b;
                    }
                }

                parse_tpl (fields[FLD_PACKAGE], &text, &len, &alloc,
                        replacements, escaping);

                free (replacements[0]);

                debug ("-> %s", (char *) i->data);
            }
            else
            {
                kalu_package_t *pkg = i->data;

                if (fields[FLD_PACKAGE])
                {
                    int j;

                    net_size = (int) (pkg->new_size - pkg->old_size);
                    dsize += pkg->dl_size;
                    isize += pkg->new_size;
                    nsize += net_size;

                    replacements[0] = new0 (replacement_t, 1);
                    replacements[0]->name = "REPO";
                    replacements[0]->value = (pkg->repo) ? pkg->repo : (char *) "-";
                    replacements[0]->need_escaping = TRUE;
                    replacements[1] = new0 (replacement_t, 1);
                    replacements[1]->name = "PKG";
                    replacements[1]->value = pkg->name;
                    replacements[1]->need_escaping = TRUE;
                    replacements[2] = new0 (replacement_t, 1);
                    replacements[2]->name = "OLD";
                    replacements[2]->value = pkg->old_version;
                    replacements[3] = new0 (replacement_t, 1);
                    replacements[3]->name = "NEW";
                    replacements[3]->value = pkg->new_version;
                    replacements[4] = new0 (replacement_t, 1);
                    replacements[4]->name = "DL";
                    size_h = humanize_size (pkg->dl_size, '\0', &unit);
                    snprint_size (buf, 255, size_h, unit);
                    replacements[4]->value = strdup (buf);
                    replacements[5] = new0 (replacement_t, 1);
                    replacements[5]->name = "INS";
                    size_h = humanize_size (pkg->new_size, '\0', &unit);
                    snprint_size (buf, 255, size_h, unit);
                    replacements[5]->value = strdup (buf);
                    replacements[6] = new0 (replacement_t, 1);
                    replacements[6]->name = "NET";
                    size_h = humanize_size (net_size, '\0', &unit);
                    snprint_size (buf, 255, size_h, unit);
                    replacements[6]->value = strdup (buf);
                    replacements[7] = new0 (replacement_t, 1);
                    replacements[7]->name = "DESC";
                    replacements[7]->value = pkg->desc;
                    replacements[7]->need_escaping = TRUE;
                    replacements[8] = NULL;

                    /* add separator? */
                    if (nb > 1 && fields[FLD_SEP])
                    {
                        s = text + len;
                        for (b = fields[FLD_SEP]; *b; ++b, ++s, ++len)
                        {
                            *s = *b;
                        }
                    }

                    parse_tpl (fields[FLD_PACKAGE], &text, &len, &alloc,
                            replacements, escaping);

                    free (replacements[4]->value);
                    free (replacements[5]->value);
                    free (replacements[6]->value);
                    for (j = 0; j < 7; ++j)
                        free (replacements[j]);

                    debug ("-> %s %s -> %s [dl=%d; ins=%d]",
                            pkg->name,
                            pkg->old_version,
                            pkg->new_version,
                            (int) pkg->dl_size,
                            (int) pkg->new_size);
                }

                /* construct list of packages, for use in cmdline */
                if (string_pkgs)
                {
                    string_pkgs = g_string_append (string_pkgs, pkg->name);
                    string_pkgs = g_string_append_c (string_pkgs, ' ');
                }
            }
        }
    }

    alloc = 255;
    summary = new0 (gchar, alloc + 1);
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

    parse_tpl (fields[FLD_TITLE], &summary, &len, &alloc,
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
        if (text)
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
    else if (!(type & _CHECK_AUR_NOT_FOUND))
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
    gboolean     got_something      = FALSE;
#ifndef DISABLE_GUI
    gint         nb_upgrades        = -1;
    gint         nb_watched         = -1;
    gint         nb_aur             = -1;
    gint         nb_aur_not_found   = -1;
    gint         nb_watched_aur     = -1;
    gint         nb_news            = -1;
#endif /* DISABLE_GUI */
    unsigned int checks             = (is_auto)
        ? config->checks_auto
        : config->checks_manual;
    gboolean     show_it            = (is_auto) ? config->auto_notifs : TRUE;

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
        if (!kalu_alpm_load (NULL, config->pacmanconf,
#ifndef DISABLE_GUI
                    get_kalpm_synced_dbs (),
#else
                    NULL,
#endif
                    &error))
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
                && !kalu_alpm_syncdbs (
#ifndef DISABLE_GUI
                    get_kalpm_synced_dbs (),
#else
                    NULL,
#endif
                    &error))
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
                             * notify_error) because we want the buttons
                             * ("Update system") to be featured. */
                            notif_t *notif;

                            notif = new (notif_t, 1);
                            notif->type = CHECK_UPGRADES;
                            notif->summary = strdup (_("Unable to compile list of packages"));
                            notif->text = strdup (error->message);
                            notif->data = NULL;

                            /* add the notif to the last of last notifications,
                             * so we can re-show it later */
                            debug ("adding new notif (%s) to last_notifs",
                                    notif->summary);
                            config->last_notifs = alpm_list_add (
                                    config->last_notifs,
                                    notif);
                            /* mark icon blue, upgrades are available, we just
                             * don't know which/how many (due to the conflict) */
                            nb_upgrades = UPGRADES_NB_CONFLICT;
                            /* show notification */
                            show_notif (notif);
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
                alpm_list_t *not_found = NULL;

                packages = NULL;
                if (aur_has_updates (&packages, &not_found, aur_pkgs, FALSE, &error))
                {
                    got_something = TRUE;
#ifndef DISABLE_GUI
                    nb_aur = (gint) alpm_list_count (packages);
#endif
                    notify_updates (packages, CHECK_AUR, NULL, show_it);
                    FREE_PACKAGE_LIST (packages);
                    if (not_found)
                    {
#ifndef DISABLE_GUI
                        nb_aur_not_found = (gint) alpm_list_count (not_found);
#endif
                        notify_updates (not_found, _CHECK_AUR_NOT_FOUND, NULL, show_it);
                        FREE_PACKAGE_LIST (not_found);
                    }
                }
#ifndef DISABLE_GUI
                else if (error == NULL)
                {
                    nb_aur = 0;
                    if (not_found)
                    {
                        nb_aur_not_found = (gint) alpm_list_count (not_found);
                        notify_updates (not_found, _CHECK_AUR_NOT_FOUND, NULL, show_it);
                        FREE_PACKAGE_LIST (not_found);
                    }
                    else
                    {
                        nb_aur_not_found = 0;
                    }
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
                nb_aur = nb_aur_not_found = 0;
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
                if (nb_aur_not_found >= 0)
                {
                    set_kalpm_nb (_CHECK_AUR_NOT_FOUND, nb_aur_not_found, FALSE);
                }
#endif
        }

        kalu_alpm_free ();
    }

    if (checks & CHECK_WATCHED_AUR && config->watched_aur /* NULL if not watched aur pkgs */)
    {
        packages = NULL;
        if (aur_has_updates (&packages, NULL, config->watched_aur, TRUE, &error))
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
    int tpl, fld;

    if (config == NULL)
    {
        return;
    }

    free (config->pacmanconf);

    /* templates: custom values */
    for (tpl = 0; tpl < _NB_TPL; ++tpl)
        for (fld = 0; fld < _NB_FLD; ++fld)
            free (config->templates[tpl].fields[fld].custom);

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
    free (package->repo);
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
#ifdef ENABLE_STATUS_NOTIFIER
extern StatusNotifier *sn;
extern GdkPixbuf *sn_icon[NB_SN_ICONS];
#endif
extern GtkStatusIcon *icon;
extern GPtrArray     *open_windows;

static GdkPixbuf *
get_paused_pixbuf (GdkPixbuf *pixbuf)
{
    cairo_surface_t *s;
    cairo_t         *cr;
    GdkPixbuf       *pb;

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

    return pb;
}

static GdkPixbuf *
get_gray_pixbuf (GdkPixbuf *pixbuf)
{
    cairo_surface_t *s;
    cairo_pattern_t *pattern;
    cairo_t         *cr;
    GdkPixbuf       *pb;

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

    return pb;
}

struct fifo
{
    gchar *name;
    gint   fd;

    gchar  buf[128];
    gchar *b;
};

static void
free_fifo (struct fifo *fifo)
{
    if (fifo->fd >= 0)
    {
        close (fifo->fd);
        fifo->fd = -1;
    }

    if (fifo->name)
    {
        if (unlink (fifo->name) < 0)
        {
            gint _errno = errno;
            debug ("failed to remove fifo: %s", g_strerror (_errno));
        }
        g_free (fifo->name);
        fifo->name = NULL;
    }
}

static gboolean
dispatch (GSource *source _UNUSED_, GSourceFunc callback, gpointer data)
{
    return callback (data);
}

static gboolean read_fifo (struct fifo *fifo);

static void
open_fifo (struct fifo *fifo)
{
    fifo->fd = open (fifo->name, O_RDONLY | O_NONBLOCK);
    if (fifo->fd < 0)
    {
        gint _errno = errno;
        debug ("failed to open fifo: %s", g_strerror (_errno));
        do_show_error (_("Unable to open FIFO"), g_strerror (_errno), NULL);
        free_fifo (fifo);
    }
    else
    {
        static GSourceFuncs funcs = {
            .prepare = NULL,
            .check = NULL,
            .dispatch = dispatch,
            .finalize = NULL
        };
        GSource *source;

        debug ("opened fifo: %d", fifo->fd);
        fifo->b = fifo->buf;

        source = g_source_new (&funcs, sizeof (GSource));
        g_source_add_unix_fd (source, fifo->fd, G_IO_IN);
        g_source_set_callback (source, (GSourceFunc) read_fifo, fifo, NULL);
        g_source_attach (source, NULL);
        g_source_unref (source);
    }
}

static gboolean
read_fifo (struct fifo *fifo)
{
    gssize len;

again:
    len = read (fifo->fd, fifo->b, 127 - (gsize) (fifo->b - fifo->buf));

    if (len < 0)
    {
        if (errno == EINTR)
            goto again;
        else
        {
            gint _errno = errno;
            debug ("error reading from fifo: %s", g_strerror (_errno));
        }
    }
    else
    {
        gchar *s;

        if (len == 0)
        {
            if (fifo->b > fifo->buf)
                *fifo->b++ = '\n';
            else
                goto skip;
        }

repeat:
        fifo->b[len] = '\0';
        s = strchr (fifo->buf, '\n');
        if (s)
        {
            *s = '\0';
            process_fifo_command (fifo->buf);
            if (fifo->b + len > s + 1)
            {
                memmove (fifo->buf, s + 1, (gsize) (fifo->b + len - s - 1));
                len = fifo->b + len - s - 1;
                fifo->b = fifo->buf;
                goto repeat;
            }
            else
                fifo->b = fifo->buf;
        }
        else
            fifo->b += len;

        if (len == 0)
        {
skip:
            close (fifo->fd);
            open_fifo (fifo);
            return FALSE;
        }

        if (G_UNLIKELY (fifo->b >= fifo->buf + 127))
        {
            do_show_error (_("Received too much invalid data on FIFO, resetting"),
                    NULL, NULL);
            fifo->b = fifo->buf;
        }
    }

    return TRUE;
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

#ifndef DISABLE_GUI
static void
create_status_icon (void)
{
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    debug ("create GtkStatusIcon");
    icon = gtk_status_icon_new_from_icon_name ("kalu-gray");
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
    G_GNUC_END_IGNORE_DEPRECATIONS
}
#endif

#ifdef ENABLE_STATUS_NOTIFIER
static void
sn_state_cb (void)
{
    if (status_notifier_get_state (sn) == STATUS_NOTIFIER_STATE_REGISTERED)
    {
        debug ("StatusNotifier registered");
        if (icon)
        {
            debug ("removing GtkStatusIcon");
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_status_icon_set_visible (icon, FALSE);
            G_GNUC_END_IGNORE_DEPRECATIONS
            g_object_unref (icon);
            icon = NULL;
        }
    }
}

static void
sn_reg_failed (StatusNotifier *_sn _UNUSED_, GError *error)
{
    guint i;

    debug ("StatusNotifier registration failed: %s", error->message);
    if (status_notifier_get_state (sn) == STATUS_NOTIFIER_STATE_FAILED)
    {
        debug ("no possible recovery; destroy StatusNotifier");
        g_object_unref (sn);
        sn = NULL;
        for (i = 0; i < NB_SN_ICONS; ++i)
            if (sn_icon[i])
            {
                g_object_unref (sn_icon[i]);
                sn_icon[i] = NULL;
            }
    }
    /* fallback to systray */
    create_status_icon ();
}
#endif

#ifndef DISABLE_GUI
static void
sig_handler (int sig _UNUSED_)
{
    if (kalpm_state.is_busy)
        return;
    gtk_main_quit ();
}

static inline void
set_sighandlers (void)
{
    struct sigaction sa;

    sa.sa_handler = sig_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction (SIGINT,  &sa, NULL);
    sigaction (SIGTERM, &sa, NULL);
}
#endif

int
main (int argc, char *argv[])
{
    GError          *error = NULL;
#ifndef DISABLE_GUI
    GtkIconTheme    *icon_theme;
    GdkPixbuf       *pixbuf;
    GdkPixbuf       *pixbuf_kalu;
    struct fifo      fifo = { .fd = -1, .name = NULL };
    gint             r;
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
    gchar           *tmp_dbpath         = NULL;
    gboolean         keep_tmp_dbpath    = FALSE;
    GOptionEntry     options[] = {
        { "auto-checks",    'a', 0, G_OPTION_ARG_NONE, &run_auto_checks,
            N_("Run automatic checks"), NULL },
        { "manual-checks",  'm', 0, G_OPTION_ARG_NONE, &run_manual_checks,
            N_("Run manual checks"), NULL },
        { "tmp-dbpath",     'T', 0, G_OPTION_ARG_STRING, &tmp_dbpath,
            N_("Use PATH as temporary dbpath"), "PATH" },
        { "keep-tmp-dbpath",'K', 0, G_OPTION_ARG_NONE, &keep_tmp_dbpath,
            N_("Keep tmp dbpath folder"), NULL },
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
                  "Copyright (C) 2012-2016 Olivier Brunel\n"
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
        if (tmp_dbpath)
            kalu_alpm_set_tmp_dbpath (tmp_dbpath);
        g_option_context_free (context);
    }

    config->pacmanconf = strdup ("/etc/pacman.conf");
    config->interval = 3600; /* 1 hour */
    config->timeout = NOTIFY_EXPIRES_DEFAULT;
    config->notif_icon = ICON_KALU;
    config->notif_icon_size = 20;
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

    config->templates[TPL_UPGRADES].fallback = NO_TPL;
    config->templates[TPL_UPGRADES].fields[FLD_TITLE].def
        = _("$NB updates available (D: $DL; N: $NET)");
    config->templates[TPL_UPGRADES].fields[FLD_PACKAGE].def
        = _("- <b>$PKG</b> $OLD > <b>$NEW</b> (D: $DL; N: $NET)");
    config->templates[TPL_UPGRADES].fields[FLD_SEP].def
        = "\n";

    config->templates[TPL_WATCHED].fallback = TPL_UPGRADES;
    config->templates[TPL_WATCHED].fields[FLD_TITLE].def
        = _("$NB watched packages updated (D: $DL; N: $NET)");

    config->templates[TPL_AUR].fallback = TPL_UPGRADES;
    config->templates[TPL_AUR].fields[FLD_TITLE].def
        = _("AUR: $NB packages updated");
    config->templates[TPL_AUR].fields[FLD_PACKAGE].def
        = "- <b>$PKG</b> $OLD > <b>$NEW</b>";

    config->templates[TPL_AUR_NOT_FOUND].fallback = TPL_UPGRADES;
    config->templates[TPL_AUR_NOT_FOUND].fields[FLD_TITLE].def
        = _("$NB packages not found in AUR");
    config->templates[TPL_AUR_NOT_FOUND].fields[FLD_PACKAGE].def
        = _("- <b>$PKG</b> ($OLD)");

    config->templates[TPL_WATCHED_AUR].fallback = TPL_AUR;
    config->templates[TPL_WATCHED_AUR].fields[FLD_TITLE].def
        = _("AUR: $NB watched packages updated");

    config->templates[TPL_NEWS].fallback = TPL_UPGRADES;
    config->templates[TPL_NEWS].fields[FLD_TITLE].def
        = _("$NB unread news");
    config->templates[TPL_NEWS].fields[FLD_PACKAGE].def
        = "- $NEWS";

#ifndef DISABLE_UPDATER
    config->color_unimportant = strdup ("gray");
    config->color_info = strdup ("blue");
    config->color_warning = strdup ("green");
    config->color_error = strdup ("red");
#endif

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
    snprintf (conffile, MAX_PATH - 1, "%s/kalu/kalu.conf",
            g_get_user_config_dir ());
    if (!parse_config_file (conffile, CONF_FILE_KALU, &error))
    {
        do_show_error (_("Errors while parsing configuration"),
                error->message, NULL);
        g_clear_error (&error);
    }
    /* parse watched */
    snprintf (conffile, MAX_PATH - 1, "%s/kalu/watched.conf",
            g_get_user_config_dir ());
    if (!parse_config_file (conffile, CONF_FILE_WATCHED, &error))
    {
        do_show_error (_("Unable to parse watched packages"),
                error->message, NULL);
        g_clear_error (&error);
    }
    /* parse watched aur */
    snprintf (conffile, MAX_PATH - 1, "%s/kalu/watched-aur.conf",
            g_get_user_config_dir ());
    if (!parse_config_file (conffile, CONF_FILE_WATCHED_AUR, &error))
    {
        do_show_error (_("Unable to parse watched AUR packages"),
                error->message, NULL);
        g_clear_error (&error);
    }
    /* parse news */
    snprintf (conffile, MAX_PATH - 1, "%s/kalu/news.conf",
            g_get_user_config_dir ());
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

    /* FIFO */
    fifo.name = g_strdup_printf ("%s/kalu_fifo_%d",
            g_get_user_runtime_dir (), getpid ());
    r = mkfifo (fifo.name, 0600);
    if (r < 0)
    {
        gint _errno = errno;
        debug ("failed to create fifo: %s: %s", fifo.name, g_strerror (_errno));
        do_show_error (_("Unable to create FIFO"), g_strerror (_errno), NULL);
        g_free (fifo.name);
        fifo.name = NULL;
    }
    else
    {
        debug ("created fifo: %s", fifo.name);
        open_fifo (&fifo);
    }

    /* icon stuff: we use 4 icons - "kalu", "kalu-paused", "kalu-gray" and
     * "kalu-gray-paused" - from the theme.
     * Using icon name allows user to easily specify icons (putting files in
     * ~/.local/share/icons) for each of the 4 icons. Because we need them all,
     * we ensure they all exists, and if not create them.
     * "kalu" will come from kalu's binary
     * "kalu-paused" comes from "kalu" w/ added bars
     * "kalu-gray" comes from "kalu" but in gray
     * "kaly-gray-paused" comes from "kalu-gray" w/ added bars */

    icon_theme = gtk_icon_theme_get_default ();
    pixbuf_kalu = NULL;

    /* kalu */
    if (!gtk_icon_theme_has_icon (icon_theme, "kalu"))
    {
        GInputStream *stream;

        /* fallback to inline logo */
        debug ("No icon 'kalu' in theme -- load inline logo");
        stream = g_memory_input_stream_new_from_data (
                &_binary_kalu_logo_start,
                (gssize) &_binary_kalu_logo_size,
                NULL);
        pixbuf_kalu = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (G_OBJECT (stream));
#ifdef ENABLE_STATUS_NOTIFIER
        sn_icon[SN_ICON_KALU] = g_object_ref (pixbuf_kalu);
#endif

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_icon_theme_add_builtin_icon ("kalu", 48, pixbuf_kalu);
        G_GNUC_END_IGNORE_DEPRECATIONS
    }

    /* kalu-paused */
    if (!gtk_icon_theme_has_icon (icon_theme, "kalu-paused"))
    {
        if (!pixbuf_kalu)
            pixbuf_kalu = gtk_icon_theme_load_icon (icon_theme, "kalu", 48, 0, NULL);

        debug ("No icon 'kalu-paused' -- creating it");
        pixbuf = get_paused_pixbuf (pixbuf_kalu);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_icon_theme_add_builtin_icon ("kalu-paused", 48, pixbuf);
        G_GNUC_END_IGNORE_DEPRECATIONS
#ifdef ENABLE_STATUS_NOTIFIER
        sn_icon[SN_ICON_KALU_PAUSED] = g_object_ref (pixbuf);
#endif
        g_object_unref (pixbuf);
    }

    /* kalu-gray */
    if (!gtk_icon_theme_has_icon (icon_theme, "kalu-gray"))
    {
        if (!pixbuf_kalu)
            pixbuf_kalu = gtk_icon_theme_load_icon (icon_theme, "kalu", 48, 0, NULL);

        debug ("No icon 'kalu-gray' -- creating it");
        pixbuf = get_gray_pixbuf (pixbuf_kalu);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_icon_theme_add_builtin_icon ("kalu-gray", 48, pixbuf);
        G_GNUC_END_IGNORE_DEPRECATIONS
#ifdef ENABLE_STATUS_NOTIFIER
        sn_icon[SN_ICON_KALU_GRAY] = g_object_ref (pixbuf);
#endif
        g_object_unref (pixbuf);
    }

    if (pixbuf_kalu)
        g_object_unref (pixbuf_kalu);

    /* kalu-gray-paused */
    if (!gtk_icon_theme_has_icon (icon_theme, "kalu-gray-paused"))
    {
        pixbuf_kalu = gtk_icon_theme_load_icon (icon_theme, "kalu-gray", 48, 0, NULL);

        debug ("No icon 'kalu-gray-paused' -- creating it");
        pixbuf = get_paused_pixbuf (pixbuf_kalu);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_icon_theme_add_builtin_icon ("kalu-gray-paused", 48, pixbuf);
        G_GNUC_END_IGNORE_DEPRECATIONS
#ifdef ENABLE_STATUS_NOTIFIER
        sn_icon[SN_ICON_KALU_GRAY_PAUSED] = g_object_ref (pixbuf);
#endif
        g_object_unref (pixbuf);
        g_object_unref (pixbuf_kalu);
    }

#ifdef ENABLE_STATUS_NOTIFIER
    debug ("create StatusNotifier");
    sn = STATUS_NOTIFIER (g_object_new (TYPE_STATUS_NOTIFIER,
            "id",               "kalu",
            "title",            "kalu",
            "tooltip-title",    "kalu",
            NULL));
    if (sn_icon[SN_ICON_KALU_GRAY])
        g_object_set (G_OBJECT (sn),
                "main-icon-pixbuf",     sn_icon[SN_ICON_KALU_GRAY],
                "tooltip-icon-pixbuf",  sn_icon[SN_ICON_KALU_GRAY],
                NULL);
    else
        g_object_set (G_OBJECT (sn),
                "main-icon-name",       "kaly-gray",
                "tooltip-icon-name",    "kaly-gray",
                NULL);
    g_signal_connect (G_OBJECT (sn), "notify::state",
            G_CALLBACK (sn_state_cb), NULL);
    g_signal_connect (G_OBJECT (sn), "registration-failed",
            G_CALLBACK (sn_reg_failed), NULL);
    g_signal_connect (G_OBJECT (sn), "context-menu",
            G_CALLBACK (sn_context_menu_cb), NULL);
    g_signal_connect_swapped (G_OBJECT (sn), "activate",
            G_CALLBACK (sn_cb), GUINT_TO_POINTER (SN_ACTIVATE));
    g_signal_connect_swapped (G_OBJECT (sn), "secondary-activate",
            G_CALLBACK (sn_cb), GUINT_TO_POINTER (SN_SECONDARY_ACTIVATE));
    status_notifier_register (sn);
#else
    /* create systray icon */
    create_status_icon ();
#endif

    /* takes care of setting timeout_skip (if needed) and also triggers the
     * auto-checks (unless within skip period) */
#if 1
    skip_next_timeout ();
#endif

    set_sighandlers ();
    notify_init ("kalu");
    gtk_main ();
eop:
    free_fifo (&fifo);
    if (!is_cli)
    {
        notify_uninit ();
    }
#ifdef ENABLE_STATUS_NOTIFIER
    guint i;
    for (i = 0; i < NB_SN_ICONS; ++i)
        if (sn_icon[i])
            g_object_unref (sn_icon[i]);
    if (sn)
        g_object_unref (sn);
#endif
#endif /* DISABLE_GUI */
    kalu_alpm_rmdb (keep_tmp_dbpath);
    if (config->is_curl_init)
    {
        curl_global_cleanup ();
    }
    free_config ();
#ifndef DISABLE_GUI
    if (kalpm_state.synced_dbs)
    {
        g_string_free (kalpm_state.synced_dbs, TRUE);
    }
    if (open_windows)
    {
        g_ptr_array_free (open_windows, TRUE);
    }
#endif
    return 0;
}
