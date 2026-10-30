#include <libbutl/regex/details.hxx>

#if LIBBUTL_REGEX_DELEGATE_TO_STD_TRANSFORM_PRIMARY
#  include <regex>
#  include <string>
#  include <memory> // make_unique()
#endif

namespace butl { namespace regex { namespace details {

#if LIBBUTL_REGEX_DELEGATE_TO_STD_TRANSFORM_PRIMARY
using std::string;
using std::wstring;
using std::locale;
using std::make_unique;

// @@ Can we factor out regex_traits_proxy<C>::traits_wrapper implementation
//    into a common base class?
//
// regex_traits_proxy<char>
//
struct regex_traits_proxy<char>::traits_wrapper
{
  std::regex_traits<char> traits;
};

regex_traits_proxy<char>::regex_traits_proxy ()
    : wrapper_ (make_unique<traits_wrapper> ())
{
}

regex_traits_proxy<char>::~regex_traits_proxy () = default;

regex_traits_proxy<char>::
regex_traits_proxy (const regex_traits_proxy& x)
    : wrapper_ (make_unique<traits_wrapper> (*x.wrapper_))
{
}

regex_traits_proxy<char>::regex_traits_proxy (regex_traits_proxy&&) = default;

regex_traits_proxy<char>& regex_traits_proxy<char>::
operator= (const regex_traits_proxy& x)
{
  wrapper_ = make_unique<traits_wrapper> (*x.wrapper_);
  return *this;
}

regex_traits_proxy<char>&
regex_traits_proxy<char>::operator= (regex_traits_proxy&&) = default;

void
regex_traits_proxy<char>::imbue (const locale& l)
{
  wrapper_->traits.imbue (l);
}

string
regex_traits_proxy<char>::transform_primary(const string& s) const
{
  return wrapper_->traits.transform_primary (s.begin (), s.end ());
}

// regex_traits_proxy<wchar_t>
//
struct regex_traits_proxy<wchar_t>::traits_wrapper
{
  std::regex_traits<wchar_t> traits;
};

regex_traits_proxy<wchar_t>::regex_traits_proxy ()
    : wrapper_ (make_unique<traits_wrapper> ())
{
}

regex_traits_proxy<wchar_t>::~regex_traits_proxy () = default;

regex_traits_proxy<wchar_t>::
regex_traits_proxy (const regex_traits_proxy& x)
    : wrapper_ (make_unique<traits_wrapper> (*x.wrapper_))
{
}

regex_traits_proxy<wchar_t>::regex_traits_proxy (regex_traits_proxy&&) = default;

regex_traits_proxy<wchar_t>& regex_traits_proxy<wchar_t>::
operator= (const regex_traits_proxy& x)
{
  wrapper_ = make_unique<traits_wrapper> (*x.wrapper_);
  return *this;
}

regex_traits_proxy<wchar_t>&
regex_traits_proxy<wchar_t>::operator= (regex_traits_proxy&&) = default;

void
regex_traits_proxy<wchar_t>::imbue (const locale& l)
{
  wrapper_->traits.imbue (l);
}

wstring
regex_traits_proxy<wchar_t>::transform_primary(const wstring& s) const
{
  return wrapper_->traits.transform_primary (s.begin (), s.end ());
}
#else
int dummy; // Suppress the 'no symbols' warning.
#endif
} } } // namespace stud { namespace regex { namespace details {
