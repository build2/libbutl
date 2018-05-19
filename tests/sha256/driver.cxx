// file      : tests/sha256/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
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
import butl.sha256;
#else
#include <libbutl/sha256.mxx>
#endif

using namespace std;
using namespace butl;

int
main ()
{
  assert (string (sha256 ().string ()) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  assert (string (sha256 ("").string ()) !=
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  assert (string (sha256 ("123", 3).string ()) ==
          "a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3");

  {
    sha256 h ("123");

    assert (string (h.string ()) ==
            "a787b6772e3e4df1b2a04d5eee56f8570ab38825eed1b6a9bda288429b7f29a1");

    assert (h.abbreviated_string (10) == "a787b6772e");
    assert (h.abbreviated_string (65) == h.string ());
  }

  {
    sha256 h;
    h.append ("1");
    h.append (string ("2"));
    h.append ("3", 1);

    auto& b (h.binary ());
    assert (b[0] == 0x20 && b[31] == 0x9d);

    assert (string (h.string ()) ==
            "204d9db65789fbede7829ed77f72ba1f0fe21a833d95abad4849b82f33a69b9d");
  }

  // Test fast path.
  //
  {
    char c ('X');
    sha256 h;
    h.append (c);
    assert (string (h.string ()) == sha256 (&c, 1).string ());
  }

  //
  //
  string fp ("F4:9D:C0:02:C6:B6:62:06:A5:48:AE:87:35:32:95:64:C2:B8:C9:6D:9B:"
             "28:85:6D:EF:CA:FA:7F:04:B5:4F:A6");

  string sh (
    "f49dc002c6b66206a548ae8735329564c2b8c96d9b28856defcafa7f04b54fa6");

  assert (fingerprint_to_sha256 (fp) == sh);
  assert (fingerprint_to_sha256 (fp, 65) == sh);
  assert (fingerprint_to_sha256 (fp, 10) == sh.substr (0, 10));

  assert (sha256_to_fingerprint (sh) == fp);
}
