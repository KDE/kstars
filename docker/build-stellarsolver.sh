#!/usr/bin/env bash
set -euo pipefail

mkdir -p "$SRC_DIR" "$BUILD_DIR"

cd "$SRC_DIR"
git clone --branch "$BRANCH" --depth 1 "$REPO" "$PACKAGE"

# Install base packages for Stellarsolver
case "$TARGET" in
    opensuse)
        zypper --non-interactive install \
            libeigen3-devel \
            cfitsio-devel \
            zlib-ng-devel \
            extra-cmake-modules \
            gettext \
            libnova-devel \
            gsl-devel \
            libraw-devel \
            wcslib-devel \
            xplanet \
            libsecret-1-0 \
            kinit-devel \
            kf6-breeze-icons
        ;;
    ubuntu)
        apt-get update
        apt-get -y install --no-install-recommends \
            build-essential \
            cmake \
            git \
            libeigen3-dev \
            libcfitsio-dev \
            zlib1g-dev \
            extra-cmake-modules \
            gettext \
            libnova-dev \
            libgsl-dev \
            libraw-dev \
            wcslib-dev \
            xplanet \
            xplanet-images \
            libsecret-1-dev \
            breeze-icon-theme
        ;;
    *)
        echo "Unsupported TARGET: $TARGET" >&2
        exit 1
        ;;
esac

# Install packages depending on the Qt version
case "$TARGET" in
    opensuse)
        if [ "$QT_VERSION" = "6" ]; then
            zypper --non-interactive install \
                libKF6Plotting6 \
                libQt6Svg6 \
                libKF6XmlGui6 \
                kf6-kio-devel \
                libKF6NewStuffCore6 \
                libKF6DocTools6 \
                libKF6Notifications6 \
                qt6-declarative-devel \
                libKF6Crash6 \
                libKF6NotifyConfig6 \
                libQt6WebSockets6 \
                qtkeychain-qt6-devel \
                libQt6Graphs6
        else
            zypper --non-interactive install \
                libKF5Plotting5 \
                libQt5Svg5 \
                libKF5XmlGui5 \
                kio-devel \
                libKF5NewStuff5 \
                libKF5DocTools5 \
                libKF5Notifications5 \
                libKF5Declarative5 \
                libKF5Crash5 \
                libKF5NotifyConfig5 \
                libQt5WebSockets5-imports \
                qtkeychain-qt5-devel
        fi
        ;;
    ubuntu)
        apt-get update
        if [ "$QT_VERSION" -ge "6" ]; then
            apt-get -y install --no-install-recommends \
                qt6-base-dev \
                libgl1-mesa-dev \
                libqt6svg6-dev \
                libqt6websockets6-dev \
                qt6-declarative-dev \
                qtkeychain-qt6-dev \
                qt6-graphs-dev
        else
            apt-get -y install --no-install-recommends \
                libqt5svg5-dev \
                libqt5websockets5-dev \
                qtdeclarative5-dev \
                qt5keychain-dev \
                kinit-dev \
                libkf5plotting-dev \
                libkf5xmlgui-dev \
                libkf5kio-dev \
                libkf5newstuff-dev \
                libkf5doctools-dev \
                libkf5notifications-dev \
                libkf5crash-dev \
                libkf5notifyconfig-dev
        fi
        ;;
esac

cd "$BUILD_DIR"

# Build Stellarsolver
if [ "$QT_VERSION" -ge "6" ]; then
    cmake -DCMAKE_INSTALL_PREFIX=/usr \
          -DCMAKE_BUILD_TYPE=RelWithDebInfo \
          -DUSE_QT5=Off \
          "$SRC_DIR/$PACKAGE"
else
    cmake -DCMAKE_INSTALL_PREFIX=/usr \
          -DCMAKE_BUILD_TYPE=RelWithDebInfo \
          -DUSE_QT5=On \
          "$SRC_DIR/$PACKAGE"
fi

make -j"${THREADS}" install

rm -rf /var/lib/apt/lists/* 2>/dev/null || true
rm -rf "$SRC_DIR" "$BUILD_DIR"

