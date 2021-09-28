// file      : libbutl/pager.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <vector>
#include <iostream>

#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Try to run the output through a pager program, such as more or less (no
  // pun intended, less is used by default). If the default pager program is
  // used, then the output is indented so that 80-character long lines will
  // appear centered in the terminal. If the default pager program fails to
  // start, then the output is sent directly to STDOUT.
  //
  // If the pager program is specified and is empty, then no pager is used
  // and the output is sent directly to STDOUT.
  //
  // Throw std::system_error if there are problems with the pager program.
  //
  // Typical usage:
  //
  // try
  // {
  //   pager p ("help for foo");
  //   ostream& os (p.stream ());
  //
  //   os << "Foo is such and so ...";
  //
  //   if (!p.wait ())
  //     ... // Pager program returned non-zero status.
  // }
  // catch (const std::system_error& e)
  // {
  //   cerr << "pager error: " << e << endl;
  // }
  //
  class LIBBUTL_SYMEXPORT pager: protected std::streambuf
  {
  public:
    ~pager () {wait (true);}

    // If verbose is true, then print (to STDERR) the pager command line.
    //
    pager (const std::string& name,
           bool verbose = false,
           const std::string* pager = nullptr,
           const std::vector<std::string>* pager_options = nullptr);

    std::ostream&
    stream () {return os_.is_open () ? os_ : std::cout;}

    bool
    wait (bool ignore_errors = false);

    // The streambuf output interface that implements indentation. You can
    // override it to implement custom output pre-processing.
    //
  protected:
    using int_type = std::streambuf::int_type;
    using traits_type = std::streambuf::traits_type;

    virtual int_type
    overflow (int_type);

    virtual int
    sync ();

  private:
    process p_;
    ofdstream os_;

    std::string indent_;
    int_type prev_ = '\n'; // Previous character.
    std::streambuf* buf_ = nullptr;
  };
}
