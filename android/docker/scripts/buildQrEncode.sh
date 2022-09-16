#!/bin/bash

VERSION=4.1.1

echo "Based on qrencode version $VERSION" >> /etc/versions
source /etc/profile

cd /usr/local/cache
if ! test -f qrencode-${VERSION}.tar.bz2; then
    curl -O https://fukuchi.org/works/qrencode/qrencode-${VERSION}.tar.bz2
fi

cd ~builduser
tar xf /usr/local/cache/qrencode-${VERSION}.tar.bz2

cd qrencode-$VERSION
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=/opt/android-qt6/lib/cmake/Qt6/qt.toolchain.cmake \
    -DCMAKE_INSTALL_PREFIX=/opt/android-qrencode \
    -DWITHOUT_PNG=ON \
    -G Ninja \
    ..

ninja install
