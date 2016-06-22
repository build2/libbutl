// file      : butl/win32-utility.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/win32-utility>

#ifdef _WIN32

#include <memory> // unique_ptr

using namespace std;

namespace butl
{
  namespace win32
  {
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
        return "unknown error code " + to_string (code);

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
