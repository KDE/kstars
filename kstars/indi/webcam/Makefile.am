if LINUX
   libwebcam_linux = libwebcam_linux.la
endif

noinst_LTLIBRARIES = libwebcam.la $(libwebcam_linux)

libwebcam_linux_la_SOURCES = PPort.cpp port.cpp QCamV4L.cpp philipsV4L.cpp ccvt_c2.c ccvt_misc.c

libwebcam_la_SOURCES = empty_file.cpp
libwebcam_la_LIBADD = $(libwebcam_linux)

empty_file.cpp:
	echo > empty_file.cpp
