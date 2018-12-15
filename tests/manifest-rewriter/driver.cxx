// file      : tests/manifest-rewriter/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <vector>
#include <string>
#include <cstdint>   // uint64_t
#include <utility>   // move()
#include <iostream>
#include <exception>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.path;
import butl.optional;
import butl.fdstream;
import butl.manifest_parser;
import butl.manifest_rewriter;
#else
#include <libbutl/path.mxx>
#include <libbutl/optional.mxx>
#include <libbutl/fdstream.mxx>
#include <libbutl/manifest-parser.mxx>
#include <libbutl/manifest-rewriter.mxx>
#endif

using namespace std;

namespace butl
{
  using butl::optional;
  using butl::nullopt;

  // Value rewriting or insertion command.
  //
  struct edit_cmd
  {
    string name;
    string value;
    optional<string> after; // Rewrite an existing value if nullopt.

    edit_cmd (string n, string v)
        : name (move (n)), value (move (v)) {}

    edit_cmd (string n, string v, string a)
        : name (move (n)), value (move (v)), after (move (a)) {}
  };

  using edit_cmds = vector<edit_cmd>;

  // Dump the manifest into the file, edit and return the resulting manifest.
  //
  // The file will stay in the filesystem for troubleshooting in case of an
  // assertion failure and will be deleted otherwise.
  //
  static path temp_file (path::temp_path ("butl-manifest-rewriter"));

  static string
  edit (const char* manifest, const edit_cmds&);

  int
  main ()
  {
    auto_rmfile rm (temp_file);

    assert (edit (":1\n# Comment\n# Comment\n a : b \n# Comment\n\nc:d\n",
                  {{"a", "xyz"}}) ==
            ":1\n# Comment\n# Comment\n a : xyz\n# Comment\n\nc:d\n");

    assert (edit (":1\n\n a: b\n", {{"a", "xyz"}}) == ":1\n\n a: xyz\n");
    assert (edit (":1\na: b", {{"a", "xyz"}}) == ":1\na: xyz");

    assert (edit (":1\na:b\nc:d\ne:f",
                  {{"a", "xyz"}, edit_cmd {"x", "y", "c"}, {"e", "123"}}) ==
            ":1\na: xyz\nc:d\nx: y\ne: 123");

    assert (edit (":1\na: b", {{"a", "xy\nz"}}) == ":1\na: \\\nxy\nz\n\\");

    assert (edit (":1\n", {{"a", "b", ""}}) == ":1\na: b\n");

    assert (edit (":1\n                                     abc: b",
                  {{"abc", "xyz"}}) ==
            ":1\n                                     abc: \\\nxyz\n\\");

    // Test editing of manifests that contains CR characters.
    //
    assert (edit (":1\r\na: b\r\r\n", {{"a", "xyz"}}) == ":1\r\na: xyz\r\r\n");

    assert (edit (":1\ra: b\r", {{"a", "xyz"}}) == ":1\ra: xyz\r");

    assert (edit (":1\na: \\s", {{"a", "xyz"}}) == ":1\na: xyz");

    assert (edit (":1\na: \\\nx\ny\nz\n\\\r", {{"a", "b"}}) == ":1\na: b\r");

    return 0;
  }

  static string
  edit (const char* manifest, const edit_cmds& cmds)
  {
    {
      ofdstream os (temp_file);
      os << manifest;
      os.close ();
    }

    struct insertion
    {
      manifest_name_value value;
      optional<manifest_name_value> pos; // Rewrite existing value if nullopt.
    };

    vector<insertion> insertions;
    {
      ifdstream is (temp_file);
      manifest_parser p (is, temp_file.string ());

      for (manifest_name_value nv; !(nv = p.next ()).empty (); )
      {
        for (const edit_cmd& c: cmds)
        {
          if (c.after)
          {
            if (nv.name == *c.after)
            {
              // Note: new value lines, columns and positions are all zero as
              // are not used for an insertion.
              //
              insertions.push_back (
                insertion {
                  manifest_name_value {c.name, c.value, 0, 0, 0, 0, 0, 0, 0},
                    move (nv)});

              break;
            }
          }
          else if (nv.name == c.name)
          {
            nv.value = c.value;
            insertions.push_back (insertion {move (nv), nullopt /* pos */});
            break;
          }
        }
      }
    }

    {
      manifest_rewriter rw (temp_file);

      for (const auto& ins: reverse_iterate (insertions))
      {
        if (ins.pos)
          rw.insert (*ins.pos, ins.value);
        else
          rw.replace (ins.value);
      }
    }

    ifdstream is (temp_file);
    return is.read_text ();
  }
}

int
main ()
{
  try
  {
    return butl::main ();
  }
  catch (const exception& e)
  {
    cerr << e << endl;
    return 1;
  }
}
