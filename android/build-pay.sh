#!/bin/bash
# This file is part of the Flowee project
# Copyright (C) 2022-2024 Tom Zander <tom@flowee.org>
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

if test -f smartBuild.sh; then
    ./smartBuild.sh
    exit
fi

if test -z "$_thehub_dir_"; then
    echo "Usage:"
    echo "  build-pay <HUB_builddir> [PAY_NATIVE_builddir]"
    echo ""
    echo "Start this client in your builddir"
    echo "HUB-builddir is the dir where the android build of flowe-thehub is."
    echo "Pay_NATIVE-builddir for a native build of flowee-pay, for translations."
    exit
fi

# check if the provided dir is really an android HUB-libs dir
_thehub_dir_=`realpath $_thehub_dir_`
_ok=0
if test -f $_thehub_dir_/lib/libflowee_p2p.a; then
    if grep -q -- -DANDROID $_thehub_dir_/build.ninja; then
    _ok=1
    fi
fi

if test "$_ok" -eq 0; then
    echo "Invalid or not compiled for Android HUB build dir."
    exit
fi

if test -z "$_docker_name_"; then
    _docker_name_="codeberg.org/flowee/buildenv-android:v6.5.3"
fi

mkdir -p imports
if test -d "$_pay_native_name_"; then
    cp -f "$_pay_native_name_"/*qm imports/
fi

floweePaySrcDir=`dirname $0`/..
if test -f $floweePaySrcDir/android/netlog.conf; then
    developerSwitches="-DNetworkLogClient=ON -Dquick_deploy=ON"
fi

if test ! -f .config; then
    cat << HERE > .config
#!/bin/bash
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
        -DCMAKE_BUILD_TYPE=Release $developerSwitches \\
        -G Ninja \\
        /home/builds/src
fi
HERE
    chmod 755 .config
fi

if test ! -f .sign; then
    cat << HERE > .sign
#!/bin/bash

cd /home/builds/build
export QT_ANDROID_KEYSTORE_STORE_PASS=longPassword
export QT_ANDROID_KEYSTORE_KEY_PASS=longPassword

/usr/local/bin/androiddeployqt \
    --release \
    --input /home/builds/build/android-pay_mobile-deployment-settings.json \
    --output /home/builds/build/android-build \
    --sign /home/builds/src/android/selfsigned.keystore floweepay

echo -n "-- COPYING: "
cp -v android-build//build/outputs/apk/release/android-build-release-signed.apk floweepay.apk
HERE
    chmod 755 .sign
fi

if ! test -f smartBuild.sh; then
cat << HERE > smartBuild.sh
#!/bin/bash
#Created by build-pay.sh

if test "\$1" = "distclean"; then
    mv android-build/assets/blockheaders .
    perl -e 'use File::Path qw(remove_tree); opendir DIR, "."; while (\$entry = readdir DIR) { if (\$entry=~/^\./) { next; } if (\$entry=~/smartBuild.sh$/ || \$entry=~/^imports$/ || \$entry=~/^blockheaders$/) { next; } unlink "\$entry"; remove_tree "\$entry"; }'
    mkdir -p android-build/assets/
    mv blockheaders android-build/assets/
fi

if test -f .docker; then
    DOCKERID=\`cat .docker\`
    if test -n "\$DOCKERID"; then
        if test -z "\`docker container inspect \$DOCKERID | grep '"Status": "running"'\`"; then
            echo "docker image died, removing"
            docker container rm \$DOCKERID
            DOCKERID=""
        fi
    fi
fi
if test -z "\$DOCKERID"; then
    echo "starting docker container"
    DOCKERID=\`docker run -d -ti \\
        --volume=`pwd`:/home/builds/build \\
        --volume=$floweePaySrcDir:/home/builds/src \\
        --volume=$_thehub_dir_:/home/builds/floweelibs \\
        ${_docker_name_} /bin/bash\`
    echo "\$DOCKERID" > .docker
fi
execInDocker="docker container exec --workdir /home/builds --user \`id -u\` \$DOCKERID"

if test ! -f build.ninja; then
    cp -n imports/*qm .
    \$execInDocker build/.config
fi

if test -f $floweePaySrcDir/android/netlog.conf; then
    cp $floweePaySrcDir/android/netlog.conf android-build/assets/
fi

\$execInDocker /usr/bin/ninja -C build pay_mobile pay_mobile_prepare_apk_dir && \
if test "\$1" = "sign" -o "\$2" = "sign"; then
    \$execInDocker build/.sign
fi

HERE
    chmod 700 smartBuild.sh
fi

./smartBuild.sh
