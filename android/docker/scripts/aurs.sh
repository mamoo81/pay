#!/bin/bash
# this runs as root.

for i in android-ndk android-sdk-platform-tools android-sdk-cmdline-tools-latest android-platform
do
    cd ~builduser
    git clone https://aur.archlinux.org/$i.git
    chown builduser:builduser -R $i
    cd "$i"
    for f in /usr/local/cache/$i*zst; do
        ln -s "$f" .
    done
    for f in /usr/local/cache/*zip; do
        ln -s "$f" .
    done
    su builduser -c makepkg
    find . -type f -name '*zst' -exec ln "{}" /usr/local/cache ';'
done

pacman -U --noconfirm /usr/local/cache/*zst
