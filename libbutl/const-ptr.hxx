// file      : libbutl/const-ptr.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <cstddef> // nullptr_t

namespace butl
{
  // Const-propagating pointer.
  //
  // It has the semantics of a raw pointer except that it passes on its own
  // const-ness to the pointed-to object. In other words, if you have a const
  // instance of this pointer, then you can only obtain a const raw pointer to
  // the underlying object. It is normally used as a data member, for example:
  //
  // struct tree
  // {
  //   const_ptr<tree> left;
  //   const_ptr<tree> right;
  //
  //   void modify ();
  // };
  //
  //       tree* x = ...;
  // const tree* y = ...;
  //
  // x.left->modify (); // Ok.
  // y.left->modify (); // Error.
  //
  // Note that due to this semantics, copy construction/assignment requires
  // a non-const instance of const_ptr.
  //
  // Note that this type is standard layout (which means we can reinterpret
  // it as a raw pointer).
  //
  // Known drawbacks/issues:
  //
  // 1. Cannot do static_cast<mytree*> (x.left).
  //
  template <typename T>
  class const_ptr
  {
  public:
    const_ptr () = default;
    explicit const_ptr (T* p): p_ (p) {}
    const_ptr (std::nullptr_t): p_ (nullptr) {}

    const_ptr& operator= (T* p) {p_ = p; return *this;}
    const_ptr& operator= (std::nullptr_t) {p_ = nullptr; return *this;}

    template <class T1> explicit const_ptr (T1* p): p_ (p) {}
    template <class T1> const_ptr (const_ptr<T1>& p): p_ (p.p_) {}

    template <class T1> const_ptr& operator= (T1* p) {p_ = p; return *this;}
    template <class T1> const_ptr& operator= (const_ptr<T1>& p) {
      p_ = p.p_; return *this;}

    T*       operator-> ()       {return p_;}
    const T* operator-> () const {return p_;}

    T&       operator* ()       {return *p_;}
    const T& operator* () const {return *p_;}

    operator       T* ()       {return p_;}
    operator const T* () const {return p_;}

    explicit operator bool () const {return p_ != nullptr;}

    T*       get ()       {return p_;}
    const T* get () const {return p_;}

  private:
    T* p_;
  };
}
