noinst_LIBRARIES =  libwebcam.a

if LINUX
   INDI_COMPILE = PPort.cpp port.cpp vcvt_c.c QCamV4L.cpp ccvt_mmx_old.S
   INDI_FLAGS   = -DLINUX
endif

if BSD
   INDI_COMPILE = dummy.cpp
endif

if NULL
   INDI_COMPILE = dummy.cpp
endif

libwebcam_a_SOURCES = $(INDI_COMPILE)

AM_CXXFLAGS = $(INDI_FLAGS)
