// file      : libbutl/default-options.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <utility>      // move(), forward(), make_pair()
#include <algorithm>    // reverse()
#include <stdexcept>    // invalid_argument
#include <system_error>

#include <libbutl/git.hxx>
#include <libbutl/filesystem.hxx>

namespace butl
{
  inline bool
  options_dir_exists (const dir_path& d)
  try
  {
    return dir_exists (d);
  }
  catch (std::system_error& e)
  {
    throw std::make_pair (path_cast<path> (d), std::move (e));
  }

  // Search for and parse the options files in the specified directory and its
  // local/ subdirectory, if exists, in the reverse order and append the
  // options to the resulting list. Verify that the number of arguments
  // doesn't exceed the limits and decrement arg_max by arg_max_file after
  // parsing each file. Return false if --no-default-options is encountered.
  //
  // Note that by default we check for the local/ subdirectory even if we
  // don't think it belongs to the remote directory; the user may move things
  // around or it could be a VCS we don't yet recognize and there doesn't seem
  // to be any harm in doing so.
  //
  // Also note that if the directory is remote, then for now the options files
  // in both the directory itself and its local/ subdirectory are considered
  // remote (see load_default_options() for details).
  //
  template <typename O, typename S, typename U, typename F>
  bool
  load_default_options_files (const dir_path& d,
                              const std::string& opt,
                              bool args,
                              bool remote,
                              const small_vector<path, 2>& fs,
                              F&& fn,
                              std::size_t& arg_max,
                              std::size_t  arg_max_file,
                              default_options<O>& def_ops,
                              bool load_sub = true,
                              bool load_dir = true)
  {
    assert (load_sub || load_dir);

    bool r (true);

    auto load = [&opt, args, &fs, &fn, &def_ops, &arg_max, arg_max_file, &r]
                (const dir_path& d, bool rem)
    {
      using namespace std;

      for (const path& f: reverse_iterate (fs))
      {
        path p (d / f);

        try
        {
          if (file_exists (p)) // Follows symlinks.
          {
            if (arg_max < arg_max_file)
              throw invalid_argument ("too many options files");

            size_t start_pos (arg_max - arg_max_file);

            fn (p, rem, false /* overwrite */);

            S s (p.string (), opt, start_pos);

            // @@ Note that the potentially thrown exceptions (unknown option,
            //    unexpected argument, etc) will not contain any location
            //    information. Intercepting exception handling to add the file
            //    attribution feels too hairy for now. Maybe we should support
            //    this in CLI.
            //
            O o;
            small_vector<string, 1> as;

            if (args)
            {
              while (s.more ())
              {
                if (!o.parse (s, U::fail, U::stop))
                  as.push_back (s.next ());
              }
            }
            else
              o.parse (s, U::fail, U::fail);

            if (s.position () > arg_max)
              throw invalid_argument ("too many options in file " +
                                      p.string ());

            // Don't decrement arg_max for the empty option files.
            //
            if (s.position () != start_pos)
              arg_max = start_pos;

            if (o.no_default_options ())
              r = false;

            def_ops.push_back (default_options_entry<O> {move (p),
                                                         move (o),
                                                         move (as),
                                                         rem});
          }
        }
        catch (system_error& e)
        {
          throw make_pair (move (p), move (e));
        }
      }
    };

    dir_path ld (d / dir_path ("local"));

    if (load_sub && options_dir_exists (ld))
      load (ld, remote);

    // Don't load options from .build2/ if --no-default-options is encountered
    // in .build2/local/.
    //
    if (load_dir && r)
      load (d, remote);

    return r;
  }

  template <typename O, typename S, typename U, typename F>
  default_options<O>
  load_default_options (const optional<dir_path>& sys_dir,
                        const optional<dir_path>& home_dir,
                        const optional<dir_path>& extra_dir,
                        const default_options_files& ofs,
                        F&& fn,
                        const std::string& opt,
                        std::size_t arg_max,
                        std::size_t arg_max_file,
                        bool args)
  {
    if (sys_dir)
      assert (sys_dir->absolute () && sys_dir->normalized ());

    if (home_dir)
      assert (home_dir->absolute () && home_dir->normalized ());

    if (extra_dir)
      assert (extra_dir->absolute () && extra_dir->normalized ());

    default_options<O> r;

    // Search for and parse the options files in the start and outer
    // directories until root/home directory and in the extra and system
    // directories, stopping if --no-default-options is encountered and
    // reversing the resulting options entry list in the end.
    //
    // Note that the extra directory is handled either during the start/outer
    // directory traversal, if it turns out to be a subdirectory of any of the
    // traversed directories, or right after.
    //
    bool load       (true);
    bool load_extra (extra_dir && options_dir_exists (*extra_dir));

    if (ofs.start)
    {
      assert (ofs.start->absolute () && ofs.start->normalized ());

      for (dir_path d (*ofs.start);
           !(d.root () || (home_dir && d == *home_dir));
           d = d.directory ())
      {
        bool remote;

        try
        {
          remote = git_repository (d);
        }
        catch (std::system_error& e)
        {
          throw std::make_pair (d / ".git", std::move (e));
        }

        // If the directory is remote, then mark all the previously collected
        // local entries (that belong to its subdirectories but not to the
        // extra directory) as remote too.
        //
        // @@ Note that currently the local/ subdirectory of a remote
        //    directory is considered remote (see above for details). When
        //    changing that, skip entries from directories with the `local`
        //    name.
        //
        if (remote)
        {
          // We could optimize this, iterating in the reverse order until the
          // fist remote entry. However, let's preserve the function calls
          // order for entries being overwritten.
          //
          for (default_options_entry<O>& e: r)
          {
            if (!e.remote &&
                (!extra_dir || e.file.directory () != *extra_dir))
            {
              e.remote = true;

              fn (e.file, true /* remote */, true /* overwrite */);
            }
          }
        }

        // If --no-default-options is encountered, then stop the files search
        // but continue the directory traversal until the remote directory is
        // encountered and, if that's the case, mark the already collected
        // local entries as remote.
        //
        if (load)
        {
          dir_path od (d / dir_path (".build2"));

          // If the extra directory is a subdirectory of the current
          // directory, then load it first, but don't load .build2/ or
          // .build2/local/ if it matches any of them.
          //
          bool load_build2       (true);
          bool load_build2_local (true);

          if (load_extra && extra_dir->sub (d))
          {
            load = load_default_options_files<O, S, U> (*extra_dir,
                                                        opt,
                                                        args,
                                                        false /* remote */,
                                                        ofs.files,
                                                        std::forward<F> (fn),
                                                        arg_max,
                                                        arg_max_file,
                                                        r);

            load_extra        = false;
            load_build2       = *extra_dir != od;
            load_build2_local = *extra_dir != od / dir_path ("local");
          }

          if (load && options_dir_exists (od))
            load = load_default_options_files<O, S, U> (od,
                                                        opt,
                                                        args,
                                                        remote,
                                                        ofs.files,
                                                        std::forward<F> (fn),
                                                        arg_max,
                                                        arg_max_file,
                                                        r,
                                                        load_build2_local,
                                                        load_build2);
        }

        if (!load && remote)
          break;
      }
    }

    if (load && load_extra)
      load = load_default_options_files<O, S, U> (*extra_dir,
                                                  opt,
                                                  args,
                                                  false /* remote */,
                                                  ofs.files,
                                                  std::forward<F> (fn),
                                                  arg_max,
                                                  arg_max_file,
                                                  r);

    if (load && home_dir)
    {
      dir_path d (*home_dir / dir_path (".build2"));

      if (options_dir_exists (d))
        load = load_default_options_files<O, S, U> (d,
                                                    opt,
                                                    args,
                                                    false /* remote */,
                                                    ofs.files,
                                                    std::forward<F> (fn),
                                                    arg_max,
                                                    arg_max_file,
                                                    r);
    }

    if (load && sys_dir && options_dir_exists (*sys_dir))
      load_default_options_files<O, S, U> (*sys_dir,
                                           opt,
                                           args,
                                           false /* remote */,
                                           ofs.files,
                                           std::forward<F> (fn),
                                           arg_max,
                                           arg_max_file,
                                           r);

    std::reverse (r.begin (), r.end ());

    return r;
  }

  template <typename O, typename F>
  O
  merge_default_options (const default_options<O>& def_ops,
                         const O& cmd_ops,
                         F&& fn)
  {
    // Optimize for the common case.
    //
    if (def_ops.empty ())
      return cmd_ops;

    O r;
    for (const default_options_entry<O>& e: def_ops)
    {
      fn (e, cmd_ops);
      r.merge (e.options);
    }

    r.merge (cmd_ops);
    return r;
  }

  template <typename O, typename  AS, typename F>
  AS
  merge_default_arguments (const default_options<O>& def_ops,
                           const AS& cmd_args,
                           F&& fn)
  {
    AS r;
    for (const default_options_entry<O>& e: def_ops)
    {
      fn (e, cmd_args);
      r.insert (r.end (), e.arguments.begin (), e.arguments.end ());
    }

    // Optimize for the common case.
    //
    if (r.empty ())
      return cmd_args;

    r.insert (r.end (), cmd_args.begin (), cmd_args.end ());
    return r;
  }

  template <typename I, typename F>
  optional<dir_path>
  default_options_start (const optional<dir_path>& home, I b, I e, F&& f)
  {
    if (home)
      assert (home->absolute () && home->normalized ());

    if (b == e)
      return nullopt;

    // Use the first directory as a start.
    //
    I i (b);
    dir_path d (f (i));

    // Try to find a common prefix for each subsequent directory.
    //
    for (++i; i != e; ++i)
    {
      bool p (false);

      for (;
           !(d.root () || (home && d == *home));
           d = d.directory ())
      {
        if (f (i).sub (d))
        {
          p = true;
          break;
        }
      }

      if (!p)
        return nullopt;
    }

    return d;
  }
}
