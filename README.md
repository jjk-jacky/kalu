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

## Want to know more?

Some useful links if you're looking for more info:

- blog post about kalu: http://mywaytoarch.tumblr.com/post/19350380240/keep-arch-linux-up-to-date-with-kalu

- source code & issue tracker: https://bitbucket.org/jjacky/kalu

- PKGBUILD in AUR: https://aur.archlinux.org/packages.php?ID=56673

Plus, kalu comes with a man page.
