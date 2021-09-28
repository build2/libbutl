// file      : libbutl/lz4-stream.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <memory>  // unique_ptr
#include <cstddef> // size_t
#include <cstdint> // uint64_t
#include <utility> // move()
#include <istream>
#include <ostream>
#include <cassert>

#include <libbutl/lz4.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/bufstreambuf.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  namespace lz4
  {
    // istream
    //

    class LIBBUTL_SYMEXPORT istreambuf: public bufstreambuf
    {
    public:
      optional<std::uint64_t>
      open (std::istream&, bool end);

      bool
      is_open () const {return is_ != nullptr;}

      void
      close ();

    public:
      using base = bufstreambuf;

      // basic_streambuf input interface.
      //
    public:
      virtual int_type
      underflow () override;

      // Direct access to the get area. Use with caution.
      //
      using base::gptr;
      using base::egptr;
      using base::gbump;

      // Return the (logical) position of the next byte to be read.
      //
      using base::tellg;

    private:
      std::pair<std::size_t, bool>
      read (char*, std::size_t);

      bool
      load ();

    private:
      std::istream* is_ = nullptr;
      bool end_;
      decompressor d_;
      std::unique_ptr<char[]> ib_; // Decompressor input buffer.
      std::unique_ptr<char[]> ob_; // Decompressor output buffer.
      std::size_t h_;              // Decompressor next chunk hint.
    };

    // Typical usage:
    //
    //   try
    //   {
    //     ifdstream ifs (..., fdopen_mode::binary, ifdstream::badbit);
    //     lz4::istream izs (ifs, true /* end */);
    //     ... // Read from izs.
    //   }
    //   catch (const invalid_argument& e)
    //   {
    //     ... // Invalid compressed content, call e.what() for description.
    //   }
    //   catch (/* ifdstream exceptions */)
    //   {
    //     ...
    //   }
    //
    // See class decompressor for details on semantics nad exceptions thrown.
    //
    // @@ TODO: get rid of badbit-only requirement.
    // @@ TODO: re-openning support (will need compressor reset).
    //
    class LIBBUTL_SYMEXPORT istream: public std::istream
    {
    public:
      explicit
      istream (iostate e = badbit | failbit)
        : std::istream (&buf_)
      {
        assert (e & badbit);
        exceptions (e);
      }

      // The underlying input stream is expected to throw on badbit but not
      // failbit. If end is true, then on reaching the end of compressed data
      // verify there is no more input.
      //
      // Note that this implementation does not support handing streams of
      // compressed contents (end is false) that may include individual
      // contents that uncompress to 0 bytes (see istreambuf::open()
      // implementation for details).
      //
      istream (std::istream& is, bool end, iostate e = badbit | failbit)
        : istream (e)
      {
        open (is, end);
      }

      // Return decompressed content size, if available.
      //
      optional<std::uint64_t>
      open (std::istream& is, bool end)
      {
        return buf_.open (is, end);
      }

      bool
      is_open () const
      {
        return buf_.is_open ();
      }

      // Signal that no further uncompressed input will be read.
      //
      void
      close ()
      {
        return buf_.close ();
      }

    private:
      istreambuf buf_;
    };

    // ostream
    //

    class LIBBUTL_SYMEXPORT ostreambuf: public bufstreambuf
    {
    public:
      void
      open (std::ostream&,
            int compression_level,
            int block_size_id,
            optional<std::uint64_t> content_size);

      bool
      is_open () const {return os_ != nullptr;}

      void
      close ();

      virtual
      ~ostreambuf () override;

    public:
      using base = bufstreambuf;

      // basic_streambuf output interface.
      //
      // Note that syncing the input buffer before the end doesn't make much
      // sense (it will just get buffered in the compressor). In fact, it can
      // break our single-shot compression arrangement (for compatibility with
      // the lz4 utility). Thus we inherit noop sync() from the base.
      //
    public:
      virtual int_type
      overflow (int_type) override;

      virtual std::streamsize
      xsputn (const char_type*, std::streamsize) override;

      // Return the (logical) position of the next byte to be written.
      //
      using base::tellp;

    private:
      void
      write (char*, std::size_t);

      void
      save ();

    private:
      std::ostream* os_ = nullptr;
      bool end_;
      compressor c_;
      std::unique_ptr<char[]> ib_; // Compressor input buffer.
      std::unique_ptr<char[]> ob_; // Compressor output buffer.
    };

    // Typical usage:
    //
    //   try
    //   {
    //     ofdstream ofs (..., fdopen_mode::binary);
    //     lz4::ostream ozs (ofs, 9, 4 /* 64KB */, nullopt /* content_size */);
    //
    //     ... // Write to ozs.
    //
    //     ozs.close ();
    //     ofs.close ();
    //   }
    //   catch (/* ofdstream exceptions */)
    //   {
    //     ...
    //   }
    //
    // See class compressor for details on semantics nad exceptions thrown.
    //
    // @@ TODO: re-openning support (will need compressor reset).
    //
    class LIBBUTL_SYMEXPORT ostream: public std::ostream
    {
    public:
      explicit
      ostream (iostate e = badbit | failbit)
        : std::ostream (&buf_)
      {
        assert (e & badbit);
        exceptions (e);
      }

      // The underlying output stream is expected to throw on badbit or
      // failbit.
      //
      // See compress() for the description of the compression level, block
      // size and content size arguments.
      //
      ostream (std::ostream& os,
               int compression_level,
               int block_size_id,
               optional<std::uint64_t> content_size,
               iostate e = badbit | failbit)
        : ostream (e)
      {
        open (os, compression_level, block_size_id, content_size);
      }

      void
      open (std::ostream& os,
            int compression_level,
            int block_size_id,
            optional<std::uint64_t> content_size)
      {
        buf_.open (os, compression_level, block_size_id, content_size);
      }

      bool
      is_open () const
      {
        return buf_.is_open ();
      }

      // Signal that no further uncompressed output will be written.
      //
      void
      close ()
      {
        return buf_.close ();
      }

    private:
      ostreambuf buf_;
    };
  }
}
