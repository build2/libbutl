// file      : butl/diagnostics.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
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
      *diag_stream << os.str () << endl;
      empty_ = true;

      if (epilogue_ != nullptr)
        epilogue_ (*this); // Can throw.
    }
  }

  diag_record::
  ~diag_record () noexcept (false)
  {
    // Don't flush the record if this destructor was called as part of
    // the stack unwinding. Right now this means we cannot use this
    // mechanism in destructors, which is not a big deal, except for
    // one place: exception_guard. So for now we are going to have
    // this ugly special check which we will be able to get rid of
    // once C++17 uncaught_exceptions() becomes available.
    //
    if (!std::uncaught_exception () || exception_unwinding_dtor)
      flush ();
  }
}
