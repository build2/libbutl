// file      : libbutl/lz4.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/lz4.hxx>

// This careful macro dance makes sure that all the LZ4 C API functions are
// made static while making sure we include the headers in the same way as the
// implementation files that we include below.
//
#define LZ4LIB_VISIBILITY static
#define LZ4_STATIC_LINKING_ONLY
#define LZ4_PUBLISH_STATIC_FUNCTIONS
#define LZ4_DISABLE_DEPRECATE_WARNINGS
#include "lz4.h"
#include "lz4hc.h"

#define LZ4FLIB_VISIBILITY static
#define LZ4F_STATIC_LINKING_ONLY
#define LZ4F_PUBLISH_STATIC_FUNCTIONS
#define LZ4F_DISABLE_DEPRECATE_WARNINGS
#include "lz4frame.h"

#include <new>       // bad_alloc
#include <memory>    // unique_ptr
#include <cstring>   // memcpy()
#include <cassert>
#include <stdexcept> // invalid_argument, logic_error

#include <libbutl/utility.hxx> // eos()

#if 0
#include <libbutl/lz4-stream.hxx>
#endif

using namespace std;

namespace butl
{
  namespace lz4
  {
    static inline size_t
    block_size (LZ4F_blockSizeID_t id)
    {
      return (id == LZ4F_max4MB   ? 4 * 1024 * 1024 :
              id == LZ4F_max1MB   ? 1 * 1024 * 1024 :
              id == LZ4F_max256KB ?      256 * 1024 :
              id == LZ4F_max64KB  ?       64 * 1024 : 0);
    }

    [[noreturn]] static void
    throw_exception (LZ4F_errorCodes c)
    {
      using i = invalid_argument;

      switch (c)
      {
      case LZ4F_ERROR_GENERIC:                     throw i ("generic LZ4 error");
      case LZ4F_ERROR_maxBlockSize_invalid:        throw i ("invalid LZ4 block size");
      case LZ4F_ERROR_blockMode_invalid:           throw i ("invalid LZ4 block mode");
      case LZ4F_ERROR_contentChecksumFlag_invalid: throw i ("invalid LZ4 content checksum flag");
      case LZ4F_ERROR_compressionLevel_invalid:    throw i ("invalid LZ4 compression level");
      case LZ4F_ERROR_headerVersion_wrong:         throw i ("wrong LZ4 header version");
      case LZ4F_ERROR_blockChecksum_invalid:       throw i ("invalid LZ4 block checksum");
      case LZ4F_ERROR_reservedFlag_set:            throw i ("reserved LZ4 flag set");
      case LZ4F_ERROR_srcSize_tooLarge:            throw i ("LZ4 input too large");
      case LZ4F_ERROR_dstMaxSize_tooSmall:         throw i ("LZ4 output too small");
      case LZ4F_ERROR_frameHeader_incomplete:      throw i ("incomplete LZ4 frame header");
      case LZ4F_ERROR_frameType_unknown:           throw i ("unknown LZ4 frame type");
      case LZ4F_ERROR_frameSize_wrong:             throw i ("wrong LZ4 frame size");
      case LZ4F_ERROR_decompressionFailed:         throw i ("invalid LZ4 compressed content");
      case LZ4F_ERROR_headerChecksum_invalid:      throw i ("invalid LZ4 header checksum");
      case LZ4F_ERROR_contentChecksum_invalid:     throw i ("invalid LZ4 content checksum");

      case LZ4F_ERROR_allocation_failed:           throw bad_alloc ();

      // These seem to be programming errors.
      //
      case LZ4F_ERROR_srcPtr_wrong:                 // NULL pointer.
      case LZ4F_ERROR_frameDecoding_alreadyStarted: // Incorrect call seq.

      // We should never get these.
      //
      case LZ4F_OK_NoError:
      case LZ4F_ERROR_maxCode:
      case _LZ4F_dummy_error_enum_for_c89_never_used:
        break;
      }

      assert (false);
      throw logic_error (LZ4F_getErrorName ((LZ4F_errorCode_t)(-c)));
    }

    // As above but for erroneous LZ4F_*() function result.
    //
    [[noreturn]] static inline void
    throw_exception (size_t r)
    {
      throw_exception (LZ4F_getErrorCode (r));
    }

    // compression
    //

    compressor::
    ~compressor ()
    {
      if (LZ4F_cctx* ctx = static_cast<LZ4F_cctx*> (ctx_))
      {
        LZ4F_errorCode_t e (LZ4F_freeCompressionContext (ctx));
        assert (!LZ4F_isError (e));
      }
    }

    inline void compressor::
    init_preferences (void* vp) const
    {
      LZ4F_preferences_t* p (static_cast<LZ4F_preferences_t*> (vp));

      p->autoFlush = 1;
      p->favorDecSpeed = 0;
      p->compressionLevel = level_;
      p->frameInfo.blockMode = LZ4F_blockLinked;
      p->frameInfo.blockSizeID = static_cast<LZ4F_blockSizeID_t> (block_id_);
      p->frameInfo.blockChecksumFlag = LZ4F_noBlockChecksum;
      p->frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
      p->frameInfo.contentSize = content_size_
        ? static_cast<unsigned long long> (*content_size_)
        : 0;
    }

    void compressor::
    begin (int level,
           int block_id,
           optional<uint64_t> content_size)
    {
      assert (block_id >= 4 && block_id <= 7);

      level_ = level;
      block_id_ = block_id;
      content_size_ = content_size;

      LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
      init_preferences (&prefs);

      // Input/output buffer capacities.
      //
      // To be binary compatible with the lz4 utility we have to compress
      // files that fit into the block with a single *_compressFrame() call
      // instead of *_compressBegin()/*_compressUpdate(). And to determine the
      // output buffer capacity we must use *_compressFrameBound() instead of
      // *_compressBound(). The problem is, at this stage (before filling the
      // input buffer), we don't know which case it will be.
      //
      // However, in our case (autoFlush=1), *Bound() < *FrameBound() and so
      // we can always use the latter at the cost of slight overhead. Also,
      // using *FrameBound() allows us to call *Begin() and *Update() without
      // flushing the buffer in between (this insight is based on studying the
      // implementation of the *Bound() functions).
      //
      // Actually, we can use content_size (we can get away with much smaller
      // buffers for small inputs). We just need to verify the caller is not
      // lying to us (failed that, we may end up with strange error like
      // insufficient output buffer space).
      //
      ic = block_size (prefs.frameInfo.blockSizeID);

      if (content_size_ && *content_size_ < ic)
      {
        // This is nuanced: we need to add an extra byte in order to detect
        // EOF.
        //
        ic = static_cast<size_t> (*content_size_) + 1;
      }

      oc = LZ4F_compressFrameBound (ic, &prefs);

      begin_ = true;
    }

    void compressor::
    next (bool end)
    {
      LZ4F_cctx* ctx;

      // Unlike the decompression case below, compression cannot fail due to
      // invalid content. So any LZ4F_*() function failure is either due to a
      // programming bug or argument inconsistencies (e.g., content size does
      // not match actual).

      if (begin_)
      {
        begin_ = false;

        LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
        init_preferences (&prefs);

        // If we've allocated smaller buffers based on content_size_, then
        // verify the input size matches what's promised.
        //
        // Note also that LZ4F_compressFrame() does not fail if it doesn't
        // match instead replacing it with the actual value.
        //
        size_t bs (block_size (prefs.frameInfo.blockSizeID));
        if (content_size_ && *content_size_ < bs)
        {
          if (!end || in != *content_size_)
            throw_exception (LZ4F_ERROR_frameSize_wrong);
        }

        // Must be < for lz4 compatibility (see EOF nuance above for the
        // likely reason).
        //
        if (end && in < bs)
        {
          on = LZ4F_compressFrame (ob, oc, ib, in, &prefs);
          if (LZ4F_isError (on))
            throw_exception (on);

          in = 0; // All consumed.
          return;
        }
        else
        {
          if (LZ4F_isError (LZ4F_createCompressionContext (&ctx, LZ4F_VERSION)))
            throw bad_alloc ();

          ctx_ = ctx;

          // Write the header.
          //
          on = LZ4F_compressBegin (ctx, ob, oc, &prefs);
          if (LZ4F_isError (on))
            throw_exception (on);

          // Fall through.
        }
      }
      else
      {
        ctx = static_cast<LZ4F_cctx*> (ctx_);
        on = 0;
      }

      size_t n;

      if (in != 0)
      {
        n = LZ4F_compressUpdate (ctx, ob + on, oc - on, ib, in, nullptr);
        if (LZ4F_isError (n))
          throw_exception (n);

        in = 0; // All consumed.
        on += n;
      }

      // Write the end marker.
      //
      if (end)
      {
        // Note that this call also verifies specified and actual content
        // sizes match.
        //
        n = LZ4F_compressEnd (ctx, ob + on, oc - on, nullptr);
        if (LZ4F_isError (n))
          throw_exception (n);

        on += n;
      }
    }

    uint64_t
    compress (ofdstream& os, ifdstream& is,
              int level,
              int block_id,
              optional<uint64_t> content_size)
    {
#if 0
      char buf[1024 * 3 + 7];
      ostream cos (os, level, block_id, content_size);

      for (bool e (false); !e; )
      {
        e = eof (is.read (buf, sizeof (buf)));
        cos.write (buf, is.gcount ());
        //for (streamsize i (0), n (is.gcount ()); i != n; ++i)
        //  cos.put (buf[i]);
      }

      cos.close ();
      return content_size ? *content_size : 0;
#else
      compressor c;

      // Input/output buffer guards.
      //
      unique_ptr<char[]> ibg;
      unique_ptr<char[]> obg;

      // First determine required buffer capacities.
      //
      c.begin (level, block_id, content_size);

      ibg.reset ((c.ib = new char[c.ic]));
      obg.reset ((c.ob = new char[c.oc]));

      // Read into the input buffer updating the eof flag.
      //
      // Note that we could try to do direct fd read/write but that would
      // complicate things quite a bit (error handling, stream state, etc).
      //
      bool eof (false);
      auto read = [&is, &c, &eof] ()
      {
        eof = butl::eof (is.read (c.ib, c.ic));
        c.in = static_cast<size_t> (is.gcount ());
      };

      // Write from the output buffer updating the total written.
      //
      uint64_t ot (0);
      auto write = [&os, &c, &ot] ()
      {
        os.write (c.ob, static_cast<streamsize> (c.on));
        ot += c.on;
      };

      // Keep reading, compressing, and writing chunks of content.
      //
      while (!eof)
      {
        read ();

        c.next (eof);

        if (c.on != 0) // next() may just buffer the data.
          write ();
      }

      return ot;
#endif
    }

    // decompression
    //

    static_assert (sizeof (decompressor::hb) == LZ4F_HEADER_SIZE_MAX,
                   "LZ4 header size mismatch");

    decompressor::
    ~decompressor ()
    {
      if (LZ4F_dctx* ctx = static_cast<LZ4F_dctx*> (ctx_))
      {
        LZ4F_errorCode_t e (LZ4F_freeDecompressionContext (ctx));
        assert (!LZ4F_isError (e));
      }
    }

    size_t decompressor::
    begin (optional<uint64_t>* content_size)
    {
      LZ4F_dctx* ctx;

      if (LZ4F_isError (LZ4F_createDecompressionContext (&ctx, LZ4F_VERSION)))
        throw bad_alloc ();

      ctx_ = ctx;

      LZ4F_frameInfo_t info = LZ4F_INIT_FRAMEINFO;

      // Input hint and end as signalled by the LZ4F_*() functions.
      //
      size_t h, e;

      h = LZ4F_getFrameInfo (ctx, &info, hb, &(e = hn));
      if (LZ4F_isError (h))
        throw_exception (h);

      if (content_size != nullptr)
      {
        if (info.contentSize != 0)
          *content_size = static_cast<uint64_t> (info.contentSize);
        else
          *content_size = nullopt;
      }

      // Use the block size for the output buffer capacity and compressed
      // bound plus the header size for the input. The expectation is that
      // LZ4F_decompress() should never hint for more than that.
      //
      oc = block_size (info.blockSizeID);
      ic = LZ4F_compressBound (oc, nullptr) + LZ4F_BLOCK_HEADER_SIZE;

      assert (h <= ic);

      // Move over whatever is left in the header buffer to be beginning.
      //
      hn -= e;
      memmove (hb, hb + e, hn);

      return h;
    }

    size_t decompressor::
    next ()
    {
      LZ4F_dctx* ctx (static_cast<LZ4F_dctx*> (ctx_));

      size_t h, e;

      // Note that LZ4F_decompress() verifies specified and actual content
      // sizes match (similar to compression).
      //
      h = LZ4F_decompress (ctx, ob, &(on = oc), ib, &(e = in), nullptr);
      if (LZ4F_isError (h))
        throw_exception (h);

      // We expect LZ4F_decompress() to consume what it asked for.
      //
      assert (e == in && h <= ic);
      in = 0; // All consumed.

      return h;
    }

    uint64_t
    decompress (ofdstream& os, ifdstream& is)
    {
      // Write the specified number of bytes from the output buffer updating
      // the total written.
      //
      uint64_t ot (0);
      auto write = [&os, &ot] (char* b, size_t n)
      {
        os.write (b, static_cast<streamsize> (n));
        ot += n;
      };

#if 0
      char buf[1024 * 3 + 7];
      istream dis (is, true, istream::badbit);

      for (bool e (false); !e; )
      {
        e = eof (dis.read (buf, sizeof (buf)));
        write (buf, static_cast<size_t> (dis.gcount ()));
      }
#else
      // Read into the specified buffer returning the number of bytes read and
      // updating the eof flag.
      //
      bool eof (false);
      auto read = [&is, &eof] (char* b, size_t c) -> size_t
      {
        size_t n (0);
        do
        {
          eof = butl::eof (is.read (b + n, c - n));
          n += static_cast<size_t> (is.gcount ());
        }
        while (!eof && n != c);

        return n;
      };

      decompressor d;

      // Input/output buffer guards.
      //
      unique_ptr<char[]> ibg;
      unique_ptr<char[]> obg;

      size_t h; // Input hint.

      // First read in the header and allocate the buffers.
      //
      // What if we hit EOF here? And could begin() return 0? Turns out the
      // answer to both questions is yes: 0-byte content compresses to 15
      // bytes (with or without content size; 1-byte -- to 20/28 bytes). We
      // can ignore EOF here since an attempt to read more will result in
      // another EOF. And code below is prepared to handle 0 initial hint.
      //
      // @@ We could end up leaving some of the input content from the
      //    header in the input buffer which the caller will have to way
      //    of using/detecting.
      //
      d.hn = read (d.hb, sizeof (d.hb));
      h = d.begin ();

      ibg.reset ((d.ib = new char[d.ic]));
      obg.reset ((d.ob = new char[d.oc]));

      // Copy over whatever is left in the header buffer and read up to
      // the hinted size.
      //
      memcpy (d.ib, d.hb, (d.in = d.hn));

      if (h > d.in)
        d.in += read (d.ib + d.in, h - d.in);

      // Keep decompressing, writing, and reading chunks of compressed
      // content.
      //
      while (h != 0)
      {
        h = d.next ();

        if (d.on != 0) // next() may just buffer the data.
          write (d.ob, d.on);

        if (h != 0)
        {
          if (eof)
            throw invalid_argument ("incomplete LZ4 compressed content");

          d.in = read (d.ib, h);
        }
      }
#endif

      return ot;
    }
  }
}

// Include the implementation into our translation unit. Let's keep it last
// since the implementation defines a bunch of macros.
//
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

// This header is only include in the implementation so we can include it
// here instead of the above.
//
#define XXH_PRIVATE_API // Makes API static and includes xxhash.c.
#include "xxhash.h"

// Clang targeting MSVC prior to version 10 has difficulty with _tzcnt_u64()
// (see Clang bug 47099 for a potentially related issue). Including relevant
// headers (<immintrin.h>, <intrin.h>) does not appear to help. So for now we
// just disable the use of _tzcnt_u64().
//
#if defined(_MSC_VER) && defined(__clang__) && __clang_major__ < 10
#  define LZ4_FORCE_SW_BITCOUNT
#endif

// Note that the order of inclusion is important (see *_SRC_INCLUDED macros).
//
extern "C"
{
#include "lz4.c"
#include "lz4hc.c"
#include "lz4frame.c"
}
