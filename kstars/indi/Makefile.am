bin_PROGRAMS = indiserver lx200generic wx

liblilxml_a_SOURCES =  lilxml.c
noinst_LIBRARIES =  liblilxml.a

indiserver_SOURCES = indiserver.c
indiserver_LDADD = liblilxml.a

lx200generic_SOURCES =  indimain.c lx200_16.cpp lx200autostar.cpp lx200driver.c lx200generic.cpp lx200GPS.cpp
lx200generic_LDADD = liblilxml.a

wx_SOURCES =  indimain.c wx.c
wx_LDADD = liblilxml.a

appdatadir = $(kde_datadir)/kstars
appdata_DATA = drivers.xml

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)/bin
	-$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200autostar
	-$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200_16
	-$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200gps

uninstall-local:
	rm $(DESTDIR)$(bindir)/lx200autostar $(DESTDIR)$(bindir)lx200_16 $(DESTDIR)$(bindir)lx200gps
