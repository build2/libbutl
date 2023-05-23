// file      : tests/builtin/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifdef _WIN32
#  include <libbutl/win32-utility.hxx>
#endif

#include <string>
#include <vector>
#include <chrono>
#include <utility>  // move()
#include <cstdint>  // uint8_t
#include <ostream>
#include <iostream>
#ifndef _WIN32
#  include <thread> // this_thread::sleep_for()
#endif

#include <libbutl/path.hxx>
#include <libbutl/utility.hxx>   // eof()
#include <libbutl/builtin.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/timestamp.hxx> // to_stream(duration)

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Disable arguments globbing that may be enabled by default for MinGW runtime
// (see tests/wildcard/driver.cxx for details).
//
#ifdef __MINGW32__
int _CRT_glob = 0;
#endif

inline ostream&
operator<< (ostream& os, const path& p)
{
  return os << p.representation ();
}

// Usage: argv[0] [-d <dir>] [-o <opt>] [-c] [-i] [-t <msec>] [-s <sec>]
//        <builtin> <builtin-args>
//
// Execute the builtin and exit with its exit status.
//
// -d <dir>   use as a current working directory
// -c         use callbacks that, in particular, trace calls to stdout
// -o <opt>   additional builtin option recognized by the callback
// -i         read lines from stdin and append them to the builtin arguments
// -t <msec>  print diag if the builtin didn't complete in <msec> milliseconds
// -s <sec>   sleep <sec> seconds prior to running the builtin
//
// Note that the 'roundtrip' builtin name is also recognized and results in
// running the pseudo-builtin that just roundtrips stdin to stdout.
//
int
main (int argc, char* argv[])
{
  using butl::optional;

  cin.exceptions  (ios::badbit);
  cout.exceptions (ios::failbit | ios::badbit);
  cerr.exceptions (ios::failbit | ios::badbit);

  bool in (false);
  dir_path cwd;
  string option;
  builtin_callbacks callbacks;
  optional<duration> timeout;
  optional<chrono::seconds> sec;

  string name;
  vector<string> args;

  auto flag = [] (bool v) {return v ? "true" : "false";};

  auto num = [] (const string& s)
  {
    assert (!s.empty ());

    char* e (nullptr);
    errno = 0; // We must clear it according to POSIX.
    uint64_t r (strtoull (s.c_str (), &e, 10)); // Can't throw.
    assert (errno != ERANGE && e == s.c_str () + s.size ());
    return r;
  };

  // Parse the driver options and arguments.
  //
  int i (1);
  for (; i != argc; ++i)
  {
    string a (argv[i]);

    if (a == "-d")
    {
      ++i;

      assert (i != argc);
      cwd = dir_path (argv[i]);
    }
    else if (a == "-o")
    {
      ++i;

      assert (i != argc);
      option = argv[i];
    }
    else if (a == "-c")
    {
      callbacks = builtin_callbacks (
        [&flag] (const path& p, bool pre)
        {
          cout << "create " << p << ' ' << flag (pre) << endl;
        },
        [&flag] (const path& from, const path& to, bool force, bool pre)
        {
          cout << "move " << from << ' ' << to << ' ' << flag (force) << ' '
                          << flag (pre) << endl;
        },
        [&flag] (const path& p, bool force, bool pre)
        {
          cout << "remove " << p << ' ' << flag (force) << ' ' << flag (pre)
                            << endl;
        },
        [&option] (const vector<string>& args, size_t i)
        {
          cout << "option " << args[i] << endl;
          return !option.empty () && args[i] == option ? 1 : 0;
        },
        [] (const duration& d)
        {
          cout << "sleep ";
          to_stream (cout, d, false /* nanoseconds */);
          cout << endl;
        }
      );
    }
    else if (a == "-i")
    {
      in = true;
    }
    else if (a == "-t")
    {
      ++i;

      assert (i != argc);
      timeout = chrono::milliseconds (num (argv[i]));
    }
    else if (a == "-s")
    {
      ++i;

      assert (i != argc);
      sec = chrono::seconds (num (argv[i]));
    }
    else
      break;
  }

  // Parse the builtin name and arguments.
  //
  assert (i != argc);
  name = argv[i++];

  for (; i != argc; ++i)
    args.push_back (argv[i]);

  // Read out additional arguments from stdin.
  //
  if (in)
  {
    string s;
    while (!eof (getline (cin, s)))
      args.push_back (move (s));
  }

  auto sleep = [&sec] ()
  {
    if (sec)
    {
      // MINGW GCC 4.9 doesn't implement this_thread so use Win32 Sleep().
      //
#ifndef _WIN32
      this_thread::sleep_for (*sec);
#else
      Sleep (static_cast<DWORD> (sec->count () * 1000));
#endif
    }
  };

  auto wait = [&timeout] (builtin& b)
  {
    optional<uint8_t> r;

    if (timeout)
    {
      r = b.timed_wait (*timeout);

      if (!r)
      {
        cerr << "timeout expired" << endl;

        b.wait ();
        r = 1;
      }
    }
    else
      r = b.wait ();

    assert (b.try_wait ()); // While at it, test try_wait().

    return *r;
  };

  // Execute the builtin.
  //
  if (name != "roundtrip")
  {
    const builtin_info* bi (builtins.find (name));

    if (bi == nullptr)
    {
      cerr << "unknown builtin '" << name << "'" << endl;
      return 1;
    }

    if (bi->function == nullptr)
    {
      cerr << "external builtin '" << name << "'" << endl;
      return 1;
    }

    sleep ();

    uint8_t r; // Storage.
    builtin b (bi->function (r, args, nullfd, nullfd, nullfd, cwd, callbacks));
    return wait (b);
  }
  else
  {
    uint8_t r; // Storage.

    auto run = [&r, &sleep] ()
    {
      // While at it, test that a non-copyable lambda can be used as a
      // builtin.
      //
      auto_fd fd;

      return pseudo_builtin (
        r,
        [&sleep, fd = move (fd)] () mutable noexcept
        {
          fd.reset ();

          sleep ();

          if (cin.peek () != istream::traits_type::eof ())
            cout << cin.rdbuf ();

          return 0;
        });
    };

    builtin b (run ());
    return wait (b);
  }
}
