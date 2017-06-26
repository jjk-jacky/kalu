dnl Checks for PATH_MAX and defines it if not present
AC_DEFUN([PATH_MAX_DEFINED],
  [AC_CACHE_CHECK([PATH_MAX defined], path_max_cv_defined,
    [AC_EGREP_CPP(yes, [[
#include <limits.h>
#if defined(PATH_MAX)
yes
#endif
]],
      [path_max_cv_defined=yes],
      [path_max_cv_defined=no])]
  )
  if test $path_max_cv_defined = no; then
    AC_DEFINE([PATH_MAX], 4096, [Define if PATH_MAX is undefined by limits.h.])
  fi
])
