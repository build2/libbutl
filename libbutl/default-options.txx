// file      : libbutl/default-options.txx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
{
  // Search for and parse the options files in the specified directory and
  // its local/ subdirectory, if exists, and append the options to the
  // resulting list.
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
  void
  load_default_options_files (const dir_path& d,
                              bool remote,
                              const small_vector<path, 2>& fs,
                              F&& fn,
                              default_options<O>& r)
  {
    auto load = [&fs, &fn, &r] (const dir_path& d, bool remote)
    {
      using namespace std;

      for (const path& f: fs)
      {
        path p (d / f);

        if (file_exists (p)) // Follows symlinks.
        {
          fn (p, remote);

          S s (p.string ());

          // @@ Note that the potentially thrown exceptions (unknown option,
          //    unexpected argument, etc) will not contain any location
          //    information. Intercepting exception handling to add the file
          //    attribution feels too hairy for now. Maybe we should support
          //    this in CLI.
          //
          O o;
          o.parse (s, U::fail, U::fail);

          r.push_back (default_options_entry<O> {move (p), move (o), remote});
        }
      }
    };

    load (d, remote);

    dir_path ld (d / dir_path ("local"));

    if (dir_exists (ld))
      load (ld, remote);
  }

  // Search for and parse the options files in the specified and outer
  // directories until root/home directory (excluding) and append the options
  // to the resulting list. Return true if the directory is "remote" (i.e.,
  // belongs to a VCS repository).
  //
  template <typename O, typename S, typename U, typename F>
  bool
  load_default_options_files (const dir_path& start_dir,
                              const optional<dir_path>& home_dir,
                              const small_vector<path, 2>& fs,
                              F&& fn,
                              default_options<O>& r)
  {
    if (start_dir.root () || (home_dir && start_dir == *home_dir))
      return false;

    bool remote (load_default_options_files<O, S, U> (start_dir.directory (),
                                                     home_dir,
                                                     fs,
                                                     std::forward<F> (fn),
                                                     r) ||
                 git_repository (start_dir));

    dir_path d (start_dir / dir_path (".build2"));

    if (dir_exists (d))
      load_default_options_files<O, S, U> (d,
                                           remote,
                                           fs,
                                           std::forward<F> (fn),
                                           r);

    return remote;
  }

  template <typename O, typename S, typename U, typename F>
  default_options<O>
  load_default_options (const optional<dir_path>& sys_dir,
                        const optional<dir_path>& home_dir,
                        const default_options_files& ofs,
                        F&& fn)
  {
    default_options<O> r;

    if (sys_dir)
    {
      assert (sys_dir->absolute () && sys_dir->normalized ());

      if (dir_exists (*sys_dir))
        load_default_options_files<O, S, U> (*sys_dir,
                                             false /* remote */,
                                             ofs.files,
                                             std::forward<F> (fn),
                                             r);
    }

    if (home_dir)
    {
      assert (home_dir->absolute () && home_dir->normalized ());

      dir_path d (*home_dir / dir_path (".build2"));

      if (dir_exists (d))
        load_default_options_files<O, S, U> (d,
                                             false /* remote */,
                                             ofs.files,
                                             std::forward<F> (fn),
                                             r);
    }

    if (ofs.start_dir)
    {
      assert (ofs.start_dir->absolute () && ofs.start_dir->normalized ());

      load_default_options_files<O, S, U> (*ofs.start_dir,
                                           home_dir,
                                           ofs.files,
                                           std::forward<F> (fn),
                                           r);
    }

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
