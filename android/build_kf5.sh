#!/bin/bash

: ${QT_ANDROID?"Qt Android SDK path must be set"}
: ${ANDROID_NDK?"Android NDK path must be set"}
: ${ANDROID_SDK_ROOT?"Android SDK path must be set"}
: ${ANDROID_API_LEVEL?"Android API level"}

export ANDROID_ARCHITECTURE=arm
export ANDROID_ABI=armeabi-v7a
export ANDROID_TOOLCHAIN=arm-linux-androideabi
export ANDROID_NATIVE_API_LEVEL=android-$ANDROID_API_LEVEL

# Get the directory where the script is stored
SRCDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CURDIR="$(pwd)"/

#git clean -fdx

# Clone the KF5 sources
mkdir kf5
cd kf5
git clone git://anongit.kde.org/scratch/cordlandwehr/kdesrc-conf-android.git
mkdir -p extragear/kdesrc-build
git clone git://anongit.kde.org/kdesrc-build extragear/kdesrc-build
ln -s extragear/kdesrc-build/kdesrc-build kdesrc-build
ln -s kdesrc-conf-android/kdesrc-buildrc kdesrc-buildrc

# Change the build configuration
sed -E -i "s|build-dir.*|build-dir $CURDIR/kf5/kde/build/${android_architecture} |g" kdesrc-conf-android/kdesrc-buildrc
sed -E -i "s|source-dir.*|source-dir $CURDIR/kf5/kde/src |g" kdesrc-conf-android/kdesrc-buildrc
sed -E -i "s|kdedir.*|kdedir $CURDIR/kf5/kde/install/${android_architecture} |g" kdesrc-conf-android/kdesrc-buildrc
sed -E -i "s|-DCMAKE_TOOLCHAIN_FILE=.*?\\ |-DCMAKE_TOOLCHAIN_FILE=$SRCDIR/toolchain-android-kf5.cmake |g" kdesrc-conf-android/kdesrc-buildrc

if [ -e $qt_android_libs ]
then
    sed -E -i "s|-DCMAKE_PREFIX_PATH=.*?\\ |-DCMAKE_PREFIX_PATH=$QT_ANDROID |g" kdesrc-conf-android/kdesrc-buildrc
else
    echo "Qt Android libraries path doesn't exist. Exiting."
    exit
fi

sed -E -i "s|use-modules.+|use-modules kconfig ki18n kplotting|g" kdesrc-conf-android/kdesrc-buildrc
rm -rf ${kf5_android_path}/kde/build/${android_architecture}/* # clean build folders
./kdesrc-build libintl-lite extra-cmake-modules frameworks-android

# Fix some config files
sed -i '/find_package(Gettext/ s/^/#/' kde/install/lib/cmake/KF5I18n/KF5I18NMacros.cmake
sed -i '/find_package(PythonInterp/ s/^/#/' kde/install/lib/cmake/KF5I18n/KF5I18NMacros.cmake
sed -i '/find_dependency(Qt5Xml/ s/^/#/' kde/install/lib/cmake/KF5Config/KF5ConfigConfig.cmake
sed -i '/cxx_decltype/ s/^/#/' ${QT_ANDROID}/lib/cmake/Qt5Core/Qt5CoreConfigExtras.cmake

cp /usr/lib/x86_64-linux-gnu/libexec/kf5/kconfig_compiler_kf5 $CURDIR/kf5/kde/install/lib/libexec/kf5/kconfig_compiler_kf5

cd $CURDIR
