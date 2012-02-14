
#ifndef _KALU_NEWS_H
#define _KALU_NEWS_H

/* glib */
#include <glib-2.0/glib.h>

/* alpm list */
#include <alpm_list.h>

gboolean
news_has_updates (alpm_list_t **titles,
                  gchar       **xml_news,
                  GError      **error);

gboolean
news_show (gchar *xml_news, gboolean only_updates, GError **error);

#endif /* _KALU_NEWS_H */
