/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * config.c
 * Copyright (C) 2012 Olivier Brunel <i.am.jack.mail@gmail.com>
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

#define _BSD_SOURCE /* for strdup w/ -std=c99 */

/* C */
#include <string.h>
#include <glob.h>
#include <sys/utsname.h> /* uname */
#include <errno.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* kalu */
#include "kalu.h"
#include "config.h"
#include "util.h"
#include "alpm.h"

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
	for (i = values; i; i = alpm_list_next (i))
    {
		const char *original = i->data, *value;
		int package = 0, database = 0;

		if (strncmp (original, "Package", strlen ("Package")) == 0)
        {
			/* only packages are affected, don't flip flags for databases */
			value = original + strlen("Package");
			package = 1;
		}
        else if (strncmp (original, "Database", strlen ("Database")) == 0)
        {
			/* only databases are affected, don't flip flags for packages */
			value = original + strlen("Database");
			database = 1;
		}
        else
        {
			/* no prefix, so anything found will affect both packages and dbs */
			value = original;
			package = database = 1;
		}

		/* now parse out and store actual flag if it is valid */
		if (strcmp (value, "Never") == 0)
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
        else if (strcmp (value, "Optional") == 0)
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
        else if (strcmp (value, "Required") == 0)
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
        else if (strcmp (value, "TrustedOnly") == 0)
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
        else if (strcmp (value, "TrustAll") == 0)
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
            g_set_error (error, KALU_ERROR, 1, "Config file %s, line %d: "
                "invalid value for SigLevel: %s", file, linenum, original);
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
    "Config file %s, line %d: " fmt, file, linenum, __VA_ARGS__);
/** inspired from pacman's function */
gboolean
parse_pacman_conf (const char       *file,
                   char             *name,
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
        *pacconf = calloc (1, sizeof (**pacconf));
        if (*pacconf == NULL)
        {
            g_set_error (error, KALU_ERROR, 1, "Unable to allocate memory");
            success = FALSE;
            goto cleanup;
        }
        (*pacconf)->siglevel = ALPM_SIG_USE_DEFAULT;
    }
    pacman_config_t *pac_conf = *pacconf;
    /* the db/repo we're currently parsing, if any */
    static database_t *cur_db = NULL;

	debug ("config: attempting to read file %s", file);
	fp = fopen (file, "r");
	if (fp == NULL)
    {
        g_set_error (error, KALU_ERROR, 1, "Config file %s could not be read",
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
                set_error ("%s", "bad section name");
				success = FALSE;
				goto cleanup;
			}
			/* new config section, skip the '[' */
            if (name != NULL)
            {
                free (name);
            }
			name = strdup (line + 1);
			name[line_len - 2] = '\0';
			debug ("config: new section '%s'", name);
			is_options = (strcmp(name, "options") == 0);
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
            set_error ("%s", "syntax error: missing key.");
			success = FALSE;
			goto cleanup;
		}
        /* For each directive, compare to the camelcase string. */
        if (name == NULL)
        {
            set_error ("%s", "All directives must belong to a section.");
            success = FALSE;
            goto cleanup;
        }
        /* Include is allowed in both options and repo sections */
        if (strcmp(key, "Include") == 0)
        {
            glob_t globbuf;
            int globret;
            size_t gindex;
            
            if (depth + 1 >= max_depth)
            {
                set_error ("parsing exceeded max recursion depth of %d", max_depth);
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
                if (strcmp (key, "UseSyslog") == 0)
                {
                    pac_conf->usesyslog = 1;
                    debug ("config: usesyslog");
                }
                else if (strcmp (key, "VerbosePkgLists") == 0)
                {
                    pac_conf->verbosepkglists = 1;
                    debug ("config: verbosepkglists");
                }
                else if (strcmp (key, "UseDelta") == 0)
                {
                    pac_conf->usedelta = 1;
                    debug ("config: usedelta");
                }
                else if (strcmp (key, "CheckSpace") == 0)
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
                
                if (strcmp (key, "NoUpgrade") == 0)
                {
                    setrepeatingoption (value, "NoUpgrade", &(pac_conf->noupgrades));
                }
                else if (strcmp (key, "NoExtract") == 0)
                {
                    setrepeatingoption (value, "NoExtract", &(pac_conf->noextracts));
                }
                else if (strcmp (key, "IgnorePkg") == 0)
                {
                    setrepeatingoption (value, "IgnorePkg", &(pac_conf->ignorepkgs));
                }
                else if (strcmp (key, "IgnoreGroup") == 0)
                {
                    setrepeatingoption (value, "IgnoreGroup", &(pac_conf->ignoregroups));
                }
                else if (strcmp (key, "SyncFirst") == 0)
                {
                    setrepeatingoption (value, "SyncFirst", &(pac_conf->syncfirst));
                }
                else if (strcmp (key, "CacheDir") == 0)
                {
                    setrepeatingoption (value, "CacheDir", &(pac_conf->cachedirs));
                }
                else if (strcmp (key, "Architecture") == 0)
                {
                    if (strcmp(value, "auto") == 0)
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
                else if (strcmp (key, "DBPath") == 0)
                {
                    pac_conf->dbpath = strdup (value);
                    debug ("config: dbpath: %s", value);
                }
                else if (strcmp (key, "RootDir") == 0)
                {
                    pac_conf->rootdir = strdup (value);
                    debug ("config: rootdir: %s", value);
                }
                else if (strcmp (key, "GPGDir") == 0)
                {
                    pac_conf->gpgdir = strdup (value);
                    debug ("config: gpgdir: %s", value);
                }
                else if (strcmp (key, "LogFile") == 0)
                {
                    pac_conf->logfile = strdup (value);
                    debug ("config: logfile: %s", value);
                }
                else if (strcmp (key, "SigLevel") == 0)
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
                /* we silently ignore "unrecognized" options, since we don't
                 * parse all of pacman's options anyways... */
            }
        }
        /* ... or in a repo section */
        else
        {
            if (cur_db == NULL)
            {
                cur_db = calloc (1, sizeof (*cur_db));
                cur_db->name = strdup (name);
            }
            
            if (strcmp (key, "Server") == 0)
            {
                if (value == NULL)
                {
                    set_error ("directive %s needs a value.", key);
                    success = FALSE;
                    goto cleanup;
                }
                cur_db->servers = alpm_list_add (cur_db->servers, strdup (value));
            }
            else if (strcmp (key, "SigLevel") == 0)
            {
                siglevel_def_t *siglevel_def;
                siglevel_def = calloc (1, sizeof (*siglevel_def));
                siglevel_def->file = strdup (file);
                siglevel_def->linenum = linenum;
                siglevel_def->def = strdup (value);
                cur_db->siglevel_def = alpm_list_add (cur_db->siglevel_def, siglevel_def);
			}
            else
            {
                set_error ("directive %s in section %s not recognized.", key, name);
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
        for (i = pac_conf->databases; i; i = alpm_list_next (i))
        {
            database_t *db = i->data;
            db->siglevel = pac_conf->siglevel;
            for (j = db->siglevel_def; j; j = alpm_list_next (j))
            {
				siglevel_def_t *siglevel_def = j->data;
                alpm_list_t *values = NULL;
				setrepeatingoption (siglevel_def->def, "SigLevel", &values);
				if (values)
                {
                    local_err = NULL;
					if (process_siglevel (values, &db->siglevel, siglevel_def->file,
                            siglevel_def->linenum, &local_err))
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
        if (name != NULL)
        {
            free (name);
            name = NULL;
        }
        /* so are the siglevel_def of each & all databases */
        for (i = pac_conf->databases; i; i = alpm_list_next (i))
        {
            database_t *db = i->data;
            for (j = db->siglevel_def; j; j = alpm_list_next (j))
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
    for (i = pac_conf->databases; i; i = alpm_list_next (i))
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
setstringoption (char *value, const char *option, char **cfg)
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
    
    *cfg = strreplace (value, "\\n", "\n");
    debug ("config: %s: %s", option, value);
}

#define set_error(fmt, ...)  g_set_error (error, KALU_ERROR, 1, \
    "Config file %s, line %d: " fmt, file, linenum, __VA_ARGS__);
/** inspired from pacman's function */
gboolean
parse_config_file (const char       *file,
                   conf_file_t       conf_file,
                   GError          **error)
{
	FILE       *fp              = NULL;
	char        line[MAX_PATH];
	int         linenum         = 0;
    char       *section         = NULL;
	int         success         = TRUE;

	debug ("config: attempting to read file %s", file);
	fp = fopen (file, "r");
	if (fp == NULL)
    {
        /* not an error if file does not exists */
        if (errno != ENOENT)
        {
            g_set_error (error, KALU_ERROR, 1, "Config file %s could not be read", file);
            success = FALSE;
        }
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
                set_error ("%s", "bad section name");
				success = FALSE;
				goto cleanup;
			}
			/* new config section, skip the '[' */
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
            set_error ("%s", "syntax error: missing key");
			success = FALSE;
			goto cleanup;
		}
        
        /* kalu.conf*/
        if (conf_file == CONF_FILE_KALU)
        {
            if (value == NULL)
            {
                set_error ("value missing for %s", key);
                success = FALSE;
                goto cleanup;
            }
            else if (strcmp ("options", section) == 0)
            {
                if (strcmp (key, "PacmanConf") == 0)
                {
                    setstringoption (value, "pacmanconf", &(config->pacmanconf));
                }
                else if (strcmp (key, "Interval") == 0)
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
                else if (strcmp (key, "Timeout") == 0)
                {
                    if (strcmp (value, "DEFAULT") == 0)
                    {
                        config->timeout = NOTIFY_EXPIRES_DEFAULT;
                    }
                    else if (strcmp (value, "NEVER") == 0)
                    {
                        config->timeout = NOTIFY_EXPIRES_NEVER;
                    }
                    else
                    {
                        int timeout = atoi (value);
                        if (timeout < 4 || timeout > 42)
                        {
                            set_error ("Invalid timeout delay: %s", value);
                            success = FALSE;
                            goto cleanup;
                        }
                        config->timeout = timeout * 1000; /* from seconds to ms */
                    }
                    debug ("config: timeout: %d", config->timeout);
                }
                else if (strcmp (key, "SkipPeriod") == 0)
                {
                    int begin_hour, begin_minute, end_hour, end_minute;
                    
                    if (sscanf (value, "%d:%d-%d:%d", &begin_hour, &begin_minute,
                            &end_hour, &end_minute) == 4)
                    {
                        if (begin_hour < 0 || begin_hour > 23
                            || begin_minute < 0 || begin_minute > 59
                            || end_hour < 0 || end_hour > 23
                            || end_minute < 0 || end_minute > 59)
                        {
                            set_error ("invalid value for SkipPeriod: %s", value);
                            success = FALSE;
                            goto cleanup;
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
                        set_error ("unable to parse SkipPeriod (must be HH:MM-HH:MM) : %s",
                                   value);
                        success = FALSE;
                        goto cleanup;
                    }
                }
                else if (strcmp (key, "UpgradeAction") == 0)
                {
                    if (strcmp (value, "NONE") == 0)
                    {
                        config->action = UPGRADE_NO_ACTION;
                    }
                    else if (strcmp (value, "KALU") == 0)
                    {
                        config->action = UPGRADE_ACTION_KALU;
                    }
                    else if (strcmp (value, "CMDLINE") == 0)
                    {
                        config->action = UPGRADE_ACTION_CMDLINE;
                    }
                    else
                    {
                        set_error ("Invalid value for UpgradeAction: %s", value);
                        success = FALSE;
                        goto cleanup;
                    }
                    debug ("config: action: %d", config->action);
                }
                else if (strcmp (key, "CmdLine") == 0)
                {
                    setstringoption (value, "cmdline", &(config->cmdline));
                }
                else if (strcmp (key, "CmdLineAur") == 0)
                {
                    setstringoption (value, "cmdline_aur", &(config->cmdline_aur));
                }
                else if (strcmp (key, "PostSysUpgrade") == 0)
                {
                    config->cmdline_post = alpm_list_add (config->cmdline_post,
                        strdup (value));
                    debug ("config: postsysupgrade: %s", value);
                }
                else if (strcmp (key, "AurIgnore") == 0)
                {
                    setrepeatingoption (value, "aur_ignore", &(config->aur_ignore));
                }
                else if (strcmp (key, "ManualChecks") == 0
                        || strcmp (key, "AutoChecks") == 0)
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
                        
                        if (strcmp ("UPGRADES", v) == 0)
                        {
                            checks |= CHECK_UPGRADES;
                        }
                        else if (strcmp ("WATCHED", v) == 0)
                        {
                            checks |= CHECK_WATCHED;
                        }
                        else if (strcmp ("AUR", v) == 0)
                        {
                            checks |= CHECK_AUR;
                        }
                        else if (strcmp ("WATCHED_AUR", v) == 0)
                        {
                            checks |= CHECK_WATCHED_AUR;
                        }
                        else if (strcmp ("NEWS", v) == 0)
                        {
                            checks |= CHECK_NEWS;
                        }
                        else
                        {
                            set_error ("unknown value for %s: %s", key, v);
                            success = FALSE;
                            goto cleanup;
                        }
                        
                        if (s)
                        {
                            v = s + 1;
                        }
                    }
                    
                    if (strcmp (key, "AutoChecks") == 0)
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
                else
                {
                    set_error ("unknown option: %s", key);
                    success = FALSE;
                    goto cleanup;
                }
            }
            else
            {
                templates_t *t;
                if (strcmp ("template-upgrades", section) == 0)
                {
                    t = config->tpl_upgrades;
                }
                else if (strcmp ("template-watched", section) == 0)
                {
                    t = config->tpl_watched;
                }
                else if (strcmp ("template-aur", section) == 0)
                {
                    t = config->tpl_aur;
                }
                else if (strcmp ("template-watched-aur", section) == 0)
                {
                    t = config->tpl_watched_aur;
                }
                else if (strcmp ("template-news", section) == 0)
                {
                    t = config->tpl_news;
                }
                else
                {
                    set_error ("unknown section: %s", section);
                    success = FALSE;
                    goto cleanup;
                }
                
                /* we're in a valid template */
                
                if (strcmp (key, "Title") == 0)
                {
                    setstringoption (value, "title", &(t->title));
                }
                else if (strcmp (key, "Package") == 0)
                {
                    setstringoption (value, "package", &(t->package));
                }
                else if (strcmp (key, "Sep") == 0)
                {
                    setstringoption (value, "sep", &(t->sep));
                }
            }
        }
        /* watched{,-aur}.conf */
        else if (conf_file == CONF_FILE_WATCHED || conf_file == CONF_FILE_WATCHED_AUR)
        {
            if (value == NULL)
            {
                set_error ("watched package %s: version number missing", key);
                success = FALSE;
                goto cleanup;
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
                w_pkg = calloc (1, sizeof (*w_pkg));
                w_pkg->name = strdup (key);
                w_pkg->version = strdup (value);
                *list = alpm_list_add (*list, w_pkg);
                debug ("config: watched (aur) packages: added %s %s",
                       w_pkg->name, w_pkg->version);
            }
        }
        /* news.conf */
        else if (conf_file == CONF_FILE_NEWS)
        {
            if (value == NULL)
            {
                set_error ("news data: value missing for %s", key);
                success = FALSE;
                goto cleanup;
            }
            else if (strcmp ("Last", key) == 0)
            {
                config->news_last = strdup (value);
                debug ("config: news_last: %s", value);
            }
            else if (strcmp ("Read", key) == 0)
            {
                config->news_read = alpm_list_add (config->news_read, strdup (value));
                debug ("config: news_read: added %s", value);
            }
        }
	}

cleanup:
	if (fp)
    {
		fclose(fp);
	}
    if (config->action == UPGRADE_ACTION_CMDLINE && config->cmdline == NULL)
    {
        config->action = UPGRADE_ACTION_KALU;
        debug ("config: action: no cmdline set, reverting to %d", config->action);
    }
	debug ("config: finished parsing %s", file);
	return success;
}
#undef set_error
