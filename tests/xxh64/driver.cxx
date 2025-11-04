// file      : tests/xxh64/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <cstddef> // size_t

#include <libbutl/path.hxx>
#include <libbutl/xxh64.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/filesystem.hxx> // auto_rmfile

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

int
main ()
{
  assert (string (xxh64 ().string ()) == "ef46db3751d8e999");
  assert (string (xxh64 ("").string ()) != "ef46db3751d8e999");
  assert (string (xxh64 ("123", 3).string ()) == "3c697d223fa7e885");


  {
    string s;
    for (size_t i (0); i < 1024; ++i)
      s += "0123456789";

    path p (path::temp_path ("butl-xxh64"));

    auto_rmfile pr;       // Always remove the file after the stream is closed.
    ofdstream os (p);
    pr = auto_rmfile (p);

    os << s;
    os.close ();

    ifdstream is (p);

    assert (string (xxh64 (is).string ()) ==
            xxh64 (s.c_str (), s.size ()).string ());

    assert (is.eof ());
    is.close ();
  }

  {
    xxh64 h ("123");
    assert (string (h.string ()) == "b7585f4d63630bd5");
  }

  {
    xxh64 h;
    h.append ("1");
    h.append (string ("2"));
    h.append ("3", 1);

    auto& b (h.binary ());
    assert (b[0] == 0x47 && b[7] == 0xed);

    assert (string (h.string ()) == "47d0d3d8df43a5ed");
  }
}
