// file      : libbutl/process.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#ifndef _WIN32
#  include <sys/types.h> // pid_t
#endif

#include <string>
#include <vector>
#include <chrono>
#include <ostream>
#include <cstddef>      // size_t
#include <cstdint>      // uint32_t
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>     // auto_fd, fdpipe
#include <libbutl/vector-view.hxx>
#include <libbutl/small-vector.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  struct process_error: std::system_error
  {
    const bool child;

    process_error (int e, bool child = false)
        : system_error (e, std::generic_category ()), child (child) {}

#ifdef _WIN32
    process_error (const std::string& d, int fallback_errno_code = 0)
        : system_error (fallback_errno_code, std::system_category (), d),
          child (false) {}
#endif
  };

  struct process_child_error: process_error
  {
    explicit
    process_child_error (int e): process_error (e, true) {}
  };

  // Process arguments (i.e., the command line). The first must be an
  // executable name and the last element should be NULL. Can also be the
  // multi-process piped command line (see process::print() for details).
  //
  struct process_args
  {
    const char* const* argv;
    std::size_t argc;
  };

  // A process executable has three paths: initial, recall, and effective.
  // Initial is the original "command" that you specify in argv[0] and on
  // POSIX that's what ends up in the child's argv[0]. But not on Windows. On
  // Windows the command is first searched for in the parent executable's
  // directory and if found then that's what should end up in child's argv[0].
  // So this is the recall path. It is called recall because this is what the
  // caller of the parent process will be able to execute if you printed the
  // command line (provided you haven't changed the CWD). Finally, effective
  // is the absolute path to the executable that will include the directory
  // part if found in PATH, the .exe extension if one is missing, etc.
  //
  // As an example, let's say we run foo\foo.exe that itself spawns bar which
  // is found as foo\bar.exe. The paths will then be:
  //
  // initial:   bar
  // recall:    foo\bar
  // effective: c:\...\foo\bar.exe
  //
  // In most cases, at least on POSIX, the first two paths will be the same.
  // As an optimization, if the recall path is empty, then it means it is the
  // same as initial. Similarly, if the effective path is empty then, it is
  // the same as recall (and if that is empty, as initial).
  //
  // Note that the call to path_search() below adjust args[0] to point to the
  // recall path which brings up lifetime issues. To address this this class
  // also implements an optional RAII-based auto-restore of args[0] to its
  // initial value.
  //
  class process_path
  {
  public:
    const char* initial = nullptr;
    path recall;
    path effect;

    // Handle empty recall/effect.
    //
    const char* recall_string () const;
    const char* effect_string () const;

    bool
    empty () const
    {
      return (initial == nullptr || *initial == '\0') &&
        recall.empty () && effect.empty ();
    }

    // Clear recall making it the same as effective.
    //
    void
    clear_recall ();

    // Make all three paths the same.
    //
    explicit
    process_path (path effect);

    process_path (const char* initial, path&& recall, path&& effect);
    process_path () = default;

    // Moveable-only type.
    //
    process_path (process_path&&) noexcept;
    process_path& operator= (process_path&&) noexcept;

    process_path (const process_path&) = delete;
    process_path& operator= (const process_path&) = delete;

    ~process_path ();

    // Manual copying. Should not use args[0] RAII. See path_search() for the
    // init semantics.
    //
    process_path (const process_path&, bool init);

  private:
    friend class process;
    const char** args0_ = nullptr;
  };

  // Process exit information.
  //
  struct LIBBUTL_SYMEXPORT process_exit
  {
    // Status type is the raw exit value as returned by GetExitCodeProcess()
    // (NTSTATUS value that represents exit or error codes; MSDN refers to the
    // error code as "value of the exception that caused the termination") or
    // waitpid(1). Code type is the return value if the process exited
    // normally.
    //
#ifndef _WIN32
    using status_type = int;
    using code_type = std::uint8_t;
#else
    using status_type = std::uint32_t; // Win32 DWORD
    using code_type = std::uint16_t;   // Win32 WORD
#endif

    status_type status;

    process_exit () = default;

    explicit
    process_exit (code_type);

    enum as_status_type {as_status};
    process_exit (status_type s, as_status_type): status (s) {}

    // Return false if the process exited abnormally.
    //
    bool
    normal () const;

    // C/C++ don't apply constraints on program exit code other than it being
    // of type int.
    //
    // POSIX specifies that only the least significant 8 bits shall be
    // available from wait() and waitpid(); the full value shall be available
    // from waitid() (read more at _Exit, _exit Open Group spec).
    //
    // While the Linux man page for waitid() doesn't mention any deviations
    // from the standard, the FreeBSD implementation (as of version 11.0) only
    // returns 8 bits like the other wait*() calls.
    //
    // Windows supports 32-bit exit codes.
    //
    // Note that in shells some exit values can have special meaning so using
    // them can be a source of confusion. For bash values in the [126, 255]
    // range are such a special ones (see Appendix E, "Exit Codes With Special
    // Meanings" in the Advanced Bash-Scripting Guide).
    //
    // So [0, 125] appears to be the usable exit code range.
    //
    code_type
    code () const;

    explicit operator bool () const {return normal () && code () == 0;}

    // Abnormal termination information.
    //

    // Return the signal number that caused the termination or 0 if no such
    // information is available.
    //
    int
    signal () const;

    // Return true if the core file was generated.
    //
    bool
    core () const;

    // Return a description of the reason that caused the process to terminate
    // abnormally. On POSIX this is the signal name, on Windows -- the summary
    // produced from the corresponding error identifier defined in ntstatus.h.
    //
    std::string
    description () const;
  };

  // Canonical exit status description:
  //
  // "terminated abnormally: <...> (core dumped)"
  // "exited with code <...>"
  //
  // So you would normally do:
  //
  // cerr << "process " << args[0] << " " << *pr.exit << endl;
  //
  LIBBUTL_SYMEXPORT std::string
  to_string (process_exit);

  inline std::ostream&
  operator<< (std::ostream& os, process_exit pe)
  {
    return os << to_string (pe);
  }

  class LIBBUTL_SYMEXPORT process
  {
  public:
#ifndef _WIN32
    using handle_type = pid_t;
    using id_type = pid_t;
#else
    using handle_type = void*;     // Win32 HANDLE
    using id_type = std::uint32_t; // Win32 DWORD
#endif

    // Start another process using the specified command line. The default
    // values to the in, out and err arguments indicate that the child process
    // should inherit the parent process stdin, stdout, and stderr,
    // respectively. If -1 is passed instead, then the corresponding child
    // process descriptor is connected (via a pipe) to out_fd for stdin,
    // in_ofd for stdout, and in_efd for stderr (see data members below). If
    // -2 is passed, then the corresponding child process descriptor is
    // replaced with the null device descriptor (e.g., /dev/null). This
    // results in the child process not being able to read anything from stdin
    // (gets immediate EOF) and all data written to stdout/stderr being
    // discarded.
    //
    // On Windows parent process pipe descriptors are set to text mode to be
    // consistent with the default (text) mode of standard file descriptors of
    // the child process. When reading in the text mode the sequence of 0xD,
    // 0xA characters is translated into the single OxA character and 0x1A is
    // interpreted as EOF. When writing in the text mode the OxA character is
    // translated into the 0xD, 0xA sequence. Use the fdmode() function to
    // change the mode, if required.
    //
    // Instead of passing -1, -2 or the default value, you can also pass your
    // own descriptors. Note, however, that in this case they are not closed by
    // the parent. So you should do this yourself, if required.  For example,
    // to redirect the child process stdout to stderr, you can do:
    //
    // process pr (..., 0, 2);
    //
    // Note also that the somewhat roundabout setup with -1 as a redirect
    // "instruction" and out_fd/in_ofd/in_efd data members for the result
    // helps to make sure the stream instances are destroyed before the
    // process instance. For example:
    //
    // process pr (..., 0, -1, 2);
    // ifdstream is (move (pr.in_ofd));
    //
    // This is important in case an exception is thrown where we want to make
    // sure all our pipe ends are closed before we wait for the process exit
    // (which happens in the process destructor).
    //
    // And speaking of the destruction order, another thing to keep in mind is
    // that only one stream can use the skip mode (fdstream_mode::skip;
    // because skipping is performed in the blocking mode) and the stream that
    // skips should come first so that all other streams are destroyed/closed
    // before it (failed that, we may end up in a deadlock). For example:
    //
    // process pr (..., -1, -1, -1);
    // ifdstream is (move (pr.in_ofd), fdstream_mode::skip); // Must be first.
    // ifdstream es (move (pr.in_efd));
    // ofdstream os (move (pr.out_fd));
    //
    // The cwd argument allows to change the current working directory of the
    // child process. NULL and empty arguments are ignored.
    //
    // The envvars argument allows to set and unset environment variables in
    // the child process. If not NULL, it must contain strings in the
    // "name=value" (set) or "name" (unset) forms and be terminated with
    // NULL. Note that all other variables are inherited from the parent
    // process.
    //
    // Throw process_error if anything goes wrong. Note that some of the
    // exceptions (e.g., if exec() failed) can be thrown in the child
    // version of us (as process_child_error).
    //
    // Note that the versions without the the process_path argument may
    // temporarily change args[0] (see path_search() for details).
    //
    process (const char**,
             int in = 0, int out = 1, int err = 2,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const char* const*,
             int in = 0, int out = 1, int err = 2,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (std::vector<const char*>&,
             int in = 0, int out = 1, int err = 2,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const std::vector<const char*>&,
             int in = 0, int out = 1, int err = 2,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    // If the descriptors are pipes that you have created, then you should use
    // this constructor instead to communicate this information (the parent
    // end may need to be "probed" on Windows).
    //
    // For generality, if the "other" end of the pipe is -1, then assume this
    // is not a pipe.
    //
    struct pipe
    {
      pipe () = default;
      pipe (int i, int o): in (i), out (o) {}

      explicit
      pipe (const fdpipe& p): in (p.in.get ()), out (p.out.get ()) {}

      // Transfer ownership to one end of the pipe.
      //
      pipe (auto_fd i, int o): in (i.release ()), out (o), own_in (true) {}
      pipe (int i, auto_fd o): in (i), out (o.release ()), own_out (true) {}

      // Moveable-only type.
      //
      pipe (pipe&&) noexcept;
      pipe& operator= (pipe&&) noexcept;

      pipe (const pipe&) = delete;
      pipe& operator= (const pipe&) = delete;

      ~pipe ();

    public:
      int in  = -1;
      int out = -1;

      bool own_in = false;
      bool own_out = false;
    };

    process (const char**,
             pipe in, pipe out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const char**,
             int in, int out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const char* const*,
             pipe in, pipe out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const char* const*,
             int in, int out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (std::vector<const char*>&,
             pipe in, pipe out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (std::vector<const char*>&,
             int in, int out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const std::vector<const char*>&,
             pipe in, pipe out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const std::vector<const char*>&,
             int in, int out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    // The "piping" constructor, for example:
    //
    // process lhs (..., 0, -1); // Redirect stdout to a pipe.
    // process rhs (..., lhs);   // Redirect stdin to lhs's pipe.
    //
    // rhs.wait (); // Wait for last first.
    // lhs.wait ();
    //
    process (const char**,
             process&, int out = 1, int err = 2,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const char* const*,
             process&, int out = 1, int err = 2,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const char**,
             process&, pipe out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const char**,
             process&, int out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const char* const*,
             process&, pipe out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    process (const process_path&, const char* const*,
             process&, int out, pipe err,
             const char* cwd = nullptr,
             const char* const* envvars = nullptr);

    // Wait for the process to terminate. Return true if the process
    // terminated normally and with the zero exit code. Unless ignore_error
    // is true, throw process_error if anything goes wrong. This function can
    // be called multiple times with subsequent calls simply returning the
    // status.
    //
    bool
    wait (bool ignore_errors = false);

    // Return the same result as wait() if the process has already terminated
    // and nullopt otherwise.
    //
    optional<bool>
    try_wait ();

    // Wait for the process to terminate for up to the specified time
    // duration. Return the same result as wait() if the process has
    // terminated in this timeframe and nullopt otherwise.
    //
    template <typename R, typename P>
    optional<bool>
    timed_wait (const std::chrono::duration<R, P>&);

    // Note that the destructor will wait for the process but will ignore
    // any errors and the exit status.
    //
    ~process () { if (handle != 0) wait (true); }

    // Process termination.
    //

    // Send SIGKILL to the process on POSIX and call TerminateProcess() with
    // DBG_TERMINATE_PROCESS exit code on Windows. Noop for an already
    // terminated process.
    //
    // Note that if the process is killed, it terminates as if it has called
    // abort() (functions registered with atexit() are not called, etc).
    //
    // Also note that on Windows calling this function for a terminating
    // process results in the EPERM process_error exception.
    //
    void
    kill ();

    // Send SIGTERM to the process on POSIX and call kill() on Windows (where
    // there is no general way to terminate a console process gracefully).
    // Noop for an already terminated process.
    //
    void
    term ();

    // Moveable-only type.
    //
    process (process&&) noexcept;
    process& operator= (process&&) noexcept (false); // Note: calls wait().

    process (const process&) = delete;
    process& operator= (const process&) = delete;

    // Create an empty or "already terminated" process. By default the
    // termination status is unknown but you can change that.
    //
    explicit
    process (optional<process_exit> = nullopt);

    // Resolve process' paths based on the initial path in args0. If recall
    // differs from initial, adjust args0 to point to the recall path. If
    // resolution fails, throw process_error. Normally, you will use this
    // function like this:
    //
    // const char* args[] = {"foo", ..., nullptr};
    //
    // process_path pp (process::path_search (args[0]))
    //
    // ... // E.g., print args[0].
    //
    // process pr (pp, args);
    //
    // You can also specify the fallback directory which will be tried last.
    // This, for example, can be used to implement the Windows "search in the
    // parent executable's directory" semantics across platforms.
    //
    // If path_only is true then only search in the PATH environment variable
    // (or in CWD if there is a directory component) ignoring other places
    // (like calling process' directory and, gasp, CWD on Windows).
    //
    // If the paths argument is not NULL, search in this list of paths rather
    // than in the PATH environment variable. Note that in this case you may
    // want to clear the recall path (process_path::clear_recall()) since the
    // path won't be "recallable" (unless you've passed a cache of the PATH
    // environment variable or some such).
    //
    static process_path
    path_search (const char*& args0,
                 const dir_path& fallback = dir_path (),
                 bool path_only = false,
                 const char* paths = nullptr);

    // This version is primarily useful when you want to pre-search the
    // executable before creating the args[] array. In this case you will
    // use the recall path for args[0].
    //
    // The init argument determines whether to initialize the initial path to
    // the shallow copy of file. If it is true, then initial is the same as
    // file and recall is either empty or contain a different path. If it is
    // false then initial contains a shallow copy of recall, and recall is
    // either a different path or a deep copy of file. Normally you don't care
    // about initial once you got recall and the main reason to pass true to
    // this argument is to save a copy (since initial and recall are usually
    // the same).
    //
    static process_path
    path_search (const char* file, bool init,
                 const dir_path& = dir_path (),
                 bool = false,
                 const char* = nullptr);

    static process_path
    path_search (const std::string&, bool,
                 const dir_path& = dir_path (),
                 bool = false,
                 const char* = nullptr);

    static process_path
    path_search (const path&, bool,
                 const dir_path& = dir_path (),
                 bool = false,
                 const char* = nullptr);

    // As above but if not found return empty process_path instead of
    // throwing.
    //
    static process_path
    try_path_search (const char*, bool,
                     const dir_path& = dir_path (),
                     bool = false,
                     const char* = nullptr);

    static process_path
    try_path_search (const std::string&, bool,
                     const dir_path& = dir_path (),
                     bool = false,
                     const char* = nullptr);

    static process_path
    try_path_search (const path&, bool,
                     const dir_path& = dir_path (),
                     bool = false,
                     const char* = nullptr);

    // Print process commmand line. If the number of elements is specified,
    // then it will print the piped multi-process command line, if present.
    // In this case, the expected format is as follows:
    //
    // name1 arg arg ... nullptr
    // name2 arg arg ... nullptr
    // ...
    // nameN arg arg ... nullptr nullptr
    //
    static void
    print (std::ostream&, const char* const* args, size_t n = 0);

    // Quote and escape the specified command line argument. If batch is true
    // then also quote the equal (`=`), comma (`,`) and semicolon (`;`)
    // characters which are treated as argument separators in batch file.
    // Return the original string if neither is necessary and a pointer to the
    // provided buffer string containing the escaped version otherwise.
    //
#ifdef _WIN32
    static const char*
    quote_argument (const char*, std::string& buffer, bool batch);
#endif

  public:
    id_type
    id () const;

    static id_type
    current_id ();

  public:
    handle_type handle;

    static handle_type
    current_handle ();

    // Absence means that the exit information is not (yet) known. This can be
    // because you haven't called wait() yet or because wait() failed.
    //
    optional<process_exit> exit;

    // Use the following file descriptors to communicate with the new
    // process's standard streams (if redirected to pipes; see above).
    //
    auto_fd out_fd; // Write to it to send to stdin.
    auto_fd in_ofd; // Read from it to receive from stdout.
    auto_fd in_efd; // Read from it to receive from stderr.
  };

  // Higher-level process running interface that aims to make executing a
  // process for the common cases as simple as calling a functions. Normally
  // it is further simplified by project-specific wrapper functions that
  // handle the process_error exception as well as abnormal and/or non-zero
  // exit status.
  //
  // The I/O/E arguments determine the child's stdin/stdout/stderr. They can
  // be of type int, auto_fd, fd_pipe and process::pipe (and, in the future,
  // perhaps also string, buffer, etc). For example, the following call will
  // make stdin read from /dev/null, stdout redirect to stderr, and inherit
  // the parent's stderr.
  //
  // process_run (fdopen_null (), 2, 2, ...)
  //
  // The P argument is the program path. It can be anything that can be passed
  // to process::path_search() (const char*, std::string, path) or the
  // process_path itself.
  //
  // The A arguments can be anything convertible to const char* via the
  // overloaded process_arg_as() (see below). Out of the box you can use const
  // char* (with NULL values ignored), std::string, path/dir_path, (as well as
  // [small_]vector[_view] of these), numeric types, as well as optional<T> of
  // all the above with absent arguments ignored.
  //
  struct process_env
  {
    const process_path* path;
    const dir_path*     cwd  = nullptr;
    const char* const*  vars = nullptr;

    // Return true if there is an "environment", that is, either the current
    // working directory or environment variables.
    //
    bool
    env () const
    {
      return (cwd != nullptr && !cwd->empty ()) ||
             (vars != nullptr && *vars != nullptr);
    }

    process_env (): path (nullptr) {}

    process_env (const process_path& p,
                 const dir_path& c = dir_path (),
                 const char* const* v = nullptr)
        : path (&p),

          // Note that this is not just an optimization. It is required when
          // the ctor is called with the default arguments (not to keep the
          // temporary object pointer).
          //
          cwd (!c.empty () ? &c : nullptr),

          vars (v) {}

    process_env (const process_path& p, const char* const* v)
        : path (&p), cwd (nullptr), vars (v) {}

    template <typename V>
    process_env (const process_path& p, const dir_path& c, const V& v)
        : process_env (p, v) {cwd = &c;}

    template <typename V>
    process_env (const process_path& p, const V& v)
        : process_env (p) {init_vars (v);}

    process_env (const char* p,
                 const dir_path& c = dir_path (),
                 const char* const* v = nullptr)
        : process_env (path_, c, v) {path_ = process::path_search (p, true);}

    process_env (const std::string& p,
                 const dir_path& c = dir_path (),
                 const char* const* v = nullptr)
        : process_env (p.c_str (), c, v) {}

    process_env (const butl::path& p,
                 const dir_path& c = dir_path (),
                 const char* const* v = nullptr)
        : process_env (p.string (), c, v) {}

    template <typename V>
    process_env (const char* p, const dir_path& c, const V& v)
        : process_env (path_, c, v) {path_ = process::path_search (p, true);}

    template <typename V>
    process_env (const std::string& p, const dir_path& c, const V& v)
        : process_env (p.c_str (), c, v) {}

    template <typename V>
    process_env (const butl::path& p, const dir_path& c, const V& v)
        : process_env (p.string (), c, v) {}

    template <typename V>
    process_env (const char* p, const V& v)
        : process_env (path_, v) {path_ = process::path_search (p, true);}

    template <typename V>
    process_env (const std::string& p, const V& v)
        : process_env (p.c_str (), v) {}

    template <typename V>
    process_env (const butl::path& p, const V& v)
        : process_env (p.string (), v) {}

    // Moveable-only type.
    //
    process_env (process_env&&) noexcept;
    process_env& operator= (process_env&&) noexcept;

    process_env (const process_env&) = delete;
    process_env& operator= (const process_env&) = delete;

  private:
    template <typename V>
    void
    init_vars (const V&);

    template <std::size_t N>
    void
    init_vars (const char* const (&v)[N])
    {
      vars = v;
    }

    process_path path_;
    small_vector<const char*, 3> vars_;
  };

  template <typename I,
            typename O,
            typename E,
            typename... A>
  process_exit
  process_run (I&& in,
               O&& out,
               E&& err,
               const process_env&,
               A&&... args);

  // The version with the command callback that can be used for printing the
  // command line or similar. It should be callable with the following
  // signature:
  //
  // void (const char* const*, std::size_t)
  //
  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  process_exit
  process_run_callback (const C&,
                        I&& in,
                        O&& out,
                        E&& err,
                        const process_env&,
                        A&&... args);

  // Versions that start the process without waiting.
  //
  template <typename I,
            typename O,
            typename E,
            typename... A>
  process
  process_start (I&& in,
                 O&& out,
                 E&& err,
                 const process_env&,
                 A&&... args);

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  process
  process_start_callback (const C&,
                          I&& in,
                          O&& out,
                          E&& err,
                          const process_env&,
                          A&&... args);

  // Call the callback without actually running/starting anything.
  //
  template <typename C,
            typename... A>
  void
  process_print_callback (const C&,
                          const process_env&,
                          A&&... args);

  // Conversion of types to their C string representations. Can be overloaded
  // (including via ADL) for custom types. The default implementation calls
  // to_string() which covers all the numeric values via std::to_string () and
  // also any type that defines to_string() (via ADL).
  //
  template <typename T>
  inline const char*
  process_arg_as (const T& x, std::string& storage)
  {
    return (storage = std::to_string (x)).c_str ();
  }

  inline const char*
  process_arg_as (const std::string& s, std::string&) {return s.c_str ();}

  template <typename K>
  inline const char*
  process_arg_as (const basic_path<char, K>& p, std::string&)
  {
    return p.string ().c_str ();
  }

  // char[N]
  //
  template <typename V>
  inline void
  process_args_as (V& v, const char* s, std::string&)
  {
    if (s != nullptr)
      v.push_back (s);
  }

  template <std::size_t N>
  inline const char*
  process_arg_as (char (&s)[N], std::string&) {return s;}

  template <std::size_t N>
  inline const char*
  process_arg_as (const char (&s)[N], std::string&) {return s;}

  template <typename V, typename T>
  inline void
  process_args_as (V& v, const T& x, std::string& storage)
  {
    v.push_back (process_arg_as (x, storage));
  }

  template <typename V, typename T>
  inline void
  process_args_as (V& v, const optional<T>& x, std::string& storage)
  {
    if (x)
      process_args_as (v, *x, storage);
  }

  // [small_]vector[_view]<>
  //
  template <typename V>
  inline void
  process_args_as (V& v, const std::vector<std::string>& vs, std::string&)
  {
    for (const std::string& s: vs)
      v.push_back (s.c_str ());
  }

  template <typename V, std::size_t N>
  inline void
  process_args_as (V& v, const small_vector<std::string, N>& vs, std::string&)
  {
    for (const std::string& s: vs)
      v.push_back (s.c_str ());
  }

  template <typename V>
  inline void
  process_args_as (V& v, const vector_view<std::string>& vs, std::string&)
  {
    for (const std::string& s: vs)
      v.push_back (s.c_str ());
  }

  template <typename V>
  inline void
  process_args_as (V& v, const std::vector<const char*>& vs, std::string&)
  {
    for (const char* s: vs)
      if (s != nullptr)
        v.push_back (s);
  }

  template <typename V, std::size_t N>
  inline void
  process_args_as (V& v, const small_vector<const char*, N>& vs, std::string&)
  {
    for (const char* s: vs)
      if (s != nullptr)
        v.push_back (s);
  }

  template <typename V>
  inline void
  process_args_as (V& v, const vector_view<const char*>& vs, std::string&)
  {
    for (const char* s: vs)
      if (s != nullptr)
        v.push_back (s);
  }
}

#include <libbutl/process.ixx>
#include <libbutl/process-run.txx>
