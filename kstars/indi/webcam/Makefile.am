if HAVE_V4L2
   libwebcam_linux = libwebcam_v4l2_linux.la
else
   libwebcam_linux = libwebcam_v4l1_linux.la
endif

noinst_LTLIBRARIES = libwebcam.la $(libwebcam_linux)

libwebcam_v4l1_linux_la_SOURCES = PPort.cpp port.cpp v4l1_base.cpp v4l1_pwc.cpp ccvt_c2.c ccvt_misc.c
libwebcam_v4l2_linux_la_SOURCES = PPort.cpp port.cpp v4l2_base.cpp ccvt_c2.c ccvt_misc.c

libwebcam_la_SOURCES = empty_file.cpp
libwebcam_la_LIBADD = $(libwebcam_linux)

empty_file.cpp:
	echo > empty_file.cpp
