#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([ibus-fbterm], [0.99.0],
        [https://github.com/fujiwarat/ibus-fbterm/issues],
        [ibus-fbterm])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG
                                          
# Checks for libraries.
PKG_CHECK_MODULES([IBUS], [ibus-1.0 >= 1.5.0])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
