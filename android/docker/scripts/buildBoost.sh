#!/bin/bash

VERSION=1.67.0

echo "Based on boost version $VERSION" >> /etc/versions
source /etc/profile
VER2=`echo $VERSION|sed -e 's#\.#_#g'`

cd /usr/local/cache
if ! test -f boost_$VER2.tar.bz2; then
    curl -O https://netix.dl.sourceforge.net/project/boost/boost/$VERSION/boost_$VER2.tar.bz2
fi

cd ~builduser
tar xf /usr/local/cache/boost_$VER2.tar.bz2

cd boost_$VER2
export PATH="$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"
./bootstrap.sh --without-icu \
    --with-toolset=clang \
    --with-libraries=chrono,filesystem,iostreams,program_options,system,thread \
    --prefix=/opt/android-boost

echo "using clang : arm : /opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang++ : <cxxflags>\"-std=c++17 -fvisibility=hidden -fPIC\" <cflags>\"-fPIC\" ;" > user-config.jam

./b2 \
    --reconfigure \
    --user-config=user-config.jam \
    architecture=arm \
    address-model=64 \
    binary-format=elf \
    abi=aapcs \
    target-os=android \
    variant=release \
    link=static \
    threading=multi \
    install

#   toolset=clang-arm \
#   runtime-link=shared \
#   -sNO_BZIP2=1 \
#   -sNO_ZLIB=1 \
#
#    debug-symbols=off
