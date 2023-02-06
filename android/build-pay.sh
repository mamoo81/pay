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

if test -f smartBuild.sh; then
    ./smartBuild.sh noapk
    exit
fi

if test -z "$_pay_native_name_"; then
    echo "Usage:"
    echo "  build-pay <HUB_builddir> <PAY_NATIVE_builddir>"
    echo ""
    echo "Start this client in your builddir"
    echo "HUB-builddir is the dir where the android build of flowe-thehub is."
    echo "Pay_NATIVE-builddir for a native build of flowee-pay."
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
    _docker_name_="flowee/buildenv-android:v6.4.1"
fi

if ! test -f "$_pay_native_name_/blockheaders-meta-extractor"; then
    echo "Invalid or not compiled for Android Pay-native dir."
    exit
fi

mkdir -p imports
cp -f "$_pay_native_name_"/*qm imports/
cp -f "$_pay_native_name_"/blockheaders-meta-extractor imports/

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
    --input /home/builds/build/android-pay_mobile-deployment-settings.json \
    --output /home/builds/build/android-build \
    --sign /home/builds/src/android/selfsigned.keystore floweepay
HERE
    chmod 755 .sign
fi

if ! test -f smartBuild.sh; then
cat << HERE > smartBuild.sh
#!/bin/bash
#Created by build-pay.sh

if test "\$1" = "distclean"; then
    perl -e 'use File::Path qw(remove_tree); opendir DIR, "."; while (\$entry = readdir DIR) { if (\$entry=~/^\./) { next; } if (\$entry=~/smartBuild.sh$/ || \$entry=~/^imports$/) { next; } unlink "\$entry"; remove_tree "\$entry"; }'
fi

if test "\$1" = "sign" -o "\$2" = "sign"
then
    MAKE_SIGNED_APK=1
else
  if test "\$1" != "noapk"; then
    MAKE_UNSIGNED_APK=apk
  fi
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

if test -f android-build/assets/blockheaders -a ! -f android-build/assets/blockheaders.info; then
    imports/blockheaders-meta-extractor android-build/assets
fi
if test -f $floweePaySrcDir/android/netlog.conf; then
    cp $floweePaySrcDir/android/netlog.conf android-build/assets/
fi

\$execInDocker /usr/bin/ninja -C build pay_mobile pay_mobile_prepare_apk_dir \$MAKE_UNSIGNED_APK

if test -n "\$MAKE_SIGNED_APK"
then
    \$execInDocker build/.sign
    echo -n "-- COPYING: "
    cp -v android-build//build/outputs/apk/release/android-build-release-signed.apk floweepay.apk
fi

HERE
    chmod 700 smartBuild.sh
fi

./smartBuild.sh noapk
