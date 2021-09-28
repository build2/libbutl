// file      : libbutl/lz4.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <cstdint>
#include <cstddef>

#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  namespace lz4
  {
    // Read the content from the input stream, compress it using the specified
    // compression level and block size, and write the compressed content to
    // the output stream. If content size is specified, then include it into
    // the compressed content header. Return the compressed content size.
    //
    // This function may throw std::bad_alloc as well as exceptions thrown by
    // fdstream read/write functions. It may also throw std::invalid_argument
    // in case of argument inconsistencies (e.g., content size does not match
    // actual) with what() returning the error description. The input stream
    // is expected to throw on badbit (but not failbit). The output stream is
    // expected to throw on badbit or failbit.
    //
    // The output and most likely the input streams must be in the binary
    // mode.
    //
    // Valid values for the compression level are between 1 (fastest) and 12
    // (best compression level) though, practically, after 9 returns are
    // diminished.
    //
    // Valid block sizes and their IDs:
    //
    // 4:   64KB
    // 5:  256KB
    // 6:    1MB
    // 7:    4MB
    //
    // Note that due to the underlying API limitations, 0 content size is
    // treated as absent and it's therefore impossible to compress 0-byte
    // content with content size.
    //
    // This function produces compressed content identical to:
    //
    // lz4 -z -<compression_level> -B<block_size_id> -BD [--content-size]
    //
    LIBBUTL_SYMEXPORT std::uint64_t
    compress (ofdstream&,
              ifdstream&,
              int compression_level,
              int block_size_id,
              optional<std::uint64_t> content_size);

    // Low-level iterative compression API.
    //
    // This API may throw std::bad_alloc in case of memory allocation errors
    // and std::invalid_argument in case of argument inconsistencies (e.g.,
    // content size does not match actual) with what() returning the error
    // description.
    //
    // See the implementation of the compress() function above for usage
    // example.
    //
    // @@ TODO: reset support.
    //
    struct LIBBUTL_SYMEXPORT compressor
    {
      // Buffer, current size (part filled with data), and capacity.
      //
      char* ib; std::size_t in, ic; // Input.
      char* ob; std::size_t on, oc; // Output.

      // As a first step call begin(). This function sets the required input
      // and output buffer capacities (ic, oc).
      //
      // The caller normally allocates the input and output buffers and fills
      // the input buffer.
      //
      void
      begin (int compression_level,
             int block_size_id,
             optional<std::uint64_t> content_size);

      // Then call next() to compress the next chunk of input passing true on
      // reaching EOF. Note that the input buffer should be filled to capacity
      // unless end is true and the output buffer must be flushed before each
      // subsequent call to next().
      //
      void
      next (bool end);

      // Not copyable or movable.
      //
      compressor (const compressor&) = delete;
      compressor (compressor&&) = delete;
      compressor& operator= (const compressor&) = delete;
      compressor& operator= (compressor&&) = delete;

      // Implementation details.
      //
      compressor (): ctx_ (nullptr) {}
      ~compressor ();

    public:
      void
      init_preferences (void*) const;

      void* ctx_;
      int level_;
      int block_id_;
      optional<std::uint64_t> content_size_;
      bool begin_;
    };


    // Read the compressed content from the input stream, decompress it, and
    // write the decompressed content to the output stream. Return the
    // decompressed content size.
    //
    // This function may throw std::bad_alloc as well as exceptions thrown by
    // fdstream read/write functions. It may also throw std::invalid_argument
    // if the compressed content is invalid with what() returning the error
    // description. The input stream is expected to throw on badbit but not
    // failbit. The output stream is expected to throw on badbit or failbit.
    //
    // The input and most likely the output streams must be in the binary
    // mode.
    //
    // Note that this function does not require the input stream to reach EOF
    // at the end of compressed content. So if you have this requirement, you
    // will need to enforce it yourself.
    //
    LIBBUTL_SYMEXPORT std::uint64_t
    decompress (ofdstream&, ifdstream&);

    // Low-level iterative decompression API.
    //
    // This API may throw std::bad_alloc in case of memory allocation errors
    // and std::invalid_argument if the compressed content is invalid with
    // what() returning the error description.
    //
    // See the implementation of the decompress() function above for usage
    // example.
    //
    // The LZ4F_*() decompression functions return a hint of how much data
    // they want on the next call. So the plan is to allocate the input
    // buffer large enough to hold anything that can be asked for and then
    // fill it in in the asked chunks. This way we avoid having to shift the
    // unread data around.
    //
    // @@ TODO: reset support.
    //
    struct LIBBUTL_SYMEXPORT decompressor
    {
      // Buffer, current size (part filled with data), and capacity.
      //
      char  hb[19]; std::size_t hn    ; // Header.
      char* ib;     std::size_t in, ic; // Input.
      char* ob;     std::size_t on, oc; // Output.

      // As a first step, fill in the header buffer and call begin(). This
      // function sets the required input and output buffer capacities (ic,
      // oc) and the number of bytes left in the header buffer (hn) and
      // returns the number of bytes expected by the following call to next().
      // If content_size is not NULL, then it is set to the decompressed
      // content size, if available.
      //
      // The caller normally allocates the input and output buffers, copies
      // remaining header buffer data over to the input buffer, and then fills
      // in the remainder of the input buffer up to what's expected by the
      // call to next().
      //
      std::size_t
      begin (optional<std::uint64_t>* content_size = nullptr);

      // Then call next() to decompress the next chunk of input. This function
      // returns the number of bytes expected by the following call to next()
      // or 0 if no further input is expected. Note that the output buffer
      // must be flushed before each subsequent call to next().
      //
      std::size_t
      next ();

      // Not copyable or movable.
      //
      decompressor (const decompressor&) = delete;
      decompressor (decompressor&&) = delete;
      decompressor& operator= (const decompressor&) = delete;
      decompressor& operator= (decompressor&&) = delete;

      // Implementation details.
      //
      decompressor (): hn (0), in (0), on (0), ctx_ (nullptr) {}
      ~decompressor ();

    public:
      void* ctx_;
    };
  }
}
