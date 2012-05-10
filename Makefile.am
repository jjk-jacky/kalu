
CLEANFILES = index.html kalu.png org.jjk.kalu.service

bin_PROGRAMS = kalu
if ! DISABLE_UPDATER
bin_PROGRAMS += kalu-dbus
dist_bin_SCRIPTS = kalu-dbus-launcher
endif

nodist_man_MANS = kalu.1
dist_doc_DATA = AUTHORS COPYING HISTORY LICENSE README.md
doc_DATA = index.html

logodir = $(datadir)/pixmaps
logo_DATA = kalu.png

desktopdir = /usr/share/applications
dist_desktop_DATA = kalu.desktop

if ! DISABLE_UPDATER
policydir = /usr/share/polkit-1/actions
dist_policy_DATA = org.jjk.kalu.policy
dbusservicedir = /usr/share/dbus-1/system-services
nodist_dbusservice_DATA = org.jjk.kalu.service
dbusconfdir = /etc/dbus-1/system.d
dist_dbusconf_DATA = org.jjk.kalu.conf
endif

install-data-hook:
	mkdir $(DESTDIR)$(docdir)/html
	mv $(DESTDIR)$(docdir)/index.html $(DESTDIR)$(docdir)/html/

dist-hook:
	cp $(srcdir)/kalu.pod $(distdir)/
	cp $(srcdir)/org.jjk.kalu.service.tpl $(distdir)/
	cp $(srcdir)/arch_linux_48x48_icon_by_painlessrob.png $(distdir)/

AM_CPPFLAGS = -DDOCDIR='"$(docdir)"'
AM_CFLAGS = -g \
		-Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
		-Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
		-Wuninitialized -Wconversion -Wstrict-prototypes
AM_CFLAGS += -D_BSD_SOURCE

kalu_CFLAGS = ${AM_CFLAGS} @GTK_CFLAGS@ @NOTIFY_CFLAGS@
kalu_LDADD = @GTK_LIBS@ @NOTIFY_LIBS@ -lalpm -lm @LIBCURL@
kalu_SOURCES = main.c arch_linux.h kalu.h \
		conf.h conf.c \
		util.h util.c util-gtk.h util-gtk.c \
		kalu-alpm.h kalu-alpm.c \
		watched.h watched.c  \
		curl.h curl.c \
		cJSON.h cJSON.c \
		aur.h aur.c \
		news.h news.c \
		preferences.h preferences.c
if ! DISABLE_UPDATER
kalu_SOURCES += closures.h closures.c \
		updater-dbus.h kupdater.h \
		kalu-updater.h kalu-updater.c \
		updater.h updater.c


kalu_dbus_CFLAGS = ${AM_CFLAGS} @GTK_CFLAGS@ @POLKIT_CFLAGS@
kalu_dbus_LDADD = @GTK_LIBS@ @POLKIT_LIBS@ -lalpm
kalu_dbus_SOURCES = updater-dbus.h kupdater.h kalu-dbus.c
endif

org.jjk.kalu.service: org.jjk.kalu.service.tpl
	sed 's|@BINDIR@|$(bindir)|' org.jjk.kalu.service.tpl > org.jjk.kalu.service

kalu.1: kalu.pod
	pod2man --center='Keeping Arch Linux Up-to-date' --section=1 --release=$(PACKAGE_VERSION) kalu.pod kalu.1

index.html: kalu.1
	groff -T html -man kalu.1 > index.html

kalu.png: arch_linux_48x48_icon_by_painlessrob.png
	$(LN_S) arch_linux_48x48_icon_by_painlessrob.png kalu.png

