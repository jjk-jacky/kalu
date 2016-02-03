/**
 * kalu - Copyright (C) 2012-2016 Olivier Brunel
 *
 * util.c
 * Copyright (C) 2012-2016 Olivier Brunel <jjk@jjacky.com>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

/* alpm */
#include <alpm_list.h>

/* kalu */
#include "kalu.h"
#include "util.h"

/**
 * Makes sure the path for the filename exists. that is, makes sure every
 * bits before the last / exists and is a folder
 */
gboolean
ensure_path (char *path)
{
    char *s, *p;
    struct stat stat_info;

    p = path + 1; /* skip the root */
    while ((s = strchr (p, '/')))
    {
        *s = '\0';
        if (0 != stat (path, &stat_info))
        {
            /* if does not exist, we create it */
            if (errno == ENOENT)
            {
                debug ("mkdir %s", path);
                if (0 != mkdir (path, S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH
                            | S_IWOTH))
                {
                    *s = '/'; /* restore */
                    return FALSE;
                }
            }
            else
            {
                debug ("failed to stat %s: %s", path, strerror (errno));
                *s = '/'; /* restore */
                return FALSE;
            }
        }
        /* make sure it's a folder */
        else if (!S_ISDIR (stat_info.st_mode))
        {
            *s = '/'; /* restore */
            return FALSE;
        }
        *s = '/'; /* restore */
        p = s + 1;
    }
    return TRUE;
}

/*******************************************************************************
 * The following functions come from pacman's source code. (They might have
 * been (slightly) modified.)
 *
 * Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 * Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 * http://projects.archlinux.org/pacman.git
 *
 ******************************************************************************/

/**
 * Trim whitespace and newlines from a string
 */
char *
strtrim (char *str)
{
    char *pch = str;

    if (str == NULL || *str == '\0')
    {
        /* string is empty, so we're done. */
        return str;
    }

    while (isspace ((unsigned char) *pch))
    {
        pch++;
    }
    if (pch != str)
    {
        size_t len = strlen (pch);
        if (len)
        {
            memmove (str, pch, len + 1);
        }
        else
        {
            *str = '\0';
        }
    }

    /* check if there wasn't anything but whitespace in the string. */
    if (*str == '\0')
    {
        return str;
    }

    pch = (str + (strlen (str) - 1));
    while (isspace ((unsigned char) *pch))
    {
        pch--;
    }
    *++pch = '\0';

    return str;
}

/**
 * Replace all occurances of 'needle' with 'replace' in 'str', returning
 * a new string (must be free'd)
 */
char
*strreplace (const char *str, const char *needle, const char *replace)
{
    const char *p = NULL, *q = NULL;
    char *newstr = NULL, *newp = NULL;
    alpm_list_t *i = NULL, *list = NULL;
    size_t needlesz = strlen (needle), replacesz = strlen (replace);
    size_t newsz;

    if (!str)
    {
        return NULL;
    }

    p = str;
    q = strstr (p, needle);
    while (q)
    {
        list = alpm_list_add (list, (char *) q);
        p = q + needlesz;
        q = strstr (p, needle);
    }

    /* no occurences of needle found */
    if (!list)
    {
        return strdup (str);
    }
    /* size of new string = size of old string + "number of occurences of needle"
     * x "size difference between replace and needle" */
    newsz = strlen (str) + 1 + alpm_list_count (list) * (replacesz - needlesz);
    newstr = new0 (char, newsz);
    if (!newstr)
    {
        return NULL;
    }

    p = str;
    newp = newstr;
    FOR_LIST (i, list)
    {
        q = i->data;
        if (q > p)
        {
            /* add chars between this occurence and last occurence, if any */
            memcpy (newp, p, (size_t) (q - p));
            newp += q - p;
        }
        memcpy (newp, replace, replacesz);
        newp += replacesz;
        p = q + needlesz;
    }
    alpm_list_free (list);

    if (*p)
    {
        /* add the rest of 'p' */
        strcpy (newp, p);
    }

    return newstr;
}

gboolean
trans_init (kalu_alpm_t *alpm, alpm_transflag_t flags, int check_valid, GError **error)
{
    GError *local_err = NULL;

    if (!check_syncdbs (alpm, 0, check_valid, &local_err))
    {
        g_propagate_error (error, local_err);
        return FALSE;
    }

    if (alpm_trans_init (alpm->handle, flags) == -1)
    {
        g_set_error (error, KALU_ERROR, 1,
                _("Failed to initiate transaction: %s"),
                alpm_strerror (alpm_errno (alpm->handle)));
        return FALSE;
    }

    return TRUE;
}

gboolean
trans_release (kalu_alpm_t *alpm, GError **error)
{
    if (alpm_trans_release (alpm->handle) == -1)
    {
        g_set_error (error, KALU_ERROR, 2,
                _("Failed to release transaction: %s"),
                alpm_strerror (alpm_errno (alpm->handle)));
        return FALSE;
    }
    return TRUE;
}

/** Converts sizes in bytes into human readable units.
 *
 * @param bytes the size in bytes
 * @param target_unit '\0' or a short label. If equal to one of the short unit
 * labels ('B', 'K', ...) bytes is converted to target_unit; if '\0', the first
 * unit which will bring the value to below a threshold of 2048 will be chosen.
 * @param long_labels whether to use short ("K") or long ("KiB") unit labels
 * @param label will be set to the appropriate unit label
 *
 * @return the size in the appropriate unit
 */
double
humanize_size (off_t bytes, const char target_unit, const char **label)
{
    static const char *labels[] = {"B", "KiB", "MiB", "GiB",
        "TiB", "PiB", "EiB", "ZiB", "YiB"};
    static const int unitcount = sizeof (labels) / sizeof (labels[0]);

    double val = (double) bytes;
    int index;

    for (index = 0; index < unitcount - 1; index++)
    {
        if (target_unit != '\0' && labels[index][0] == target_unit)
        {
            break;
        }
        else if (target_unit == '\0' && val <= 2048.0 && val >= -2048.0)
        {
            break;
        }
        val /= 1024.0;
    }

    if (label)
    {
        *label = labels[index];
    }

    return val;
}

/* does the same thing as 'rm -rf' */
int
rmrf (const char *path)
{
    int              errflag = 0;
    struct dirent   *dp;
    DIR             *dirp;

    if (unlink (path))
    {
        if (errno == ENOENT)
        {
            goto done;
        }
        else if (errno == EPERM)
        {
            /* fallthrough */
        }
        else if (errno == EISDIR)
        {
            /* fallthrough */
        }
        else if (errno == ENOTDIR)
        {
            errflag = 1;
            goto done;
        }
        else
        {
            /* not a directory */
            errflag = 1;
            goto done;
        }

        dirp = opendir (path);
        if (!dirp)
        {
            errflag = 1;
            goto done;
        }
        for (dp = readdir (dirp); dp != NULL; dp = readdir (dirp))
        {
            if (dp->d_ino)
            {
                if (!streq (dp->d_name, "..") && !streq (dp->d_name, "."))
                {
                    char name[MAX_PATH];
                    if (snprintf (name, MAX_PATH, "%s/%s",
                                path, dp->d_name) < MAX_PATH)
                    {
                        errflag += rmrf (name);
                    }
                    else
                    {
                        ++errflag;
                    }
                }
            }
        }
        closedir (dirp);
        if (rmdir (path))
        {
            ++errflag;
        }

    }
done:
    debug ("removing %s %s (%d)",
            path,
            (!errflag) ? "success" : "failed",
            errflag);
    return errflag;
}

gboolean
check_syncdbs (kalu_alpm_t *alpm, size_t need_repos, int check_valid, GError **error)
{
    alpm_list_t *i;
    alpm_list_t *sync_dbs = alpm_get_syncdbs (alpm->handle);

    if (need_repos && sync_dbs == NULL)
    {
        g_set_error (error, KALU_ERROR, 1,
                _("No usable package repositories configured"));
        return FALSE;
    }

    if (check_valid)
    {
        /* ensure all known dbs are valid */
        FOR_LIST (i, sync_dbs)
        {
            alpm_db_t *db = i->data;
            if (alpm_db_get_valid (db))
            {
                g_set_error (error, KALU_ERROR, 1,
                        _("Database %s is not valid: %s"),
                        alpm_db_get_name (db),
                        alpm_strerror (alpm_errno (alpm->handle)));
                return FALSE;
            }
        }
    }
    return TRUE;
}

/******************************************************************************/

inline void
snprint_size (char *buf, int buflen, double size, const char *unit)
{
    const char *fmt;
    if (unit && *unit == 'B' && *(unit + 1) == '\0')
    {
        fmt = "%.0f %s";
    }
    else
    {
        fmt = "%.2f %s";
    }
    snprintf (buf, (size_t) buflen, fmt, size, unit);
}

void
parse_tpl (
        const char       *tpl,
        char            **text,
        unsigned int     *len,
        unsigned int     *alloc,
        replacement_t   **_replacements,
        gboolean          escaping
        )
{
    replacement_t *r, **replacements;
    const char *t;
    char *s;
    char *b;
    size_t l;
    char found;

    if (*tpl)
        return;

    for (t = tpl, s = *text + *len; *t; ++t)
    {
        found = 0;

        /* placeholder? */
        if (*t == '$')
        {
            replacements = _replacements;
            for (r = *replacements; r; ++replacements, r = *replacements)
            {
                if (r->name == NULL || r->value == NULL)
                {
                    continue;
                }

                l = strlen (r->name);
                if (strncmp (t + 1, r->name, l) == 0)
                {
                    for (b = r->value; *b; ++b)
                    {
                        const gchar *esc[] =
                        {
                            "&",  "&amp;",
                            "'",  "&apos;",
                            "\"", "&quot;",
                            "<",  "&lt;",
                            ">",  "&gt;",
                            NULL
                        };
                        gchar *_s;
                        unsigned int _l, i;

                        _s = b;
                        _l = 1;
                        if (escaping && r->need_escaping)
                        {
                            const gchar **e;

                            for (e = esc; *e; e += 2)
                            {
                                if (*_s == *e[0])
                                {
                                    _s = (gchar *) e[1];
                                    _l = (unsigned int) strlen (_s);
                                    break;
                                }
                            }
                        }

                        /* enough memory? */
                        if (*len + _l >= *alloc)
                        {
                            *alloc += 1024;
                            *text = renew (gchar, *alloc + 1, *text);
                            s = *text + *len;
                        }

                        for (i = 0; i < _l; ++i, ++*len)
                            *s++ = *_s++;
                    }
                    t += l;
                    found = 1;
                    break;
                }
            }
        }
        /* not a placeholder, or nothing known/supported, i.e. just a '$' */
        if (found == 0)
        {
            /* copy character */
            *s++ = *t;
            ++*len;
        }
        /* out of memory? */
        if (*len >= *alloc)
        {
            *alloc += 1024;
            *text = renew (gchar, *alloc + 1, *text);
            s = *text + *len;
        }
    }

    *s = '\0';
}

int
watched_package_cmp (watched_package_t *w_pkg1, watched_package_t *w_pkg2)
{
    int ret;
    ret = strcmp (w_pkg1->name, w_pkg2->name);
    if (ret == 0)
    {
        ret = strcmp (w_pkg1->version, w_pkg2->version);
    }
    return ret;
}
