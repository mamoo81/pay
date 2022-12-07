ARG QtVersion=v6.4.1

FROM archlinux:latest
ARG QtVersion
LABEL maintainer="Tom Zander <tom@flowee.org>"
LABEL qtversion="${QtVersion}"
ENTRYPOINT ["su", "-", "builduser", "-c"]

add scripts /usr/local/bin
# for europe you might want to copy your local mirrorlist
#copy mirrorlist /etc/pacman.d/
RUN useradd builduser -d /home/builds -m -u 1000 -U \
    && pacman -Suy --noconfirm --noprogressbar \
    && pacman -Sy --noconfirm --noprogressbar --needed base-devel \
    cmake \
    ninja \
    git \
    python \
    libjpeg-turbo \
    xcb-util-keysyms \
    xcb-util-cursor \
    libgl \
    fontconfig \
    xdg-utils \
    shared-mime-info \
    xcb-util-wm \
    libxrender \
    libxi \
    sqlite \
    mesa \
    vulkan-headers \
    tslib \
    libinput \
    libxkbcommon-x11 \
    libproxy \
    libcups \
    double-conversion \
    brotli \
    libb2 \
    md4c \
    wget \
    jdk11-openjdk \
    && pacman -Sc --noconfirm \
    && rm -rf /var/cache/pacman/pkg/* \
    && /usr/local/bin/createRootPwd

# to enable caching, check the README.md
#add cache /usr/local/cache/
RUN mkdir -p /usr/local/cache \
    && git config --global advice.detachedHead false \
    && /usr/local/bin/aurs.sh \
    && rm -rf ~builduser/* \
    && /usr/local/bin/buildOpenSsl.sh \
    && rm -rf ~builduser/* \
    && /usr/local/bin/buildQt.sh ${QtVersion} \
    && rm -rf ~builduser/* \
    && /usr/local/bin/buildBoost.sh \
    && rm -rf ~builduser/* \
    && /usr/local/bin/buildQrEncode.sh \
    && rm -rf ~builduser/* \
    && /usr/local/bin/buildZXing.sh \
    && rm -rf ~builduser/* \
    && rm -rf /usr/local/cache

