
#ifndef _KALU_CURL_H
#define _KALU_CURL_H

/* glib */
#include <glib-2.0/glib.h>

char *
curl_download (const char *url, GError **error);

#endif /* _KALU_CURL_H */
