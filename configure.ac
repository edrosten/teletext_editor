AC_INIT(teletext_Editor,1.0-dev)
AC_COPYRIGHT(Copyright Edward Rosten (2013--2015))


orig_CXXFLAGS="$CXXFLAGS"

AC_PROG_AWK
AC_PROG_CXX

AC_LANG(C++)

################################################################################
#
# Useful macros
#

dnl APPEND(var, value)
dnl This appends vale to a shell variable var
define(APPEND, [$1="$$1 $2"])

dnl CHECK_FOR_OPTION(option, code, [success], [fail])
dnl option corresponds to the first parameter for the corresponding
dnl AC_ARG_WITH. If the library is present, then have_option and CVD_HAVE_OPTION
dnl are set. If with_option is "", it will be set to yes.
dnl
dnl The test code shall do nothing on success, and set the shell variable a to 1
dnl on failure. For convenience, LIBS  and CXXFLAGS are checkpointed 
dnl automatically. On failure, they are restored automatically.
dnl
dnl On success, "option" is appended to options, and the code "success" is run
dnl On failure including --without) fail is run.
define(CHECK_FOR_OPTION,[
        if test "$have_$1" != yes
        then
                if test "$with_$1" == ""
                then
                        with_$1=yes
                fi

                save_LIBS="$LIBS"
                save_CXXFLAGS="$CXXFLAGS"
                a=
                $2
                
                if test x$a == x
                then
                        AC_DEFINE(CVD_HAVE_[]m4_toupper($1), 1)
                        AC_SUBST(have_$1, yes)
                        APPEND(options, $1)
                        $3
                else
                        LIBS="$save_LIBS"
                        CXXFLAGS="$save_CXXFLAGS"
                fi
        fi
        
        if test "$have_$1" != yes
        then
                :
                $4
        fi
        ])



dnl TEST_AND_SET_CXXFLAG(flag, [program], [run])
dnl
dnl This attempts to compile a and run program with a certain compiler flag.
dnl If no program is given, then the minimal C++ program is compiled, and 
dnl this tests just the validity of the compiler flag. 
dnl
define([TEST_AND_SET_CXXFLAG],[
	if test "$3" == ""
	then
		AC_MSG_CHECKING([if compiler flag $1 works])	
	else
		AC_MSG_CHECKING([$3])	
	fi
	save_CXXFLAGS="$CXXFLAGS"
	APPEND(CXXFLAGS, [$1])

	m4_if([$2],[],[pushdef(prog, [int main(){}])], [pushdef(prog, [$2])])

	m4_if([$4],[run],
		[AC_RUN_IFELSE([AC_LANG_SOURCE([prog])], [cvd_conf_test=1],[cvd_conf_test=0], [cvd_conf_test=0])],
		[AC_COMPILE_IFELSE([AC_LANG_SOURCE([prog])], [cvd_conf_test=1],[cvd_conf_test=0])]
	)
	
	
	popdef([prog])

	if test $cvd_conf_test = 1
	then
		AC_MSG_RESULT(yes)
		ts_success=yes
	else
		AC_MSG_RESULT(no)
		CXXFLAGS="$save_CXXFLAGS"
		ts_success=no
	fi
])


TEST_AND_SET_CXXFLAG(-Wall)
TEST_AND_SET_CXXFLAG(-Wextra)
TEST_AND_SET_CXXFLAG(-W)
TEST_AND_SET_CXXFLAG(-fPIC)

if ! echo $host | grep mingw
then
        TEST_AND_SET_CXXFLAG(-fPIC)
        if test $ts_success == no
        then
                AC_MSG_WARN(["I don't know how to compile position independent code. The Java plugin won't work"])
        fi
fi

################################################################################
#
# Check for FLTK
#

fltk=yes
AC_CHECK_HEADERS(FL/Fl.H, [], [fltk=no])
AC_CHECK_LIB(fltk, main, [], [fltk=no])

if test $fltk == no
then
	AC_MSG_ERROR([A working copy of FLTK-1 is needed])
fi

################################################################################
#
# Check for libcvd
#
cvd=yes
AC_CHECK_HEADERS(cvd/image_io.h, [], [cvd=no])


AC_CHECK_LIB(cvd, main)
cvd=no
cvdlib=no
test "$ac_cv_lib_cvd_main" == yes && cvdlib=yes

if test $cvdlib == yes
then
	AC_MSG_CHECKING(libCVD)
	cvd=no
	AC_LINK_IFELSE([AC_LANG_SOURCE([
		#include <cvd/fast_corner.h>
		#include <cvd/image_io.h>
		int main(){ CVD::Image<CVD::byte> i = CVD::img_load(""); return 0;}
	])], [cvd=yes], [cvd=no])
	AC_MSG_RESULT([$cvd])
fi


if test $cvdlib == yes && test $cvd == no
then
	AC_MSG_CHECKING(If libcvd needs -msse2)
	APPEND(CXXFLAGS,[ -msse2])

	AC_LINK_IFELSE([AC_LANG_SOURCE([
		#include <cvd/fast_corner.h>
		#include <cvd/image_io.h>
		int main(){ CVD::Image<CVD::byte> i = CVD::img_load(""); return 0;}
	])], [cvd=yes], [cvd=no])
	AC_MSG_RESULT($cvd)
fi

if test $cvd = no
then
	AC_MSG_ERROR([A working copy of libCVD is required for this program.
Get libCVD here: http://www.edwardrosten.com/cvd/
])
fi

AC_OUTPUT(Makefile)
