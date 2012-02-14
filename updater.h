
#ifndef _KALU_UPDATER_H
#define _KALU_UPDATER_H

/* glib */
#include <glib.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

void
updater_run (alpm_list_t *cmdline_post);

#endif /* _KALU_UPDATER_H */
