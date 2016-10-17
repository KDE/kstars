#!/bin/bash
# Use this script to compile cfitsio for the Android platform. It does a cross-compilation 
# using the Android NDK standalone toolchain. Consult STANDALONE-TOOLCHAIN.HTML in the 
# Android NDK documentation for more information 
#
# There is one code issue that you'll need to fix. In fitscore.c there are two references
# to the decimal_point member of the struct lconv type. With these builds, that struct is
# actually empty. In methods ffc2rr and ffc2dd I replaced the variable decimalpt with
# an explicit value of '.' and deleted the four lines starting "if (!decimalpt)".
#
#
# wherever your ndk is
export NDK_ROOT=$ANDROID_NDK
#
# where you'd like the temporary toolchain to be
mkdir /tmp/androidToolchainC -p
export PREFIX=/tmp/androidToolchainC
#
# N.B. This was developed on a Mac running Mac OSX 10.8.5 and Xcode 5.0.2. You may need
# different variables on other systems. See STANDALONE-TOOLCHAIN.HTML
$NDK_ROOT/build/tools/make-standalone-toolchain.sh --platform=android-${ANDROID_API_LEVEL} --install-dir=$PREFIX --toolchain=$ANDROID_TOOLCHAIN-clang
export PATH="$PREFIX/bin:$PATH"
# Using clang here. Gcc is an alternative, but there is currently an Android compiler bug # that prevents using gcc
# https://code.google.com/p/android/issues/detail?id=59913 

if [ "$ANDROID_ARCHITECTURE" == "x86" ]
then
	ANDROID_TOOLCHAIN="i686-linux-android"
fi

export CC="$ANDROID_TOOLCHAIN-clang" 
export LD="$ANDROID_TOOLCHAIN-ld"
export AR="$ANDROID_TOOLCHAIN-ar"
export RANLIB="$ANDROID_TOOLCHAIN-ranlib"
export STRIP="$ANDROID_TOOLCHAIN-strip"
make distclean

# configure will complain about the --host, but ignore it. This is indeed a 
# cross-compilation case
./configure --prefix=$PREFIX --host=$ANDROID_TOOLCHAIN
make install
