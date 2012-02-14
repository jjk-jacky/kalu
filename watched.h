
#ifndef _KALU_WATCHED_H
#define _KALU_WATCHED_H

void watched_update (alpm_list_t *packages, gboolean is_aur);
void watched_manage (gboolean is_aur);

#endif /* _KALU_WATCHED_H */
