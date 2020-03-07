// file      : libbutl/win32-utility.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  namespace win32
  {
    inline void auto_handle::
    reset (HANDLE h) noexcept
    {
      // Don't check for an error as not much we can do here.
      //
      if (handle_ != INVALID_HANDLE_VALUE)
        CloseHandle (handle_);

      handle_ = h;
    }

    inline auto_handle& auto_handle::
    operator= (auto_handle&& h) noexcept
    {
      reset (h.release ());
      return *this;
    }

    inline auto_handle::
    ~auto_handle () noexcept
    {
      reset ();
    }
  }
}
