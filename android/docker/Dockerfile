ARG QtVersion=v6.3.1

# We use a 'cache' feature where we place big downloads and compiled stuff.
# We also use the 'FROM' multiple times to avoid inheriting an image
# which had lots of intermediate stuff installed that we don't need in
# the final image.

# First get some native stuff built.
FROM archlinux:latest
ARG QtVersion
LABEL maintainer="Tom Zander <tom@flowee.org>"
LABEL qtVersion="${QtVersion}"

# for europe you might want to copy your local mirrorlist
#copy mirrorlist /etc/pacman.d/
RUN pacman -Sy --noconfirm base-devel \
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
    jdk11-openjdk \
    git \
    && pacman -Sc --noconfirm
add scripts /usr/local/bin
# to enable caching, check the README.md
add cache /usr/local/cache/
RUN useradd builduser -d /home/builds -m -u 1000 -U && \
    mkdir -p /usr/local/cache && \
    git config --global advice.detachedHead false && \
    /usr/local/bin/aurs.sh && \
    /usr/local/bin/buildOpenSsl.sh && \
    /usr/local/bin/buildQt.sh ${QtVersion}

#&& \
RUN /usr/local/bin/buildBoost.sh && \
    rm -rf ~builduser/* && \
    rm -rf /usr/local/

