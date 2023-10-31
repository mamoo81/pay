#!/bin/bash

if test "$1" != "force"; then
    if test `id -u` != "1000";
    then
        echo "The docker file assumes you compile using a user with UID=1000"
        echo "Your current user-id is not 1000, and thus the generated image"
        echo "won't work for you. You may want to adjust the Docker file and"
        echo "set your user-id in the 'useradd' line."
        echo -n "Your current users ID is: "
        id -u
        echo "If you are ready to build the image, pass in 'force' to this build script"
        exit
    fi
fi

QtVersion=v6.5.3

cd `dirname $0`
docker build . --tag flowee/buildenv-android:$QtVersion --build-arg QtVersion=$QtVersion

# copy newly downloaded items to the cache (does nothing by default)
docker run --rm -ti -v `pwd`:/mnt flowee/buildenv-android:$QtVersion /usr/local/bin/copyBack.sh
