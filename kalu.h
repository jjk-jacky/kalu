
#ifndef _KALU_H
#define _KALU_H

/* C */
#include <stdio.h>

/* glib */
#include <glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

#define _UNUSED_            __attribute__ ((unused)) 

#define KALU_VERSION       "0.0.0"

#define MAX_PATH            255

#define KALU_ERROR          g_quark_from_static_string ("kalu error")

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

#define debug(...)     
#define _debug(...)     do {                                         \
                                fprintf (stdout, "[DEBUG] ");       \
                                fprintf (stdout, __VA_ARGS__);      \
                                fprintf (stdout, "\n");             \
                          } while (0)

typedef enum {
    UPGRADE_NO_ACTION = 0,
    UPGRADE_ACTION_KALU,
    UPGRADE_ACTION_CMDLINE
} upgrade_action_t;

typedef enum {
    CHECK_UPGRADES      = (1 << 0),
    CHECK_WATCHED       = (1 << 1),
    CHECK_AUR           = (1 << 2),
    CHECK_WATCHED_AUR   = (1 << 3),
    CHECK_NEWS          = (1 << 4)
} check_t;

typedef struct _templates_t {
    char *title;
    char *package;
    char *sep;
} templates_t;

typedef struct _config_t {
    unsigned int     verbose;
    unsigned int     verbose_watched;
    unsigned int     verbose_aur;
    unsigned int     verbose_watched_aur;
    char            *pacmanconf;
    unsigned int     checks_manual;
    unsigned int     checks_auto;
    int              interval;
    int              has_skip;
    int              skip_begin_hour;
    int              skip_begin_minute;
    int              skip_end_hour;
    int              skip_end_minute;
    upgrade_action_t action;
    char            *cmdline;
    char            *cmdline_aur;
    alpm_list_t     *cmdline_post;
    
    templates_t     *tpl;
    templates_t     *tpl_verbose;
    templates_t     *tpl_watched;
    templates_t     *tpl_watched_verbose;
    templates_t     *tpl_aur;
    templates_t     *tpl_aur_verbose;
    templates_t     *tpl_watched_aur;
    templates_t     *tpl_watched_aur_verbose;
    templates_t     *tpl_news;
    
    alpm_list_t     *aur_ignore;
    
    alpm_list_t     *watched;
    alpm_list_t     *watched_aur;
    
    char            *news_last;
    alpm_list_t     *news_read;
    
    gboolean         is_curl_init;
} config_t;

typedef struct _watched_package_t {
    char    *name;
    char    *version;
} watched_package_t;

typedef struct _kalu_package_t {
    char    *name;
    char    *old_version;
    char    *new_version;
    guint    dl_size;
    guint    old_size; /* old installed size */
    guint    new_size; /* new installed size */
} kalu_package_t;

typedef struct _kalpm_state_t {
    gboolean    is_busy;
    guint       timeout;
    guint       timeout_icon;
    GDateTime  *last_check;
    gint        nb_upgrades;
    gint        nb_watched;
    gint        nb_aur;
    gint        nb_watched_aur;
    gint        nb_news;
} kalpm_state_t;

/* global variable */
extern config_t *config;


void free_package (kalu_package_t *package);
void free_watched_package (watched_package_t *w_pkg);

gboolean reload_watched (gboolean is_aur, GError **error);

void set_kalpm_busy (gboolean busy);
void set_kalpm_nb (check_t type, gint nb);

#endif /* _KALU_H */
