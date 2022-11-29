#!/bin/bash
# this runs as root.

function makeAur(
    cd ~builduser
    pkg="$1"
    git clone https://aur.archlinux.org/$pkg.git
    cd "$pkg"
    commit=$2
    if test -n "$commit"
    then
        git checkout $commit
    fi
    chown builduser:builduser -R . .git
    for i in /usr/local/cache/$pkg*zst; do
        ln -s "$i" .
    done
    for i in /usr/local/cache/*zip; do
        if test -f $i; then
            ln -s "$i" .
        fi
    done
    su builduser -c makepkg
    find . -type f -name '*zst' -exec ln "{}" /usr/local/cache ';'
)


makeAur android-ndk
# For now we hardcode the 31.0.3 release of the sdk, seeing as Qt hardcodes gradle 7.4.2, which doesn't like a newer sdk.
makeAur android-sdk-platform-tools 89f6840df092ea2f1fc3d446c41be78cf9b66339
makeAur android-sdk-build-tools 87aea36e5aef112d7af0c2ae5db154e96ab633c3
makeAur android-sdk-cmdline-tools-latest
makeAur android-platform

pacman -U --noconfirm /usr/local/cache/*zst

