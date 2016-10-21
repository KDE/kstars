LibRaw for Android NDK
===================
LibRaw is a library for reading RAW files from digital photo cameras 
(CRW/CR2, NEF, RAF, DNG, MOS, KDC, DCR, etc, virtually all RAW formats are 
supported). 

It pays special attention to correct retrieval of data required for subsequent 
RAW conversion.
    
The library is intended for embedding in RAW converters, data analyzers, and 
other programs using RAW files as the initial data.. 

This Android build already contains LCMS2 and libjpeg library dependencies, and was built with **-DUSE_LCMS2** and **-DUSE_JPEG**.



----------


How to build
-------------
Android NDK is required:
http://developer.android.com/intl/ru/ndk/downloads/index.html
```bash
git clone https://github.com/R-Tur/LibRaw-Android

cd LibRaw-Android/Android/jni

ndk-build
```

The resulting .so library will appear here: 

    LibRaw-Android/Android/libs

Writing a code
-------------------

Some useful example can be found at:

    Android/jni/!!!your_code_here!!!/example.cpp

Also, you should be familiar with JNI ("Java native interface") programming:

https://en.wikipedia.org/wiki/Java_Native_Interface

http://developer.android.com/training/articles/perf-jni.html

http://developer.android.com/ndk/samples/sample_hellojni.html


**Enjoy !**
