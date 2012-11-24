/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * kalu.h
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

#ifndef _KALU_H
#define _KALU_H

/* C */
#include <stdio.h>

/* glib */
#include <glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

#define _UNUSED_                __attribute__ ((unused)) 

#if defined(GIT_VERSION)
#undef PACKAGE_VERSION
#define PACKAGE_VERSION GIT_VERSION
#endif
#define PACKAGE_TAG             "Keeping Arch Linux Up-to-date"

#define MAX_PATH                255
#ifndef DISABLE_GUI
#define UPGRADES_NB_CONFLICT    -2  /* set to nb_upgrades when conflict makes it
                                       impossible to get packages number */
#endif

#ifndef NOTIFY_EXPIRES_DEFAULT
#define NOTIFY_EXPIRES_DEFAULT  -1
#endif
#ifndef NOTIFY_EXPIRES_NEVER
#define NOTIFY_EXPIRES_NEVER     0
#endif

#define KALU_ERROR              g_quark_from_static_string ("kalu error")

#define FREE_PACKAGE_LIST(p)                                                \
            do                                                              \
            {                                                               \
                alpm_list_free_inner (p, (alpm_list_fn_free) free_package); \
                alpm_list_free (p);                                         \
                p = NULL;                                                   \
            } while(0)

#define FREE_WATCHED_PACKAGE_LIST(p)                                        \
            do                                                              \
            {                                                               \
                alpm_list_free_inner (p, (alpm_list_fn_free) free_watched_package); \
                alpm_list_free (p);                                         \
                p = NULL;                                                   \
            } while(0)

typedef enum {
    UPGRADE_NO_ACTION = 0,
    UPGRADE_ACTION_CMDLINE,
    #ifndef DISABLE_UPDATER
    UPGRADE_ACTION_KALU,
    #endif
} upgrade_action_t;

typedef enum {
    CHECK_UPGRADES      = (1 << 0),
    CHECK_WATCHED       = (1 << 1),
    CHECK_AUR           = (1 << 2),
    CHECK_WATCHED_AUR   = (1 << 3),
    CHECK_NEWS          = (1 << 4)
} check_t;

typedef enum {
    DO_NOTHING = 0,
    DO_CHECK,
    DO_SYSUPGRADE,
    DO_TOGGLE_WINDOWS,
    DO_LAST_NOTIFS,
    DO_TOGGLE_PAUSE,
    DO_SAME_AS_ACTIVE
} on_click_t;

typedef enum {
    ICON_NONE = 0,
    ICON_KALU,
    ICON_USER
} notif_icon_t;

enum {
    IP_WHATEVER = 0,
    IPv4,
    IPv6
};

typedef struct _templates_t {
    char *title;
    char *package;
    char *sep;
} templates_t;

typedef struct _config_t {
    gboolean         is_debug;
    char            *pacmanconf;
    check_t          checks_manual;
    check_t          checks_auto;
    int              syncdbs_in_tooltip;
    int              interval;
    int              timeout;
    int              has_skip;
    int              skip_begin_hour;
    int              skip_begin_minute;
    int              skip_end_hour;
    int              skip_end_minute;
    notif_icon_t     notif_icon;
    char            *notif_icon_user;
    upgrade_action_t action;
    char            *cmdline;
    char            *cmdline_aur;
    #ifndef DISBALE_UPDATER
    alpm_list_t     *cmdline_post;
    gboolean         confirm_post;
    #endif
    gboolean         sane_sort_order;
    gboolean         check_pacman_conflict;
    on_click_t       on_sgl_click;
    on_click_t       on_dbl_click;
    on_click_t       on_sgl_click_paused;
    on_click_t       on_dbl_click_paused;
    int              use_ip;
    
    templates_t     *tpl_upgrades;
    templates_t     *tpl_watched;
    templates_t     *tpl_aur;
    templates_t     *tpl_watched_aur;
    templates_t     *tpl_news;
    
    alpm_list_t     *aur_ignore;
    
    alpm_list_t     *watched;
    alpm_list_t     *watched_aur;
    
    char            *news_last;
    alpm_list_t     *news_read;
    #ifndef DISBALE_UPDATER
    char            *cmdline_link;
    #endif
    
    gboolean         is_curl_init;
    #ifndef DISBALE_UPDATER
    alpm_list_t     *last_notifs;
    #endif
} config_t;

typedef struct _watched_package_t {
    char    *name;
    char    *version;
} watched_package_t;

typedef struct _kalu_package_t {
    char    *name;
    char    *desc;
    char    *old_version;
    char    *new_version;
    guint    dl_size;
    guint    old_size; /* old installed size */
    guint    new_size; /* new installed size */
} kalu_package_t;

typedef enum {
    SKIP_UNKNOWN = 0,
    SKIP_BEGIN,
    SKIP_END
} skip_next_t;

typedef struct _kalpm_state_t {
    gboolean    is_paused;
    skip_next_t skip_next;
    guint       timeout_skip;
    gint        is_busy;
    guint       timeout;
    guint       timeout_icon;
    GDateTime  *last_check;
    gint        nb_syncdbs;
    gint        nb_upgrades;
    gint        nb_watched;
    gint        nb_aur;
    gint        nb_watched_aur;
    gint        nb_news;
} kalpm_state_t;

typedef struct _notif_t {
    check_t     type;
    gchar      *summary;
    gchar      *text;
    gpointer    data;
} notif_t;

/* global variable */
extern config_t *config;

void debug (const char *fmt, ...);

void free_package (kalu_package_t *package);
void free_watched_package (watched_package_t *w_pkg);

void kalu_check_work (gboolean is_auto);

#endif /* _KALU_H */
