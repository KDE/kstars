#!/usr/bin/env bash
set -euo pipefail

apt update
apt -y install git cmake

# Suitable for builds
apt -y install --no-install-recommends \
    gcc-multilib \
    g++-multilib \
    make \
    gettext \
    coreutils \
    cmake \
    extra-cmake-modules

apt -y install --no-install-recommends \
    libeigen3-dev \
    zlib1g-dev \
    libcfitsio-dev \
    libnova-dev \
    wcslib-dev \
    libraw-dev \
    libgsl-dev \
    libsecret-1-dev

if [ "$QT_VERSION" -eq 6 ]; then
    apt -y install --no-install-recommends \
        qt6-base-dev \
	qt6-graphs-dev \
        qt6-declarative-dev \
        qt6-multimedia-dev \
        qt6-positioning-dev \
        libqt6websockets6-dev \
        libqt6svg6-dev \
        libqt6sql6-sqlite \
        libkf6doctools-dev \
        libkf6config-dev \
        libkf6guiaddons-dev \
        libkf6i18n-dev \
        libkf6newstuff-dev \
        libkf6notifications-dev \
        libkf6xmlgui-dev \
        libkf6plotting-dev \
        libkf6crash-dev \
        libkf6notifyconfig-dev
else
    apt -y install --no-install-recommends \
        libkf5plotting-dev \
        libkf5xmlgui-dev \
        libkf5kio-dev \
        kinit-dev \
        libkf5newstuff-dev \
        libkf5doctools-dev \
        libkf5notifications-dev \
        libkf5crash-dev \
        libkf5notifyconfig-dev \
        libqt5svg5-dev \
        qtdeclarative5-dev \
        libqt5websockets5-dev \
        qt5keychain-dev
fi

apt -y install --no-install-recommends \
    phonon4qt6-backend-vlc \
    qtkeychain-qt6-dev \
    qml-module-qtquick-controls

ln -snf "/usr/share/zoneinfo/$TZ" /etc/localtime
echo "$TZ" > /etc/timezone

apt -y --no-install-recommends install \
    make \
    cmake \
    extra-cmake-modules \
    xplanet \
    xplanet-images \
    astrometry.net \
    kded5 \
    kinit \
    breeze-icon-theme \
    xvfb

# qt6 theme
apt -y --no-install-recommends install qt6ct
d=/root/.config/qt6ct
mkdir -p "$d"
printf '[Appearance]\nicon_theme=breeze\n' > "$d/qt6ct.conf"

# Ninja
apt -y --no-install-recommends install ninja-build

# CCache
apt -y --no-install-recommends install ccache
mkdir -p "$CCACHE_DIR"
chmod a=rwx "$CCACHE_DIR"
update-ccache-symlinks

# AppImage
apt -y --no-install-recommends install \
    python3-pip \
    python3-setuptools \
    patchelf \
    desktop-file-utils \
    libgdk-pixbuf-xlib-2.0-dev \
    fakeroot \
    wget

# Saxon
apt install -y --no-install-recommends \
    libsaxon-java \
    openjdk-11-jre-headless

# Astrometry
mkdir -p /root/.local/share/kstars/astrometry
wget -O /root/.local/share/kstars/astrometry/index-4208.fits \
    http://data.astrometry.net/4200/index-4208.fits
wget -O /root/.local/share/kstars/astrometry/index-4209.fits \
    http://data.astrometry.net/4200/index-4209.fits
wget -O /root/.local/share/kstars/astrometry/index-4210.fits \
    http://data.astrometry.net/4200/index-4210.fits

# From KDE CI tooling image
apt -y install dbus
dbus-uuidgen > /etc/machine-id

rm -rf /var/lib/apt/lists/*
