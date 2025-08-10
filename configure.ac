# Initialize Autoconf and define the project name, version, and bug report address.
AC_INIT([rpi-modswitch],[0.0.1],[work.kaliassistant.github@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([src/modswitchd.c])
AC_CONFIG_HEADERS([config.h])

# Check for a C compiler.
AC_PROG_CC
LT_INIT

# prevent libtool from adding -rpath
hardcode_libdir_flag_spec=
runpath_var=

# Check for the math library 'm'.
AC_SEARCH_LIBS([cos], [m])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN

# Define version macros to be used in C code.
AC_DEFINE_UNQUOTED([VERSION_MAJOR], [0], [Major version number])
AC_DEFINE_UNQUOTED([VERSION_MINOR], [0], [Minor version number])
AC_DEFINE_UNQUOTED([VERSION_MICRO], [1], [Micro version number])


# Create the output files.
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
