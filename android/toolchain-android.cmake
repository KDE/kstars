
SET(CMAKE_SYSTEM_NAME ${HOST_PREFIX})
SET(CMAKE_CROSSCOMPILING TRUE)

SET(ANDROID_NDK_ROOT $ENV{ANDROID_NDK})
SET(ANDROID_PLATFORM android-$ENV{ANDROID_API_LEVEL})

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)

SET(ANDROID ON)
SET(CMAKE_C_COMPILER_TARGET "armv7-linux-androideabi")
SET(CMAKE_C_COMPILER ${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin/clang)
SET(CMAKE_CXX_COMPILER_TARGET "armv7-linux-androideabi")
SET(CMAKE_CXX_COMPILER ${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++)
SET(CMAKE_AR ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ar CACHE FILEPATH "AR Fix")
SET(CMAKE_CR ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ar)
SET(CMAKE_RANLIB ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ranlib)
SET(CMAKE_STRIP ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-strip)
SET(CMAKE_READELF ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-readelf)

SET(CMAKE_SYSROOT ${ANDROID_NDK_ROOT}/sysroot)

SET(CC_LINKER ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc)
SET(CXX_LINKER ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-g++)

SET(CMAKE_C_CREATE_SHARED_LIBRARY "${CC_LINKER} <CMAKE_SHARED_LIBRARY_C_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
SET(CMAKE_CXX_CREATE_SHARED_LIBRARY "${CXX_LINKER} <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
SET(CMAKE_C_LINK_EXECUTABLE "${CC_LINKER} <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
SET(CMAKE_CXX_LINK_EXECUTABLE "${CXX_LINKER}  <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

# We need to use g++ for linking otherwise it fails with clang++
SET(CMAKE_AS ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-as)

SET(CMAKE_C_FLAGS_RELEASE -Os -gline-tables-only -fomit-frame-pointer -fno-strict-aliasing -DNDEBUG -DRELEASE -D_RELEASE)
SET(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
SET(CMAKE_C_FLAGS_DEBUG -O0 -gdwarf-2 -DDEBUG -D_DEBUG)
SET(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})

SET(CMAKE_C_FLAGS "-fPIE -mfloat-abi=soft -fsigned-char -ffunction-sections -fdata-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -fno-strict-aliasing")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DANDROID -D__ANDROID__ -D_FORTIFY_SOURCE=2 -fno-short-enums -Wa,--noexecstack -Wno-c++11-narrowing")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Werror=format-security -Wno-unknown-warning-option -Wno-extern-c-compat -Wno-deprecated-register")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --sysroot=${ANDROID_NDK_ROOT}/platforms/${ANDROID_PLATFORM}/arch-arm")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${ANDROID_NDK_ROOT}/platforms/${ANDROID_PLATFORM}/arch-arm/usr/include")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${ANDROID_NDK_ROOT}/sources/cxx-stl/gnu-libstdc++/4.9/include")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${ANDROID_NDK_ROOT}/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${ANDROID_NDK_ROOT}/sources/cxx-stl/gnu-libstdc++/4.9/include/backward")

INCLUDE_DIRECTORIES(${ANDROID_NDK_ROOT}/platforms/${ANDROID_PLATFORM}/arch-arm/usr/include)
INCLUDE_DIRECTORIES(${ANDROID_NDK_ROOT}/sources/cxx-stl/gnu-libstdc++/4.9/include)
INCLUDE_DIRECTORIES(${ANDROID_NDK_ROOT}/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include)
INCLUDE_DIRECTORIES(${ANDROID_NDK_ROOT}/sources/cxx-stl/gnu-libstdc++/4.9/include/backward)
INCLUDE_DIRECTORIES(${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/lib/gcc/arm-linux-androideabi/4.9.x/include)

SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -fexceptions -frtti -Wno-unknown-warning-option -Wno-unused-local-typedef")

SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} --sysroot=${ANDROID_NDK_ROOT}/platforms/${ANDROID_PLATFORM}/arch-arm")
SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -fPIE -L${ANDROID_NDK_ROOT}/platforms/${ANDROID_PLATFORM}/arch-arm/usr/lib -Wl,-rpath=${ANDROID_NDK_ROOT}/platforms/${ANDROID_PLATFORM}/arch-arm/usr/lib")
SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -L${ANDROID_NDK_ROOT}/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a -lgnustl_static")
SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -L${ANDROID_NDK_ROOT}/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a")
SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--no-undefined -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -march=armv7-a -Wl,--fix-cortex-a8 -Wl,--gc-sections")
SET(CMAKE_CXX_LINK_FLAGS ${CMAKE_C_LINK_FLAGS})
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS}")
