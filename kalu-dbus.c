/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * kalu-dbus.c
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

/* C */
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h> /* off_t */

/* PolicyKit */
#include <polkit/polkit.h>

/* gio - for dbus */
#include <gio/gio.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* kalu */
#include "updater-dbus.h"

#define _UNUSED_                __attribute__ ((unused)) 

#define CHOICE_FREE             -1
#define CHOICE_WAITING          -2

static GDBusNodeInfo *introspection_data = NULL;
static GDBusConnection *connection = NULL;

static GMainLoop *loop;

static gboolean       is_init = FALSE;
static gchar         *client  = NULL;
static alpm_handle_t *handle  = NULL;

static gchar buffer[1024];

#define debug(...)     do {                                             \
    snprintf (buffer, 1024, __VA_ARGS__);                               \
    g_dbus_connection_emit_signal (connection,                          \
                                   client,                              \
                                   OBJECT_PATH,                         \
                                   INTERFACE_NAME,                      \
                                   "Debug",                             \
                                   g_variant_new ("(s)", buffer),       \
                                   NULL);                               \
    } while (0)

#define emit_signal(name, fmt, ...)                                            \
    g_dbus_connection_emit_signal (connection,                                 \
                                   client,                                     \
                                   OBJECT_PATH,                                \
                                   INTERFACE_NAME,                             \
                                   name,                                       \
                                   g_variant_new ("(" fmt ")", __VA_ARGS__),   \
                                   NULL);

#define emit_signal_no_params(name)                                            \
    g_dbus_connection_emit_signal (connection,                                 \
                                   client,                                     \
                                   OBJECT_PATH,                                \
                                   INTERFACE_NAME,                             \
                                   name,                                       \
                                   NULL,                                       \
                                   NULL);

/**
 * Replace all occurances of 'needle' with 'replace' in 'str', returning
 * a new string (must be free'd)
 */
static char
*strreplace (const char *str, const char *needle, const char *replace)
{
	const char *p = NULL, *q = NULL;
	char *newstr = NULL, *newp = NULL;
	alpm_list_t *i = NULL, *list = NULL;
	size_t needlesz = strlen(needle), replacesz = strlen(replace);
	size_t newsz;

	if(!str) {
		return NULL;
	}

	p = str;
	q = strstr(p, needle);
	while(q) {
		list = alpm_list_add(list, (char *)q);
		p = q + needlesz;
		q = strstr(p, needle);
	}

	/* no occurences of needle found */
	if(!list) {
		return strdup(str);
	}
	/* size of new string = size of old string + "number of occurences of needle"
	 * x "size difference between replace and needle" */
	newsz = strlen(str) + 1 +
		alpm_list_count(list) * (replacesz - needlesz);
	newstr = calloc(newsz, sizeof(char));
	if(!newstr) {
		return NULL;
	}

	p = str;
	newp = newstr;
	for(i = list; i; i = alpm_list_next(i)) {
		q = i->data;
		if(q > p) {
			/* add chars between this occurence and last occurence, if any */
			memcpy(newp, p, (size_t)(q - p));
			newp += q - p;
		}
		memcpy(newp, replace, replacesz);
		newp += replacesz;
		p = q + needlesz;
	}
	alpm_list_free(list);

	if(*p) {
		/* add the rest of 'p' */
		strcpy(newp, p);
	}

	return newstr;
}

/******************
 * ALPM CALLBACKS *
 ******************/

static int choice = CHOICE_FREE;

/* callback to handle messages/notifications from libalpm transactions */
static void
event_cb (alpm_event_t event, void *data1, void *data2)
{
    if (event == ALPM_EVENT_ADD_DONE)
    {
        alpm_logaction (handle, "kalu: installed %s (%s)\n",
            alpm_pkg_get_name (data1),
            alpm_pkg_get_version (data1));
        
        /* computing optional dependencies */
        GVariantBuilder *builder;
        alpm_list_t *i, *optdeps = alpm_pkg_get_optdepends (data1);
        
        builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        for (i = optdeps; i; i = alpm_list_next (i))
        {
            g_variant_builder_add (builder, "s", (const char *) i->data);
        }
        
        emit_signal ("EventInstalled", "ssas",
                     alpm_pkg_get_name (data1),
                     alpm_pkg_get_version (data1),
                     builder);
        g_variant_builder_unref (builder);
    }
    else if (event == ALPM_EVENT_REMOVE_DONE)
    {
        alpm_logaction (handle, "kalu: removed %s (%s)\n",
            alpm_pkg_get_name (data1),
            alpm_pkg_get_version (data1));
        emit_signal ("EventRemoved", "ss",
                     alpm_pkg_get_name (data1),
                     alpm_pkg_get_version (data1));
    }
    else if (event == ALPM_EVENT_UPGRADE_DONE)
    {
        alpm_logaction (handle, "kalu: upgraded %s (%s -> %s)\n",
            alpm_pkg_get_name (data1),
            alpm_pkg_get_version (data2),
            alpm_pkg_get_version (data1));
        
        /* computing new optional dependencies */
        GVariantBuilder *builder;
        alpm_list_t *old = alpm_pkg_get_optdepends (data2);
        alpm_list_t *new = alpm_pkg_get_optdepends (data1);
        alpm_list_t *i, *optdeps = alpm_list_diff (new, old, (alpm_list_fn_cmp) strcmp);
        
        builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        for (i = optdeps; i; i = alpm_list_next (i))
        {
            g_variant_builder_add (builder, "s", (const char *) i->data);
        }
        
        emit_signal ("EventUpgraded", "sssas",
                     alpm_pkg_get_name (data1),
                     alpm_pkg_get_version (data2),
                     alpm_pkg_get_version (data1),
                     builder);
        g_variant_builder_unref (builder);
        alpm_list_free (optdeps);
    }
    else if (event == ALPM_EVENT_RETRIEVE_START)
    {
        /* Retrieving packages (from repo) */
        emit_signal ("EventRetrievingPkgs", "s", (const char *) data1);
    }
    else if (event == ALPM_EVENT_CHECKDEPS_START)
    {
        /* checking dependencies */
        emit_signal ("Event", "i", EVENT_CHECKING_DEPS);
    }
    else if (event == ALPM_EVENT_RESOLVEDEPS_START)
    {
        /* resolving dependencies */
        emit_signal ("Event", "i", EVENT_RESOLVING_DEPS);
    }
    else if (event == ALPM_EVENT_INTERCONFLICTS_START)
    {
        /* looking for inter-conflicts */
        emit_signal ("Event", "i", EVENT_INTERCONFLICTS);
    }
    else if (event == ALPM_EVENT_SCRIPTLET_INFO)
    {
        emit_signal ("EventScriptlet", "s", data1);
    }
    else if (event == ALPM_EVENT_DELTA_INTEGRITY_START)
    {
        /* checking delta integrity */
        emit_signal ("Event", "i", EVENT_DELTA_INTEGRITY);
    }
    else if (event == ALPM_EVENT_DELTA_PATCHES_START)
    {
        /* applying deltas */
        emit_signal ("Event", "i", EVENT_DELTA_PATCHES);
    }
    else if (event == ALPM_EVENT_DELTA_PATCH_START)
    {
        emit_signal ("EventDeltaGenerating", "ss", data2, data1);
    }
    else if (event == ALPM_EVENT_DELTA_PATCH_DONE)
    {
        emit_signal ("Event", "i", EVENT_DELTA_PATCH_DONE);
    }
    else if (event == ALPM_EVENT_DELTA_PATCH_FAILED)
    {
        emit_signal ("Event", "i", EVENT_DELTA_PATCH_FAILED);
    }
}

/* callback to handle display of transaction progress */
static void
progress_cb (alpm_progress_t _event, const char *_pkgname, int percent,
             size_t _howmany, size_t _current)
{
    guint howmany = (guint) _howmany;
    guint current = (guint) _current;
    event_t event;
    switch (_event)
    {
        case ALPM_PROGRESS_ADD_START:
            event = EVENT_INSTALLING;
            break;
        case ALPM_PROGRESS_UPGRADE_START:
            event = EVENT_UPGRADING;
            break;
        case ALPM_PROGRESS_REMOVE_START:
            event = EVENT_REMOVING;
            break;
        case ALPM_PROGRESS_CONFLICTS_START:
            event = EVENT_FILE_CONFLICTS;
            break;
        case ALPM_PROGRESS_DISKSPACE_START:
            event = EVENT_CHECKING_DISKSPACE;
            break;
        case ALPM_PROGRESS_INTEGRITY_START:
            event = EVENT_PKG_INTEGRITY;
            break;
        case ALPM_PROGRESS_LOAD_START:
            event = EVENT_LOAD_PKGFILES;
            break;
        default:
            return;
    }
    const gchar *pkgname = (_pkgname) ? _pkgname : "";
    emit_signal ("Progress", "isiuu", event, pkgname, percent, howmany, current);
}

/* callback to handle receipt of total download value */
static void
dl_total_cb (off_t _total)
{
    guint total = (guint) _total;
    emit_signal ("TotalDownload", "u", total);
}

/* callback to handle display of download progress */
static void
dl_progress_cb (const char *filename, off_t _xfered, off_t _total)
{
    guint xfered = (guint) _xfered;
    guint total  = (guint) _total;
    emit_signal ("Downloading", "suu", filename, xfered, total);
}

/* callback to handle notifications from the library */
static void
log_cb (alpm_loglevel_t level, const char *fmt, va_list args)
{
    if (!fmt || *fmt == '\0')
    {
        return;
    }
    
    if (level & ALPM_LOG_DEBUG || level & ALPM_LOG_FUNCTION)
        return;
    
    gchar buf[1024];
    if (vsnprintf (buf, 1024, fmt, args) >= 1024)
    {
        buf[1023] = '\0';
    }
    emit_signal ("Log", "is", (gint) level, buf);
}

/* callback to handle questions from libalpm */
static void
question_cb (alpm_question_t event, void *data1, void *data2, void *data3, int *response)
{
    const char *repo1, *pkg1;
    const char *repo2, *pkg2;
    GVariantBuilder *builder;
    alpm_list_t *i;
    
    if (choice != CHOICE_FREE)
    {
        debug ("Received question (%d) while already busy", event);
        return;
    }
    choice = CHOICE_WAITING;
    
    if (event & ALPM_QUESTION_INSTALL_IGNOREPKG)
    {
        emit_signal ("AskInstallIgnorePkg", "s", alpm_pkg_get_name (data1));
    }
    else if (event & ALPM_QUESTION_REPLACE_PKG)
    {
        repo1 = alpm_db_get_name (alpm_pkg_get_db (data1));
        pkg1 = alpm_pkg_get_name (data1);
        repo2 = (const char *) data3;
        pkg2 = alpm_pkg_get_name (data2);
        emit_signal ("AskReplacePkg", "ssss", repo1, pkg1, repo2, pkg2);
    }
    else if (event & ALPM_QUESTION_CONFLICT_PKG)
    {
        /* this time no pointers to alpm_pkt_t, it's all strings */
        /* also, reason (data3) can be just same as data1 or data2...) */
        const char *reason;
        if (strcmp (data3, data1) == 0 || strcmp (data3, data2) == 0)
        {
            reason = "";
        }
        else
        {
            reason = (const char *) data3;
        }
        emit_signal ("AskConflictPkg", "sss", data1, data2, reason);
    }
    else if (event & ALPM_QUESTION_REMOVE_PKGS)
    {
        builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        for (i = (alpm_list_t *) data1; i; i = alpm_list_next (i))
        {
            g_variant_builder_add (builder, "s", alpm_pkg_get_name (i->data));
        }
        
        emit_signal ("AskRemovePkgs", "as", builder);
        g_variant_builder_unref (builder);
    }
    else if (event & ALPM_QUESTION_SELECT_PROVIDER)
    {
        /* creates a string like "foobar>=4.2" */
        char *pkg = alpm_dep_compute_string ((alpm_depend_t *) data2);
        GVariantBuilder *builder2;
        
        builder = g_variant_builder_new (G_VARIANT_TYPE ("aas"));
        for (i = (alpm_list_t *) data1; i; i = alpm_list_next (i))
        {
            builder2 = g_variant_builder_new (G_VARIANT_TYPE ("as"));
            /* repo */
            g_variant_builder_add (builder2, "s", alpm_db_get_name (alpm_pkg_get_db (i->data)));
            /* pkg */
            g_variant_builder_add (builder2, "s", alpm_pkg_get_name (i->data));
            /* version */
            g_variant_builder_add (builder2, "s", alpm_pkg_get_version (i->data));
            /* add it to main builder */
            g_variant_builder_add (builder, "as", builder2);
            g_variant_builder_unref (builder2);
        }
        
        emit_signal ("AskSelectProvider", "saas", pkg, builder);
        free (pkg);
        g_variant_builder_unref (builder);
    }
    else if (event & ALPM_QUESTION_LOCAL_NEWER)
    {
        pkg1 = alpm_pkg_get_name (data1);
        /* get pkg from local db */
        alpm_pkg_t *pkg = alpm_db_get_pkg (alpm_option_get_localdb (handle), pkg1);
        emit_signal ("AskLocalNewer", "ssss", 
                     /* pkg name */
                     pkg1,
                     /* local version */
                     alpm_pkg_get_version (pkg),
                     /* repo */
                     alpm_db_get_name (alpm_pkg_get_db (data1)),
                     /* repo version */
                     alpm_pkg_get_version (data1));
    }
    else if (event & ALPM_QUESTION_CORRUPTED_PKG)
    {
        emit_signal ("AskCorruptedPkg", "ss", data1,
                     alpm_strerror (*(enum _alpm_errno_t *)data2));
    }
    else if (event & ALPM_QUESTION_IMPORT_KEY)
    {
        alpm_pgpkey_t *key = data1;
        gchar created[12];
        strftime (created, 12, "%Y-%m-%d", localtime (&key->created));
        
        emit_signal ("AskImportKey", "sss", key->fingerprint, key->uid, created);
    }
    else
    {
        choice = CHOICE_FREE;
        debug ("Received unknown question-event: %d", event);
        return;
    }
    
    /* wait for a choice -- happens when method Answer is called */
    while (choice == CHOICE_WAITING)
    {
        g_main_context_iteration (NULL, TRUE);
    }
    *response = choice;
    choice = CHOICE_FREE;
}

/***********
 * METHODS *
 ***********/

#define method_failed(name, ...)    do {                        \
        snprintf (buffer, 1024, __VA_ARGS__);                   \
        emit_signal ("MethodFailed", "ss", name, buffer);       \
    } while (0)

#define method_finished(name)   emit_signal ("MethodFinished", "s", name)

/* methods must ALWAYS do the following :
 * - g_variant_unref (parameters) to free them
 * - either call method_failed() or emit_signal w/ their XxxxFinished signal
 * - return FALSE to remove the timeout
 */

static gboolean
init (GVariant *parameters)
{
    gchar *sender;
    g_variant_get (parameters, "(s)", &sender);
    g_variant_unref (parameters);
    
    /* already init */
    if (is_init)
    {
        free (sender);
        method_failed ("Init", "Session already initialized\n");
        return FALSE;
    }
    /* checking auth */
    GError *error = NULL;
    PolkitAuthority *authority;
    PolkitSubject *subject;
    PolkitAuthorizationResult *result;

    authority = polkit_authority_get_sync (NULL, NULL);
    subject = polkit_system_bus_name_new (sender);
    result = polkit_authority_check_authorization_sync (
                authority,
                subject, 
                "org.jjk.kalu.sysupgrade",
                NULL,
                POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                NULL,
                &error);
    if (result == NULL)
    {
        free (sender);
        method_failed ("Init", error->message);
        g_clear_error (&error);
        /* we have no reason to keep running */
        g_main_loop_quit (loop);
        return FALSE;
    }
    if (!polkit_authorization_result_get_is_authorized (result))
    {
        free (sender);
        g_object_unref (result);
        method_failed ("Init", "Authorization from PolicyKit failed\n");
        /* we have no reason to keep running */
        g_main_loop_quit (loop);
        return FALSE;
    }
    g_object_unref (result);
    
    /* ok, we're good */
    is_init = TRUE;
    client = sender; /* therefore, we shoudln't free sender */
    debug ("client is %s", client);
    method_finished ("Init");
    return FALSE;
}

static gboolean
init_alpm (GVariant *parameters)
{
    const gchar  *rootdir;
    const gchar  *dbpath;
    const gchar  *logfile;
    const gchar  *gpgdir;
    GVariantIter *cachedirs_iter;
    alpm_list_t  *cachedirs = NULL;
    int           siglevel;
    const gchar  *arch;
    gboolean      checkspace;
    gboolean      usesyslog;
    gboolean      usedelta;
    GVariantIter *ignorepkgs_iter;
    alpm_list_t  *ignorepkgs = NULL;
    GVariantIter *ignoregroups_iter;
    alpm_list_t  *ignoregroups = NULL;
    GVariantIter *noupgrades_iter;
    alpm_list_t  *noupgrades = NULL;
    GVariantIter *noextracts_iter;
    alpm_list_t  *noextracts = NULL;
    
    /* to extract arrays into alpm_list_t */
    const gchar *s;
    
    debug ("getting alpm params");
    g_variant_get (parameters, "(ssssasisbbbasasasas)",
        &rootdir,
        &dbpath,
        &logfile,
        &gpgdir,
        &cachedirs_iter,
        &siglevel,
        &arch,
        &checkspace,
        &usesyslog,
        &usedelta,
        &ignorepkgs_iter,
        &ignoregroups_iter,
        &noupgrades_iter,
        &noextracts_iter);
    g_variant_unref (parameters);
    
    debug ("init alpm");
    enum _alpm_errno_t err;
    int ret;
    
    /* init alpm */
    handle = alpm_initialize(rootdir, dbpath, &err);
    if (!handle)
    {
        method_failed ("InitAlpm", "Failed to initialize alpm library: %s\n",
                       alpm_strerror (err));
        return FALSE;
    }
    
    if (!(alpm_capabilities () & ALPM_CAPABILITY_DOWNLOADER))
    {
        method_failed ("InitAlpm", "ALPM has no downloader capability\n");
        return FALSE;
    }
    
    /* set callbacks, that we'll turn into signals */
    alpm_option_set_logcb (handle, log_cb);
    alpm_option_set_dlcb (handle, dl_progress_cb);
    alpm_option_set_eventcb (handle, event_cb);
    alpm_option_set_questioncb (handle, question_cb);
    alpm_option_set_progresscb (handle, progress_cb);
    alpm_option_set_totaldlcb (handle, dl_total_cb);
    
    ret = alpm_option_set_logfile (handle, logfile);
    if (ret != 0)
    {
        method_failed ("InitAlpm", "Unable to set log file: %s\n",
                       alpm_strerror (alpm_errno (handle)));
        return FALSE;
    }
    
    /* Set GnuPG's home directory.  This is not relative to rootdir, even if
     * rootdir is defined. Reasoning: gpgdir contains configuration data. */
    ret = alpm_option_set_gpgdir (handle, gpgdir);
    if (ret != 0)
    {
        method_failed ("InitAlpm", "Unable to set gpgdir: %s\n",
                       alpm_strerror (alpm_errno (handle)));
        return FALSE;
    }
    
    /* cachedirs */
    while (g_variant_iter_loop (cachedirs_iter, "s", &s))
    {
        cachedirs = alpm_list_add (cachedirs, strdup (s));
    }
    g_variant_iter_free (cachedirs_iter);
    
    if (0 != alpm_option_set_cachedirs (handle, cachedirs))
    {
        FREELIST (cachedirs);
        method_failed ("InitAlpm", "Unable to set cache dirs: %s\n",
                       alpm_strerror (alpm_errno (handle)));
        return FALSE;
    }
    FREELIST (cachedirs);
    
    if (0 != alpm_option_set_default_siglevel (handle, siglevel))
    {
        method_failed ("Unable to set default siglevel: %s\n",
                       alpm_strerror (alpm_errno (handle)));
        return FALSE;
    }
    
    /* following options can't really fail, unless handle is wrong but
     * that would have caused lots of failures before reacing here */
    alpm_option_set_arch (handle, arch);
    alpm_option_set_checkspace (handle, checkspace);
    alpm_option_set_usesyslog (handle, usesyslog);
    alpm_option_set_usedelta (handle, usedelta);
    
    while (g_variant_iter_loop (ignorepkgs_iter, "s", &s))
    {
        ignorepkgs = alpm_list_add (ignorepkgs, strdup (s));
    }
    g_variant_iter_free (ignorepkgs_iter);
    
    while (g_variant_iter_loop (ignoregroups_iter, "s", &s))
    {
        ignoregroups = alpm_list_add (ignoregroups, strdup (s));
    }
    g_variant_iter_free (ignoregroups_iter);
    
    while (g_variant_iter_loop (noupgrades_iter, "s", &s))
    {
        noupgrades = alpm_list_add (noupgrades, strdup (s));
    }
    g_variant_iter_free (noupgrades_iter);
    
    while (g_variant_iter_loop (noextracts_iter, "s", &s))
    {
        noextracts = alpm_list_add (noextracts, strdup (s));
    }
    g_variant_iter_free (noextracts_iter);
    
    alpm_option_set_ignorepkgs (handle, ignorepkgs);
    alpm_option_set_ignoregroups (handle, ignoregroups);
    alpm_option_set_noupgrades (handle, noupgrades);
    alpm_option_set_noextracts (handle, noextracts);
    
    FREELIST (ignorepkgs);
    FREELIST (ignoregroups);
    FREELIST (noupgrades);
    FREELIST (noextracts);
    
    /* done */
    method_finished ("InitAlpm");
    return FALSE;
}

static gboolean
free_alpm (GVariant *parameters)
{
    g_variant_unref (parameters);
    free (client);
    client = NULL;
    
    /* free alpm */
    if (handle && alpm_release (handle) == -1)
    {
        method_failed ("FreeAlpm", "Failed to release alpm library\n");
    }
    else
    {
        method_finished ("FreeAlpm");
    }
    handle = NULL;
    
    /* we have no reason to keep running at this point */
    g_main_loop_quit (loop);
    return FALSE;
}

static gboolean
add_db (GVariant *parameters)
{
    const gchar  *name;
    int           siglevel;
    GVariantIter *servers_iter;
    alpm_list_t  *servers = NULL;
    
    /* to extract arrays into alpm_list_t */
    const gchar *s;
    
    g_variant_get (parameters, "(sias)",
        &name,
        &siglevel,
        &servers_iter);
    g_variant_unref (parameters);
    
    alpm_db_t *db;
    db = alpm_db_register_sync (handle, name, (alpm_siglevel_t) siglevel);
    if (db == NULL)
    {
        method_failed ("AddDb", "Could not register database %s: %s\n",
                       name, alpm_strerror (alpm_errno (handle)));
        return FALSE;
    }
    
    while (g_variant_iter_loop (servers_iter, "s", &s))
    {
        servers = alpm_list_add (servers, strdup (s));
    }
    g_variant_iter_free (servers_iter);
    
    alpm_list_t *i;
    const char *arch = alpm_option_get_arch (handle);
    for (i = servers; i; i = alpm_list_next (i))
    {
        char *value = i->data;
        char *temp = strreplace (value, "$repo", name);
        char *server;
        
        if (arch)
        {
            server = strreplace (temp, "$arch", arch);
            free (temp);
        }
        else
        {
            if (strstr (temp, "$arch"))
            {
                free (temp);
                FREELIST (servers);
                method_failed ("AddDb",
                    "Server %s contains the $arch variable, but no Architecture was defined",
                    value);
                return FALSE;
            }
            server = temp;
        }
        
        debug ("add server %s into %s", server, name);
        if (alpm_db_add_server (db, server) != 0)
        {
            FREELIST (servers);
            free (server);
            /* pm_errno is set by alpm_db_setserver */
            method_failed ("AddDb", "Could not add server %s to database %s: %s\n",
                           server, name, alpm_strerror (alpm_errno (handle)));
            return FALSE;
        }
        free (server);
    }
    FREELIST (servers);
    
    /* ensure db is valid */
    if (alpm_db_get_valid (db))
    {
        method_failed ("AddDb", "Database %s is not valid: %s\n",
                       name, alpm_strerror (alpm_errno (handle)));
        return FALSE;
    }
    
    /* done */
    method_finished ("AddDb");
    return FALSE;
}

static gboolean
sync_dbs (GVariant *parameters)
{
    alpm_list_t        *syncdbs   = NULL;
    alpm_list_t        *i;
    int                 ret;
    sync_db_results_t   result;
    
    g_variant_unref (parameters);
    
    syncdbs = alpm_option_get_syncdbs (handle);
    emit_signal ("SyncDbs", "i", alpm_list_count (syncdbs));
    for (i = syncdbs; i; i = alpm_list_next (i))
    {
        alpm_db_t *db = i->data;
        emit_signal ("SyncDbStart", "s", alpm_db_get_name (db));
        ret = alpm_db_update (0, db);
        if (ret < 0)
        {
            result = SYNC_FAILURE;
            debug ("Sync db %s failed: %s", alpm_db_get_name (db),
                   alpm_strerror (alpm_errno (handle)));
        }
        else if (ret == 1)
        {
            result = SYNC_NOT_NEEDED;
        }
        else
        {
            result = SYNC_SUCCESS;
            alpm_logaction (handle, "kalu: synchronized database %s\n",
                            alpm_db_get_name (db));
        }
        emit_signal ("SyncDbEnd", "i", result);
    }
    
    method_finished ("SyncDbs");
    return FALSE;
}

static gboolean
answer (GVariant *parameters)
{
    int response;
    
    g_variant_get (parameters, "(i)", &response);
    g_variant_unref (parameters);
    
    if (choice != CHOICE_WAITING)
    {
        method_failed ("Answer", "Invalid call to Answer, no Question pending");
        return FALSE;
    }
    
    if (response >= 0)
    {
        choice = response;
    }
    else
    {
        debug ("Invalid answer, defaulting to no (0)");
        choice = 0;
    }
    method_finished ("Answer");
    return FALSE;
}

static gboolean
get_packages (GVariant *parameters)
{
    g_variant_unref (parameters);
    
    if (alpm_trans_init (handle, 0) == -1)
    {
        method_failed ("GetPackages", "Failed to initiate transaction: %s\n",
                       alpm_strerror (alpm_errno (handle)));
        return FALSE;
    }
    
    if (alpm_sync_sysupgrade (handle, 0) == -1)
    {
        method_failed ("GetPackages", "%s", alpm_strerror (alpm_errno (handle)));
        alpm_trans_release (handle);
        return FALSE;
    }
    
    alpm_list_t *alpm_data = NULL;
    if (alpm_trans_prepare (handle, &alpm_data) == -1)
    {
        alpm_list_t *i, *details = NULL;
        gchar buf[255], *errmsg;
        size_t len = 0;
        enum _alpm_errno_t err = alpm_errno (handle);
        if (err == ALPM_ERR_PKG_INVALID_ARCH)
        {
            for (i = alpm_data; i; i = alpm_list_next (i))
            {
                snprintf (buf, 255, "- Package %s does not have a valid architecture\n",
                    (const char *) i->data);
                details = alpm_list_add (details, strdup (buf));
                len += strlen (buf);
            }
        }
        else if (err == ALPM_ERR_UNSATISFIED_DEPS)
        {
            for (i = alpm_data; i; i = alpm_list_next (i))
            {
                alpm_depmissing_t *miss = i->data;
                char *depstring = alpm_dep_compute_string (miss->depend);
                snprintf (buf, 255, "- Package %s requires %s\n",
                    miss->target, depstring);
                free (depstring);
                details = alpm_list_add (details, strdup (buf));
                len += strlen (buf);
            }
        }
        else if (err == ALPM_ERR_CONFLICTING_DEPS)
        {
            for (i = alpm_data; i; i = alpm_list_next (i))
            {
                alpm_conflict_t *conflict = i->data;
                if (conflict->reason->mod == ALPM_DEP_MOD_ANY)
                {
                    snprintf (buf, 255, "- Packages %s and %s are in conflict\n",
                        conflict->package1, conflict->package2);
                }
                else
                {
                    char *reason = alpm_dep_compute_string (conflict->reason);
                    snprintf (buf, 255, "- Packages %s and %s are in conflict: %s\n",
                        conflict->package1, conflict->package2, reason);
                    free (reason);
                }
                details = alpm_list_add (details, strdup (buf));
                len += strlen (buf);
            }
        }
        
        if (len > 0)
        {
            errmsg = calloc (len + 1, sizeof (*errmsg));
            for (i = details; i; i = alpm_list_next (i))
            {
                strcat (errmsg, i->data);
                free (i->data);
            }
            alpm_list_free (details);
            details = NULL;
            method_failed ("GetPackages", "Failed to prepare transaction: %s :\n%s\n",
                           alpm_strerror (err), errmsg);
            free (errmsg);
        }
        else
        {
            method_failed ("GetPackages", "Failed to prepare transaction: %s\n",
                           alpm_strerror (err));
        }
        
        FREELIST (alpm_data);
        alpm_trans_release (handle);
        return FALSE;
    }
    FREELIST (alpm_data);
    
    alpm_list_t *pkgs;
    GVariantBuilder *builder;
    alpm_db_t *localdb = alpm_option_get_localdb (handle);
    alpm_list_t *i;
    
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a(sssuuu)"));
    
    pkgs = alpm_trans_get_add (handle);
    for (i = pkgs; i; i = alpm_list_next (i))
    {
        alpm_pkg_t *pkg      = i->data;
        const char *name     = alpm_pkg_get_name (pkg);
        alpm_pkg_t *localpkg = alpm_db_get_pkg (localdb, name);
        
        g_variant_builder_add (builder, "(sssuuu)",
            name,
            (localpkg) ? alpm_pkg_get_version (localpkg) : "-",
            alpm_pkg_get_version (pkg),
            (guint) alpm_pkg_download_size (pkg),
            (guint) alpm_pkg_get_isize (localpkg),
            (guint) alpm_pkg_get_isize (pkg));
    }
    
    pkgs = alpm_trans_get_remove (handle);
    for (i = pkgs; i; i = alpm_list_next (i))
    {
        alpm_pkg_t *pkg      = i->data;
        
        g_variant_builder_add (builder, "(sssuuu)",
            alpm_pkg_get_name (pkg),
            alpm_pkg_get_version (pkg),
            "-",
            (guint) 0,
            (guint) alpm_pkg_get_isize (pkg),
            (guint) 0);
    }
    
    emit_signal ("GetPackagesFinished", "a(sssuuu)", builder);
    g_variant_builder_unref (builder);
    /* we don't alpm_trans_release (handle) since that will be done only if
     * user cancel (NoSysUpgrade) or after the update is done (SysUpgrade) */
    return FALSE;
}

static gboolean
sysupgrade (GVariant *parameters)
{
    g_variant_unref (parameters);
    
    alpm_logaction (handle, "kalu: starting sysupgrade...\n");
    
    alpm_list_t *alpm_data = NULL;
    if (alpm_trans_commit (handle, &alpm_data) == -1)
    {
        alpm_list_t *i, *details = NULL;
        gchar buf[255], *errmsg;
        size_t len = 0;
        enum _alpm_errno_t err = alpm_errno (handle);
        if (err == ALPM_ERR_FILE_CONFLICTS)
        {
            for (i = alpm_data; i; i = alpm_list_next (i))
            {
                alpm_fileconflict_t *conflict = i->data;
                if (conflict->type == ALPM_FILECONFLICT_TARGET)
                {
                    snprintf (buf, 255, "- %s exists in both %s and %s\n",
                        conflict->file, conflict->target, conflict->ctarget);
                }
                else if (conflict->type == ALPM_FILECONFLICT_FILESYSTEM)
                {
                    snprintf (buf, 255, "- %s exists in both %s and current filesystem\n",
                        conflict->file, conflict->target);
                }
                else
                {
                    snprintf (buf, 255, "- Unknown conflict for %s\n",
                        conflict->target);
                }
                
                details = alpm_list_add (details, strdup (buf));
                len += strlen (buf);
            }
        }
        else if (  err == ALPM_ERR_PKG_INVALID
                || err == ALPM_ERR_PKG_INVALID_CHECKSUM
                || err == ALPM_ERR_PKG_INVALID_SIG
                || err == ALPM_ERR_DLT_INVALID)
        {
            for (i = alpm_data; i; i = alpm_list_next (i))
            {
                snprintf (buf, 255, "- %s in invalid or corrupted\n",
                    (const char *) i->data);
                details = alpm_list_add (details, strdup (buf));
                len += strlen (buf);
            }
        }
        
        if (len > 0)
        {
            errmsg = calloc (len + 1, sizeof (*errmsg));
            for (i = details; i; i = alpm_list_next (i))
            {
                strcat (errmsg, i->data);
                free (i->data);
            }
            alpm_list_free (details);
            details = NULL;
            method_failed ("SysUpgrade", "Failed to commit transaction: %s :\n%s\n",
                           alpm_strerror (err), errmsg);
            free (errmsg);
        }
        else
        {
            method_failed ("SysUpgrade", "Failed to commit transaction: %s\n",
                           alpm_strerror (err));
        }
        
        FREELIST (alpm_data);
        alpm_logaction (handle, "kalu: Failed to commit sysupgrade transaction: %s\n",
                        alpm_strerror (err));
        alpm_trans_release (handle);
        return FALSE;
    }
    
    FREELIST (alpm_data);
    alpm_trans_release (handle);
    alpm_logaction (handle, "kalu: sysupgrade completed\n");
    method_finished ("SysUpgrade");
    return FALSE;
}

static gboolean
no_sysupgrade (GVariant *parameters)
{
    g_variant_unref (parameters);
    alpm_trans_release (handle);
    method_finished ("NoSysUpgrade");
    return FALSE;
}

#undef method_finished
#undef method_failed

/******************
 * DBUS INTERFACE *
 ******************/

#define send_error(error, ...)  do {                    \
    snprintf (buffer, 255, __VA_ARGS__);                \
    g_dbus_method_invocation_return_dbus_error (        \
        invocation,                                     \
        "org.jjk.kalu." error,                          \
        buffer);                                        \
    } while (0)
#define if_method(name, func)   do  {                                   \
        if (g_strcmp0 (method_name, name) == 0)                         \
        {                                                               \
            g_variant_ref (parameters);                                 \
            g_timeout_add (1, (GSourceFunc) func, parameters);          \
            g_dbus_method_invocation_return_value (invocation, NULL);   \
            return;                                                     \
        }                                                               \
    } while (0)
static void
handle_method_call (GDBusConnection       *conn _UNUSED_,
                    const gchar           *sender,
                    const gchar           *object_path _UNUSED_,
                    const gchar           *interface_name _UNUSED_,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               data _UNUSED_)
{
    debug ("sender is %s -- client is %s", sender, client);
    /* Init: check auth from PolicyKit, and "lock" to client/sender */
    if (g_strcmp0 (method_name, "Init") == 0)
    {
        /* we need to send the sender to init, hence the following */
        GVariant *value = g_variant_new ("(s)", sender);
        parameters = value;
        g_variant_ref (parameters);
        g_timeout_add (1, (GSourceFunc) init, parameters);
        g_variant_unref (value);
        g_dbus_method_invocation_return_value (invocation, NULL);
        return;
    }
    /* at this point, we must be init.. */
    if (!is_init)
    {
        send_error ("NoInitError", "Session not initialized");
        return;
    }
    /* ..and only accept from client */
    if (g_strcmp0 (sender, client) != 0)
    {
        send_error ("InvalidInitError", "Session initialized for another client");
        return;
    }
    
    /* client/sender has been auth (PK) */
    
    if_method ("InitAlpm",      init_alpm);
    if_method ("FreeAlpm",      free_alpm);
    if_method ("AddDb",         add_db);
    if_method ("SyncDbs",       sync_dbs);
    if_method ("Answer",        answer);
    if_method ("GetPackages",   get_packages);
    if_method ("SysUpgrade",    sysupgrade);
    if_method ("NoSysUpgrade",  no_sysupgrade);
    
    send_error ("UnknownMethod", "Unknown method: %s\n", method_name);
}
#undef if_method
#undef send_error

static void
on_bus_acquired (GDBusConnection *conn,
                 const gchar     *name _UNUSED_,
                 gpointer         user_data _UNUSED_)
{
    guint registration_id;
    GDBusInterfaceVTable interface_vtable;
    memset (&interface_vtable, 0, sizeof (GDBusInterfaceVTable));
    interface_vtable.method_call  = handle_method_call;
    
    connection = conn;
    registration_id = g_dbus_connection_register_object (
                        connection,
                        OBJECT_PATH,
                        introspection_data->interfaces[0],
                        &interface_vtable,
                        NULL,
                        NULL,
                        NULL);
    g_assert (registration_id > 0);
}

static void
on_name_acquired (GDBusConnection *conn _UNUSED_,
                  const gchar     *name _UNUSED_,
                  gpointer         user_data _UNUSED_)
{
    /* void */
}

static void
on_name_lost (GDBusConnection *conn _UNUSED_,
              const gchar     *name _UNUSED_,
              gpointer         user_data _UNUSED_)
{
  g_main_loop_quit (loop);
}


int
main (int argc _UNUSED_, char *argv[] _UNUSED_)
{
  guint owner_id;

  g_type_init ();

  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
                             "org.jjk.kalu",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);
  g_dbus_node_info_unref (introspection_data);
  return 0;
}
