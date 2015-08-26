/**
 * kalu - Copyright (C) 2012-2014 Olivier Brunel
 *
 * conf.c
 * Copyright (C) 2012-2014 Olivier Brunel <jjk@jjacky.com>
 * Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <glob.h>
#include <sys/utsname.h> /* uname */
#include <errno.h>

#ifndef DISABLE_UPDATER
#include <gdk/gdk.h>        /* for parsing RGBA colors */
#endif

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* kalu */
#include "kalu.h"
#include "conf.h"
#include "util.h"

/* some default values */
#define PACMAN_ROOTDIR      "/"
#define PACMAN_DBPATH       "/var/lib/pacman/"
#define PACMAN_CACHEDIR     "/var/cache/pacman/pkg/"
#define PACMAN_LOGFILE      "/var/log/pacman.log"
#define PACMAN_GPGDIR       "/etc/pacman.d/gnupg/"

typedef struct _siglevel_def_t {
    char *file;
    int   linenum;
    char *def;
} siglevel_def_t;

/*******************************************************************************
 * The following functions come from pacman's source code. (They might have
 * been modified.)
 * 
 * Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 * Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 * http://projects.archlinux.org/pacman.git
 * 
 ******************************************************************************/

/** Add repeating options such as NoExtract, NoUpgrade, etc to libalpm
 * settings. Refactored out of the parseconfig code since all of them did
 * the exact same thing and duplicated code.
 * @param ptr a pointer to the start of the multiple options
 * @param option the string (friendly) name of the option, used for messages
 * @param list the list to add the option to
 */
static void
setrepeatingoption (char *ptr, const char *option, alpm_list_t **list)
{
    char *q;

    while ((q = strchr(ptr, ' ')))
    {
        *q = '\0';
        *list = alpm_list_add (*list, strdup (ptr));
        debug ("config: %s: %s", option, ptr);
        ptr = q;
        ptr++;
    }
    *list = alpm_list_add (*list, strdup (ptr));
    debug ("config: %s: %s", option, ptr);
}

/**
 * Parse a signature verification level line.
 * @param values the list of parsed option values
 * @param storage location to store the derived signature level; any existing
 * value here is used as a starting point
 * @param file path to the config file
 * @param linenum current line number in file
 * @return 0 on success, 1 on any parsing error
 */
static int
process_siglevel (alpm_list_t *values, alpm_siglevel_t *storage,
                  const char *file, int linenum, GError **error)
{
    alpm_siglevel_t level = *storage;
    alpm_list_t *i;
    int ret = 0;

    /* Collapse the option names into a single bitmasked value */
    FOR_LIST (i, values)
    {
        const char *original = i->data, *value;
        int package = 0, database = 0;

        /* 7 == strlen ("Package") */
        if (strncmp (original, "Package", 7) == 0)
        {
            /* only packages are affected, don't flip flags for databases */
            value = original + 7;
            package = 1;
        }
        /* 8 == strlen ("Database") */
        else if (strncmp (original, "Database", 8) == 0)
        {
            /* only databases are affected, don't flip flags for packages */
            value = original + 8;
            database = 1;
        }
        else
        {
            /* no prefix, so anything found will affect both packages and dbs */
            value = original;
            package = database = 1;
        }

        /* now parse out and store actual flag if it is valid */
        if (streq (value, "Never"))
        {
            if (package)
            {
                level &= ~ALPM_SIG_PACKAGE;
            }
            if (database)
            {
                level &= ~ALPM_SIG_DATABASE;
            }
        }
        else if (streq (value, "Optional"))
        {
            if (package)
            {
                level |= ALPM_SIG_PACKAGE;
                level |= ALPM_SIG_PACKAGE_OPTIONAL;
            }
            if (database)
            {
                level |= ALPM_SIG_DATABASE;
                level |= ALPM_SIG_DATABASE_OPTIONAL;
            }
        }
        else if (streq (value, "Required"))
        {
            if (package)
            {
                level |= ALPM_SIG_PACKAGE;
                level &= ~ALPM_SIG_PACKAGE_OPTIONAL;
            }
            if (database)
            {
                level |= ALPM_SIG_DATABASE;
                level &= ~ALPM_SIG_DATABASE_OPTIONAL;
            }
        }
        else if (streq (value, "TrustedOnly"))
        {
            if (package)
            {
                level &= ~ALPM_SIG_PACKAGE_MARGINAL_OK;
                level &= ~ALPM_SIG_PACKAGE_UNKNOWN_OK;
            }
            if (database)
            {
                level &= ~ALPM_SIG_DATABASE_MARGINAL_OK;
                level &= ~ALPM_SIG_DATABASE_UNKNOWN_OK;
            }
        }
        else if (streq (value, "TrustAll"))
        {
            if (package)
            {
                level |= ALPM_SIG_PACKAGE_MARGINAL_OK;
                level |= ALPM_SIG_PACKAGE_UNKNOWN_OK;
            }
            if (database)
            {
                level |= ALPM_SIG_DATABASE_MARGINAL_OK;
                level |= ALPM_SIG_DATABASE_UNKNOWN_OK;
            }
        }
        else
        {
            g_set_error (error, KALU_ERROR, 1,
                    _("Config file %s, line %d: invalid value for SigLevel: %s"),
                    file,
                    linenum,
                    original);
            ret = 1;
            break;
        }
        level &= ~ALPM_SIG_USE_DEFAULT;
    }

    if(!ret)
    {
        *storage = level;
    }
    return ret;
}

#define set_error(fmt, ...)  g_set_error (error, KALU_ERROR, 1, \
    _("Config file %s, line %d: " fmt), file, linenum, __VA_ARGS__);
/** inspired from pacman's function */
gboolean
parse_pacman_conf (const char       *file,
                   char            **name,
                   int               is_options,
                   int               depth,
                   pacman_config_t **pacconf,
                   GError          **error)
{
    FILE       *fp              = NULL;
    char        line[MAX_PATH];
    int         linenum         = 0;
    int         success         = TRUE;
    const int   max_depth       = 10;
    GError     *local_err       = NULL;
    alpm_list_t *i, *j;

    /* if struct is not init yet, we do it */
    if (*pacconf == NULL)
    {
        *pacconf = new0 (pacman_config_t, 1);
        (*pacconf)->siglevel = ALPM_SIG_USE_DEFAULT;
    }
    pacman_config_t *pac_conf = *pacconf;
    /* the db/repo we're currently parsing, if any */
    static database_t *cur_db = NULL;

    debug ("config: attempting to read file %s", file);
    fp = fopen (file, "r");
    if (fp == NULL)
    {
        g_set_error (error, KALU_ERROR, 1,
                _("Config file %s could not be read"),
                file);
        success = FALSE;
        goto cleanup;
    }

    while (fgets (line, MAX_PATH, fp))
    {
        char *key, *value, *ptr;
        size_t line_len;

        ++linenum;
        strtrim (line);
        line_len = strlen(line);

        /* ignore whole line and end of line comments */
        if (line_len == 0 || line[0] == '#')
        {
            continue;
        }
        if ((ptr = strchr(line, '#')))
        {
            *ptr = '\0';
        }

        if (line[0] == '[' && line[line_len - 1] == ']')
        {
            /* only possibility here is a line == '[]' */
            if (line_len <= 2)
            {
                set_error ("%s", _("bad section name"));
                success = FALSE;
                goto cleanup;
            }
            /* new config section, skip the '[' */
            if (*name != NULL)
            {
                free (*name);
            }
            *name = strdup (line + 1);
            (*name)[line_len - 2] = '\0';
            debug ("config: new section '%s'", *name);
            is_options = (strcmp(*name, "options") == 0);
            /* parsed a db/repo? if so we add it */
            if (cur_db != NULL)
            {
                pac_conf->databases = alpm_list_add (pac_conf->databases, cur_db);
                cur_db = NULL;
            }
            continue;
        }

        /* directive */
        /* strsep modifies the 'line' string: 'key \0 value' */
        key = line;
        value = line;
        strsep (&value, "=");
        strtrim (key);
        strtrim (value);

        if (key == NULL)
        {
            set_error ("%s", _("syntax error: missing key."));
            success = FALSE;
            goto cleanup;
        }
        /* For each directive, compare to the camelcase string. */
        if (*name == NULL)
        {
            set_error ("%s", _("All directives must belong to a section."));
            success = FALSE;
            goto cleanup;
        }
        /* Include is allowed in both options and repo sections */
        if (strcmp(key, "Include") == 0)
        {
            glob_t globbuf;
            int globret;
            size_t gindex;

            if (depth >= max_depth - 1)
            {
                set_error ("parsing exceeded max recursion depth of %d",
                        max_depth);
                success = FALSE;
                goto cleanup;
            }

            if (value == NULL)
            {
                set_error ("directive %s needs a value.", key);
                success = FALSE;
                goto cleanup;
            }

            /* Ignore include failures... assume non-critical */
            globret = glob (value, GLOB_NOCHECK, NULL, &globbuf);
            switch (globret)
            {
                case GLOB_NOSPACE:
                    debug ("config file %s, line %d: include globbing out of space",
                            file, linenum);
                    break;
                case GLOB_ABORTED:
                    debug ("config file %s, line %d: include globbing read error for %s",
                            file, linenum, value);
                    break;
                case GLOB_NOMATCH:
                    debug ("config file %s, line %d: no include found for %s",
                            file, linenum, value);
                    break;
                default:
                    for (gindex = 0; gindex < globbuf.gl_pathc; gindex++)
                    {
                        debug ("config file %s, line %d: including %s",
                                file, linenum, globbuf.gl_pathv[gindex]);
                        parse_pacman_conf (globbuf.gl_pathv[gindex], name,
                                is_options, depth + 1, &pac_conf, error);
                    }
                    break;
            }
            globfree (&globbuf);
            continue;
        }
        /* we are either in options ... */
        if (is_options)
        {
            if (value == NULL)
            {
                /* options without settings */
                if (streq (key, "UseSyslog"))
                {
                    pac_conf->usesyslog = 1;
                    debug ("config: usesyslog");
                }
                else if (streq (key, "VerbosePkgLists"))
                {
                    pac_conf->verbosepkglists = 1;
                    debug ("config: verbosepkglists");
                }
                else if (streq (key, "UseDelta"))
                {
                    pac_conf->usedelta = 0.7;
                    debug ("config: usedelta (default: 0.7)");
                }
                else if (streq (key, "CheckSpace"))
                {
                    pac_conf->checkspace = 1;
                    debug ("config: checkspace");
                }
                /* we silently ignore "unrecognized" options, since we don't
                 * parse all of pacman's options anyways... */
            }
            else
            {
                /* options with settings */

                if (streq (key, "NoUpgrade"))
                {
                    setrepeatingoption (value, "NoUpgrade",
                            &(pac_conf->noupgrades));
                }
                else if (streq (key, "NoExtract"))
                {
                    setrepeatingoption (value, "NoExtract",
                            &(pac_conf->noextracts));
                }
                else if (streq (key, "IgnorePkg"))
                {
                    setrepeatingoption (value, "IgnorePkg",
                            &(pac_conf->ignorepkgs));
                }
                else if (streq (key, "IgnoreGroup"))
                {
                    setrepeatingoption (value, "IgnoreGroup",
                            &(pac_conf->ignoregroups));
                }
                else if (streq (key, "SyncFirst"))
                {
                    setrepeatingoption (value, "SyncFirst",
                            &(pac_conf->syncfirst));
                }
                else if (streq (key, "CacheDir"))
                {
                    setrepeatingoption (value, "CacheDir",
                            &(pac_conf->cachedirs));
                }
                else if (streq (key, "Architecture"))
                {
                    if (streq (value, "auto"))
                    {
                        struct utsname un;
                        uname (&un);
                        pac_conf->arch = strdup (un.machine);
                    }
                    else
                    {
                        pac_conf->arch = strdup (value);
                    }
                    debug ("config: arch: %s", pac_conf->arch);
                }
                else if (streq (key, "DBPath"))
                {
                    pac_conf->dbpath = strdup (value);
                    debug ("config: dbpath: %s", value);
                }
                else if (streq (key, "RootDir"))
                {
                    pac_conf->rootdir = strdup (value);
                    debug ("config: rootdir: %s", value);
                }
                else if (streq (key, "GPGDir"))
                {
                    pac_conf->gpgdir = strdup (value);
                    debug ("config: gpgdir: %s", value);
                }
                else if (streq (key, "LogFile"))
                {
                    pac_conf->logfile = strdup (value);
                    debug ("config: logfile: %s", value);
                }
                else if (streq (key, "SigLevel"))
                {
                    alpm_list_t *values = NULL;
                    setrepeatingoption (value, "SigLevel", &values);
                    local_err = NULL;
                    if (process_siglevel (values, &pac_conf->siglevel, file,
                                linenum, &local_err))
                    {
                        g_propagate_error (error, local_err);
                        FREELIST (values);
                        success = FALSE;
                        goto cleanup;
                    }
                    FREELIST (values);
                }
                else if (strcmp (key, "UseDelta") == 0)
                {
                    double ratio;
                    char *end;
                    ratio = g_ascii_strtod (value, &end);
                    if (*end != '\0' || ratio < 0.0 || ratio > 2.0)
                    {
                        set_error ("config file %s, line %d: invalid delta ratio: %s",
                                file, linenum, value);
                        success = FALSE;
                        goto cleanup;
                    }
                    pac_conf->usedelta = ratio;
                    debug ("config: usedelta=%f", ratio);
                }
                /* we silently ignore "unrecognized" options, since we don't
                 * parse all of pacman's options anyways... */
            }
        }
        /* ... or in a repo section */
        else
        {
            if (cur_db == NULL)
            {
                cur_db = new0 (database_t, 1);
                cur_db->name = strdup (*name);
            }

            if (strcmp (key, "Server") == 0)
            {
                if (value == NULL)
                {
                    set_error ("directive %s needs a value.", key);
                    success = FALSE;
                    goto cleanup;
                }
                cur_db->servers = alpm_list_add (cur_db->servers,
                        strdup (value));
            }
            else if (strcmp (key, "SigLevel") == 0)
            {
                siglevel_def_t *siglevel_def;
                siglevel_def = new0 (siglevel_def_t, 1);
                siglevel_def->file = strdup (file);
                siglevel_def->linenum = linenum;
                siglevel_def->def = strdup (value);
                cur_db->siglevel_def = alpm_list_add (cur_db->siglevel_def,
                        siglevel_def);
            }
            else
            {
                set_error ("directive %s in section %s not recognized.",
                        key, *name);
                success = FALSE;
                goto cleanup;
            }
        }
    }

    if (depth == 0)
    {
        /* parsed a db/repo? if so we add it */
        if (cur_db != NULL)
        {
            pac_conf->databases = alpm_list_add (pac_conf->databases, cur_db);
            cur_db = NULL;
        }
        /* processing databases siglevel */
        FOR_LIST (i, pac_conf->databases)
        {
            database_t *db = i->data;
            db->siglevel = pac_conf->siglevel;
            FOR_LIST (j, db->siglevel_def)
            {
                siglevel_def_t *siglevel_def = j->data;
                alpm_list_t *values = NULL;
                setrepeatingoption (siglevel_def->def, "SigLevel", &values);
                if (values)
                {
                    local_err = NULL;
                    if (process_siglevel (values,
                                &db->siglevel,
                                siglevel_def->file,
                                siglevel_def->linenum,
                                &local_err))
                    {
                        g_propagate_error (error, local_err);
                        FREELIST (values);
                        success = FALSE;
                        goto cleanup;
                    }
                    FREELIST (values);
                }
            }
        }
        /* set some default values for undefined options */
        if (NULL == pac_conf->rootdir)
        {
            pac_conf->rootdir = strdup (PACMAN_ROOTDIR);
        }
        if (NULL == pac_conf->dbpath)
        {
            pac_conf->dbpath = strdup (PACMAN_DBPATH);
        }
        if (NULL == pac_conf->cachedirs)
        {
            pac_conf->cachedirs = alpm_list_add (pac_conf->cachedirs,
                    strdup (PACMAN_CACHEDIR));
        }
        if (NULL == pac_conf->logfile)
        {
            pac_conf->logfile = strdup (PACMAN_LOGFILE);
        }
        if (NULL == pac_conf->gpgdir)
        {
            pac_conf->gpgdir = strdup (PACMAN_GPGDIR);
        }
    }

cleanup:
    if (fp)
    {
        fclose(fp);
    }
    if (depth == 0)
    {
        /* section name is for internal processing only */
        if (*name != NULL)
        {
            free (*name);
            *name = NULL;
        }
        /* so are the siglevel_def of each & all databases */
        FOR_LIST (i, pac_conf->databases)
        {
            database_t *db = i->data;
            FOR_LIST (j, db->siglevel_def)
            {
                siglevel_def_t *siglevel_def = j->data;
                free (siglevel_def->file);
                free (siglevel_def->def);
                free (siglevel_def);
            }
            alpm_list_free (db->siglevel_def);
            db->siglevel_def = NULL;
        }
    }
    debug ("config: finished parsing %s", file);
    return success;
}
#undef set_error

/******************************************************************************/

void
free_pacman_config (pacman_config_t *pac_conf)
{
    if (pac_conf == NULL)
    {
        return;
    }

    /* alpm */
    free (pac_conf->rootdir);
    free (pac_conf->dbpath);
    free (pac_conf->logfile);
    free (pac_conf->gpgdir);
    FREELIST (pac_conf->cachedirs);
    free (pac_conf->arch);
    FREELIST (pac_conf->ignorepkgs);
    FREELIST (pac_conf->ignoregroups);
    FREELIST (pac_conf->noupgrades);
    FREELIST (pac_conf->noextracts);

    /* non-alpm */
    FREELIST (pac_conf->syncfirst);

    /* dbs/repos */
    alpm_list_t *i;
    FOR_LIST (i, pac_conf->databases)
    {
        database_t *db = i->data;
        free (db->name);
        FREELIST (db->servers);
        free (db);
    }
    alpm_list_free (pac_conf->databases);

    /* done */
    free (pac_conf);
}

static void
setstringoption (char *value, const char *option, char **cfg, gboolean esc)
{
    size_t len;

    if (NULL != *cfg)
        free (*cfg);

    if (value[0] == '"')
    {
        len = strlen (value) - 1;
        if (value[len] == '"')
        {
            value[len] = '\0';
            ++value;
        }
    }
    else if (esc && value[0] == '\0')
    {
        *cfg = NULL;
        debug ("config: %s has no value", option);
        return;
    }

    *cfg = strreplace (value, "\\n", "\n");
    if (esc && strstr (*cfg, "\\e"))
    {
        char *s = *cfg;
        *cfg = strreplace (s, "\\e", "\e");
        free (s);
    }
    debug ("config: %s: %s", option, value);
}

#define add_error(fmt, ...) do {                    \
    if (!err_msg)                                   \
    {                                               \
        err_msg = g_string_sized_new (1024);        \
    }                                               \
    g_string_append_printf (err_msg,                \
            _("Config file %s, line %d: " fmt "\n"),\
            file, linenum, __VA_ARGS__);            \
} while (0)
/** inspired from pacman's function */
gboolean
parse_config_file (const char       *file,
                   conf_file_t       conf_file,
                   GError          **error)
{
    char       *data            = NULL;
    char       *line;
    gchar     **lines           = NULL;
    gchar     **l;
    int         linenum         = 0;
    char       *section         = NULL;
    int         success         = TRUE;
    GString    *err_msg         = NULL;
    GError     *local_err       = NULL;
    int         tpl;
    int         fld;

    debug ("config: attempting to read file %s", file);
    if (!g_file_get_contents (file, &data, NULL, &local_err))
    {
        /* not an error if file does not exists */
        if (local_err->domain != G_FILE_ERROR
                || local_err->code != G_FILE_ERROR_NOENT)
        {
            success = FALSE;
            g_set_error (error, KALU_ERROR, 1,
                    _("Config file %s could not be read: %s"),
                    file,
                    local_err->message);
        }
        g_clear_error (&local_err);
        goto cleanup;
    }

    lines = g_strsplit (data, "\n", 0);
    g_free (data);
    for (l = lines; *l; ++l)
    {
        char *key, *value, *ptr;
        size_t line_len;

        line = *l;
        ++linenum;
        strtrim (line);
        line_len = strlen (line);

        /* ignore whole line and end of line comments */
        if (line_len == 0 || line[0] == '#')
        {
            continue;
        }
        if ((ptr = strchr (line, '#')))
        {
            *ptr = '\0';
        }

        if (line[0] == '[' && line[line_len - 1] == ']')
        {
            /* only possibility here is a line == '[]' */
            if (line_len <= 2)
            {
                add_error ("%s", _("bad section name"));
                free (section);
                section = NULL;
                continue;
            }
            /* new config section, skip the '[' */
            free (section);
            section = strdup (line + 1);
            section[line_len - 2] = '\0';

            debug ("config: new section '%s'", section);
            continue;
        }

        /* directive */
        /* strsep modifies the 'line' string: 'key \0 value' */
        key = line;
        value = line;
        strsep (&value, "=");
        strtrim (key);
        strtrim (value);

        if (key == NULL)
        {
            add_error ("%s", _("syntax error: missing key"));
            continue;
        }

        /* kalu.conf*/
        if (conf_file == CONF_FILE_KALU)
        {
            if (value == NULL)
            {
                add_error ("value missing for %s", key);
                continue;
            }
            else if (streq ("options", section))
            {
                if (streq (key, "PacmanConf"))
                {
                    setstringoption (value, "pacmanconf", &(config->pacmanconf), FALSE);
                }
                else if (streq (key, "Interval"))
                {
                    config->interval = atoi (value);
                    if (config->interval > 0)
                    {
                        config->interval *= 60; /* minutes into seconds */
                    }
                    else
                    {
                        config->interval = 3600; /* 1 hour */
                    }

                    debug ("config: interval: %d", config->interval);
                }
                else if (streq (key, "Timeout"))
                {
                    if (streq (value, "DEFAULT"))
                    {
                        config->timeout = NOTIFY_EXPIRES_DEFAULT;
                    }
                    else if (streq (value, "NEVER"))
                    {
                        config->timeout = NOTIFY_EXPIRES_NEVER;
                    }
                    else
                    {
                        int timeout = atoi (value);
                        if (timeout < 4 || timeout > 42)
                        {
                            add_error ("Invalid timeout delay: %s", value);
                            continue;
                        }
                        config->timeout = timeout * 1000; /* from seconds to ms */
                    }
                    debug ("config: timeout: %d", config->timeout);
                }
                else if (streq (key, "SkipPeriod"))
                {
                    int begin_hour, begin_minute, end_hour, end_minute;

                    if (sscanf (value, "%d:%d-%d:%d",
                                &begin_hour,
                                &begin_minute,
                                &end_hour,
                                &end_minute) == 4)
                    {
                        if (begin_hour < 0 || begin_hour > 23
                                || begin_minute < 0 || begin_minute > 59
                                || end_hour < 0 || end_hour > 23
                                || end_minute < 0 || end_minute > 59)
                        {
                            add_error ("invalid value for SkipPeriod: %s",
                                    value);
                            continue;
                        }
                        config->has_skip = TRUE;
                        config->skip_begin_hour   = begin_hour;
                        config->skip_begin_minute = begin_minute;
                        config->skip_end_hour     = end_hour;
                        config->skip_end_minute   = end_minute;
                        debug ("config: SkipPeriod: from %d:%d to %d:%d",
                                config->skip_begin_hour, config->skip_begin_minute,
                                config->skip_end_hour, config->skip_end_minute);
                    }
                    else
                    {
                        add_error ("unable to parse SkipPeriod (must be HH:MM-HH:MM) : %s",
                                value);
                        continue;
                    }
                }
                else if (streq (key, "NotificationIcon"))
                {
                    if (streq (value, "KALU"))
                    {
                        config->notif_icon = ICON_KALU;
                    }
                    else if (streq (value, "NONE"))
                    {
                        config->notif_icon = ICON_NONE;
                    }
                    else if (value[0] == '/')
                    {
                        config->notif_icon = ICON_USER;
                        config->notif_icon_user = strdup (value);
                    }
                    else
                    {
                        add_error ("invalid value for %s: %s", key, value);
                        continue;
                    }
                    debug ("config: NotifIcon: %d", config->notif_icon);
                    debug ("config: NotifIconUser: %s", config->notif_icon_user);
                }
                else if (streq (key, "NotificationIconSize"))
                {
                    config->notif_icon_size = atoi (value);
                    config->notif_icon_size = CLAMP (config->notif_icon_size, 8, 48);

                    debug ("config: notif icon size: %d", config->notif_icon_size);
                }
                else if (streq (key, "UpgradeAction"))
                {
                    if (streq (value, "NONE"))
                    {
                        config->action = UPGRADE_NO_ACTION;
                    }
#ifndef DISABLE_UPDATER
                    else if (streq (value, "KALU"))
                    {
                        config->action = UPGRADE_ACTION_KALU;
                    }
#endif
                    else if (streq (value, "CMDLINE"))
                    {
                        config->action = UPGRADE_ACTION_CMDLINE;
                    }
                    else
                    {
                        add_error ("Invalid value for UpgradeAction: %s",
                                value);
                        continue;
                    }
                    debug ("config: action: %d", config->action);
                }
                else if (streq (key, "CmdLine"))
                {
                    setstringoption (value, "cmdline", &(config->cmdline), FALSE);
                }
                else if (streq (key, "CmdLineAur"))
                {
                    setstringoption (value, "cmdline_aur", &(config->cmdline_aur), FALSE);
                }
#ifndef DISABLE_UPDATER
                else if (streq (key, "PostSysUpgrade"))
                {
                    config->cmdline_post = alpm_list_add (config->cmdline_post,
                            strdup (value));
                    debug ("config: postsysupgrade: %s", value);
                }
                else if (streq (key, "ConfirmPostSysUpgrade"))
                {
                    config->confirm_post = (*value == '1');
                    debug ("config: confirm postsysupgrade: %d", config->confirm_post);
                }
                else if (streq (key, "CmdLineLink"))
                {
                    setstringoption (value, "cmdline_link", &(config->cmdline_link), FALSE);
                }
#endif
                else if (streq (key, "AurIgnore"))
                {
                    setrepeatingoption (value, "aur_ignore", &(config->aur_ignore));
                }
                else if (streq (key, "ManualChecks")
                        || streq (key, "AutoChecks"))
                {
                    char *v, *s;
                    check_t checks = 0;

                    for (v = value, s = (char *) 1; s; )
                    {
                        s = strchr(v, ' ');
                        if (s)
                        {
                            *s = '\0';
                        }

                        if (streq ("UPGRADES", v))
                        {
                            checks |= CHECK_UPGRADES;
                        }
                        else if (streq ("WATCHED", v))
                        {
                            checks |= CHECK_WATCHED;
                        }
                        else if (streq ("AUR", v))
                        {
                            checks |= CHECK_AUR;
                        }
                        else if (streq ("WATCHED_AUR", v))
                        {
                            checks |= CHECK_WATCHED_AUR;
                        }
                        else if (streq ("NEWS", v))
                        {
                            checks |= CHECK_NEWS;
                        }
                        else
                        {
                            add_error ("unknown value for %s: %s", key, v);
                            continue;
                        }

                        if (s)
                        {
                            v = s + 1;
                        }
                    }

                    if (streq (key, "AutoChecks"))
                    {
                        config->checks_auto = checks;
                        debug ("config: checks_auto: %d", checks);
                    }
                    else /* if (strcmp (key, "ManualChecks") == 0) */
                    {
                        config->checks_manual = checks;
                        debug ("config: checks_manual: %d", checks);
                    }
                }
                else if (  streq (key, "OnSglClick")
                        || streq (key, "OnDblClick")
                        || streq (key, "OnMdlClick")
                        || streq (key, "OnSglClickPaused")
                        || streq (key, "OnDblClickPaused")
                        || streq (key, "OnMdlClickPaused"))
                {
                    on_click_t *on_click;
                    gboolean is_paused = FALSE;

                    if (streq (key, "OnSglClick"))
                    {
                        on_click = &(config->on_sgl_click);
                    }
                    else if (streq (key, "OnDblClick"))
                    {
                        on_click = &(config->on_dbl_click);
                    }
                    else if (streq (key, "OnMdlClick"))
                    {
                        on_click = &(config->on_mdl_click);
                    }
                    else if (streq (key, "OnSglClickPaused"))
                    {
                        is_paused = TRUE;
                        on_click = &(config->on_sgl_click_paused);
                    }
                    else if (streq (key, "OnDblClickPaused"))
                    {
                        is_paused = TRUE;
                        on_click = &(config->on_dbl_click_paused);
                    }
                    else /* if (streq (key, "OnMdlClickPaused")) */
                    {
                        is_paused = TRUE;
                        on_click = &(config->on_mdl_click_paused);
                    }

                    if (streq (value, "CHECK"))
                    {
                        *on_click = DO_CHECK;
                    }
                    else if (streq (value, "SYSUPGRADE"))
                    {
                        *on_click = DO_SYSUPGRADE;
                    }
#ifndef DISABLE_UPDATER
                    else if (streq (value, "SIMULATION"))
                    {
                        *on_click = DO_SIMULATION;
                    }
#endif
                    else if (streq (value, "NOTHING"))
                    {
                        *on_click = DO_NOTHING;
                    }
                    else if (streq (value, "TOGGLE_WINDOWS"))
                    {
                        *on_click = DO_TOGGLE_WINDOWS;
                    }
                    else if (streq (value, "LAST_NOTIFS"))
                    {
                        *on_click = DO_LAST_NOTIFS;
                    }
                    else if (streq (value, "TOGGLE_PAUSE"))
                    {
                        *on_click = DO_TOGGLE_PAUSE;
                    }
                    else if (streq (value, "EXIT"))
                    {
                        *on_click = DO_EXIT;
                    }
                    else if (is_paused && streq (value, "SAME_AS_ACTIVE"))
                    {
                        *on_click = DO_SAME_AS_ACTIVE;
                    }
                    else
                    {
                        add_error ("unknown value for %s: %s", key, value);
                        continue;
                    }
                    debug ("config: %s: %d", key, *on_click);
                }
                else if (streq (key, "SaneSortOrder"))
                {
                    add_error ("%s", _("Option SaneSortOrder doesn't exist anymore; "
                            "Please use GTK3 option gtk-alternative-sort-arrows instead."));
                }
                else if (streq (key, "SyncDbsInTooltip"))
                {
                    config->syncdbs_in_tooltip = (*value == '1');
                    debug ("config: syncdbs in tooltip: %d", config->syncdbs_in_tooltip);
                }
                else if (streq (key, "CheckPacmanConflict"))
                {
                    config->check_pacman_conflict = (*value == '1');
                    debug ("config: check for pacman/kalu conflict: %d",
                            config->check_pacman_conflict);
                }
                else if (streq (key, "UseIP"))
                {
                    if (value[0] == '4' && value[1] == '\0')
                    {
                        config->use_ip = IPv4;
                        debug ("config: use IPv4");
                    }
                    else if (value[0] == '6' && value[1] == '\0')
                    {
                        config->use_ip = IPv6;
                        debug ("config: use IPv6");
                    }
                    else
                    {
                        add_error ("unknown value for %s: %s", key, value);
                        continue;
                    }
                }
                else if (streq (key, "AutoNotifs"))
                {
                    if (value[0] == '0' && value[1] == '\0')
                    {
                        config->auto_notifs = FALSE;
                        debug ("config: disable showing notifs for auto-checks");
                    }
                    else if (value[0] == '1' && value[1] == '\0')
                    {
                        config->auto_notifs = TRUE;
                        debug ("config: enable showing notifs for auto-checks");
                    }
                    else
                    {
                        add_error ("unknown value for %s: %s", key, value);
                        continue;
                    }
                }
                else if (streq (key, "NotifButtons"))
                {
                    if (value[0] == '0' && value[1] == '\0')
                    {
                        config->notif_buttons = FALSE;
                        debug ("config: disable action-buttons on notifications");
                    }
                    else if (value[0] == '1' && value[1] == '\0')
                    {
                        config->notif_buttons = TRUE;
                        debug ("config: enable action-buttons on notifications");
                    }
                    else
                    {
                        add_error ("unknown value for %s: %s", key, value);
                        continue;
                    }
                }
#ifndef DISABLE_UPDATER
                else if (streq (key, "ColorUnimportant")
                        || streq (key, "ColorInfo")
                        || streq (key, "ColorWarning")
                        || streq (key, "ColorError"))
                {
                    GdkRGBA rgba;
                    gchar **cfg;

                    if (*value == '.')
                        *value = '#';
                    if (!gdk_rgba_parse (&rgba, value))
                    {
                        add_error ("invalid value for %s: %s", key, value);
                        continue;
                    }

                    if (streq (key, "ColorInfo"))
                        cfg = &config->color_info;
                    else if (streq (key, "ColorWarning"))
                        cfg = &config->color_warning;
                    else if (streq (key, "ColorError"))
                        cfg = &config->color_error;
                    else
                        cfg = &config->color_unimportant;

                    free (*cfg);
                    *cfg = strdup (value);

                    debug ("config: set %s to %s", key, value);
                }
                else if (streq (key, "AutoShowLog"))
                {
                    if (value[0] == '1' && value[1] == '\0')
                    {
                        config->auto_show_log = TRUE;
                        debug ("config: enable auto-show-log");
                    }
                    else if (value[0] != '0' || value[1] != '\0')
                    {
                        add_error ("unknown value for %s: %s", key, value);
                        continue;
                    }
                }
#endif
                else
                {
                    add_error ("unknown option: %s", key);
                    continue;
                }
            }
            else
            {
                int is_sce = 0;

                if (!streqn ("template-", section, 9))
                {
                    add_error ("unknown section: %s", section);
                    continue;
                }
                for (tpl = 0; tpl < _NB_TPL; ++tpl)
                    if (streq (tpl_names[tpl], section + 9))
                        break;
                if (tpl >= _NB_TPL)
                {
                    add_error ("unknown section: %s", section);
                    continue;
                }

                /* we're in a valid template */

                for (fld = 0; fld < _NB_FLD; ++fld)
                {
                    size_t e = strlen (fld_names[fld]);
                    if (streqn (key, fld_names[fld], e)
                            && (key[e] == '\0' || streq (key + e, "Sce")))
                    {
                        is_sce = key[e] == 'S';
                        break;
                    }
                }
                if (fld >= _NB_FLD)
                {
                    add_error ("unknown template option: %s", key);
                    continue;
                }

                if (is_sce)
                {
                    int i;

                    /* skip 0 == TPL_SCE_UNDEFINED */
                    for (i = 1; i < _NB_TPL_SCE; ++i)
                    {
                        /* FLD_TITLE has no FALLBACK nor NONE */
                        if (fld == FLD_TITLE
                                && (i == TPL_SCE_FALLBACK || i == TPL_SCE_NONE))
                            continue;

                        if (streq (value, tpl_sce_names[i]))
                        {
                            config->templates[tpl].fields[fld].source = i;
                            break;
                        }
                    }
                    if (i >= _NB_TPL_SCE)
                    {
                        add_error ("unknown value for %s: %s", key, value);
                        continue;
                    }
                    debug ("config: %s: %d",
                            key, config->templates[tpl].fields[fld].source);
                }
                else
                {
                    setstringoption (value, key,
                            &(config->templates[tpl].fields[fld].custom), TRUE);
                    if (config->templates[tpl].fields[fld].source == TPL_SCE_UNDEFINED)
                    {
                        if (config->templates[tpl].fields[fld].custom)
                            config->templates[tpl].fields[fld].source = TPL_SCE_CUSTOM;
                        else if (fld != FLD_TITLE)
                            config->templates[tpl].fields[fld].source = TPL_SCE_FALLBACK;
                        debug ("config: auto-set %sSce: %d",
                                key, config->templates[tpl].fields[fld].source);
                    }
                }
            }
        }
        /* watched{,-aur}.conf */
        else if (conf_file == CONF_FILE_WATCHED
                || conf_file == CONF_FILE_WATCHED_AUR)
        {
            if (value == NULL)
            {
                add_error ("watched package %s: version number missing", key);
                continue;
            }
            else
            {
                alpm_list_t **list;
                if (conf_file == CONF_FILE_WATCHED)
                {
                    list = &(config->watched);
                }
                else /* if (conf_file == CONF_FILE_WATCHED_AUR) */
                {
                    list = &(config->watched_aur);
                }

                watched_package_t *w_pkg;
                w_pkg = new0 (watched_package_t, 1);
                w_pkg->name = strdup (key);
                w_pkg->version = strdup (value);
                *list = alpm_list_add (*list, w_pkg);
                debug ("config: watched%s packages: added %s %s",
                        (conf_file == CONF_FILE_WATCHED) ? "" : " AUR",
                        w_pkg->name, w_pkg->version);
            }
        }
        /* news.conf */
        else if (conf_file == CONF_FILE_NEWS)
        {
            if (value == NULL)
            {
                add_error ("news data: value missing for %s", key);
                continue;
            }
            else if (streq ("Last", key))
            {
                config->news_last = strdup (value);
                debug ("config: news_last: %s", value);
            }
            else if (streq ("Read", key))
            {
                config->news_read = alpm_list_add (config->news_read,
                        strdup (value));
                debug ("config: news_read: added %s", value);
            }
        }
    }

    /* ensure templates sources are valid */
    for (tpl = 0; tpl < _NB_TPL; ++tpl)
        for (fld = 0; fld < _NB_FLD; ++fld)
        {
            struct field *f = &config->templates[tpl].fields[fld];
            tpl_sce_t sce = f->source;

            if (f->source == TPL_SCE_UNDEFINED)
                f->source = TPL_SCE_DEFAULT;
            if (f->source == TPL_SCE_CUSTOM && !f->custom)
                f->source = TPL_SCE_DEFAULT;
            if (f->source == TPL_SCE_DEFAULT && !f->def)
                f->source = TPL_SCE_FALLBACK;
            if (f->source == TPL_SCE_FALLBACK && config->templates[tpl].fallback == NO_TPL)
                f->source = TPL_SCE_DEFAULT;

            if (f->source != sce)
                debug ("config: auto-correct source for field %d in template %d: %d -> %d",
                        fld, tpl, sce, f->source);
        }

cleanup:
    g_strfreev (lines);
    free (section);
    if (config->action == UPGRADE_ACTION_CMDLINE && config->cmdline == NULL)
    {
#ifndef DISABLE_UPDATER
        config->action = UPGRADE_ACTION_KALU;
#else
        config->action = UPGRADE_NO_ACTION;
#endif
        debug ("config: action: no cmdline set, reverting to %d",
                config->action);
    }
    if (err_msg)
    {
        g_set_error (error, KALU_ERROR, 1, "%s", err_msg->str);
        g_string_free (err_msg, TRUE);
        err_msg = NULL;
        success = FALSE;
    }
    debug ("config: finished parsing %s", file);
    return success;
}
#undef add_error
