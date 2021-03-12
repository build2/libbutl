// file      : libbutl/lz4.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/lz4.hxx>

// Clang 8 targeting MSVC does not have _tzcnt_u64().
//
#if defined(_MSC_VER) && defined(__clang__) && __clang_major__ <= 8
#  define LZ4_FORCE_SW_BITCOUNT
#endif

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

#include <libbutl/utility.mxx> // eos()

using namespace std;

namespace butl
{
  namespace lz4
  {
    struct cctx
    {
      LZ4F_cctx* ctx;

      operator LZ4F_cctx* () const {return ctx;};

      cctx ()
      {
        if (LZ4F_isError (LZ4F_createCompressionContext (&ctx, LZ4F_VERSION)))
          throw bad_alloc ();
      }

      ~cctx ()
      {
        LZ4F_errorCode_t e (LZ4F_freeCompressionContext (ctx));
        assert (!LZ4F_isError (e));
      }
    };

    struct dctx
    {
      LZ4F_dctx* ctx;

      operator LZ4F_dctx* () const {return ctx;};

      dctx ()
      {
        if (LZ4F_isError (LZ4F_createDecompressionContext (&ctx, LZ4F_VERSION)))
          throw bad_alloc ();
      }

      ~dctx ()
      {
        LZ4F_errorCode_t e (LZ4F_freeDecompressionContext (ctx));
        assert (!LZ4F_isError (e));
      }
    };

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
      case LZ4F_ERROR_GENERIC:                     throw i ("generic error");
      case LZ4F_ERROR_maxBlockSize_invalid:        throw i ("invalid block size");
      case LZ4F_ERROR_blockMode_invalid:           throw i ("invalid block mode");
      case LZ4F_ERROR_contentChecksumFlag_invalid: throw i ("invalid content checksum flag");
      case LZ4F_ERROR_compressionLevel_invalid:    throw i ("invalid compression level");
      case LZ4F_ERROR_headerVersion_wrong:         throw i ("wrong header version");
      case LZ4F_ERROR_blockChecksum_invalid:       throw i ("invalid block checksum");
      case LZ4F_ERROR_reservedFlag_set:            throw i ("reserved flag set");
      case LZ4F_ERROR_srcSize_tooLarge:            throw i ("input too large");
      case LZ4F_ERROR_dstMaxSize_tooSmall:         throw i ("output too small");
      case LZ4F_ERROR_frameHeader_incomplete:      throw i ("incomplete frame header");
      case LZ4F_ERROR_frameType_unknown:           throw i ("unknown frame type");
      case LZ4F_ERROR_frameSize_wrong:             throw i ("wrong frame size");
      case LZ4F_ERROR_decompressionFailed:         throw i ("invalid compressed content");
      case LZ4F_ERROR_headerChecksum_invalid:      throw i ("invalid header checksum");
      case LZ4F_ERROR_contentChecksum_invalid:     throw i ("invalid content checksum");

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

    // Return the compressed size.
    //
    uint64_t
    compress (ofdstream& os, ifdstream& is,
              int level,
              int block_id,
              optional<uint64_t> content_size)
    {
      assert (block_id >= 4 && block_id <= 7);

      LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
      prefs.autoFlush = 1;
      prefs.favorDecSpeed = 0;
      prefs.compressionLevel = level;
      prefs.frameInfo.blockMode = LZ4F_blockLinked;
      prefs.frameInfo.blockSizeID = static_cast<LZ4F_blockSizeID_t> (block_id);
      prefs.frameInfo.blockChecksumFlag = LZ4F_noBlockChecksum;
      prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
      prefs.frameInfo.contentSize =
        content_size ? static_cast<unsigned long long> (*content_size) : 0;

      // Input/output buffer capacities.
      //
      size_t ic (block_size (prefs.frameInfo.blockSizeID));
      size_t oc;

      // Input/output buffers.
      //
      unique_ptr<char[]> ibg (new char[ic]); char* ib (ibg.get ());
      unique_ptr<char[]> obg;                char* ob;

      // Read into the input buffer returning the number of bytes read and
      // updating the total read and the eof flag.
      //
      // Note that we could try to do direct fd read/write but that would
      // complicate things quite a bit (error handling, stream state, etc).
      //
      uint64_t it (0);
      bool eof (false);
      auto read = [&is, ib, ic, &it, &eof] () -> size_t
      {
        eof = butl::eof (is.read (ib, ic));
        size_t n (static_cast<size_t> (is.gcount ()));
        it += n;
        return n;
      };

      // Write the specified number of bytes from the output buffer updating
      // the total written.
      //
      uint64_t ot (0);
      auto write = [&os, &ob, &ot] (size_t n)
      {
        os.write (ob, static_cast<streamsize> (n));
        ot += n;
      };

      // Unlike the decompression case below, compression cannot fail due to
      // invalid content. So any LZ4F_*() function failure is either due to a
      // programming bug or argument inconsistencies (e.g., content size does
      // not match actual).

      // To be binary compatible with the lz4 utility we have to compress
      // files that fit into the block with a single LZ4F_compressFrame()
      // call.
      //
      size_t in (read ());
      size_t on;

      if (eof && in < ic) // Should be really <= but that's not lz4-compatible.
      {
        oc = LZ4F_compressFrameBound (in, &prefs);
        obg.reset ((ob = new char[oc]));

        on = LZ4F_compressFrame (ob, oc, ib, in, &prefs);
        if (LZ4F_isError (on))
          throw_exception (on);

        write (on);

        // Verify specified and actual content sizes match.
        //
        // LZ4F_compressFrame() does not fail if it doesn't match instead
        // replacing it with the actual value.
        //
        if (content_size && *content_size != it)
          throw_exception (LZ4F_ERROR_frameSize_wrong);
      }
      else
      {
        cctx ctx;

        oc = LZ4F_compressBound (ic, &prefs);
        obg.reset ((ob = new char[oc]));

        // Write the header.
        //
        on = LZ4F_compressBegin (ctx, ob, oc, &prefs);
        if (LZ4F_isError (on))
          throw_exception (on);

        write (on);

        // Keep compressing, writing, and reading chunks of content.
        //
        for (;;)
        {
          on = LZ4F_compressUpdate (ctx, ob, oc, ib, in, nullptr);
          if (LZ4F_isError (on))
            throw_exception (on);

          if (on != 0) // LZ4F_compressUpdate() may just buffer the data.
            write (on);

          if (eof)
            break;

          in = read ();
        }

        // Write the end marker.
        //
        // Note that this call also verifies specified and actual content
        // sizes match.
        //
        on = LZ4F_compressEnd (ctx, ob, oc, nullptr);
        if (LZ4F_isError (on))
          throw_exception (on);

        write (on);
      }

      return ot;
    }

    uint64_t
    decompress (ofdstream& os, ifdstream& is)
    {
      // The LZ4F_*() decompression functions return a hint of how much data
      // they want on the next call. So the plan is to allocate the input
      // buffer large enough to hold anything that can be asked for and then
      // fill it in in the asked chunks. This way we avoid having to shift the
      // unread data, etc.
      //
      dctx ctx;

      // Input/output buffer capacities and sizes.
      //
      size_t ic, oc;
      size_t in, on;

      // Input/output buffers.
      //
      unique_ptr<char[]> ibg; char* ib;
      unique_ptr<char[]> obg; char* ob;

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

      // Write the specified number of bytes from the output buffer updating
      // the total written.
      //
      uint64_t ot (0);
      auto write = [&os, &ob, &ot] (size_t n)
      {
        os.write (ob, static_cast<streamsize> (n));
        ot += n;
      };

      // Input hint and end as signalled by the LZ4F_*() functions.
      //
      size_t ih, ie;

      // Read the header.
      //
      LZ4F_frameInfo_t info = LZ4F_INIT_FRAMEINFO;
      {
        char hb[LZ4F_HEADER_SIZE_MAX];
        in = read (hb, sizeof (hb));

        ih = LZ4F_getFrameInfo (ctx, &info, hb, &(ie = in));
        if (LZ4F_isError (ih))
          throw_exception (ih);

        // Use the block size for the output buffer capacity and compressed
        // bound plus the header size for the input. The expectation is that
        // LZ4F_decompress() should never hint for more than that.
        //
        oc = block_size (info.blockSizeID);
        ic = LZ4F_compressBound (oc, nullptr) + LZ4F_BLOCK_HEADER_SIZE;

        assert (ih <= ic);

        ibg.reset ((ib = new char[ic]));
        obg.reset ((ob = new char[oc]));

        // Copy over whatever is left in the header buffer and read up to
        // the hinted size.
        //
        in -= ie;
        memcpy (ib, hb + ie, in);
        in += read (ib + in, ih - in);
      }

      // Keep decompressing, writing, and reading chunks of compressed
      // content.
      //
      // Note that LZ4F_decompress() verifies specified and actual content
      // sizes match (similar to compression).
      //
      for (;;)
      {
        ih = LZ4F_decompress (ctx, ob, &(on = oc), ib, &(ie = in), nullptr);
        if (LZ4F_isError (ih))
          throw_exception (ih);

        // We expect LZ4F_decompress() to consume what it asked for.
        //
        assert (ie == in);

        write (on);

        if (ih == 0)
          break;

        if (eof)
          throw invalid_argument ("incomplete compressed content");

        assert (ih <= ic);
        in = read (ib, ih);
      }

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

// Note that the order of inclusion is important (see *_SRC_INCLUDED macros).
//
extern "C"
{
#include "lz4.c"
#include "lz4hc.c"
#include "lz4frame.c"
}
