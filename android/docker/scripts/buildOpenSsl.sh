#!/bin/bash

# this runs as root.
VERSION=1.1.1q
echo "Using OpenSSL $VERSION" >> /etc/versions
source /etc/profile
cd /usr/local/cache

if ! test -f openssl-$VERSION.tar.gz; then
    curl -O https://www.openssl.org/source/openssl-$VERSION.tar.gz
fi

cd ~builduser
export PATH="/opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"
tar xf /usr/local/cache/openssl-$VERSION.tar.gz
cd openssl-$VERSION
./Configure shared android-arm64 -D__ANDROID_API__=21 --prefix=/opt/android-ssl

# don't compress these two into one, 'install' doesn't like -j
make -j`nproc` 2>&1 >/dev/null
make install 2>&1 >/dev/null

echo "SSL $VERSION DONE"
