SUBDIRS = backend m4

bin_SCRIPTS = ibus-fbterm

man_one_in_files = ibus-fbterm.1.in
man_one_files = $(man_one_in_files:.1.in=.1)
man_one_DATA =$(man_one_files:.1=.1.gz)
man_onedir = $(mandir)/man1
%.1: %.1.in
	$(AM_V_GEN) sed \
	    -e 's|@VERSION[@]|$(VERSION)|g' $< > $@.tmp && \
	    mv $@.tmp $@
%.1.gz: %.1
	$(AM_V_GEN) gzip -c $< > $@.tmp && mv $@.tmp $@

dist-hook:
	if test -d .git ; then \
	    git log --name-status --date=iso > $(distdir)/ChangeLog ; \
	    PREV_MICRO_VERSION=`expr $(PACKAGE_VERSION_MICRO) - 1`; \
	    PREV_VERSION=\
"$(PACKAGE_VERSION_MAJOR).$(PACKAGE_VERSION_MINOR).$$PREV_MICRO_VERSION"; \
	    echo "Changes in $(PACKAGE_NAME) $(PACKAGE_VERSION)" \
	        > $(distdir)/NEWS; \
	    echo "" >> $(distdir)/NEWS; \
	    git log $$PREV_VERSION...$(PACKAGE_VERSION) --reverse \
	        --pretty=format:'%s (%an) %h' >> $(distdir)/NEWS; \
	fi

EXTRA_DIST = \
    $(man_one_in_files) \
    ibus-fbterm \
    ibus-fbterm.spec.in \
    $(NULL)

CLEANFILES = \
    $(man_one_DATA) \
    $(man_one_files) \
    $(NULL)
