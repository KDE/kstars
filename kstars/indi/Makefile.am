bin_PROGRAMS = indiserver lx200generic celestrongps fliccd

liblilxml_a_SOURCES = lilxml.c
libindicom_a_SOURCES = indicom.c fitsrw.c

noinst_LIBRARIES =  liblilxml.a libindicom.a

indiserver_SOURCES = indiserver.c
indiserver_LDADD = liblilxml.a $(LIBSOCKET)

lx200generic_SOURCES = indidrivermain.c eventloop.c lx200driver.c lx200autostar.cpp lx200_16.cpp lx200gps.cpp lx200generic.cpp lx200classic.cpp
lx200generic_LDADD = liblilxml.a libindicom.a

celestrongps_SOURCES =  indidrivermain.c eventloop.c celestronprotocol.c celestrongps.cpp
celestrongps_LDADD = liblilxml.a libindicom.a

fliccd_SOURCES =  eventloop.c fli_ccd.c indidrivermain.c
fliccd_LDADD = fli/libfli.a libindicom.a liblilxml.a -lm

appdatadir = $(kde_datadir)/kstars
appdata_DATA = drivers.xml

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
	chmod +s $(DESTDIR)$(bindir)/fliccd

uninstall-local:
	rm $(DESTDIR)$(bindir)/lx200autostar $(DESTDIR)$(bindir)/lx200_16 $(DESTDIR)$(bindir)/lx200gps

KDE_OPTIONS = nofinal

SUBDIRS =  fli

