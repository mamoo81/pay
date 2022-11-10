#!/bin/bash

TAG=$1
if test -z "$TAG"; then
    print "Missing required argument 'TAG'"
    exit
fi
echo "Based on Qt version $TAG" >> /etc/versions
source /etc/profile

for i in qtbase qtshadertools qtdeclarative qtsvg
do
    cd /usr/local/cache
    if ! test -d $i.git; then
        git clone --bare https://code.qt.io/qt/$i.git
    fi
    cd ~builduser
    git clone -l /usr/local/cache/$i.git -b $TAG
done

###  Native build
# QtBase
mkdir -p ~builduser/build-native/qtbase
(cd ~builduser/build-native/qtbase && \
~builduser/qtbase/configure \
    -prefix /usr/local  \
    -no-openssl \
    -nomake examples \
    -no-dbus && \
ninja install)

# Others
for i in qtshadertools qtdeclarative qtsvg
do
    cd ~builduser/build-native
    mkdir -p $i
    cd $i
    /usr/local/bin/qt-configure-module ~builduser/$i
    ninja install
done

###  Android build
# QtBase

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

# qtbase gives
# -- Neither ANDROID_PLATFORM nor ANDROID_NATIVE_API_LEVEL were specified, using API level 23 as default

# Others
for i in qtshadertools qtdeclarative qtsvg
do
    cd ~builduser/build
    mkdir -p $i
    cd $i
    /opt/android-qt6/bin/qt-configure-module ~builduser/$i
    ninja install
done

cd /opt/android-qt6/
patch -p0 < /usr/local/bin/qtbase-cmake-macros.patch


