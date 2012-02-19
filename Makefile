
.PHONY = all kalu kalu-dbus doc install uninstall clean

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
			-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
			-Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
			-Wuninitialized -Wconversion -Wstrict-prototypes
CFLAGS := -g -std=c99 $(WARNINGS) -imacros configure.h

PROGRAMS = kalu kalu-dbus
DOCS = kalu.1.gz

SRCFILES =	main.c alpm.c config.c util.c watched.c util-gtk.c kalu-updater.c \
			updater.c closures.c cJSON.c aur.c curl.c news.c preferences.c

HDRFILES =	alpm.h config.h util.h kalu.h watched.h util-gtk.h kalu-updater.h \
			updater.h closures.h updater-dbus.h kupdater.h cJSON.h aur.h \
			curl.h news.h arch_linux.h preferences.h

OBJFILES =	main.o alpm.o config.o util.o watched.o util-gtk.o kalu-updater.o \
			updater.o closures.o cJSON.o aur.o curl.o news.o preferences.o

DBUSSRCFILES = kalu-dbus.c
DBUSOBJFILES = kalu-dbus.o

MANFILES = kalu.1

#all: $(PROGRAMS) $(DOCS)
all: $(PROGRAMS)

kalu: $(OBJFILES)
	$(CC) -o kalu $(OBJFILES) `pkg-config --libs gtk+-3.0 libnotify` `curl-config --libs` -lalpm -lm

main.o:	main.c arch_linux.h alpm.h config.h util.h kalu.h updater.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags gtk+-3.0 libnotify` -lalpm main.c

alpm.o: alpm.c alpm.h config.h util.h kalu.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0` alpm.c

config.o: config.c alpm.h config.h util.h kalu.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0` config.c

util.o: util.c alpm.h util.h kalu.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0` util.c

watched.o: watched.c alpm.h watched.h kalu.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags gtk+-3.0` watched.c

util-gtk.o: util-gtk.c kalu.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags gtk+-3.0 libnotify` util-gtk.c

kalu-updater.o:	kalu-updater.c kalu-updater.h kalu.h updater-dbus.h kupdater.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0` kalu-updater.c

updater.o:	updater.c updater.h kalu.h kupdater.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags gtk+-3.0` updater.c

closures.o:	closures.c closures.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0` closures.c

cJSON.o:	cJSON.c cJSON.h
	$(CC) -c $(CFLAGS) cJSON.c

aur.o:	aur.c aur.h kalu.h curl.h cJSON.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0` aur.c

curl.o:	curl.c curl.h kalu.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0` `curl-config --cflags` curl.c

news.o:	news.c news.h kalu.h curl.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags gtk+-3.0` news.c

preferences.o:	preferences.c preferences.h kalu.h util.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags gtk+-3.0` preferences.c

kalu-dbus: $(DBUSOBJFILES)
	$(CC) -o kalu-dbus $(DBUSOBJFILES) `pkg-config --libs glib-2.0 polkit-gobject-1` -lalpm

kalu-dbus.o: kalu-dbus.c updater-dbus.h kupdater.h
	$(CC) -c $(CFLAGS) `pkg-config --cflags glib-2.0 polkit-gobject-1` kalu-dbus.c

doc: $(DOCS)

kalu.1.gz: $(MANFILES)
	gzip -c kalu.1 > kalu.1.gz

index.html:
	groff -T html -man kalu.1 > index.html

install:
	install -D -m755 kalu $(DESTDIR)/usr/bin/kalu
	install -D -m755 kalu-dbus $(DESTDIR)/usr/bin/kalu-dbus
#	install -D -m644 kalu.1.gz $(DESTDIR)/usr/share/man/man1/kalu.1.gz
#	install -D -m644 index.html $(DESTDIR)usr/share/doc/kalu/html/index.html
	install -D -m644 arch_linux_48x48_icon_by_painlessrob.png $(DESTDIR)usr/share/pixmaps/kalu.png
	install -D -m644 org.jjk.kalu.policy $(DESTDIR)usr/share/polkit-1/actions/org.jjk.kalu.policy
	install -D -m644 org.jjk.kalu.service $(DESTDIR)usr/share/dbus-1/system-services/org.jjk.kalu.service
	install -D -m644 org.jjk.kalu.conf $(DESTDIR)etc/dbus-1/system.d/org.jjk.kalu.conf
	install -D -m644 kalu.desktop $(DESTDIR)usr/share/applications/kalu.desktop

uninstall:
	rm -f $(DESTDIR)/usr/bin/kalu
	rm -f $(DESTDIR)/usr/bin/kalu-dbus
	rm -f $(DESTDIR)/usr/share/man/man1/kalu.1.gz
	rm -rf $(DESTDIR)usr/share/doc/kalu
	rm -f $(DESTDIR)usr/share/pixmaps/kalu.png
	rm -f $(DESTDIR)usr/share/polkit-1/actions/org.jjk.kalu.policy
	rm -f $(DESTDIR)usr/share/dbus-1/system-services/org.jjk.kalu.service
	rm -f $(DESTDIR)etc/dbus-1/system.d/org.jjk.kalu.conf
	rm -f $(DESTDIR)usr/share/applications/kalu.desktop

clean:
	rm -f $(PROGRAMS)
	rm -f $(OBJFILES)
	rm -f $(DOCS)
