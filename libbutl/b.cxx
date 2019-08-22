// file      : libbutl/b.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/b.mxx>
#endif

// C includes.

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <functional>

#include <ios>     // ios::failure
#include <utility> // move()
#include <sstream>
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.b;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.url;
import butl.path;
import butl.process;
import butl.optional;
import butl.project_name;
import butl.standard_version;
#endif

import butl.utility;      // next_word(), eof(), etc
import butl.path_io;
import butl.fdstream;
import butl.process_io;   // operator<<(ostream, process_path)
import butl.small_vector;
#else
#include <libbutl/utility.mxx>
#include <libbutl/path-io.mxx>
#include <libbutl/fdstream.mxx>
#include <libbutl/process-io.mxx>
#include <libbutl/small-vector.mxx>
#endif

using namespace std;

namespace butl
{
  b_error::
  b_error (const string& d, optional<process_exit> e)
      : runtime_error (d),
        exit (move (e))
  {
  }

  [[noreturn]] static inline void
  bad_value (const string& d)
  {
    throw runtime_error ("invalid " + d);
  }

  b_project_info
  b_info (const dir_path& project,
          uint16_t verb,
          const function<b_callback>& cmd_callback,
          const path& program,
          const dir_path& search_fallback,
          const vector<string>& ops)
  {
    try
    {
      process_path pp (
        process::path_search (program, true /* init */, search_fallback));

      process pr;
      try
      {
        fdpipe pipe (fdopen_pipe ()); // Text mode seems appropriate.

        small_vector<string, 2> vops;

        // Let's suppress warnings the `b info` command may issue, unless the
        // verbosity level is 2 (-v) or higher. For example:
        //
        // warning: configured src_root prj/ does not match forwarded prj/prj/
        //
        if (verb >= 2)
        {
          vops.push_back ("--verbose");
          vops.push_back (std::to_string (verb));
        }
        else
          vops.push_back ("-q");

        pr = process_start_callback (
          cmd_callback ? cmd_callback : [] (const char* const*, size_t) {},
          0 /* stdin */,
          pipe,
          2 /* stderr */,
          pp,
          vops,
          "-s",
          ops,
          "info:", "'" + project.representation () + "'");

        pipe.out.close ();
        ifdstream is (move (pipe.in), fdstream_mode::skip, ifdstream::badbit);

        auto parse_name = [] (string&& s, const char* what)
        {
          try
          {
            return project_name (move (s));
          }
          catch (const invalid_argument& e)
          {
            bad_value (string (what) + " name '" + s + "': " + e.what ());
          }
        };

        auto parse_dir = [] (string&& s, const char* what)
        {
          try
          {
            return dir_path (move (s));
          }
          catch (const invalid_path& e)
          {
            bad_value (
              string (what) + " directory '" + e.path + "': " + e.what ());
          }
        };

        b_project_info r;
        for (string l; !eof (getline (is, l)); )
        {
          if (l.compare (0, 9, "project: ") == 0)
          {
            string v (l, 9);
            if (!v.empty ())
              r.project = parse_name (move (v), "project");
          }
          else if (l.compare (0, 9, "version: ") == 0)
          {
            string v (l, 9);
            if (!v.empty ())
            try
            {
              r.version = standard_version (v, standard_version::allow_stub);
            }
            catch (const invalid_argument& e)
            {
              bad_value ("version '" + v + "': " + e.what ());
            }
          }
          else if (l.compare (0, 9, "summary: ") == 0)
          {
            r.summary = string (l, 9);
          }
          else if (l.compare (0, 5, "url: ") == 0)
          {
            string v (l, 5);
            if (!v.empty ())
            try
            {
              r.url = url (v);
            }
            catch (const invalid_argument& e)
            {
              bad_value ("url '" + v + "': " + e.what ());
            }
          }
          else if (l.compare (0, 10, "src_root: ") == 0)
          {
            r.src_root = parse_dir (string (l, 10), "src_root");
          }
          else if (l.compare (0, 10, "out_root: ") == 0)
          {
            r.out_root = parse_dir (string (l, 10), "out_root");
          }
          else if (l.compare (0, 14, "amalgamation: ") == 0)
          {
            string v (l, 14);
            if (!v.empty ())
              r.amalgamation = parse_dir (move (v), "amalgamation");
          }
          else if (l.compare (0, 13, "subprojects: ") == 0)
          {
            string v (l, 13);

            for (size_t b (0), e (0); next_word (v, b, e); )
            {
              string s (v, b, e - b);
              size_t p (s.find ('@'));

              if (p == string::npos)
                bad_value ("subproject '" + s + "': missing '@'");

              project_name sn;
              if (p != 0)
                sn = parse_name (string (s, 0, p), "subproject");

              r.subprojects.push_back (
                b_project_info::subproject {move (sn),
                                            parse_dir (string (s, p + 1),
                                                       "subproject")});
            }
          }
          else if (l.compare (0, 12, "operations: ") == 0)
          {
            string v (l, 12);
            for (size_t b (0), e (0); next_word (v, b, e); )
              r.operations.push_back (string (v, b, e - b));
          }
          else if (l.compare (0, 17, "meta-operations: ") == 0)
          {
            string v (l, 17);
            for (size_t b (0), e (0); next_word (v, b, e); )
              r.meta_operations.push_back (string (v, b, e - b));
          }
        }

        is.close (); // Detect errors.

        if (pr.wait ())
          return r;
      }
      // Note that ios::failure inherits from std::runtime_error, so this
      // catch-clause must go last.
      //
      catch (const ios::failure& e)
      {
        // Note: using ostream to get sanitized representation.
        //
        if (pr.wait ())
        {
          ostringstream os;
          os << "error reading " << pp << " output: " << e;
          throw b_error (os.str (), move (pr.exit));
        }
        else if (!pr.exit) // Pipe opening failed before the process started.
        {
          ostringstream os;
          os << "unable to open pipe: " << e;
          throw b_error (os.str ());
        }

        // Fall through.
      }
      catch (const runtime_error& e)
      {
        // Re-throw as b_error if the process exited normally with zero code.
        //
        if (pr.wait ())
          throw b_error (e.what (), move (pr.exit));

        // Fall through.
      }

      // We should only get here if the child exited with an error status.
      //
      assert (!pr.wait ());

      throw b_error (
        string ("process ") + pp.recall_string () + " " + to_string (*pr.exit),
        move (pr.exit));
    }
    catch (const process_error& e)
    {
      ostringstream os;
      os << "unable to execute " << program << ": " << e;
      throw b_error (os.str ());
    }
  }
}
