// file      : libbutl/diagnostics.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_DIAGNOSTICS_HXX
#define LIBBUTL_DIAGNOSTICS_HXX

#include <cassert>
#include <ostream>
#include <sstream>
#include <utility>                  // move(), forward()
#include <libbutl/ft/exception.hxx> // uncaught_exceptions
#include <exception>                // uncaught_exception(s)()

#include <libbutl/export.hxx>
#include <libbutl/utility.hxx>

namespace butl
{
  // Diagnostic facility base infrastructure.
  //

  // Diagnostics destination stream (std::cerr by default). Note that its
  // modification is not MT-safe. Also note that concurrent writing to the
  // stream from multiple threads can result in interleaved characters. To
  // prevent this an object of diag_lock type (see below) must be created prior
  // to write operation.
  //
  LIBBUTL_SYMEXPORT extern std::ostream* diag_stream;

  // Acquire the diagnostics exclusive access mutex in ctor, release in dtor.
  // An object of the type must be created prior to writing to diag_stream (see
  // above).
  //
  struct LIBBUTL_SYMEXPORT diag_lock
  {
    diag_lock ();
    ~diag_lock ();
  };

  struct diag_record;
  template <typename> struct diag_prologue;
  template <typename> struct diag_mark;

  using diag_epilogue = void (const diag_record&);

  struct LIBBUTL_SYMEXPORT diag_record
  {
    template <typename T>
    friend const diag_record&
    operator<< (const diag_record& r, const T& x)
    {
      r.os << x;
      return r;
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
    flush () const;

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
        os (std::move (r.os))
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
}

#endif // LIBBUTL_DIAGNOSTICS_HXX
