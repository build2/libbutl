#pragma once

#include <string>
#include <locale>
#include <memory>      // pointer_traits, unique_ptr
#include <utility>     // declval
#include <cstdint>     // uint16_t
#include <iterator>
#include <type_traits>

#include <libbutl/regex/config.hxx>

#ifdef __cpp_lib_three_way_comparison
#  include <compare>
#endif

// Note that using names containing double underscore in our own code is
// generally a bad idea. However, since libc++ uses such names throughout its
// implementation, the renaming would make our code incomparable to the
// original libc++ code and complicate the upgrades dramatically (not to
// mention the amount of work). Thus, let's keep the original names and see
// how it goes.
//
namespace butl { namespace regex { namespace details {

// libcxx/include/__type_traits/void_t.h
//
template <class...>
using __void_t LIBSTUD_REGEX_NODEBUG = void;

// libcxx/include/__type_traits/enable_if.h
//
template <bool _Bp, class _Tp = void>
#if 0
using __enable_if_t _LIBCPP_NODEBUG = typename enable_if<_Bp, _Tp>::type;
#else
using __enable_if_t LIBSTUD_REGEX_NODEBUG = typename std::enable_if<_Bp, _Tp>::type;
#endif

// libcxx/include/__type_traits/is_swappable.h
//
// Note that GCC 4.9 doesn't support variable templates. Thus, we will use the
// class templates with static members instead.
//
#if 0
template <class _Tp, class _Up, class = void>
inline const bool __is_swappable_with_v = false;

template <class _Tp>
inline const bool __is_swappable_v = __is_swappable_with_v<_Tp&, _Tp&>;

template <class _Tp, class _Up, bool = __is_swappable_with_v<_Tp, _Up> >
inline const bool __is_nothrow_swappable_with_v = false;

template <class _Tp>
inline const bool __is_nothrow_swappable_v = __is_nothrow_swappable_with_v<_Tp&, _Tp&>;

template <class _Tp, class _Up>
inline const bool __is_swappable_with_v<_Tp,
                                        _Up,
                                        __void_t<decltype(swap(std::declval<_Tp>(), std::declval<_Up>())),
                                                 decltype(swap(std::declval<_Up>(), std::declval<_Tp>()))> > = true;

#ifndef _LIBCPP_CXX03_LANG // C++03 doesn't have noexcept, so things are never nothrow swappable
template <class _Tp, class _Up>
inline const bool __is_nothrow_swappable_with_v<_Tp, _Up, true> =
    noexcept(swap(std::declval<_Tp>(), std::declval<_Up>())) &&
    noexcept(swap(std::declval<_Up>(), std::declval<_Tp>()));
#endif

#else

template <class _Tp, class _Up, class = void>
struct __is_swappable_with {
  static constexpr bool value = false;
};

template <class _Tp>
struct __is_swappable {
  static constexpr bool value = __is_swappable_with<_Tp&, _Tp&>::value;
};

template <class _Tp, class _Up, bool = __is_swappable_with<_Tp, _Up>::value >
struct __is_nothrow_swappable_with {
  static constexpr bool value = false;
};

template <class _Tp>
struct __is_nothrow_swappable {
  static constexpr bool value = __is_nothrow_swappable_with<_Tp&, _Tp&>::value;
};

template <class _Tp, class _Up>
struct __is_swappable_with<_Tp,
                           _Up,
                           __void_t<decltype(std::swap(std::declval<_Tp>(), std::declval<_Up>())),
                                    decltype(std::swap(std::declval<_Up>(), std::declval<_Tp>()))> > {
  static constexpr bool value = true;
};

template <class _Tp, class _Up>
struct __is_nothrow_swappable_with<_Tp, _Up, true> {
  static constexpr bool value =
    noexcept(std::swap(std::declval<_Tp>(), std::declval<_Up>())) &&
    noexcept(std::swap(std::declval<_Up>(), std::declval<_Tp>()));
};
#endif

// libcxx/include/__type_traits/detected_or.h
//
template <class _Default, class _Void, template <class...> class _Op, class... _Args>
struct __detector {
  using type LIBSTUD_REGEX_NODEBUG = _Default;
};

template <class _Default, template <class...> class _Op, class... _Args>
struct __detector<_Default, __void_t<_Op<_Args...> >, _Op, _Args...> {
  using type LIBSTUD_REGEX_NODEBUG = _Op<_Args...>;
};

template <class _Default, template <class...> class _Op, class... _Args>
using __detected_or_t LIBSTUD_REGEX_NODEBUG = typename __detector<_Default, void, _Op, _Args...>::type;

// libcxx/include/__type_traits/conjunction.h
//
template <class...>
using __expand_to_true LIBSTUD_REGEX_NODEBUG = std::true_type;

template <class... _Pred>
__expand_to_true<__enable_if_t<_Pred::value>...> __and_helper(int);

template <class...>
#if 0
false_type __and_helper(...);
#else
std::false_type __and_helper(...);
#endif

template <class... _Pred>
#if 0
using _And _LIBCPP_NODEBUG = decltype(std::__and_helper<_Pred...>(0));
#else
using _And LIBSTUD_REGEX_NODEBUG = decltype(__and_helper<_Pred...>(0));
#endif

// libcxx/include/__type_traits/disjunction.h
//
template <bool>
struct _OrImpl;

template <>
struct _OrImpl<true> {
  template <class _Res, class _First, class... _Rest>
  using _Result LIBSTUD_REGEX_NODEBUG =
    typename _OrImpl<!bool(_First::value) && sizeof...(_Rest) != 0>::template _Result<_First, _Rest...>;
};

template <>
struct _OrImpl<false> {
  template <class _Res, class...>
  using _Result LIBSTUD_REGEX_NODEBUG = _Res;
};

template <class... _Args>
#if 0
using _Or _LIBCPP_NODEBUG = typename _OrImpl<sizeof...(_Args) != 0>::template _Result<false_type, _Args...>;
#else
using _Or LIBSTUD_REGEX_NODEBUG = typename _OrImpl<sizeof...(_Args) != 0>::template _Result<std::false_type, _Args...>;
#endif

// libcxx/include/__type_traits/remove_reference.h
//
#if 0
#if __has_builtin(__remove_reference_t)
template <class _Tp>
struct _LIBCPP_NO_SPECIALIZATIONS remove_reference {
  using type _LIBCPP_NODEBUG = __remove_reference_t(_Tp);
};

template <class _Tp>
using __libcpp_remove_reference_t _LIBCPP_NODEBUG = __remove_reference_t(_Tp);
#elif __has_builtin(__remove_reference)
template <class _Tp>
struct remove_reference {
  using type _LIBCPP_NODEBUG = __remove_reference(_Tp);
};

template <class _Tp>
using __libcpp_remove_reference_t = typename remove_reference<_Tp>::type;
#else
#  error "remove_reference not implemented!"
#endif // __has_builtin(__remove_reference_t)

#else

template <class _Tp>
using __libcpp_remove_reference_t = typename std::remove_reference<_Tp>::type;
#endif

// libcxx/include/__type_traits/make_const_lvalue_ref.h
//
template <class _Tp>
using __make_const_lvalue_ref LIBSTUD_REGEX_NODEBUG = const __libcpp_remove_reference_t<_Tp>&;

// libcxx/include/__type_traits/decay.h
//
#if 0
#ifdef _LIBCPP_COMPILER_GCC
template <class _Tp>
using __decay_t _LIBCPP_NODEBUG = typename decay<_Tp>::type;
#else
template <class _Tp>
using __decay_t _LIBCPP_NODEBUG = __decay(_Tp);
#endif

#else

template <class _Tp>
using __decay_t LIBSTUD_REGEX_NODEBUG = typename std::decay<_Tp>::type;
#endif

// libcxx/include/__type_traits/nat.h
//
struct __nat {
#ifndef LIBSTUD_REGEX_CXX03_LANG
  __nat()                        = delete;
  __nat(const __nat&)            = delete;
  __nat& operator=(const __nat&) = delete;
  ~__nat()                       = delete;
#endif
};

// libcxx/include/__memory/pointer_traits.h
//
template <class _Tp>
LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR _Tp* __to_address(_Tp* __p) LIBSTUD_REGEX_NOEXCEPT {
#if 0
  static_assert(!is_function<_Tp>::value, "_Tp is a function type");
#else
  static_assert(!std::is_function<_Tp>::value, "_Tp is a function type");
#endif
  return __p;
}

// Note that GCC 4.9 doesn't support variable templates (see above for
// details).
//
#if 0
template <class _Pointer, class = void>
constexpr bool __has_to_address_v = false;

template <class _Pointer>
constexpr bool
    __has_to_address_v<_Pointer,
                       decltype((void)std::pointer_traits<_Pointer>::to_address(std::declval<const _Pointer&>()))> = true;

template <class _Pointer, __enable_if_t<__has_to_address_v<_Pointer>, int> = 0>
LIBSTUD_REGEX_CONSTEXPR __decay_t<decltype(std::pointer_traits<_Pointer>::to_address(std::declval<const _Pointer&>()))>
__to_address(const _Pointer& __p) LIBSTUD_REGEX_NOEXCEPT {
  return std::pointer_traits<_Pointer>::to_address(__p);
}

#else

template <class _Pointer, class = void>
struct __has_to_address {
  static constexpr bool value = false;
};

template <class _Pointer>
struct __has_to_address<_Pointer,
                        decltype((void)std::pointer_traits<_Pointer>::to_address(std::declval<const _Pointer&>()))> {
  static constexpr bool value = true;
};

template <class _Pointer, __enable_if_t<__has_to_address<_Pointer>::value, int> = 0>
LIBSTUD_REGEX_CONSTEXPR __decay_t<decltype(std::pointer_traits<_Pointer>::to_address(std::declval<const _Pointer&>()))>
__to_address(const _Pointer& __p) LIBSTUD_REGEX_NOEXCEPT {
  return std::pointer_traits<_Pointer>::to_address(__p);
};
#endif

// libcxx/include/__iterator/iterator_traits.h
//
template <class _Tp>
using __iterator_category LIBSTUD_REGEX_NODEBUG = typename _Tp::iterator_category;

template <class _Tp, class _Up>
using __has_iterator_category_convertible_to LIBSTUD_REGEX_NODEBUG =
#if 0
    is_convertible<__detected_or_t<__nat, __iterator_category, iterator_traits<_Tp> >, _Up>;
#else
    std::is_convertible<__detected_or_t<__nat, __iterator_category, std::iterator_traits<_Tp> >, _Up>;
#endif

template <class _Tp>
using __has_exactly_input_iterator_category LIBSTUD_REGEX_NODEBUG =
#if 0
    integral_constant<bool,
                      __has_iterator_category_convertible_to<_Tp, input_iterator_tag>::value &&
                          !__has_iterator_category_convertible_to<_Tp, forward_iterator_tag>::value>;
#else
    std::integral_constant<bool,
                            __has_iterator_category_convertible_to<_Tp, std::input_iterator_tag>::value &&
                               !__has_iterator_category_convertible_to<_Tp, std::forward_iterator_tag>::value>;
#endif

template <class _Tp>
using __has_forward_iterator_category LIBSTUD_REGEX_NODEBUG =
#if 0
    __has_iterator_category_convertible_to<_Tp, forward_iterator_tag>;
#else
    __has_iterator_category_convertible_to<_Tp, std::forward_iterator_tag>;
#endif

template <class _Iter>
#if 0
using __iterator_reference _LIBCPP_NODEBUG = typename iterator_traits<_Iter>::reference;
#else
using __iterator_reference LIBSTUD_REGEX_NODEBUG = typename std::iterator_traits<_Iter>::reference;
#endif

// libcxx/include/__iterator/wrap_iter.h
//
template <class _Iter>
class LIBSTUD_REGEX_WARN_UNUSED __wrap_iter {
public:
#if 0
  typedef typename iterator_traits<_Iter>::value_type value_type;
  typedef typename iterator_traits<_Iter>::difference_type difference_type;
  typedef typename iterator_traits<_Iter>::pointer pointer;
  typedef typename iterator_traits<_Iter>::reference reference;
  typedef typename iterator_traits<_Iter>::iterator_category iterator_category;
#if _LIBCPP_STD_VER >= 20
  typedef contiguous_iterator_tag iterator_concept;
#endif
#else
  typedef typename std::iterator_traits<_Iter>::value_type value_type;
  typedef typename std::iterator_traits<_Iter>::difference_type difference_type;
  typedef typename std::iterator_traits<_Iter>::pointer pointer;
  typedef typename std::iterator_traits<_Iter>::reference reference;
  typedef typename std::iterator_traits<_Iter>::iterator_category iterator_category;
#ifdef __cpp_lib_ranges
  typedef std::contiguous_iterator_tag iterator_concept;
#endif
#endif

private:
  _Iter __i_;

public:
// Suppress the bogus 'in C++14 constexpr will not imply const' warning, which
// MSVC annoys with until the version 15.0 (VS 2017).
//
#if defined(_MSC_VER) && _MSC_VER < 1910 // MSVC < 15.0
#  pragma warning (push)
#  pragma warning (disable:4814)
#endif
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter() LIBSTUD_REGEX_NOEXCEPT : __i_() {}
  template <class _OtherIter,
            __enable_if_t<
#if 0
                _And<is_convertible<const _OtherIter&, _Iter>,
                     _Or<is_same<reference, __iterator_reference<_OtherIter> >,
                         is_same<reference, __make_const_lvalue_ref<__iterator_reference<_OtherIter> > > > >::value,
#else
                _And<std::is_convertible<const _OtherIter&, _Iter>,
                  _Or<std::is_same<reference, __iterator_reference<_OtherIter> >,
                      std::is_same<reference, __make_const_lvalue_ref<__iterator_reference<_OtherIter> > > > >::value,
#endif
              int> = 0>
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter(const __wrap_iter<_OtherIter>& __u) LIBSTUD_REGEX_NOEXCEPT
      : __i_(__u.__i_) {}
  LIBSTUD_REGEX_NODISCARD LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 reference operator*() const LIBSTUD_REGEX_NOEXCEPT {
    return *__i_;
  }
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 pointer operator->() const LIBSTUD_REGEX_NOEXCEPT {
#if 0
    return std::__to_address(__i_);
#else
    return __to_address(__i_);
#endif
  }
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter& operator++() LIBSTUD_REGEX_NOEXCEPT {
    ++__i_;
    return *this;
  }
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter operator++(int) LIBSTUD_REGEX_NOEXCEPT {
    __wrap_iter __tmp(*this);
    ++(*this);
    return __tmp;
  }

  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter& operator--() LIBSTUD_REGEX_NOEXCEPT {
    --__i_;
    return *this;
  }
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter operator--(int) LIBSTUD_REGEX_NOEXCEPT {
    __wrap_iter __tmp(*this);
    --(*this);
    return __tmp;
  }
  LIBSTUD_REGEX_NODISCARD LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter
  operator+(difference_type __n) const LIBSTUD_REGEX_NOEXCEPT {
    __wrap_iter __w(*this);
    __w += __n;
    return __w;
  }
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter& operator+=(difference_type __n) LIBSTUD_REGEX_NOEXCEPT {
    __i_ += __n;
    return *this;
  }
  LIBSTUD_REGEX_NODISCARD LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter
  operator-(difference_type __n) const LIBSTUD_REGEX_NOEXCEPT {
    return *this + (-__n);
  }
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter& operator-=(difference_type __n) LIBSTUD_REGEX_NOEXCEPT {
    *this += -__n;
    return *this;
  }
  LIBSTUD_REGEX_NODISCARD LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 reference
  operator[](difference_type __n) const LIBSTUD_REGEX_NOEXCEPT {
    return __i_[__n];
  }
#if defined(_MSC_VER) && _MSC_VER < 1910
#  pragma warning (pop)
#endif

private:
  LIBSTUD_REGEX_HIDE_FROM_ABI LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 explicit __wrap_iter(_Iter __x) LIBSTUD_REGEX_NOEXCEPT : __i_(__x) {}

  template <class _Up>
  friend class __wrap_iter;
#if 0
  template <class _CharT, class _Traits, class _Alloc>
  friend class basic_string;
  template <class _CharT, class _Traits>
  friend class basic_string_view;
  template <class _Tp, class _Alloc>
  friend class vector;
  template <class _Tp, size_t>
  friend class span;
  template <class _Tp, size_t _Size>
  friend struct array;
#endif

#ifndef __cpp_lib_three_way_comparison
  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator==(const __wrap_iter& __x, const __wrap_iter& __y) LIBSTUD_REGEX_NOEXCEPT {
    return __x.__i_ == __y.__i_;
  }

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator==(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT {
    return __x.__i_ == __y.__i_;
  }

  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator<(const __wrap_iter& __x, const __wrap_iter& __y) LIBSTUD_REGEX_NOEXCEPT {
    return __x.__i_ < __y.__i_;
  }

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator<(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT {
    return __x.__i_ < __y.__i_;
  }

  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator!=(const __wrap_iter& __x, const __wrap_iter& __y) LIBSTUD_REGEX_NOEXCEPT {
    return !(__x == __y);
  }

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator!=(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT {
    return !(__x == __y);
  }

  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator>(const __wrap_iter& __x, const __wrap_iter& __y) LIBSTUD_REGEX_NOEXCEPT {
    return __y < __x;
  }

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator>(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT {
    return __y < __x;
  }

  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator>=(const __wrap_iter& __x, const __wrap_iter& __y) LIBSTUD_REGEX_NOEXCEPT {
    return !(__x < __y);
  }

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator>=(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT {
    return !(__x < __y);
  }

  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator<=(const __wrap_iter& __x, const __wrap_iter& __y) LIBSTUD_REGEX_NOEXCEPT {
    return !(__y < __x);
  }

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR bool
  operator<=(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT {
    return !(__y < __x);
  }

#else
  LIBSTUD_REGEX_HIDE_FROM_ABI friend bool operator==(const __wrap_iter&, const __wrap_iter&) = default;

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend constexpr bool
  operator==(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) noexcept {
    return __x.__i_ == __y.__i_;
  }

  template <class _Iter2>
  LIBSTUD_REGEX_HIDE_FROM_ABI friend constexpr std::strong_ordering
  operator<=>(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) noexcept {
#if 1
    using std::three_way_comparable_with;
    using std::strong_ordering;
#endif
    if constexpr (three_way_comparable_with<_Iter, _Iter2, strong_ordering>) {
      return __x.__i_ <=> __y.__i_;
    } else {
      if (__x.__i_ < __y.__i_)
        return strong_ordering::less;

      if (__x.__i_ == __y.__i_)
        return strong_ordering::equal;

      return strong_ordering::greater;
    }
  }
#endif // LIBSTUD_REGEX_STD_VER >= 20

#ifndef LIBSTUD_REGEX_CXX03_LANG
  template <class _Iter2>
  LIBSTUD_REGEX_NODISCARD LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 auto
  operator-(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT->decltype(__x.__i_ - __y.__i_)
#else
  template <class _Iter2>
  LIBSTUD_REGEX_NODISCARD LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14
  typename __wrap_iter::difference_type operator-(const __wrap_iter& __x, const __wrap_iter<_Iter2>& __y) LIBSTUD_REGEX_NOEXCEPT
#endif // C++03
  {
    return __x.__i_ - __y.__i_;
  }

  LIBSTUD_REGEX_NODISCARD LIBSTUD_REGEX_HIDE_FROM_ABI friend LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14 __wrap_iter
  operator+(typename __wrap_iter::difference_type __n, __wrap_iter __x) LIBSTUD_REGEX_NOEXCEPT {
    __x += __n;
    return __x;
  }
};

// Note that libstdc++ 4.9, which supports C++14, doesn't support the
// std::ctype_base::blank flag, introduced in C++11. In this, and potentially
// other similar cases, we fallback to defining the std::ctype_base::blank
// flag as std::ctype_base::space. Interestingly, MSVC (as of version 18.0)
// also defines these flags identically.
//
// Also note that for C locale the blank character class contains only tab and
// space (separators of words in a sentence) and the space character class
// extends blank with \n, \v, \f, \r (whitespaces).
//
using ctype_base_mask_numeric_type = std::make_unsigned_t<std::ctype_base::mask>;

template <typename T>
constexpr auto
ctype_base_blank_flag(...) -> decltype(T::space)
{
  return T::space;
}

template <typename T>
constexpr auto
ctype_base_blank_flag (bool) -> decltype(T::blank)
{
  return T::blank;
}

constexpr ctype_base_mask_numeric_type ctype_base_blank (
  static_cast<ctype_base_mask_numeric_type> (
    ctype_base_blank_flag<std::ctype_base> (true)));

// Validate, for good measure.
//
static_assert (
  (ctype_base_blank != std::ctype_base::print &&
   ctype_base_blank != std::ctype_base::cntrl &&
   ctype_base_blank != std::ctype_base::upper &&
   ctype_base_blank != std::ctype_base::lower &&
   ctype_base_blank != std::ctype_base::alpha &&
   ctype_base_blank != std::ctype_base::digit &&
   ctype_base_blank != std::ctype_base::punct &&
   ctype_base_blank != std::ctype_base::xdigit),
  "ctype_base_blank clashes with std::ctype_base::* constants");

// Provide the regex_traits::__regex_word flag by claiming the rightmost not
// taken bit in std::ctype_base::mask.
//
constexpr ctype_base_mask_numeric_type ctype_base_mask_all_flags (
  std::ctype_base::space  |
  std::ctype_base::print  |
  std::ctype_base::cntrl  |
  std::ctype_base::upper  |
  std::ctype_base::lower  |
  std::ctype_base::alpha  |
  std::ctype_base::digit  |
  std::ctype_base::punct  |
  std::ctype_base::xdigit |
  ctype_base_blank);

// Note: '+1' flips the rightmost 0 and '~...&' removes the existing flags.
//
constexpr ctype_base_mask_numeric_type regex_word (
  ~ctype_base_mask_all_flags & (ctype_base_mask_all_flags + 1));

static_assert (regex_word != 0,
               "cannot assign value for regex_word since no "
               "zero bits left in std::ctype_base::mask");

// Validate, for good measure.
//
static_assert (
  regex_word != std::ctype_base::space  &&
  regex_word != std::ctype_base::print  &&
  regex_word != std::ctype_base::cntrl  &&
  regex_word != std::ctype_base::upper  &&
  regex_word != std::ctype_base::lower  &&
  regex_word != std::ctype_base::alpha  &&
  regex_word != std::ctype_base::digit  &&
  regex_word != std::ctype_base::punct  &&
  regex_word != std::ctype_base::xdigit &&
  regex_word != ctype_base_blank,
  "regex_word clashes with std::ctype_base::* constants");

// Delegate stud::regex::regex_traits::transform_primary() call to
// std::regex_traits::transform_primary() (see
// LIBSTUD_REGEX_DELEGATE_TO_STD_TRANSFORM_PRIMARY for details), but make sure
// that no std::regex classes are exposed to the user.
//
#if LIBSTUD_REGEX_DELEGATE_TO_STD_TRANSFORM_PRIMARY
template <typename C>
struct regex_traits_proxy
{
  // Update the internal std::regex_traits object with the new locale.
  //
  void
  imbue (const std::locale&);

  // Delegate to the respective std::regex_traits::transform_primary() call.
  //
  std::basic_string<C>
  transform_primary (const std::basic_string<C>&) const;
};

template <>
struct LIBBUTL_SYMEXPORT regex_traits_proxy<char>
{
  void
  imbue (const std::locale&);

  std::string
  transform_primary (const std::string&) const;

  regex_traits_proxy ();
  ~regex_traits_proxy ();

  regex_traits_proxy (const regex_traits_proxy&);
  regex_traits_proxy (regex_traits_proxy&&);
  regex_traits_proxy& operator= (const regex_traits_proxy&);
  regex_traits_proxy& operator= (regex_traits_proxy&&);

private:
  struct traits_wrapper;
  std::unique_ptr<traits_wrapper> wrapper_;
};

template <>
struct LIBBUTL_SYMEXPORT regex_traits_proxy<wchar_t>
{
  void
  imbue (const std::locale&);

  std::wstring
  transform_primary (const std::wstring&) const;

  regex_traits_proxy ();
  ~regex_traits_proxy ();

  regex_traits_proxy (const regex_traits_proxy&);
  regex_traits_proxy (regex_traits_proxy&&);
  regex_traits_proxy& operator= (const regex_traits_proxy&);
  regex_traits_proxy& operator= (regex_traits_proxy&&);

private:
  struct traits_wrapper;
  std::unique_ptr<traits_wrapper> wrapper_;
};
#endif
} } } // namespace stud { namespace regex { namespace details {
