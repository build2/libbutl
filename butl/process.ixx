// file      : butl/process.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <utility> // move()

namespace butl
{
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
  process (optional<status_type> s)
      : handle (0),
        status (s),
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
        status (p.status),
        out_fd (p.out_fd),
        in_ofd (p.in_ofd),
        in_efd (p.in_efd)
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
      status = std::move (p.status);
      out_fd = p.out_fd;
      in_ofd = p.in_ofd;
      in_efd = p.in_efd;

      p.handle = 0;
    }

    return *this;
  }
}
