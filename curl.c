
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
        data->content = realloc (data->content, data->alloc + total + 1024);
        data->alloc += total + 1024;
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
    memset (&data, 0, sizeof (string_t));
    
    curl = curl_easy_init();
    if (!curl)
    {
        g_set_error (error, KALU_ERROR, 1, "Unable to init cURL\n");
        return NULL;
    }
    
    curl_easy_setopt (curl, CURLOPT_USERAGENT, "kalu/" KALU_VERSION);
    curl_easy_setopt (curl, CURLOPT_URL, url);
    curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &data);
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, errmsg);
    
    if (curl_easy_perform (curl) != 0)
    {
        curl_easy_cleanup (curl);
        if (data.content != NULL)
        {
            free (data.content);
        }
        g_set_error (error, KALU_ERROR, 1, errmsg);
        return NULL;
    }
    curl_easy_cleanup (curl);
    debug ("downloaded %d bytes", data.len);
    
    /* content is not NULL-terminated yet */
    data.content[data.len] = '\0';
    
    return data.content;
}
