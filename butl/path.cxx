// file      : butl/path.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/path>

#ifdef _WIN32
#  include <stdlib.h> // _MAX_PATH
#  include <direct.h> // _[w]getcwd(), _[w]chdir()
#else
#  include <errno.h>  // EINVAL
#  include <stdlib.h> // mbstowcs(), wcstombs(), realpath()
#  include <limits.h> // PATH_MAX
#  include <unistd.h> // getcwd(), chdir()
#endif

#include <cassert>
#include <system_error>

#ifndef _WIN32
#  ifndef PATH_MAX
#    define PATH_MAX 4096
#  endif
#endif

using namespace std;

namespace butl
{
  char const* invalid_path_base::
  what () const throw ()
  {
    return "invalid filesystem path";
  }

  //
  // char
  //

  template <>
  path_traits<char>::string_type path_traits<char>::
  current ()
  {
    // @@ throw system_error (and in the other current() versions).

#ifdef _WIN32
    char cwd[_MAX_PATH];
    if(_getcwd (cwd, _MAX_PATH) == 0)
      throw system_error (errno, system_category ());
#else
    char cwd[PATH_MAX];
    if (getcwd (cwd, PATH_MAX) == 0)
      throw system_error (errno, system_category ());
#endif

    return string_type (cwd);
  }

  template <>
  void path_traits<char>::
  current (string_type const& s)
  {
#ifdef _WIN32
    if(_chdir (s.c_str ()) != 0)
      throw system_error (errno, system_category ());
#else
    if (chdir (s.c_str ()) != 0)
      throw system_error (errno, system_category ());
#endif
  }

#ifndef _WIN32
  template <>
  void path_traits<char>::
  realize (string_type& s)
  {
    char r[PATH_MAX];
    if (realpath (s.c_str (), r) == nullptr)
    {
      // @@ If there were a message in invalid_basic_path, we could have said
      // what exactly is wrong with the path.
      //
      if (errno == EACCES || errno == ENOENT || errno == ENOTDIR)
        throw invalid_basic_path<char> (s);
      else
        throw system_error (errno, system_category ());
    }

    s = r;
  }
#endif

  //
  // wchar_t
  //

  template <>
  path_traits<wchar_t>::string_type path_traits<wchar_t>::
  current ()
  {
#ifdef _WIN32
    wchar_t wcwd[_MAX_PATH];
    if(_wgetcwd (wcwd, _MAX_PATH) == 0)
      throw system_error (errno, system_category ());
#else
    char cwd[PATH_MAX];
    if (getcwd (cwd, PATH_MAX) == 0)
      throw system_error (errno, system_category ());

    wchar_t wcwd[PATH_MAX];
    if (mbstowcs (wcwd, cwd, PATH_MAX) == size_type (-1))
      throw system_error (EINVAL, system_category ());
#endif

    return string_type (wcwd);
  }

  template <>
  void path_traits<wchar_t>::
  current (string_type const& s)
  {
#ifdef _WIN32
    if(_wchdir (s.c_str ()) != 0)
      throw system_error (errno, system_category ());
#else
    char ns[PATH_MAX + 1];

    if (wcstombs (ns, s.c_str (), PATH_MAX) == size_type (-1))
      throw system_error (EINVAL, system_category ());

    ns[PATH_MAX] = '\0';

    if (chdir (ns) != 0)
      throw system_error (errno, system_category ());
#endif
  }

#ifndef _WIN32
  template <>
  void path_traits<wchar_t>::
  realize (string_type&)
  {
    assert (false); // Implement if/when needed.
  }
#endif
}
