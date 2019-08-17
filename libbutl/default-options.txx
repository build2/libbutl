// file      : libbutl/default-options.txx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
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

  // Search for and parse the options files in the specified directory and
  // its local/ subdirectory, if exists, in the reverse order and append the
  // options to the resulting list. Return false if --no-default-options is
  // encountered.
  //
  // Note that we check for the local/ subdirectory even if we don't think it
  // belongs to the remote directory; the user may move things around or it
  // could be a VCS we don't yet recognize and there doesn't seem to be any
  // harm in doing so.
  //
  // Also note that if the directory is remote, then for now the options files
  // in both the directory itself and its local/ subdirectory are considered
  // remote (see load_default_options() for details).
  //
  template <typename O, typename S, typename U, typename F>
  bool
  load_default_options_files (const dir_path& d,
                              bool remote,
                              const small_vector<path, 2>& fs,
                              F&& fn,
                              default_options<O>& def_ops)
  {
    bool r (true);

    auto load = [&fs, &fn, &def_ops, &r] (const dir_path& d, bool remote)
    {
      using namespace std;

      for (const path& f: reverse_iterate (fs))
      {
        path p (d / f);

        try
        {
          if (file_exists (p)) // Follows symlinks.
          {
            fn (p, remote, false /* overwrite */);

            S s (p.string ());

            // @@ Note that the potentially thrown exceptions (unknown option,
            //    unexpected argument, etc) will not contain any location
            //    information. Intercepting exception handling to add the file
            //    attribution feels too hairy for now. Maybe we should support
            //    this in CLI.
            //
            O o;
            o.parse (s, U::fail, U::fail);

            if (o.no_default_options ())
              r = false;

            def_ops.push_back (default_options_entry<O> {move (p),
                                                         move (o),
                                                         remote});
          }
        }
        catch (std::system_error& e)
        {
          throw std::make_pair (move (p), std::move (e));
        }
      }
    };

    dir_path ld (d / dir_path ("local"));

    if (options_dir_exists (ld))
      load (ld, remote);

    // Don't load options from .build2/ if --no-default-options is encountered
    // in .build2/local/.
    //
    if (r)
      load (d, remote);

    return r;
  }

  template <typename O, typename S, typename U, typename F>
  default_options<O>
  load_default_options (const optional<dir_path>& sys_dir,
                        const optional<dir_path>& home_dir,
                        const default_options_files& ofs,
                        F&& fn)
  {
    default_options<O> r;

    // Search for and parse the options files in the specified and outer
    // directories until root/home directory and in the system directory,
    // stopping if --no-default-options is encountered and reversing the
    // resulting options entry list in the end.
    //
    bool load (true);

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
        // local entries (that belong to its subdirectories) as remote too.
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
            if (!e.remote)
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

          if (options_dir_exists (od))
            load = load_default_options_files<O, S, U> (od,
                                                        remote,
                                                        ofs.files,
                                                        std::forward<F> (fn),
                                                        r);
        }

        if (!load && remote)
          break;
      }
    }

    if (home_dir)
    {
      assert (home_dir->absolute () && home_dir->normalized ());

      if (load)
      {
        dir_path d (*home_dir / dir_path (".build2"));

        if (options_dir_exists (d))
          load = load_default_options_files<O, S, U> (d,
                                                      false /* remote */,
                                                      ofs.files,
                                                      std::forward<F> (fn),
                                                      r);
      }
    }

    if (sys_dir)
    {
      assert (sys_dir->absolute () && sys_dir->normalized ());

      if (load && options_dir_exists (*sys_dir))
        load_default_options_files<O, S, U> (*sys_dir,
                                             false /* remote */,
                                             ofs.files,
                                             std::forward<F> (fn),
                                             r);
    }

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
}
