
bin_PROGRAMS = indiserver lx200generic lx200autostar lx200_16 lx200gps wx

indiserver_SOURCES = indiserver.c
indiserver_LDADD = liblilxml.la

lx200generic_SOURCES =  indimain.c lx200_16.cpp lx200autostar.cpp lx200driver.c lx200generic.cpp lx200GPS.cpp
lx200generic_LDADD = liblilxml.la

lx200autostar_SOURCES =  indimain.c lx200_16.cpp lx200autostar.cpp lx200driver.c lx200generic.cpp lx200GPS.cpp
lx200autostar_LDADD = liblilxml.la

lx200_16_SOURCES = indimain.c lx200_16.cpp lx200autostar.cpp lx200driver.c lx200generic.cpp lx200GPS.cpp
lx200_16_LDADD = liblilxml.la

lx200gps_SOURCES = indimain.c lx200_16.cpp lx200autostar.cpp lx200driver.c lx200generic.cpp lx200GPS.cpp
lx200gps_LDADD = liblilxml.la

liblilxml_la_SOURCES = lilxml.c
noinst_LIBRARIES = liblilxml.la

wx_SOURCES =  indimain.c wx.c
wx_LDADD = liblilxml.la
