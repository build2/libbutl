// file      : libbutl/sendmail.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/sendmail.mxx>
#endif

// C includes.

#ifndef __cpp_lib_modules_ts
#include <string>
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.sendmail;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
import butl.process;
import butl.fdstream;
import butl.small_vector;
#endif

#endif

using namespace std;

namespace butl
{
  void sendmail::
  headers (const std::string& from,
           const std::string& subj,
           const recipients_type& to,
           const recipients_type& cc,
           const recipients_type& bcc)
  {
    if (!from.empty ())
      out << "From: " << from << endl;

    auto rcp = [this] (const char* h, const recipients_type& rs)
    {
      if (!rs.empty ())
      {
        bool f (true);
        out << h << ": ";
        for (const string& r: rs)
          out << (f ? (f = false, "") : ", ") << r;
        out << endl;
      }
    };

    rcp ("To",  to);
    rcp ("Cc",  cc);
    rcp ("Bcc", bcc);

    out << "Subject: " << subj << endl
        << endl; // Header/body separator.
  }
}
