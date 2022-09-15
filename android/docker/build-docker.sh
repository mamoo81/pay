#!/bin/bash

QtVersion=v6.3.1

cd `dirname $0`
mkdir -p cache
docker build . --tag flowee/buildenv-android:$QtVersion --build-arg QtVersion=$QtVersion

# copy the cache out.
#   cid=`docker run -d -ti -v cache:/mnt flowee/buildenv:android /bin/bash`
#   docker container exec $cid /usr/local/bin/copyBack.sh
#   docker container stop $cid
#   docker container rm $cid
