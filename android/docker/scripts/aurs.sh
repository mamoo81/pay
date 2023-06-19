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
makeAur android-sdk-platform-tools
makeAur android-sdk-build-tools
makeAur android-sdk-cmdline-tools-latest
makeAur android-platform

pacman -U --noconfirm /usr/local/cache/*zst

