// file      : libbutl/diagnostics.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/diagnostics.hxx>

#ifndef _WIN32
#  include <unistd.h> // write()
#else
#  include <libbutl/win32-utility.hxx>
#  include <io.h> //_write()
#endif

#include <ios>      // ios::failure
#include <mutex>
#include <string>
#include <cassert>
#include <cstddef>  // size_t
#include <iostream> // cerr

#ifndef LIBBUTL_MINGW_STDTHREAD
#  include <mutex>
#else
#  include <libbutl/mingw-mutex.hxx>
#endif

#include <libbutl/ft/lang.hxx> // thread_local

#include <libbutl/utility.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>

using namespace std;

namespace butl
{
  ostream* diag_stream = &cerr;

#ifndef LIBBUTL_MINGW_STDTHREAD
  static std::mutex diag_mutex;
#else
  static mingw_stdthread::mutex diag_mutex;
#endif

  string diag_progress;
  static string diag_progress_blank; // Being printed blanks out the line.
  static size_t diag_progress_size;  // Size of the last printed progress.

  static optional<bool> diag_term;

  // Print the progress string to STDERR. Ignore underlying OS errors (this is
  // a progress bar after all, and throwing from dtors wouldn't be nice). Must
  // be called with the diag_mutex being aquired.
  //
  // Note that the output will not interleave with that of independent writers,
  // given that the printed strings are not longer than PIPE_BUF for POSIX
  // (which is at least 512 bytes on all platforms).
  //
  // @@ Is there any atomicity guarantees on Windows?
  //
  static inline void
  progress_print (string& s)
  {
    if (!diag_term)
    try
    {
      diag_term = fdterm (stderr_fd());
    }
    catch (const ios::failure&)
    {
      diag_term = false;
    }

    // If we print to a terminal, and the new progress string is shorter than
    // the printed one, then we will complement it with the required number of
    // spaces (to overwrite the trailing junk) prior to printing, and restore
    // it afterwards.
    //
    size_t n (s.size ());

    if (*diag_term && n < diag_progress_size)
      s.append (diag_progress_size - n, ' ');

    if (!s.empty ())
    {
      s += *diag_term
        ? '\r'  // Position the cursor at the beginning of the line.
        : '\n';

      try
      {
#ifndef _WIN32
        if (write (stderr_fd(), s.c_str (), s.size ())) {} // Suppress warning.
#else
        _write (stderr_fd(), s.c_str (), static_cast<unsigned int> (s.size ()));
#endif
      }
      catch (const ios::failure&) {}

      s.resize (n);           // Restore the progress string.
      diag_progress_size = n; // Save the size of the printed progress string.
    }
  }

  diag_stream_lock::diag_stream_lock ()
  {
    diag_mutex.lock ();

    // If diagnostics shares the output stream with the progress bar, then
    // blank out the line prior to printing the diagnostics, if required.
    //
    if (diag_stream == &cerr && diag_progress_size != 0)
      progress_print (diag_progress_blank);
  }

  diag_stream_lock::~diag_stream_lock ()
  {
    // If diagnostics shares output stream with the progress bar and we use
    // same-line progress style, then reprint the current progress string
    // that was overwritten with the diagnostics.
    //
    if (diag_stream == &cerr    &&
        !diag_progress.empty () &&
        diag_term               &&
        *diag_term)
      progress_print (diag_progress);

    diag_mutex.unlock ();
  }

  diag_progress_lock::diag_progress_lock ()
  {
    assert (diag_stream == &cerr);
    diag_mutex.lock ();
  }

  diag_progress_lock::~diag_progress_lock ()
  {
    progress_print (diag_progress);
    diag_mutex.unlock ();
  }

  static void
  default_writer (const diag_record& r)
  {
    r.os.put ('\n');

    diag_stream_lock l;
    (*diag_stream) << r.os.str ();

    // We can endup flushing the result of several writes. The last one may
    // possibly be incomplete, but that's not a problem as it will also be
    // followed by the flush() call.
    //
    diag_stream->flush ();
  }

  diag_writer* diag_record::writer = &default_writer;

  void diag_record::
  flush (void (*w) (const diag_record&)) const
  {
    if (!empty_)
    {
      if (epilogue_ == nullptr)
      {
        if (w != nullptr || (w = writer) != nullptr)
          w (*this);

        empty_ = true;
      }
      else
      {
        // Clear the epilogue in case it calls us back.
        //
        auto e (epilogue_);
        epilogue_ = nullptr;
        e (*this, w); // Can throw.
        flush (w);    // Call ourselves to write the data in case it returns.
      }
    }
  }

  diag_record::
  ~diag_record () noexcept (false)
  {
    // Don't flush the record if this destructor was called as part of the
    // stack unwinding.
    //
#ifdef __cpp_lib_uncaught_exceptions
    if (uncaught_ == std::uncaught_exceptions ())
      flush ();
#else
    // Fallback implementation. Right now this means we cannot use this
    // mechanism in destructors, which is not a big deal, except for one
    // place: exception_guard. Thus the ugly special check.
    //
    if (!std::uncaught_exception () || exception_unwinding_dtor ())
      flush ();
#endif
  }

  // Diagnostics stack.
  //
  static
#ifdef __cpp_thread_local
  thread_local
#else
  __thread
#endif
  const diag_frame* diag_frame_stack = nullptr;

  const diag_frame* diag_frame::
  stack () noexcept
  {
    return diag_frame_stack;
  }

  const diag_frame* diag_frame::
  stack (const diag_frame* f) noexcept
  {
    const diag_frame* r (diag_frame_stack);
    diag_frame_stack = f;
    return r;
  }
}
