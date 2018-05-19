// file      : tests/manifest-parser/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <vector>
#include <string>
#include <utility> // pair
#include <sstream>
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.manifest_parser;
#else
#include <libbutl/manifest-parser.mxx>
#endif

using namespace std;
using namespace butl;

using pairs = vector<pair<string, string>>;

static bool
test (const char* manifest, const pairs& expected);

static bool
fail (const char* manifest);

int
main ()
{
  // Whitespaces and comments.
  //
  assert (test (" \t", {{"",""}}));
  assert (test (" \t\n \n\n", {{"",""}}));
  assert (test ("# one\n  #two", {{"",""}}));

  // Test encountering eos at various points.
  //
  assert (test ("", {{"",""}}));
  assert (test (" ", {{"",""}}));
  assert (test ("\n", {{"",""}}));
  assert (fail ("a"));
  assert (test (":1\na:", {{"","1"},{"a", ""},{"",""},{"",""}}));

  // Invalid manifests.
  //
  assert (fail ("a:"));          // format version pair expected
  assert (fail (":"));           // format version value expected
  assert (fail (":9"));          // unsupported format version
  assert (fail ("a"));           // ':' expected after name
  assert (fail ("a b"));         // ':' expected after name
  assert (fail ("a\tb"));        // ':' expected after name
  assert (fail ("a\nb"));        // ':' expected after name
  assert (fail (":1\na:b\n:9")); // unsupported format version

  // Empty manifest.
  //
  assert (test (":1", {{"","1"},{"",""},{"",""}}));
  assert (test (" \t :1", {{"","1"},{"",""},{"",""}}));
  assert (test (" \t : 1", {{"","1"},{"",""},{"",""}}));
  assert (test (" \t : 1 ", {{"","1"},{"",""},{"",""}}));
  assert (test (":1\n", {{"","1"},{"",""},{"",""}}));
  assert (test (":1 \n", {{"","1"},{"",""},{"",""}}));

  // Single manifest.
  //
  assert (test (":1\na:x", {{"","1"},{"a", "x"},{"",""},{"",""}}));
  assert (test (":1\na:x\n", {{"","1"},{"a","x"},{"",""},{"",""}}));
  assert (test (":1\na:x\nb:y",
                {{"","1"},{"a","x"},{"b","y"},{"",""},{"",""}}));
  assert (test (":1\na:x\n\tb : y\n  #comment",
                {{"","1"},{"a","x"},{"b","y"},{"",""},{"",""}}));

  // Multiple manifests.
  //
  assert (test (":1\na:x\n:\nb:y",
                {{"","1"},{"a", "x"},{"",""},
                 {"","1"},{"b", "y"},{"",""},{"",""}}));
  assert (test (":1\na:x\n:1\nb:y",
                {{"","1"},{"a", "x"},{"",""},
                 {"","1"},{"b", "y"},{"",""},{"",""}}));
  assert (test (":1\na:x\n:\nb:y\n:\nc:z\n",
                {{"","1"},{"a", "x"},{"",""},
                 {"","1"},{"b", "y"},{"",""},
                 {"","1"},{"c", "z"},{"",""},{"",""}}));

  // Name parsing.
  //
  assert (test (":1\nabc:", {{"","1"},{"abc",""},{"",""},{"",""}}));
  assert (test (":1\nabc :", {{"","1"},{"abc",""},{"",""},{"",""}}));
  assert (test (":1\nabc\t:", {{"","1"},{"abc",""},{"",""},{"",""}}));

  // Simple value parsing.
  //
  assert (test (":1\na: \t xyz \t ", {{"","1"},{"a","xyz"},{"",""},{"",""}}));

  // Simple value escaping.
  //
  assert (test (":1\na:x\\", {{"","1"},{"a","x"},{"",""},{"",""}}));
  assert (test (":1\na:x\\\ny", {{"","1"},{"a","xy"},{"",""},{"",""}}));
  assert (test (":1\na:x\\\\\nb:",
                {{"","1"},{"a","x\\"},{"b",""},{"",""},{"",""}}));
  assert (test (":1\na:x\\\\\\\nb:",
                {{"","1"},{"a","x\\\\"},{"b",""},{"",""},{"",""}}));

  // Simple value literal newline.
  //
  assert (test (":1\na:x\\\n\\", {{"","1"},{"a","x\n"},{"",""},{"",""}}));
  assert (test (":1\na:x\\\n\\\ny", {{"","1"},{"a","x\ny"},{"",""},{"",""}}));
  assert (test (":1\na:x\\\n\\\ny\\\n\\\nz",
                {{"","1"},{"a","x\ny\nz"},{"",""},{"",""}}));

  // Multi-line value parsing.
  //
  assert (test (":1\na:\\", {{"","1"},{"a", ""},{"",""},{"",""}}));
  assert (test (":1\na:\\\n", {{"","1"},{"a", ""},{"",""},{"",""}}));
  assert (test (":1\na:\\x", {{"","1"},{"a", "\\x"},{"",""},{"",""}}));
  assert (test (":1\na:\\\n\\", {{"","1"},{"a", ""},{"",""},{"",""}}));
  assert (test (":1\na:\\\n\\\n", {{"","1"},{"a", ""},{"",""},{"",""}}));
  assert (test (":1\na:\\\n\\x\n\\",
                {{"","1"},{"a", "\\x"},{"",""},{"",""}}));
  assert (test (":1\na:\\\nx\ny", {{"","1"},{"a", "x\ny"},{"",""},{"",""}}));
  assert (test (":1\na:\\\n \n#\t\n\\",
                {{"","1"},{"a", " \n#\t"},{"",""},{"",""}}));
  assert (test (":1\na:\\\n\n\n\\", {{"","1"},{"a", "\n"},{"",""},{"",""}}));

  // Multi-line value escaping.
  //
  assert (test (":1\na:\\\nx\\", {{"","1"},{"a","x"},{"",""},{"",""}}));
  assert (test (":1\na:\\\nx\\\ny\n\\",
                {{"","1"},{"a","xy"},{"",""},{"",""}}));
  assert (test (":1\na:\\\nx\\\\\n\\\nb:",
                {{"","1"},{"a","x\\"},{"b",""},{"",""},{"",""}}));
  assert (test (":1\na:\\\nx\\\\\\\n\\\nb:",
                {{"","1"},{"a","x\\\\"},{"b",""},{"",""},{"",""}}));

  // Manifest value splitting (into the value/comment pair).
  //
  {
    auto p (manifest_parser::split_comment ("value\\; text ; comment text"));
    assert (p.first == "value; text" && p.second == "comment text");
  }

  {
    auto p (manifest_parser::split_comment ("value"));
    assert (p.first == "value" && p.second == "");
  }

  {
    auto p (manifest_parser::split_comment ("; comment"));
    assert (p.first == "" && p.second == "comment");
  }
}

static ostream&
operator<< (ostream& os, const pairs& ps)
{
  os << '{';

  bool f (true);
  for (const auto& p: ps)
    os << (f ? (f = false, "") : ",")
       << '{' << p.first << ',' << p.second << '}';

  os << '}';
  return os;
}

static pairs
parse (const char* m)
{
  istringstream is (m);
  is.exceptions (istream::failbit | istream::badbit);
  manifest_parser p (is, "");

  pairs r;

  for (bool eom (true), eos (false); !eos; )
  {
    manifest_name_value nv (p.next ());

    if (nv.empty ()) // End pair.
    {
      eos = eom;
      eom = true;
    }
    else
      eom = false;

    r.emplace_back (nv.name, nv.value); // move
  }

  return r;
}

static bool
test (const char* m, const pairs& e)
{
  pairs r (parse (m));

  if (r != e)
  {
    cerr << "actual: " << r << endl
         << "expect: " << e << endl;

    return false;
  }

  return true;
}

static bool
fail (const char* m)
{
  try
  {
    pairs r (parse (m));
    cerr << "nofail: " << r << endl;
    return false;
  }
  catch (const manifest_parsing&)
  {
    //cerr << e << endl;
  }

  return true;
}
