#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([kalu], [1.4.0], [i.am.jack.mail@gmail.com])
AC_CONFIG_SRCDIR([src/kalu/kalu.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AC_SYS_LARGEFILE

AM_INIT_AUTOMAKE([-Wall -Werror foreign silent-rules])
AM_SILENT_RULES([yes])

# For automake >= 1.12
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_CC_C_O

LT_INIT

AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION([0.18.1])

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
		[disable kalu GUI (make a CLI-only binary))]),
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
            [disable kalu updater (GTK GUI for system upgrade)]),
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

# Enable extra compiler warning flags
AC_ARG_ENABLE([warning-flags],
	AS_HELP_STRING([--enable-warning-flags], [enable extra compiler warning flags]),
	[warningflags=$enableval], [warningflags=no])

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
            AC_MSG_ERROR([PolicyKit is required (for kalu updater)]))
    else
        AC_DEFINE([DISABLE_UPDATER], 1, [Disable kalu udpater])
    fi

    # Checks for GTK+3
    PKG_CHECK_MODULES(GTK, [gtk+-3.0], , AC_MSG_ERROR([GTK+3 is required]))
], [
    AC_DEFINE([DISABLE_GUI], 1, [Disable GUI])
    AC_DEFINE([DISABLE_UPDATER], 1, [Disable kalu udpater])
    # No GTK, check for Glib
    PKG_CHECK_MODULES(GLIB2, [glib-2.0 gobject-2.0 gthread-2.0], ,
                      AC_MSG_ERROR([glib2 is required]))
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
AC_DEFINE_UNQUOTED([KALU_LOGO],
                   ["$(eval echo $(eval echo ${datadir}))/pixmaps/kalu.png"],
                   [Path/to/kalu.png])
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

# warning flags
WARNING_CFLAGS="-Wall"
AC_MSG_CHECKING([for extra compiler warning flags])
if test "x$warningflags" = "xyes"; then
    AC_MSG_RESULT([yes])
    CFLAGS_ADD([-Wextra], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wshadow], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wpointer-arith], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wcast-align], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wwrite-strings], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wmissing-prototypes], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wmissing-declarations], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wredundant-decls], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wnested-externs], [WARNING_CFLAGS])
    CFLAGS_ADD([-Winline], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wno-long-long], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wuninitialized], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wconversion], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wstrict-prototypes], [WARNING_CFLAGS])

    CFLAGS_ADD([-Wclobbered], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wempty-body], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wfloat-equal], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wformat-nonliteral], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wformat-security], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wignored-qualifiers], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wlogical-op], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wmissing-field-initializers], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wmissing-parameter-type], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wold-style-declaration], [WARNING_CFLAGS])
    CFLAGS_ADD([-Woverride-init], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wsign-compare], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wstrict-aliasing], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wstrict-overflow=5], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wtype-limits], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wunused-but-set-parameter], [WARNING_CFLAGS])
    CFLAGS_ADD([-Wunused-parameter], [WARNING_CFLAGS])
else
    AC_MSG_RESULT([no])
fi

AC_CONFIG_FILES([Makefile po/Makefile.in])
AC_OUTPUT

echo "
    ${PACKAGE} version ${PACKAGE_VERSION}${gitver}

 Build information:
   source code location     : ${srcdir}
   prefix                   : ${prefix}
   GUI                      : ${with_gui}
   kalu's updater           : ${with_updater}
   compiler warning flags   : ${WARNING_CFLAGS}

   Arch Linux News RSS URL  : ${NEWS_RSS_URL}
   AUR URL prefix           : ${AUR_URL_PREFIX}
   AUR URL package prefix   : ${AUR_URL_PREFIX_PKG}

 Install paths:
   binaries                 : $(eval echo $(eval echo ${bindir}))
   documentation            : $(eval echo $(eval echo ${docdir}))
   man pages                : $(eval echo $(eval echo ${mandir}))
"
