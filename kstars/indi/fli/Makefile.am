INCLUDES = $(all_includes)
METASOURCES = AUTO

if LINUX
libfli_linux   = libfli_linux.la
endif

if BSD
libfli_bsd     = libfli_bsd.la
endif

if NULL
libfli_null    = libfli_null.la
endif

noinst_LTLIBRARIES =  libfli.la $(libfli_linux) $(libfli_bsd) $(libfli_null)

libfli_linux_la_SOURCES = libfli-parport.c libfli-usb-sys-linux.c
libfli_bsd_la_SOURCES = libfli-usb-sys-bsd.c
libfli_null_la_SOURCES = libfli-usb-sys-null.c

libfli_la_SOURCES = libfli.c libfli-camera.c libfli-camera-parport.c libfli-camera-usb.c libfli-filter-focuser.c libfli-mem.c libfli-serial.c libfli-sys.c libfli-usb.c libfli-debug.c
AM_LDFLAGS = $(all_libraries)
libfli_la_LIBADD  = $(libfli_linux) $(libfli_bsd) $(libfli_null)
