if LINUX
   libwebcam_linux = libwebcam_linux.la
endif

if BSD
   libwebcam_bsd = libwebcam_null.la
endif

if NULL
   libwebcam_null = libwebcam_null.la
endif

noinst_LTLIBRARIES = libwebcam.la $(libwebcam_linux) $(libwebcam_bsd) $(libwebcam_null)

libwebcam_linux_la_SOURCES = PPort.cpp port.cpp vcvt_c.c QCamV4L.cpp philipsV4L.cpp ccvt_mmx_old.S
libwebcam_null_la_SOURCES = dummy.cpp

libwebcam_la_SOURCES = empty_file.cpp
libwebcam_la_LIBADD = $(libwebcam_linux) $(libwebcam_bsd) $(libwebcam_null)

empty_file.cpp:
	echo > empty_file.cpp
