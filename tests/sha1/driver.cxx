// file      : tests/sha1/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.sha1;
#else
#include <libbutl/sha1.mxx>
#endif

using namespace std;
using namespace butl;

int
main ()
{
  assert (string (sha1 ().string ()) ==
          "da39a3ee5e6b4b0d3255bfef95601890afd80709");

  assert (string (sha1 ("").string ()) !=
          "da39a3ee5e6b4b0d3255bfef95601890afd80709");

  assert (string (sha1 ("123").string ()) ==
          "cc320164df1a2130045a28f08d3b88bc5bbcc43a");

  assert (string (sha1 ("123", 3).string ()) ==
          "40bd001563085fc35165329ea1ff5c5ecbdbbeef");

  {
    sha1 h;
    h.append ("1");
    h.append (string ("2"));
    h.append ("3", 1);

    auto& b (h.binary ());
    assert (b[0] == 0x58 && b[19] == 0xfd);

    string s (h.string ());
    assert (s == "58c596bafad8d007952934af1db9abc5401d4dfd");
  }
}
