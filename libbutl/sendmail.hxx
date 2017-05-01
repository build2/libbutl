// file      : libbutl/sendmail.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_SENDMAIL_HXX
#define LIBBUTL_SENDMAIL_HXX

#include <string>

#include <libbutl/export.hxx>

#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/small-vector.hxx>

namespace butl
{
  // Send email using the sendmail(1) program.
  //
  // Write the body of the email to out. Note that you must explicitly close
  // it before calling wait(). Throw process_error and io_error (both derive
  // from system_error) in case of errors.
  //
  // Typical usage:
  //
  // try
  // {
  //   sendmail sm (2,                    // Diagnostics to stderr.
  //                "",                   // Default From: address.
  //                "Test subject",
  //                {"test@example.com"});
  //
  //   sm.out << "Test body" << endl;
  //
  //   sm.out.close ();
  //
  //   if (!sm.wait ())
  //     ... // sendmail returned non-zero status.
  // }
  // catch (const std::system_error& e)
  // {
  //   cerr << "sendmail error: " << e << endl;
  // }
  //
  class LIBBUTL_EXPORT sendmail: public process
  {
  public:
    ofdstream out;

    // Notes:
    //
    // - If from is empty then the process user's address is used.
    //
    // - The to/cc/bcc addressed should already be quoted if required.
    //
    using recipients_type = small_vector<std::string, 1>;

    template <typename E, typename... O>
    sendmail (E&& err,
              const std::string& from,
              const std::string& subject,
              const recipients_type& to,
              const recipients_type& cc = recipients_type (),
              const recipients_type& bcc = recipients_type (),
              O&&... options);

    // Version with the command line callback (see process_run() for details).
    //
    template <typename C, typename E, typename... O>
    sendmail (const C&,
              E&& err,
              const std::string& from,
              const std::string& subject,
              const recipients_type& to,
              const recipients_type& cc = recipients_type (),
              const recipients_type& bcc = recipients_type (),
              O&&... options);

  private:
    void
    headers (const std::string& from,
             const std::string& subj,
             const recipients_type& to,
             const recipients_type& cc,
             const recipients_type& bcc);
  };
}

#include <libbutl/sendmail.ixx>

#endif // LIBBUTL_SENDMAIL_HXX
