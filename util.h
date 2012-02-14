
#ifndef _KALU_UTIL_H
#define _KALU_UTIL_H

/* glib */
#include <glib.h>

/* alpm */
#include "alpm.h"

/* kalu */
#include "kalu.h"

typedef struct _replacement_t {
    const char *name;
    char *value;
} replacement_t;

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
parse_tpl (char *tpl, char **text, unsigned int *len, unsigned int *alloc,
           replacement_t **replacements);

int
watched_package_cmp (watched_package_t *w_pkg1, watched_package_t *w_pkg2);

#endif /* _KALU_UTIL_H */
