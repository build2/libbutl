// file      : butl/process.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline process::
  process ()
      : id (0),
        status (0), // This is a bit of an assumption.
        out_fd (-1),
        in_ofd (-1),
        in_efd (-1)
  {
  }

  inline process::
  process (char const* const args[], int in, int out, int err)
      : process (nullptr, args, in, out, err) {}

  inline process::
  process (char const* const args[], process& in, int out, int err)
      : process (nullptr, args, in, out, err) {}

  inline process::
  process (process&& p)
      : id (p.id),
        status (p.status),
        out_fd (p.out_fd),
        in_ofd (p.in_ofd),
        in_efd (p.in_efd)
  {
    p.id = 0;
  }

  inline process& process::
  operator= (process&& p)
  {
    if (this != &p)
    {
      if (id != 0)
        wait ();

      id = p.id;
      status = p.status;
      out_fd = p.out_fd;
      in_ofd = p.in_ofd;
      in_efd = p.in_efd;

      p.id = 0;
    }

    return *this;
  }
}
