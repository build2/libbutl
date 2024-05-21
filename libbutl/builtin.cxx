// file      : libbutl/builtin.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/builtin.hxx>

#ifdef _WIN32
#  include <libbutl/win32-utility.hxx>
#endif

#include <ios>
#include <chrono>
#include <cerrno>
#include <cassert>
#include <ostream>
#include <sstream>
#include <cstdlib>      // strtoull()
#include <cstring>      // strcmp()
#include <exception>
#include <system_error>

#include <libbutl/regex.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/utility.hxx>      // operator<<(ostream,exception),
                                    // throw_generic_error()
#include <libbutl/optional.hxx>
#include <libbutl/filesystem.hxx>
#include <libbutl/small-vector.hxx>

#include <libbutl/builtin-options.hxx>

// Strictly speaking a builtin which reads/writes from/to standard streams
// must be asynchronous so that the caller can communicate with it through
// pipes without being blocked on I/O operations. However, as an optimization,
// we allow builtins that only print diagnostics to STDERR to be synchronous
// assuming that their output will always fit the pipe buffer. Synchronous
// builtins must not read from STDIN and write to STDOUT. Later we may relax
// this rule to allow a "short" output for such builtins.
//
using namespace std;

namespace butl
{
  using strings = vector<string>;
  using io_error = ios_base::failure;

  using builtin_impl = uint8_t (const strings& args,
                                auto_fd in, auto_fd out, auto_fd err,
                                const dir_path& cwd,
                                const builtin_callbacks&);

  // Operation failed, diagnostics has already been issued.
  //
  struct failed {};

  // Accumulate an error message, print it atomically in dtor to the provided
  // stream and throw failed afterwards if requested. Prefixes the message
  // with the builtin name.
  //
  // Move constructible-only, not assignable (based to diag_record).
  //
  class error_record
  {
  public:
    template <typename T>
    friend const error_record&
    operator<< (const error_record& r, const T& x)
    {
      r.ss_ << x;
      return r;
    }

    error_record (ostream& o, bool fail, const char* name)
        : os_ (o), fail_ (fail), empty_ (false)
    {
      ss_ << name << ": ";
    }

    // Older versions of libstdc++ don't have the ostringstream move support.
    // Luckily, GCC doesn't seem to be actually needing move due to copy/move
    // elision.
    //
#ifdef __GLIBCXX__
    error_record (error_record&&);
#else
    error_record (error_record&& r)
        : os_ (r.os_),
          ss_ (move (r.ss_)),
          fail_ (r.fail_),
          empty_ (r.empty_)
    {
      r.empty_ = true;
    }
#endif

    ~error_record () noexcept (false)
    {
      if (!empty_)
      {
        // The output stream can be in a bad state (for example as a result of
        // unsuccessful attempt to report a previous error), so we check it.
        //
        if (os_.good ())
        {
          ss_.put ('\n');
          os_ << ss_.str ();
          os_.flush ();
        }

        if (fail_)
          throw failed ();
      }
    }

  private:
    ostream& os_;
    mutable ostringstream ss_;

    bool fail_;
    bool empty_;
  };

  // Call a function returning its resulting value. Fail if an exception is
  // thrown by the function.
  //
  template <typename F, typename... A>
  static inline auto
  call (const function<error_record ()>& fail,
        const function<F>& fn,
        A&&... args) -> decltype (fn (forward<A> (args)...))
  {
    assert (fn);

    try
    {
      return fn (forward<A> (args)...);
    }
    catch (const std::exception& e)
    {
      fail () << e;
    }
    catch (...)
    {
      fail () << "unknown error";
    }

    assert (false); // Can't be here.
    throw failed ();
  }

  // Parse builtin options. Call the callback to parse unknown options and
  // throw cli::unknown_option if the callback is not specified or doesn't
  // parse the option.
  //
  template <typename O>
  static O
  parse (cli::vector_scanner& scan,
         const strings& args,
         const function<builtin_callbacks::parse_option_function>& parse,
         const function<error_record ()>& fail)
  {
    O ops;

    while (true)
    {
      // Parse the next chunk of options until we reach an argument, --,
      // unknown option, or eos.
      //
      ops.parse (scan, cli::unknown_mode::stop);

      // Bail out on eos.
      //
      if (!scan.more ())
        break;

      const char* o (scan.peek ());

      // Bail out on --.
      //
      if (strcmp (o, "--") == 0)
      {
        scan.next (); // Skip --.
        break;
      }

      // Bail out on an argument.
      //
      if (!(o[0] == '-' && o[1] != '\0'))
        break;

      // Parse the unknown option if the callback is specified and fail if
      // that's not the case or the callback doesn't recognize the option
      // either.
      //
      size_t n (parse ? call (fail, parse, args, scan.end ()) : 0);

      if (n == 0)
        throw cli::unknown_option (o);

      // Skip the parsed arguments and continue.
      //
      assert (scan.end () + n <= args.size ());
      scan.reset (scan.end () + n);
    }

    return ops;
  }

  // Parse and normalize a path. Also make a relative path absolute using the
  // specified directory path if it is not empty (in which case it must be
  // absolute). Fail if the path is empty, and on parsing and normalization
  // errors.
  //
  static path
  parse_path (string s,
              const dir_path& d,
              const function<error_record ()>& fail)
  {
    assert (d.empty () || d.absolute ());

    try
    {
      path p (move (s));

      if (p.empty ())
        throw invalid_path ("");

      if (p.relative () && !d.empty ())
        p = d / move (p);

      p.normalize ();
      return p;
    }
    catch (const invalid_path& e)
    {
      fail () << "invalid path '" << e.path << "'";
    }

    assert (false); // Can't be here.
    return path ();
  }

  // Return the current working directory if wd is empty and wd otherwise,
  // completed using the current directory if it is relative. Fail if
  // std::system_error is thrown by the underlying function call.
  //
  static dir_path
  current_directory (const dir_path& wd, const function<error_record ()>& fail)
  {
    try
    {
      if (wd.empty ())
        return dir_path::current_directory ();

      if (wd.relative ())
        return move (dir_path (wd).complete ());
    }
    catch (const system_error& e)
    {
      fail () << "unable to obtain current directory: " << e;
    }

    return wd;
  }

  // Builtin commands functions.
  //

  // cat <file>...
  //
  // Note that POSIX doesn't specify if after I/O operation failure the
  // command should proceed with the rest of the arguments. The current
  // implementation exits immediatelly in such a case.
  //
  // @@ Shouldn't we check that we don't print a nonempty regular file to
  //    itself, as that would merely exhaust the output device? POSIX allows
  //    (but not requires) such a check and some implementations do this. That
  //    would require to fstat() file descriptors and complicate the code a
  //    bit. Was able to reproduce on a big file (should be bigger than the
  //    stream buffer size) with the test `cat file >+file`.
  //
  // Note: must be executed asynchronously.
  //
  static uint8_t
  cat (const strings& args,
       auto_fd in, auto_fd out, auto_fd err,
       const dir_path& cwd,
       const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "cat");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      ifdstream cin (in != nullfd ? move (in) : fddup (stdin_fd ()),
                     fdstream_mode::binary);

      ofdstream cout (out != nullfd ? move (out) : fddup (stdout_fd ()),
                      fdstream_mode::binary);

      // Parse arguments.
      //
      cli::vector_scanner scan (args);
      parse<cat_options> (scan, args, cbs.parse_option, fail);

      // Print files.
      //
      // Copy input stream to STDOUT.
      //
      auto copy = [&cout] (istream& is)
      {
        if (is.peek () != ifdstream::traits_type::eof ())
          cout << is.rdbuf ();

        is.clear (istream::eofbit); // Sets eofbit.
      };

      // Path of a file being printed to STDOUT. An empty path represents
      // STDIN. Used in diagnostics.
      //
      path p;

      try
      {
        // Print STDIN.
        //
        if (!scan.more ())
          copy (cin);

        dir_path wd;

        // Print files.
        //
        while (scan.more ())
        {
          string f (scan.next ());

          if (f == "-")
          {
            if (!cin.eof ())
            {
              p.clear ();
              copy (cin);
            }

            continue;
          }

          if (wd.empty () && cwd.relative ())
            wd = current_directory (cwd, fail);

          p = parse_path (move (f), !wd.empty () ? wd : cwd, fail);

          ifdstream is (p, fdopen_mode::binary);
          copy (is);
          is.close ();
        }
      }
      catch (const io_error& e)
      {
        error_record d (fail ());
        d << "unable to print ";

        if (p.empty ())
          d << "stdin";
        else
          d << "'" << p << "'";

        d << ": " << e;
      }

      cin.close ();
      cout.close ();
      r = 0;
    }
    // Can be thrown while creating/closing cin, cout or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // Make a copy of a file at the specified path, preserving permissions, and
  // calling the hook for a newly created file. The file paths must be
  // absolute and normalized. Fail if an exception is thrown by the underlying
  // copy operation.
  //
  static void
  cpfile (const path& from, const path& to,
          bool overwrite,
          bool attrs,
          const builtin_callbacks& cbs,
          const function<error_record ()>& fail)
  {
    assert (from.absolute () && from.normalized ());
    assert (to.absolute () && to.normalized ());

    try
    {
      if (cbs.create)
        call (fail, cbs.create, to, true /* pre */);

      cpflags f (overwrite
                 ? cpflags::overwrite_permissions | cpflags::overwrite_content
                 : cpflags::none);

      if (attrs)
        f |= cpflags::overwrite_permissions | cpflags::copy_timestamps;

      cpfile (from, to, f);

      if (cbs.create)
        call (fail, cbs.create, to, false /* pre */);
    }
    catch (const system_error& e)
    {
      fail () << "unable to copy file '" << from << "' to '" << to << "': "
              << e;
    }
  }

  // Make a copy of a directory at the specified path, calling the hook for
  // the created filesystem entries. The directory paths must be absolute and
  // normalized. Fail if the destination directory already exists or an
  // exception is thrown by the underlying copy operation.
  //
  static void
  cpdir (const dir_path& from, const dir_path& to,
         bool attrs,
         const builtin_callbacks& cbs,
         const function<error_record ()>& fail)
  {
    assert (from.absolute () && from.normalized ());
    assert (to.absolute () && to.normalized ());

    try
    {
      if (cbs.create)
        call (fail, cbs.create, to, true /* pre */);

      if (try_mkdir (to) == mkdir_status::already_exists)
        throw_generic_error (EEXIST);

      if (cbs.create)
        call (fail, cbs.create, to, false /* pre */);

      for (const auto& de: dir_iterator (from, dir_iterator::no_follow))
      {
        path f (from / de.path ());
        path t (to / de.path ());

        if (de.type () == entry_type::directory)
          cpdir (path_cast<dir_path> (move (f)),
                 path_cast<dir_path> (move (t)),
                 attrs,
                 cbs,
                 fail);
        else
          cpfile (f, t, false /* overwrite */, attrs, cbs, fail);
      }

      // Note that it is essential to copy timestamps and permissions after
      // the directory content is copied.
      //
      if (attrs)
      {
        path_permissions (to, path_permissions (from));
        dir_time (to, dir_time (from));
      }
    }
    catch (const system_error& e)
    {
      fail () << "unable to copy directory '" << from << "' to '" << to
              << "': " << e;
    }
  }

  // cp [-p|--preserve]                   <src-file>    <dst-file>
  // cp [-p|--preserve] -R|-r|--recursive <src-dir>     <dst-dir>
  // cp [-p|--preserve]                   <src-file>... <dst-dir>/
  // cp [-p|--preserve] -R|-r|--recursive <src-path>... <dst-dir>/
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  cp (const strings& args,
      auto_fd in, auto_fd out, auto_fd err,
      const dir_path& cwd,
      const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "cp");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);
      cp_options ops (parse<cp_options> (scan, args, cbs.parse_option, fail));

      // Copy files or directories.
      //
      if (!scan.more ())
        fail () << "missing arguments";

      // Note that the arguments semantics depends on the last argument, so we
      // read out and cache them.
      //
      small_vector<string, 2> args;
      while (scan.more ())
        args.push_back (scan.next ());

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      auto i (args.begin ());
      auto j (args.rbegin ());
      path dst (parse_path (move (*j++), wd, fail));
      auto e (j.base ());

      if (i == e)
        fail () << "missing source path";

      // If destination is not a directory path (no trailing separator) then
      // make a copy of the filesystem entry at the specified path (the only
      // source path is allowed in such a case). Otherwise copy the source
      // filesystem entries into the destination directory.
      //
      if (!dst.to_directory ())
      {
        path src (parse_path (move (*i++), wd, fail));

        // If there are multiple sources but no trailing separator for the
        // destination, then, most likelly, it is missing.
        //
        if (i != e)
          fail () << "multiple source paths without trailing separator for "
                  << "destination directory";

        if (!ops.recursive ())
          // Synopsis 1: make a file copy at the specified path.
          //
          cpfile (src, dst, true /* overwrite */, ops.preserve (), cbs, fail);
        else
          // Synopsis 2: make a directory copy at the specified path.
          //
          cpdir (path_cast<dir_path> (src), path_cast<dir_path> (dst),
                 ops.preserve (),
                 cbs,
                 fail);
      }
      else
      {
        for (; i != e; ++i)
        {
          path src (parse_path (move (*i), wd, fail));

          if (ops.recursive () && dir_exists (src))
            // Synopsis 4: copy a filesystem entry into the specified
            // directory. Note that we handle only source directories here.
            // Source files are handled below.
            //
            cpdir (path_cast<dir_path> (src),
                   path_cast<dir_path> (dst / src.leaf ()),
                   ops.preserve (),
                   cbs,
                   fail);
          else
            // Synopsis 3: copy a file into the specified directory. Also,
            // here we cover synopsis 4 for the source path being a file.
            //
            cpfile (src, dst / src.leaf (),
                    true /* overwrite */,
                    ops.preserve (),
                    cbs,
                    fail);
        }
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // date [-u|--utc] [+<format>]
  //
  // Note: must be executed asynchronously.
  //
  static uint8_t
  date (const strings& args,
        auto_fd in, auto_fd out, auto_fd err,
        const dir_path&,
        const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "date");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      ofdstream cout (out != nullfd ? move (out) : fddup (stdout_fd ()));

      // Parse arguments.
      //
      cli::vector_scanner scan (args);

      date_options ops (
        parse<date_options> (scan, args, cbs.parse_option, fail));

      const char* format;

      string fs; // Storage.
      if (scan.more ())
      {
        fs = scan.next ();
        if (fs[0] != '+')
          fail () << "date format argument must start with '+'";

        format = fs.c_str () + 1;
      }
      else
        format = "%a %b %e %H:%M:%S %Z %Y";

      if (scan.more ())
        fail () << "unexpected argument '" << scan.next () << "'";

      // Print the current time.
      //
      try
      {
        to_stream (cout,
                   system_clock::now (),
                   format,
                   false /* special */,
                   !ops.utc ());
      }
      catch (const system_error& e)
      {
        fail () << "unable to print time in format '" << format << "': " << e;
      }

      cout << '\n';
      cout.close ();
      r = 0;
    }
    // Can be thrown while closing cin or creating, writing to, or closing
    // cout.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // echo <string>...
  //
  // Note: must be executed asynchronously.
  //
  static uint8_t
  echo (const strings& args,
        auto_fd in, auto_fd out, auto_fd err,
        const dir_path&,
        const builtin_callbacks&) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    try
    {
      in.close ();
      ofdstream cout (out != nullfd ? move (out) : fddup (stdout_fd ()));

      for (auto b (args.begin ()), i (b), e (args.end ()); i != e; ++i)
        cout << (i != b ? " " : "") << *i;

      cout << '\n';
      cout.close ();
      r = 0;
    }
    // Can be thrown while closing cin or creating, writing to, or closing
    // cout.
    //
    catch (const io_error& e)
    {
      cerr << "echo: " << e << endl;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // false
  //
  // Failure to close the file descriptors is silently ignored.
  //
  // Note: can be executed synchronously.
  //
  static builtin
  false_ (uint8_t& r,
          const strings&,
          auto_fd, auto_fd, auto_fd,
          const dir_path&,
          const builtin_callbacks&)
  {
    return builtin (r = 1);
  }

  // true
  //
  // Failure to close the file descriptors is silently ignored.
  //
  // Note: can be executed synchronously.
  //
  static builtin
  true_ (uint8_t& r,
         const strings&,
         auto_fd, auto_fd, auto_fd,
         const dir_path&,
         const builtin_callbacks&)
  {
    return builtin (r = 0);
  }

  // find <start-path>... [-name <pattern>]
  //                      [-type <type>]
  //                      [-mindepth <depth>]
  //                      [-maxdepth <depth>]
  //
  // Note: must be executed asynchronously.
  //
  static uint8_t
  find (const strings& args,
        auto_fd in, auto_fd out, auto_fd err,
        const dir_path& cwd,
        const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    // Note that on some errors we will issue diagnostics but continue the
    // search and return with non-zero code at the end. This is consistent
    // with how major implementations behave (see below).
    //
    bool error_occured (false);
    auto error = [&cerr, &error_occured] (bool fail = false)
    {
      error_occured = true;
      return error_record (cerr, fail, "find");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      ofdstream cout (out != nullfd ? move (out) : fddup (stdout_fd ()));

      // Parse arguments.
      //
      cli::vector_scanner scan (args);

      // Currently, we don't expect any options.
      //
      parse<find_options> (scan, args, cbs.parse_option, fail);

      // Parse path arguments until the first primary (starts with '-') is
      // encountered.
      //
      small_vector<path, 1> paths;

      while (scan.more ())
      {
        if (*scan.peek () == '-')
          break;

        try
        {
          paths.emplace_back (scan.next ());
        }
        catch (const invalid_path& e)
        {
          fail () << "invalid path '" << e.path << "'";
        }
      }

      // Note that POSIX doesn't explicitly describe the behavior if no paths
      // are specified on the command line. On Linux the current directory is
      // assumed in this case. We, however, will follow the FreeBSD behavior
      // and fail since this seems to be less error-prone.
      //
      if (paths.empty ())
        fail () << "missing start path";

      // Parse primaries.
      //
      optional<string>     name;
      optional<entry_type> type;
      optional<uint64_t>   min_depth;
      optional<uint64_t>   max_depth;

      while (scan.more ())
      {
        const char* p (scan.next ());

        // Return the string value of the current primary. Fail if absent or
        // empty, unless empty value is allowed.
        //
        auto str = [p, &scan, &fail] (bool allow_empty = false)
        {
          if (!scan.more ())
          {
            fail () << "missing value for primary '" << p << "'";
          }

          string n (p); // Save for diagnostics.
          string r (scan.next ());

          if (r.empty () && !allow_empty)
            fail () << "empty value for primary '" << n << "'";

          return r;
        };

        // Return the unsigned numeric value of the current primary. Fail if
        // absent or is not a valid number.
        //
        auto num = [p, &str, &fail] ()
        {
          string n (p); // Save for diagnostics.
          string s (str ());

          const char* b (s.c_str ());
          char* e (nullptr);
          errno = 0; // We must clear it according to POSIX.
          uint64_t r (strtoull (b, &e, 10)); // Can't throw.

          if (errno == ERANGE || e != b + s.size ())
            fail () << "invalid value '" << s << "' for primary '" << n << "'";

          return r;
        };

        if (strcmp (p, "-name") == 0)
        {
          // Note that the empty never-matching pattern is allowed.
          //
          name = str (true /* allow_empty */);
        }
        else if (strcmp (p, "-type") == 0)
        {
          string s (str ());
          char t (s.size () == 1 ? s[0] : '\0');

          switch (t)
          {
          case 'f': type = entry_type::regular;   break;
          case 'd': type = entry_type::directory; break;
          case 'l': type = entry_type::symlink;   break;
          default: fail () << "invalid value '" << s << "' for primary '-type'";
          }
        }
        else if (strcmp (p, "-mindepth") == 0)
        {
          min_depth = num ();
        }
        else if (strcmp (p, "-maxdepth") == 0)
        {
          max_depth = num ();
        }
        else
          fail () << "unknown primary '" << p << "'";
      }

      // Print the path if the expression evaluates to true for it. Traverse
      // further down if the path refers to a directory and the maximum depth
      // is not specified or is not reached.
      //
      // Note that paths for evaluating/printing (pp) and for
      // stating/traversing (ap) are passed separately. The former is
      // potentially relative and the latter is absolute. Also note that
      // for optimization we separately pass the base name simple path.
      //
      auto find = [&cout,
                   &name,
                   &type,
                   &min_depth,
                   &max_depth,
                   &fail] (const path& pp,
                           const path& ap,
                           const path& bp,
                           entry_type t,
                           uint64_t level,
                           const auto& find) -> void
      {
        // Print the path if no primary evaluates to false.
        //
        if ((!type      || *type == t)          &&
            (!min_depth || level >= *min_depth) &&
            (!name      || path_match (bp.string (), *name)))
        {
          // Print the trailing directory separator, if present.
          //
          if (pp.to_directory ())
          {
            // The trailing directory separator can only be present for
            // paths specified on the command line.
            //
            assert (level == 0);

            cout << pp.representation () << '\n';
          }
          else
            cout << pp << '\n';
        }

        // Traverse the directory, unless the max depth is specified and
        // reached.
        //
        if (t == entry_type::directory && (!max_depth || level < *max_depth))
        try
        {
          for (const auto& de: dir_iterator (path_cast<dir_path> (ap),
                                             dir_iterator::no_follow))
          {
            find (pp / de.path (),
                  ap / de.path (),
                  de.path (),
                  de.ltype (),
                  level + 1,
                  find);
          }
        }
        catch (const system_error& e)
        {
          fail () << "unable to scan directory '" << pp << "': " << e;
        }
      };

      dir_path wd;

      for (const path& p: paths)
      {
        // Complete the path if it is relative, so that we can properly stat
        // it and, potentially, traverse. Note that we don't normalize it
        // since POSIX requires that the paths should be evaluated (by
        // primaries) and printed unaltered.
        //
        path ap;

        if (p.relative ())
        {
          if (wd.empty () && cwd.relative ())
            wd = current_directory (cwd, fail);

          ap = (!wd.empty () ? wd : cwd) / p;
        }

        // Issue an error if the path is empty, doesn't exist, or has the
        // trailing directory separator but refers to a non-directory.
        //
        // Note that POSIX doesn't explicitly describe the behavior if any of
        // the above happens. We will follow the behavior which is common for
        // both Linux and FreeBSD by issuing the diagnostics, proceeding to
        // the subsequent paths, and returning with non-zero code at the end.
        //
        if (p.empty ())
        {
          error () << "empty path";
          continue;
        }

        const path& fp (!ap.empty () ? ap : p);
        pair<bool, entry_stat> pe;

        try
        {
          pe = path_entry (fp);
        }
        catch (const system_error& e)
        {
          fail () << "unable to stat '" << p << "': " << e;
        }

        if (!pe.first)
        {
          error () << "'" << p << "' doesn't exists";
          continue;
        }

        entry_type t (pe.second.type);

        if (p.to_directory () && t != entry_type::directory)
        {
          error () << "'" << p << "' is not a directory";
          continue;
        }

        find (p, fp, p.leaf (), t, 0 /* level */, find);
      }

      cout.close ();
      r = !error_occured ? 0 : 1;
    }
    // Can be thrown while closing cin or creating, writing to, or closing
    // cout or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // Create a symlink to a file or directory at the specified path and calling
  // the hook for the created filesystem entries. The paths must be absolute
  // and normalized. Fall back to creating a hardlink, if symlink creation is
  // not supported for the link path. If hardlink creation is not supported
  // either, then fall back to copies. Fail if the target filesystem entry
  // doesn't exist or an exception is thrown by the underlying filesystem
  // operation (specifically for an already existing filesystem entry at the
  // link path).
  //
  // Note that supporting optional removal of an existing filesystem entry at
  // the link path (the -f option) tends to get hairy. Also removing non-empty
  // directories doesn't look very natural, but would be required if we want
  // the behavior on POSIX and Windows to be consistent.
  //
  static void
  mksymlink (const path& target, const path& link,
             const builtin_callbacks& cbs,
             const function<error_record ()>& fail)
  {
    assert (link.absolute () && link.normalized ());

    // Determine the target type, fail if the target doesn't exist. Note that
    // to do that we need to complete a relative target path using the link
    // directory making the target path absolute.
    //
    const path& atp (target.relative ()
                     ? link.directory () / target
                     : target);

    bool dir (false);

    try
    {
      pair<bool, entry_stat> pe (path_entry (atp));

      if (!pe.first)
        fail () << "unable to create symlink to '" << atp << "': no such "
                << "file or directory";

      dir = pe.second.type == entry_type::directory;
    }
    catch (const system_error& e)
    {
      fail () << "unable to stat '" << atp << "': " << e;
    }

    // First we try to create a symlink. If that fails (e.g., "Windows
    // happens"), then we resort to hard links. If that doesn't work out
    // either (e.g., not on the same filesystem), then we fall back to copies.
    // So things are going to get a bit nested.
    //
    // Note: similar to mkanylink() but with support for directories.
    //
    try
    {
      if (cbs.create)
        call (fail, cbs.create, link, true /* pre */);

      mksymlink (target, link, dir);

      if (cbs.create)
        call (fail, cbs.create, link, false /* pre */);
    }
    catch (const system_error& e)
    {
      // Note that we are not guaranteed (here and below) that the
      // system_error exception is of the generic category.
      //
      int c (e.code ().value ());
      if (!(e.code ().category () == generic_category () &&
            (c == ENOSYS || // Not implemented.
             c == EPERM)))  // Not supported by the filesystem(s).
        fail () << "unable to create symlink '" << link << "' to '"
                << atp << "': " << e;

      // Note that for hardlinking/copying we need to use the complete
      // (absolute) target path.
      //
      try
      {
        mkhardlink (atp, link, dir);

        if (cbs.create)
          call (fail, cbs.create, link, false /* pre */);
      }
      catch (const system_error& e)
      {
        c = e.code ().value ();
        if (!(e.code ().category () == generic_category () &&
              (c == ENOSYS || // Not implemented.
               c == EPERM  || // Not supported by the filesystem(s).
               c == EXDEV)))  // On different filesystems.
          fail () << "unable to create hardlink '" << link << "' to '" << atp
                  << "': " << e;

        if (dir)
          cpdir (path_cast<dir_path> (atp), path_cast<dir_path> (link),
                 false /* attrs */,
                 cbs,
                 fail);
        else
          cpfile (atp, link,
                  false /* overwrite */,
                  true /* attrs */,
                  cbs,
                  fail);
      }
    }
  }

  // ln -s|--symbolic <target-path>    <link-path>
  // ln -s|--symbolic <target-path>... <link-dir>/
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  ln (const strings& args,
      auto_fd in, auto_fd out, auto_fd err,
      const dir_path& cwd,
      const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "ln");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);
      ln_options ops (parse<ln_options> (scan, args, cbs.parse_option, fail));

      if (!ops.symbolic ())
        fail () << "missing -s|--symbolic option";

      // Create file or directory symlinks.
      //
      if (!scan.more ())
        fail () << "missing arguments";

      // Note that the arguments semantics depends on the last argument, so we
      // read out and cache them.
      //
      small_vector<string, 2> args;
      while (scan.more ())
        args.push_back (scan.next ());

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      auto i (args.begin ());
      auto j (args.rbegin ());
      path link (parse_path (move (*j++), wd, fail));
      auto e (j.base ());

      if (i == e)
        fail () << "missing target path";

      // If link is not a directory path (no trailing separator), then create
      // a symlink to the target path at the specified link path (the only
      // target path is allowed in such a case). Otherwise create links to the
      // target paths inside the specified directory.
      //
      if (!link.to_directory ())
      {
        // Don't complete a relative target and pass it to mksymlink() as is.
        //
        path target (parse_path (move (*i++), dir_path (), fail));

        // If there are multiple targets but no trailing separator for the
        // link, then, most likelly, it is missing.
        //
        if (i != e)
          fail () << "multiple target paths with non-directory link path";

        // Synopsis 1: create a target path symlink at the specified path.
        //
        mksymlink (target, link, cbs, fail);
      }
      else
      {
        for (; i != e; ++i)
        {
          // Don't complete a relative target and pass it to mksymlink() as
          // is.
          //
          path target (parse_path (move (*i), dir_path (), fail));

          // Synopsis 2: create a target path symlink in the specified
          // directory.
          //
          mksymlink (target, link / target.leaf (), cbs, fail);
        }
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // Create a directory if not exist and its parent directories if necessary,
  // calling the hook for the created directories. The directory path must be
  // absolute and normalized. Throw system_error on failure.
  //
  static void
  mkdir_p (const dir_path& p,
           const builtin_callbacks& cbs,
           const function<error_record ()>& fail)
  {
    assert (p.absolute () && p.normalized ());

    if (!dir_exists (p))
    {
      if (!p.root ())
        mkdir_p (p.directory (), cbs, fail);

      if (cbs.create)
        call (fail, cbs.create, p, true /* pre */);

      try_mkdir (p); // Returns success or throws.

      if (cbs.create)
        call (fail, cbs.create, p, false /* pre */);
    }
  }

  // mkdir [-p|--parents] <dir>...
  //
  // Note that POSIX doesn't specify if after a directory creation failure the
  // command should proceed with the rest of the arguments. The current
  // implementation exits immediatelly in such a case.
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  mkdir (const strings& args,
         auto_fd in, auto_fd out, auto_fd err,
         const dir_path& cwd,
         const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "mkdir");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);

      mkdir_options ops (
        parse<mkdir_options> (scan, args, cbs.parse_option, fail));

      // Create directories.
      //
      if (!scan.more ())
        fail () << "missing directory";

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      while (scan.more ())
      {
        dir_path p (
          path_cast<dir_path> (parse_path (scan.next (), wd, fail)));

        try
        {
          if (ops.parents ())
            mkdir_p (p, cbs, fail);
          else
          {
            if (cbs.create)
              call (fail, cbs.create, p, true /* pre */);

            if (try_mkdir (p) == mkdir_status::success)
            {
              if (cbs.create)
                call (fail, cbs.create, p, false /* pre */);
            }
            else //           == mkdir_status::already_exists
              throw_generic_error (EEXIST);
          }
        }
        catch (const system_error& e)
        {
          fail () << "unable to create directory '" << p << "': " << e;
        }
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // mv [-f|--force] <src-path>    <dst-path>
  // mv [-f|--force] <src-path>... <dst-dir>/
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  mv (const strings& args,
      auto_fd in, auto_fd out, auto_fd err,
      const dir_path& cwd,
      const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "mv");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);
      mv_options ops (parse<mv_options> (scan, args, cbs.parse_option, fail));

      // Move filesystem entries.
      //
      if (!scan.more ())
        fail () << "missing arguments";

      // Note that the arguments semantics depends on the last argument, so we
      // read out and cache them.
      //
      small_vector<string, 2> args;
      while (scan.more ())
        args.push_back (scan.next ());

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      auto i (args.begin ());
      auto j (args.rbegin ());
      path dst (parse_path (move (*j++), wd, fail));
      auto e (j.base ());

      if (i == e)
        fail () << "missing source path";

      auto mv = [ops, &fail, cbs] (const path& from, const path& to)
      {
        if (cbs.move)
          call (fail, cbs.move, from, to, ops.force (), true /* pre */);

        try
        {
          bool exists (entry_exists (to));

          // Fail if the source and destination paths are the same.
          //
          // Note that for mventry() function (that is based on the POSIX
          // rename() function) this is a noop.
          //
          if (exists && to == from)
            fail () << "unable to move entry '" << from << "' to itself";

          // Rename/move the filesystem entry, replacing an existing one.
          //
          mventry (from, to,
                   cpflags::overwrite_permissions |
                   cpflags::overwrite_content);

          if (cbs.move)
            call (fail, cbs.move, from, to, ops.force (), false /* pre */);
        }
        catch (const system_error& e)
        {
          fail () << "unable to move entry '" << from << "' to '" << to
                  << "': " << e;
        }
      };

      // If destination is not a directory path (no trailing separator) then
      // move the filesystem entry to the specified path (the only source path
      // is allowed in such a case). Otherwise move the source filesystem
      // entries into the destination directory.
      //
      if (!dst.to_directory ())
      {
        path src (parse_path (move (*i++), wd, fail));

        // If there are multiple sources but no trailing separator for the
        // destination, then, most likelly, it is missing.
        //
        if (i != e)
          fail () << "multiple source paths without trailing separator for "
                  << "destination directory";

        // Synopsis 1: move an entry to the specified path.
        //
        mv (src, dst);
      }
      else
      {
        // Synopsis 2: move entities into the specified directory.
        //
        for (; i != e; ++i)
        {
          path src (parse_path (move (*i), wd, fail));
          mv (src, dst / src.leaf ());
        }
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // rm [-r|--recursive] [-f|--force] <path>...
  //
  // The implementation deviates from POSIX in a number of ways. It doesn't
  // interact with a user and fails immediatelly if unable to process an
  // argument. It doesn't check for dots containment in the path, and doesn't
  // consider files and directory permissions in any way just trying to remove
  // a filesystem entry. Always fails if empty path is specified.
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  rm (const strings& args,
      auto_fd in, auto_fd out, auto_fd err,
      const dir_path& cwd,
      const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "rm");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);
      rm_options ops (parse<rm_options> (scan, args, cbs.parse_option, fail));

      // Remove entries.
      //
      if (!scan.more () && !ops.force ())
        fail () << "missing file";

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      while (scan.more ())
      {
        path p (parse_path (scan.next (), wd, fail));

        if (cbs.remove)
          call (fail, cbs.remove, p, ops.force (), true /* pre */);

        try
        {
          dir_path d (path_cast<dir_path> (p));

          pair<bool, entry_stat> es (path_entry (d));
          if (es.first && es.second.type == entry_type::directory)
          {
            if (!ops.recursive ())
              fail () << "'" << p << "' is a directory";

            // The call can result in rmdir_status::not_exist. That's not very
            // likelly but there is also nothing bad about it.
            //
            try_rmdir_r (d);
          }
          else if (try_rmfile (p) == rmfile_status::not_exist &&
                   !ops.force ())
            throw_generic_error (ENOENT);

          if (cbs.remove)
            call (fail, cbs.remove, p, ops.force (), false /* pre */);
        }
        catch (const system_error& e)
        {
          fail () << "unable to remove '" << p << "': " << e;
        }
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // rmdir [-f|--force] <path>...
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  rmdir (const strings& args,
         auto_fd in, auto_fd out, auto_fd err,
         const dir_path& cwd,
         const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "rmdir");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);

      rmdir_options ops (
        parse<rmdir_options> (scan, args, cbs.parse_option, fail));

      // Remove directories.
      //
      if (!scan.more () && !ops.force ())
        fail () << "missing directory";

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      while (scan.more ())
      {
        dir_path p (
          path_cast<dir_path> (parse_path (scan.next (), wd, fail)));

        if (cbs.remove)
          call (fail, cbs.remove, p, ops.force (), true /* pre */);

        try
        {
          rmdir_status s (try_rmdir (p));

          if (s == rmdir_status::not_empty)
            throw_generic_error (ENOTEMPTY);
          else if (s == rmdir_status::not_exist && !ops.force ())
            throw_generic_error (ENOENT);

          if (cbs.remove)
            call (fail, cbs.remove, p, ops.force (), false /* pre */);
        }
        catch (const system_error& e)
        {
          fail () << "unable to remove '" << p << "': " << e;
        }
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // sed [-n|--quiet] [-i|--in-place] (-e|--expression <script>)... [<file>]
  //
  // Note: must be executed asynchronously.
  //
  static uint8_t
  sed (const strings& args,
       auto_fd in, auto_fd out, auto_fd err,
       const dir_path& cwd,
       const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "sed");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      // Automatically remove a temporary file (used for in place editing) on
      // failure.
      //
      auto_rmfile rm;

      if (in == nullfd)
        in = fddup (stdin_fd ());

      if (out == nullfd)
        out = fddup (stdout_fd ());

      // Turn the streams into the binary mode to preserve the original line
      // endings.
      //
      fdmode (in.get (),  fdstream_mode::binary);
      fdmode (out.get (), fdstream_mode::binary);

      // Do not throw when failbit is set (getline() failed to extract any
      // character).
      //
      ifdstream cin (move (in), ifdstream::badbit);

      ofdstream cout (move (out));

      // Parse arguments.
      //
      cli::vector_scanner scan (args);

      sed_options ops (
        parse<sed_options> (scan, args, cbs.parse_option, fail));

      if (ops.expression ().empty ())
        fail () << "missing script";

      struct subst
      {
        std::regex regex;
        string replacement;
        bool global;
        bool print;
      };

      small_vector<subst, 1> substs;

      for (const string& v: ops.expression ())
      {
        if (v.empty ())
          fail () << "empty script";

        if (v[0] != 's')
          fail () << "unknown command in '" << v << "': only 's' command "
                  << "supported";

        // Parse the substitute command.
        //
        if (v.size () < 2)
          fail () << "no delimiter for 's' command in '" << v << "'";

        char delim (v[1]);
        if (delim == '\\' || delim == '\n')
          fail () << "invalid delimiter for 's' command in '" << v << "'";

        // Parse the substitute command regex (as string), replacement, and
        // flags.
        //
        pair<string, string> rf;
        bool icase  (false);
        bool global (false);
        bool print  (false);

        try
        {
          size_t e;
          rf = regex_replace_parse (v.c_str () + 1, v.size () - 1, e);

          char c;
          for (size_t i (e + 1); (c = v[i]) != '\0'; ++i)
          {
            switch (c)
            {
            case 'i': icase  = true; break;
            case 'g': global = true; break;
            case 'p': print  = true; break;
            default:
              {
                fail () << "invalid 's' command flag '" << c << "' in '" << v
                        << "'";
              }
            }
          }
        }
        catch (const invalid_argument& e)
        {
          fail () << "invalid 's' command '" << v << "': " << e;
        }

        // Parse the regex and add the substitution to the list.
        //
        try
        {
          // Note that ECMAScript is implied if no grammar flag is specified.
          //
          regex re (rf.first, icase ? regex::icase : regex::ECMAScript);

          substs.push_back ({move (re),
                             move (rf.second),
                             global,
                             print});
        }
        catch (const regex_error& e)
        {
          // Print regex_error description if meaningful (no space).
          //
          fail () << "invalid regex '" << rf.first << "' in '" << v << "'"
                  << e;
        }
      }

      // Path of a file to edit. An empty path represents stdin.
      //
      path p;
      if (scan.more ())
      {
        string f (scan.next ());

        if (f != "-")
          p = parse_path (move (f),
                          (cwd.absolute ()
                           ? cwd
                           : current_directory (cwd, fail)),
                          fail);
      }

      if (scan.more ())
        fail () << "unexpected argument '" << scan.next () << "'";

      // Edit file.
      //
      // If we edit file in place make sure that the file path is specified
      // and obtain a temporary file path. We will be writing to the temporary
      // file (rather than to stdout) and will move it to the original file
      // path afterwards.
      //
      path tp;
      if (ops.in_place ())
      {
        if (p.empty ())
          fail () << "-i|--in-place option specified while reading from "
                  << "stdin";

        try
        {
          tp = path::temp_path ("build2-sed");

          cout.close ();  // Flush and close.

          cout.open (fdopen (tp,
                             fdopen_mode::out      |
                             fdopen_mode::truncate |
                             fdopen_mode::create   |
                             fdopen_mode::binary,
                             path_permissions (p)));
        }
        catch (const io_error& e)
        {
          fail () << "unable to open '" << tp << "': " << e;
        }
        catch (const system_error& e)
        {
          fail () << "unable to obtain temporary file: " << e;
        }

        rm = auto_rmfile (tp);
      }

      // Edit a file or STDIN.
      //
      try
      {
        // Open a file if specified.
        //
        if (!p.empty ())
        {
          cin.close (); // Flush and close.
          cin.open (p, fdopen_mode::binary);
        }

        // Read until failbit is set (throw on badbit).
        //
        string ps;
        while (getline (cin, ps))
        {
          // Remember the line ending type and, if it is CRLF, strip the
          // trailing '\r'.
          //
          bool crlf (!ps.empty () && ps.back() == '\r');
          if (crlf)
            ps.pop_back();

          bool prn (!ops.quiet ());

          for (const subst& s: substs)
          {
            auto r (regex_replace_search (
                      ps,
                      s.regex,
                      s.replacement,
                      s.global
                      ? regex_constants::format_default
                      : regex_constants::format_first_only));

            // If the regex matches, then override the pattern space with the
            // replacement result and print it and proceed to the next line,
            // if requested.
            //
            if (r.second)
            {
              ps = move (r.first);

              if (s.print)
              {
                prn = true;
                break;
              }
            }
          }

          // Add newline regardless whether the source line is newline-
          // terminated or not (in accordance with POSIX), preserving the
          // original line ending.
          //
          if (prn)
            cout << ps << (crlf ? "\r\n" : "\n");
        }

        cin.close ();
        cout.close ();

        if (ops.in_place ())
        {
          mvfile (tp, p,
                  cpflags::overwrite_content |
                  cpflags::overwrite_permissions);

          rm.cancel ();
        }

        r = 0;
      }
      catch (const io_error& e)
      {
        error_record d (fail ());
        d << "unable to edit ";

        if (p.empty ())
          d << "stdin";
        else
          d << "'" << p << "'";

        d << ": " << e;
      }
    }
    // Can be thrown while creating cin, cout or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const system_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // sleep <seconds>
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  sleep (const strings& args,
         auto_fd in, auto_fd out, auto_fd err,
         const dir_path&,
         const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "sleep");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);
      parse<sleep_options> (scan, args, cbs.parse_option, fail);

      if (!scan.more ())
        fail () << "missing time interval";

      uint64_t n;

      for (;;) // Breakout loop.
      {
        string a (scan.next ());

        // Note: strtoull() allows these.
        //
        if (!a.empty () && a[0] != '-' && a[0] != '+')
        {
          char* e (nullptr);
          errno = 0; // We must clear it according to POSIX.
          n = strtoull (a.c_str (), &e, 10); // Can't throw.

          if (errno != ERANGE && e == a.c_str () + a.size ())
            break;
        }

        fail () << "invalid time interval '" << a << "'";
      }

      if (scan.more ())
        fail () << "unexpected argument '" << scan.next () << "'";

      // Sleep.
      //
      using namespace chrono;

      seconds d (n);

      if (cbs.sleep)
        call (fail, cbs.sleep, d);
      else
      {
        // MinGW GCC 4.9 doesn't implement this_thread so use Win32 Sleep().
        //
#ifndef _WIN32
        this_thread::sleep_for (d);
#else
        Sleep (static_cast<DWORD> (duration_cast<milliseconds> (d).count ()));
#endif
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // test (-f|--file)|(-d|--directory) <path>
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  test (const strings& args,
        auto_fd in, auto_fd out, auto_fd err,
        const dir_path& cwd,
        const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (2);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "test");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);

      test_options ops (
        parse<test_options> (scan, args, cbs.parse_option, fail));

      if (!ops.file () && !ops.directory ())
        fail () << "either -f|--file or -d|--directory must be specified";

      if (ops.file () && ops.directory ())
        fail () << "both -f|--file and -d|--directory specified";

      if (!scan.more ())
        fail () << "missing path";

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      path p (parse_path (scan.next (), wd, fail));

      if (scan.more ())
        fail () << "unexpected argument '" << scan.next () << "'";

      try
      {
        r = (ops.file () ? file_exists (p) : dir_exists (p)) ? 0 : 1;
      }
      catch (const system_error& e)
      {
        fail () << "cannot test '" << p << "': " << e;
      }
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 2;
  }

  // touch [--after <ref-file>] <file>...
  //
  // Note that POSIX doesn't specify the behavior for touching an entry other
  // than file.
  //
  // Also note that POSIX doesn't specify if after a file touch failure the
  // command should proceed with the rest of the arguments. The current
  // implementation exits immediatelly in such a case.
  //
  // Note: can be executed synchronously.
  //
  static uint8_t
  touch (const strings& args,
         auto_fd in, auto_fd out, auto_fd err,
         const dir_path& cwd,
         const builtin_callbacks& cbs) noexcept
  try
  {
    uint8_t r (1);
    ofdstream cerr (err != nullfd ? move (err) : fddup (stderr_fd ()));

    auto error = [&cerr] (bool fail = false)
    {
      return error_record (cerr, fail, "touch");
    };

    auto fail = [&error] () {return error (true /* fail */);};

    try
    {
      in.close ();
      out.close ();

      // Parse arguments.
      //
      cli::vector_scanner scan (args);

      touch_options ops (
        parse<touch_options> (scan, args, cbs.parse_option, fail));

      auto mtime = [&fail] (const path& p) -> timestamp
      {
        try
        {
          timestamp t (file_mtime (p));

          if (t == timestamp_nonexistent)
            throw_generic_error (ENOENT);

          return t;
        }
        catch (const system_error& e)
        {
          fail () << "cannot obtain file '" << p << "' modification time: "
                  << e;
        }
        assert (false); // Can't be here.
        return timestamp ();
      };

      const dir_path& wd (cwd.absolute ()
                          ? cwd
                          : current_directory (cwd, fail));

      optional<timestamp> after;
      if (ops.after_specified ())
        after = mtime (parse_path (ops.after (), wd, fail));

      if (!scan.more ())
        fail () << "missing file";

      // Create files.
      //
      while (scan.more ())
      {
        path p (parse_path (scan.next (), wd, fail));

        try
        {
          if (cbs.create)
            call (fail, cbs.create, p, true /* pre */);

          touch_file (p);

          if (cbs.create)
            call (fail, cbs.create, p, false /* pre */);

          if (after)
          {
            while (mtime (p) <= *after)
              touch_file (p, false /* create */);
          }
        }
        catch (const system_error& e)
        {
          fail () << "cannot create/update '" << p << "': " << e;
        }
      }

      r = 0;
    }
    // Can be thrown while closing in, out or writing to cerr.
    //
    catch (const io_error& e)
    {
      error () << e;
    }
    catch (const failed&)
    {
      // Diagnostics has already been issued.
    }
    catch (const cli::exception& e)
    {
      error () << e;
    }

    cerr.close ();
    return r;
  }
  // In particular, handles io_error exception potentially thrown while
  // creating, writing to, or closing cerr.
  //
  catch (const std::exception&)
  {
    return 1;
  }

  // Run builtin implementation asynchronously.
  //
  static builtin
  async_impl (builtin_impl* fn,
              uint8_t& r,
              const strings& args,
              auto_fd in, auto_fd out, auto_fd err,
              const dir_path& cwd,
              const builtin_callbacks& cbs)
  {
    unique_ptr<builtin::async_state> s (
      new builtin::async_state (
        r,
        [fn,
         &args,
         in = move (in), out = move (out), err = move (err),
         &cwd,
         &cbs] () mutable noexcept -> uint8_t
        {
          return fn (args,
                     move (in), move (out), move (err),
                     cwd,
                     cbs);
        }));

    return builtin (r, move (s));
  }

  template <builtin_impl fn>
  static builtin
  async_impl (uint8_t& r,
              const strings& args,
              auto_fd in, auto_fd out, auto_fd err,
              const dir_path& cwd,
              const builtin_callbacks& cbs)
  {
    return async_impl (
      fn, r, args, move (in), move (out), move (err), cwd, cbs);
  }

  // Run builtin implementation synchronously.
  //
  template <builtin_impl fn>
  static builtin
  sync_impl (uint8_t& r,
             const strings& args,
             auto_fd in, auto_fd out, auto_fd err,
             const dir_path& cwd,
             const builtin_callbacks& cbs)
  {
    r = fn (args, move (in), move (out), move (err), cwd, cbs);
    return builtin (r);
  }

  const builtin_map builtins
  {
    {"cat",   {&async_impl<&cat>,  2}},
    {"cp",    {&sync_impl<&cp>,    2}},
    {"date",  {&async_impl<&date>, 2}},
    {"diff",  {nullptr,            2}},
    {"echo",  {&async_impl<&echo>, 2}},
    {"false", {&false_,            0}},
    {"find",  {&async_impl<&find>, 2}},
    {"ln",    {&sync_impl<&ln>,    2}},
    {"mkdir", {&sync_impl<&mkdir>, 2}},
    {"mv",    {&sync_impl<&mv>,    2}},
    {"rm",    {&sync_impl<&rm>,    1}},
    {"rmdir", {&sync_impl<&rmdir>, 1}},
    {"sed",   {&async_impl<&sed>,  2}},
    {"sleep", {&sync_impl<&sleep>, 1}},
    {"test",  {&sync_impl<&test>,  1}},
    {"touch", {&sync_impl<&touch>, 2}},
    {"true",  {&true_,             0}}
  };

  // builtin
  //
  uint8_t builtin::
  wait ()
  {
    if (state_ != nullptr)
    {
      unique_lock l (state_->mutex);

      if (!state_->finished)
        state_->condv.wait (l, [this] {return state_->finished;});
    }

    return result_;
  }

  template <>
  optional<uint8_t> builtin::
  timed_wait (const chrono::milliseconds& tm)
  {
    if (state_ != nullptr)
    {
      unique_lock l (state_->mutex);

      if (!state_->finished &&
          !state_->condv.wait_for (l, tm, [this] {return state_->finished;}))
        return nullopt;
    }

    return result_;
  }
}
