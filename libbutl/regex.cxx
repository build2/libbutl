// file      : libbutl/regex.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/regex.hxx>

#include <ostream>
#include <sstream>
#include <stdexcept> // runtime_error

#if defined(_MSC_VER) && _MSC_VER < 2000
#  include <cstring> // strstr()
#endif

#include <libbutl/utility.hxx> // operator<<(ostream, exception)

namespace std
{
  // Currently libstdc++ just returns the name of the exception (bug #67361).
  // So we check that the description contains at least one space character.
  //
  // While VC's description is meaningful, it has an undesired prefix that
  // resembles the following: 'regex_error(error_badrepeat): '. So we skip it.
  //
  ostream&
  operator<< (ostream& o, const regex_error& e)
  {
    const char* d (e.what ());

#if defined(_MSC_VER) && _MSC_VER < 2000
    // Note: run the regex test like this to check new VC version:
    //
    // ./driver.exe a '{' b
    //
    const char* rd (strstr (d, "): "));
    if (rd != nullptr)
      d = rd + 3;
#endif

    ostringstream os;
    os << runtime_error (d); // Sanitize the description.

    string s (os.str ());
    if (s.find (' ') != string::npos)
      o << ": " << s;

    return o;
  }
}
