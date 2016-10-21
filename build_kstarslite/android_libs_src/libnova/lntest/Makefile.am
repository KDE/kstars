

noinst_PROGRAMS = \
	lntest 
	
bin_PROGRAMS = \
	libnovaconfig
	
lntest_SOURCES = \
	test.c 
	
libnovaconfig_SOURCES = \
	libnova_config.c 
	
		
AM_CPPFLAGS = \
	 -Wall -I$(top_srcdir)/src

lntest_LDADD = \
	../src/libnova.la
	
	
libnovaconfig_LDADD = \
	../src/libnova.la


