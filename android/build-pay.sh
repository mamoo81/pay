#!/bin/bash
# This file is part of the Flowee project
# Copyright (C) 2022 Tom Zander <tom@flowee.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

_thehub_dir_="$1"
_pay_native_name_="$2"

if test -d .bin; then
    .bin/doBuild
    exit
fi

if test -z "$_thehub_dir_"; then
    echo "Usage:"
    echo "  build-android <HUB_builddir> [PAY_NATIVE_builddir]"
    echo ""
    echo "Start this client in your builddir"
    echo "HUB-builddir is the dir where the android build of flowe-thehub is."
    echo "Pay_NATIVE-builddir for a native build of flowee-pay (optional)."
    exit
fi


# check if the provided dir is really an android HUB-libs dir
_thehub_dir_=`realpath $_thehub_dir_`
_ok=0
if test -f $_thehub_dir_/lib/libflowee_p2p.a; then
    if grep -q OS_ANDROID $_thehub_dir_/build.ninja; then
    _ok=1
    fi
fi

if test "$_ok" -eq 0; then
    echo "Invalid or not compiled for Android HUB build dir."
    exit
fi

if test -z "$_docker_name_"; then
    _docker_name_="flowee/buildenv-android:v6.4.0"
fi

if test -d "$_pay_native_name_"; then
    cp -f "$_pay_native_name_"/*qm .
fi

floweePaySrcDir=`dirname $0`/..

if ! test -f .config; then
    cat << HERE > .config
cd /home/builds/build

if ! test -f build.ninja; then
    cmake \\
        -DCMAKE_TOOLCHAIN_FILE=/opt/android-qt6/lib/cmake/Qt6/qt.toolchain.cmake \\
        -DANDROID_SDK_ROOT=/opt/android-sdk \\
        -DANDROID_NDK_ROOT=/opt/android-ndk \\
        -Dflowee_DIR=/home/builds/floweelibs/cmake \\
        -DOPENSSL_ROOT_DIR=/opt/android-ssl \\
        -DOPENSSL_CRYPTO_LIBRARY=/opt/android-ssl/lib/libcrypto.a \\
        -DOPENSSL_SSL_LIBRARY=/opt/android-ssl/lib/libssl.a \\
        -DOPENSSL_INCLUDE_DIR=/opt/android-ssl/include/ \\
        -G Ninja \\
        -DCMAKE_INSTALL_PREFIX=\`pwd\` \\
        /home/builds/src
fi

ninja install
HERE
    chmod 755 .config
fi

if ! test -d .bin; then
    mkdir .bin
cat << HERE > .bin/doBuild
docker run --rm -ti\
    --volume=`pwd`:/home/builds/build \
    --volume=$floweePaySrcDir:/home/builds/src \
    --volume=$_thehub_dir_:/home/builds/floweelibs \
    $_docker_name_ \
    build/.config
HERE
    chmod 700 .bin/doBuild
fi

.bin/doBuild
