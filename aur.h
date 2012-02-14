
#ifndef _KALU_AUR_H
#define _KALU_AUR_H

#define AUR_URL_PREFIX      "http://aur.archlinux.org/rpc.php?type=multiinfo"
#define AUR_URL_PREFIX_PKG  "&arg[]="

gboolean
aur_has_updates (alpm_list_t **packages,
                 alpm_list_t *aur_pkgs,
                 gboolean is_watched,
                 GError **error);

#endif /* _KALU_AUR_H */
