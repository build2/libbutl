// file      : libbutl/sendmail.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/sendmail.hxx>

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
