INCLUDES = 
METASOURCES = AUTO

if HAVE_LIBUSB
libapogee_target = libapogee_USB.la
endif

if LINUX
apogee_targets   = libapogee_ISA.la libapogee_PCI.la libapogee_PPI.la $(libapogee_target)
endif

noinst_LTLIBRARIES = $(apogee_targets)

libapogee_ISA_la_SOURCES = CameraIO_Linux.cpp CameraIO_LinuxISA.cpp
libapogee_PCI_la_SOURCES = CameraIO_Linux.cpp CameraIO_LinuxPCI.cpp
libapogee_PPI_la_SOURCES = CameraIO_Linux.cpp CameraIO_LinuxPPI.cpp

libapogee_USB_la_SOURCES = ApnCamData.cpp ApnCamData_CCD3011HS.cpp ApnCamData_CCD3011LS.cpp ApnCamData_CCD4240HS.cpp  ApnCamData_CCD4240LS.cpp ApnCamData_CCD4710HS.cpp ApnCamData_CCD4710LS.cpp ApnCamData_CCD4710LS2.cpp ApnCamData_CCD4710LS3.cpp  ApnCamData_CCD4710LS4.cpp ApnCamData_CCD4710LS5.cpp ApnCamData_CCD4720HS.cpp ApnCamData_CCD4720LS.cpp ApnCamData_CCD5520HS.cpp ApnCamData_CCD5520LS.cpp ApnCamData_CCD5710HS.cpp ApnCamData_CCD5710LS.cpp ApnCamData_CCD7700HS.cpp ApnCamData_CCD7700LS.cpp ApnCamData_KAF0261E.cpp ApnCamData_KAF0401E.cpp ApnCamData_KAF1001E.cpp ApnCamData_KAF1301E.cpp ApnCamData_KAF1401E.cpp ApnCamData_KAF3200E.cpp ApnCamData_KAF4202.cpp ApnCamData_KAF1602E.cpp ApnCamData_KAF16801E.cpp  ApnCamData_KAF6303E.cpp ApnCamData_TH7899.cpp ApnCamTable.cpp ApnCamera.cpp ApnCamera_USB.cpp ApnCamera_Linux.cpp ApogeeUsbLinux.cpp
libapogee_USB_la_LADD = -lusb

KDE_OPTIONS = nofinal
KDE_CXXFLAGS = -D_POSIX_C_SOURCE=199506L
