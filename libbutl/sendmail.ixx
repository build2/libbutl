// file      : libbutl/sendmail.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cstddef> // size_t
#include <utility> // move(), forward()

namespace butl
{
  template <typename E, typename... O>
  inline sendmail::
  sendmail (E&& err,
            const std::string& from,
            const std::string& subj,
            const recipients_type& to,
            const recipients_type& cc,
            const recipients_type& bcc,
            O&&... options)
      : sendmail ([] (const char* [], std::size_t) {},
                  std::forward<E> (err),
                  from,
                  subj,
                  to,
                  cc,
                  bcc,
                  std::forward<O> (options)...)
  {
  }

  template <typename E>
  inline sendmail::
  sendmail (E&& err,
            const std::string& from,
            const std::string& subj,
            const recipients_type& to,
            const recipients_type& cc)
      : sendmail (err, from, subj, to, cc, recipients_type ())
  {
  }

  template <typename E>
  inline sendmail::
  sendmail (E&& err,
            const std::string& from,
            const std::string& subj,
            const recipients_type& to)
      : sendmail (err, from, subj, to, recipients_type ())
  {
  }

  template <typename C, typename E, typename... O>
  inline sendmail::
  sendmail (const C& cmdc,
            E&& err,
            const std::string& from,
            const std::string& subj,
            const recipients_type& to,
            const recipients_type& cc,
            const recipients_type& bcc,
            O&&... options)
  {
    fdpipe pipe (fdopen_pipe ()); // Text mode seems appropriate.

    process& p (*this);
    p = process_start_callback (cmdc,
                                pipe,
                                2, // No output expected so redirect to stderr.
                                std::forward<E> (err),
                                "sendmail",
                                "-i", // Don't treat '.' as the end of input.
                                "-t", // Read recipients from headers.
                                std::forward<O> (options)...);

    // Close the reading end of the pipe not to block on writing if sendmail
    // terminates before that.
    //
    pipe.in.close ();

    out.open (std::move (pipe.out));

    // Write headers.
    //
    // Note that if this throws, then the ofdstream will be closed first
    // (which should signal to the process we are done). Then the process's
    // destructor will wait for its termination ignoring any errors.
    //
    headers (from, subj, to, cc, bcc);
  }

  template <typename C, typename E>
  inline sendmail::
  sendmail (const C& cmdc,
            E&& err,
            const std::string& from,
            const std::string& subj,
            const recipients_type& to,
            const recipients_type& cc)
      : sendmail (cmdc, err, from, subj, to, cc, recipients_type ())
  {
  }

  template <typename C, typename E>
  inline sendmail::
  sendmail (const C& cmdc,
            E&& err,
            const std::string& from,
            const std::string& subj,
            const recipients_type& to)
      : sendmail (cmdc, err, from, subj, to, recipients_type ())
  {
  }
}
