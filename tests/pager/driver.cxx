// file      : tests/pager/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <ios>      // ios_base::failure
#include <vector>
#include <string>
#include <utility>  // move()
#include <sstream>
#include <iostream>

#include <libbutl/pager.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

int
main (int argc, const char* argv[])
{
  bool child (false);
  bool interactive (false);
  string pgprog;
  vector<string> pgopts;

  assert (argc > 0);

  int i (1);
  for (; i != argc; ++i)
  {
    string v (argv[i]);
    if (pgprog.empty ())
    {
      if (v == "-c")
        child = true;
      else if (v == "-i")
        interactive = true;
      else
      {
        pgprog = move (v);
        interactive = true;
      }
    }
    else
      pgopts.emplace_back (move (v));
  }

  if (i != argc)
  {
    if (!child)
      cerr << "usage: " << argv[0] << " [-c] [-i] [<pager> [<options>]]"
           << endl;

    return 1;
  }

  const char* s (R"delim(
class fdstream_base
{
protected:
  fdstream_base () = default;
  fdstream_base (int fd): buf_ (fd) {}

protected:
  fdbuf buf_;
};

class ifdstream: fdstream_base, public std::istream
{
public:
  ifdstream (): std::istream (&buf_) {}
  ifdstream (int fd): fdstream_base (fd), std::istream (&buf_) {}

  void close () {buf_.close ();}
  void open (int fd) {buf_.open (fd);}
  bool is_open () const {return buf_.is_open ();}
};
)delim");

  if (child)
  {
    string il;
    string ol;
    istringstream is (s);
    do
    {
      getline (cin, il);
      getline (is, ol);
    }
    while (cin.good () && is.good () && il == ol);
    return cin.eof () && !cin.bad () && is.eof () ? 0 : 1;
  }

  try
  {
    string prog (argv[0]);
    vector<string> opts ({"-c"});

    pager p ("pager test",
             false,
             interactive ? (pgprog.empty () ? nullptr : &pgprog) : &prog,
             interactive ? (pgopts.empty () ? nullptr : &pgopts) : &opts);

    p.stream () << s;

    assert (p.wait ());
  }
  catch (const ios_base::failure&)
  {
    assert (false);
  }
  catch (const system_error&)
  {
    assert (false);
  }
}
