m4_include([libtool.m4])

AC_DEFUN([CHECK_SNET],
[
    AC_MSG_CHECKING(for snet)
    snetdir="libsnet"
    AC_ARG_WITH(snet,
	    AC_HELP_STRING([--with-snet=DIR], [path to snet]),
	    snetdir="$withval")
    if test -f "$snetdir/snet.h"; then
	found_snet="yes";
	CPPFLAGS="$CPPFLAGS -I$snetdir";
    fi
    if test x_$found_snet != x_yes; then
	AC_MSG_ERROR(cannot find snet libraries)
    else
	LIBS="$LIBS -lsnet";
	LDFLAGS="$LDFLAGS -L$snetdir";
    fi
    AC_MSG_RESULT(yes)
])

AC_DEFUN([CHECK_SSL],
[
    AC_MSG_CHECKING(for ssl)
    ssldirs="/usr/local/openssl /usr/lib/openssl /usr/openssl \
	    /usr/local/ssl /usr/lib/ssl /usr/ssl \
	    /usr/pkg /usr/local /usr"
    AC_ARG_WITH(ssl,
	    AC_HELP_STRING([--with-ssl=DIR], [path to ssl]),
	    ssldirs="$withval")
    for dir in $ssldirs; do
	ssldir="$dir"
	if test -f "$dir/include/openssl/ssl.h"; then
	    found_ssl="yes";
	    CPPFLAGS="$CPPFLAGS -I$ssldir/include";
	    break;
	fi
	if test -f "$dir/include/ssl.h"; then
	    found_ssl="yes";
	    CPPFLAGS="$CPPFLAGS -I$ssldir/include";
	    break
	fi
    done
    if test x_$found_ssl != x_yes; then
	AC_MSG_ERROR(cannot find ssl libraries)
    else
	AC_DEFINE(HAVE_LIBSSL)
	LIBS="$LIBS -lssl -lcrypto";
	LDFLAGS="$LDFLAGS -L$ssldir/lib";
    fi
    AC_MSG_RESULT(yes)
])

AC_DEFUN([CHECK_ZEROCONF],
[
    AC_MSG_CHECKING(for zeroconf)
    zeroconfdirs="/usr /usr/local"
    AC_ARG_WITH(zeroconf,
	    AC_HELP_STRING([--with-zeroconf=DIR], [path to zeroconf]),
	    zeroconfdirs="$withval")
    for dir in $zeroconfdirs; do
	zcdir="$dir"
	if test -f "$dir/include/DNSServiceDiscovery/DNSServiceDiscovery.h"; then
	    found_zeroconf="yes";
	    CPPFLAGS="$CPPFLAGS -I$zcdir/include";
	    break;
	fi
    done
    if test x_$found_zeroconf != x_yes; then
	AC_MSG_RESULT(no)
    else
	AC_DEFINE(HAVE_ZEROCONF)
	AC_MSG_RESULT(yes)
    fi
])

AC_DEFUN([CHECK_ZLIB],
[
    AC_MSG_CHECKING(for zlib)
    zlibdirs="/usr /usr/local"
	withval=""
    AC_ARG_WITH(zlib,
            [AC_HELP_STRING([--with-zlib=DIR], [path to zlib])],
            [])
    if test x_$withval != x_no; then
	if test x_$withval != x_yes -a \! -z "$withval"; then
		zlibdirs="$answer"
	fi
	for dir in $zlibdirs; do
	    zlibdir="$dir"
	    if test -f "$dir/include/zlib.h"; then
			found_zlib="yes";
			break;
		fi
	done
	if test x_$found_zlib == x_yes; then
		if test "$dir" != "/usr"; then
			CPPFLAGS="$CPPFLAGS -I$zlibdir/include";
	    fi
	    AC_DEFINE(HAVE_ZLIB)
	    AC_MSG_RESULT(yes)
	else
	    AC_MSG_RESULT(no)
	fi
    else
	AC_MSG_RESULT(no)
    fi
])

AC_DEFUN([CHECK_PROFILED],
[
    # Allow user to control whether or not profiled libraries are built
    AC_MSG_CHECKING(whether to build profiled libraries)
    PROFILED=true
    AC_ARG_ENABLE(profiled,
      [  --enable-profiled       build profiled libsnet (default=yes)],
      [test x_$enable_profiled = x_no && PROFILED=false]
    )
    AC_SUBST(PROFILED)
    if test x_$PROFILED = x_true ; then
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
    fi
])

AC_DEFUN([CHECK_SASL],
[
    withval=""
    AC_MSG_CHECKING(for sasl)
    sasldirs="/usr/local/sasl2 /usr/lib/sasl2 /usr/sasl2 \
            /usr/pkg /usr/local /usr"
    AC_ARG_WITH(sasl,
            AC_HELP_STRING([--with-sasl=DIR], [path to sasl]),
            sasldirs="$withval")
    if test x_$withval != x_no; then
	for dir in $sasldirs; do
	    sasldir="$dir"
	    if test -f "$dir/include/sasl/sasl.h"; then
		found_sasl="yes";
		CPPFLAGS="$CPPFLAGS -I$sasldir/include";
		break;
	    fi
	    if test -f "$dir/include/sasl.h"; then
		found_sasl="yes";
		CPPFLAGS="$CPPFLAGS -I$sasldir/include";
		break
	    fi
	done
	if test x_$found_sasl == x_yes; then
	    AC_DEFINE(HAVE_LIBSASL)
	    LIBS="$LIBS -lsasl2";
	    LDFLAGS="$LDFLAGS -L$sasldir/lib";
	    AC_MSG_RESULT(yes)
	else
	    AC_MSG_RESULT(no)
	fi
    else
	AC_MSG_RESULT(no)
    fi
])
