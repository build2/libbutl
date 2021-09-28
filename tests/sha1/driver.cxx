// file      : tests/sha1/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <cstddef> // size_t

#include <libbutl/sha1.hxx>
#include <libbutl/path.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/filesystem.hxx> // auto_rmfile

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

int
main ()
{
  assert (string (sha1 ().string ()) ==
          "da39a3ee5e6b4b0d3255bfef95601890afd80709");

  assert (string (sha1 ("").string ()) !=
          "da39a3ee5e6b4b0d3255bfef95601890afd80709");

  assert (string (sha1 ("123", 3).string ()) ==
          "40bd001563085fc35165329ea1ff5c5ecbdbbeef");

  {
    string s;
    for (size_t i (0); i < 1024; ++i)
      s += "0123456789";

    path p (path::temp_path ("butl-sha1"));

    auto_rmfile pr;       // Always remove the file after the stream is closed.
    ofdstream os (p);
    pr = auto_rmfile (p);

    os << s;
    os.close ();

    ifdstream is (p);

    assert (string (sha1 (is).string ()) ==
            sha1 (s.c_str (), s.size ()).string ());

    assert (is.eof ());
    is.close ();
  }

  {
    sha1 h ("123");
    assert (string (h.string ()) ==
            "cc320164df1a2130045a28f08d3b88bc5bbcc43a");

    assert (h.abbreviated_string (10) == "cc320164df");
    assert (h.abbreviated_string (41) == h.string ());
  }

  {
    sha1 h;
    h.append ("1");
    h.append (string ("2"));
    h.append ("3", 1);

    auto& b (h.binary ());
    assert (b[0] == 0x58 && b[19] == 0xfd);

    assert (string (h.string ()) ==
            "58c596bafad8d007952934af1db9abc5401d4dfd");
  }
}
