/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * kalu.h
 * Copyright (C) 2012-2017 Olivier Brunel <jjk@jjacky.com>
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

/* kalu */
#include "shared.h"

#if defined(GIT_VERSION)
#undef PACKAGE_VERSION
#define PACKAGE_VERSION GIT_VERSION
#endif
#define PACKAGE_TAG             "Keeping Arch Linux Up-to-date"

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

#define FREE_PACKAGE_LIST(p)    do {                            \
    alpm_list_free_inner (p, (alpm_list_fn_free) free_package); \
    alpm_list_free (p);                                         \
    p = NULL;                                                   \
} while(0)

#define FREE_WATCHED_PACKAGE_LIST(p)    do {                            \
    alpm_list_free_inner (p, (alpm_list_fn_free) free_watched_package); \
    alpm_list_free (p);                                                 \
    p = NULL;                                                           \
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
    CHECK_NEWS          = (1 << 4),

    _CHECK_AUR_NOT_FOUND = (1 << 7)
} check_t;

typedef enum {
    DO_NOTHING = 0,
    DO_CHECK,
    DO_SYSUPGRADE,
    DO_SIMULATION,
    DO_TOGGLE_WINDOWS,
    DO_LAST_NOTIFS,
    DO_TOGGLE_PAUSE,
    DO_SAME_AS_ACTIVE,
    DO_EXIT
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

#ifdef ENABLE_STATUS_NOTIFIER
enum {
    SN_ICON_KALU = 0,
    SN_ICON_KALU_PAUSED,
    SN_ICON_KALU_GRAY,
    SN_ICON_KALU_GRAY_PAUSED,
    NB_SN_ICONS
};
#define SN_ACTIVATE             1
#define SN_SECONDARY_ACTIVATE   2
#endif

typedef enum {
    NO_TPL = -1, /* to indicate no fallback */
    TPL_UPGRADES,
    TPL_WATCHED,
    TPL_AUR,
    TPL_AUR_NOT_FOUND,
    TPL_WATCHED_AUR,
    TPL_NEWS,
    _NB_TPL
} tpl_t;

/* template names in config (prefixed w/ "template-") */
const gchar *tpl_names[_NB_TPL];

typedef enum {
    FLD_TITLE,
    FLD_PACKAGE,
    FLD_SEP,
    _NB_FLD
} fld_t;

/* field names in config */
const gchar *fld_names[_NB_FLD];

typedef enum {
    TPL_SCE_UNDEFINED = 0,
    TPL_SCE_DEFAULT,
    TPL_SCE_FALLBACK,
    TPL_SCE_CUSTOM,
    TPL_SCE_NONE,
    _NB_TPL_SCE
} tpl_sce_t;

/* source names in config */
const gchar *tpl_sce_names[_NB_TPL_SCE];

struct field {
    const char *def;
    char *custom;
    tpl_sce_t source;
};

typedef struct _templates_t {
    tpl_t fallback;
    struct field fields[_NB_TPL];
} templates_t;

typedef struct _config_t {
    int              is_debug;
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
    int              notif_icon_size;
    upgrade_action_t action;
    char            *cmdline;
    char            *cmdline_aur;
#ifndef DISABLE_UPDATER
    alpm_list_t     *cmdline_post;
    gboolean         confirm_post;
#endif
    gboolean         check_pacman_conflict;
    on_click_t       on_sgl_click;
    on_click_t       on_dbl_click;
    on_click_t       on_mdl_click;
    on_click_t       on_sgl_click_paused;
    on_click_t       on_dbl_click_paused;
    on_click_t       on_mdl_click_paused;
    int              use_ip;
    gboolean         auto_notifs;
    gboolean         notif_buttons;

    templates_t      templates[_NB_TPL];

    alpm_list_t     *aur_ignore;

    alpm_list_t     *watched;
    alpm_list_t     *watched_aur;

    char            *news_last;
    alpm_list_t     *news_read;
#ifndef DISABLE_GUI
    char            *cmdline_link;
#endif

    gboolean         is_curl_init;
#ifndef DISABLE_GUI
    alpm_list_t     *last_notifs;
#endif
#ifndef DISABLE_UPDATER
    char            *color_unimportant;
    char            *color_info;
    char            *color_warning;
    char            *color_error;
    gboolean         auto_show_log;
#endif
#ifdef ENABLE_STATUS_NOTIFIER
    gboolean         sn_force_icons;
#endif
} config_t;

typedef struct _watched_package_t {
    char    *name;
    char    *version;
} watched_package_t;

typedef struct _kalu_package_t {
    char    *repo;
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
    skip_next_t skip_next;
    guint       timeout_skip;
    gint8       is_paused   : 1;
    gint8       is_busy     : 2;
    gint8       is_updater  : 1;
    gint8       _unused     : 4;
    guint       timeout;
    guint       timeout_icon;
    GDateTime  *last_check;
    GString    *synced_dbs;
    gint        nb_upgrades;
    gint        nb_watched;
    gint        nb_aur;
    gint        nb_aur_not_found;
    gint        nb_watched_aur;
    gint        nb_news;
} kalpm_state_t;

typedef struct _notif_t {
    check_t     type;
    gchar      *summary;
    gchar      *text;
    gpointer    data;
} notif_t;

/* global variables */
extern config_t *config;
extern gint aborting;

void debug (const char *fmt, ...);

void free_package (kalu_package_t *package);
void free_watched_package (watched_package_t *w_pkg);

void kalu_check_work (gboolean is_auto);

#endif /* _KALU_H */
