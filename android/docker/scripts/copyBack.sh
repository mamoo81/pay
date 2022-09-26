#!/bin/bash

# this runs as a normal user.

cd /usr/local/cache
for i in *
do
    if ! test -a /mnt/cache/$i; then
        cp -ar "$i" /mnt/cache/
    fi
done
