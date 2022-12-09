// file      : libbutl/diagnostics.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <cassert>
#include <ostream>
#include <sstream>
#include <utility>   // move(), forward()
#include <exception> // uncaught_exception[s]()

#include <libbutl/ft/exception.hxx> // uncaught_exceptions

#include <libbutl/export.hxx>

namespace butl
{
  // Diagnostic facility base infrastructure.
  //

  // Diagnostics destination stream (std::cerr by default). Note that its
  // modification is not MT-safe. Also note that concurrent writing to the
  // stream from multiple threads can result in interleaved characters. To
  // prevent this an object of diag_stream_lock type (see below) must be
  // created prior to write operation.
  //
  LIBBUTL_SYMEXPORT extern std::ostream* diag_stream;

  // Acquire the diagnostics exclusive access mutex in ctor, release in dtor.
  // An object of the type must be created prior to writing to diag_stream
  // (see above).
  //
  // Note that this class also manages the interaction with the progress
  // printing (see below).
  //
  struct LIBBUTL_SYMEXPORT diag_stream_lock
  {
    diag_stream_lock ();
    ~diag_stream_lock ();

    // Support for one-liners, for example:
    //
    // diag_stream_lock () << "Hello, World!" << endl;
    //
    template <typename T>
    std::ostream&
    operator<< (const T& x) const
    {
      return *diag_stream << x;
    }
  };

  // Progress line facility.
  //
  // The idea is to keep a progress line at the bottom of the terminal with
  // other output scrolling above it. For a non-terminal STDERR the progress
  // line is printed as a regular one terminated with the newline character.
  // The printing of the progress line is integrated into diag_stream_lock and
  // diag_progress_lock. To print or update the progress acquire
  // diag_progress_lock and update the diag_progress string. To remove the
  // progress line, set this string to empty. For better readability start the
  // progress line with a space (which is where the cursor will be parked).
  // Should only be used if diag_stream points to std::cerr.
  //
  // Note that child processes writing to the same stream may not completely
  // overwrite the progress line so in this case it makes sense to keep the
  // line as short as possible.
  //
  // To restore the progress line being overwritten by an independent writer
  // (such as a child process), create and destroy the diag_progress_lock.
  //
  LIBBUTL_SYMEXPORT extern std::string diag_progress;

  struct LIBBUTL_SYMEXPORT diag_progress_lock
  {
    diag_progress_lock ();
    ~diag_progress_lock ();
  };

  // Diagnostic record and marks (error, warn, etc).
  //
  // There are two ways to use this facility in a project: simple, where we
  // just alias the types in our namespace, and complex, where instead we
  // derive from them and "override" (hide, really) operator<< (and a few
  // other functions) in order to make ADL look in our namespace rather than
  // butl. In the simple case we may have to resort to defining some
  // operator<< overloads in namespace std in order to satisfy ADL. This is
  // usually not an acceptable approach for libraries, which is where the
  // complex case comes in (see libbuild2 for a "canonical" example of the
  // complex case). Note also that it doesn't seem worth templatazing epilogue
  // so the complex case may also need to do a few casts but those should be
  // limited to the diagnostics infrastructure.
  //
  struct diag_record;
  template <typename> struct diag_prologue;
  template <typename> struct diag_mark;

  using diag_writer = void (const diag_record&);
  using diag_epilogue = void (const diag_record&, diag_writer*);

  struct LIBBUTL_SYMEXPORT diag_record
  {
    template <typename T>
    const diag_record&
    operator<< (const T& x) const
    {
      os << x;
      return *this;
    }

    diag_record ()
        :
#ifdef __cpp_lib_uncaught_exceptions
        uncaught_ (std::uncaught_exceptions ()),
#endif
        empty_ (true),
        epilogue_ (nullptr) {}

    template <typename B>
    explicit
    diag_record (const diag_prologue<B>& p): diag_record () { *this << p;}

    template <typename B>
    explicit
    diag_record (const diag_mark<B>& m): diag_record () { *this << m;}

    ~diag_record () noexcept (false);

    bool
    empty () const {return empty_;}

    bool
    full () const {return !empty_;}

    void
    flush (diag_writer* = nullptr) const;

    void
    append (const char* indent, diag_epilogue* e) const
    {
      // Ignore subsequent epilogues (e.g., from nested marks, etc).
      //
      if (empty_)
      {
        epilogue_ = e;
        empty_ = false;
      }
      else if (indent != nullptr)
        os << indent;
    }

    // Move constructible-only type.
    //
    // Older versions of libstdc++ don't have the ostringstream move support
    // and accuratly detecting its version is non-trivial. So we always use
    // the pessimized implementation with libstdc++. Luckily, GCC doesn't seem
    // to be needing move due to copy/move elision.
    //
#ifdef __GLIBCXX__
    diag_record (diag_record&&);
#else
    diag_record (diag_record&& r)
        :
#ifdef __cpp_lib_uncaught_exceptions
        uncaught_ (r.uncaught_),
#endif
        empty_ (r.empty_),
        epilogue_ (r.epilogue_),
        os (std::move (r.os)) // Note: can throw.
    {
      if (!empty_)
      {
        r.empty_ = true;
        r.epilogue_ = nullptr;
      }
    }
#endif

    diag_record& operator= (diag_record&&) = delete;

    diag_record (const diag_record&) = delete;
    diag_record& operator= (const diag_record&) = delete;

    // Diagnostics writer. The default implementation writes the record text
    // to diag_stream. If it is NULL, then the record text is ignored.
    //
    static diag_writer* writer;

  protected:
#ifdef __cpp_lib_uncaught_exceptions
    const int uncaught_;
#endif
    mutable bool empty_;
    mutable diag_epilogue* epilogue_;

  public:
    mutable std::ostringstream os;
  };

  template <typename B>
  struct diag_prologue: B
  {
    const char* indent;
    diag_epilogue* epilogue;

    diag_prologue (const char* i = "\n  ", diag_epilogue* e = nullptr)
        : B (), indent (i), epilogue (e) {}

    template <typename... A>
    diag_prologue (A&&... a)
        : B (std::forward<A> (a)...), indent ("\n  "), epilogue (nullptr) {}

    template <typename... A>
    diag_prologue (diag_epilogue* e, A&&... a)
        : B (std::forward<A> (a)...), indent ("\n  "), epilogue (e) {}

    template <typename... A>
    diag_prologue (const char* i, diag_epilogue* e, A&&... a)
        : B (std::forward<A> (a)...), indent (i), epilogue (e) {}

    template <typename T>
    diag_record
    operator<< (const T& x) const
    {
      diag_record r;
      r.append (indent, epilogue);
      B::operator() (r);
      r << x;
      return r;
    }

    friend const diag_record&
    operator<< (const diag_record& r, const diag_prologue& p)
    {
      r.append (p.indent, p.epilogue);
      p (r);
      return r;
    }
  };

  template <typename B>
  struct diag_mark: B
  {
    diag_mark (): B () {}

    template <typename... A>
    diag_mark (A&&... a): B (std::forward<A> (a)...) {}

    template <typename T>
    diag_record
    operator<< (const T& x) const
    {
      return B::operator() () << x;
    }

    friend const diag_record&
    operator<< (const diag_record& r, const diag_mark& m)
    {
      return r << m ();
    }
  };

  template <typename B>
  struct diag_noreturn_end: B
  {
    diag_noreturn_end (): B () {}

    template <typename... A>
    diag_noreturn_end (A&&... a): B (std::forward<A> (a)...) {}

    [[noreturn]] friend void
    operator<< (const diag_record& r, const diag_noreturn_end& e)
    {
      // We said that we never return which means this end mark cannot be used
      // to "maybe not return". And not returning without any diagnostics is
      // probably a mistake.
      //
      assert (r.full ());
      e.B::operator() (r);
    }
  };

  // Diagnostics stack. Each frame is "applied" to the diag record.
  //
  // Unfortunately most of our use-cases don't fit into the 2-pointer small
  // object optimization of std::function. So we have to complicate things
  // a bit here.
  //
  struct LIBBUTL_SYMEXPORT diag_frame
  {
    explicit
    diag_frame (void (*f) (const diag_frame&, const diag_record&))
        : func_ (f)
    {
      if (func_ != nullptr)
        prev_ = stack (this);
    }

    diag_frame (diag_frame&& x)
        : func_ (x.func_)
    {
      if (func_ != nullptr)
      {
        prev_ = x.prev_;
        stack (this);

        x.func_ = nullptr;
      }
    }

    diag_frame& operator= (diag_frame&&) = delete;

    diag_frame (const diag_frame&) = delete;
    diag_frame& operator= (const diag_frame&) = delete;

    ~diag_frame ()
    {
      if (func_ != nullptr )
        stack (prev_);
    }

    // Normally passed as an epilogue. Writer is not used.
    //
    static void
    apply (const diag_record& r, diag_writer* = nullptr)
    {
      for (const diag_frame* f (stack ()); f != nullptr; f = f->prev_)
        f->func_ (*f, r);
    }

    // Tip of the stack.
    //
    static const diag_frame*
    stack () noexcept;

    // Set the new and return the previous tip of the stack.
    //
    static const diag_frame*
    stack (const diag_frame*) noexcept;

    struct stack_guard
    {
      explicit stack_guard (const diag_frame* s): s_ (stack (s)) {}
      ~stack_guard () {stack (s_);}
      const diag_frame* s_;
    };

  private:
    void (*func_) (const diag_frame&, const diag_record&);
    const diag_frame* prev_;
  };

  template <typename F>
  struct diag_frame_impl: diag_frame
  {
    explicit
    diag_frame_impl (F f): diag_frame (&thunk), func_ (move (f)) {}

  private:
    static void
    thunk (const diag_frame& f, const diag_record& r)
    {
      static_cast<const diag_frame_impl&> (f).func_ (r);
    }

    const F func_;
  };

  template <typename F>
  inline diag_frame_impl<F>
  make_diag_frame (F f)
  {
    return diag_frame_impl<F> (move (f));
  }
}
