// file      : libbutl/b.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/b.hxx>

#include <ios>       // ios::failure
#include <cassert>
#include <utility>   // move()
#include <sstream>
#include <algorithm>

#include <libbutl/utility.hxx>      // next_word(), eof(), etc
#include <libbutl/path-io.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/process-io.hxx>   // operator<<(ostream, process_path)
#include <libbutl/small-vector.hxx>

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

  void
  b_info (std::vector<b_project_info>& r,
          const vector<dir_path>& projects,
          b_info_flags fl,
          uint16_t verb,
          const function<b_callback>& cmd_callback,
          const path& program,
          const dir_path& search_fallback,
          const vector<string>& ops)
  {
    // Bail out if the project list is empty.
    //
    if (projects.empty ())
      return;

    // Reserve enough space in the result and save its original size.
    //
    size_t rn (r.size ());
    {
      size_t n (rn + projects.size ());
      if (r.capacity () < n)
        r.reserve (n);
    }

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

        string spec ("info(");

        // Note that quoting is essential here.
        //
        for (size_t i (0); i != projects.size(); ++i)
        {
          if (i != 0)
            spec += ' ';

          spec += '\'' + projects[i].representation () + '\'';
        }

        if ((fl & b_info_flags::subprojects) == b_info_flags::none)
          spec += ",no_subprojects";

        spec += ')';

        pr = process_start_callback (
          cmd_callback ? cmd_callback : [] (const char* const*, size_t) {},
          0 /* stdin */,
          pipe,
          2 /* stderr */,
          pp,
          vops,
          ((fl & b_info_flags::ext_mods) == b_info_flags::none
           ? "--no-external-modules"
           : nullptr),
          "-s",
          ops,
          spec);

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

        b_project_info pi;
        auto add_project = [&r, &pi] ()
        {
          // Parse version string to standard version if the project loaded
          // the version module.
          //
          const auto& ms (pi.modules);
          if (find (ms.begin (), ms.end (), "version") != ms.end ())
          {
            try
            {
              pi.version = standard_version (pi.version_string,
                                             standard_version::allow_stub);
            }
            catch (const invalid_argument& e)
            {
              bad_value ("version '" + pi.version_string + "': " + e.what ());
            }
          }

          // Add the project info and prepare for the next project info
          // parsing.
          //
          r.push_back (move (pi));
          pi = b_project_info ();
        };

        for (string l; !eof (getline (is, l)); )
        {
          if (l.empty ())
          {
            add_project ();
          }
          else if (l.compare (0, 9, "project: ") == 0)
          {
            string v (l, 9);
            if (!v.empty ())
              pi.project = parse_name (move (v), "project");
          }
          else if (l.compare (0, 9, "version: ") == 0)
          {
            pi.version_string = string (l, 9);
          }
          else if (l.compare (0, 9, "summary: ") == 0)
          {
            pi.summary = string (l, 9);
          }
          else if (l.compare (0, 5, "url: ") == 0)
          {
            string v (l, 5);
            if (!v.empty ())
            try
            {
              pi.url = url (v);
            }
            catch (const invalid_argument& e)
            {
              bad_value ("url '" + v + "': " + e.what ());
            }
          }
          else if (l.compare (0, 10, "src_root: ") == 0)
          {
            pi.src_root = parse_dir (string (l, 10), "src_root");
          }
          else if (l.compare (0, 10, "out_root: ") == 0)
          {
            pi.out_root = parse_dir (string (l, 10), "out_root");
          }
          else if (l.compare (0, 14, "amalgamation: ") == 0)
          {
            string v (l, 14);
            if (!v.empty ())
              pi.amalgamation = parse_dir (move (v), "amalgamation");
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

              pi.subprojects.push_back (
                b_project_info::subproject {move (sn),
                                            parse_dir (string (s, p + 1),
                                                       "subproject")});
            }
          }
          else if (l.compare (0, 12, "operations: ") == 0)
          {
            string v (l, 12);
            for (size_t b (0), e (0); next_word (v, b, e); )
              pi.operations.push_back (string (v, b, e - b));
          }
          else if (l.compare (0, 17, "meta-operations: ") == 0)
          {
            string v (l, 17);
            for (size_t b (0), e (0); next_word (v, b, e); )
              pi.meta_operations.push_back (string (v, b, e - b));
          }
          else if (l.compare (0, 9, "modules: ") == 0)
          {
            string v (l, 9);
            for (size_t b (0), e (0); next_word (v, b, e); )
              pi.modules.push_back (string (v, b, e - b));
          }
        }

        is.close (); // Detect errors.

        if (pr.wait ())
        {
          add_project (); // Add the remaining project info.

          if (r.size () - rn == projects.size ())
            return;

          ostringstream os;
          os << "invalid " << pp << " output: expected information for "
             << projects.size () << " projects instead of " << r.size () - rn;
          throw b_error (os.str (), move (pr.exit));
        }
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
        string ("process ") + pp.recall_string () + ' ' + to_string (*pr.exit),
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
