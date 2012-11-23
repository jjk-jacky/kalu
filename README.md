# kalu: Keeping Arch Linux Up-to-date

kalu (which could stand for "Keeping Arch Linux Up-to-date") is a small application that will add an icon to your systray and sit there, regularly checking if there's anything new for you to upgrade. As soon as it finds something, it'll show a notification to let you know about it. Very classic stuff.

## What makes kalu any different?

For starter, **it doesn't need root privileges** to do its checking. Not because it doesn't synchronize databases, since that would make it mostly useless, but because it will create a temporary copy of your sync databases, sync those, and then remove them of course.

The idea is not only not to require root privileges, but more importantly to **avoid putting you in a situation where you'd risk messing up your system**, as you might unknowingly end up basically doing a `pacman -Sy foobar` (which is pretty generally understood to be a bad idea).

Because if kalu did sync your databases, and there were upgrades available, but you did not apply them right away (for one reason or another, e.g. you're busy, or were AFK when the notification poped up and didn't see it...) then your next -S operation would really by a -Sy, event though you might not even realize it.

## What does it check for?

kalu can check for a few things :

- **Arch Linux News**. To be sure not to miss an important announcement from Arch Linux's official website.

- **Available upgrades for installed packages**. That is, anything from one of the repos, or what would be upgraded with a `pacman -Syu`

- **Available upgrades for non-installed packages**. You can define a list of "watched packages", that is packages for which you'd like to be notified when an upgrade is out, even though they're not installed. (E.g. packages you repack for yourself to apply a patch or something)

- **Available upgrades for AUR packages**. All foreign packages (i.e. not found in any repo, aka `-Qm`) can be checked for upgrades available in the AUR.

- **Available upgrades for watched AUR packages**. Just like with "regular" packages, you can have a list of packages in the AUR for which you'd like to be notified when an upgrade is available, even though they're not installed.

Of course you don't have to use all of this, and you can define which of those checks kalu should do. Besides maintaining lists of watched (AUR) packages, you can also define a list of foreign packages that kalu should not check the AUR for. Since there's no reason to check for packages you know aren't from the AUR (e.g. packages of your own making).

## More than a notifier: kalu's updater

When a notification is shown, it will feature an action button. This button can be used to simply trigger a process of your choosing, e.g. you could have it start pacman with something like `urxvt -e sudo pacman -Syu`

However, **kalu comes with an integrated system upgrader**, which does exactly the same, only in a GTK GUI. Before being able to synchronize your databases (and possibly upgrade the system) the updater needs root privileges, obviously.

The way it works is: **kalu itself only contains the GUI**, and the part that does interact with libalpm (to actually upgrade your system) is in a secondary binary (`kalu-dbus`). This binary only will require root privileges, and will rely on PolicyKit to ensure you are authorized before doing anything.

You can also define one or more processes to be run after completing a system upgrade (to start e.g. [localepurge](https://aur.archlinux.org/packages.php?ID=11975 "AUR: localepurge: Script to remove disk space wasted for unneeded localizations") and/or [PkgClip](http://mywaytoarch.tumblr.com/post/16005116198/pkgclip-does-your-pacman-cache-need-a-trim "PkgClip: Does your pacman cache need a trim?")), and kalu will start them after each succesfull sysupgrade (and an optional confirmation, which for multiple processes will feature the full list so you can specify which (if any) to start).

Note that if you're not interested in this, you can remove it by specifying `--disable-updater` on the `configure` command line.

## Want to know more?

Some useful links if you're looking for more info:

- [blog post about kalu](http://mywaytoarch.tumblr.com/post/19350380240/keep-arch-linux-up-to-date-with-kalu "Keep Arch Linux Up-to-date with kalu")

- [source code & issue tracker](https://github.com/jjk-jacky/kalu "kalu @ GitHub.com")

- [PKGBUILD in AUR](https://aur.archlinux.org/packages.php?ID=56673 "AUR: kalu")

Plus, kalu comes with a man page.
