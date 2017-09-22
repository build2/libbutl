// file      : libbutl/path.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/path.mxx>
#endif

#ifdef _WIN32
#  include <libbutl/win32-utility.hxx>

#  include <io.h>       // _find*()
#  include <stdlib.h>   // _MAX_PATH
#  include <direct.h>   // _getcwd(), _chdir()
#  include <shlobj.h>   // SHGetFolderPath*(), CSIDL_PROFILE
#  include <winerror.h> // SUCCEEDED()
#else
#  include <pwd.h>       // struct passwd, getpwuid_r()
#  include <errno.h>     // EINVAL
#  include <stdlib.h>    // realpath(), getenv()
#  include <limits.h>    // PATH_MAX
#  include <unistd.h>    // getcwd(), chdir()
#  include <string.h>    // strlen(), strcpy()
#  include <sys/stat.h>  // stat(), S_IS*
#  include <sys/types.h> // stat
#endif

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>
#include <cstddef>
#include <utility>

#include <atomic>
#include <cstring> // strcpy()
#endif

#ifdef __cpp_modules
module butl.path;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
#endif
#endif

import butl.utility; // throw_*_error()
import butl.process; // process::current_id()
#else
#include <libbutl/utility.mxx>
#include <libbutl/process.mxx>
#endif

#include <libbutl/export.hxx>

#ifndef _WIN32
#  ifndef PATH_MAX
#    define PATH_MAX 4096
#  endif
#endif

using namespace std;

#ifdef _WIN32
using namespace butl::win32;
#endif

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
  LIBBUTL_SYMEXPORT path_traits<char>::string_type path_traits<char>::
  current_directory ()
  {
#ifdef _WIN32
    char cwd[_MAX_PATH];
    if (_getcwd (cwd, _MAX_PATH) == 0)
      throw_generic_error (errno);
    cwd[0] = toupper (cwd[0]); // Canonicalize.
#else
    char cwd[PATH_MAX];
    if (getcwd (cwd, PATH_MAX) == 0)
      throw_generic_error (errno);
#endif

    return cwd;
  }

  template <>
  LIBBUTL_SYMEXPORT void path_traits<char>::
  current_directory (string_type const& s)
  {
#ifdef _WIN32
    // A path like 'C:', while being a root path in our terminology, is not as
    // such for Windows, that maintains current directory for each drive, and
    // so "change current directory to C:" means "change the process current
    // directory to current directory on the C drive". Changing it to the root
    // one of the drive requires the trailing directory separator to be
    // present.
    //
    string_type const& d (
      !root (s)
      ? s
      //@@ MOD VC ADL does not seem to kick in for some reason...
      : string_type (std::operator+ (s, directory_separator)));

    if (_chdir (d.c_str ()) != 0)
      throw_generic_error (errno);
#else
    if (chdir (s.c_str ()) != 0)
      throw_generic_error (errno);
#endif
  }

#ifndef _WIN32
  static const char*
  temp_directory ()
  {
    const char* dir (nullptr);
    const char* env[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR", nullptr};

    for (auto e (env); dir == nullptr && *e != nullptr; ++e)
      dir = getenv (*e);

    if (dir == nullptr)
      dir = "/tmp";

    struct stat s;
    if (stat (dir, &s) != 0)
      throw_generic_error (errno);

    if (!S_ISDIR (s.st_mode))
      throw_generic_error (ENOTDIR);

    return dir;
  }

  static string
  home ()
  {
    if (const char* h = getenv ("HOME"))
      return h;

    // Struct passwd has 5 members that will use this buffer. Two are the
    // home directory and shell paths. The other three are the user login
    // name, password, and real name (comment). We expect them to fit into
    // PATH_MAX * 2.
    //
    char buf[PATH_MAX * 4];

    passwd pw;
    passwd* rpw;

    int r (getpwuid_r (getuid (), &pw, buf, sizeof (buf), &rpw));
    if (r == -1)
      throw_generic_error (errno);

    if (r == 0 && rpw == nullptr)
      // According to POSIX errno should be left unchanged if an entry is not
      // found.
      //
      throw_generic_error (ENOENT);

    return pw.pw_dir;
  }
#endif

  template <>
  LIBBUTL_SYMEXPORT path_traits<char>::string_type path_traits<char>::
  temp_directory ()
  {
#ifdef _WIN32
    char d[_MAX_PATH + 1];
    if (GetTempPathA (_MAX_PATH + 1, d) == 0)
      throw_system_error (GetLastError ());

    return d;
#else
    return butl::temp_directory ();
#endif
  }

  static atomic<size_t> temp_name_count (0);

  template <>
  LIBBUTL_SYMEXPORT path_traits<char>::string_type path_traits<char>::
  temp_name (string_type const& prefix)
  {
    // Otherwise compiler get confused with butl::to_string(timestamp).
    //
    using std::to_string;

    return prefix
      + "-" + to_string (process::current_id ())
      + "-" + to_string (temp_name_count++);
  }

  template <>
  LIBBUTL_SYMEXPORT path_traits<char>::string_type path_traits<char>::
  home_directory ()
  {
#ifndef _WIN32
    return home ();
#else
    // Could be set by, e.g., MSYS and Cygwin shells.
    //
    if (const char* h = getenv ("HOME"))
      return h;

    char h[_MAX_PATH];
    HRESULT r (SHGetFolderPathA (NULL, CSIDL_PROFILE, NULL, 0, h));

    if (!SUCCEEDED (r))
      throw_system_error (r);

    return h;
#endif
  }

#ifndef _WIN32
  template <>
  LIBBUTL_SYMEXPORT void path_traits<char>::
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
        throw_generic_error (errno);
    }

    s = r;
  }
#else
  template <>
  LIBBUTL_SYMEXPORT bool
  basic_path_append_actual_name<char> (string& r,
                                       const string& d,
                                       const string& n)
  {
    assert (d.size () + n.size () + 1 < _MAX_PATH);

    char p[_MAX_PATH];
    strcpy (p, d.c_str ());
    p[d.size ()] = '\\';
    strcpy (p + d.size () + 1, n.c_str ());

    // It could be that using FindFirstFile() is faster.
    //
    _finddata_t fi;
    intptr_t h (_findfirst (p, &fi));

    if (h == -1 && errno == ENOENT)
      return false;

    if (h == -1 || _findclose (h) == -1)
      throw_generic_error (errno);

    r += fi.name;
    return true;
  }
#endif
}
