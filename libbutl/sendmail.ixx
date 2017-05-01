// file      : libbutl/sendmail.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

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
    p = process_start (cmdc,
                       pipe.in,
                       2, // No output expected so redirect to stderr.
                       std::forward<E> (err),
                       dir_path (),
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
}
