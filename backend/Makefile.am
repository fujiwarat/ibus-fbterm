# vim:set noet ts=4:
#
# Copyright (c) 2015 Takao Fujiwara <takao.fujiwara1@gmail.com>
# Copyright (c) 2015 Red Hat, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


libexec_PROGRAMS = ibus-fbterm-backend

marshalers_sources = \
        marshalers.c \
        marshalers.h \
        $(NULL)

ibus_fbterm_backend_SOURCES = \
        $(marshalers_sources) \
        fbio.c \
        fbio.h \
        fbshell.c \
        fbshell.h \
        fbshellman.c \
        fbshellman.h \
        fbterm.c \
        fbterm.h \
        fbtty.c \
        fbtty.h \
        ibusfbcontext.c \
        ibusfbcontext.h \
        $(NULL)

ibus_fbterm_backend_DEPENDENCIES = \
        $(NULL)

ibus_fbterm_backend_LDADD = \
        @GLIB2_LIBS@ \
        @IBUS_LIBS@ \
        -lutil \
        $(NULL)

ibus_fbterm_backend_CFLAGS = \
        @GLIB2_CFLAGS@ \
        @IBUS_CFLAGS@ \
        -I$(top_srcdir)/src \
        -I$(top_builddir)/src \
        $(NULL)

BUILT_SOURCES = \
        $(marshalers_sources) \
        $(NULL)

marshalers.h: marshalers.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_fb_marshal $(srcdir)/marshalers.list --header --internal > $@.tmp && \
	mv $@.tmp $@

marshalers.c: marshalers.h marshalers.list
	$(AM_V_GEN) (echo "#include \"marshalers.h\""; \
	$(GLIB_GENMARSHAL) --prefix=_fb_marshal $(srcdir)/marshalers.list --body --internal) > $@.tmp && \
	mv $@.tmp $@
