FROM ubuntu:24.10

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
      extra-cmake-modules
RUN apt update && apt -y install --no-install-recommends \
      libeigen3-dev \
      zlib1g-dev \
      libcfitsio-dev \
      libnova-dev \
      wcslib-dev \
      libraw-dev \
      libgsl-dev \
      libsecret-1-dev
RUN apt update && apt -y install --no-install-recommends \
      qt6-base-dev \
      qt6-declarative-dev \
      qt6-multimedia-dev \
      qt6-positioning-dev \
      libqt6websockets6-dev \
      libqt6svg6-dev \
      libkf6doctools-dev \
      libkf6config-dev \
      libkf6guiaddons-dev \
      libkf6i18n-dev \
      libkf6newstuff-dev \
      libkf6notifications-dev \
      libkf6xmlgui-dev \
      libkf6plotting-dev \
      libkf6crash-dev \
      libkf6notifyconfig-dev \
      libqt6sql6-sqlite \
      libqt6datavisualization6-dev
RUN apt update && apt -y install --no-install-recommends \
      phonon4qt6-backend-vlc \
      qtkeychain-qt6-dev \
      qml-module-qtquick-controls

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

# qt6 theme
ENV QT_QPA_PLATFORMTHEME=qt6ct
RUN apt update && apt -y --no-install-recommends install qt6ct
RUN d=/root/.config/qt6ct ; mkdir -p "$d" && echo '[Appearance]\nicon_theme=breeze' > "$d/qt6ct.conf"

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

# From https://invent.kde.org/sysadmin/ci-tooling/-/blob/master/system-images/suse-qt615/Dockerfile
RUN apt update && apt -y install dbus
RUN dbus-uuidgen > /etc/machine-id

CMD /bin/bash
