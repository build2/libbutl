// file      : libbutl/win32-utility.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

// Use this header to include <windows.h> and a couple of Win32-specific
// utilities.
//

#ifdef _WIN32

// Try to include <windows.h> so that it doesn't mess other things up.
//
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX // No min and max macros.
#    define NOMINMAX
#    include <windows.h>
#    undef NOMINMAX
#  else
#    include <windows.h>
#  endif
#  undef WIN32_LEAN_AND_MEAN
#else
#  ifndef NOMINMAX
#    define NOMINMAX
#    include <windows.h>
#    undef NOMINMAX
#  else
#    include <windows.h>
#  endif
#endif

#include <string>

#include <libbutl/export.hxx>

namespace butl
{
  namespace win32
  {
    // RAII type for handles. Note that failure to close the handle is
    // silently ignored by both the destructor and reset().
    //
    // The handle can be INVALID_HANDLE_VALUE. Such a handle is treated as
    // unopened and is not closed.
    //
    struct nullhandle_t
    {
      constexpr explicit nullhandle_t (HANDLE) {}
      operator HANDLE () const {return INVALID_HANDLE_VALUE;}
    };

    LIBBUTL_SYMEXPORT extern const nullhandle_t nullhandle;

    class LIBBUTL_SYMEXPORT auto_handle
    {
    public:
      auto_handle (nullhandle_t = nullhandle) noexcept
        : handle_ (INVALID_HANDLE_VALUE) {}

      explicit
      auto_handle (HANDLE h) noexcept: handle_ (h) {}

      auto_handle (auto_handle&& h) noexcept: handle_ (h.release ()) {}
      auto_handle& operator= (auto_handle&&) noexcept;

      auto_handle (const auto_handle&) = delete;
      auto_handle& operator= (const auto_handle&) = delete;

      ~auto_handle () noexcept;

      HANDLE
      get () const noexcept {return handle_;}

      void
      reset (HANDLE h = INVALID_HANDLE_VALUE) noexcept;

      HANDLE
      release () noexcept
      {
        HANDLE r (handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return r;
      }

      // Close an open handle. Throw std::system_error on the underlying OS
      // error. Reset the descriptor to INVALID_HANDLE_VALUE whether the
      // exception is thrown or not.
      //
      void
      close ();

    private:
      HANDLE handle_;
    };

    inline bool
    operator== (const auto_handle& x, const auto_handle& y)
    {
      return x.get () == y.get ();
    }

    inline bool
    operator!= (const auto_handle& x, const auto_handle& y)
    {
      return !(x == y);
    }

    inline bool
    operator== (const auto_handle& x, nullhandle_t)
    {
      return x.get () == INVALID_HANDLE_VALUE;
    }

    inline bool
    operator!= (const auto_handle& x, nullhandle_t y)
    {
      return !(x == y);
    }

    LIBBUTL_SYMEXPORT std::string
    error_msg (DWORD code);

    LIBBUTL_SYMEXPORT std::string
    last_error_msg ();
  }
};

#include <libbutl/win32-utility.ixx>
#endif // _WIN32
