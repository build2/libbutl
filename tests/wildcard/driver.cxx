// file      : tests/wildcard/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>    // sort()
#include <exception>
#include <functional>
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/utility.hxx>      // operator<<(ostream, exception)
#include <libbutl/optional.hxx>
#include <libbutl/filesystem.hxx>
#include <libbutl/path-pattern.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Disable arguments globbing that may be enabled by default for MinGW runtime.
//
// Note that if _CRT_glob symbol is not defined explicitly, then runtime will
// be bound to the one defined in the implicitly linked libmingw32.a library.
// Yet another (but more troublesome) way is to link CRT_noglob.o (from MinGW
// libraries directory) that provides exactly the same symbol definition.
//
#ifdef __MINGW32__
int _CRT_glob = 0;
#endif

// Usages:
//
// argv[0] -mn <name> <pattern>
// argv[0] -sd [-i] [-n] <pattern> [<dir>]
// argv[0] -sp [-i] [-n] <path> <pattern> [<dir>]
//
// Execute actions specified by the first option. Exit with code 0 if succeed,
// 1 if fail, 2 on the underlying OS error (print error description to STDERR).
//
// -mn
//    Match a name against the pattern.
//
// -sd
//    Search for paths matching the pattern in the directory specified (absent
//    directory means the current one). Print the matching canonicalized paths
//    to STDOUT in the ascending order. Succeed if at least one matching path
//    is found. For each matching path we will assert that it is also get
//    matched being searched in the directory tree represented by this path
//    itself.
//
//    Note that the driver excludes from search file system entries which names
//    start from dot, unless the pattern explicitly matches them.
//
// -sp
//    Same as above, but behaves as if the directory tree being searched
//    through contains only the specified entry. The start directory is used if
//    the first pattern component is a self-matching wildcard.
//
// -d (print|stop)
//    If a inaccessible/dangling link is encountered, then print its path to
//    stderr and, optionally, stop the search. Meaningful in combination with
//    -sd and must follow it, if specified in the command line.
//
// -i
//    Pass path_match_flags::match_absent to the match/search functions.
//    Meaningful in combination with -sd or -sp options and must follow it, if
//    specified in the command line.
//
// -n
//    Do not sort paths found. Meaningful in combination with -sd or -sp
//    options and must follow it, if specified in the command line.
//
int
main (int argc, const char* argv[])
try
{
  using butl::optional;

  assert (argc >= 2);

  string op (argv[1]);

  if (op == "-mn")
  {
    assert (argc == 4);

    string name (argv[2]);
    string pattern (argv[3]);
    return path_match (name, pattern) ? 0 : 1;
  }
  else if (op == "-sd" || op == "-sp")
  {
    assert (argc >= (op == "-sd" ? 3 : 4));

    bool sort (true);
    path_match_flags flags (path_match_flags::follow_symlinks);

    bool dangle_stop (false);
    function<bool (const dir_entry&)> dangle_func;

    int i (2);
    for (; i != argc; ++i)
    {
      string o (argv[i]);
      if (o == "-n")
        sort = false;
      else if (o == "-i")
        flags |= path_match_flags::match_absent;
      else if (o == "-d")
      {
        ++i;

        assert (op == "-sd" && i != argc);

        string v (argv[i]);

        if (v == "print")
        {
          dangle_func = [] (const dir_entry& de)
          {
            cerr << de.base () / de.path () << endl;
            return true;
          };
        }
        else if (v == "stop")
        {
          dangle_func = [&dangle_stop] (const dir_entry& de)
          {
            cerr << de.base () / de.path () << endl;
            dangle_stop = true;
            return false;
          };
        }
        else
          assert (false);
      }
      else
        break; // End of options.
    }

    optional<path> entry;

    if (op == "-sp")
    {
      assert (i != argc);
      entry = path (argv[i++]);
    }

    assert (i != argc); // Still need pattern.
    path pattern (argv[i++]);

    dir_path start;
    if (i != argc)
      start = dir_path (argv[i++]);

    assert (i == argc); // All args parsed,

    vector<path> paths;
    map<path, size_t> path_count;

    auto add = [&paths, &path_count, &start] (path&& p,
                                              const string& pt,
                                              bool interim)
    {
      bool pd (!pt.empty () && pt[0] == '.'); // Dot-started pattern.

      const path& fp (!p.empty ()
                      ? p
                      : path_cast<path> (!start.empty ()
                                         ? start
                                         : path::current_directory ()));

      const string& s (fp.leaf ().string ());
      assert (!s.empty ());

      bool ld (s[0] == '.'); // Dot-started leaf.

      // Skip dot-started names if pattern is not dot-started.
      //
      bool skip (ld && !pd);

      if (interim)
        return !skip;

      if (!skip)
      {
        p.canonicalize ();

        auto i (path_count.find (p));
        if (i == path_count.end ())
          path_count[p] = 1;
        else
          ++(i->second);

        paths.emplace_back (move (p));
      }

      return true;
    };

    if (!entry)
      path_search (pattern, add, start, flags, dangle_func);
    else
      path_search (pattern, *entry, add, start, flags);

    if (dangle_stop)
      return 1;

    // It the search succeeds, then test search in the directory tree
    // represented by each matched path. Otherwise, if the directory tree is
    // specified, then make sure that it doesn't match the pattern.
    //
    if (!path_count.empty ())
    {
      for (const auto& p: path_count)
      {
        // Will match multiple times if the pattern contains several recursive
        // components.
        //
        size_t match_count (0);

        auto check = [&p, &match_count, flags]
                     (path&& pe, const string&, bool inter)
        {
          if (pe == p.first)
          {
            if (!inter)
              ++match_count;
            else if ((flags & path_match_flags::match_absent) ==
                     path_match_flags::none)
              // For self-matching the callback is first called in the interim
              // mode (through the preopen function) with an empty path.
              //
              assert (pe.empty ());
          }

          return true;
        };

        path_search (pattern, p.first, check, start, flags);
        assert (match_count == p.second);

        // Test path match.
        //
        assert (path_match (p.first, pattern, start, flags));
      }
    }
    else if (entry)
      assert (!path_match (*entry, pattern, start, flags));

    // Print the found paths.
    //
    if (sort)
      std::sort (paths.begin (), paths.end ());

    for (const auto& p: paths)
      cout << p.representation () << endl;

    return paths.empty () ? 1 : 0;
  }
  else
    assert (false);
}
catch (const invalid_path& e)
{
  cerr << e << ": " << e.path << endl;
  return 2;
}
catch (const system_error& e)
{
  cerr << e << endl;
  return 3;
}
catch (const exception& e)
{
  cerr << e << endl;
  return 4;
}
