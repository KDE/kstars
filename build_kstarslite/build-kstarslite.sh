#!/bin/bash
. build-config #load values from config file
mkdir android_libs -p
# Ask user what libraries should be downloaded/built
if [ -z "$download_kf5" ]
then
	echo "Should we download everything needed to build KF5 for Android(Y/N)? Check (http://thelastpolaris.blogspot.de/2016/05/how-to-build-kstars-lite-for-android.html)"
	read user_response
	download_kf5="$user_response"
fi

if [ -z "$build_kf5" ]
then
	echo "Do you need to build KF5 for Android?"
	read user_response
	build_kf5="$user_response"
fi

if [ -z "$build_cfitsio" ]
then
	echo "Do you need to build CFITSIO for Android"
	read user_response
	build_cfitsio="$user_response"
fi

if [ -z "$build_nova" ]
then
	echo "Do you need to build NOVA for Android"
	read user_response
	build_nova="$user_response"
fi

if [ -z "$download_indi" ]
then
	echo "Do you need to download INDI? (You can set location of INDI in build-config file)"
	read user_response
	download_indi="$user_response"
fi

if [ "$download_indi" = "Y" ] || [ "$download_indi" = "y" ] || [ "$download_indi" = "Yes" ] || [ "$download_indi" = "yes" ]
then
		echo "Specify the location where to put INDI"
		read user_response
		download_indi_path="$user_response"
fi

if [ -z "$build_indi" ]
then
	echo "Do you need to build INDI for Android?"
	read user_response
	build_indi="$user_response"
fi

if [ -z "$build_libraw" ]
then
	echo "Do you need to build LibRAW for Android?"
	read user_response
	build_libraw="$user_response"
fi

# if [ ! -d "$kf5_android_path" ]
# then
# 	echo "Directory for saving KF5 doesn't exist"
# 	exit
# fi

echo "Libraries to download:"
if [ "$download_kf5" = "Y" ] || [ "$download_kf5" = "y" ] || [ "$download_kf5" = "Yes" ] || [ "$download_kf5" = "yes" ]
then
	echo "* KF5"
fi
if [ "$download_indi" = "Y" ] || [ "$download_indi" = "y" ] || [ "$download_indi" = "Yes" ] || [ "$download_indi" = "yes" ]
then
	echo "* INDI"
fi

echo "Libraries to build:"
if [ "$build_kf5" = "Y" ] || [ "$build_kf5" = "y" ] || [ "$build_kf5" = "Yes" ] || [ "$build_kf5" = "yes" ]
then
	echo "* KF5"
fi
if [ "$build_cfitsio" = "Y" ] || [ "$build_cfitsio" = "y" ] || [ "$build_cfitsio" = "Yes" ] || [ "$build_cfitsio" = "yes" ]
then
	echo "* CFITSIO"
fi
if [ "$build_nova" = "Y" ] || [ "$build_nova" = "y" ] || [ "$build_nova" = "Yes" ] || [ "$build_nova" = "yes" ]
then
	echo "* NOVA"
fi
if [ "$build_indi" = "Y" ] || [ "$build_indi" = "y" ] || [ "$build_indi" = "Yes" ] || [ "$build_indi" = "yes" ]
then
	echo "* INDI"
fi
if [ "$build_libraw" = "Y" ] || [ "$build_libraw" = "y" ] || [ "$build_libraw" = "Yes" ] || [ "$build_libraw" = "yes" ]
then
	echo "* LibRAW"
fi

echo "Please, wait while we are downloading/building the libraries"

if [ "$download_kf5" = "Y" ] || [ "$download_kf5" = "y" ] || [ "$download_kf5" = "Yes" ] || [ "$download_kf5" = "yes" ]
then
	# rm -rf ${kf5_android_path}
	mkdir ${kf5_android_path} -p
	cd ${kf5_android_path}
	git clone git://anongit.kde.org/scratch/cordlandwehr/kdesrc-conf-android.git
	mkdir -p extragear/kdesrc-build  -p
	git clone git://anongit.kde.org/kdesrc-build extragear/kdesrc-build  
	ln -s extragear/kdesrc-build/kdesrc-build kdesrc-build
	ln -s kdesrc-conf-android/kdesrc-buildrc kdesrc-buildrc
fi

export ANDROID_NDK="$android_ndk"
export ANDROID_SDK_ROOT="$android_sdk_root"
export Qt5_android="$qt_android_libs"
export PATH=$ANDROID_SDK_ROOT/platform-tools/:$PATH
export ANT="$ant"
export JAVA_HOME="$java_home"
export ANDROID_ARCHITECTURE="$android_architecture"
export ANDROID_API_LEVEL="$android_api_level"

if [ "$ANDROID_ARCHITECTURE" == "arm64" ]
then
	export ANDROID_TOOLCHAIN="aarch64-linux-android"
	export ANDROID_ABI="arm64-v8a"
elif [ "$ANDROID_ARCHITECTURE" == "arm" ]
then
	export ANDROID_TOOLCHAIN="arm-linux-androideabi"
	export ANDROID_ABI="armeabi-v7a"
elif [ "$ANDROID_ARCHITECTURE" == "x86" ]
then
	export ANDROID_TOOLCHAIN="x86"
	export ANDROID_ABI="x86"
elif [ "$ANDROID_ARCHITECTURE" == "x86_64" ]
then
	export ANDROID_TOOLCHAIN="x86_64-linux-android"
	export ANDROID_ABI="x86_64"
elif [ "$ANDROID_ARCHITECTURE" == "mips" ]
then
	export ANDROID_TOOLCHAIN="mipsel-linux-android"
	export ANDROID_ABI="mips"
elif [ "$ANDROID_ARCHITECTURE" == "mips64" ]
then
	export ANDROID_TOOLCHAIN="mips64el-linux-android"
	export ANDROID_ABI="mips64"
else
	echo "Can't recognize the architecture. Exit."
	exit
fi

if [ "$build_kf5" = "Y" ] || [ "$build_kf5" = "y" ] || [ "$build_kf5" = "Yes" ] || [ "$build_kf5" = "yes" ]
then
	cd ${kf5_android_path}
	sed -E -i "s|build-dir.*|build-dir ${kf5_android_path}/kde/build/${android_architecture} |g" kdesrc-conf-android/kdesrc-buildrc
	sed	-E -i "s|source-dir.*|source-dir ${kf5_android_path}/kde/src |g" kdesrc-conf-android/kdesrc-buildrc
	sed -E -i "s|kdedir.*|kdedir ${kf5_android_path}/kde/install/${android_architecture} |g" kdesrc-conf-android/kdesrc-buildrc
	sed -E -i "s|-DCMAKE_TOOLCHAIN_FILE=.*?\\ |-DCMAKE_TOOLCHAIN_FILE=${kstars_DIR}/build_kstarslite/android_libs_src/AndroidToolchain.cmake |g" kdesrc-conf-android/kdesrc-buildrc

	if [ -e $qt_android_libs ]
	then
		sed -E -i "s|-DCMAKE_PREFIX_PATH=.*?\\ |-DCMAKE_PREFIX_PATH=${qt_android_libs} |g" kdesrc-conf-android/kdesrc-buildrc
	else
		echo "Qt Android libraries path doesn't exist. Exiting."
		exit
	fi

	sed -E -i "s|use-modules.+|use-modules kconfig ki18n kplotting|g" kdesrc-conf-android/kdesrc-buildrc
	rm -rf ${kf5_android_path}/kde/build/${android_architecture}/* # clean build folders
	./kdesrc-build extra-cmake-modules libintl-lite frameworks-android
fi

#Create separate directory for all libraries built for this architecture
mkdir "${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}" -p

if [ "$build_cfitsio" = "Y" ] || [ "$build_cfitsio" = "y" ] || [ "$build_cfitsio" = "Yes" ] || [ "$build_cfitsio" = "yes" ]
then
	# Build CFITSIO
	cd "${kstars_DIR}/build_kstarslite/android_libs_src/cfitsio"
	make distclean
	./build.sh "${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/"
	mv libcfitsio.a "${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/"
	make distclean
fi

# Build libnova
if [ "$build_nova" = "Y" ] || [ "$build_nova" = "y" ] || [ "$build_nova" = "Yes" ] || [ "$build_nova" = "yes" ]
then
	mkdir "${kstars_DIR}/build_kstarslite/android_libs_src/libnova/build" -p
	cd "${kstars_DIR}/build_kstarslite/android_libs_src/libnova/build"
	make clean
	cmake ../ -DCMAKE_TOOLCHAIN_FILE="${kstars_DIR}/build_kstarslite/android_libs_src/AndroidToolchain.cmake" -DBUILD_SHARED_LIBRARY=OFF
	make
	mv lib/liblibnova.a "${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/"
	make clean
fi

# Download INDI
if [ "$download_indi" = "Y" ] || [ "$download_indi" = "y" ] || [ "$download_indi" = "Yes" ] || [ "$download_indi" = "yes" ]
then	
		mkdir $download_indi_path -p
		cd $download_indi_path
		echo $PWD
		git clone https://github.com/indilib/indi.git
		# Update indi location in build-config file
		sed -E -i "s|indi_location.*|indi_location=\"$download_indi_path\" # Set INDI source location (location of libindi with CMakeLists.txt in it)|g" ${kstars_DIR}/build_kstarslite/build-config
		indi_location=$download_indi_path		
fi

# Build INDI
if [ "$build_indi" = "Y" ] || [ "$build_indi" = "y" ] || [ "$build_indi" = "Yes" ] || [ "$build_indi" = "yes" ]
then
	mkdir $indi_location/indi/build-android -p
	cd $indi_location/indi/build-android
	rm CMakeCache.txt
	make clean
	cmake $indi_location/indi/libindi -DCMAKE_TOOLCHAIN_FILE="${kstars_DIR}/build_kstarslite/android_libs_src/AndroidToolchain.cmake" \
		-DCFITSIO_LIBRARIES="${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/libcfitsio.a" \
		-DCFITSIO_INCLUDE_DIR="${kstars_DIR}/build_kstarslite/include" \
		-DNOVA_LIBRARIES="${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/liblibnova.a" \
		-DCMAKE_PREFIX_PATH="${Qt5_android}"
	make
	mv libindi.a "${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/"
	mv libindiclientqt.a "${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/"
	rm CMakeCache.txt
	make clean
	cd "${kstars_DIR}/build_kstarslite/android_libs/${ANDROID_ARCHITECTURE}"
	mv libindi.a _libindi.a
	
	# Combine libindi and liblibnova
ar -M <<EOM
    CREATE libindi.a
    ADDLIB liblibnova.a
    ADDLIB _libindi.a
    SAVE
    END
EOM
ranlib libindi.a
rm _libindi.a

fi

#Build LibRAW
if [ "$build_libraw" = "Y" ] || [ "$build_libraw" = "y" ] || [ "$build_libraw" = "Yes" ] || [ "$build_libraw" = "yes" ]
then
	cd "${kstars_DIR}/build_kstarslite/android_libs_src/libraw/Android/jni/"
	sed -E -i "s|APP_PLATFORM.*|APP_PLATFORM := android-${ANDROID_API_LEVEL} |g" Application.mk
	sed -E -i "s|APP_ABI.*|APP_ABI := ${ANDROID_ABI} |g" Application.mk
	${ANDROID_NDK}/./ndk-build
	mv ../libs/${ANDROID_ABI}/libRAWExtractor.so "${kstars_DIR}/build_kstarslite/android_libs/${android_architecture}/"
	rm -rf ../libs/
	rm -rf ../obj/
fi

#Build KStars Lite
mkdir ${build_dir} -p
rm "${build_dir}/build/CMakeCache.txt"
mkdir "${build_dir}/build" -p
cd "${build_dir}/build"
make clean

rm -rf "${build_dir}/export"
mkdir "${build_dir}/export" -p

cmake "${kstars_DIR}" -DCMAKE_TOOLCHAIN_FILE="${kstars_DIR}/build_kstarslite/android_libs_src/AndroidToolchain.cmake" \
	-DANDROID_ARCHITECTURE=${ANDROID_ARCHITECTURE} \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_PREFIX_PATH=${qt_android_libs} \
	-DCMAKE_INSTALL_PREFIX="${build_dir}/export" \
	-DQTANDROID_EXPORTED_TARGET=kstars \
	-DANDROID_APK_DIR="${kstars_DIR}"/apk \
	-DBUILD_KSTARS_LITE=ON \
	-DKF5Config_DIR="${kf5_android_path}/kde/install/${android_architecture}/lib/cmake/KF5Config" \
	-DKF5Plotting_DIR="${kf5_android_path}/kde/install/${android_architecture}/lib/cmake/KF5Plotting" \
	-DKF5I18n_DIR="${kf5_android_path}/kde/install/${android_architecture}/lib/cmake/KF5I18n" \
	-DKF5_HOST_TOOLING="${kf5_host_tooling}" \

make install
make create-apk-kstars

echo "If you want to install KStars Lite to your Android device, connect it to your PC (only one Android device at a time) and enter Y/Yes. 
Otherwise the APK file is ${build_dir}/build/kstars_build_apk/bin/QtApp-debug.apk"
read user_response
if [ "$user_response" = "Y" ] || [ "$user_response" = "y" ] || [ "$user_response" = "Yes" ] || [ "$user_response" = "yes" ]
then
	adb install -r kstars_build_apk/bin/QtApp-debug.apk	
fi