SUBDIRS = backend

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
