/**
 * kalu - Copyright (C) 2012-2013 Olivier Brunel
 *
 * curl.c
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

#include <config.h>

/* C */
#include <string.h>

/* curl */
#include <curl/curl.h>

/* kalu */
#include "kalu.h"
#include "curl.h"

/* struct to hold data downloaded via curl */
typedef struct _string_t {
    char   *content;
    size_t  len;
    size_t  alloc;
} string_t;

static size_t
curl_write (void *content, size_t size, size_t nmemb, string_t *data)
{
    size_t total = size * nmemb;

    /* alloc memory if needed */
    if (data->len + total >= data->alloc)
    {
        data->alloc += total + 1024;
        data->content = renew (char, data->alloc, data->content);
    }

    /* copy data */
    memcpy (&(data->content[data->len]), content, total);
    data->len += total;

    return total;
}

char *
curl_download (const char *url, GError **error)
{
    CURL *curl;
    string_t data;
    char errmsg[CURL_ERROR_SIZE];

    debug ("downloading %s", url);
    zero (data);

    curl = curl_easy_init();
    if (!curl)
    {
        g_set_error (error, KALU_ERROR, 1, _("Unable to init cURL\n"));
        return NULL;
    }

    curl_easy_setopt (curl, CURLOPT_USERAGENT, PACKAGE_NAME "/" PACKAGE_VERSION);
    curl_easy_setopt (curl, CURLOPT_URL, url);
    curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, (curl_write_callback) curl_write);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &data);
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, errmsg);
    if (config->use_ip == IPv4)
    {
        debug ("set curl to IPv4");
        curl_easy_setopt (curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    }
    else if (config->use_ip == IPv6)
    {
        debug ("set curl to IPv6");
        curl_easy_setopt (curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
    }

    if (curl_easy_perform (curl) != 0)
    {
        curl_easy_cleanup (curl);
        if (data.content != NULL)
        {
            free (data.content);
        }
        g_set_error (error, KALU_ERROR, 1, "%s", errmsg);
        return NULL;
    }
    curl_easy_cleanup (curl);
    debug ("downloaded %d bytes", data.len);

    /* content is not NULL-terminated yet */
    data.content[data.len] = '\0';

    return data.content;
}
