AC_INIT([Real Time Benchmark], 
        [0.1],
        [martina.maggio@control.lth.se], 
        [rt-bench],
        [http://www.control.lth.se])

AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([1.10 -Wall no-define foreign])
m4_ifdef([AM_PROG_AR],[AM_PROG_AR])

AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_RANLIB

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_CHECK_LIB([pthread], [pthread_mutex_init], [], [
              echo "ERROR: pthread library is missing"
              exit -1])
AC_CHECK_LIB([json-c], [json_object_get_int], [], [
              echo "ERROR: json library is missing"
              exit -1])
AC_CHECK_LIB([m], [round])
AC_CHECK_LIB([rt], [clock_gettime])

#AC_CHECK_FUNCS(shmat)
