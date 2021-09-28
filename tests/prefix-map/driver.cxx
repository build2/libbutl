// file      : tests/prefix-map/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>

#include <libbutl/prefix-map.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

int
main ()
{
  typedef prefix_map<string, int, '.'> pm;

  {
    const pm m;

    {
      auto r (m.find_sub (""));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo"));
      assert (r.first == r.second);
    }
  }

  {
    pm m {{{"foo", 1}}};

    {
      auto r (m.find_sub (""));
      assert (r.first != r.second && r.first->second == 1 &&
              ++r.first == r.second);
    }

    {
      auto r (m.find_sub ("fo"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("fox"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("fooo"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo.bar"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo"));
      assert (r.first != r.second && r.first->second == 1 &&
              ++r.first == r.second);
    }
  }

  {
    pm m {{{"foo", 1}, {"bar", 2}}};

    {
      auto r (m.find_sub (""));
      assert (r.first != r.second && r.first->second == 2 &&
              ++r.first != r.second && r.first->second == 1 &&
              ++r.first == r.second);
    }

    {
      auto r (m.find_sub ("fo"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("fox"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("fooo"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo.bar"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo"));
      assert (r.first != r.second && r.first->second == 1 &&
              ++r.first == r.second);
    }

    {
      auto r (m.find_sub ("bar"));
      assert (r.first != r.second && r.first->second == 2 &&
              ++r.first == r.second);
    }
  }

  {
    pm m (
      {{"boo", 1},
       {"foo", 2}, {"fooa", 3}, {"foo.bar", 4}, {"foo.fox", 5},
       {"xoo", 5}});

    {
      auto r (m.find_sub ("fo"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("fox"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("fooo"));
      assert (r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo.bar"));
      assert (r.first != r.second && r.first->second == 4 &&
              ++r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo.fox"));
      assert (r.first != r.second && r.first->second == 5 &&
              ++r.first == r.second);
    }

    {
      auto r (m.find_sub ("foo"));
      assert (r.first != r.second && r.first->second == 2 &&
              ++r.first != r.second && r.first->second == 4 &&
              ++r.first != r.second && r.first->second == 5 &&
              ++r.first == r.second);
    }
  }

  {
    pm m (
      {{"foo",         1},
       {"fooa",        2},
       {"foo.bar",     3},
       {"foo.baz.aaa", 4},
       {"foo.baz.bbb", 5},
       {"foo.baz.xxx", 6},
       {"xoo",         7}});

    auto e (m.end ());

    {
      auto i (m.find_sup ("fox"));
      assert (i == e);
    }

    {
      auto i (m.find_sup ("foo.baz.bbb"));
      assert (i != e && i->second == 5);
    }

    {
      auto i (m.find_sup ("foo.baz.ccc"));
      assert (i != e && i->second == 1);
    }

    {
      auto i (m.find_sup ("foo.baz"));
      assert (i != e && i->second == 1);
    }

    {
      auto i (m.find_sup ("xoo.bar"));
      assert (i != e && i->second == 7);
    }
  }

  {
    // Test the special empty prefix logic.
    //
    pm m ({{"", 1}});

    auto e (m.end ());

    assert (m.find_sup ("") != e);
    assert (m.find_sup ("foo") != e);
    assert (m.find_sup ("foo.bar") != e);
  }
}
