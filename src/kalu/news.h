/**
 * kalu - Copyright (C) 2012-2013 Olivier Brunel
 *
 * news.h
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

gboolean
show_help (GError **error);

gboolean
show_history (GError **error);

void
show_pacman_conflict (void);

#endif /* _KALU_NEWS_H */
