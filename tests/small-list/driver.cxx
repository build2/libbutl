// file      : tests/small-list/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>

#include <libbutl/small-list.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Return true if the data is stored entirely inside l.
//
template <typename T, size_t N>
inline bool
small (const small_list<T, N>& l)
{
  // In VC it is always "large" (see note in small_list) so we omit this
  // test.
  //
#ifndef _MSC_VER
  for (const T& x: l)
  {
    const void* p (&x);

    if (p < &l || p >= (&l + 1))
      return false;
  }
#endif

  return true;
}

template <typename T, size_t N>
inline bool
large (const small_list<T, N>& l)
{
#ifndef _MSC_VER
  return !small (l);
#else
  return true;
#endif
}

int
main ()
{
  using list = small_list<string, 1>;

  {
    list l;

    l.push_back ("abc");
    assert (l.front () == "abc" && small (l));

    l.push_back ("ABC");
    assert (l.front () == "abc" && l.back () == "ABC" && large (l));

    l.pop_back ();
    assert (l.front () == "abc" && small (l));

    l.push_back ("ABC");
    l.pop_front ();
    assert (l.front () == "ABC" && large (l));

    l.push_back ("abc");
    l.pop_front ();
    assert (l.front () == "abc" && small (l));

    l.clear ();
    l.push_back ("abc");
    assert (l.front () == "abc" && small (l));
  }

  // Copy constructor.
  //
  {
    list s1 ({"abc"}), s2 (s1);
    assert (s1 == s2 && small (s2));

    list l1 ({"abc", "ABC"}), l2 (l1);
    assert (l1 == l2 && large (l2));
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

    using list = small_list<mstring, 1>;

    {
      list s1;
      s1.emplace_back ("abc");
      list s2 (move (s1));
      assert (s2.front () == "abc" && small (s2));
    }

    {
      list l1;
      l1.emplace_back ("abc");
      l1.emplace_back ("ABC");
      list l2 (move (l1));
      assert (l2.front () == "abc" && l2.back () == "ABC" && large (l2));
    }
  }

  // Other constructors.
  //

  {
    const char* sa[] = {"abc"};
    const char* la[] = {"abc", "ABC"};

    list s (sa, sa + 1);
    assert (s.front () == "abc" && small (s));

    list l (la, la + 2);
    assert (l.front () == "abc" && l.back () == "ABC" && large (l));
  }

  {
    list s (1, "abc");
    assert (s.front () == "abc" && small (s));

    list l (3, "abc");
    assert (l.front () == "abc" && l.back () == "abc" && large (l));
  }

  {
    list s (1);
    assert (s.front () == "" && small (s));

    list l (3);
    assert (l.front () == "" && l.back () == "" && large (l));
  }
}
