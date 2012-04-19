#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([kalu], [0.1.5.1], [i.am.jack.mail@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([kalu.h])
AC_CONFIG_HEADERS([config.h])

AC_SYS_LARGEFILE

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S

# Option for News URL
AC_ARG_WITH([news-rss-url],
        AC_HELP_STRING([--with-news-rss-url=URL], [set the RSS URL for Arch Linux News]),
        [NEWS_RSS_URL=$withval], [NEWS_RSS_URL="http://www.archlinux.org/feeds/news/"])

# Options for AUR URL
AC_ARG_WITH([url-aur-prefix],
        AC_HELP_STRING([--with-url-aur-prefix=URL], [set the prefix for the AUR URL]),
        [AUR_URL_PREFIX=$withval], [AUR_URL_PREFIX="http://aur.archlinux.org/rpc.php?type=multiinfo"])
AC_ARG_WITH([url-aur-prefix-pkg],
	AC_HELP_STRING([--with-url-aur-prefix-pkg=PREFIX],
		[set the prefix before each package for the AUR URL]),
	[AUR_URL_PREFIX_PKG=$withval], [AUR_URL_PREFIX_PKG="&arg[[]]="])

# Feature: updater
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
AM_CONDITIONAL([DISABLE_UPDATER], [test "x$with_updater" = "xno"])

# Checks for libraries.
AC_CHECK_LIB([alpm], [alpm_db_get_pkg], ,
	AC_MSG_ERROR([libalpm is required]))
AC_CHECK_LIB([m], [fabs], ,
	AC_MSG_ERROR([libm is required]))
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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "
	${PACKAGE} version ${PACKAGE_VERSION}

 Build information:
   source code location		: ${srcdir}
   prefix			: ${prefix}
   kalu's updater		: ${with_updater}

   Arch Linux News RSS URL	: ${NEWS_RSS_URL}
   AUR URL prefix		: ${AUR_URL_PREFIX}
   AUR URL package prefix	: ${AUR_URL_PREFIX_PKG}

 Install paths:
   binaries			: $(eval echo $(eval echo ${bindir}))
   documentation		: $(eval echo $(eval echo ${docdir}))
   man pages			: $(eval echo $(eval echo ${mandir}))

"

