INCLUDES = 
METASOURCES = AUTO
noinst_LIBRARIES =  libfli.a

if LINUX
  linux_para = libfli-parport.c
  linux_usb  = libfli-usb-sys-linux.c
endif

if BSD
  bsd_usb    = libfli-usb-sys-bsd.c
endif

if NULL
  null_usb   = libfli-usb-sys-null.c
endif

libfli_a_SOURCES =  libfli.c libfli-camera.c libfli-camera-parport.c libfli-camera-usb.c libfli-filter-focuser.c libfli-mem.c libfli-serial.c libfli-sys.c libfli-usb.c libfli-debug.c $(linux_para) $(linux_usb) $(bsd_usb) $(null_usb)
