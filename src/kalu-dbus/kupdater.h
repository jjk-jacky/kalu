/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * kupdater.h
 * Copyright (C) 2012-2016 Olivier Brunel <jjk@jjacky.com>
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

#ifndef _KALU_KUPDATER_H
#define _KALU_KUPDATER_H

/* keep in sync w/ kalu_alpm_syncdbs in kalu-alpm.c */
typedef enum _sync_db_results_t {
    SYNC_SUCCESS,
    SYNC_FAILURE,
    SYNC_NOT_NEEDED
} sync_db_results_t;

/* same as alpm_loglevel_t */
typedef enum _loglevel_t {
    LOG_ERROR    = 1,
    LOG_WARNING  = (1 << 1),
    LOG_DEBUG    = (1 << 2),
    LOG_FUNCTION = (1 << 3)
} loglevel_t;

/* type of events for signal "Event" & "Progress" */
typedef enum _event_t {
    /* Event */
    EVENT_CHECKING_DEPS,
    EVENT_RESOLVING_DEPS,
    EVENT_INTERCONFLICTS,
    EVENT_RETRIEVING_PKGS,
    EVENT_RETRIEVING_PKGS_DONE,
    EVENT_RETRIEVING_PKGS_FAILED,
    EVENT_KEY_DOWNLOAD,
    EVENT_HOOKS_PRE,
    EVENT_HOOKS_POST,
    EVENT_TRANSACTION,
    /* Progress */
    EVENT_INSTALLING,
    EVENT_REINSTALLING,
    EVENT_UPGRADING,
    EVENT_DOWNGRADING,
    EVENT_REMOVING,
    EVENT_FILE_CONFLICTS,
    EVENT_CHECKING_DISKSPACE,
    EVENT_PKG_INTEGRITY,
    EVENT_LOAD_PKGFILES,
    EVENT_KEYRING,
} event_t;

typedef enum _event_type_t {
    EVENT_TYPE_START,
    EVENT_TYPE_DONE
} event_type_t;

#endif /* _KALU_KUPDATER_H */
