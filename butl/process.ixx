// file      : butl/process.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <utility> // move()

namespace butl
{
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

  inline process::
  process ()
      : handle (0),
        status (0), // This is a bit of an assumption.
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
      status = p.status;
      out_fd = p.out_fd;
      in_ofd = p.in_ofd;
      in_efd = p.in_efd;

      p.handle = 0;
    }

    return *this;
  }
}
