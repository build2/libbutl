// file      : tests/path/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>
#include <iostream>
#include <type_traits>

#include <butl/path>
#include <butl/path-io>

using namespace std;
using namespace butl;

int
main ()
{
  static_assert (is_nothrow_destructible<path>::value, "");
  static_assert (is_nothrow_move_constructible<path>::value, "");

  static_assert (is_nothrow_destructible<dir_path>::value, "");
  static_assert (is_nothrow_move_constructible<dir_path>::value, "");

  assert (path ("/").string () == "/");
  assert (path ("//").string () == "/");
  assert (path ("/tmp/foo/").string () == "/tmp/foo");
#ifdef _WIN32
  assert (path ("\\\\").string () == "\\");
  assert (path ("/\\").string () == "/");
  assert (path ("C:").string () == "C:");
  assert (path ("C:\\").string () == "C:");
  assert (path ("C:\\tmp\\foo\\").string () == "C:\\tmp\\foo");
#endif

  // abslote/relative/root
  //
#ifndef _WIN32
  assert (path ("/").root ());
  assert (path ("//").root ());
  assert (path ("/").absolute ());
  assert (path ("/foo/bar").absolute ());
  assert (path ("bar/baz").relative ());
#else
  assert (path ("C:").root ());
  assert (path ("C:\\").root ());
  assert (path ("C:\\").absolute ());
  assert (path ("C:\\foo\\bar").absolute ());
  assert (path ("bar\\baz").relative ());
#endif


  // leaf
  //
#ifndef _WIN32
  assert (path ("/").leaf ().string () == "");
  assert (path ("/tmp").leaf ().string () == "tmp");
  assert (path ("//tmp").leaf ().string () == "tmp");
#else
  assert (path ("C:").leaf ().string () == "C:");
  assert (path ("C:\\tmp").leaf ().string () == "tmp");
  assert (path ("C:\\\\tmp").leaf ().string () == "tmp");
#endif

  // directory
  //
#ifndef _WIN32
  assert (path ("/").directory ().string () == "");
  assert (path ("/tmp").directory ().string () == "/");
  assert (path ("//tmp").directory ().string () == "/");
#else
  assert (path ("C:").directory ().string () == "");
  assert (path ("C:\\tmp").directory ().string () == "C:");
  assert (path ("C:\\\\tmp").directory ().string () == "C:");
#endif

  // base
  //
  assert (path ("/").base ().string () == "/");
  assert (path ("/foo.txt").base ().string () == "/foo");
  assert (path (".txt").base ().string () == ".txt");
  assert (path ("/.txt").base ().string () == "/.txt");
  assert (path ("foo.txt.orig").base ().string () == "foo.txt");
#ifdef _WIN32
  assert (path ("C:").base ().string () == "C:");
  assert (path ("C:\\foo.txt").base ().string () == "C:\\foo");
#endif

  // iteration
  //
  {
    path p;
    assert (p.begin () == p.end ());
  }
  {
    path p;
    assert (p.rbegin () == p.rend ());
  }
  {
    path p ("foo");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "foo");
    assert (++i == p.end ());
  }
  {
    path p ("foo");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "foo");
    assert (++i == p.rend ());
  }
  {
    path p ("foo/bar");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "foo");
    assert (++i != p.end () && *i == "bar");
    assert (++i == p.end ());
  }
  {
    path p ("foo/bar");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "bar");
    assert (++i != p.rend () && *i == "foo");
    assert (++i == p.rend ());
  }
  {
    path p ("/foo/bar");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "");
    assert (++i != p.end () && *i == "foo");
    assert (++i != p.end () && *i == "bar");
    assert (++i == p.end ());
  }
  {
    path p ("/foo/bar");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "bar");
    assert (++i != p.rend () && *i == "foo");
    assert (++i != p.rend () && *i == "");
    assert (++i == p.rend ());
  }
#ifndef _WIN32
  {
    path p ("/");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "");
    assert (++i == p.end ());
  }
  {
    path p ("/");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "");
    assert (++i == p.rend ());
  }
#endif

  // iterator range construction
  //
  {
    path p;
    assert (path (p.begin (), p.end ()) == p);
  }
  {
    path p ("foo");
    assert (path (p.begin (), p.end ()) == p);
    assert (path (++p.begin (), p.end ()) == path ());
  }
  {
    path p ("foo/bar");
    assert (path (p.begin (), p.end ()) == p);
    assert (path (++p.begin (), p.end ()) == path ("bar"));
    assert (path (p.begin (), ++p.begin ()) == path ("foo"));
  }
  {
    path p ("/foo/bar");
    assert (path (p.begin (), p.end ()) == p);
    assert (path (++p.begin (), p.end ()) == path ("foo/bar"));
    assert (path (++(++p.begin ()), p.end ()) == path ("bar"));
    assert (path (p.begin (), ++p.begin ()) == path ("/"));
    assert (path (++p.begin (), ++(++p.begin ())) == path ("foo"));
    assert (path (++(++p.begin ()), ++(++(++p.begin ()))) == path ("bar"));
  }
#ifndef _WIN32
  {
    path p ("/");
    assert (path (p.begin (), p.end ()) == p);
    assert (path (++p.begin (), p.end ()) == path ());
  }
#endif

  // operator/
  //
#ifndef _WIN32
  assert ((path ("/") / path ("tmp")).string () == "/tmp");
  assert ((path ("foo") / path ("bar")).string () == "foo/bar");
#else
  assert ((path ("\\") / path ("tmp")).string () == "\\tmp");
  assert ((path ("C:\\") / path ("tmp")).string () == "C:\\tmp");
  assert ((path ("foo") / path ("bar")).string () == "foo\\bar");
#endif

  // normalize
  //
#ifndef _WIN32
  assert (path ("../foo").normalize ().string () == "../foo");
  assert (path ("..///foo").normalize ().string () == "../foo");
  assert (path ("../../foo").normalize ().string () == "../../foo");
  assert (path (".././foo").normalize ().string () == "../foo");
  assert (path (".").normalize ().string () == "");
  assert (path ("./..").normalize ().string () == "..");
  assert (path ("../.").normalize ().string () == "..");
  assert (path ("foo/./..").normalize ().string () == "");
  assert (path ("/foo/./..").normalize ().string () == "/");
  assert (path ("./foo").normalize ().string () == "foo");
#else
  assert (path ("../foo").normalize ().string () == "..\\foo");
  assert (path ("..///foo").normalize ().string () == "..\\foo");
  assert (path ("..\\../foo").normalize ().string () == "..\\..\\foo");
  assert (path (".././foo").normalize ().string () == "..\\foo");
  assert (path (".").normalize ().string () == "");
  assert (path ("./..").normalize ().string () == "..");
  assert (path ("../.").normalize ().string () == "..");
  assert (path ("foo/./..").normalize ().string () == "");
  assert (path ("C:/foo/./..").normalize ().string () == "C:");
  assert (path ("./foo").normalize ().string () == "foo");

  assert (path ("C:").normalize ().string () == "C:");
  assert (path ("C:\\Foo12//Bar").normalize ().string () == "C:\\Foo12\\Bar");
#endif

  // comparison
  //
  assert (path ("./foo") == path ("./foo"));
  assert (path ("./boo") < path ("./foo"));
#ifdef _WIN32
  assert (path (".\\foo") == path ("./FoO"));
  assert (path (".\\boo") < path (".\\Foo"));
#endif

  // posix_string
  //
  assert (path ("foo/bar/../baz").posix_string () == "foo/bar/../baz");
#ifdef _WIN32
  assert (path ("foo\\bar\\..\\baz").posix_string () == "foo/bar/../baz");
  try
  {
    path ("c:\\foo\\bar\\..\\baz").posix_string ();
    assert (false);
  }
  catch (const invalid_path&) {}
#endif

  // sub
  //
  assert (path ("foo").sub (path ("foo")));
  assert (path ("foo/bar").sub (path ("foo/bar")));
  assert (path ("foo/bar").sub (path ("foo")));
  assert (!path ("foo/bar").sub (path ("bar")));
  assert (path ("/foo/bar").sub (path ("/foo")));
  assert (path ("/foo/bar/baz").sub (path ("/foo/bar")));
  assert (!path ("/foo/bar/baz").sub (path ("/foo/baz")));
#ifdef _WIN32
  assert (path ("c:").sub (path ("c:")));
  assert (!path ("c:").sub (path ("d:")));
  assert (path ("c:\\foo").sub (path ("c:")));
#else
  assert (path ("/foo/bar/baz").sub (path ("/")));
#endif

  // relative
  //
  assert (path ("foo").relative (path ("foo")) == path ());
  assert (path ("foo/bar").relative (path ("foo/bar")) == path ());
  assert (path ("foo/bar/baz").relative (path ("foo/bar")) == path ("baz"));
  assert (path ("foo/bar/baz").relative (path ("foo/bar/buz")).
    posix_string () == "../baz");
  assert (path ("foo/bar/baz").relative (path ("foo/biz/baz")).
    posix_string () == "../../bar/baz");
  assert (path ("foo/bar/baz").relative (path ("fox/bar/baz")).
    posix_string () == "../../../foo/bar/baz");
#ifdef _WIN32
  assert (path ("c:\\foo\\bar").relative (path ("c:\\fox\\bar")) ==
          path ("..\\..\\foo\\bar"));
  try
  {
    path ("c:\\foo\\bar").relative (path ("d:\\fox\\bar"));
    assert (false);
  }
  catch (const invalid_path&) {}
#else
  assert (path ("/foo/bar/baz").relative (path ("/")) ==
          path ("foo/bar/baz"));
#endif

  /*
  path p ("../foo");
  p.complete ();

  cerr << path::current () << endl;
  cerr << p << endl;
  p.normalize ();
  cerr << p << endl;
  */
}