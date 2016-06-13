// file      : butl/path.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/path>

#ifdef _WIN32
#  include <stdlib.h>  // _MAX_PATH, _wgetenv()
#  include <direct.h>  // _[w]getcwd(), _[w]chdir()
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>  // GetTempPath*(), FormatMessageA(), LocalFree()
#  include <shlobj.h>   // SHGetFolderPath*(), CSIDL_PROFILE
#  include <winerror.h> // SUCCEEDED()
#  include <memory>     // unique_ptr
#else
#  include <pwd.h>       // struct passwd, getpwuid_r()
#  include <errno.h>     // EINVAL
#  include <stdlib.h>    // mbstowcs(), wcstombs(), realpath(), getenv()
#  include <limits.h>    // PATH_MAX
#  include <unistd.h>    // getcwd(), chdir()
#  include <string.h>    // strlen(), strcpy()
#  include <sys/stat.h>  // stat(), S_IS*
#  include <sys/types.h> // stat
#  include <vector>
#endif

#include <atomic>
#include <cassert>
#include <system_error>

#include <butl/process>

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
    if (_getcwd (cwd, _MAX_PATH) == 0)
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
    if (_chdir (s.c_str ()) != 0)
      throw system_error (errno, system_category ());
#else
    if (chdir (s.c_str ()) != 0)
      throw system_error (errno, system_category ());
#endif
  }

#ifdef _WIN32
  struct msg_deleter
  {
    void operator() (char* p) const {LocalFree (p);}
  };

  static string
  error_msg (DWORD e)
  {
    char* msg;
    if (!FormatMessageA (
          FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS |
          FORMAT_MESSAGE_MAX_WIDTH_MASK,
          0,
          e,
          MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
          (char*)&msg,
          0,
          0))
      return "unknown error code " + to_string (e);

    unique_ptr<char, msg_deleter> m (msg);
    return msg;
  }

  inline static string
  last_error ()
  {
    return error_msg (GetLastError ());
  }
#else
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
      throw system_error (errno, system_category ());

    if (!S_ISDIR (s.st_mode))
      throw system_error (ENOTDIR, system_category ());

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
      throw system_error (errno, system_category ());

    if (r == 0 && rpw == nullptr)
      // According to POSIX errno should be left unchanged if an entry is not
      // found.
      //
      throw system_error (ENOENT, system_category ());

    return pw.pw_dir;
  }
#endif

  template <>
  path_traits<char>::string_type path_traits<char>::
  temp_directory ()
  {
#ifdef _WIN32
    char d[_MAX_PATH + 1];
    DWORD r (GetTempPathA (_MAX_PATH + 1, d));

    if (r == 0)
    {
      string e (last_error ());
      throw system_error (ENOTDIR, system_category (), e);
    }

    return string_type (d);
#else
    return string_type (butl::temp_directory ());
#endif
  }

  static atomic<size_t> temp_name_count;

  template <>
  path_traits<char>::string_type path_traits<char>::
  temp_name (string_type const& prefix)
  {
    return prefix
      + "-" + to_string (process::current_id ())
      + "-" + to_string (temp_name_count++);
  }

  template <>
  path_traits<char>::string_type path_traits<char>::
  home ()
  {
#ifndef _WIN32
    return string_type (butl::home ());
#else
    // Could be set by, e.g., MSYS and Cygwin shells.
    //
    if (const char* h = getenv ("HOME"))
      return string_type (h);

    char h[_MAX_PATH];
    HRESULT r (SHGetFolderPathA (NULL, CSIDL_PROFILE, NULL, 0, h));

    if (!SUCCEEDED (r))
    {
      string e (error_msg (r));
      throw system_error (ENOTDIR, system_category (), e);
    }

    return string_type (h);
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
    if (_wgetcwd (wcwd, _MAX_PATH) == 0)
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
    if (_wchdir (s.c_str ()) != 0)
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

  template <>
  path_traits<wchar_t>::string_type path_traits<wchar_t>::
  temp_directory ()
  {
#ifdef _WIN32
    wchar_t d[_MAX_PATH + 1];
    DWORD r (GetTempPathW (_MAX_PATH + 1, d));

    if (r == 0)
    {
      string e (last_error ());
      throw system_error (ENOTDIR, system_category (), e);
    }
#else
    wchar_t d[PATH_MAX];

    // The usage of mbstowcs() supposes the program's C-locale is set to the
    // proper locale before the call (can be done with setlocale(LC_ALL, "...")
    // call). Otherwise mbstowcs() fails with EILSEQ errno for non-ASCII
    // directory paths.
    //
    size_t r (mbstowcs (d, butl::temp_directory (), PATH_MAX));

    if (r == size_t (-1))
      throw system_error (EINVAL, system_category ());

    if (r == PATH_MAX)
      throw system_error (ENOTSUP, system_category ());
#endif

    return string_type (d);
  }

  template <>
  path_traits<wchar_t>::string_type path_traits<wchar_t>::
  temp_name (string_type const& prefix)
  {
    return prefix +
      L"-" + to_wstring (process::current_id ()) +
      L"-" + to_wstring (temp_name_count++);
  }

  template <>
  path_traits<wchar_t>::string_type path_traits<wchar_t>::
  home ()
  {
#ifndef _WIN32
    wchar_t d[PATH_MAX];
    size_t r (mbstowcs (d, butl::home ().c_str (), PATH_MAX));

    if (r == size_t (-1))
      throw system_error (EINVAL, system_category ());

    if (r == PATH_MAX)
      throw system_error (ENOTSUP, system_category ());

    return string_type (d);
#else
    // Could be set by, e.g., MSYS and Cygwin shells.
    //
    if (const wchar_t* h = _wgetenv (L"HOME"))
      return string_type (h);

    wchar_t h[_MAX_PATH];
    HRESULT r (SHGetFolderPathW (NULL, CSIDL_PROFILE, NULL, 0, h));

    if (!SUCCEEDED (r))
    {
      string e (error_msg (r));
      throw system_error (ENOTDIR, system_category (), e);
    }

    return string_type (h);
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
