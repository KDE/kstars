
KDE_LANG = en
KDE_DOCS = AUTO
docs-am: docs-ks index.cache.bz2

docs-ks:
	if test `echo $(VERSION) | cut -f 2 -d .` = 0; \
	then \
	cp -f index3.0.docbook index.docbook; \
	else \
	cp -f index3.1.docbook index.docbook; \
	fi;
