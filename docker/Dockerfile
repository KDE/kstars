FROM ubuntu:22.04

# Suitable for builds
RUN apt update && apt -y install --no-install-recommends software-properties-common gpg-agent
RUN apt update && add-apt-repository ppa:mutlaqja/ppa
RUN apt update && apt -y install --no-install-recommends \
      gcc-multilib \
      g++-multilib \
      make \
      gettext \
      coreutils \
      cmake \
      extra-cmake-modules \
      qtbase5-dev \
      qtdeclarative5-dev \
      qtmultimedia5-dev \
      qtpositioning5-dev \
      libqt5websockets5-dev \
      libqt5svg5-dev \
      libqt5sql5-sqlite \
      libkf5doctools-dev \
      libkf5config-dev \
      libkf5guiaddons-dev \
      libkf5i18n-dev \
      libkf5newstuff-dev \
      libkf5notifications-dev \
      libkf5xmlgui-dev \
      libkf5plotting-dev \
      libkf5crash-dev \
      libkf5notifyconfig-dev \
      libeigen3-dev \
      zlib1g-dev \
      libcfitsio-dev \
      libnova-dev \
      wcslib-dev \
      libraw-dev \
      libgsl-dev \
      phonon4qt5-backend-vlc \
      qt5keychain-dev \
      libqt5sql5-sqlite \
      libqt5datavisualization5-dev \
      qml-module-qtquick-controls \
      libsecret-1-dev

# Suitable for tests
RUN apt update && add-apt-repository ppa:mutlaqja/ppa
RUN apt update && add-apt-repository ppa:pch/phd2

ENV TZ=Greenwich
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt update && apt -y --no-install-recommends install \
      make \
      cmake \
      extra-cmake-modules \
      xplanet \
      xplanet-images \
      astrometry.net \
      kded5 \
      kinit \
      breeze-icon-theme \
      gsc gsc-data \
      phd2 \
      xvfb

# QT5 theme
ENV QT_QPA_PLATFORMTHEME=qt5ct
RUN apt update && apt -y --no-install-recommends install qt5ct
RUN d=/root/.config/qt5ct ; mkdir -p "$d" && echo '[Appearance]\nicon_theme=breeze' > "$d/qt5ct.conf"

# Ninja
RUN apt update && apt -y --no-install-recommends install ninja-build

# CCache
ENV CCACHE_DIR=/var/ccache
RUN apt update && apt -y --no-install-recommends install ccache
RUN mkdir -p $CCACHE_DIR && chmod a=rwx $CCACHE_DIR && update-ccache-symlinks

# AppImage
RUN apt update && apt -y --no-install-recommends install \
      python3-pip \
      python3-setuptools \
      patchelf \
      desktop-file-utils \
      libgdk-pixbuf2.0-dev \
      fakeroot \
      wget

# Saxon
RUN apt install -y --no-install-recommends \
      libsaxon-java \
      openjdk-11-jre-headless

# Astrometry
ADD http://data.astrometry.net/4200/index-4208.fits /root/.local/share/kstars/astrometry/
ADD http://data.astrometry.net/4200/index-4209.fits /root/.local/share/kstars/astrometry/
ADD http://data.astrometry.net/4200/index-4210.fits /root/.local/share/kstars/astrometry/

# From https://invent.kde.org/sysadmin/ci-tooling/-/blob/master/system-images/suse-qt515/Dockerfile
RUN apt update && apt -y install dbus
RUN dbus-uuidgen > /etc/machine-id

CMD /bin/bash

