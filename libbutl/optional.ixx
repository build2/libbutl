// file      : libbutl/optional.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  namespace details
  {
    // optional_data<T, false>
    //

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (nullopt_t)
    {
      if (v_)
      {
        d_.~T ();
        v_ = false;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (const T& v)
    {
      if (v_)
      {
        d_.~T ();
        v_ = false;
      }

      new (&d_) T (v);
      v_ = true;

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (T&& v)
    {
      if (v_)
      {
        d_.~T ();
        v_ = false;
      }

      new (&d_) T (std::move (v));
      v_ = true;

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>::
    optional_data (const optional_data& o)
        : v_ (o.v_)
    {
      if (v_)
        new (&d_) T (o.d_);
    }

    template <typename T>
    inline optional_data<T, false>::
    optional_data (optional_data&& o)
        : v_ (o.v_)
    {
      if (v_)
        new (&d_) T (std::move (o.d_));
    }

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (const optional_data& o)
    {
      if (v_)
      {
        d_.~T ();
        v_ = false;
      }

      if (o.v_)
      {
        new (&d_) T (o.d_);
        v_ = true;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (optional_data&& o)
    {
      if (v_)
      {
        d_.~T ();
        v_ = false;
      }

      if (o.v_)
      {
        new (&d_) T (std::move (o.d_));
        v_ = true;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>::
    ~optional_data ()
    {
      if (v_)
        d_.~T ();
    }

    // optional_data<T, true>
    //

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (nullopt_t)
    {
      if (v_)
        v_ = false;

      return *this;
    }

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (const T& v)
    {
      if (v_)
        v_ = false;

      new (&d_) T (v);
      v_ = true;

      return *this;
    }

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (T&& v)
    {
      if (v_)
        v_ = false;

      new (&d_) T (std::move (v));
      v_ = true;

      return *this;
    }

    template <typename T>
    inline optional_data<T, true>::
    optional_data (const optional_data& o)
        : v_ (o.v_)
    {
      if (v_)
        new (&d_) T (o.d_);
    }

    template <typename T>
    inline optional_data<T, true>::
    optional_data (optional_data&& o)
        : v_ (o.v_)
    {
      if (v_)
        new (&d_) T (std::move (o.d_));
    }

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (const optional_data& o)
    {
      if (v_)
        v_ = false;

      if (o.v_)
      {
        new (&d_) T (o.d_);
        v_ = true;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (optional_data&& o)
    {
      if (v_)
        v_ = false;

      if (o.v_)
      {
        new (&d_) T (std::move (o.d_));
        v_ = true;
      }

      return *this;
    }
  }
}
