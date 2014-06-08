DISTCLEANFILES = \
	src/kalu-dbus/updater-dbus.h \
	doc/kalu.1 \
	doc/index.html

CLEANFILES = \
	kalu.png \
	kalu-logo \
	misc/org.jjk.kalu.service \
	misc/30-kalu.rules

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = po

DEFS = \
	-DLOCALEDIR=\"@localedir@\" \
	@DEFS@

if USE_GIT_VERSION
_VERSION = `git describe --abbrev=4 --dirty`
DEFS += -DGIT_VERSION=\"$(_VERSION)\"
else
_VERSION = $(PACKAGE_VERSION)
endif

bin_PROGRAMS = kalu
if ! DISABLE_UPDATER
bin_PROGRAMS += kalu-dbus
dist_bin_SCRIPTS = src/kalu-dbus/kalu-dbus-launcher
endif

AM_CFLAGS = \
	-g \
	-DDOCDIR='"$(docdir)"' \
	-D_BSD_SOURCE \
	${WARNING_CFLAGS}

noinst_LTLIBRARIES = libshared.la
libshared_la_SOURCES = \
	src/kalu/shared.h \
	src/kalu/shared.c

kalu_CFLAGS = ${AM_CFLAGS}
kalu_LDADD = libshared.la -lalpm -lm @LIBCURL_LIBS@
kalu_SOURCES = \
	src/kalu/main.c \
	src/kalu/kalu.h \
	src/kalu/conf.h \
	src/kalu/conf.c \
	src/kalu/util.h \
	src/kalu/util.c \
	src/kalu/kalu-alpm.h \
	src/kalu/kalu-alpm.c \
	src/kalu/curl.h \
	src/kalu/curl.c \
	src/kalu/cJSON.h \
	src/kalu/cJSON.c \
	src/kalu/aur.h \
	src/kalu/aur.c \
	src/kalu/news.h \
	src/kalu/news.c \
	src/kalu/rt_timeout.h \
	src/kalu/rt_timeout.c

if ! DISABLE_GUI
kalu_CFLAGS += @GTK_CFLAGS@ @NOTIFY_CFLAGS@
kalu_LDADD += \
	@GTK_LIBS@ \
	@NOTIFY_LIBS@ \
	kalu-logo.o
kalu_SOURCES += \
	src/kalu/imagemenuitem.h \
	src/kalu/imagemenuitem.c \
	src/kalu/gui.h \
	src/kalu/gui.c \
	src/kalu/util-gtk.h \
	src/kalu/util-gtk.c \
	src/kalu/watched.h \
	src/kalu/watched.c \
	src/kalu/preferences.h \
	src/kalu/preferences.c
else
kalu_CFLAGS += @GLIB2_CFLAGS@
kalu_LDADD += @GLIB2_LIBS@
endif

if ENABLE_STATUS_NOTIFIER
kalu_CFLAGS += @STATUS_NOTIFIER_CFLAGS@
kalu_LDADD += @STATUS_NOTIFIER_LIBS@
endif

if ! DISABLE_UPDATER
BUILT_SOURCES = \
	src/kalu-dbus/updater-dbus.h

kalu_SOURCES += \
	src/kalu-dbus/updater-dbus.h \
	src/kalu-dbus/kupdater.h \
	src/kalu/closures.h \
	src/kalu/closures.c \
	src/kalu/kalu-updater.h \
	src/kalu/kalu-updater.c \
	src/kalu/updater.h \
	src/kalu/updater.c

kalu_dbus_CFLAGS = ${AM_CFLAGS} @GTK_CFLAGS@ @POLKIT_CFLAGS@
kalu_dbus_LDADD = libshared.la -lalpm @GTK_LIBS@ @POLKIT_LIBS@
kalu_dbus_SOURCES = \
	src/kalu-dbus/updater-dbus.h \
	src/kalu-dbus/kupdater.h \
	src/kalu-dbus/kalu-dbus.c
endif

if ! DISABLE_GUI
logodir = $(datadir)/pixmaps
logo_DATA = kalu.png

hicolordir = $(datadir)/icons/hicolor/48x48/apps
hicolor_DATA = kalu.png

desktopdir = /usr/share/applications
dist_desktop_DATA = misc/kalu.desktop
endif

if ! DISABLE_UPDATER
policydir = /usr/share/polkit-1/actions
dist_policy_DATA = misc/org.jjk.kalu.policy
rulesdir = /usr/share/polkit-1/rules.d
dist_rules_DATA = misc/30-kalu.rules
dbusservicedir = /usr/share/dbus-1/system-services
nodist_dbusservice_DATA = misc/org.jjk.kalu.service
dbusconfdir = /etc/dbus-1/system.d
dist_dbusconf_DATA = misc/org.jjk.kalu.conf
endif

dist_man_MANS = doc/kalu.1
dist_doc_DATA = doc/index.html \
				HISTORY \
				AUTHORS \
				COPYING \
				LICENSE \
				README.md

EXTRA_DIST = \
	src/kalu-dbus/gen-interface \
	src/kalu-dbus/updater-dbus.xml \
	doc/kalu.pod \
	misc/org.jjk.kalu.service.tpl \
	misc/30-kalu.rules.tpl \
	misc/arch_linux_48x48_icon_by_painlessrob.png

src/kalu-dbus/updater-dbus.h: src/kalu-dbus/updater-dbus.xml
	$(AM_V_GEN)cd src/kalu-dbus && ./gen-interface > updater-dbus.h

kalu-logo.o: kalu-logo
	$(AM_V_GEN)$(LD) -r -b binary -o kalu-logo.o kalu-logo

kalu-logo: kalu.png
	$(AM_V_at)$(LN_S) kalu.png kalu-logo

doc/kalu.1: doc/kalu.pod
	$(AM_V_GEN)pod2man --center='Keeping Arch Linux Up-to-date' --section=1 \
		--release=$(_VERSION) doc/kalu.pod doc/kalu.1

doc/index.html: doc/kalu.1
	$(AM_V_GEN)groff -T html -man doc/kalu.1 > doc/index.html

misc/org.jjk.kalu.service: misc/org.jjk.kalu.service.tpl
	$(AM_V_GEN)cd misc && sed 's|@BINDIR@|$(bindir)|' org.jjk.kalu.service.tpl \
		> org.jjk.kalu.service

misc/30-kalu.rules: misc/30-kalu.rules.tpl
	$(AM_V_GEN)cd misc && sed 's|@GROUP@|$(SYSUPGRADE_GROUP)|' 30-kalu.rules.tpl \
		> 30-kalu.rules

kalu.png: misc/arch_linux_48x48_icon_by_painlessrob.png
	$(AM_V_GEN)$(LN_S) misc/arch_linux_48x48_icon_by_painlessrob.png kalu.png

install-data-hook:
	mkdir "$(DESTDIR)$(docdir)/html"
	mv "$(DESTDIR)$(docdir)/index.html" "$(DESTDIR)$(docdir)/html/"
