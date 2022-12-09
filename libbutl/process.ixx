// file      : libbutl/process.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cassert>
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

  // Note that moving the argument into recall and leaving effective empty
  // complies with the constructor semantics and also makes sure that the
  // move/copy constructors and assignment operators work correctly.
  //
  inline process_path::
  process_path (path e)
      : recall (std::move (e)),
        args0_ (nullptr)
  {
    initial = recall.string ().c_str ();
  }

  inline process_path::
  process_path (const char* i, path&& r, path&& e)
      : initial (i),
        recall (std::move (r)),
        effect (std::move (e)),
        args0_ (nullptr) {}

  inline process_path::
  process_path (process_path&& p) noexcept
      : effect (std::move (p.effect)),
        args0_ (p.args0_)
  {
    bool init (p.initial != p.recall.string ().c_str ());

    recall  = std::move (p.recall);
    initial = init ? p.initial : recall.string ().c_str ();

    p.args0_ = nullptr;
  }

  inline process_path& process_path::
  operator= (process_path&& p) noexcept
  {
    if (this != &p)
    {
      if (args0_ != nullptr)
        *args0_ = initial; // Restore.

      bool init (p.initial != p.recall.string ().c_str ());

      recall  = std::move (p.recall);
      effect  = std::move (p.effect);
      initial = init ? p.initial : recall.string ().c_str ();

      args0_  = p.args0_;
      p.args0_ = nullptr;
    }

    return *this;
  }

  inline process_path::
  process_path (const process_path& p, bool init)
      : recall (p.recall), effect (p.effect)
  {
    assert (p.args0_ == nullptr);

    if (!p.empty ())
    {
      assert (init == (p.initial != p.recall.string ().c_str ()));
      initial = init ? p.initial : recall.string ().c_str ();
    }
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

  inline void process_path::
  clear_recall ()
  {
    if (!effect.empty ())
    {
      bool init (initial != recall.string ().c_str ());

      recall = std::move (effect);
      effect.clear ();

      if (!init)
        initial = recall.string ().c_str ();
    }
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

  // process::pipe
  //
  inline process::pipe::
  pipe (pipe&& p) noexcept
      : in (p.in), out (p.out), own_in (p.own_in), own_out (p.own_out)
  {
    p.in = p.out = -1;
  }

  inline process::pipe& process::pipe::
  operator= (pipe&& p) noexcept
  {
    if (this != &p)
    {
      int d (own_in ? in : own_out ? out : -1);
      if (d != -1)
        fdclose (d);

      in = p.in;
      out = p.out;
      own_in = p.own_in;
      own_out = p.own_out;

      p.in = p.out = -1;
    }
    return *this;
  }

  inline process::pipe::
  ~pipe ()
  {
    int d (own_in ? in : own_out ? out : -1);
    if (d != -1)
      fdclose (d);
  }

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
  path_search (const char*& a0, const dir_path& fb, bool po, const char* ps)
  {
    process_path r (path_search (a0, true, fb, po, ps));

    if (!r.recall.empty ())
    {
      r.args0_ = &a0;
      a0 = r.recall.string ().c_str ();
    }

    return r;
  }

  inline process_path process::
  path_search (const std::string& f, bool i,
               const dir_path& fb, bool po, const char* ps)
  {
    return path_search (f.c_str (), i, fb, po, ps);
  }

  inline process_path process::
  path_search (const path& f, bool i,
               const dir_path& fb, bool po, const char* ps)
  {
    return path_search (f.string ().c_str (), i, fb, po, ps);
  }

  inline process_path process::
  try_path_search (const std::string& f, bool i,
                   const dir_path& fb, bool po, const char* ps)
  {
    return try_path_search (f.c_str (), i, fb, po, ps);
  }

  inline process_path process::
  try_path_search (const path& f, bool i,
                   const dir_path& fb, bool po, const char* ps)
  {
    return try_path_search (f.string ().c_str (), i, fb, po, ps);
  }

  inline process::
  process (optional<process_exit> e)
      : handle (0), exit (std::move (e))
  {
  }

  inline process::
  process (const process_path& pp, const char* const* args,
           int in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args,
                 pipe (in, -1), pipe (-1, out), pipe (-1, err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (const char** args,
           int in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args, in, out, err, cwd, envvars)
  {
  }

  inline process::
  process (const process_path& pp, const std::vector<const char*>& args,
           int in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args.data (),
                 pipe (in, -1), pipe (-1, out), pipe (-1, err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (std::vector<const char*>& args,
           int in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args.data (),
                 in, out, err,
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (const char** args,
           pipe in, pipe out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args,
                 std::move (in), std::move (out), std::move (err),
                 cwd, envvars)
  {
  }

  inline process::
  process (const char** args,
           int in, int out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args,
                 pipe (in, -1), pipe (-1, out), std::move (err),
                 cwd, envvars)
  {
  }

  inline process::
  process (const process_path& pp, const char* const* args,
           int in, int out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args,
                 pipe (in, -1), pipe (-1, out), std::move (err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (std::vector<const char*>& args,
           pipe in, pipe out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args.data (),
                 std::move (in), std::move (out), std::move (err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (std::vector<const char*>& args,
           int in, int out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args.data (),
                 pipe (in, -1), pipe (-1, out), std::move (err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (const process_path& pp, const std::vector<const char*>& args,
           pipe in, pipe out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args.data (),
                 std::move (in), std::move (out), std::move (err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (const process_path& pp, const std::vector<const char*>& args,
           int in, int out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args.data (),
                 pipe (in, -1), pipe (-1, out), std::move (err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (const process_path& pp, const char* const* args,
           process& in, pipe out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args,
                 [&in] ()
                 {
                   assert (in.in_ofd != nullfd); // Should be a pipe.
                   return process::pipe (std::move (in.in_ofd), -1);
                 } (),
                 std::move (out), std::move (err),
                 cwd, envvars)
  {
  }

  inline process::
  process (const process_path& pp, const char* const* args,
           process& in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args, in, pipe (-1, out), pipe (-1, err), cwd, envvars)
  {
  }

  inline process::
  process (const char** args,
           process& in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args, in, out, err, cwd, envvars)
  {
  }

  inline process::
  process (const char** args,
           process& in, pipe out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args,
                 in, std::move (out), std::move (err),
                 cwd, envvars)
  {
  }

  inline process::
  process (const char** args,
           process& in, int out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args,
                 in, pipe (-1, out), std::move (err),
                 cwd, envvars)
  {
  }

  inline process::
  process (const process_path& pp, const char* const* args,
           process& in, int out, pipe err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args, in, pipe (-1, out), std::move (err), cwd, envvars)
  {
  }

  inline process::
  process (process&& p) noexcept
      : handle (p.handle),
        exit   (std::move (p.exit)),
        out_fd (std::move (p.out_fd)),
        in_ofd (std::move (p.in_ofd)),
        in_efd (std::move (p.in_efd))
  {
    p.handle = 0;
  }

  inline process& process::
  operator= (process&& p) noexcept (false)
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

  // Implement timed_wait() function templates in terms of their milliseconds
  // specialization.
  //
  template <>
  LIBBUTL_SYMEXPORT optional<bool> process::
  timed_wait (const std::chrono::milliseconds&);

  template <typename R, typename P>
  inline optional<bool> process::
  timed_wait (const std::chrono::duration<R, P>& d)
  {
    using namespace std::chrono;
    return timed_wait (duration_cast<milliseconds> (d));
  }

  // process_env
  //
  inline process_env::
  process_env (process_env&& e) noexcept
  {
    *this = std::move (e);
  }

  inline process_env& process_env::
  operator= (process_env&& e) noexcept
  {
    if (this != &e)
    {
      cwd = e.cwd;

      bool sp (e.path == &e.path_);
      path_ = std::move (e.path_);
      path = sp ? &path_ : e.path;

      bool sv (e.vars == e.vars_.data ());
      vars_ = std::move (e.vars_);
      vars = sv ? vars_.data () : e.vars;
    }

    return *this;
  }
}
