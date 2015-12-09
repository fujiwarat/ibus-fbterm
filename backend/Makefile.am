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

ibus_fbterm_backend_SOURCES = \
        fbio.c \
        fbio.h \
        fbkey.c \
        fbkey.h \
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
        input_key.h \
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

-include $(top_srcdir)/git.mk
