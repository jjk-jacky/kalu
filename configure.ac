#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([kalu], [1.3.0], [i.am.jack.mail@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([kalu.h])
AC_CONFIG_HEADERS([config.h])

AC_SYS_LARGEFILE

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_CC_C_O

# Option for News URL
AC_ARG_WITH([news-rss-url],
        AC_HELP_STRING([--with-news-rss-url=URL], [set the RSS URL for Arch Linux News]),
        [NEWS_RSS_URL=$withval], [NEWS_RSS_URL="https://www.archlinux.org/feeds/news/"])

# Options for AUR URL
AC_ARG_WITH([url-aur-prefix],
        AC_HELP_STRING([--with-url-aur-prefix=URL], [set the prefix for the AUR URL]),
        [AUR_URL_PREFIX=$withval], [AUR_URL_PREFIX="https://aur.archlinux.org/rpc.php?type=multiinfo"])
AC_ARG_WITH([url-aur-prefix-pkg],
	AC_HELP_STRING([--with-url-aur-prefix-pkg=PREFIX],
		[set the prefix before each package for the AUR URL]),
	[AUR_URL_PREFIX_PKG=$withval], [AUR_URL_PREFIX_PKG="&arg[[]]="])

# Feature: GUI
AC_ARG_ENABLE([gui],
	AC_HELP_STRING([--disable-gui],
		[disable kalu's GUI (make a CLI-only binary))]),
	if test $enableval = "yes"; then
		with_gui=yes
	elif test $enableval = "no"; then
		with_gui=no
	else
		AC_MSG_ERROR([Invalid value given to --enable-gui; must be yes or no])
	fi
	,
	with_gui=yes)
AM_CONDITIONAL([DISABLE_GUI], [test "x$with_gui" = "xno"])

# Feature: updater (unless GUI was disabled)
if test "x$with_gui" = "xyes"; then
    AC_ARG_ENABLE([updater],
        AC_HELP_STRING([--disable-updater],
            [disable kalu's updater (GTK GUI for system upgrade)]),
        if test $enableval = "yes"; then
            with_updater=yes
        elif test $enableval = "no"; then
            with_updater=no
        else
            AC_MSG_ERROR([Invalid value given to --enable-updater; must be yes or no])
        fi
        ,
        with_updater=yes)
else
    with_updater=no
fi
AM_CONDITIONAL([DISABLE_UPDATER], [test "x$with_updater" = "xno"])

# Option to use git version
AC_ARG_ENABLE([git-version],
	AS_HELP_STRING([--enable-git-version], [enable the use of git version]),
	[wantgitver=$enableval], [wantgitver=no])

# Checks for libraries.
AC_CHECK_LIB([alpm], [alpm_db_get_pkg], ,
	AC_MSG_ERROR([libalpm is required]))
AC_CHECK_LIB([m], [fabs], ,
	AC_MSG_ERROR([libm is required]))
AS_IF([test "x$with_gui" = "xyes"], [
    PKG_CHECK_MODULES(NOTIFY, [libnotify], ,
        AC_MSG_ERROR([libnotify is required]))
    if test "x$with_updater" = "xyes"; then
        PKG_CHECK_MODULES(POLKIT, [polkit-gobject-1], ,
            AC_MSG_ERROR([PolicyKit is required (for kalu's updater)]))
    else
        AC_DEFINE([DISABLE_UPDATER], 1, [Disable kalu's udpater])
    fi
    
    # Checks for GTK+3
    PKG_CHECK_MODULES(GTK, [gtk+-3.0], , AC_MSG_ERROR([GTK+3 is required]))
], [
    AC_DEFINE([DISABLE_GUI], 1, [Disable GUI])
    AC_DEFINE([DISABLE_UPDATER], 1, [Disable kalu's udpater])
    # No GTK, check for Glib
    PKG_CHECK_MODULES(GLIB2, [glib-2.0 gobject-2.0 gthread-2.0], , AC_MSG_ERROR([glib2 is required]))
])

# Check for libcurl
LIBCURL_CHECK_CONFIG([yes], , , AC_MSG_ERROR([libcurl is required]))

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h limits.h stdlib.h string.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([floor memmove memset mkdir pow rmdir strchr strdup strerror strrchr strstr uname utime])

# Defines some constants
AC_DEFINE_UNQUOTED([NEWS_RSS_URL], ["$NEWS_RSS_URL"],
        [News RSS URL])
AC_DEFINE_UNQUOTED([AUR_URL_PREFIX], ["$AUR_URL_PREFIX"],
	[Prefix to construct URL for AUR])
AC_DEFINE_UNQUOTED([AUR_URL_PREFIX_PKG], ["$AUR_URL_PREFIX_PKG"],
	[Prefix before each package to construct AUR URL])

# git version
AC_MSG_CHECKING([if git version must be used])
if test "x$wantgitver" = "xyes"; then
    AC_MSG_RESULT([yes])
    AC_CHECK_PROGS([GIT], [git])
    if test "x$GIT" = "x"; then
        AC_MSG_ERROR([Cannot use git version: git not found])
    fi
    AC_CHECK_FILE([.git/], hasgitdir=yes)
    if test "x$hasgitdir" = "xyes"; then
        usegitver=yes
        gitver=-git
        AC_DEFINE([USE_GIT_VERSION], , [Use GIT version])
    else
        AC_MSG_ERROR([Cannot use git version: .git not found])
    fi
else
    AC_MSG_RESULT([no])
    usegitver=no
    gitver=
fi
AM_CONDITIONAL(USE_GIT_VERSION, test "x$usegitver" = "xyes")

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "
	${PACKAGE} version ${PACKAGE_VERSION}${gitver}

 Build information:
   source code location		: ${srcdir}
   prefix			: ${prefix}
   GUI				: ${with_gui}
   kalu's updater		: ${with_updater}

   Arch Linux News RSS URL	: ${NEWS_RSS_URL}
   AUR URL prefix		: ${AUR_URL_PREFIX}
   AUR URL package prefix	: ${AUR_URL_PREFIX_PKG}

 Install paths:
   binaries			: $(eval echo $(eval echo ${bindir}))
   documentation		: $(eval echo $(eval echo ${docdir}))
   man pages			: $(eval echo $(eval echo ${mandir}))
"
