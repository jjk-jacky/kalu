/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * conf.h
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
    alpm_list_t     *hookdirs;
    alpm_list_t     *cachedirs;
    alpm_siglevel_t  siglevel;
    char            *arch;
    int              checkspace;
    int              usesyslog;
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
                   char            **name,
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
