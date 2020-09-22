FROM ubuntu:latest

# Suitable for builds
RUN apt update && apt -y install --no-install-recommends software-properties-common
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
      kdoctools-dev \
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
      libindi-dev \
      indi-bin \
      wcslib-dev \
      libraw-dev \
      libgsl-dev \
      phonon4qt5-backend-vlc \
      qt5keychain-dev \
      libsecret-1-dev

# Suitable for tests
RUN apt update && add-apt-repository ppa:mutlaqja/ppa
RUN apt update && add-apt-repository ppa:pch/phd2
RUN apt update && apt -y --no-install-recommends install \
      make \
      cmake \
      extra-cmake-modules \
      xplanet \
      xplanet-images \
      astrometry.net \
      kded5 \
      kinit \
      libindi1 \
      indi-bin \
      breeze-icon-theme \
      libqt5sql5-sqlite \
      qml-module-qtquick-controls \
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
      wget \
      gpg-agent

# Saxon
RUN apt install -y --no-install-recommends \
      libsaxon-java \
      openjdk-11-jre-headless

CMD /bin/bash

