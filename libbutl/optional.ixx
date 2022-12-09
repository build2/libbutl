// file      : libbutl/optional.ixx -*- C++ -*-
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
        d_ = v;
      else
      {
        new (&d_) T (v);
        v_ = true;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (T&& v)
    {
      if (v_)
        d_ = std::move (v);
      else
      {
        new (&d_) T (std::move (v));
        v_ = true;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (const optional_data& o)
    {
      if (o.v_)
      {
        if (v_)
          d_ = o.d_;
        else
        {
          new (&d_) T (o.d_);
          v_ = true;
        }
      }
      else if (v_)
      {
        d_.~T ();
        v_ = false;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, false>& optional_data<T, false>::
    operator= (optional_data&& o)
      noexcept (std::is_nothrow_move_constructible<T>::value &&
                std::is_nothrow_move_assignable<T>::value    &&
                std::is_nothrow_destructible<T>::value)
    {
      if (o.v_)
      {
        if (v_)
          d_ = std::move (o.d_);
        else
        {
          new (&d_) T (std::move (o.d_));
          v_ = true;
        }
      }
      else if (v_)
      {
        d_.~T ();
        v_ = false;
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
        d_ = v;
      else
      {
        new (&d_) T (v);
        v_ = true;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (T&& v)
    {
      if (v_)
        d_ = std::move (v);
      else
      {
        new (&d_) T (std::move (v));
        v_ = true;
      }

      return *this;
    }

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (const optional_data& o)
    {
      if (o.v_)
      {
        if (v_)
          d_ = o.d_;
        else
        {
          new (&d_) T (o.d_);
          v_ = true;
        }
      }
      else if (v_)
        v_ = false;

      return *this;
    }

    template <typename T>
    inline optional_data<T, true>& optional_data<T, true>::
    operator= (optional_data&& o)
      noexcept (std::is_nothrow_move_constructible<T>::value &&
                std::is_nothrow_move_assignable<T>::value)
    {
      if (o.v_)
      {
        if (v_)
          d_ = std::move (o.d_);
        else
        {
          new (&d_) T (std::move (o.d_));
          v_ = true;
        }
      }
      else if (v_)
        v_ = false;

      return *this;
    }
  }
}
