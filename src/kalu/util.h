/**
 * kalu - Copyright (C) 2012-2013 Olivier Brunel
 *
 * util.h
 * Copyright (C) 2012-2013 Olivier Brunel <i.am.jack.mail@gmail.com>
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

#ifndef _KALU_UTIL_H
#define _KALU_UTIL_H

/* glib */
#include <glib.h>

/* alpm */
#include <alpm.h>

/* kalu */
#include "kalu.h"
#include "kalu-alpm.h"

typedef struct _replacement_t {
    const char *name;
    char *value;
    gboolean need_escaping;
} replacement_t;

gboolean
ensure_path (char *path);

char *
strtrim (char *str);

char
*strreplace (const char *str, const char *needle, const char *replace);

gboolean
check_syncdbs (kalu_alpm_t *alpm, size_t need_repos, int check_valid, GError **error);

gboolean
trans_init (kalu_alpm_t *alpm, alpm_transflag_t flags, int check_valid, GError **error);

gboolean
trans_release (kalu_alpm_t *alpm, GError **error);

double
humanize_size(off_t bytes, const char target_unit, const char **label);

int
rmrf (const char *path);

void
snprint_size (char *buf, int buflen, double size, const char *unit);

void
parse_tpl (char *tpl, char **text, unsigned int *len, unsigned int *alloc,
           replacement_t **replacements, gboolean escaping);

int
watched_package_cmp (watched_package_t *w_pkg1, watched_package_t *w_pkg2);

#endif /* _KALU_UTIL_H */
