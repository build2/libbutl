#include <libbutl/regex/regex-io.hxx>

#include <string>
#include <cctype>  // tolower()
#include <ostream>
#include <cassert>

namespace butl
{
  namespace regex
  {
    std::ostream&
    operator<< (std::ostream& o, const regex_error& e)
    {
      std::string s (e.what ());

      assert (!s.empty ()); // See regex.cxx for error descriptions.

      s[0] = std::tolower (s[0]);

      if (s.back () == '.')
        s.pop_back ();

      o << s;
      return o;
    }
  }
}
