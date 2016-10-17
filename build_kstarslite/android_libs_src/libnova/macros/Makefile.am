## Please update this variable if any new macros are created
MACROS=						\
	aclocal-include.m4				\
	libnova-check.m4

EXTRA_DIST=$(MACROS) autogen.sh
MAINTAINERCLEANFILES=macros.dep

install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) @LIBNOVA_MACRO_DIR@; \
	$(INSTALL_DATA) libnova-check.m4 $(LIBNOVA_MACRO_DIR); 

@MAINT@macros.dep: Makefile.am
@MAINT@	@echo '$$(top_srcdir)/aclocal.m4: $(MACROS:%=macros/%)' > $@

