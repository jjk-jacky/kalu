/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * aur.c
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

#define _BSD_SOURCE /* for strdup w/ -std=c99 */

/* C */
#include <string.h>

/* glib */
#include <glib-2.0/glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* cjson */
#include "cJSON.h"

/* kalu */
#include "kalu.h"
#include "aur.h"
#include "curl.h"

#define MAX_URL_LENGTH          1024

static void *
get_pkg_from_list (const char *pkgname, alpm_list_t *pkgs, gboolean is_watched)
{
    alpm_list_t *i;
    const char *name;
    for (i = pkgs; i; i = alpm_list_next (i))
    {
        if (is_watched)
        {
            name = ((watched_package_t *) i->data)->name;
        }
        else
        {
            name = alpm_pkg_get_name ((alpm_pkg_t *) i->data);
        }
        if (strcmp (pkgname, name) == 0)
        {
            return i->data;
        }
    }
    return NULL;
}

#define add(str)    do {                                \
        len = snprintf (s, (size_t) max, "%s", str);    \
        max -= len;                                     \
        s += len;                                       \
    } while (0)
gboolean
aur_has_updates (alpm_list_t **packages,
                 alpm_list_t *aur_pkgs,
                 gboolean is_watched,
                 GError **error)
{
    alpm_list_t *urls = NULL, *i;
    char buf[MAX_URL_LENGTH + 1], *s;
    int max, len;
    size_t len_prefix = strlen (AUR_URL_PREFIX_PKG);
    GError *local_err = NULL;
    char *data;
    const char *pkgname, *pkgver, *oldver;
    cJSON *json, *results, *package;
    int c, j;
    void *pkg;
    kalu_package_t *kpkg;
    
    debug ("looking for AUR updates");
    
    /* print start of url */
    max = MAX_URL_LENGTH;
    s = buf;
    add (AUR_URL_PREFIX);
    
    for (i = aur_pkgs; i; i = alpm_list_next (i))
    {
        if (is_watched)
        {
            pkgname = ((watched_package_t *) i->data)->name;
        }
        else
        {
            pkgname = alpm_pkg_get_name ((alpm_pkg_t *) i->data);
        }
        
        /* still enough space to add this pkgname ? */
        if ((int) (len_prefix + strlen (pkgname)) > max)
        {
            /* nope. so we store this url and start a new one */
            urls = alpm_list_add (urls, strdup (buf));
            max = MAX_URL_LENGTH;
            s = buf;
            add (AUR_URL_PREFIX);
        }
        /* add pkgname */
        add (AUR_URL_PREFIX_PKG);
        add (pkgname);
    }
    urls = alpm_list_add (urls, strdup (buf));
    
    /* download */
    for (i = urls; i; i = alpm_list_next (i))
    {
        data = curl_download (i->data, &local_err);
        if (local_err != NULL)
        {
            g_propagate_error (error, local_err);
            FREELIST (urls);
            FREE_PACKAGE_LIST (*packages);
            return FALSE;
        }
        free (i->data);
        i->data = NULL;
        
        /* parse json */
        debug ("parsing json");
        json = cJSON_Parse (data);
        results = cJSON_GetObjectItem (json, "results");
        c = cJSON_GetArraySize (results);
        debug ("got %d results", c);
        for (j = 0; j < c; ++j)
        {
            package = cJSON_GetArrayItem (results, j);
            if (package)
            {
                /* AUR */
                pkgname = cJSON_GetObjectItem (package, "Name")->valuestring;
                pkgver = cJSON_GetObjectItem (package, "Version")->valuestring;
                /* ALPM/watched */
                pkg = get_pkg_from_list (pkgname, aur_pkgs, is_watched);
                if (is_watched)
                {
                    oldver = ((watched_package_t *) pkg)->version;
                }
                else
                {
                    oldver = alpm_pkg_get_version ((alpm_pkg_t *) pkg);
                }
                /* is AUR newer? */
                if (alpm_pkg_vercmp (pkgver, oldver) == 1)
                {
                    debug ("%s %s -> %s", pkgname, oldver, pkgver);
                    kpkg = calloc (1, sizeof (*kpkg));
                    kpkg->name = strdup (pkgname);
                    kpkg->old_version = strdup (oldver);
                    kpkg->new_version = strdup (pkgver);
                    *packages = alpm_list_add (*packages, kpkg);
                }
            }
        }
        cJSON_Delete (json);
        free (data);
    }
    alpm_list_free (urls);
    urls = NULL;
    
    return (*packages != NULL);
}
#undef add

