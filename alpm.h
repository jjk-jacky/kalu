
#ifndef _KALU_ALPM_H
#define _KALU_ALPM_H

/* glib */
#include <glib-2.0/glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

typedef struct _kalu_alpm_t {
    char            *dbpath; /* the tmp-path where we copied dbs */
    alpm_handle_t   *handle;
    alpm_transflag_t flags;
} kalu_alpm_t;

/* global variable */
extern unsigned short alpm_verbose;

gboolean
kalu_alpm_load (const gchar *conffile, GError **error);

gboolean
kalu_alpm_syncdbs (GError **error);

gboolean
kalu_alpm_has_updates (alpm_list_t **packages, GError **error);

gboolean
kalu_alpm_has_updates_watched (alpm_list_t **packages, alpm_list_t *watched, GError **error);

gboolean
kalu_alpm_has_foreign (alpm_list_t **packages, alpm_list_t *ignore, GError **error);

void
kalu_alpm_free (void);

#endif /* _KALU_ALPM_H */
