#!/bin/bash

VERSION=1.4.0

echo "Based on zxing version $VERSION" >> /etc/versions
source /etc/profile

cd /usr/local/cache
if ! test -f zxing-cpp-v${VERSION}.tar.gz; then
    wget --quiet -O zxing-cpp-v${VERSION}.tar.gz https://github.com/zxing-cpp/zxing-cpp/archive/refs/tags/v${VERSION}.tar.gz
fi
cd ~builduser
tar xf /usr/local/cache/zxing-cpp-v${VERSION}.tar.gz
cd zxing-cpp-${VERSION}
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=/opt/android-qt6/lib/cmake/Qt6/qt.toolchain.cmake \
    -DCMAKE_INSTALL_PREFIX=/opt/android-zxing \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_BLACKBOX_TESTS=OFF \
    -DBUILD_UNIT_TESTS=OFF \
    -DBUILD_PYTHON_MODULE=OFF \
    -G Ninja \
    ..

ninja install

