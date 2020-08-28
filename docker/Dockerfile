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
      libsecret-1-dev \
      libstellarsolver

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

CMD /bin/bash

