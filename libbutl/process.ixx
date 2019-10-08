// file      : libbutl/process.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

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
      : effect (std::move (p.effect)),
        args0_ (p.args0_)
  {
    bool init (p.initial != p.recall.string ().c_str ());

    recall  = std::move (p.recall);
    initial = init ? p.initial : recall.string ().c_str ();

    p.args0_ = nullptr;
  }

  inline process_path& process_path::
  operator= (process_path&& p)
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
      recall = std::move (effect);
      effect.clear ();
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
      : handle (0),
        exit (std::move (e)),
        out_fd (-1),
        in_ofd (-1),
        in_efd (-1)
  {
  }

  inline process::
  process (const process_path& pp, const char* args[],
           int in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (pp,
                 args,
                 pipe (in, -1), pipe (-1, out), pipe (-1, err),
                 cwd,
                 envvars)
  {
  }

  inline process::
  process (const char* args[],
           int in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args, in, out, err, cwd, envvars) {}

  inline process::
  process (const process_path& pp, const char* args[],
           process& in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (pp, args, in.in_ofd.get (), out, err, cwd, envvars)
  {
    assert (in.in_ofd.get () != -1); // Should be a pipe.
    in.in_ofd.reset (); // Close it on our side.
  }

  inline process::
  process (const char* args[],
           process& in, int out, int err,
           const char* cwd,
           const char* const* envvars)
      : process (path_search (args[0]), args, in, out, err, cwd, envvars) {}

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

  // Implement timed_wait() function templates in terms of their milliseconds
  // specialization.
  //
  template <>
  optional<bool> process::
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
  process_env (process_env&& e)
  {
    *this = std::move (e);
  }

  inline process_env& process_env::
  operator= (process_env&& e)
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
