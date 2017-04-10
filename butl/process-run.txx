// file      : butl/process-run.txx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>
#include <utility> // move(), forward(), index_sequence

namespace butl
{
  inline int process_stdin  (int v) {assert (v >= 0); return v;}
  inline int process_stdout (int v) {assert (v >= 0); return v;}
  inline int process_stderr (int v) {assert (v >= 0); return v;}

  inline int
  process_stdin (const auto_fd& v) {assert (v.get () >= 0); return v.get ();}

  inline int
  process_stdout (const auto_fd& v) {assert (v.get () >= 0); return v.get ();}

  inline int
  process_stderr (const auto_fd& v) {assert (v.get () >= 0); return v.get ();}

  process
  process_start (const dir_path& cwd,
                 const process_path& pp,
                 const char* cmd[],
                 int in,
                 int out,
                 int err);

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A,
            typename std::size_t... index>
  process
  process_start (std::index_sequence<index...>,
                 const C& cmdc,
                 I&& in,
                 O&& out,
                 E&& err,
                 const dir_path& cwd,
                 const process_path& pp,
                 A&&... args)
  {
    // Map stdin/stdout/stderr arguments to their integer values, as expected
    // by the process constructor.
    //
    int in_i  (process_stdin  (std::forward<I> (in)));
    int out_i (process_stdout (std::forward<O> (out)));
    int err_i (process_stderr (std::forward<E> (err)));

    // Construct the command line array.
    //
    const std::size_t args_size (sizeof... (args));

    std::string storage[args_size];
    const char* cmd[args_size + 2] = {
      pp.recall_string (),
      process_arg_as (args, storage[index])...,
      nullptr};

    // The last argument can be NULL (used to handle zero A... pack).
    //
    cmdc (cmd, args_size + 2);

    // @@ Do we need to make sure certain fd's are closed before calling
    //    wait()? Is this only the case with pipes? Needs thinking.

    return process_start (cwd, pp, cmd, in_i, out_i, err_i);
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  inline process
  process_start (const C& cmdc,
                 I&& in,
                 O&& out,
                 E&& err,
                 const dir_path& cwd,
                 const process_path& pp,
                 A&&... args)
  {
    return process_start (std::index_sequence_for<A...> (),
                          cmdc,
                          std::forward<I> (in),
                          std::forward<O> (out),
                          std::forward<E> (err),
                          cwd,
                          pp,
                          std::forward<A> (args)...);
  }

  template <typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  inline process
  process_start (I&& in,
                 O&& out,
                 E&& err,
                 const dir_path& cwd,
                 const P& p,
                 A&&... args)
  {
    return process_start ([] (const char* [], std::size_t) {},
                          std::forward<I> (in),
                          std::forward<O> (out),
                          std::forward<E> (err),
                          cwd,
                          process::path_search (p, true),
                          std::forward<A> (args)...);
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  inline process
  process_start (const C& cmdc,
                 I&& in,
                 O&& out,
                 E&& err,
                 const dir_path& cwd,
                 const P& p,
                 A&&... args)
  {
    return process_start (cmdc,
                          std::forward<I> (in),
                          std::forward<O> (out),
                          std::forward<E> (err),
                          cwd,
                          process::path_search (p, true),
                          std::forward<A> (args)...);
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  inline process_exit
  process_run (const C& cmdc,
               I&& in,
               O&& out,
               E&& err,
               const dir_path& cwd,
               const process_path& pp,
               A&&... args)
  {
    process pr (
      process_start (cmdc,
                     std::forward<I> (in),
                     std::forward<O> (out),
                     std::forward<E> (err),
                     cwd,
                     pp,
                     std::forward<A> (args)...));

    pr.wait ();
    return *pr.exit;
  }

  template <typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  inline process_exit
  process_run (I&& in,
               O&& out,
               E&& err,
               const dir_path& cwd,
               const P& p,
               A&&... args)
  {
    return process_run ([] (const char* [], std::size_t) {},
                        std::forward<I> (in),
                        std::forward<O> (out),
                        std::forward<E> (err),
                        cwd,
                        process::path_search (p, true),
                        std::forward<A> (args)...);
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  inline process_exit
  process_run (const C& cmdc,
               I&& in,
               O&& out,
               E&& err,
               const dir_path& cwd,
               const P& p,
               A&&... args)
  {
    return process_run (cmdc,
                        std::forward<I> (in),
                        std::forward<O> (out),
                        std::forward<E> (err),
                        cwd,
                        process::path_search (p, true),
                        std::forward<A> (args)...);
  }
}
