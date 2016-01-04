#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([ibus-fbterm], [1.0.0],
        [https://github.com/fujiwarat/ibus-fbterm/issues],
        [ibus-fbterm])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_VALAC([0.20])
PKG_PROG_PKG_CONFIG
                                          
# Checks for libraries.
AM_PATH_GLIB_2_0
PKG_CHECK_MODULES([GLIB2], [glib-2.0 >= 2.32.0])
PKG_CHECK_MODULES([IBUS], [ibus-1.0 >= 1.5.0])

AC_CONFIG_FILES([Makefile
backend/Makefile])
AC_OUTPUT
