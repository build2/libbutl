// file      : tests/small-vector/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <cassert>
#include <iostream>

#include <butl/small-vector>

using namespace std;
using namespace butl;

// Return if v.data() points to somewhere inside v.
//
template <typename T, size_t N>
inline bool
small (const small_vector<T, N>& v)
{
  const void* d (v.data ());
  return d >= &v && d < (&v + 1);
}

int
main ()
{
  using vector = small_vector<string, 1>;

  {
    vector v;
    assert (v.capacity () == 1 && small (v));

    v.push_back ("abc");
    assert (v[0] == "abc" && v.capacity () == 1 && small (v));

    string* d (v.data ());              // Small buffer...

    v.push_back ("xyz");
    assert (v[0] == "abc" && v.data () != d && !small (v));

    v.pop_back ();
    v.shrink_to_fit ();
    assert (v[0] == "abc" && v.data () == d);
  }

  // Allocator comparison.
  //
  {
    vector v1, v2;
    assert (v1.get_allocator () != v2.get_allocator ()); // stack/stack

    v1.assign ({"abc", "xyz"});
    assert (v1.get_allocator () != v2.get_allocator ()); // heap/stack

    v2.assign ({"abc", "xyz"});
    assert (v1.get_allocator () == v2.get_allocator ()); // heap/heap

    v1.pop_back ();
    v1.shrink_to_fit ();
    assert (v1.get_allocator () != v2.get_allocator ()); // stack/heap

    v2.pop_back ();
    v2.shrink_to_fit ();
    assert (v1.get_allocator () != v2.get_allocator ()); // stack/stack
  }

  // Copy constructor.
  //
  {
    vector s1 ({"abc"}), s2 (s1);
    assert (s1 == s2 && s2.capacity () == 1 && small (s2));

    vector l1 ({"abc", "xyz"}), l2 (l1);
    assert (l1 == l2 && !small (l2));
  }

  // Move constructor.
  //
  {
    struct mstring: string // Move-only string.
    {
      mstring () = default;
      explicit mstring (const char* s): string (s) {}

      mstring (mstring&&) = default;
      mstring& operator= (mstring&&) = default;

      mstring (const mstring&) = delete;
      mstring& operator= (const mstring&) = delete;
    };

    using vector = small_vector<mstring, 1>;

    vector s1; s1.emplace_back ("abc");
    vector s2 (move (s1));
    assert (s2[0] == "abc" && s2.capacity () == 1 && small (s2));

    vector l1; l1.emplace_back ("abc"); l1.emplace_back ("xyz");
    vector l2 (move (l1));
    assert (l2[0] == "abc" && l2[1] == "xyz" && !small (l2));
  }

  // Other constructors.
  //

  {
    const char* sa[] = {"abc"};
    const char* la[] = {"abc", "xyz"};

    vector s (sa, sa + 1);
    assert (s[0] == "abc" && s.capacity () == 1 && small (s));

    vector l (la, la + 2);
    assert (l[0] == "abc" && l[1] == "xyz" && !small (l));
  }

  {
    vector s (1, "abc");
    assert (s[0] == "abc" && s.capacity () == 1 && small (s));

    vector l (2, "abc");
    assert (l[0] == "abc" && l[1] == "abc" && !small (l));
  }

  {
    vector s (1);
    assert (s[0] == "" && s.capacity () == 1 && small (s));

    vector l (2);
    assert (l[0] == "" && l[1] == "" && !small (l));
  }
}