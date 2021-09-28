// file      : tests/small-forward-list/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>

#include <libbutl/small-forward-list.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Return true if the data is stored entirely inside l.
//
template <typename T, size_t N>
inline bool
small (const small_forward_list<T, N>& l)
{
  for (const T& x: l)
  {
    const void* p (&x);

    if (p < &l || p >= (&l + 1))
      return false;
  }

  return true;
}

template <typename T, size_t N>
inline const T&
front (const small_forward_list<T, N>& l)
{
  return l.front ();
}

template <typename T, size_t N>
inline const T&
back (const small_forward_list<T, N>& l)
{
  auto i (l.begin ());;
  for (auto j (i); ++j != l.end (); ) i = j;
  return *i;
}

int
main ()
{
  using list = small_forward_list<string, 1>;

  {
    list l;

    l.push_front ("abc");
    assert (front (l) == "abc" && small (l));

    l.push_front ("ABC");
    assert (front (l) == "ABC" && back (l) == "abc" && !small (l));

    l.pop_front ();
    assert (front (l) == "abc" && small (l));

    l.push_front ("ABC");
    l.reverse ();
    l.pop_front ();
    assert (front (l) == "ABC" && !small (l));

    l.push_front ("abc");
    l.reverse ();
    l.pop_front ();
    assert (front (l) == "abc" && small (l));

    l.clear ();
    l.push_front ("abc");
    assert (front (l) == "abc" && small (l));
  }

  // Copy constructor.
  //
  {
    list s1 ({"abc"}), s2 (s1);
    assert (s1 == s2 && small (s2));

    list l1 ({"abc", "ABC"}), l2 (l1);
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

    using list = small_forward_list<mstring, 1>;

    {
      list s1;
      s1.emplace_front ("abc");
      list s2 (move (s1));
      assert (front (s2) == "abc" && small (s2));
    }

    {
      list l1;
      l1.emplace_front ("ABC");
      l1.emplace_front ("abc");
      list l2 (move (l1));
      assert (front (l2) == "abc" && back (l2) == "ABC" && !small (l2));
    }
  }

  // Other constructors.
  //

  {
    const char* sa[] = {"abc"};
    const char* la[] = {"abc", "ABC"};

    list s (sa, sa + 1);
    assert (front (s) == "abc" && small (s));

    list l (la, la + 2);
    assert (front (l) == "abc" && back (l) == "ABC" && !small (l));
  }

  {
    list s (1, "abc");
    assert (front (s) == "abc" && small (s));

    list l (3, "abc");
    assert (front (l) == "abc" && back (l) == "abc" && !small (l));
  }

  {
    list s (1);
    assert (s.front () == "" && small (s));

    list l (3);
    assert (front (l) == "" && back (l) == "" && !small (l));
  }
}
