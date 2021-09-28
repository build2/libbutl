// file      : libbutl/win32-utility.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/win32-utility.hxx>

// Note that while we exclude it from the buildfile-based builds, we still
// need these guards for build2 bootstrap.
//
#ifdef _WIN32

#include <memory> // unique_ptr

#include <libbutl/utility.hxx> // throw_system_error()

using namespace std;

namespace butl
{
  namespace win32
  {
    const nullhandle_t nullhandle (INVALID_HANDLE_VALUE);

    void auto_handle::
    close ()
    {
      if (handle_ != INVALID_HANDLE_VALUE)
      {
        bool r (CloseHandle (handle_));

        // If CloseHandle() failed then no reason to expect it to succeed the
        // next time.
        //
        handle_ = INVALID_HANDLE_VALUE;

        if (!r)
          throw_system_error (GetLastError ());
      }
    }

    struct msg_deleter
    {
      void operator() (char* p) const {LocalFree (p);}
    };

    string
    error_msg (DWORD code)
    {
      char* msg;
      if (!FormatMessageA (
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS |
            FORMAT_MESSAGE_MAX_WIDTH_MASK,
            0,
            code,
            MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
            (char*)&msg,
            0,
            0))
        return "unknown error code " + std::to_string (code);

      unique_ptr<char, msg_deleter> m (msg);
      return msg;
    }

    string
    last_error_msg ()
    {
      return error_msg (GetLastError ());
    }
  }
}

#endif // _WIN32
