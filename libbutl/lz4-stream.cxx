// file      : libbutl/lz4-stream.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/lz4-stream.hxx>

#include <cstring>   // memcpy()
#include <stdexcept> // invalid_argument

#include <libbutl/utility.hxx> // eof()

using namespace std;

namespace butl
{
  namespace lz4
  {
    // istream
    //

    // Read into the specified buffer returning the number of bytes read and
    // the eof flag.
    //
    pair<size_t, bool> istreambuf::
    read (char* b, size_t c)
    {
      size_t n (0);
      bool e (false);

      // @@ TODO: would it be faster to do a direct buffer copy if input
      //    stream is bufstreabuf-based (see sha*.cxx for code)?
      do
      {
        e = eof (is_->read (b + n, c - n));
        n += static_cast<size_t> (is_->gcount ());
      }
      while (!e && n != c);

      return make_pair (n, e);
    }

    optional<uint64_t> istreambuf::
    open (std::istream& is, bool end)
    {
      assert (is.exceptions () == std::istream::badbit);

      is_ = &is;
      end_ = end;

      // Read in the header and allocate the buffers.
      //
      // What if we hit EOF here? And could begin() return 0? Turns out the
      // answer to both questions is yes: 0-byte content compresses to 15
      // bytes (with or without content size; 1-byte -- to 20/28 bytes). We
      // can ignore EOF here since an attempt to read more will result in
      // another EOF. And our load() is prepared to handle 0 hint.
      //
      // @@ We could end up leaving some of the input content from the header
      //    in the input buffer which the caller will have to way of using
      //    (e.g., in a stream of compressed contents). Doesn't look like
      //    there is much we can do (our streams don't support putback) other
      //    than document this limitation.
      //
      optional<uint64_t> r;

      d_.hn = read (d_.hb, sizeof (d_.hb)).first;
      h_ = d_.begin (&r);

      ib_.reset ((d_.ib = new char[d_.ic]));
      ob_.reset ((d_.ob = new char[d_.oc]));

      // Copy over whatever is left in the header buffer.
      //
      memcpy (d_.ib, d_.hb, (d_.in = d_.hn));

      setg (d_.ob, d_.ob, d_.ob);
      return r;
    }

    void istreambuf::
    close ()
    {
      if (is_open ())
      {
        is_ = nullptr;
      }
    }

    istreambuf::int_type istreambuf::
    underflow ()
    {
      int_type r (traits_type::eof ());

      if (is_open ())
      {
        if (gptr () < egptr () || load ())
          r = traits_type::to_int_type (*gptr ());
      }

      return r;
    }

    bool istreambuf::
    load ()
    {
      // Note that the first call to this function may be with h_ == 0 (see
      // open() for details). In which case we just need to verify there is
      // no just after the compressed content.
      //
      bool r;

      if (h_ == 0)
        r = false; // EOF
      else
      {
        // Note: next() may just buffer the data.
        //
        do
        {
          // Note that on the first call we may already have some data in the
          // input buffer (leftover header data).
          //
          if (h_ > d_.in)
          {
            pair<size_t, bool> p (read (d_.ib + d_.in, h_ - d_.in));

            d_.in += p.first;

            if (p.second && d_.in != h_)
              throw invalid_argument ("incomplete LZ4 compressed content");
          }

          h_ = d_.next (); // Clears d_.in.

        } while (d_.on == 0 && h_ != 0);

        setg (d_.ob, d_.ob, d_.ob + d_.on);
        off_ += d_.on;
        r = (d_.on != 0);
      }

      // If we don't expect any more compressed content and we were asked to
      // end the underlying input stream, then verify there is no more input.
      //
      if (h_ == 0 && end_)
      {
        end_ = false;

        if (d_.in != 0 ||
            (!is_->eof () &&
             is_->good () &&
             is_->peek () != istream::traits_type::eof ()))
          throw invalid_argument ("junk after LZ4 compressed content");
      }

      return r;
    }

    // ostream
    //

    void ostreambuf::
    write (char* b, std::size_t n)
    {
      os_->write (b, static_cast<streamsize> (n));
    }

    void ostreambuf::
    open (std::ostream& os,
          int level,
          int block_id,
          optional<std::uint64_t> content_size)
    {
      assert (os.exceptions () == (std::ostream::badbit |
                                   std::ostream::failbit));

      os_ = &os;

      // Determine required buffer capacities.
      //
      c_.begin (level, block_id, content_size);

      ib_.reset ((c_.ib = new char[c_.ic]));
      ob_.reset ((c_.ob = new char[c_.oc]));

      setp (c_.ib, c_.ib + c_.ic - 1); // Keep space for overflow's char.
      end_ = false;
    }

    void ostreambuf::
    close ()
    {
      if (is_open ())
      {
        if (!end_)
          save ();

        os_ = nullptr;
      }
    }

    ostreambuf::
    ~ostreambuf ()
    {
      close ();
    }

    ostreambuf::int_type ostreambuf::
    overflow (int_type c)
    {
      int_type r (traits_type::eof ());

      if (is_open () && c != traits_type::eof ())
      {
        // Store last character in the space we reserved in open(). Note
        // that pbump() doesn't do any checks.
        //
        *pptr () = traits_type::to_char_type (c);
        pbump (1);

        save ();
        r = c;
      }

      return r;
    }

    void ostreambuf::
    save ()
    {
      c_.in = pptr () - pbase ();
      off_ += c_.in;

      // We assume this is the end if the input buffer is not full.
      //
      end_ = (c_.in != c_.ic);
      c_.next (end_);

      if (c_.on != 0) // next() may just buffer the data.
        write (c_.ob, c_.on);

      setp (c_.ib, c_.ib + c_.ic - 1);
    }

    streamsize ostreambuf::
    xsputn (const char_type* s, streamsize sn)
    {
      if (!is_open () || end_)
        return 0;

      // To avoid futher 'signed/unsigned comparison' compiler warnings.
      //
      size_t n (static_cast<size_t> (sn));

      // The plan is to keep copying the data into the input buffer and
      // calling save() (our compressor API currently has no way of avoiding
      // the copy).
      //
      while (n != 0)
      {
        // Amount of free space in the buffer (including the extra byte
        // we've reserved).
        //
        size_t an (epptr () - pptr () + 1);

        size_t m (n > an ? an : n);
        memcpy (pptr (), s, m);
        pbump (static_cast<int> (m));

        if (n < an)
          break; // All fitted with at least 1 byte left.

        save ();

        s += m;
        n -= m;
      }

      return sn;
    }
  }
}
