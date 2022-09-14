#!/bin/bash

cd /usr/local/cache
for i in *
do
    if ! test -a /mnt/$i; then
        cp -ar "$i" /mnt/
    fi
done
