/**
 * kalu - Copyright (C) 2012-2016 Olivier Brunel
 *
 * aur.c
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
#include <string.h>
#include <ctype.h>  /* isalnum() */

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
    FOR_LIST (i, pkgs)
    {
        if (is_watched)
        {
            name = ((watched_package_t *) i->data)->name;
        }
        else
        {
            name = alpm_pkg_get_name ((alpm_pkg_t *) i->data);
        }
        if (streq (pkgname, name))
        {
            return i->data;
        }
    }
    return NULL;
}

static gint
find_nf (gpointer data, gpointer _find_data)
{
    struct {
        gboolean is_watched;
        const gchar *pkgname;
    } *find_data = _find_data;

    if (find_data->is_watched)
    {
        return strcmp (((watched_package_t *) data)->name, find_data->pkgname);
    }
    else
    {
        return strcmp (alpm_pkg_get_name ((alpm_pkg_t *) data), find_data->pkgname);
    }
}

#define add(str)    do {                            \
    len = snprintf (s, (size_t) max, "%s", str);    \
    max -= len;                                     \
    s += len;                                       \
} while (0)
gboolean
aur_has_updates (alpm_list_t **packages,
                 alpm_list_t **not_found,
                 alpm_list_t *aur_pkgs,
                 gboolean is_watched,
                 GError **error)
{
    alpm_list_t *urls = NULL, *i;
    alpm_list_t *list_nf = NULL;
    char buf[MAX_URL_LENGTH + 1], *s;
    int max, len;
    int len_prefix = (int) strlen (AUR_URL_PREFIX_PKG);
    GError *local_err = NULL;
    char *data;
    const char *pkgname, *pkgdesc, *pkgver, *oldver;
    cJSON *json, *results, *package;
    int c, j;
    void *pkg;
    kalu_package_t *kpkg;

    debug ((is_watched)
            ? "looking for Watched AUR updates"
            : "looking for AUR updates");

    /* print start of url */
    max = MAX_URL_LENGTH;
    s = buf;
    add (AUR_URL_PREFIX);

    FOR_LIST (i, aur_pkgs)
    {
        char *end;
        const char *p;

        if (is_watched)
        {
            pkgname = ((watched_package_t *) i->data)->name;
        }
        else
        {
            pkgname = alpm_pkg_get_name ((alpm_pkg_t *) i->data);
        }

        /* fill list of not found packages */
        if (not_found || is_watched)
        {
            list_nf = alpm_list_add (list_nf, i->data);
        }

        /* make sure we can at least add the prefix */
        if (len_prefix > max)
        {
            /* nope. so we store this url and start a new one */
            urls = alpm_list_add (urls, strdup (buf));
            max = MAX_URL_LENGTH;
            s = buf;
            add (AUR_URL_PREFIX);
        }

        /* this is where we'll end the URL, should there not be enough space */
        end = s;
        /* add prefix */
        add (AUR_URL_PREFIX_PKG);
        /* now we add the pkgname; one char at a time, urlencoding if needed */
        for (p = pkgname; *p; ++p)
        {
            /* unreserved characters */
            if (isalnum (*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~')
            {
                if (max < 1)
                {
                    /* add url until this pkg and start a new one */
                    *end = '\0';
                    urls = alpm_list_add (urls, strdup (buf));
                    max = MAX_URL_LENGTH;
                    s = buf;
                    add (AUR_URL_PREFIX);
                    /* reset to start this pkgname over */
                    end = s;
                    add (AUR_URL_PREFIX_PKG);
                    p = pkgname - 1;
                    continue;
                }
                *s++ = *p;
                --max;
            }
            else
            {
                const char hex[] = "0123456789ABCDEF";

                if (max < 3)
                {
                    /* add url until this pkg and start a new one */
                    *end = '\0';
                    urls = alpm_list_add (urls, strdup (buf));
                    max = MAX_URL_LENGTH;
                    s = buf;
                    add (AUR_URL_PREFIX);
                    /* reset to start this pkgname over */
                    end = s;
                    add (AUR_URL_PREFIX_PKG);
                    p = pkgname - 1;
                    continue;
                }
                /* Credit to Fred Bullback for the following */
                *s++ = '%';
                *s++ = hex[*p >> 4];
                *s++ = hex[*p & 15];
                max -= 3;
            }
        }
        *s = '\0';
    }
    urls = alpm_list_add (urls, strdup (buf));

    /* download */
    FOR_LIST (i, urls)
    {
        data = curl_download (i->data, &local_err);
        if (local_err != NULL)
        {
            g_propagate_error (error, local_err);
            FREELIST (urls);
            FREE_PACKAGE_LIST (*packages);
            alpm_list_free (list_nf);
            return FALSE;
        }

        /* parse json */
        debug ("parsing json");
        json = cJSON_Parse (data);
        if (!json)
        {
            debug ("invalid json");
            g_set_error (error, KALU_ERROR, 8,
                    _("Invalid JSON response from the AUR"));
            FREELIST (urls);
            FREE_PACKAGE_LIST (*packages);
            alpm_list_free (list_nf);
            free (data);
            return FALSE;
        }
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
                pkgdesc = cJSON_GetObjectItem (package, "Description")->valuestring;
                /* because desc is not required */
                if (!pkgdesc)
                    pkgdesc = "";
                pkgver = cJSON_GetObjectItem (package, "Version")->valuestring;
                /* ALPM/watched */
                pkg = get_pkg_from_list (pkgname, aur_pkgs, is_watched);
                if (!pkg)
                {
                    debug ("package %s not found in aur_pkgs", pkgname);
                    g_set_error (error, KALU_ERROR, 8,
                            _("Unexpected results from the AUR [%s]"),
                            pkgname);
                    FREELIST (urls);
                    FREE_PACKAGE_LIST (*packages);
                    alpm_list_free (list_nf);
                    free (data);
                    cJSON_Delete (json);
                    return FALSE;
                }
                /* remove from list of not found packages */
                if (list_nf)
                {
                    struct {
                        gboolean is_watched;
                        const gchar *pkgname;
                    } find_data = { is_watched, pkgname };
                    list_nf = alpm_list_remove (list_nf, &find_data,
                            (alpm_list_fn_cmp) find_nf, NULL);
                }
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
                    kpkg = new0 (kalu_package_t, 1);
                    kpkg->name = strdup (pkgname);
                    kpkg->desc = strdup (pkgdesc);
                    kpkg->old_version = strdup (oldver);
                    kpkg->new_version = strdup (pkgver);
                    *packages = alpm_list_add (*packages, kpkg);
                }
            }
        }
        cJSON_Delete (json);
        free (data);
    }
    FREELIST (urls);

    /* turn not_found into a list of kalu_package_t as it should be, or add them
     * to packages (if not_found is NULL, i.e. is_watched is TRUE) */
    if (list_nf)
    {
        FOR_LIST (i, list_nf)
        {
            kpkg = new0 (kalu_package_t, 1);

            if (is_watched)
            {
                watched_package_t *wp = i->data;

                kpkg->name = strdup (wp->name);
                kpkg->desc = strdup (_("<package not found>"));
                kpkg->old_version = strdup (wp->version);
                kpkg->new_version = strdup ("-");
            }
            else
            {
                alpm_pkg_t *p = i->data;

                kpkg->name = strdup (alpm_pkg_get_name (p));
                kpkg->desc = strdup (alpm_pkg_get_desc (p));
                kpkg->old_version = strdup (alpm_pkg_get_version (p));
            }

            if (not_found)
            {
                debug ("adding to not found: package %s", kpkg->name);
                *not_found = alpm_list_add (*not_found, kpkg);
            }
            else
            {
                debug ("not found: %s", kpkg->name);
                *packages = alpm_list_add (*packages, kpkg);
            }
        }
        alpm_list_free (list_nf);
    }

    return (*packages != NULL);
}
#undef add
