// file      : libbutl/process.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_PROCESS_HXX
#define LIBBUTL_PROCESS_HXX

#ifndef _WIN32
#  include <sys/types.h> // pid_t
#endif

#include <vector>
#include <iosfwd>
#include <cassert>
#include <cstdint>      // uint32_t
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/export.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>     // auto_fd, fdpipe
#include <libbutl/vector-view.hxx>
#include <libbutl/small-vector.hxx>

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
  // also implements an RAII-based auto-restore of args[0] to its initial
  // value.
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

    bool empty () const
    {
      return initial == nullptr && recall.empty () && effect.empty ();
    }

    // Moveable-only type.
    //
    process_path (process_path&&);
    process_path& operator= (process_path&&);

    process_path (const process_path&) = delete;
    process_path& operator= (const process_path&) = delete;

    process_path () = default;
    process_path (const char* i, path&& r, path&& e);
    ~process_path ();

  private:
    friend class process;
    const char** args0_ = nullptr;
  };

  // Process exit information.
  //
  struct LIBBUTL_EXPORT process_exit
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

  class LIBBUTL_EXPORT process
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
    // process p (..., 0, 2);
    //
    // Throw process_error if anything goes wrong. Note that some of the
    // exceptions (e.g., if exec() failed) can be thrown in the child
    // version of us (as process_child_error).
    //
    // Note that the versions without the the process_path argument may
    // temporarily change args[0] (see path_search() for details).
    //
    process (const char* args[], int in = 0, int out = 1, int err = 2);

    process (const process_path&, const char* args[],
             int in = 0, int out = 1, int err = 2);

    // The "piping" constructor, for example:
    //
    // process lhs (..., 0, -1); // Redirect stdout to a pipe.
    // process rhs (..., lhs);   // Redirect stdin to lhs's pipe.
    //
    // rhs.wait (); // Wait for last first.
    // lhs.wait ();
    //
    process (const char* args[], process& in, int out = 1, int err = 2);

    process (const process_path&, const char* args[],
             process& in, int out = 1, int err = 2);

    // Versions of the above constructors that allow us to change the
    // current working directory of the child process. NULL and empty
    // cwd arguments are ignored.
    //
    process (const char* cwd, const char* [], int = 0, int = 1, int = 2);

    process (const char* cwd,
             const process_path&, const char* [],
             int = 0, int = 1, int = 2);

    process (const char* cwd, const char* [], process&, int = 1, int = 2);

    process (const char* cwd,
             const process_path&, const char* [],
             process&, int = 1, int = 2);

    // Wait for the process to terminate. Return true if the process
    // terminated normally and with the zero exit code. Unless ignore_error
    // is true, throw process_error if anything goes wrong. This function can
    // be called multiple times with subsequent calls simply returning the
    // status.
    //
    bool
    wait (bool ignore_errors = false);

    // Return true if the process has already terminated in which case
    // optionally set the argument to the result of wait().
    //
    bool
    try_wait ();

    bool
    try_wait (bool&);

    // Note that the destructor will wait for the process but will ignore
    // any errors and the exit status.
    //
    ~process () {if (handle != 0) wait (true);}

    // Moveable-only type.
    //
    process (process&&);
    process& operator= (process&&);

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
    // process p (pp, args);
    //
    // You can also specify the fallback directory which will be tried last.
    // This, for example, can be used to implement the Windows "search in the
    // parent executable's directory" semantics across platforms.
    //
    static process_path
    path_search (const char*& args0, const dir_path& fallback = dir_path ());

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
    path_search (const char* file, bool init, const dir_path& = dir_path ());

    static process_path
    path_search (const std::string&, bool, const dir_path& = dir_path ());

    static process_path
    path_search (const path&, bool, const dir_path& = dir_path ());

    // As above but if not found return empty process_path instead of
    // throwing.
    //
    static process_path
    try_path_search (const char*, bool, const dir_path& = dir_path ());

    static process_path
    try_path_search (const std::string&, bool, const dir_path& = dir_path ());

    static process_path
    try_path_search (const path&, bool, const dir_path& = dir_path ());

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
    print (std::ostream&, const char* const args[], size_t n = 0);

  public:
    id_type
    id () const;

    static id_type
    current_id ();

  public:
    handle_type handle;

    // Absence means that the exit information is not (yet) known. This can be
    // because you haven't called wait() yet or because wait() failed.
    //
    optional<process_exit> exit;

    // Use the following file descriptors to communicate with the new process's
    // standard streams.
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
  // be of type int, auto_fd (and, in the future, perhaps also fd_pipe,
  // string, buffer, etc). For example, the following call will make stdin
  // read from /dev/null, stdout redirect to stderr, and inherit the parent's
  // stderr.
  //
  // process_run (..., fdnull (), 2, 2, ...)
  //
  // The P argument is the program path. It can be anything that can be passed
  // to process::path_search() (const char*, std::string, path) or the
  // process_path itself.
  //
  // The A arguments can be anything convertible to const char* via the
  // overloaded process_arg_as() (see below). Out of the box you can use const
  // char*, std::string, path/dir_path, (as well as [small_]vector[_view] of
  // these), and numeric types.
  //
  template <typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  process_exit
  process_run (I&& in,
               O&& out,
               E&& err,
               const dir_path& cwd,
               const P&,
               A&&... args);

  // The version with the command callback that can be used for printing the
  // command line or similar. It should be callable with the following
  // signature:
  //
  // void (const char*[], std::size_t)
  //
  template <typename C,
            typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  process_exit
  process_run (const C&,
               I&& in,
               O&& out,
               E&& err,
               const dir_path& cwd,
               const P&,
               A&&... args);

  // Versions that start the process without waiting.
  //
  template <typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  process
  process_start (I&& in,
                 O&& out,
                 E&& err,
                 const dir_path& cwd,
                 const P&,
                 A&&... args);

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename P,
            typename... A>
  process
  process_start (const C&,
                 I&& in,
                 O&& out,
                 E&& err,
                 const dir_path& cwd,
                 const P&,
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
    using namespace std;
    return (storage = to_string (x)).c_str ();
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
  inline const char*
  process_arg_as (const char* s, std::string&) {return s;}

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
      v.push_back (s);
  }

  template <typename V, std::size_t N>
  inline void
  process_args_as (V& v, const small_vector<const char*, N>& vs, std::string&)
  {
    for (const char* s: vs)
      v.push_back (s);
  }

  template <typename V>
  inline void
  process_args_as (V& v, const vector_view<const char*>& vs, std::string&)
  {
    for (const char* s: vs)
      v.push_back (s);
  }
}

#include <libbutl/process.ixx>

#include <libbutl/process-run.txx>

#endif // LIBBUTL_PROCESS_HXX