#!/bin/bash

cd `dirname $0`
mkdir -p cache
docker build . --tag flowee/buildenv:android


# copy the cache out.
#   cid=`docker run -d -ti -v cache:/mnt flowee/buildenv:android /bin/bash`
#   docker container exec $cid /usr/local/bin/copyBack.sh
#   docker container stop $cid
#   docker container rm $cid
