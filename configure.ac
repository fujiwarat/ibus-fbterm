#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([ibus-fbterm], [0.9.1])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/ibus-fbterm.c])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG
                                          
# Checks for libraries.
PKG_CHECK_MODULES([ibus], [ibus-1.0 >= 1.2.0])

AC_SUBST([IBUS_INCS], ["$ibus_CFLAGS"])
AC_SUBST([IBUS_LIBS], ["$ibus_LIBS"])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
