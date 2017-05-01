// file      : libbutl/process.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <utility> // move()

namespace butl
{
  // process_path
  //
  inline process_path::
  ~process_path ()
  {
    if (args0_ != nullptr)
      *args0_ = initial;
  }

  inline process_path::
  process_path (const char* i, path&& r, path&& e)
      : initial (i),
        recall (std::move (r)),
        effect (std::move (e)),
        args0_ (nullptr) {}

  inline process_path::
  process_path (process_path&& p)
      : initial (p.initial),
        recall (std::move (p.recall)),
        effect (std::move (p.effect)),
        args0_ (p.args0_)
  {
    p.args0_ = nullptr;
  }

  inline process_path& process_path::
  operator= (process_path&& p)
  {
    if (this != &p)
    {
      if (args0_ != nullptr)
        *args0_ = initial;

      initial = p.initial;
      recall = std::move (p.recall);
      effect = std::move (p.effect);
      args0_ = p.args0_;

      p.args0_ = nullptr;
    }

    return *this;
  }

  inline const char* process_path::
  recall_string () const
  {
    return recall.empty () ? initial : recall.string ().c_str ();
  }

  inline const char* process_path::
  effect_string () const
  {
    return effect.empty () ? recall_string () : effect.string ().c_str ();
  }

  // process_exit
  //
#ifdef _WIN32
  inline int process_exit::
  signal () const
  {
    return 0;
  }

  inline bool process_exit::
  core () const
  {
    return false;
  }
#endif

  // process
  //
#ifndef _WIN32
  inline process::id_type process::
  id () const
  {
    return handle;
  }
#endif

  inline process_path process::
  path_search (const char*& a0, const dir_path& fb)
  {
    process_path r (path_search (a0, true, fb));

    if (!r.recall.empty ())
    {
      r.args0_ = &a0;
      a0 = r.recall.string ().c_str ();
    }

    return r;
  }

  inline process_path process::
  path_search (const std::string& f, bool i, const dir_path& fb)
  {
    return path_search (f.c_str (), i, fb);
  }

  inline process_path process::
  path_search (const path& f, bool i, const dir_path& fb)
  {
    return path_search (f.string ().c_str (), i, fb);
  }

  inline process_path process::
  try_path_search (const std::string& f, bool i, const dir_path& fb)
  {
    return try_path_search (f.c_str (), i, fb);
  }

  inline process_path process::
  try_path_search (const path& f, bool i, const dir_path& fb)
  {
    return try_path_search (f.string ().c_str (), i, fb);
  }

  inline process::
  process (optional<process_exit> e)
      : handle (0),
        exit (std::move (e)),
        out_fd (-1),
        in_ofd (-1),
        in_efd (-1)
  {
  }

  inline process::
  process (const char* args[], int in, int out, int err)
      : process (nullptr, path_search (args[0]), args, in, out, err) {}

  inline process::
  process (const process_path& pp, const char* args[],
           int in, int out, int err)
      : process (nullptr, pp, args, in, out, err) {}

  inline process::
  process (const char* args[], process& in, int out, int err)
      : process (nullptr, path_search (args[0]), args, in, out, err) {}

  inline process::
  process (const process_path& pp, const char* args[],
           process& in, int out, int err)
      : process (nullptr, pp, args, in, out, err) {}

  inline process::
  process (const char* cwd, const char* args[], int in, int out, int err)
      : process (cwd, path_search (args[0]), args, in, out, err) {}

  inline process::
  process (const char* cwd, const char* args[], process& in, int out, int err)
      : process (cwd, path_search (args[0]), args, in, out, err) {}

  inline process::
  process (process&& p)
      : handle (p.handle),
        exit   (std::move (p.exit)),
        out_fd (std::move (p.out_fd)),
        in_ofd (std::move (p.in_ofd)),
        in_efd (std::move (p.in_efd))
  {
    p.handle = 0;
  }

  inline process& process::
  operator= (process&& p)
  {
    if (this != &p)
    {
      if (handle != 0)
        wait ();

      handle = p.handle;
      exit   = std::move (p.exit);
      out_fd = std::move (p.out_fd);
      in_ofd = std::move (p.in_ofd);
      in_efd = std::move (p.in_efd);

      p.handle = 0;
    }

    return *this;
  }

  inline bool process::
  try_wait (bool& s)
  {
    bool r (try_wait ());

    if (r)
      s = exit && exit->normal () && exit->code () == 0;

    return r;
  }
}
