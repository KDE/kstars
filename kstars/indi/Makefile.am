appdatadir = $(kde_datadir)/kstars
appdata_DATA = drivers.xml apogee_caminfo.xml


AM_CPPFLAGS = -DTOP_DATADIR=\"$(appdatadir)\"

if LINUX
bin_add   =  apogee_ppi sbigccd v4ldriver v4lphilips meade_lpi
endif

bin_PROGRAMS = indiserver lx200basic lx200generic celestrongps apmount fliccd fliwheel temma skycommander $(bin_add)

liblilxml_a_SOURCES = lilxml.c
libindicom_a_SOURCES = indicom.c fitsrw.c base64.c

noinst_LIBRARIES =  liblilxml.a libindicom.a

indiserver_SOURCES = indiserver.c fq.c
indiserver_LDADD = liblilxml.a ${LIBPTHREAD} ${USE_THREADS} $(LIBSOCKET)

lx200generic_SOURCES = indidrivermain.c base64.c eventloop.c lx200driver.c lx200autostar.cpp lx200_16.cpp lx200gps.cpp lx200generic.cpp lx200classic.cpp
lx200generic_LDADD = liblilxml.a libindicom.a -lm

lx200basic_SOURCES = indidrivermain.c base64.c eventloop.c lx200driver.c lx200basic.cpp
lx200basic_LDADD = liblilxml.a libindicom.a

celestrongps_SOURCES =  indidrivermain.c base64.c eventloop.c celestronprotocol.c celestrongps.cpp
celestrongps_LDADD = liblilxml.a libindicom.a -lm

apmount_SOURCES = indidrivermain.c base64.c eventloop.c  lx200driver.c apmount.cpp
apmount_LDADD =  liblilxml.a libindicom.a

fliccd_SOURCES =  eventloop.c base64.c fli_ccd.c indidrivermain.c
fliccd_LDADD = fli/libfli.la libindicom.a liblilxml.a -lm -lz $(LIBSOCKET)

fliwheel_SOURCES =  eventloop.c base64.c fli_wheel.c indidrivermain.c
fliwheel_LDADD = fli/libfli.la libindicom.a liblilxml.a -lm

v4ldriver_SOURCES = eventloop.c base64.c indidrivermain.c v4ldriver.cpp indi_v4l.cpp
v4ldriver_LDADD = libindicom.a liblilxml.a webcam/libwebcam.la -lm -lz

v4lphilips_SOURCES = eventloop.c base64.c indidrivermain.c v4ldriver.cpp v4lphilips.cpp indi_philips.cpp
v4lphilips_LDADD = libindicom.a liblilxml.a webcam/libwebcam.la -lm -lz

meade_lpi_SOURCES = eventloop.c base64.c indidrivermain.c v4ldriver.cpp indi_lpi.cpp
meade_lpi_LDADD = libindicom.a liblilxml.a webcam/libwebcam.la -lz

temma_SOURCES =  indidrivermain.c base64.c eventloop.c temmadriver.c
temma_LDADD = liblilxml.a libindicom.a -lm

skycommander_SOURCES =  indidrivermain.c base64.c eventloop.c lx200driver.c skycommander.c
skycommander_LDADD = liblilxml.a libindicom.a -lm

apogee_ppi_SOURCES = apogee_ppi.cpp base64.c eventloop.c indidrivermain.c
apogee_ppi_LDADD = libindicom.a liblilxml.a apogee/libapogee_PPI.la -lz

sbigccd_SOURCES = sbigccd.cpp base64.c eventloop.c indidrivermain.c
sbigccd_LDADD = libindicom.a liblilxml.a -lz

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	rm -f $(DESTDIR)$(bindir)/lx200classic
	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200classic
	rm -f $(DESTDIR)$(bindir)/lx200autostar
	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200autostar
	rm -f $(DESTDIR)$(bindir)/lx200_16
	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200_16
	rm -f $(DESTDIR)$(bindir)/lx200gps
	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200gps

uninstall-local:
	rm $(DESTDIR)$(bindir)/lx200autostar $(DESTDIR)$(bindir)/lx200_16 $(DESTDIR)$(bindir)/lx200gps

KDE_OPTIONS = nofinal

if LINUX
SUBDIRS = fli webcam apogee
else
SUBDIRS = fli apogee
endif

include $(top_srcdir)/admin/Doxyfile.am
