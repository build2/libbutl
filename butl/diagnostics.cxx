// file      : butl/diagnostics.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/diagnostics>

#include <iostream> // cerr

using namespace std;

namespace butl
{
  ostream* diag_stream = &cerr;

  void diag_record::
  flush () const
  {
    if (!empty_)
    {
      if (epilogue_ == nullptr)
      {
        os.put ('\n');
        *diag_stream << os.str ();
        diag_stream->flush ();

        empty_ = true;
      }
      else
      {
        // Clear the epilogue in case it calls us back.
        //
        auto e (epilogue_);
        epilogue_ = nullptr;
        e (*this); // Can throw.
        flush ();  // Call ourselves to write the data in case it returns.
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
}
