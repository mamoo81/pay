#!/bin/bash

TAG=$1
if test -z "$TAG"; then
    print "Missing required argument 'TAG'"
    exit
fi
echo "Based on Qt version $TAG" >> /etc/versions
source /etc/profile

function checkout (
    (cd /usr/local/cache
    if ! test -d $1.git; then
        git clone --bare https://code.qt.io/qt/$1.git
    fi
    cd ~builduser
    git clone -l /usr/local/cache/$1.git -b $TAG)
)

# The QtBase builds are different.
checkout qtbase
mkdir -p ~builduser/build/qtbase
(cd ~builduser/build/qtbase && \
~builduser/qtbase/configure \
    -prefix /usr/local  \
    -no-openssl \
    -nomake examples \
    -no-dbus && \
ninja install)
rm -rf ~builduser/build/*

###  Android build
mkdir -p ~builduser/build/qtbase
cd ~builduser/build/qtbase
~builduser/qtbase/configure \
    -platform android-clang \
    -prefix /opt/android-qt6/ \
    -android-ndk /opt/android-ndk \
    -android-sdk /opt/android-sdk \
    -qt-host-path /usr/local \
    -android-abis arm64-v8a \
    -android-style-assets \
    -openssl-linked \
    -- \
    -DOPENSSL_USE_STATIC_LIBS=ON \
    -DOPENSSL_ROOT_DIR=/opt/android-ssl
ninja install
rm -rf ~builduser/build/*


# All the others.
for i in qtshadertools qtdeclarative qtsvg qtmultimedia
do
    checkout $i
    mkdir -p ~builduser/build/$i
    cd ~builduser/build/$i
    /usr/local/bin/qt-configure-module ~builduser/$i
    ninja install
    cd ~builduser
    rm -rf build/*

    # Android
    mkdir -p ~builduser/build/$i
    cd ~builduser/build/$i
    /opt/android-qt6/bin/qt-configure-module ~builduser/$i
    ninja install
    cd ~builduser
    rm -rf build/*
done

