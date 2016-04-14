// file      : tests/triplet/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <cassert>
#include <iostream>

#include <butl/sha256>

using namespace std;
using namespace butl;

int
main ()
{
  assert (string (sha256 ().string ()) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  assert (string (sha256 ("").string ()) !=
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  assert (string (sha256 ("123").string ()) ==
          "a787b6772e3e4df1b2a04d5eee56f8570ab38825eed1b6a9bda288429b7f29a1");

  assert (string (sha256 ("123", 3).string ()) ==
          "a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3");

  sha256 h;
  h.append ("1");
  h.append (string ("2"));
  h.append ("3", 1);

  auto& b (h.binary ());
  assert (b[0] == 0x20 && b[31] == 0x9d);

  string s (h.string ());
  assert (s ==
          "204d9db65789fbede7829ed77f72ba1f0fe21a833d95abad4849b82f33a69b9d");
}