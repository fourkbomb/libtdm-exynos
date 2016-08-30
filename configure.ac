#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

AC_PREREQ(2.69)
AC_INIT(libtdm-android, 0.9.0)
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_CONFIG_HEADER([config.h])

AC_PROG_LIBTOOL
AC_PROG_CC

AC_HEADER_STDC

PKG_CHECK_MODULES(TDM_BACKEND_ANDROID, libtdm libtbm android-headers android-hw-libs)

# set the dir for the tdm module
DEFAULT_TDM_MODULE_PATH="${libdir}/tdm"
AC_ARG_WITH(tdm-module-path, AS_HELP_STRING([--with-tdm-module-path=PATH], [tdm module dir]),
				[ TDM_MODULE_PATH="$withval" ],
				[ TDM_MODULE_PATH="${DEFAULT_TDM_MODULE_PATH}" ])
AC_SUBST(TDM_MODULE_PATH)

# For enumerating devices in test case
AC_OUTPUT([
	Makefile
	src/Makefile])

echo ""
echo "$PACKAGE_STRING will be compiled with:"
echo ""
echo "CFLAGS                     : $CFLAGS"
echo "LDFLAGS                    : $LDFLAGS"
echo "TDM_BACKEND_ANDROID_CFLAGS : $TDM_BACKEND_ANDROID_CFLAGS"
echo "TDM_BACKEND_ANDROID_LIBS   : $TDM_BACKEND_ANDROID_LIBS"
echo "tdm_module_dir             : $TDM_MODULE_PATH"
echo ""
