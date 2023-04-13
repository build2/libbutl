// file      : libbutl/process-run.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <utility> // forward(), index_sequence

namespace butl
{
  template <typename V>
  void process_env::
  init_vars (const V& v)
  {
    if (!v.empty ())
    {
      std::string storage;
      process_args_as (vars_, v, storage);
      assert (storage.empty ()); // We don't expect the storage to be used.

      vars_.push_back (nullptr);
      vars = vars_.data ();
    }
  }

  inline process::pipe
  process_stdin  (int v)
  {
    assert (v >= 0);
    return process::pipe (v, -1);
  }

  inline process::pipe
  process_stdout (int v)
  {
    assert (v >= 0);
    return process::pipe (-1, v);
  }

  inline process::pipe
  process_stderr (int v)
  {
    assert (v >= 0);
    return process::pipe (-1, v);
  }

  inline process::pipe
  process_stdin (const auto_fd& v)
  {
    assert (v.get () >= 0);
    return process::pipe (v.get (), -1);
  }

  inline process::pipe
  process_stdout (const auto_fd& v)
  {
    assert (v.get () >= 0);
    return process::pipe (-1, v.get ());
  }

  inline process::pipe
  process_stderr (const auto_fd& v)
  {
    assert (v.get () >= 0);
    return process::pipe (-1, v.get ());
  }

  inline process::pipe
  process_stdin (const fdpipe& v)
  {
    assert (v.in.get () >= 0 && v.out.get () >= 0);
    return process::pipe (v);
  }

  inline process::pipe
  process_stdout (const fdpipe& v)
  {
    assert (v.in.get () >= 0 && v.out.get () >= 0);
    return process::pipe (v);
  }

  inline process::pipe
  process_stderr (const fdpipe& v)
  {
    assert (v.in.get () >= 0 && v.out.get () >= 0);
    return process::pipe (v);
  }

  // Not necessarily a real pipe, so only a single end is constrained to be a
  // valid file descriptor.
  //
  inline process::pipe
  process_stdin (process::pipe v)
  {
    assert (v.in >= 0);
    return v;
  }

  inline process::pipe
  process_stdout (process::pipe v)
  {
    assert (v.out >= 0);
    return v;
  }

  inline process::pipe
  process_stderr (process::pipe v)
  {
    assert (v.out >= 0);
    return v;
  }

  LIBBUTL_SYMEXPORT process
  process_start (const dir_path* cwd,
                 const process_path& pp,
                 const char* cmd[],
                 const char* const* envvars,
                 process::pipe in,
                 process::pipe out,
                 process::pipe err);

  template <typename V, typename T>
  inline const char*
  process_args_as_wrapper (V& v, const T& x, std::string& storage)
  {
    process_args_as (v, x, storage);
    return nullptr;
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A,
            typename std::size_t... index>
  process
  process_start_impl (std::index_sequence<index...>,
                      const C& cmdc,
                      I&& in,
                      O&& out,
                      E&& err,
                      const process_env& env,
                      A&&... args)
  {
    // Map stdin/stdout/stderr arguments to their integer values, as expected
    // by the process constructor.
    //
    process::pipe in_i  (process_stdin  (std::forward<I> (in)));
    process::pipe out_i (process_stdout (std::forward<O> (out)));
    process::pipe err_i (process_stderr (std::forward<E> (err)));

    // Construct the command line array.
    //
    const std::size_t args_size (sizeof... (args));

    small_vector<const char*, args_size + 2> cmd;

    assert (env.path != nullptr);
    cmd.push_back (env.path->recall_string ());

    std::string storage[args_size != 0 ? args_size : 1];

    const char* dummy[] = {
      nullptr, process_args_as_wrapper (cmd, args, storage[index])... };

    cmd.push_back (dummy[0]); // NULL (and get rid of unused warning).

    cmdc (cmd.data (), cmd.size ());

    // @@ Do we need to make sure certain fd's are closed before calling
    //    wait()? Is this only the case with pipes? Needs thinking.

    return process_start (env.cwd,
                          *env.path, cmd.data (),
                          env.vars,
                          std::move (in_i),
                          std::move (out_i),
                          std::move (err_i));
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  inline process
  process_start_callback (const C& cmdc,
                          I&& in,
                          O&& out,
                          E&& err,
                          const process_env& env,
                          A&&... args)
  {
    return process_start_impl (std::index_sequence_for<A...> (),
                               cmdc,
                               std::forward<I> (in),
                               std::forward<O> (out),
                               std::forward<E> (err),
                               env,
                               std::forward<A> (args)...);
  }

  template <typename I,
            typename O,
            typename E,
            typename... A>
  inline process
  process_start (I&& in,
                 O&& out,
                 E&& err,
                 const process_env& env,
                 A&&... args)
  {
    return process_start_callback ([] (const char* [], std::size_t) {},
                                   std::forward<I> (in),
                                   std::forward<O> (out),
                                   std::forward<E> (err),
                                   env,
                                   std::forward<A> (args)...);
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  inline process_exit
  process_run_callback (const C& cmdc,
                        I&& in,
                        O&& out,
                        E&& err,
                        const process_env& env,
                        A&&... args)
  {
    process pr (
      process_start_callback (cmdc,
                              std::forward<I> (in),
                              std::forward<O> (out),
                              std::forward<E> (err),
                              env,
                              std::forward<A> (args)...));

    pr.wait ();
    return *pr.exit;
  }

  template <typename I,
            typename O,
            typename E,
            typename... A>
  inline process_exit
  process_run (I&& in,
               O&& out,
               E&& err,
               const process_env& env,
               A&&... args)
  {
    return process_run_callback ([] (const char* [], std::size_t) {},
                                 std::forward<I> (in),
                                 std::forward<O> (out),
                                 std::forward<E> (err),
                                 env,
                                 std::forward<A> (args)...);
  }

  template <typename C,
            typename... A,
            typename std::size_t... index>
  void
  process_print_impl (std::index_sequence<index...>,
                      const C& cmdc,
                      const process_env& env,
                      A&&... args)
  {
    // Construct the command line array.
    //
    const std::size_t args_size (sizeof... (args));

    small_vector<const char*, args_size + 2> cmd;

    assert (env.path != nullptr);
    cmd.push_back (env.path->recall_string ());

    std::string storage[args_size != 0 ? args_size : 1];

    const char* dummy[] = {
      nullptr, process_args_as_wrapper (cmd, args, storage[index])... };

    cmd.push_back (dummy[0]); // NULL (and get rid of unused warning).

    cmdc (cmd.data (), cmd.size ());
  }

  template <typename C,
            typename... A>
  inline void
  process_print_callback (const C& cmdc,
                          const process_env& env,
                          A&&... args)
  {
    process_print_impl (std::index_sequence_for<A...> (),
                        cmdc,
                        env,
                        std::forward<A> (args)...);
  }
}
