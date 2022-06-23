// file      : tests/manifest-parser/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <vector>
#include <string>
#include <utility> // pair, move()
#include <sstream>
#include <iostream>

#include <libbutl/optional.hxx>
#include <libbutl/manifest-parser.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;

namespace butl
{
  using butl::optional;
  using butl::nullopt;

  using pairs = vector<pair<string, string>>;

  static optional<pairs>
  to_pairs (optional<vector<manifest_name_value>>&&);

  static bool
  equal (const optional<pairs>& actual, const optional<pairs>& expected);

  static pairs
  parse (const char* m, manifest_parser::filter_function f = {});

  // Test manifest as it is represented in the stream, including format
  // version and end-of-manifest values.
  //
  static bool
  test (const char* manifest,
        const pairs& expected,
        manifest_parser::filter_function = {});

  static bool
  fail (const char* manifest);

  // Test manifest as it is returned by parse_manifest(), without the special
  // format version and end-of-manifest values.
  //
  static bool
  test_parse (const char* manifest, const optional<pairs>& expected);

  static bool
  fail_parse (const char* manifest);

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
    assert (test (":1\na: \t xyz \t ",
                  {{"","1"},{"a","xyz"},{"",""},{"",""}}));

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
    assert (test (":1\na:x\\\n\\\ny",
                  {{"","1"},{"a","x\ny"},{"",""},{"",""}}));
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
    // Single-line.
    //
    {
      auto p (manifest_parser::split_comment (
                "\\value\\\\\\; text ; comment text"));

      assert (p.first == "\\value\\; text" && p.second == "comment text");
    }

    {
      auto p (manifest_parser::split_comment ("value\\"));
      assert (p.first == "value\\" && p.second == "");
    }

    {
      auto p (manifest_parser::split_comment ("; comment"));
      assert (p.first == "" && p.second == "comment");
    }

    // Multi-line.
    //
    {
      auto p (manifest_parser::split_comment ("value\n;"));
      assert (p.first == "value" && p.second == "");
    }

    {
      auto p (manifest_parser::split_comment ("value\ntext\n"));
      assert (p.first == "value\ntext\n" && p.second == "");
    }

    {
      auto p (manifest_parser::split_comment ("value\ntext\n;"));
      assert (p.first == "value\ntext" && p.second == "");
    }

    {
      auto p (manifest_parser::split_comment ("value\ntext\n;\n"));
      assert (p.first == "value\ntext" && p.second == "");
    }

    {
      auto p (manifest_parser::split_comment ("\n\\\nvalue\ntext\n"
                                              ";\n"
                                              "\n\n comment\ntext"));

      assert (p.first == "\n\\\nvalue\ntext" && p.second ==
              "\n\n comment\ntext");
    }

    {
      auto p (manifest_parser::split_comment ("\n;\ncomment"));
      assert (p.first == "" && p.second == "comment");
    }

    {
      auto p (manifest_parser::split_comment (";\ncomment"));
      assert (p.first == "" && p.second == "comment");
    }

    {
      auto p (manifest_parser::split_comment (";\n"));
      assert (p.first == "" && p.second == "");
    }

    {
      auto p (manifest_parser::split_comment (
                "\\;\n\\\\;\n\\\\\\;\n\\\\\\\\;\n\\\\\\\\\\;"));

      assert (p.first == ";\n\\;\n\\;\n\\\\;\n\\\\;" && p.second == "");
    }

    // UTF-8.
    //
    assert (test (":1\n#\xD0\xB0\n\xD0\xB0y\xD0\xB0:\xD0\xB0z\xD0\xB0",
                  {{"","1"},
                   {"\xD0\xB0y\xD0\xB0", "\xD0\xB0z\xD0\xB0"},
                   {"",""},
                   {"",""}}));

    assert (fail (":1\n#\xD0\n\xD0\xB0y\xD0\xB0:\xD0\xB0z\xD0\xB0"));
    assert (fail (":1\n#\xD0\xB0\n\xB0y\xD0\xB0:\xD0\xB0z\xD0\xB0"));
    assert (fail (":1\n#\xD0\xB0\n\xD0y\xD0\xB0:\xD0\xB0z\xD0\xB0"));
    assert (fail (":1\n#\xD0\xB0\n\xD0\xB0y\xD0:\xD0\xB0z\xD0\xB0"));
    assert (fail (":1\n#\xD0\xB0\n\xD0\xB0y\xD0\xB0:\xD0z\xD0\xB0"));
    assert (fail (":1\n#\xD0\xB0\n\xD0\xB0y\xD0\xB0:\xD0\xB0z\xD0"));
    assert (fail (":1\r\r\xB0#\n\xD0\xB0y\xD0\xB0:\xD0\xB0z\xD0\xB0"));
    assert (fail (":1\r\xD0#\n\xD0\xB0y\xD0\xB0:\xD0\xB0z\xD0\xB0"));
    assert (fail (":1\n#\xD0\xB0\n\xD0\xB0y\xD0\xB0:\xD0\xB0z\xD0\xB0\r\xD0"));

    // Test parsing failure for manifest with multi-byte UTF-8 sequences
    // (the column is properly reported, etc).
    //
    try
    {
      parse (":1\na\xD0\xB0\xD0\xB0\xFE");
      assert (false);
    }
    catch (const manifest_parsing& e)
    {
      assert (e.line == 2   &&
              e.column == 4 &&
              e.description ==
              "invalid manifest name: "
              "invalid UTF-8 sequence first byte (0xFE)");
    }

    // Filtering.
    //
    assert (test (":1\na: abc\nb: bca\nc: cab",
                  {{"","1"},{"a","abc"},{"c","cab"},{"",""},{"",""}},
                  [] (manifest_name_value& nv) {return nv.name != "b";}));

    assert (test (":1\na: abc\nb: bca",
                  {{"","1"},{"ax","abc."},{"bx","bca."},{"",""},{"",""}},
                  [] (manifest_name_value& nv)
                  {
                    if (!nv.name.empty ())
                    {
                      nv.name += 'x';
                      nv.value += '.';
                    }

                    return true;
                  }));

    // Test parse_manifest().
    //
    test_parse ("", nullopt);
    test_parse (":1", pairs ());
    test_parse (":1\na: abc\nb: cde", pairs ({{"a", "abc"}, {"b", "cde"}}));

    fail_parse ("# abc");
    fail_parse ("a: abc");

    // Parse the manifest list.
    //
    {
      istringstream is (":1\na: abc\nb: bcd\n:\nx: xyz");
      is.exceptions (istream::failbit | istream::badbit);
      manifest_parser p (is, "");

      equal (to_pairs (try_parse_manifest (p)),
            pairs ({{"a", "abc"}, {"b", "bcd"}}));

      equal (to_pairs (try_parse_manifest (p)), pairs ({{"x", "xyz"}}));
      equal (to_pairs (try_parse_manifest (p)), nullopt);
    }

    return 0;
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

  static ostream&
  operator<< (ostream& os, const optional<pairs>& ps)
  {
    return ps ? (os << *ps) : (os << "[null]");
  }

  static bool
  equal (const optional<pairs>& a, const optional<pairs>& e)
  {
    if (a != e)
    {
      cerr << "actual: " << a << endl
           << "expect: " << e << endl;

      return false;
    }

    return true;
  }

  static optional<pairs>
  to_pairs (optional<vector<manifest_name_value>>&& nvs)
  {
    if (!nvs)
      return nullopt;

    pairs r;
    for (manifest_name_value& nv: *nvs)
      r.emplace_back (move (nv.name), move (nv.value));

    return r;
  }

  static pairs
  parse (const char* m, manifest_parser::filter_function f)
  {
    istringstream is (m);
    is.exceptions (istream::failbit | istream::badbit);
    manifest_parser p (is, "", move (f));

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

      r.emplace_back (move (nv.name), move (nv.value));
    }

    return r;
  }

  static bool
  test (const char* m, const pairs& e, manifest_parser::filter_function f)
  {
    return equal (parse (m, move (f)), e);
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

  static bool
  test_parse (const char* m, const optional<pairs>& e)
  {
    istringstream is (m);
    is.exceptions (istream::failbit | istream::badbit);
    manifest_parser p (is, "");

    return equal (to_pairs (try_parse_manifest (p)), e);
  }

  static bool
  fail_parse (const char* m)
  {
    try
    {
      istringstream is (m);
      is.exceptions (istream::failbit | istream::badbit);
      manifest_parser p (is, "");

      optional<pairs> r (to_pairs (parse_manifest (p)));
      cerr << "nofail: " << r << endl;
      return false;
    }
    catch (const manifest_parsing&)
    {
      //cerr << e << endl;
    }

    return true;
  }
}

int
main ()
{
  return butl::main ();
}
