
#ifndef _KALU_CONFIG_H
#define _KALU_CONFIG_H

/* glib */
#include <glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* type of conf file to parse */
typedef enum _conf_file_t {
    CONF_FILE_KALU,
    CONF_FILE_WATCHED,
    CONF_FILE_WATCHED_AUR,
    CONF_FILE_NEWS
} conf_file_t;

/* database definition from parsing conf */
typedef struct _database_t {
    char            *name;
    alpm_siglevel_t  siglevel;
    alpm_list_t     *siglevel_def; /* for internal processing/parsing */
    alpm_list_t     *servers;
} database_t;

/* config data loaded from parsing pacman.conf */
typedef struct _pacman_config_t {
    /* alpm */
    char            *rootdir;
    char            *dbpath;
    char            *logfile;
    char            *gpgdir;
    alpm_list_t     *cachedirs;
    alpm_siglevel_t  siglevel;
    char            *arch;
    int              checkspace;
    int              usesyslog;
    int              usedelta;
    alpm_list_t     *ignorepkgs;
    alpm_list_t     *ignoregroups;
    alpm_list_t     *noupgrades;
    alpm_list_t     *noextracts;
    
    /* non-alpm */
    alpm_list_t     *syncfirst;
    unsigned short   verbosepkglists;
    
    /* dbs/repos */
    alpm_list_t     *databases;
} pacman_config_t;

gboolean
parse_pacman_conf (const char       *file,
                   char             *name,
                   int               is_options,
                   int               depth,
                   pacman_config_t **pac_conf,
                   GError          **error);

void
free_pacman_config (pacman_config_t *pac_conf);

gboolean
parse_config_file (const char       *file,
                   conf_file_t       conf_file,
                   GError          **error);


#endif /* _KALU_CONFIG_H */
